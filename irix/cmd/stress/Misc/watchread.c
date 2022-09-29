/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.13 $ $Author: kanoj $"

#include "sys/types.h"
#include "signal.h"
#include "getopt.h"
#include "sys/ipc.h"
#include "sys/shm.h"
#include "sys/ucontext.h"
#include "sys/time.h"
#include "sys/siginfo.h"
#include "sys/procfs.h"
#include "stdio.h"
#include "wait.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"
#include "malloc.h"
#include "sys/times.h"
#include "sys/prctl.h"
#include "limits.h"
#include "termios.h"
#include "semaphore.h"

volatile unsigned *space;
typedef struct {
	volatile int startread;
	volatile int leave;
	volatile int nloops;
	sem_t	     sem[2];
} sstate_t;
sstate_t *shaddr;
int debug = 0;
int verbose = 0;
int nreaders = 2;
int parentpid;
int exitonerror = 1;
void kid(void);
void errexit(void);
void prwatch(int pfd);
prwatch_t *getwatch(int pfd, int *nwp);
int getnwatch(int pfd);
void readit(char *pname);

void
initproc(void)
{
	if ((sem_init(&shaddr->sem[0], 1, 0) != 0) ||
			(sem_init(&shaddr->sem[1], 1, 0) != 0)) {
		printf("initproc failure\n");
		exit(1);
	}
}

void
blockproc1(int pid)
{
	int idx = (pid == parentpid)? 0 : 1;
	
	if (sem_wait(&shaddr->sem[idx]) != 0) {
		printf("blockproc1 %d failure\n", pid);
		exit(1);
	}
}

void
unblockproc1(int pid)
{
	int idx = (pid == parentpid)? 0 : 1;

	if (sem_post(&shaddr->sem[idx]) != 0) {
		printf("blockproc1 %d failure\n", pid);
		exit(1);
	}
}

/*
 * watchread - have 2 processes read a process that has watchpoints
 * One is forked process reading via /proc
 * The other is a forked child reading its data space
 * The parent sets the watchpoint
 */

void
wstopit(int pfd, prstatus_t *psi)
{
	if (ioctl(pfd, PIOCWSTOP, psi) != 0) {
		perror("watchread:ERROR:PWSTOP");
		errexit();
	}
}

void
stopit(int pfd)
{
	if (ioctl(pfd, PIOCSTOP, NULL) != 0) {
		perror("watchread:ERROR:PSTOP");
		errexit();
	}
}

void
runit(int pfd, long flags)
{
	prrun_t pr;

	pr.pr_flags = flags;
	if (ioctl(pfd, PIOCRUN, &pr) != 0) {
		perror("watchread:ERROR:PRUN");
		fprintf(stderr, "watchread:ERROR:PRUN flags 0x%x\n", flags);
		errexit();
	}
}

int
setwatch(int pfd, void *addr, u_long size, u_long mode)
{
	prwatch_t pw;

	pw.pr_vaddr = addr;
	pw.pr_size = size;
	pw.pr_wflags = mode;
	return(ioctl(pfd, PIOCSWATCH, &pw));
}

int
clrwatch(int pfd, void *addr)
{
	prwatch_t pw;

	pw.pr_vaddr = addr;
	pw.pr_size = 0;
	return(ioctl(pfd, PIOCSWATCH, &pw));
}

char *whytab[] = {
	"UNK",
	"REQ",
	"SIG",
	"SYSENTRY",
	"SYSEXIT",
	"FAULTED",
	"JOBCONTROL"
};
char *whatftab[] = {
	"",
	"FLTILL",
	"FLTPRIV",
	"FLTBPT",
	"FLTTRACE",
	"FLTACCESS",
	"FLTBOUNDS",
	"FLTIOVF",
	"FLTIZDIV",
	"FLTFPE",
	"FLTSTACK",
	"FLTPAGE",
	"FLTPCINVAL",
	"FLTWATCH",
	"FLTKWATCH",
};

