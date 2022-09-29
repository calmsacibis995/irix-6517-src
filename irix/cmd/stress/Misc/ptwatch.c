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

#ident	"$Revision: 1.2 $ $Author: kanoj $"

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
char sbuf[20];
unsigned long ts[3];
typedef struct {
	volatile int state;
	volatile int leave;
	volatile int nsig;
} sstate_t;

struct barr {
	volatile int count;
	volatile int spin;
	volatile int use;
	sem_t	     lock;
} *bar;

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
void kid2f(double a);
void kid(int);
void instruct_children(int, char *, char *, int, int);

#ifdef PTHREAD
void ptkid(void);
int numthrds = 2;	/* # uthreads */
int numpthrds = 2;	/* # pthreads */
#else
int numthrds = 1;
int numpthrds = 1;
#endif
int numwait;

/*
 * All children wait for the next set of tests and continue when
 * instructed to do so by parent.
 */
void
wait_at_barrier(int tid)
{
	/*
	 * Wait for everyone to come out of barrier and start last stage.
	 */
	/* while (bar->use) sched_yield(); */
	while (bar->use);

	/*
	 * Grab barrier lock
	 */
	if (sem_wait(&bar->lock) != 0) {
		printf("wait_at_barrier error\n");
		exit(1);
	}

	/*
	 * Indicate that one more thread has arrived at barrier.
	 */
	bar->count++;

	/*
	 * Release lock 
	 */
	if (sem_post(&bar->lock) != 0) {
		printf("wait_at_barrier error1\n");
		exit(1);
	}

	/*
	 * Wait for parent to tell us it is okay to go.
	 */
	/* while (bar->spin) sched_yield(); */
	while (bar->spin);

	/*
	 * Grab barrier lock
	 */
	if (sem_wait(&bar->lock) != 0) {
		printf("wait_at_barrier error2\n");
		exit(1);
	}

	if (--bar->count == 0) {
		bar->spin = 1;
		bar->use = 0;
	}

	/*
	 * Release lock 
	 */
	if (sem_post(&bar->lock) != 0) {
		printf("wait_at_barrier error3\n");
		exit(1);
	}
}


/*
 * Instruct all children stuck at the barrier to go forward.
 */ 
void 
continue_from_barrier(void) 
{
	bar->use = 1;
	bar->spin = 0; 
}

/*
 * Parent waits for all children to hit the barrier.
 */
void
wait_for_barrier(void)
{
	int i;
/*
	while (bar->use) sched_yield();
	while (bar->count != numpthrds) sched_yield();
*/
	while (bar->use);
	while (bar->count != numpthrds);
}


void 
initproc(char *buffer)
{
	if ((numpthrds % 2) == 0)
		numwait = (numpthrds/2);
	else
		numwait = (numpthrds/2) + 1;
	bar = (struct barr *)buffer;
	bar->use = 0;
	bar->spin = 1; 
	bar->count = 0; 
	if (sem_init(&bar->lock, 1, 1) != 0) {
		printf("barrier sema init failure\n");
		exit(1);
	}
}

