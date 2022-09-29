/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.14 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h> 
#include <signal.h> 
#include <ulocks.h>
#include <errno.h>
#include <getopt.h>
#include <wait.h>
#include "stress.h"

/*
 * usema - basic semaphore test
 */

char *Cmd;
char *afile;
usptr_t *handle;
int nsemas = 16;
int nacquires = 16;
usema_t **s;
void simple1(void), slave(void *);
extern void segcatch(int, int, struct sigcontext *);
int debug = 0;
int verbose = 0;
int ovflw = 0;
pid_t mpid;
int autoreserve;
int use_devzero;

int
main(int argc, char **argv)
{
	int		c;
	int		err = 0;
	int		i;
	int		arenasize = 64;
	void		*attachaddr = (void *)~0L;
	int		 nusers = 8;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "zrm:oa:vdn:u:s:")) != EOF) {
		switch (c) {
		case 'z':
			use_devzero = 1;
			break;
		case 'r':
			autoreserve = 1;
			break;
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
		case 'a':
			attachaddr = (void *)strtol(optarg, NULL, 0);
			break;
		case 'o':
			/* test queue overflow */
			ovflw++;
			break;
		case '?':
			err++;
			break;
		}
	}
	mpid = get_pid();
	sigset(SIGSEGV, segcatch);
	sigset(SIGBUS, segcatch);
	parentexinit(0);

	if (err) {
		fprintf(stderr, "Usage: %s [-v][-d][-u users][-n acquires][-s semas][-m arenasize (in Kb)][-a attachaddr]\n", Cmd);
		exit(1);
	}
	if (debug)
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);
	if (attachaddr != (void *)~0L)
		usconfig(CONF_ATTACHADDR, attachaddr);
	if (autoreserve)
		usconfig(CONF_AUTORESV);

	printf("%s:%d: # users %d # semas %d # acquires %d\n",
		Cmd, get_pid(), nusers, nsemas, nacquires);
	if (ovflw) {
		/* test queue overflow */
		if (usconfig(CONF_INITUSERS, nusers > 3 ? nusers - 3 : 1) < 0) {
			errprintf(1, "INITUSERS");
			/* NOTREACHED */
		}
	} else {
		if (usconfig(CONF_INITUSERS, nusers) < 0) {
			errprintf(1, "INITUSERS");
			/* NOTREACHED */
		}
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITSIZE, arenasize * 1024);

	if (use_devzero)
		afile = "/dev/zero";
	else
		afile = tempnam(NULL, "usema");
	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	usconfig(CONF_ATTACHADDR, -1);

	/* if testing ovflw - set nusers correct so I/O arena will work */
	if (usconfig(CONF_INITUSERS, nusers) < 0) {
		errprintf(1, "INITUSERS");
		/* NOTREACHED */
	}

	s = malloc(nsemas * sizeof(usema_t *));

	/* alloc semaphores */
	if (debug)
		usconfig(CONF_HISTON, handle);
	for (i = 0; i < nsemas; i++) {
		if ((s[i] = usnewsema(handle, 1)) == NULL) {
			errprintf(1, "newsema failed i:%d\n", i);
			/* NOTREACHED */
		}
		if (debug) {
			usctlsema(s[i], CS_DEBUGON);
		}
	}

	prctl(PR_SETSTACKSIZE, 64*1024);
	/* start up all users */
	for (i = 0; i < nusers-1; i++) {
		if (sproc(slave, PR_SALL, 0) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_EXIT, "sproc failed i:%d", i);
				/* NOTREACHED */
			} else {
				fprintf(stderr, "%s:can't sproc:%s\n",
					Cmd, strerror(errno));
				/* continue with what we got */
				break;
			}
		}
	}
	/* so we don't hang during overflow */
	if (!ovflw)
		simple1();

	while (wait(0) >= 0 || errno == EINTR)
		;
	for (i = 0; i < nsemas; i++)
		usfreesema(s[i], handle);
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
}

/* ARGSUSED */
void
slave(void *arg)
{
	slaveexinit();
	simple1();
}

void
simple1(void)
{
	register int i, j;

	if (!ovflw) {
		if (usadd(handle) != 0) {
			errprintf(3, "%d slave usadd", get_pid());
			exit(-1);
			/* NOTREACHED */
		}
	}

	for (j = 0; j < nacquires; j++) {
		for (i = 0; i < nsemas; i++) {
			if (uspsema(s[i]) != 1) {
				if (ovflw) {
					errprintf(3, "%d uspsema failed", get_pid());
					return;
				} else {
					errprintf(ERR_ERRNO_EXIT, "%d uspsema failed", get_pid());
				}
			}
			sginap(1);
			if (ovflw && j >= (nacquires-1))
				return;
			if (usvsema(s[i]) != 0) {
				if (ovflw) {
					errprintf(3, "%d uvpsema failed", get_pid());
					return;
				} else {
					errprintf(ERR_ERRNO_EXIT, "%d uvpsema failed", get_pid());
				}
			}
		}
		if (verbose)
			printf("%s:%d pass:%d\n", Cmd, get_pid(), j+1);
	}
	if (verbose)
		printf("%s:%d done\n", Cmd, get_pid());
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
