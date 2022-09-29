/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.8 $"

#include "sys/types.h"
#include "limits.h"
#include "unistd.h"
#include "stdlib.h"
#include "fcntl.h"
#include "stdio.h"
#include "signal.h"
#include "string.h"
#include "time.h"
#include "sys/mman.h"
#include "sys/times.h"
#include "errno.h"
#include "sys/prctl.h"
#include "sys/siginfo.h"
#include "sys/sysmp.h"
#include "wait.h"
#include "stress.h"
#include "getopt.h"
#include "ulocks.h"

/*
 * test sproc sp - giving our own stack
 */
#define ISOCPU	1
int killsig = SIGUSR2;
char *Cmd;
volatile int syncro = 0;
int touchstk;
long pagesz;
int nsigs = 20;
pid_t *pids;
struct si {
	pid_t pid;
	caddr_t stkhigh;
};
caddr_t *stks;
struct si **siptrs;	/* hold stack info ptrs indexed by threadnum */
barrier_t *b;
char *afile;
int nprocs = 10;		/* total procs created */
int verbose;
int spinonerror;
int isolate;
int mustrun;
extern void slave(void *, size_t);
extern void usage(void);
extern void segcatch(int, int, struct sigcontext *);

int
main(int argc, char **argv)
{
	register int c, i;
	pid_t pid;
	size_t stksize;
	auto int status;
	register usptr_t *handle;
	int fd;

	Cmd = errinit(argv[0]);
	stksize = 32*1024;
	pagesz = sysconf(_SC_PAGESIZE);
	while ((c = getopt(argc, argv, "miSvg:n:s:t")) != EOF) {
		switch (c) {
		case 'm':
			mustrun = 1;
			break;
		case 'i':
			isolate = 1;
			break;
		case 'S':
			spinonerror = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'g':
			nsigs = atoi(optarg);
			break;
		case 's':
			stksize = atoi(optarg) * 1024;
			break;
		case 't':
			/* touch bottom of stack */
			touchstk = 1;
			break;
		case 'n':
			nprocs = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}

	sigset(SIGSEGV, segcatch);
	sigset(SIGBUS, segcatch);
	/*
	 * in case parent decides to exit - this makes sure that all
	 * kids will be killed
	 */
	parentexinit(0);

	printf("%s:%d:Creating %d processes max stack size:%d Kb\n",
		Cmd, get_pid(), nprocs, stksize/1024);

	if (isolate || mustrun) {
		/*
		 * If the machine only has a single CPU,
		 *  ignore isolate and mustrun requests.
		 */
		if (sysmp(MP_NPROCS) == 1) {
			printf("%s:%d:Cannot mustrun: running in uni-processor mode\n",
			       Cmd, get_pid());
			isolate = mustrun = 0;
		} else {
			if (sysmp(MP_MUSTRUN, ISOCPU) != 0) {
				errprintf(ERR_ERRNO_RET,
					  "mustrun failed - aborting\n");
				exit(0);
			}
		}
	}

	if (isolate) {
		if (sysmp(MP_ISOLATE, ISOCPU) != 0) {
			errprintf(ERR_ERRNO_RET, "isolate failed - aborting\n");
			exit(0);
		}
		printf("%s:isolated cpu %d\n", Cmd, ISOCPU);
	}

	/* create stacks */
	stks = calloc(nprocs, sizeof(void *));
	fd = open("/dev/zero", O_RDWR);

	for (i = 0; i < nprocs; i++) {
		if ((stks[i] = mmap(0, stksize, PROT_READ|PROT_WRITE, 
				 MAP_PRIVATE, fd, 0)) == (caddr_t)-1L) {
			errprintf(ERR_ERRNO_EXIT, "mmap failed for stack # %d", i);
			/* NOTREACHED */
		}
	}

	syncro = 1;
	pids = calloc(nprocs, sizeof(pid_t));
	siptrs = calloc(nprocs, sizeof(void *));

	if (usconfig(CONF_INITUSERS, nprocs+1) < 0)
		errprintf(1, "INITUSERS");
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	afile = tempnam(NULL, "sprocsp");
	if ((handle = usinit(afile)) == NULL)
		errprintf(ERR_ERRNO_EXIT, "usinit of %s failed", afile);
		/* NOTHREACHED */

	if ((b = new_barrier(handle)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "new_barrier failed");
		/* NOTHREACHED */
	}

	for (i = 0; i < nprocs; i++) {
		if ((pids[i] = sprocsp(slave, PR_SALL, (void *)(long)i, stks[i]+stksize, stksize)) < 0) {
			if (errno != EAGAIN) {
				errprintf(1, "sproc failed");
				/* NOTREACHED */
			}
			printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
			/*
			 * can't continue cause of barriers below..
			 */
			if (isolate)
				sysmp(MP_UNISOLATE, ISOCPU);
			exit(1);
		}
		if (verbose)
			printf("%s:created thread %d pid %d\n", Cmd, i, pids[i]);
	}
	syncro = 0;
	/* wait for all threads to finish */
	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if ((pid >= 0) && WIFSIGNALED(status)) {
			/*
			 * if anyone dies abnormally - get out .. since
			 * the barriers below will hang
			 * But don't abort - the process that got the signal
			 * hopefully aborted - that core dump is more
			 * interesting
			 */
			errprintf(2, "proc %d died of signal %d\n",
				pid, WTERMSIG(status));
			if (!WCOREDUMP(status))
				abort();
			if (isolate)
				sysmp(MP_UNISOLATE, ISOCPU);
			exit(1);
		}
	}
	if (isolate)
		sysmp(MP_UNISOLATE, ISOCPU);
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
}