void
getstatus(int pfd, int tid, prstatus_t *psi)
{
	prthreadctl_t	ptc;

	ptc.pt_tid = tid;
	ptc.pt_flags = PTFD_EQL | PTFS_ALL;
	ptc.pt_cmd = PIOCSTATUS;
	ptc.pt_data = (caddr_t)psi;
	if (ioctl(pfd, PIOCTHREAD, &ptc) != 0) {
		perror("watch:ERROR:PIOCSTATUS");
		printf("Failed on tid %d got back %d\n", tid, ptc.pt_tid);
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
wstopthrd(int pfd, int tid, prstatus_t *psi)
{
	prthreadctl_t   ptc;
	sigset_t	sigs;

	ptc.pt_tid = tid;
	ptc.pt_flags = PTFD_EQL | PTFS_ALL;
	ptc.pt_cmd = PIOCWSTOP;
	ptc.pt_data = (caddr_t)psi;
	if (ioctl(pfd, PIOCTHREAD, &ptc) != 0) {
		perror("watch:ERROR:wstopthrd");
		errexit();
	}
	premptyset(&sigs);
	ptc.pt_cmd = PIOCSHOLD;
	ptc.pt_data = &sigs;
	if (ioctl(pfd, PIOCTHREAD, &ptc) != 0) {
		perror("watch:ERROR:wstopthrd1");
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

void
runthrd(int pfd, int tid, long flags)
{
	prrun_t 	pr;
	prthreadctl_t	ptc;

	ptc.pt_tid = tid;
	ptc.pt_flags = PTFD_EQL | PTFS_ALL;
	ptc.pt_cmd = PIOCRUN;
	ptc.pt_data = (caddr_t)&pr;
	pr.pr_flags = flags | PRSHOLD;
	prfillset(&pr.pr_sighold);
	if (ioctl(pfd, PIOCTHREAD, &ptc) != 0) {
		perror("watch:ERROR:runthrd");
		fprintf(stderr, "watch:ERROR:flags 0x%x tid %d\n", flags, tid);
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
stepoverwatch(char *s, int pfd, int tid, caddr_t addr, prstatus_t *opsi)
{
	prstatus_t psi;
	prwatch_t  *list;
	int 	   i, num, loops = 1;

restart:
/*
	if (opsi->pr_reg[CTX_EPC] < 0x10000000) {
		printf("STOPPED IN SIGNAL HANDLER 0x%x\n", opsi->pr_reg[CTX_EPC] );
		errexit();
	}
*/
	printf("watch:stepoverwatch on thread %d\n", tid);
	if (opsi->pr_why != PR_FAULTED) {
		fprintf(stderr, "watch:ERROR:%s !FAULTED; why=%d tid=%d\n",
			s, opsi->pr_why, tid);
		errexit();
	}
	prps(s, opsi);

	if (opsi->pr_what == FLTKWATCH) {
		/* there is nothing to skip over since it occurred in
		 * a system call
		 */
		return;
	}

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

	if (opsi->pr_what != FLTSCWATCH && opsi->pr_info.si_addr != addr) {
		fprintf(stderr, "watch:ERROR:%s stepping over 0x%p but faulted at 0x%p\n",
			s, addr, opsi->pr_info.si_addr);
		errexit();
	}

	/* step process - this might end up in a signal handler */
	runthrd(pfd, tid, PRSTEP|PRCFAULT);
	wstopthrd(pfd, tid, &psi);
	prps(s, &psi);
	if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTTRACE) {
		perror("watch:ERROR:stepoverwatch:stop not FAULTED");
		errexit();
	}

	if (setwatch(pfd, list[num].pr_vaddr, list[num].pr_size, 
						list[num].pr_wflags)) {
		perror("setwatch in stepoverwatch");
		errexit();
	}
	free(list);
}

int
main(int argc, char **argv)
{
	int c, i, j, k;
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
#ifdef PTHREAD
	long flags = PR_JOINTSTOP|PR_JOINTPOLL;
#endif

	setlinebuf(stdout);
	setlinebuf(stderr);
	pid = 0;
#ifdef PTHREAD
	while((c = getopt(argc, argv, "dp:u:v")) != EOF)
#else
	while((c = getopt(argc, argv, "dp:v")) != EOF)
#endif
	switch (c) {
#ifdef PTHREAD
	case 'u':
		numthrds = atoi(optarg);
		break;
	case 'p':
		numpthrds = atoi(optarg);
		break;
#else
	case 'p':
		pid = atoi(optarg);
		break;
#endif
	case 'd':
		dodrain++;
		exitonerror = 0;
		break;
	case 'v':
		verbose++;
		break;
	default:
#ifdef PTHREAD
		fprintf(stderr, "Usage:watch [-d][-v][-u #][-p #]\n");
		fprintf(stderr, "\t-u# - specify #uthreads\n");
		fprintf(stderr, "\t-p# - specify #pthreads\n");
#else
		fprintf(stderr, "Usage:watch [-d][-v][-p #]\n");
		fprintf(stderr, "\t-p# - specify pid\n");
#endif
		fprintf(stderr, "\t-d - debug\n");
		fprintf(stderr, "\t-v - verbose\n");
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
	initproc((char *)shaddr + sizeof(sstate_t));
	if (pid == 0) {
		space = (volatile char *)malloc(10000);
		/* fork our own kid */
		if ((pid = fork()) < 0) {
			perror("watch:ERROR:fork");
			errexit();
		} else if (pid == 0) {
#ifdef PTHREAD
			ptkid();
#else
			kid(0);
#endif
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

	/*
	 * Wait for all threads to start up.
	 */
	wait_for_barrier();
#ifdef PTHREAD
	if (ioctl(pfd, PIOCSET, &flags) != 0) {
		perror("watch:ERROR:PIOCSET");
		errexit();
	}
#endif
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

	/*
	 * Parent sets up state 0 tests.
	 */

	/* set a bogus watchpoint */
	if (setwatch(pfd, (caddr_t)100L, 4, MA_READ) >= 0) {
		fprintf(stderr, "watch:ERROR:setwatch bogus worked!");
	}

	/* set a real watchpoint */
	if (setwatch(pfd, (char *)space, 4, MA_READ) < 0) {
		perror("watch:setwatch1");
		errexit();
	}

	/* 
	 * Let children do state 0 tests.
	 */
	instruct_children(pfd, "stepover 1st", (char *)space, 0, 0);

	/*
	 * test text watchpoints
	 */
	kid2b = (caddr_t)kid2 + (6 * 4); 
	/* set a real watchpoint */
	if (setwatch(pfd, kid2b, 4, MA_EXEC) < 0) {
		perror("watch:setwatch2");
		errexit();
	}

	/* 
	 * Let children do state 2 tests.
	 */
	shaddr->state = 2;
	instruct_children(pfd, "stepover text", kid2b, 0, 0);

	/*
	 * Time stepping vs free running
	 */
	/* cause hitting 'space' to stop kid */
	if (setwatch(pfd, (char *)space, 4, MA_WRITE) < 0) {
		perror("watch:setwatch3");
		errexit();
	}

	/* 
	 * Let children do state 5 tests.
	 */
	shaddr->state = 5;
	instruct_children(pfd, "stepover after freerun", (caddr_t)space, 
					PR_FAULTED, FLTWATCH);

	/* now run it stepping running */
	if (setwatch(pfd, kid2b, 4, MA_EXEC) < 0) {
		perror("watch:setwatch4");
		errexit();
	}
	/* 
	 * Let children do state 10 tests.
	 */
	shaddr->state = 10;
	instruct_children(pfd, "stepover kid2", kid2b, 0, 0);

	/*
	 * test getting watchpoint in system call
	 */
	if (setwatch(pfd, (char *)sbuf, 9, MA_WRITE) < 0) {
		perror("watch:setwatch8");
		errexit();
	}
	printf("watch:running after sbuf\n");
	if (dodrain)
		tcdrain(fileno(stdout));
	/* 
	 * Let children do state 20 tests.
	 */
	shaddr->state = 20;
	instruct_children(pfd, "stepover after syscall", (caddr_t)sbuf,
					PR_FAULTED, FLTKWATCH);

	/*
	 * Test:
	 * child skipping watchpoints and gets a signal, then parent
	 * decides to single step
	 */
	/* we'll always skip this since its asking for writes */
	if (setwatch(pfd, (caddr_t)kid3, 4, MA_WRITE) < 0) {
		perror("watch:setwatch9");
		errexit();
	}
	prwatch(pfd);
	shaddr->state = 30;
	shaddr->leave = 0;
	printf("watch:waiting for child to pass 30\n");
	continue_from_barrier();
#if 0
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
		/*
		 * Only one stopped due to <PR_SIGNALLED, SIGUSR1>
		 */
		for (c = k = 0; k < numthrds; k++) {
			getstatus(pfd, k, &psi);
			if (psi.pr_why == PR_SIGNALLED &&
					psi.pr_what == SIGUSR1) {
				c++;
				/* runthrd(pfd, k, PRCSIG); */
			} else if (psi.pr_why != PR_REQUESTED) {
				fprintf(stderr, "watch:expected REQ\n");
				prps("step test", &psi);
				errexit();
			}
		}
		if (c != 1) {
			fprintf(stderr, "watch:got %d sigstop\n", c);
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
			for (k = 0; k < numthrds; k++) {
				getstatus(pfd, k, &psi);
				if (psi.pr_why != PR_FAULTED ||
						psi.pr_what != FLTTRACE) {
					fprintf(stderr,"watch:ERROR:xpected FAULT\n");
					prps("step test", &psi);
					errexit();
				}
				if (verbose) {
					printf("tid %d:(0x%llx 0x%x) ", k,
					      (long long)psi.pr_reg[CTX_EPC],
					      psi.pr_flags);
					if (dodrain)
						tcdrain(fileno(stdout));
				}
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
#endif
	shaddr->leave = 1;
	printf("watch:nsig %d\n", shaddr->nsig);

	/* 
	 * Wait for state 30 test completion.
	 */
	wait_for_barrier();

	if (clrwatch(pfd, (caddr_t)kid3) != 0) {
		perror("watch:clrwatch kid3");
		errexit();
	}

	printf("watch:waiting for child to pass 40\n");

	/*
	 * Test - child is doing a ll/sc on a word we're not
	 * interested in - should get a FLTSCWATCH
	 */
	if (setwatch(pfd, (caddr_t)&ts[0], 4, MA_WRITE) < 0) {
		perror("watch:setwatch10");
		errexit();
	}
	prwatch(pfd);
	/* 
	 * Let children do state 40 tests.
	 */
	shaddr->state = 40;
	instruct_children(pfd, "stepover after scwatch", (caddr_t)&ts[0],
						PR_FAULTED, FLTSCWATCH);

	shaddr->state = 50;
	continue_from_barrier();

	/* let child finish up */
	close(pfd);	/* will run child */

	return 0;
}

#ifdef PTHREAD
void
ptkid(void)
{
	int 		e, i;
	pthread_t	pt;

	/*
	 * Create as much concurrency as needed.
	 */
	if (e = pthread_setconcurrency(numthrds)) {
		printf("setconcurrency %d\n", e);
		exit(1);
	}

	for (i = 0; i < (numpthrds -1); i++) {
		if (e = pthread_create(&pt, 0, (void *(*)(void*))kid, 
							(i + 1))) {
			printf("create %d\n", e);
			exit(1);
		}
	}
	kid(0);
}
#endif

void
kid(int tid)
{
	int mypid;
	clock_t ti, start;
	register volatile int i;
	struct tms tm;
	int fd;
	int real = ((tid % 2) == 0);

	mypid = getpid();
	sigset(SIGUSR2, kcatch);

	/*
	 * Wait for parent to set up state 0 tests.
	 */
	wait_at_barrier(tid);

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

	printf("tid %d before read\n", tid);
	if (real) 
		i = *(volatile int *)space;
	else 
		i = *((volatile int *)space + 4);

	/*
	 * Wait for parent to set up state 2 tests.
	 */
	wait_at_barrier(tid);
	if (shaddr->state != 2) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 2);
		errexit();
	}
	/* now check text watchpoints */
	if (real)
		kid2(0.0);
	else
		kid2f(0.0);

	/*
	 * Wait for parent to set up state 5 tests.
	 */
	wait_at_barrier(tid);
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
	if (real)
		*space = 0;
	else
		*((int *)space + 4) = 0;

	/*
	 * Wait for parent to set up state 10 tests.
	 */
	wait_at_barrier(tid);
	if (shaddr->state != 10) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 10);
		errexit();
	}

	/* test stepping vs free running */
	start = times(&tm);

	if (real)
		kid2(0.0);
	else
		kid2f(0.0);

	ti = times(&tm) - start;
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:time %d mS stepping running from 0x%p to 0x%p\n",
		"watch", ti, (caddr_t)kid2, (caddr_t)kid3);
	if (dodrain)
		tcdrain(fileno(stdout));

	/*
	 * Wait for parent to set up state 20 tests.
	 */
	wait_at_barrier(tid);

	if (shaddr->state != 20) {
		fprintf(stderr, "watch:state %d should be %d\n",
			shaddr->state, 20);
		errexit();
	}

	if (real) {
		/* now do system call */
		if ((fd = open("./watch", O_RDONLY)) < 0) {
			perror("watch:ERROR:kid:open of watch");
			errexit();
		}
		if (read(fd, sbuf, 4) != 4) {
			perror("watch:ERROR:kid:read error");
			errexit();
		}
	}
	else 
		*((char *)sbuf + 16) = 0;

	/*
	 * Wait for parent to set up state 30 tests.
	 */
	wait_at_barrier(tid);

	if (shaddr->state != 30) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 30);
		errexit();
	}

	/*
	 * test signal and skipping
	 */
	kid3();

	/*
	 * Wait for parent to set up for state 40 tests.
	 */
	wait_at_barrier(tid);

	if (shaddr->state != 40) {
		fprintf(stderr, "watch:ERROR:state %d should be %d\n",
			shaddr->state, 40);
		errexit();
	}

	/*
	 * test getting a watchpoint while in test_and_set
	 */
	if (real)
		/*
		 * Turn the looping test off. Look at the comment in
		 * instruct_children. The special code to handle looping
		 * tests is not adequate. This is because if one 
		 * thread is in a wpt stop, and the parent does a 
		 * requested-stop on it, the child thread might end 
		 * up needing two run commands to come back into 
		 * user mode. (In the kernel, swtch() does the real 
		 * stop first, then after the first run, it checks 
		 * for and does requested stops. It is not evident 
		 * whether this is a kernel bug, or that this test 
		 * program needs to be aware of the fact. Turn off 
		 * looping tests for the moment. Bug 545339.
		 *
		 * test_and_set(&ts[1], 1);
		 */
		ts[0] = 1;
	else
		ts[2] = 1;	

	/*
	 * Wait for parent to set up for exit.
	 */
	wait_at_barrier(tid);

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
kid2f(double a)
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

void
instruct_children(int fd, char *msg, char *wptaddr, int why, int what)
{
	prstatus_t	psi;
	int		number = 0, i;

	continue_from_barrier();
	while (number < numwait) {
		wstopit(fd, &psi);
		for (i = 0; i < numthrds; i++) {
			getstatus(fd, i, &psi);
			if (psi.pr_why == PR_FAULTED) {
				number++;
				if ((why != 0) && 
				 (psi.pr_why != why || psi.pr_what != what) &&
				 (shaddr->state != 40)) {
					fprintf(stderr, "watch:ERROR:");
					fprintf(stderr, msg);
					fprintf(stderr, ": %d\n", i);
					prps(msg, &psi);
					printf("watch:Pid %d v0 0x%llx at 0x%llx\n", psi.pr_pid, (long long)psi.pr_reg[CTX_V0], (long long)psi.pr_reg[CTX_AT]);
					prwatch(fd);
					errexit();
				}
				stepoverwatch(msg, fd, i, wptaddr, &psi);
			}
		}
		runit(fd, PRCFAULT);
	}

	/* clear watchpoint */
	if (clrwatch(fd, wptaddr) < 0) {
		perror("watch:instruct_children:clrwatch");
		errexit();
	}

	/*
	 * NOTE: The foll 4 lines of code is needed to handle any test
	 * case where the child keeps hitting a wpt in a loop. Between
	 * the time that the parent finishes doing the above loop /
	 * running the process for the last time and the time it does
	 * the clrwatch, the child might run and end up either stopped
	 * or on its way to getting stopped because it has hit the wpt
	 * again. In turn, the other threads might be requested to stop.
	 * The foll code just makes sure everyone is stopped, and then 
	 * gets them running again.
	 */
	if (shaddr->state == 40)  {		/* scwatch test */
		stopit(fd);
		runit(fd, PRCFAULT);
	}


	/*
	 * Wait for this state test completion.
	 */
	wait_for_barrier();
}
