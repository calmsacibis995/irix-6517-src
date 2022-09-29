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

#include "stdlib.h"
#include "unistd.h"
#include "sys/types.h"
#include "stdio.h"
#include "fcntl.h"
#include "signal.h"
#include "wait.h"
#include "ulocks.h"
#include "errno.h"
#include "string.h"
#include "stress.h"

/*
 * simple sharing uid/gid test
 * test should be made setuid EUID and setgid EGID
 * the uid/gid TUID/TGID must exist
 */
#define EUID	9	/* lp */
#define EGID	9	/* lp */
#define TUID	3	/* uucp */
#define TGID	5	/* uucp */
pid_t ppid;	/* parents pid */
pid_t spid;	/* share slave pid */
pid_t nspid;	/* no share slave pid */
char *Cmd;
int Verbose = 0;
volatile int shslaveok = 0;

int
main(int argc, char **argv)
{
	extern void sigcld();
	extern void shslave(), noshslave();
	int i;
	int nloop;

	Cmd = errinit(argv[0]);
	if (argc < 2) {
		fprintf(stderr, "Usage:%s [-v] nloop\n", argv[0]);
		exit(-1);
	}
	if (argc == 3) {
		Verbose++;
		nloop = atoi(argv[2]);
	} else
		nloop = atoi(argv[1]);
	ppid = get_pid();

	if (geteuid() != EUID || getegid() != EGID) {
		fprintf(stderr, "%s:effective user MUST be %d group %d\n",
			Cmd, EUID, EGID);
		exit(-1);
	}

	sigset(SIGCLD, sigcld);
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */

	/* now sproc share slave */
	if ((spid = sproc(shslave, PR_SADDR|PR_SFDS|PR_SID)) < 0) {
		errprintf(3, "sproc of share slave failed");
		sigignore(SIGCLD);
		exit(-1);
	}
	/* now sproc no-share slave */
	if ((nspid = sproc(noshslave, PR_SFDS|PR_SADDR)) < 0) {
		errprintf(3, "sproc of no-share slave failed");
		sigignore(SIGCLD);
		exit(-1);
	}

	/* wait for shslave to check things out */
	while (shslaveok == 0)
		sginap(0);
	/* now play with uid/gid */
	for (i = 0; i < nloop; i++) {
		if (setuid(TUID) != 0 || setgid(TGID) != 0) {
			errprintf(1, "setu/gid failed");
			/* NOTREACHED */
		}
		/* wait till ids change back */
		while (geteuid() != EUID || getegid() != EGID) {
			if (Verbose)
				wrout("Waiting for shslave to chg ids back\n");
			sginap(2);
		}
	}
	sigignore(SIGCLD);
	return 0;
}

void
shslave()
{
	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);

	if (geteuid() != EUID || getegid() != EGID) {
		wrout("shslave:effective user MUST be %d group %d\n",
			EUID, EGID);
		exit(-1);
	}
	shslaveok++;
	for (;;) {
		/* wait till change, then change back */
		while (geteuid() == EUID && getegid() == EGID) {
			if (Verbose)
				wrout("shslave waiting for id chg\n");
			sginap(2);
		}
		sginap(0);
		/* chg em' back */
		if (setuid(EUID) != 0 || setgid(EGID) != 0) {
			errprintf(1, "shslave:setu/gid failed");
			/* NOTREACHED */
		}
		sginap(2);
	}
}

void
noshslave()
{
	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);
	for (;;) {
		if (geteuid() != EUID || getegid() != EGID) {
			errprintf(0, "noshslave:effective user chged!");
			/* NOTREACHED */
		}
	}
}

void
sigcld()
{
	auto int status;

	pid_t pid = wait(&status);

	wrout("pid:%d sproc process died: status:%x\n", pid, status);
	exit(-1);
}
