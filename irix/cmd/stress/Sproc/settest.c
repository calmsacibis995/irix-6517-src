/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.2 $"

#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "ulocks.h"
#include "string.h"
#include "time.h"
#include "errno.h"
#include "getopt.h"
#include "malloc.h"
#include "mutex.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/times.h"
#include "sys/stat.h"
#include "stress.h"
#include "wait.h"

/*
 * settest - check out that proper things happen when locks are
 *	contended for.
 */
char *Cmd;

extern int _utrace;
int verbose = 0;
int debug = 0;
ulock_t l;
pid_t ppid;
int stopslave = 0;
char *afile;
int ptime = 15;
struct timespec nano = { 0, 1001 };
struct timespec nano2 = { 0, 1002 };
volatile unsigned long sstarted;

extern void slave(void *arg);

int
main(int argc, char **argv)
{
	int c;
	usptr_t *usp;
	int nprocs, nloops;
	int i;
	int rv, nslaves;
	pid_t pid;
	auto int status;
	lockmeter_t lm;

	Cmd = errinit(argv[0]);
	nloops = 40;
	nprocs = 1;
	while ((c = getopt(argc, argv, "i:tp:dn:v")) != -1)
	switch(c) {
	case 'v':	/* verbose */
		verbose = 1;
		break;
	case 't':
		_utrace = 1;
		break;
	case 'n':
		nloops = atoi(optarg);
		break;
	case 'd':
		debug++;
		if (usconfig(CONF_LOCKTYPE, US_DEBUG) < 0) {
			errprintf(1, "cannot set DEBUG locks");
			/* NOTREACHED */
		}
		break;
	case 'p':
		nprocs = atoi(optarg);
		break;
	case 'i':	/* interval to hold lock in ticks */
		ptime = atoi(optarg);
		break;
	default:
		fprintf(stderr, "Usage:%s [-vdt][-n nloops][-p nprocs][-i interval]\n",
						Cmd);
		fprintf(stderr, "\t-i# - # of ticks to hold lock\n");
		fprintf(stderr, "\t-p# - # of processes\n");
		fprintf(stderr, "\t-n# - # of loops\n");
		fprintf(stderr, "\t-v - verbose\n");
		fprintf(stderr, "\t-d - use debug locks\n");
		fprintf(stderr, "\t-t - turn on utrace\n");
		exit(-1);
	}

	if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) < 0) {
		errprintf(1, "usconfig ARENATYPE failed");
		/* NOTREACHED */
	}
	afile = tempnam(NULL, "settest");
	if (usconfig(CONF_INITUSERS, nprocs+1) < 0) {
		errprintf(1, "usconfig USERS failed");
		/* NOTREACHED */
	}
	if ((usp = usinit(afile)) == NULL) {
		errprintf(1, "usinit");
		/* NOTREACHED */
	}

	l = usnewlock(usp);

	/*
	 * in case parent decides to exit - this makes sure that all
	 * kids will be killed
	 */
	parentexinit(0);

	prctl(PR_SETSTACKSIZE, 64*1024);
	ppid = getpid();
	for (i = 0, nslaves = 0; i < nprocs; i++) {
		if (sproc(slave, PR_SALL, nloops) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_EXIT, "sproc failed");
				/* NOTREACHED */
			}
			printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
			/* keep going with what we've got */
			continue;
		}
		nslaves++;
	}

	while (sstarted != nslaves)
		sginap(1);

	for (i = 0; i < nloops; i++) {
		if ((rv = ussetlock(l)) != 1) {
			errprintf(1, "ussetlock error:%d", rv);
			/* NOTREACHED */
		}
		usunsetlock(l);
		nanosleep(&nano, NULL);
	}

	stopslave++;
	/* wait for all threads to finish */
	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if ((pid >= 0) && WIFSIGNALED(status)) {
			/*
			 * if anyone dies abnormally - get out
			 * But don't abort - the process that got the signal
			 * hopefully aborted - that core dump is more
			 * interesting
			 */
			errprintf(2, "proc %d died of signal %d\n",
				pid, WTERMSIG(status));
			if (!WCOREDUMP(status))
				abort();
			exit(1);
		}
	}

	if (verbose)
		usdumplock(l, stderr, "settest lock");
	if (debug) {
		usctllock(l, CL_METERFETCH, &lm);
		printf("%s:spins %d hits %d\n", Cmd, lm.lm_spins, lm.lm_hits);
	}
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
}

/* ARGSUSED */
void
slave(void *arg)
{
	register int i;
	register volatile float a, b, c;

	slaveexinit();

	if (usinit(afile) == NULL) {
		errprintf(1, "slave:usinit");
		/* NOTREACHED */
	}
	test_then_add((unsigned long *)&sstarted, 1);
	for (;;) {
		if (stopslave)
			exit(0);
		/*
		 * we grab lock and hold it for a while
		 * then release, then pause a short bit and do it again.
		 */
		ussetlock(l);
		sginap(ptime);
		usunsetlock(l);
		nanosleep(&nano2, NULL);
		for (i = 0; i < 1000; i++)
			a = b * c * (float)i;
	}
}