/* ARGSUSED */
void
sigcatcher(int sig, siginfo_t *sigi)
{
	int mythread;
	struct si *si;

	if (verbose > 1)
		printf("%s:pid %d got signal %d  - siginfo @ 0x%x\n",
			Cmd, get_pid(), sig, sigi);
	if (sigi == NULL) {
		errprintf(ERR_RET, "pid %d NULL siginfo for sig %d",
				get_pid(), sig);
		if (spinonerror)
			for (;;)
				;
		abort();
		/* NOTREACHED */
	}
	if (sigi->si_code != SI_QUEUE)
		errprintf(0, "thread pid %d has signal but no SI_QUEUE",
			get_pid());
		/* NOTREACHED */
	mythread = (int)sigi->si_value.sival_int;
	
	si = siptrs[mythread];

	if ((caddr_t)sigi > si->stkhigh ||
				(caddr_t)sigi < (si->stkhigh - 4096)) {
		errprintf(0, "bad stack pid %d @0x%x thread %d stkhigh 0x%x",
			get_pid(), sigi, mythread, si->stkhigh);
		/* NOTREACHED */
	}
}

struct timespec nano = { 1, 1002 };

void
slave(void *arg, size_t stksize)
{
	struct sigaction siga;
	pid_t spid, mypid;
	int i, threadnum;
	struct si si;
	union sigval sv;
	int sthread;

	slaveexinit();

	threadnum = (int)(long) arg;
	mypid = get_pid();
	if ((mustrun || isolate) && (threadnum % 2))
		if (sysmp(MP_RUNANYWHERE) != 0) {
			errprintf(ERR_ERRNO_EXIT, "runanywhere failed\n");
			/* NOTREACHED */
		}
	if (verbose)
		printf("%s:pid %d thread %d ssize %d\n",
			Cmd, mypid, threadnum, stksize);

	if (touchstk) {
		caddr_t stkbot;

		/* round up to page, then subtract stksize */
		stkbot = (caddr_t)((((size_t)&arg + pagesz - 1) / pagesz) * pagesz);
		stkbot -= stksize;
		if (*stkbot != 0)
			errprintf(0, "Bottom of stack not 0!");
			/* NOTREACHED */
	}
	/* wait for all threads to be created */
	while (syncro)
		nanosleep(&nano, NULL);

	/*
	 * set up a signal handler
	 * odd threads go onto alternate stacks
	 * even go onto their own stacks
	 */
	sigemptyset(&siga.sa_mask);
	siga.sa_handler = sigcatcher;
	siga.sa_flags = SA_SIGINFO| (threadnum & 1 ? SA_ONSTACK : 0);
	si.pid = mypid;

	if (threadnum & 1) {
		/* set up alternate stack */
		caddr_t newstk;
		stack_t ss;

		newstk = malloc(4096);
		ss.ss_sp = newstk + 4096 - 1;
		ss.ss_size = 4096;
		ss.ss_flags = 0;
		if (sigaltstack(&ss, NULL) != 0)
			errprintf(1, "sigaltstk failed thread #%d", threadnum);
			/* NOTREACHED */

		si.stkhigh = (caddr_t)(newstk + 4096);
	} else {
		si.stkhigh = (caddr_t)&stksize;
	}
	siptrs[threadnum] = &si;

	if (sigaction(killsig, &siga, NULL) != 0)
		errprintf(1, "sigaction failed for thread #%d", threadnum);
		/* NOTREACHED */

	/* wait for all threads to get here */
	sginap(1);
	barrier(b, nprocs);

	/* send a random thread a signal */
	for (i = 0; i < nsigs; i++) {
		sthread = rand()%nprocs;
		sv.sival_int = sthread;
		spid = pids[sthread];
		if (spid == 0)
			errprintf(0, "Bad pid thread #%d", threadnum);
again:
		if (sigqueue(spid, killsig, sv) != 0) {
			if (errno == EAGAIN) {
				sginap(1);
				goto again;
			}
			errprintf(1, "sigqueue failed for pid %d sending to %d",
				get_pid(), spid);
			/* NOTREACHED */
		}
	}
	/* wait for all threads to get here */
	barrier(b, nprocs);

	_exit(0);
}

void
usage(void)
{
	fprintf(stderr, "Usage:%s [-vtmi] [-s #][-n #][-g #]\n", Cmd);
	fprintf(stderr, "\t-t\thave slaves touch bottom of stack\n");
	fprintf(stderr, "\t-n#\tcreate 'n' threads\n");
	fprintf(stderr, "\t-s#\tset each slaves stack size to 'n'Kb\n");
	fprintf(stderr, "\t-g#\tsend 'n' signals\n");
	fprintf(stderr, "\t-i\tisolate a cpu\n");
	fprintf(stderr, "\t-m\tforce even procs to mustrun\n");
	exit(-1);
}

void
segcatch(int sig, int code, struct sigcontext *sc)
{
	fprintf(stderr, "%s:%d:ERROR: caught signal %d at pc 0x%llx code %d badvaddr %llx\n",
			Cmd, getpid(), sig, sc->sc_pc, code,
			sc->sc_badvaddr);
	abort();
	/* NOTREACHED */
}
