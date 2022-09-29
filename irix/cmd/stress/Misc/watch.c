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

#ident	"$Revision: 1.23 $ $Author: kanoj $"

#include "sys/types.h"
#include "errno.h"
#include "mutex.h"
#include "signal.h"
#include "sys/ipc.h"
#include "sys/shm.h"
#include "sys/ucontext.h"
#include "sys/time.h"
#include "sys/siginfo.h"
#include "sys/procfs.h"
#include "stdio.h"
#include "unistd.h"
#include "getopt.h"
#include "stdlib.h"
#include "fcntl.h"
#include "malloc.h"
#include "termios.h"
#include "sys/times.h"
#include "limits.h"
#include "semaphore.h"

extern ftext[];
volatile char *space;
char sbuf[4];
unsigned long ts[2];
typedef struct {
	volatile int state;
	volatile int leave;
	volatile int nsig;
	sem_t	     sem[2];
} sstate_t;
sstate_t *shaddr;
int verbose = 0;
int parentpid;
int exitonerror = 1;
int dodrain = 0;
void kcatch(void);
int getnwatch(int pfd);
prwatch_t * getwatch(int pfd, int *nwp);
void prwatch(int pfd);
void errexit(void);
void kid3(void);
void kid2(double a);
void kid(void);

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
		printf("blockproc1 %d failure errno = %d\n", pid, errno);
		exit(1);
	}
}

void
unblockproc1(int pid)
{
	int idx = (pid == parentpid)? 0 : 1;

	if (sem_post(&shaddr->sem[idx]) != 0) {
		printf("unblockproc1 %d failure errno = %d\n", pid, errno);
		exit(1);
	}
}


/*
 * watch - try out watchpoints
 */

void
wstopit(int pfd, prstatus_t *psi)
{
	if (ioctl(pfd, PIOCWSTOP, psi) != 0) {
		perror("watch:ERROR:PWSTOP");
		errexit();
	}
}

void
stopit(int pfd)
{
	if (ioctl(pfd, PIOCSTOP, NULL) != 0) {
		perror("watch:ERROR:PSTOP");
		errexit();
	}
}

void
runit(int pfd, long flags)
{
	prrun_t pr;

	pr.pr_flags = flags;
	if (ioctl(pfd, PIOCRUN, &pr) != 0) {
		perror("watch:ERROR:PRUN");
		fprintf(stderr, "watch:ERROR:PRUN flags 0x%x\n", flags);
		errexit();
	}
}

int
setwatch(int pfd, caddr_t addr, u_long size, u_long mode)
{
	prwatch_t pw;

	pw.pr_vaddr = addr;
	pw.pr_size = size;
	pw.pr_wflags = mode;
	return(ioctl(pfd, PIOCSWATCH, &pw));
}

int
clrwatch(int pfd, caddr_t addr)
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
	"FLTSCWATCH",
};

void
prps(char *s, prstatus_t *psi)
{
	printf("watch:%s:Pid %d flags 0x%x why %s(%d)",
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
			psi->pr_what == FLTKWATCH ||
			psi->pr_what == FLTSCWATCH)) {
		printf("watch:%s:Pid %d PC 0x%llx code %s%s%s(0x%x) addr 0x%p\n",
			s, psi->pr_pid,
			(long long) psi->pr_reg[CTX_EPC],
			psi->pr_info.si_code & MA_READ ? "READ " : "",
			psi->pr_info.si_code & MA_WRITE ? "WRITE " : "",
			psi->pr_info.si_code & MA_EXEC ? "EXEC " : "",
			psi->pr_info.si_code,
			psi->pr_info.si_addr);
	} else if (psi->pr_why == PR_FAULTED) {
		printf("watch:%s:Pid %d PC 0x%llx instr 0x%x\n",
			s, psi->pr_pid, (long long)psi->pr_reg[CTX_EPC],
			psi->pr_instr);
		printf("watch:%s:addr 0x%p code %d\n",
			s,
			psi->pr_info.si_addr,
			psi->pr_info.si_code);
	} else {
		printf("watch:%s:Pid %d PC 0x%llx instr 0x%x\n",
			s, psi->pr_pid, (long long)psi->pr_reg[CTX_EPC],
			psi->pr_instr);
	}
	if (dodrain)
		tcdrain(fileno(stdout));
}

/*
 * stop over watch
 * Leaves process stopped at a FLTTRACE
 */
