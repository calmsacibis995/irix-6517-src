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

#ident	"$Revision: 1.13 $ $Author: jwag $"

#include "sys/types.h"
#include "sys/mman.h"
#include "stdio.h"
#include "stddef.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"
#include "signal.h"
#include "task.h"
#include "errno.h"
#include "wait.h"
#include "sys/stat.h"
#include "stress.h"

/*
 * simple sharing address space after sproc() and fork()
 */
pid_t ppid;	/* parents pid */
pid_t spid = 0;	/* shared procs pid */
pid_t pid;	/* fork childs pid */
char *Cmd;
char *sbuf;
volatile char *Sync;
int Verbose = 0;

main(argc, argv)
 int argc;
 char **argv;
{
	extern void sigcld();
	extern void slave();
	int i;
	int nloop;
	int pgsz;

	Cmd = errinit(argv[0]);
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [-v] nloop\n", argv[0]);
		exit(-1);
	}
	if (argc == 3) {
		Verbose++;
		nloop = atoi(argv[2]);
	} else
		nloop = atoi(argv[1]);

	usconfig(CONF_INITUSERS, 3);
	/* get parent pid */
	ppid = get_pid();
	sigset(SIGCLD, sigcld);
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */

	pgsz = getpagesize();
	sbuf = malloc(3*pgsz);

	/* now sproc */
	if ((spid = sproc(slave, PR_SALL)) < 0) {
		errprintf(1, "sproc failed");
		/* NOTREACHED */
	}

	/* point into a page that we know
	 * will not have extraneous activity.
	 */
	Sync = &sbuf[pgsz+3];
	*Sync = 1;

	/* now read and write page */
	for (i = 0; i < nloop; i++) {

		/* now fork */
		if ((pid = fork()) < 0) {
			errprintf(3, "fork failed");
			goto out;
		}

		if (pid) {	/* fork parent */

			if (blockproc(ppid) < 0) {
				errprintf(1, "block myself as parent failed!");
				/* NOTREACHED */
			}
			wrout("parent changing page Sync = %x *Sync = %d",
					Sync, *Sync);
			*Sync = 2;

			while (*Sync == 2)
				;
			sginap(10);
			/* unblock fork child */
			sighold(SIGCLD);
			if (unblockproc(pid) < 0) {
				errprintf(1, "unblockproc fork child failed!");
				/* NOTREACHED */
			}
			sigpause(SIGCLD);

		} else {	/* fork child */
			prctl(PR_TERMCHILD);
			if (getppid() == 1)
				exit(0);

			/* wait to let parent block */
			while (!prctl(PR_ISBLOCKED, ppid))
				;
			/* unblock slave */
			if (unblockproc(spid) < 0) {
				errprintf(1, "unblockproc slave failed!");
				/* NOTREACHED */
			}
			/* unblock fork parent */
			if (unblockproc(ppid) < 0) {
				errprintf(1, "unblockproc parent failed!");
				/* NOTREACHED */
			}
			if (blockproc(get_pid()) < 0) {
				errprintf(1, "block myself as child failed!");
				/* NOTREACHED */
			}
			if (Verbose)
				wrout("child exiting");
			fflush(NULL);
			sginap(60);
			exit(0);
		}
	}
out:
	setblockproccnt(spid, 0);
	sigignore(SIGCLD);
	return 0;
}

void
slave()
{
	register int passno = 0;
	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);
	for (;;) {
		/* wait for child */
		if (blockproc(get_pid()) < 0) {
			errprintf(1, "block myself as slave failed!");
			/* NOTREACHED */
		}

		if (Verbose)
			wrout("slave spinning on page change");

		/* spin until *Sync != 1; */

		/* wait for parent to change */
		while (*Sync == 1)
			;
		wrout("slave read changed page");
		*Sync = 1;
		passno++;
	}
}

void
sigcld()
{
	auto int status;

	int pid = wait(&status);
	if (pid == spid) {
		fprintf(stderr, "sfork:sproc process died: status:%x\n", status);
		exit(-2);
	}
}
