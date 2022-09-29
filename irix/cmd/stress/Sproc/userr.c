/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.8 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ulocks.h>
#include <getopt.h>
#include <malloc.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <wait.h>
#include <mon.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "stress.h"

/*
 * userr - test some error handling in usinit & some of the usconfig options
 * If compiled with -p does inheritance testing of profiling - in particular
 * making sure that if parent disables profiling, sproc kids are also
 * disabled.
 */

char *Cmd;
int verbose = 0;
char *afile;
extern void slave();

int
main(int argc, char **argv)
{
	int c, i;
	int osz, sz = 4096;
	struct stat sb;
	ulock_t l;
	usema_t *s;
	usptr_t *usp;
	histptr_t hp;
	semameter_t sm;
	semadebug_t sd;
	lockdebug_t ld;
	lockmeter_t lm;
	int nusers;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "v")) != -1)
		switch(c) {
		case 'v':	/* verbose */
			verbose = 1;
			break;
		}

	if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) < 0) {
		errprintf(1, "usconfig ARENATYPE failed");
		/* NOTREACHED */
	}
	if (usconfig(CONF_LOCKTYPE, US_DEBUG) < 0) {
		errprintf(1, "usconfig LOCKTYPE failed");
		/* NOTREACHED */
	}
	if (usconfig(CONF_INITSIZE, sz) < 0) {
		errprintf(1, "usconfig INITSIZE failed");
		/* NOTREACHED */
	}
	afile = tempnam(NULL, "userr");
	while ((usp = usinit(afile)) == NULL) {
		osz = sz;
		sz += 100;
		if (usconfig(CONF_INITSIZE, sz) != osz) {
			errprintf(1, "usconfig failed");
			/* NOTREACHED */
		}
	}
	printf("userr:usinit worked for size %d\n", sz);

	/* since SHAREDONLY - file should be unlinked already */
	if (stat(afile, &sb) >= 0) {
		errprintf(1, "stat of %s worked!", afile);
		/* NOTREACHED */
	}

	/* turn off profiling - children should have NOTHING... */
	moncontrol(0);
	/* now sproc so create a libc arena */
	if (sproc(slave, PR_SALL) < 0) {
		errprintf(3, "sproc failed");
		goto out;
	}
	wait(0L);

	/* now test out dump sema and dump lock */
	if ((l = usnewlock(usp)) == NULL) {
		errprintf(1, "usnewlock FAILED");
	}
	usdumplock(l, stderr, "userr:new lock");
	ussetlock(l);
	usdumplock(l, stderr, "userr:after lock");
	usunsetlock(l);
	usdumplock(l, stderr, "userr:after unlock");
	usctllock(l, CL_DEBUGFETCH, &ld);
	usctllock(l, CL_METERFETCH, &lm);
	printf("userr:lock debug:owner %d\n", ld.ld_owner_pid);
	printf("userr:lock meter: spins %d tries %d hits %d\n",
		lm.lm_spins, lm.lm_tries, lm.lm_hits);
	usctllock(l, CL_METERRESET);
	usctllock(l, CL_DEBUGRESET);
	usdumplock(l, stderr, "userr:after reset");

	/* now test out dump sema and dump lock */
	if ((s = usnewsema(usp, 1)) == NULL) {
		errprintf(1, "usnewsema FAILED");
		/* NOTREACHED */
	}
	usconfig(CONF_HISTON, usp);
	usctlsema(s, CS_HISTON);
	usctlsema(s, CS_METERON);
	usctlsema(s, CS_DEBUGON);
	usdumpsema(s, stderr, "userr:new sema");
	uspsema(s);
	usdumpsema(s, stderr, "userr:after psema");
	usvsema(s);
	usdumpsema(s, stderr, "userr:after vsema");
	usctlsema(s, CS_DEBUGFETCH, &sd);
	usctlsema(s, CS_METERFETCH, &sm);
	printf("userr:semaphore debug:owner %d owner pc 0x%x last pid %d last pc 0x%x\n",
		sd.sd_owner_pid, sd.sd_owner_pc, sd.sd_last_pid, sd.sd_last_pc);
	printf("userr:semaphore meter: phits %d psemas %d vsemas %d vnowait %d nwait %d maxnwait %d\n",
		sm.sm_phits, sm.sm_psemas, sm.sm_vsemas, sm.sm_vnowait,
		sm.sm_nwait, sm.sm_maxnwait);
	usctlsema(s, CS_METERRESET);
	usctlsema(s, CS_DEBUGRESET);
	usdumpsema(s, stderr, "userr:after reset");

	usconfig(CONF_HISTFETCH, usp, &hp);
	printf("userr:Hist current 0x%x entries %d errors %d\n",
		hp.hp_current, hp.hp_entries, hp.hp_errors);
	usconfig(CONF_HISTRESET, usp, &hp);
	usconfig(CONF_HISTFETCH, usp, &hp);
	printf("userr:Hist after reset current 0x%x entries %d errors %d\n",
		hp.hp_current, hp.hp_entries, hp.hp_errors);

	/* test overflow pollsema queue */
	nusers = (int)usconfig(CONF_GETUSERS, usp);
	if ((s = usnewpollsema(usp, 0)) == NULL) {
		errprintf(1, "usnewsema failed");
	}
	setoserror(0);
	for (i = 0; i < nusers+1; i++) {
		if (uspsema(s) < 0)
			break;
	}
	if (oserror() != EBADF) {
		errprintf(1, "uspsema failed with unknown error");
		/* NOTREACHED */
	}
	if (usopenpollsema(s, 0664) < 0) {
		errprintf(1, "usopenpollsema failed");
	}
	setoserror(0);
	for (i = 0; i < nusers+1; i++) {
		if (uspsema(s) < 0)
			break;
	}
	if (oserror() != ERANGE) {
		errprintf(1, "uspsema range failed with unknown error");
		/* NOTREACHED */
	}

out:
	return 0;
}

void
slave()
{
	struct tms dum;
	long st, et;
	char *a[10000];
	register int i;

	/* prime the pump */
	for (i = 0; i < 10000; i++) {
		if ((a[i] = malloc(16)) == NULL) {
			errprintf(0, "slave malloc failed");
		}
	}
	for (i = 0; i < 10000; i++)
		free(a[i]);

	/* now for real */
	st = times(&dum);
	for (i = 0; i < 10000; i++) {
		if ((a[i] = malloc(16)) == NULL) {
			errprintf(0, "slave malloc failed");
		}
	}
	for (i = 0; i < 10000; i++)
		free(a[i]);
	et = times(&dum);

	printf("userr:time for 10000 malloc & frees with semas %d mS\n",
		(et - st)*1000/HZ);

	if (usconfig(CONF_STHREADMALLOCOFF) < 0) {
		errprintf(1, "usconfig mallocoff failed");
	}

	st = times(&dum);
	for (i = 0; i < 10000; i++) {
		if ((a[i] = malloc(16)) == NULL) {
			errprintf(0, "slave malloc failed");
		}
	}
	for (i = 0; i < 10000; i++)
		free(a[i]);
	et = times(&dum);

	printf("userr:time for 10000 malloc & frees w/o semas %d mS\n",
		(et - st)*1000/HZ);

}