void
stepoverwatch(char *s, int pfd, caddr_t addr, prstatus_t *opsi)
{
	prstatus_t psi;
	prwatch_t  *list;
	int 	   i, num;

restart:
/*
	if (opsi->pr_reg[CTX_EPC] < 0x10000000) {
		printf("STOPPED IN SIGNAL HANDLER 0x%x\n", opsi->pr_reg[CTX_EPC] );
		errexit();
	}
*/
	if (opsi->pr_why != PR_FAULTED) {
		fprintf(stderr, "watch:ERROR:%s not FAULTED; why == %d\n",
			s, opsi->pr_why);
		errexit();
	}
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
		perror("watch:ERROR:stepoverwatch:clrwatch");
		errexit();
	}

	if (opsi->pr_what == FLTKWATCH) {
		/* there is nothing to skip over since it occurred in
		 * a system call
		 */
		return;
	}
	if (opsi->pr_what != FLTSCWATCH && opsi->pr_info.si_addr != addr) {
		fprintf(stderr, "watch:ERROR:%s stepping over 0x%p but faulted at 0x%p\n",
			s, addr, opsi->pr_info.si_addr);
		errexit();
	}

	/* step process - this might end up in a signal handler */
	runit(pfd, PRSTEP|PRCFAULT);
	wstopit(pfd, &psi);
	prps(s, &psi);
	if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTTRACE) {
		perror("watch:ERROR:stepoverwatch:stop not FAULTED");
		errexit();
	}

	/* foll logic wrong for exec wpted branches */
	if (psi.pr_reg[CTX_EPC] != (opsi->pr_reg[CTX_EPC] + 4)) {
		setwatch(pfd, list[num].pr_vaddr, list[num].pr_size, list[num].pr_wflags);
		free(list);
		printf("RESETTING WPT AND RESTARTING 0x%llx 0x%llx 0x%llx\n", psi.pr_reg[CTX_EPC], opsi->pr_reg[CTX_EPC], opsi->pr_reg[CTX_EPC] + 4);
		runit(pfd, PRCFAULT);
		wstopit(pfd, opsi);
		goto restart;
	}
}

