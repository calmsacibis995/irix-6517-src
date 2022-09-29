/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.4 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <getopt.h>
#include <signal.h> 
#include <ulocks.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "stress.h"

/*
 * usemaintr - basic semaphore test with getting signals
 * Also test semaphore history
 */

char *Cmd;

char *afile;
usptr_t *handle;
int nusers = 8;
int nsemas = 16;
int nacquires = 16;
usema_t **s;
void simple1();
void segcatch(), catcher();
extern void dumphist(usema_t *sp, FILE *fd);
int debug = 0;
int verbose = 0;
int mpid;
int *nsigs;
pid_t *pids;

main(argc, argv)
int	argc;
char	*argv[];
{
	int		c;
	int		err = 0;
	int		i;
	int		arenasize = 64;
	FILE		*fd;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "vdn:u:s:")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			break;
		case 'u':
			nusers = atoi(optarg);
			break;
		case 's':
			nsemas = atoi(optarg);
			break;
		case 'n':
			nacquires = atoi(optarg);
			break;
		case 'm':
			arenasize = atoi(optarg);
			break;
		case '?':
			err++;
			break;
		}
	}
	mpid = get_pid();
	if (err) {
		fprintf(stderr, "Usage: %s [-v][-d][-u users][-n acquires][-s semas][-m arenasize (in Kb)\n", Cmd);
		exit(1);
	}
	if (debug)
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);

	printf("%s: # users %d # semas %d # acquires %d\n",
		Cmd, nusers, nsemas, nacquires);
	if (usconfig(CONF_INITUSERS, nusers+1) < 0) {
		errprintf(1, "INITUSERS");
		/* NOTREACHED */
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITSIZE, arenasize * 1024);

	afile = tempnam(NULL, "usemaintr");
	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */

	s = malloc(nsemas * sizeof(usema_t *));
	pids = malloc(nusers * sizeof(pid_t));
	nsigs = malloc(nusers * sizeof(int));

	/* alloc semaphores */
	usconfig(CONF_HISTON, handle);
	usconfig(CONF_HISTSIZE, handle, 1000);
	for (i = 0; i < nsemas; i++) {
		if ((s[i] = usnewsema(handle, 1)) == NULL) {
			errprintf(1, "newsema failed i:%d", i);
			/* NOTREACHED */
		}
		if (debug) {
			usctlsema(s[i], CS_DEBUGON);
		}
	}

	prctl(PR_SETSTACKSIZE, 64*1024);
	sigset(SIGTERM, catcher);
	sigset(SIGSEGV, segcatch);
	sigset(SIGBUS, segcatch);
	sigset(SIGTRAP, segcatch);
	/* start up all users */
	for (i = 0; i < nusers; i++) {
		if ((pids[i] = sproc(simple1, PR_SALL)) < 0) {
			errprintf(3, "sproc failed i:%d", i);
			exit(-1);
		}
	}

	/* 
	 * master sits and blasts signals at processes
	 */
	while (waitpid(-1, NULL, WNOHANG) >= 0 || oserror() != ECHILD) {
		for (i = 0; i < nusers; i++) {
			if (pids[i] <= 0)
				errprintf(0, "pids[%d] = %d  <= 0!", i, pids[i]);
				/* NOTREACHED */
			if (kill(pids[i], SIGTERM) != 0) {
				if (oserror() != ESRCH)
					errprintf(1, "kill of %d failed",
						pids[i]);
					/* NOTREACHED */
			}
		}
		sginap(2);
	}
	if ((fd = fopen("/usr/tmp/usemahist", "w")) == NULL) {
		errprintf(1, "fopen of semahist file failed");
		/* NOTREACHED */
	}
	fchmod(fileno(fd), 0666);
	dumphist(NULL, fd);
	return 0;
}

void
simple1()
{
	register int i, j;

	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "%d slave usinit", get_pid());
		/* NOTREACHED */

	for (j = 0; j < nacquires; j++) {
		for (i = 0; i < nsemas; i++) {
			if (uspsema(s[i]) <= 0) {
				errprintf(1, "%d:uspsema failed sp 0x%x i %d",
					get_pid(), s[i], i);
				/* NOTREACHED */
			}
			sginap(1);
			if (usvsema(s[i]) != 0) {
				errprintf(1, "%d:usvsema failed sp 0x%x i %d",
					get_pid(), s[i], i);
			}
		}
		if (verbose)
			printf("%s:%d pass:%d\n", Cmd, get_pid(), j+1);
	}
	for (i = 0; i < nusers; i++)
		if (pids[i] == get_pid())
			printf("%s:%d completed. received %d interrupts\n",
				Cmd, get_pid(), nsigs[i]);
}

void
catcher()
{
	register int i;
	for (i = 0; i < nusers; i++)
		if (pids[i] == get_pid())
			nsigs[i]++;
}

void
segcatch(sig)
{
	fprintf(stderr, "%s:pid %d caught %d\n", Cmd, get_pid(), sig);
	if (pids[0] == get_pid())
		abort();
	exit(-1);
}

char *ops[] = {
	"UNK",
	"PHIT  ",
	"PSLEEP",
	"PWOKE ",
	"VHIT  ",
	"VWAKE ",
	"PINTR ",
	"PMISS ",
};

void
dumphist(usema_t *sp, FILE *fd)
{
	hist_t *h, *oh;
	histptr_t hp;

	if (!fd)
		fd = stderr;
	if (usconfig(CONF_HISTFETCH, handle, &hp) != 0)
		return;
	fprintf(fd, "History current 0x%x entries %d errors %d\n",
		hp.hp_current, hp.hp_entries, hp.hp_errors);
	fprintf(fd, "Backward in time\n");
	oh = NULL;
	for (h = hp.hp_current; h; h = h->h_last) {
		if (sp && sp != h->h_sem)
			continue;
		fprintf(fd, "%s sema @ 0x%x pid %d wpid %d cpc 0x%x scnt %d\n",
			ops[h->h_op], h->h_sem, h->h_pid, h->h_wpid,
			h->h_cpc, h->h_scnt);
		oh = h;
	}
	fprintf(fd, "Forward in time\n");
	for (h = oh; h; h = h->h_next) {
		if (sp && sp != h->h_sem)
			continue;
		fprintf(fd, "%s sema @ 0x%x pid %d wpid %d cpc 0x%x scnt %d\n",
			ops[h->h_op], h->h_sem, h->h_pid, h->h_wpid,
			h->h_cpc, h->h_scnt);
	}
}