void
prps(char *s, prstatus_t *psi)
{
	printf("watchread:%s:Pid %d flags 0x%x why %s(%d)",
		s, psi->pr_pid, psi->pr_flags,
		whytab[psi->pr_why], psi->pr_why);
	if (psi->pr_why == PR_FAULTED) {
		printf(" what %s(%d)\n",
			whatftab[psi->pr_what], psi->pr_what);
	} else {
		printf(" what (%d)\n",
			psi->pr_what);
	}
	if (psi->pr_why == PR_FAULTED && (psi->pr_what == FLTWATCH ||
			psi->pr_what == FLTKWATCH)) {
		printf("watchread:%s:Pid %d PC 0x%p code %s%s%s(0x%x) addr 0x%p\n",
			s, psi->pr_pid,
			psi->pr_reg[CTX_EPC],
			psi->pr_info.si_code & MA_READ ? "READ " : "",
			psi->pr_info.si_code & MA_WRITE ? "WRITE " : "",
			psi->pr_info.si_code & MA_EXEC ? "EXEC " : "",
			psi->pr_info.si_code,
			psi->pr_info.si_addr);
	} else if (psi->pr_why == PR_FAULTED) {
		printf("watchread:%s:Pid %d PC 0x%p instr 0x%x\n",
			s, psi->pr_pid, psi->pr_reg[CTX_EPC], psi->pr_instr);
		printf("watchread:%s:addr 0x%p code %d\n",
			s,
			psi->pr_info.si_addr,
			psi->pr_info.si_code);
	} else {
		printf("watchread:%s:Pid %d PC 0x%p\n",
			s, psi->pr_pid, psi->pr_reg[CTX_EPC]);
	}
}

/*
 * stop over watch
 * Leaves process stopped at a FLTTRACE
 */
void
stepoverwatch(char *s, int pfd, void * addr, prstatus_t *opsi)
{
	prstatus_t psi;
	prwatch_t  *list;
	int        i, num;

restart:
/*
	if (opsi->pr_reg[CTX_EPC] < 0x10000000) {
		printf("STOPPED IN SIGNAL HANDLER 0x%x\n", opsi->pr_reg[CTX_EPC] );
		errexit();
	}
*/

	if (opsi->pr_why != PR_FAULTED) {
		fprintf(stderr, "watchread:ERROR:%s not FAULTED; why == %d\n",
			s, opsi->pr_why);
		errexit();
	}
	if (debug)
		prps(s, opsi);

	list = getwatch(pfd, &num);
	for (i = 0; i < num; i++) {
		if (list[i].pr_vaddr == addr) {
			num = i;
			break;
		}
	}
	if (i == (num +1)) {
		printf("watchpt at 0x%x not in list\n", addr);
		errexit();
	}

	/* clear watchpoint */
	if (clrwatch(pfd, addr) < 0) {
		perror("watchread:ERROR:stepoverwatchread:clrwatch");
		errexit();
	}

	if (opsi->pr_what == FLTKWATCH) {
		/* there is nothing to skip over since it occurred in
		 * a system call
		 */
		return;
	}
	if (opsi->pr_info.si_addr != addr) {
		fprintf(stderr, "watchread:ERROR:%s stepping over 0x%p but faulted at 0x%p\n",
			s, addr, opsi->pr_info.si_addr);
		errexit();
	}

	/* step process - this might end up in a signal handler */
	runit(pfd, PRSTEP|PRCFAULT);
	wstopit(pfd, &psi);
	if (debug)
		prps(s, &psi);
	if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTTRACE) {
		perror("watchread:ERROR:stepoverwatchread:stop not FAULTED");
		errexit();
	}

	/* foll logic wrong for exec wpted branches */
	if (psi.pr_reg[CTX_EPC] != (opsi->pr_reg[CTX_EPC] + 4)) {
		setwatch(pfd, list[num].pr_vaddr, list[num].pr_size, list[num].pr_wflags);
		free(list);
		if (verbose)
			printf("RESETTING WPT AND RESTARTING 0x%llx 0x%llx 0x%llx\n", psi.pr_reg[CTX_EPC], opsi->pr_reg[CTX_EPC], opsi->pr_reg[CTX_EPC] + 4);
		runit(pfd, PRCFAULT);
		wstopit(pfd, opsi);
		goto restart;
	}
}

