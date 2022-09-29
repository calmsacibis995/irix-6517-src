/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1991 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.14 $ $Author: jwag $"

#include "sys/types.h"
#include "unistd.h"
#include "stdlib.h"
#include "limits.h"
#include "stdio.h"
#include "string.h"
#include "sys/wait.h"
#include "signal.h"
#include "sys/times.h"
#include "errno.h"
#include "sys/prctl.h"
#include "task.h"
#include "stress.h"

/*
 * test sproc PRDA setting/ signal on exit
 */
volatile int noexit;
char *Cmd;
int clds = 0;
int terms = 0;
volatile int waited;
rlim_t stksize;
pid_t master;
#define DEFSTKSIZE	(64*1024)	/* 64Kb */

int
main(int argc, char **argv)
{
	extern void slave(), sigc();
	struct tms tm;
	register int i, j;
	clock_t start, ti;
	int nprocs;		/* total procs created */
	int nprocspl;		/* # procs per loop */
	int doloop;		/* # procs this loop */

	prctl(PR_SETSTACKSIZE, DEFSTKSIZE);
	if (argc < 2) {
		fprintf(stderr, "Usage:%s nprocs [stack_size(inKb)]\n", argv[0]);
		exit(-1);
	}
	if (argc == 3) {
		stksize = atoi(argv[2]) * 1024;
	} else
		stksize = DEFSTKSIZE;
	if ((stksize = (rlim_t)prctl(PR_SETSTACKSIZE, stksize)) == (rlim_t)-1L) {
		errprintf(ERR_ERRNO_EXIT, "cannot set stacksize");
		/* NOTREACHED */
	}

	Cmd = errinit(argv[0]);
	nprocs = atoi(argv[1]);
	usconfig(CONF_INITUSERS, nprocs+1);
	sigset(SIGTERM, sigc);
	sigset(SIGCLD, sigc);
	prctl(PR_SETEXITSIG, SIGTERM);
	master = get_pid();

	printf("%s:Creating %d processes max stack size:%d Kb\n",
		Cmd, nprocs, stksize/1024);
	nprocspl = 20;

	start = times(&tm);

	for (j = nprocs; j > 0; j -= nprocspl) {
		noexit = 1;
		waited = 0;
		if (j > nprocspl)
			doloop = nprocspl;
		else
			doloop = j;

		for (i = 0; i < doloop; i++) {
			if (sproc(slave, PR_SADDR|PR_SFDS) < 0) {
				if (errno != EAGAIN) {
					errprintf(ERR_ERRNO_RET, "sproc failed");
					goto bad;
				}
				printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
				doloop = i;
				break;
			}
		}

		/* let all threads go */
		noexit = 0;
		/* wait for all threads to finish */
		for (;;) {
			sighold(SIGCLD);
			if (waited == doloop)
				break;
			sigpause(SIGCLD);
		}
	}

	ti = times(&tm) - start;
	printf("%s:start:%x time:%x\n", Cmd, start, times(&tm));
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:time for %d sprocs %d mS or %d uS per create/destroy\n",
		Cmd, nprocs, ti, (ti*1000)/nprocs);
	printf("%s:# SIGCLDs %d # SIGTERMS %d\n", Cmd, clds, terms);
	printf("%s:master exitting\n", Cmd);
	return 0;

bad:
	/* ignore SIGCLD so we don't recurse and hang */
	sigignore(SIGCLD);
	abort();
	/* NOTREACHED */
}

void
slave()
{
	char buf[100];

	/* force growth of stack */
	buf[0] = *(&buf[0] - (stksize - 10000));
	sprintf(buf, "%s: pid:%d getpid:%d\n",
		Cmd, get_pid(), getpid());
	write(1, buf, strlen(buf));
	while (noexit)
		sginap(0);
	_exit(0);
}

void
sigc(int sig)
{
	auto int status;
	pid_t pid;

	if (sig == SIGCLD) {
		pid = waitpid(-1, &status, WNOHANG);
		if (pid < 0 && errno != ECHILD && errno != EINTR)  {
			sigignore(SIGCLD);
			errprintf(ERR_ERRNO_EXIT, "wait returned -1\n");
			/* NOTREACHED */
		}
		while (pid > 0) {
			clds++;
			waited++;
			if (WIFSIGNALED(status))
				fprintf(stderr, "%s:proc %d died of signal %d\n",
					Cmd, pid, WTERMSIG(status));
			pid = waitpid(-1, &status, WNOHANG);
		}

	} else if (sig == SIGTERM) {
		if (get_pid() != master)
			exit(0);
		terms++;
	} else {
		sigignore(SIGCLD);
		errprintf(ERR_EXIT, "Unknown signal %d\n", sig);
		/* NOTREACHED */
	}
}
