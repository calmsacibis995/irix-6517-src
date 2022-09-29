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

#ident "$Revision: 1.3 $"

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
#include <sys/resource.h>
#include "stress.h"

/*
 * usmalloc - test usmalloc
 */

char *Cmd;
char *afile;
usptr_t *handle;
usema_t **s;
void simple1(void), slave(void *);
extern void segcatch(int, int, struct sigcontext *);
int debug = 0;
int verbose = 0;
int mpid;
int autoreserve;
int use_devzero;
extern char *_sys_siglist[];

int
main(int argc, char **argv)
{
	int		c;
	int		err = 0;
	int		i;
	int		arenasize = 64;
	void		*attachaddr = (void *)~0L;
	int		 nusers = 8;
	pid_t		pid;
	auto int	status;
	struct rlimit corelimit;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "zrm:a:vdu:")) != EOF) {
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
		case 'm':
			arenasize = atoi(optarg);
			break;
		case 'a':
			attachaddr = (void *)strtol(optarg, NULL, 0);
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
		fprintf(stderr, "Usage: %s [-vdzr][-u users][-m arenasize (in Kb)][-a attachaddr]\n", Cmd);
		fprintf(stderr, "\t-v	verbose\n");
		fprintf(stderr, "\t-z	use /dev/zero as arena file\n");
		fprintf(stderr, "\t-d	use debug locks\n");
		fprintf(stderr, "\t-m #	arena size in Kb (default 64)\n");
		fprintf(stderr, "\t-u #	number of threads (default 8)\n");
		fprintf(stderr, "\t-a #	attach addr\n");
		fprintf(stderr, "\t-r	use CONF_AUTORESV\n");
		exit(1);
	}
	if (debug)
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);
	if (attachaddr != (void *)~0L)
		usconfig(CONF_ATTACHADDR, attachaddr);
	if (autoreserve)
		usconfig(CONF_AUTORESV);

	if (usconfig(CONF_INITUSERS, nusers+1) < 0) {
		errprintf(1, "INITUSERS");
		/* NOTREACHED */
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITSIZE, arenasize * 1024);

	if (use_devzero)
		afile = "/dev/zero";
	else
		afile = tempnam(NULL, "usmal");

	printf("%s:%d: # users %d arena size %d kB using %s as file\n",
		Cmd, get_pid(), nusers, arenasize, afile);

	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */

	usconfig(CONF_ATTACHADDR, -1);

	/*
	 * this test can get really large - don't really want to create
	 * huge core dumps when system is out of availsmem
	 */
	corelimit.rlim_max = 1 * 1024 * 1024;
	corelimit.rlim_cur = 1 * 1024 * 1024;
	if (setrlimit(RLIMIT_CORE, &corelimit) != 0) {
		errprintf(1, "setrlimit");
		/* NOTREACHED */
	}


	prctl(PR_SETSTACKSIZE, 64*1024);
	/* start up all users */
	for (i = 0; i < nusers; i++) {
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

	err = 0;
	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if (pid >= 0) {
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					err++;
					fprintf(stderr, "%s:%d exitted with %d\n",
						Cmd, pid, WEXITSTATUS(status));
				}
			} else if (WIFSIGNALED(status)) {
				err++;
				fprintf(stderr, "%s:%d died of signal %d:%s\n",
					Cmd, pid, WTERMSIG(status),
					_sys_siglist[WTERMSIG(status)]);
			}
		}
	}
	if (err)
		printf("%s:%d:FAILED\n", Cmd, getpid());
	else
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
	register int sz;
	auto unsigned int seed = 1;
	int totsz = 0;
	char *h, *eh;

	if (usadd(handle) != 0) {
		errprintf(3, "%d slave usadd", get_pid());
		exit(-1);
		/* NOTREACHED */
	}

	for (;;) {
		sz = rand_r(&seed);
		if ((h = usmalloc(sz, handle)) == NULL)
			break;
		totsz += sz;
		for (eh = h + sz; h < eh; h += 64)
			*h = 0xbe;
	}
	if (verbose)
		printf("%s:%d: usmalloc'd total of %d bytes\n",
			Cmd, get_pid(), totsz);
	exit(0);
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
