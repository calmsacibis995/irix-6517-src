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
#ident "$Revision: 1.1 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <sys/usync.h>
#include "limits.h"
#include "ulocks.h"
#include "getopt.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "sys/times.h"
#include "stress.h"

#define MAXPROC	1000
#define NLOCK 8
ulock_t l;
usema_t *s;
int cnts[MAXPROC];
pthread_t pthreads[MAXPROC];
int gcnt = 0;
int lerror = 0;
int innloops = 1000;
int verbose = 0;
int killrotor;
int npthreads = 1;
int nloops = 100;
int usesema = 0;
int spincnt = -1;
char *Cmd;
char *afile;

usema_t *sleep_sema;

void catch(int sig, int code, struct sigcontext *sc);
void chkcnt(void);
void slave(void *arg);

int
main(int argc, char **argv)
{
	int rv, c, i, j;
	usptr_t *hdr;

	Cmd = errinit(argv[0]);
	while((c = getopt(argc, argv, "w:dp:vn:ei:s")) != EOF)
	switch (c) {
	case 'd':
		usconfig(CONF_LOCKTYPE, US_DEBUG);
		break;
	case 'w':	/* # spin cnt */
		spincnt = atoi(optarg);
		break;
	case 'i':	/* # inner loops */
		innloops = atoi(optarg);
		break;
	case 'p':	/* # pthreads */
		npthreads = atoi(optarg);
		break;
	case 'n':	/* # loops */
		nloops = atoi(optarg);
		break;
	case 'v':	/* verbose */
		verbose++;
		break;
	case 'e':	/* loop on error */
		lerror++;
		break;
	case 's':	/* use semaphores */
		usesema++;
		break;
	default:
		fprintf(stderr, "Usage: %s [-i#][-e][-v][-p#][-n#][-s][-d][-w#]\n",
				Cmd);
		fprintf(stderr, "\t-i# - # of inner loops\n");
		fprintf(stderr, "\t-e - loop on error\n");
		fprintf(stderr, "\t-p# - # of processes\n");
		fprintf(stderr, "\t-n# - # of outer loops\n");
		fprintf(stderr, "\t-v - verbose\n");
		fprintf(stderr, "\t-s - use semaphores rather than locks\n");
		fprintf(stderr, "\t-d - use debug locks\n");
		fprintf(stderr, "\t-w# - use wsetlock with #\n");
		exit(-1);
	}

	printf("%s: # pthreads %d # loops %d using %s\n",
		Cmd, npthreads, nloops, usesema ? "semaphores" : "locks");
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	if (usconfig(CONF_INITUSERS, npthreads+1) < 0) {
		errprintf(1, "INITUSERS failed");
		/* NOTREACHED */
	}
	afile = tempnam(NULL, "lockstress_pt");
	if ((hdr = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	s = usnewsema(hdr, 1);
	sleep_sema = usnewsema(hdr, 0);
	l = usnewlock(hdr);

	pthreads[0] = pthread_self();

	prctl(PR_SETEXITSIG, SIGTERM);
	prctl(PR_SETSTACKSIZE, 64*1024);
	sigset(SIGUSR1, catch);
	for (i = 1; i <= npthreads; i++) {
		if (pthread_create(&pthreads[i],
				   NULL,
				   (void *) slave,
				   (void *) i)) {
			perror("lockstress_pt: pthread_create failed\n");
			/* NOTREACHED */
		}
	}

	/* start up all slaves */
	for (i = 1; i <= npthreads; i++)
		usvsema(sleep_sema);

	for (i = 0; i < nloops; i++) {
		for (j = 0; j < innloops; j++) {
			if (usesema)
				rv = uspsema(s);
			else if (spincnt >= 0)
				rv = uswsetlock(l, spincnt);
			else
				rv = ussetlock(l);
			if (rv != 1)
				errprintf(1, "lock/sema acquire failed");
			if (j == innloops / 2) {
				if (++killrotor > npthreads)
					killrotor = 0;
				if (verbose) {
					sighold(SIGUSR1);
					printf("%s:%d waking %d\n",
					       Cmd, pthreads[0],
					       pthreads[killrotor]);
					sigrelse(SIGUSR1);
				}
				if (pthread_kill(pthreads[killrotor], SIGUSR1) != 0)
					errprintf(1, "parent signal slave failed");
					/* NOTREACHED */
			}
			gcnt++;
			chkcnt();
			cnts[0]++;
			if (usesema)
				rv = usvsema(s);
			else
				rv = usunsetlock(l);
			if (rv != 0)
				errprintf(1, "lock/sema free failed");
		}
		if (verbose) {
			sighold(SIGUSR1);
			printf("%s:parent pass\n", Cmd);
			sigrelse(SIGUSR1);
		}
	}
	return 0;
}

/* ARGSUSED */
void
catch(int sig, int code, struct sigcontext *sc)
{
	if (verbose)
		printf("%s: pid %d pthread %x caught signal at pc 0x%llx\n",
			Cmd, getpid(), pthread_self(), sc->sc_pc);
}

void
slave(void *arg)
{
	int rv, j;
	int mypid;
	__psint_t myid = (__psint_t) arg;

	sighold(SIGUSR1);
	printf("%s:slave pid %d pthread %x innloops %d\n",
		Cmd, mypid = getpid(), pthread_self(), innloops);
	sigrelse(SIGUSR1);

	/* Block and wait for the parent to V the semaphore */
	uspsema(sleep_sema);

	for (;;) {
		if (verbose) {
			sighold(SIGUSR1);
			printf("%s:%d slave pass\n", Cmd, mypid);
			sigrelse(SIGUSR1);
		}
		for (j = 0; j < innloops; j++) {
			if (usesema)
				rv = uspsema(s);
			else if (spincnt >= 0)
				rv = uswsetlock(l, spincnt);
			else
				rv = ussetlock(l);
			if (rv != 1)
				errprintf(1, "lock/sema acquire failed");
			if (j == innloops / 2) {
				if (++killrotor > npthreads)
					killrotor = 0;
				if (verbose) {
					sighold(SIGUSR1);
					printf("%s:%d waking %d\n",
						Cmd, pthreads[myid],
						pthreads[killrotor]);
					sigrelse(SIGUSR1);
				}
				if (pthread_kill(pthreads[killrotor], SIGUSR1) != 0) {
					/* if parent has exitted, we'll be
					 * getting a SIGTERM soon - don't
					 * worry
					 */
					if (oserror() != ESRCH || killrotor != 0)
						
						errprintf(1, "slave signal failed");
						/* NOTREACHED */
				}
			}
			gcnt++;
			chkcnt();
			cnts[myid]++;
			if (usesema)
				rv = usvsema(s);
			else
				rv = usunsetlock(l);
			if (rv != 0)
				errprintf(1, "lock/sema free failed");
		}
	}
}

void
chkcnt(void)
{
	register int i, pcnt = 0;

	for (i = 0; i <= npthreads; i++)
		pcnt += cnts[i];

	if (gcnt != pcnt + 1) {
		sighold(SIGUSR1);
		errprintf(0, "gcnt %d pcnt %d diff %d\n",
			gcnt, pcnt, gcnt - pcnt);
		sigrelse(SIGUSR1);
		if (lerror)
			pause();
	}
}