int
main(int argc, char **argv)
{
	int c, i, j;
	char pname[32];
	int pfd;
	int pid;
	prstatus_t psi;
	char buf[128];
	auto int inst;
	sigset_t sigs;
	fltset_t faults;
	int shmid;
	caddr_t kid2b;

	setlinebuf(stdout);
	setlinebuf(stderr);
	pid = 0;
	while((c = getopt(argc, argv, "dvp:")) != EOF)
	switch (c) {
	case 'p':
		pid = atoi(optarg);
		break;
	case 'd':
		dodrain++;
		exitonerror = 0;
		break;
	case 'v':
		verbose++;
		break;
	default:
		fprintf(stderr, "Usage:watch [-d][-v][-p #]\n");
		fprintf(stderr, "\t-d - debug\n");
		fprintf(stderr, "\t-v - verbose\n");
		fprintf(stderr, "\t-p# - specify pid\n");
		exit(-1);
	}

	parentpid = getpid();
	/* set up some shd mem */
	if ((shmid = shmget(IPC_PRIVATE, 0x40000, IPC_CREAT|0777)) < 0) {
		perror("watch:ERROR:shmget");
		errexit();
	}
	if ((shaddr = shmat(shmid, 0, 0)) == (sstate_t *)-1L) {
		perror("watch:ERROR:shmat");
		errexit();
	}
	if (shmctl(shmid, IPC_RMID) != 0) {
		perror("watch:ERROR:shmctl");
		errexit();
	}
	shaddr->state = 0;
	shaddr->nsig = 0;
	initproc();
	if (pid == 0) {
		space = (volatile char *)malloc(10000);
		/* fork our own kid */
		if ((pid = fork()) < 0) {
			perror("watch:ERROR:fork");
			errexit();
		} else if (pid == 0) {
			kid();
			exit(0);
		}
	}

	/* parent */
	/* open up /debug file */
	sprintf(pname, "/debug/%05d", pid);
	if ((pfd = open(pname, O_RDWR)) < 0) {
		fprintf(stderr, "watch:ERROR:Cannot open %s\n", pname);
		perror("");
		errexit();
	}

	/* stop process */
	stopit(pfd);
	unblockproc1(pid);

	if (ioctl(pfd, PIOCSTATUS, &psi) != 0) {
		perror("watch:ERROR:PSTATUS");
		errexit();
	}
	prps("1st stop", &psi);

	/* set up to get SIGTRAP & ALL faults (incl watchpoints) */
	premptyset(&sigs);
	praddset(&sigs, SIGTRAP);
	praddset(&sigs, SIGUSR1);
	if (ioctl(pfd, PIOCSTRACE, &sigs) != 0) {
		perror("watch:ERROR:PSTRACE");
		errexit();
	}
	prfillset(&faults);
	prdelset(&faults, FLTPAGE);
	if (ioctl(pfd, PIOCSFAULT, &faults) != 0) {
		perror("watch:ERROR:PSFAULT");
		errexit();
	}

	/* force text writable */
	if (lseek(pfd, (off_t)ftext, 0) <= 0) {
		perror("watch:ERROR:lseek text");
		errexit();
	}
	if (read(pfd, &inst, 4) != 4) {
		perror("watch:ERROR:read text");
		errexit();
	}
	lseek(pfd, (off_t)-4, SEEK_CUR);
	if (write(pfd, &inst, 4) != 4) {
		perror("watch:ERROR:write text");
		errexit();
	}

	/* set a bogus watchpoint */
	if (setwatch(pfd, (caddr_t)100L, 4, MA_READ) >= 0) {
		fprintf(stderr, "watch:ERROR:setwatch bogus worked!");
	}

	/* set a real watchpoint */
	if (setwatch(pfd, (char *)space, 4, MA_READ) < 0) {
		perror("watch:ERROR:setwatch");
		errexit();
	}

	/* run process */
	runit(pfd, 0);

	/* wait for it to stop */
	wstopit(pfd, &psi);
	prps("after 1st watchpoint", &psi);
	shaddr->state = 1;

	stepoverwatch("stepover 1st", pfd, (char *)space, &psi);

	/*
	 * test text watchpoints
	 */
	kid2b = (caddr_t)kid2 + (6 * 4); 
	/* set a real watchpoint */
	if (setwatch(pfd, kid2b, 4, MA_EXEC) < 0) {
		perror("watch:ERROR:setwatch kid2");
		errexit();
	}

	/* run process */
	runit(pfd, PRCFAULT);
	shaddr->state = 2;
	unblockproc1(pid);

	/* wait for it to stop */
	wstopit(pfd, &psi);
	stepoverwatch("stepover text", pfd, kid2b, &psi);

	/*
	 * Time stepping vs free running
	 */
	/* cause hitting 'space' to stop kid */
	if (setwatch(pfd, (char *)space, 4, MA_WRITE) < 0) {
		perror("watch:ERROR:setwatch");
		errexit();
	}
	/* run process */
	runit(pfd, PRCFAULT);
	shaddr->state = 5;
	unblockproc1(pid);
	wstopit(pfd, &psi);
	if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTWATCH) {
		fprintf(stderr, "watch:ERROR:missing FLTWATCH after freerun\n");
		prps("after freerun", &psi);
		printf("watch:Pid %d v0 0x%llx at 0x%llx\n",
			psi.pr_pid, (long long)psi.pr_reg[CTX_V0],
			(long long)psi.pr_reg[CTX_AT]);
		prwatch(pfd);
		errexit();
	}
	stepoverwatch("stepover after freerun", pfd, (caddr_t)space, &psi);

	/* now run it stepping running */
	if (setwatch(pfd, kid2b, 4, MA_EXEC) < 0) {
		perror("watch:ERROR:setwatch kid2");
		errexit();
	}
	/* cause hitting 'space' to stop kid */
	if (setwatch(pfd, (char *)space, 4, MA_WRITE) < 0) {
		perror("watch:setwatch");
		errexit();
	}
	/* run it past step */
	runit(pfd, PRCFAULT);
	shaddr->state = 10;
	unblockproc1(pid);
	/* wait to hit beginning of kid2 */
	wstopit(pfd, &psi);
	stepoverwatch("stepover kid2", pfd, kid2b, &psi);
	/* put back watchpoint (shouldn't hit it anymore */
	/* XXX set another at kid2+4096 */
	if (setwatch(pfd, kid2b, 4, MA_EXEC) < 0) {
		perror("watch:ERROR:setwatch kid2");
		errexit();
	}
	runit(pfd, PRCFAULT);
	wstopit(pfd, &psi);
	if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTWATCH) {
		fprintf(stderr, "watch:ERROR:missing FLTWATCH after kid2\n");
		prps("after kid2", &psi);
		printf("watch:Pid %d v0 0x%llx at 0x%llx\n",
			psi.pr_pid, (long long)psi.pr_reg[CTX_V0],
			(long long)psi.pr_reg[CTX_AT]);
		prwatch(pfd);
		errexit();
	}
	clrwatch(pfd, kid2b);
	stepoverwatch("stepover after steprun", pfd, (caddr_t)space, &psi);


	/*
	 * Test stepping into a watch point
	 * (I think that what this does!)
	 */
	if (setwatch(pfd, (char *)buf, 9, MA_WRITE) < 0) {
		perror("watch:ERROR:setwatch");
		errexit();
	}
	if (setwatch(pfd, (char *)space, 4, MA_WRITE) < 0) {
		perror("watch:ERROR:setwatch");
		errexit();
	}
	printf("watch:running after buf\n");
	if (dodrain)
		tcdrain(fileno(stdout));
	runit(pfd, PRCFAULT);
	shaddr->state = 15;
	unblockproc1(pid);
	wstopit(pfd, &psi);
	clrwatch(pfd, (caddr_t)buf);
	stepoverwatch("stepover after stack", pfd, (caddr_t)space, &psi);

	/*
	 * test getting watchpoint in system call
	 */
	if (setwatch(pfd, (char *)sbuf, 9, MA_WRITE) < 0) {
		perror("watch:ERROR:setwatch");
		errexit();
	}
	printf("watch:running after sbuf\n");
	if (dodrain)
		tcdrain(fileno(stdout));
	runit(pfd, PRCFAULT);
	shaddr->state = 20;
	unblockproc1(pid);
	wstopit(pfd, &psi);
	if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTKWATCH) {
		prps("ERROR:missing FLTKWATCH", &psi);
		errexit();
	}
	stepoverwatch("stepover after syscall", pfd, (caddr_t)sbuf, &psi);

	/*
	 * Test:
	 * child skipping watchpoints and gets a signal, then parent
	 * decides to single step
	 */
	/* we'll always skip this since its asking for writes */
	if (setwatch(pfd, (caddr_t)kid3, 4, MA_WRITE) < 0) {
		perror("watch:ERROR:setwatch kid3");
		errexit();
	}
	prwatch(pfd);
	runit(pfd, PRCFAULT);
	shaddr->state = 30;
	shaddr->leave = 0;
	unblockproc1(pid);
	printf("watch:waiting for child to pass 30\n");
	blockproc1(parentpid);
	unblockproc1(pid);

	printf("watch:starting signal/step test\n");
	if (dodrain)
		tcdrain(fileno(stdout));
	for (i = 0; i < 30; i++) {
		kill(pid, SIGUSR1);
		wstopit(pfd, &psi);
		if (getnwatch(pfd) != 1) {
			fprintf(stderr, "watch:ERROR:Watchpoint gone!\n");
			prps("gone watch", &psi);
			errexit();
		}
		if (verbose) {
			printf("stopped at 0x%llx flags 0x%x\n",
				(long long)psi.pr_reg[CTX_EPC], psi.pr_flags);
			if (dodrain)
				tcdrain(fileno(stdout));
		}
		if (psi.pr_why != PR_SIGNALLED || psi.pr_what != SIGUSR1) {
			fprintf(stderr, "watch:ERROR:expected SIGNAL\n");
			prps("step test", &psi);
			errexit();
		}
		runit(pfd, PRCSIG|PRSTEP);
		/* SIGUSR2 wont cause a /proc stop */
		kill(pid, SIGUSR2);
		if (verbose) {
			printf("Sstopped at ");
			if (dodrain)
				tcdrain(fileno(stdout));
		}
		for (j = 0; ; j++) {
			wstopit(pfd, &psi);
			if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTTRACE) {
				fprintf(stderr, "watch:ERROR:expected FAULT\n",
					psi.pr_why, psi.pr_what);
				prps("step test", &psi);
				errexit();
			}
			if (verbose) {
				printf("(0x%llx 0x%x) ",
					(long long)psi.pr_reg[CTX_EPC],
					psi.pr_flags);
				if (dodrain)
					tcdrain(fileno(stdout));
			}
			/*
			 * need to single step at least as long as kcatch
			 */
			if (j == 30) {
				runit(pfd, PRCFAULT);
				break;
			} else
				runit(pfd, PRCFAULT|PRSTEP);
		}
		if (verbose) {
			printf("\n");
			if (dodrain)
				tcdrain(fileno(stdout));
		}
	}
	shaddr->leave = 1;
	printf("watch:nsig %d\n", shaddr->nsig);
	if (clrwatch(pfd, (caddr_t)kid3) != 0) {
		perror("watch:clrwatch kid3");
		errexit();
	}

	shaddr->state = 40;
	unblockproc1(pid);
	printf("watch:waiting for child to pass 40\n");
	blockproc1(parentpid);

	/*
	 * Test - child is doing a ll/sc on a word we're not
	 * interested in - should get a FLTSCWATCH
	 */
	if (setwatch(pfd, (caddr_t)&ts[0], 4, MA_WRITE) < 0) {
		perror("watch:ERROR:setwatch llsc");
		errexit();
	}
	prwatch(pfd);
	unblockproc1(pid);
	wstopit(pfd, &psi);
	if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTSCWATCH) {
		prps("ERROR:missing FLTSCWATCH", &psi);
		errexit();
	}
	stepoverwatch("stepover after scwatch", pfd, (caddr_t)&ts[0], &psi);
	shaddr->state = 50;
	unblockproc1(pid);

	/* let child finish up */
	runit(pfd, PRCFAULT);
	close(pfd);	/* will run child */

	return 0;
}