int
main(int argc, char **argv)
{
	int c, i;
	char pname[32];
	int pfd;
	pid_t pid, *tpid;
	prstatus_t psi;
	prpsinfo_t psinfo;
	sigset_t sigs;
	fltset_t faults;
	int shmid;

	while((c = getopt(argc, argv, "dn:v")) != EOF)
	switch (c) {
	case 'd':
		debug++;
		exitonerror = 0;
		break;
	case 'v':
		verbose++;
		break;
	case 'n':
		nreaders = atoi(optarg);
		break;
	default:
		fprintf(stderr, "Usage:watchread [-d][-v][-n #]\n");
		fprintf(stderr, "\t-d - debug\n");
		fprintf(stderr, "\t-v - verbose\n");
		fprintf(stderr, "\t-n# - # of readers\n");
		exit(-1);
	}

	setlinebuf(stdout);
	setlinebuf(stderr);
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */
	parentpid = getpid();
	/* set up some shd mem */
	if ((shmid = shmget(IPC_PRIVATE, 0x40000, IPC_CREAT|0777)) < 0) {
		perror("watchread:ERROR:shmget");
		errexit();
	}
	if ((shaddr = shmat(shmid, 0, 0)) == (sstate_t *)-1L) {
		perror("watchread:ERROR:shmat");
		errexit();
	}
	if (shmctl(shmid, IPC_RMID) != 0) {
		perror("watchread:ERROR:shmctl");
		errexit();
	}
	shaddr->startread = 0;
	shaddr->leave = 0;
	shaddr->nloops = 0;
	initproc();
	space = (volatile unsigned *)malloc(10004);
	*(unsigned *)((__psunsigned_t)space + 10000L) = 0xdeadbeef;

	/* fork our own kid */
	if ((pid = fork()) < 0) {
		perror("watchread:ERROR:fork");
		errexit();
	} else if (pid == 0) {
		kid();
		exit(0);
	}

	/* parent */
	/* open up /debug file */
	sprintf(pname, "/debug/%05d", pid);
	if ((pfd = open(pname, O_RDWR)) < 0) {
		fprintf(stderr, "watchread:ERROR:Cannot open %s\n", pname);
		perror("");
		errexit();
	}

	/* stop process */
	stopit(pfd);
	unblockproc1(pid);

	/* spin up readers */
	tpid = (pid_t *)calloc(nreaders, sizeof(pid_t));
	for (i = 0; i < nreaders; i++) {
		if ((tpid[i] = fork()) < 0) {
			perror("watchread:fork");
			errexit();
		} else if (tpid[i] == 0) {
			/* child */
			readit(pname);
			exit(0);
		}
	}

	if (ioctl(pfd, PIOCSTATUS, &psi) != 0) {
		perror("watchread:ERROR:PSTATUS");
		errexit();
	}
	if (verbose)
		prps("1st stop", &psi);

	/* set up to get SIGTRAP & ALL faults (incl watchpoints) */
	premptyset(&sigs);
	praddset(&sigs, SIGTRAP);
	if (ioctl(pfd, PIOCSTRACE, &sigs) != 0) {
		perror("watchread:ERROR:PSTRACE");
		errexit();
	}
	prfillset(&faults);
	prdelset(&faults, FLTPAGE);
	if (ioctl(pfd, PIOCSFAULT, &faults) != 0) {
		perror("watchread:ERROR:PSFAULT");
		errexit();
	}

	/* set a real watchpoint */
	if (setwatch(pfd, (char *)space, 4, MA_READ|MA_WRITE) < 0) {
		perror("watchread:ERROR:setwatch");
		errexit();
	}

	if (shaddr->nloops != 0) {
		fprintf(stderr, "watchread:ERROR:loops %d should be 0!\n", shaddr->nloops);
		errexit();
	}
	/* run process */
	runit(pfd, 0);
	shaddr->startread = 1;

	for (i = 0; i < 100; i++) {
		wstopit(pfd, &psi);
		if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTWATCH) {
			fprintf(stderr, "watchread:ERROR:missing FLTWATCH\n");
			prps("loop", &psi);
			printf("watchread:Pid %d v0 0x%p at 0x%p\n",
				psi.pr_pid, psi.pr_reg[CTX_V0],
				psi.pr_reg[CTX_AT]);
			prwatch(pfd);
			if (ioctl(pfd, PIOCPSINFO, &psinfo) == 0) {
				fprintf(stderr, "\tstate %d (%c) flags 0x%lx\n",
					psinfo.pr_state,
					psinfo.pr_sname,
					psinfo.pr_flag);
			}
			errexit();
		}
		if (getnwatch(pfd) != 1) {
			fprintf(stderr, "watchread:ERROR:Watchpoint gone!\n");
			prps("gone watch", &psi);
			errexit();
		}
		if (shaddr->nloops != i) {
			fprintf(stderr, "watchread:ERROR:loops %d should be %d!\n", shaddr->nloops, i);
			prps("bad count", &psi);
			errexit();
		}
		if (debug) {
			printf("stopped at 0x%p flags 0x%x\n",
				psi.pr_reg[CTX_EPC], psi.pr_flags);
		}
		stepoverwatch("", pfd, (char *)space, &psi);

		if (shaddr->nloops != i) {
			fprintf(stderr, "watchread:ERROR:loops %d after step should be %d!\n", shaddr->nloops, i);
			prps("bad count after step", &psi);
			errexit();
		}
		/* set a real watchpoint */
		if (setwatch(pfd, (char *)space, 4, MA_READ|MA_WRITE) < 0) {
			perror("watchread:ERROR:setwatch");
			errexit();
		}
		runit(pfd, PRCFAULT);
	}
	wstopit(pfd, &psi);
	shaddr->leave = 1;
	clrwatch(pfd, (void *)space);
	if (shaddr->nloops != 100) {
		fprintf(stderr, "watchread:ERROR:loops %d should be 100\n", shaddr->nloops);
		errexit();
	}

	/* wait for readers */
	for (i = 0; i < nreaders; i++)
		waitpid(tpid[i], NULL, 0);

	/* let child finish up */
	runit(pfd, PRCFAULT);
	close(pfd);	/* will run child */

	return 0;
}

