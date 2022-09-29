/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.9 $ $Author: jwag $"

#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "unistd.h"
#include "wait.h"
#include "string.h"
#include "getopt.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "task.h"
#include "stress.h"

/*
 * test out execing oneself with shared processes
 */
char *Cmd;
char *Argv0;
int ownsgrp;		/* should process be in its own share group then exec */
int verbose;
int nloops;

extern void slave(void *), slave2(void *);
extern void execit(int loopno);

int
main(int argc, char **argv)
{
	int c;
	pid_t pid;
	register int j, i, nprocs, amexeced;
	int curloop;
	auto int status;

	Cmd = errinit(argv[0]);
	Argv0 = strdup(argv[0]);
	nloops = 10;
	nprocs = 10;
	amexeced = 0;
	while ((c = getopt(argc, argv, "E:ovl:n:")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'n':
			nprocs = atoi(optarg);
			break;
		case 'o':
			ownsgrp = 1;
			break;
		case 'E':
			curloop = atoi(optarg);
			amexeced = 1;
			break;
		default:
			fprintf(stderr, "Usage:%s [-l loops][-n procs][-ov]\n",
				Cmd);
			exit(1);
		}
	}

	/* 
	 * we can't use the libstress parentexinit stuff since we're
	 * execing and when one execs one sends out the EXITSIG.
	 * But when one execs the caught signals get cleared, so
	 * its possible to get a EXITSIG when we're not catching it, and die
	 */
	if (!amexeced) {
		/* the origional */
		printf("sexec:sproc %d processes and execing itself %d times",
			nprocs, nloops);
		if (ownsgrp)
			printf(" forcing own share group");
		printf("\n");
		usconfig(CONF_INITUSERS, nprocs+1);
	    /*
	     * go around loop twice to catch problems when
	     * the arena isn't cleaned up twice. Since we are waiting for
	     * all processes to exit we should be able to create
	     * that many again..
	     */
	    for (j = 0; j < 2; j++) {
		for (i = 0; i < nprocs; i++) {
			if ((pid = sproc(slave, PR_SALL, (void *)(long)ownsgrp)) < 0) {
				if (errno != EAGAIN) {
					errprintf(ERR_ERRNO_EXIT,
							"sproc failed i:%d", i);
					/* NOTREACHED */
				} else {
					fprintf(stderr, "%s:can't sproc:%s\n",
						Cmd, strerror(errno));
					/* continue with what we've got */
					break;
				}
			}
		}
		for (;;) {
			pid = wait(&status);
			if (pid < 0 && errno == ECHILD)
				break;
			else if ((pid >= 0) && WIFSIGNALED(status)) {
				/*
				 * if anyone dies abnormally - get out
				 * But don't abort - the process that got
				 * the signal
				 * hopefully aborted - that core dump is more
				 * interesting
				 */
				errprintf(ERR_RET, "proc %d died of signal %d\n",
					pid, WTERMSIG(status));
				if (!WCOREDUMP(status))
					abort();
				exit(1);
			}
		}
	    }
	} else {
		/* if parent died for some reason - lets leave */
		if (getppid() == 1)
			exit(1);
		/* an execed program */
		if (curloop > nloops)
			exit(0);

		if (verbose && ((curloop % 4) == 0))
			printf("sexec:pid:%d loop:%d\n", get_pid(), curloop);
		/*
		 * to test path where the last share group member is
		 * execing rather than exitting, the ownsgrp flag can be set.
		 */
		if (ownsgrp) {
			if (sproc(slave2, PR_SALL, 0) < 0) {
				if (errno != EAGAIN) {
					errprintf(ERR_ERRNO_EXIT, "sub-sproc failed");
					/* NOTREACHED */
				} else {
					fprintf(stderr, "%s:can't sproc:%s\n",
						Cmd, strerror(errno));
					exit(1);
				}
			}
			/*
			 * wait for child - this will make us a share group
			 * and the only member
			 */
			while (wait(0) >= 0 || errno == EINTR)
				;
		}

		execit(curloop+1);
		/* NOTREACHED */
	}
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
}

/* ARGSUSED */
void
slave(void *arg)
{
	execit(1);
	/* NOTREACHED */
}

/* ARGSUSED */
void
slave2(void *arg)
{
	exit(0);
}

void
execit(int loopno)
{
	char anloops[10];
	char cloops[10];
	char *gargv[10];
	int i = 0;


	sprintf(anloops, "%d", nloops);
	sprintf(cloops, "%d", loopno);
	gargv[i++] = Argv0;
	gargv[i++] = "-E";
	gargv[i++] = cloops;
	gargv[i++] = "-l";
	gargv[i++] = anloops;
	if (ownsgrp)
		gargv[i++] = "-o";
	if (verbose)
		gargv[i++] = "-v";
	gargv[i] = NULL;
	execvp(Argv0, gargv);
	errprintf(ERR_ERRNO_EXIT, "1st execlp failed");
	/* NOTREACHED */
}