void
kid(void)
{
	int mypid;
	clock_t ti, start;
	register volatile int i;
	struct tms tm;
	int fd;

	mypid = getpid();
	sigset(SIGUSR2, kcatch);
	blockproc1(mypid);

	if (shaddr->state != 0) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 0);
		errexit();
	}
	/* a write should NOT hit watchpoint */
	fflush(stdout); 
	*(volatile int *)space = 0;
	if (shaddr->state != 0) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 0);
		errexit();
	}

	printf("before read \n");
	i = *(volatile int *)space;

	blockproc1(mypid);

	if (shaddr->state != 2) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 2);
		errexit();
	}
	/* now check text watchpoints */
	kid2(0.0);

	blockproc1(mypid);
	if (shaddr->state != 5) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 5);
		errexit();
	}

	/* test stepping vs free running */
	start = times(&tm);

	kid2(0.0);

	ti = times(&tm) - start;
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:time %d mS free running from 0x%p to 0x%p\n",
		"watch", ti, (caddr_t)kid2, (caddr_t)kid3);

	/* hitting 'space' will stop us */
	*space = 0;
	blockproc1(mypid);
	if (shaddr->state != 10) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 10);
		errexit();
	}

	/* test stepping vs free running */
	start = times(&tm);

	kid2(0.0);

	ti = times(&tm) - start;
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:time %d mS stepping running from 0x%p to 0x%p\n",
		"watch", ti, (caddr_t)kid2, (caddr_t)kid3);
	if (dodrain)
		tcdrain(fileno(stdout));

	/* hitting 'space' will stop us */
	*space = 0;
	blockproc1(mypid);
	if (shaddr->state != 15) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 15);
		errexit();
	}

	/* hitting our stack will stop us */
	*space = 0;
	blockproc1(mypid);
	if (shaddr->state != 20) {
		fprintf(stderr, "watch:state %d should be %d\n",
			shaddr->state, 20);
		errexit();
	}

	/* now do system call */
	if ((fd = open("./watch", O_RDONLY)) < 0) {
		perror("watch:ERROR:kid:open of watch");
		errexit();
	}
	if (read(fd, sbuf, 4) != 4) {
		perror("watch:ERROR:kid:read error");
		errexit();
	}
	blockproc1(mypid);
	if (shaddr->state != 30) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 30);
		errexit();
	}
	unblockproc1(parentpid);
	blockproc1(mypid);

	/*
	 * test signal and skipping
	 */
	kid3();

	blockproc1(mypid);
	if (shaddr->state != 40) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 40);
		errexit();
	}
	unblockproc1(parentpid);
	blockproc1(mypid);

	/*
	 * test getting a watchpoint while in test_and_set
	 */
	test_and_set(&ts[1], 1);

	blockproc1(mypid);
	if (shaddr->state != 50) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 50);
		errexit();
	}
}

void
kid2(double a)
{
	register int i;
	register double j = 0.0;
	for (i = 0; i < 100; i++)
		j += 99.9 * a;
}

void
kid3(void)
{
	while (!shaddr->leave)
		;
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
		perror("watch:ERROR:NWATCH");
		errexit();
	}

	wp = (prwatch_t *)malloc(sizeof(*wp) * maxw);

	if ((nw = ioctl(pfd, PIOCGWATCH, (caddr_t)wp)) < 0) {
		perror("watch:ERROR:GWATCH");
		errexit();
	}
	for (i = 0; i < nw; i++, wp++) {
		printf("watch:Watchpoint at 0x%p len 0x%x mode 0x%x\n",
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

	if ((nw = ioctl(pfd, PIOCGWATCH, (caddr_t)wp)) < 0) {
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
kcatch(void)
{
	shaddr->nsig++;
}
