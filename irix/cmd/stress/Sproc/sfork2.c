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

#ident	"$Revision: 1.3 $ $Author: jwag $"

#include "sys/types.h"
#include "sys/mman.h"
#include "sys/wait.h"
#include "stdio.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "fcntl.h"
#include "signal.h"
#include "task.h"
#include "errno.h"
#include "sys/stat.h"
#include "stress.h"
#include "getopt.h"

/*
 * simple sharing address space after sproc() and fork()
 */
pid_t ppid;	/* parents pid */
pid_t spid = 0;	/* shared procs pid */
pid_t pid;	/* fork childs pid */
int nwaits;
char *Cmd;
int Verbose = 0;

int
main(int argc, char **argv)
{
	extern void sigcld();
	extern void slave();
	register int c, i, j;
	int nper, nloop;
	char scmd[512];

	Cmd = errinit(argv[0]);

	nper = 2;
	nloop = 10;
	while ((c = getopt(argc, argv, "vl:p:")) != EOF) {
		switch (c) {
		case 'v':
			Verbose++;
			break;
		case 'l':
			nloop = atoi(optarg);
			break;
		case 'p':
			nper = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage: %s [-v] [-l nloop]\n", argv[0]);
			exit(-1);
		}
	}

	usconfig(CONF_INITUSERS, 2*(nper+2));
	/* get parent pid */
	ppid = get_pid();
	sigset(SIGCLD, sigcld);
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */

	/* now sproc */
	if ((spid = sproc(slave, PR_SALL)) < 0) {
		if (errno != EAGAIN) {
			errprintf(3, "sproc failed i:%d", i);
			abort();
		} else {
			/* running out of procs really isn't
			 * an error
			 */
			printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
			exit(1);
		}
		/* NOTREACHED */
	}
	strcpy(scmd, "sh -c 'grep root /etc/passwd' | awk 'BEGIN { FS = \":\" } { print $5 }'");
	if (!Verbose)
		strcat(scmd, " >/dev/null");

	for (i = 0; i < nloop; i++) {
		nwaits = 0;

		for (j = 0; j < nper; j++) {
			/* now fork */
			if ((pid = fork()) < 0) {
				errprintf(3, "fork failed");
				goto out;
			}

			if (pid == 0) {
				prctl(PR_TERMCHILD);
				if (getppid() == 1)
					exit(0);

				system(scmd);
				if (Verbose)
					wrout("child exiting");
				fflush(NULL);
				sginap(60);
				exit(0);
			}
		}
		sighold(SIGCLD);
		while (nwaits != nper) {
			sigpause(SIGCLD);
			sighold(SIGCLD);
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
	int fd;
	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);
	for (;;) {
		if ((fd = open("/etc/passwd", O_RDONLY)) < 0) {
			perror("passwd");
		}
		sginap(20);
		close(fd);
	}
}

void
sigcld()
{
	auto int status;
	pid_t pid;

	pid = waitpid(-1, &status, WNOHANG);
	if (pid < 0 && errno != ECHILD && errno != EINTR)  {
		sigignore(SIGCLD);
		errprintf(ERR_ERRNO_EXIT, "wait returned -1\n");
		/* NOTREACHED */
	}
	while (pid > 0) {
		nwaits++;
		if (WIFSIGNALED(status))
			fprintf(stderr, "%s:proc %d died of signal %d\n",
				Cmd, pid, WTERMSIG(status));
		if (pid == spid) {
			errprintf(ERR_EXIT, "sproc process died: status:%x\n", status);
			/* NOTREACHED */
		}
		pid = waitpid(-1, &status, WNOHANG);
	}
}
