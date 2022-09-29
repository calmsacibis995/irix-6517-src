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

#ident "$Revision: 1.7 $"

#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "ulocks.h"
#include "string.h"
#include "errno.h"
#include "getopt.h"
#include "malloc.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/times.h"
#include "sys/stat.h"
#include "stress.h"
#include "wait.h"

/*
 * csettest - test out csetlock and cpsema
 */
char *Cmd;

extern int _utrace;
int verbose = 0;
int debug = 0;
ulock_t l;
usema_t *s;
pid_t ppid;
int dosema = 0;
int innloops = 400;
int sgotit = 0;
int stopslave = 0;

extern void slave(void *arg);

int
main(int argc, char **argv)
{
	int c;
	usptr_t *usp;
	int nprocs, nloops;
	int j, i, outof, gotit;
	int rv;
	pid_t pid;
	auto int status;
	semameter_t sm;
	lockmeter_t lm;

	Cmd = errinit(argv[0]);
	nloops = 1000;
	nprocs = 1;
	while ((c = getopt(argc, argv, "itsp:dn:v")) != -1)
	switch(c) {
	case 's':
		dosema++;
		break;
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
	case 'i':	/* # inner loops */
		innloops = atoi(optarg);
		break;
	default:
		fprintf(stderr, "Usage:csettest [-v][-d][-n nloops][-p nprocs][-s]\n");
		fprintf(stderr, "\t-i# - # of inner loops\n");
		fprintf(stderr, "\t-e - loop on error\n");
		fprintf(stderr, "\t-p# - # of processes\n");
		fprintf(stderr, "\t-n# - # of outer loops\n");
		fprintf(stderr, "\t-v - verbose\n");
		fprintf(stderr, "\t-s - use semaphores rather than locks\n");
		fprintf(stderr, "\t-d - use debug locks\n");
		fprintf(stderr, "\t-t - turn on utrace\n");
		exit(-1);
	}

	if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) < 0) {
		errprintf(1, "usconfig ARENATYPE failed");
		/* NOTREACHED */
	}
	/* init 2* so 2 copies of csettest can run at once!
	 * (since they use the same arena file)
	 */
	if (usconfig(CONF_INITUSERS, (nprocs+1)*2) < 0) {
		errprintf(1, "usconfig USERS failed");
		/* NOTREACHED */
	}
	if ((usp = usinit("/usr/tmp/csettest.arena")) == NULL) {
		errprintf(1, "usinit");
		/* NOTREACHED */
	}

	l = usnewlock(usp);
	s = usnewsema(usp, 1);
	usctlsema(s, CS_METERON);
	if (debug) {
		usctlsema(s, CS_DEBUGON);
	}

	/*
	 * in case parent decides to exit - this makes sure that all
	 * kids will be killed
	 */
	parentexinit(0);

	prctl(PR_SETSTACKSIZE, 64*1024);
	ppid = getpid();
	for (i = 0; i < nprocs; i++) {
		if (sproc(slave, PR_SALL, nloops) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_EXIT, "sproc failed");
				/* NOTREACHED */
			}
			printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
			/* keep going with what we've got */
		}
	}

	gotit = 0;
	outof = 0;
	for (i = 0; i < nloops; i++) {
		for (j = 0; j < innloops; j++) {
			outof++;
			if (dosema) {
				if ((rv = uscpsema(s)) == 1) {
					/* got lock */
					gotit++;
					usvsema(s);
					if ((j % innloops/10) == 0)
						sginap(1);
				} else if (rv < 0) {
					errprintf(1, "uscpsema error");
					/* NOTREACHED */
				}
			} else {
				if ((rv = uscsetlock(l, 2)) == 1) {
					/* got lock */
					gotit++;
					usunsetlock(l);
					if ((j % innloops/10) == 0)
						sginap(1);
				} else if (rv < 0) {
					errprintf(1, "uscsetlock error");
					/* NOTREACHED */
				}
			}
		}
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

	printf("csettest:parent done got it %d times out of %d\n",
		gotit, outof);
	printf("csettest:while slave got it %d times\n", sgotit);
	if (dosema) {
		if (verbose)
			usdumpsema(s, stderr, "csettest sema");
		usctlsema(s, CS_METERFETCH, &sm);
		if (gotit + sgotit != sm.sm_vsemas)
			errprintf(0, "gotit %d != vsemas %d\n",
				gotit + sgotit, sm.sm_vsemas);
			/* NOTREACHED */
	} else {
		if (verbose)
			usdumplock(l, stderr, "csettest lock");
		if (debug) {
			usctllock(l, CL_METERFETCH, &lm);
			if (gotit + sgotit != lm.lm_hits)
				errprintf(0, "gotit %d != hits %d\n",
					gotit + sgotit, lm.lm_hits);
		}
	}
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
}

/* ARGSUSED */
void
slave(void *arg)
{
	int i, j;
	volatile float a, b, c;

	slaveexinit();

	if (usinit("/usr/tmp/csettest.arena") == NULL) {
		errprintf(1, "slave:usinit");
		/* NOTREACHED */
	}
	for (b = 3.14159, c = 2.71828;;) {
		if (stopslave)
			exit(0);
		for (j = 0; j < innloops; j++) {
			if (dosema) {
				uspsema(s);
				sgotit++;
				if ((j % innloops/4) == 0)
					sginap(0);
				usvsema(s);
			} else {
				ussetlock(l);
				sgotit++;
				if ((j % innloops/4) == 0)
					sginap(0);
				usunsetlock(l);
			}
			for (i = 0; i < 1000; i++)
				a = b * c * (float)i;
		}
	}
}