void
kid(void)
{
	pid_t mypid;
	register volatile int i = 0;

	mypid = getpid();
	if (verbose)
		printf("target child pid = %d\n", mypid);
	blockproc1(mypid);

	while (!shaddr->leave) {
		*space = ++i;
		shaddr->nloops++;
		if ((i % 2) == 0)
			*(unsigned *)((__psunsigned_t)space + 10000L) = 0xdeadbeef;
		else
			*(unsigned *)((__psunsigned_t)space + 10000L) = 0xbeefbabe;
		if ((i % 10000) == 0)
			if (getppid() == 1)
				exit(1);
	}
}

void
errexit(void)
{
	if (exitonerror)
		abort();
}

void
prwatch(int pfd)
{
	int i, nw, maxw;
	prwatch_t *wp;

	if (ioctl(pfd, PIOCNWATCH, &maxw) < 0) {
		perror("watchread:ERROR:NWATCH");
		errexit();
	}

	wp = (prwatch_t *)malloc(sizeof(*wp) * maxw);

	if ((nw = ioctl(pfd, PIOCGWATCH, (void *)wp)) < 0) {
		perror("watchread:ERROR:GWATCH");
		errexit();
	}
	for (i = 0; i < nw; i++, wp++) {
		printf("watchread:Watchpoint at 0x%p len 0x%x mode 0x%x\n",
			wp->pr_vaddr,
			wp->pr_size,
			wp->pr_wflags);
	}
}

/*
 * retrieve current set of watchpoints
 */
prwatch_t *
getwatch(int pfd, int *nwp)
{
	int nw, maxw;
	prwatch_t *wp;

	if (ioctl(pfd, PIOCNWATCH, &maxw) < 0) {
		perror("snoop:ERROR:NWATCH");
		errexit();
	}

	wp = (prwatch_t *)malloc(sizeof(*wp) * maxw);

	if ((nw = ioctl(pfd, PIOCGWATCH, (void *)wp)) < 0) {
		perror("snoop:ERROR:GWATCH");
		errexit();
	}
	*nwp = nw;
	return(wp);
}

/*
 * retrieve current number of watchpoints
 */
int
getnwatch(int pfd)
{
	prwatch_t *w;
	int nwp;

	w = getwatch(pfd, &nwp);
	free(w);
	return(nwp);
}

void
readit(char *pname)
{
	int pfd;
	uint val;
	ssize_t rv;

	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);

	if ((pfd = open(pname, O_RDWR)) < 0) {
		fprintf(stderr, "watchread:ERROR:Reader cannot open %s\n", pname);
		perror("");
		errexit();
	}
	while (shaddr->startread == 0)
		sginap(1);
	
	setlinebuf(stderr);
	while (!shaddr->leave) {
		if (lseek(pfd, (off_t)space, 0) <= 0) {
			perror("watchread:ERROR:lseek");
			errexit();
		}
		if ((rv = read(pfd, (void *)space, 10004)) != 10004) {
			if (rv == -1)
				perror("watchread:ERROR:read");
			else
				printf("watchread:ERROR:short read %d out of 10004\n", rv);
			errexit();
		}
		if (verbose)
			printf("watchread:reader:*space %d\n", *(unsigned *)space);
		val = *(unsigned *)((__psunsigned_t)space + 10000L);
		if (val != 0xdeadbeef && val != 0xbeefbabe) {
			fprintf(stderr, "watchread:ERROR:bad read value: 0x%x\n",
				val);
			errexit();
		}
	}
	exit(0);
}
