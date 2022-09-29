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
#ident "$Revision: 1.10 $"

#include "sys/types.h"
#include "stdlib.h"
#include "unistd.h"
#include "ulocks.h"
#include "limits.h"
#include "stdio.h"
#include "sys/times.h"
#include "sys/wait.h"
#include "signal.h"
#include "errno.h"

/*
 * test simple blocking by ringing threads
 */
#define Maxprocs()	(int)prctl(PR_MAXPROCS)
int Counter = 0;
int nprocs;
int Verbose = 0;
int pids[100];
char *Cmd;

extern void doit(int me);

int
main(int argc, char **argv)
{
	extern void sigcld(void), slave(void *);
	struct tms tm;
	register int i;
	int maxpthreads;		/* max threads per loop */
	int iter;
	clock_t start, ti;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	if (argc < 3) {
		fprintf(stderr, "Usage:%s [-v] num_procs iterations\n", argv[0]);
		exit(-1);
	}
	Cmd = argv[0];
	if (argc == 4) {
		argv++;
		Verbose++;
	}
	pids[0] = getpid();
	nprocs = atoi(argv[1]);
	iter = atoi(argv[2]);

	maxpthreads = Maxprocs();
	if (nprocs > maxpthreads)
		nprocs = maxpthreads;
	printf("%s:Creating %d processes\n", Cmd, nprocs);

	/* create the threads */
	sigset(SIGCLD, sigcld);
	usconfig(CONF_LOCKTYPE, US_DEBUG);
	usconfig(CONF_INITUSERS, nprocs);
	for (i = 1; i < nprocs; i++) {
		if (taskcreate("slave", slave, (void *)(__psint_t)i, 0) < 0) {
			fprintf(stderr, "%s:taskcreate failed errno:%d\n", Cmd, errno);
			goto bad;
		}
		printf("%s:Created process tid %d\n", Cmd, i);
	}
	/* wait for them all to go suspended */
	sginap(5);
loop:
	/* create so we have nprocs including ourselves */
	for (i = 1; i < nprocs; i++) {
		if (!taskctl(i, TSK_ISBLOCKED)) {
			fprintf(stderr, "%s:task:%d not suspended\n", Cmd, i);
			sginap(0);
			goto loop;
		}
	}
	start = times(&tm);

	/* ok, lets ring */
	for (i = 0; i < iter; i++) {
		doit(0);
		taskblock(0);
	}
	/*
	 * wait for them all to stop - otherwise we might kill one which
	 * has the task lock and hang is all
	 */
loop2:
	for (i = 1; i < nprocs; i++) {
		if (!taskctl(i, TSK_ISBLOCKED)) {
			fprintf(stderr, "%s:task:%d not suspended\n", Cmd, i);
			sginap(0);
			goto loop2;
		}
	}

	ti = times(&tm) - start;
	printf("%s:start:%x time:%x\n", Cmd, start, times(&tm));
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:time for %d threads %d iterations %d mS or %d uS/iteration \n",
		Cmd, nprocs, iter, ti, (ti*1000)/iter);

bad:
	sigset(SIGCLD, SIG_IGN);
	for (i = 1; i < nprocs; i++)
		taskdestroy(i);
	printf("%s:master exitting\n", Cmd);
	return 0;
}

void
slave(void *a)
{
	int arg = (int)(__psint_t)a;
	for (;;) {
		taskblock(arg);
		doit(arg);
	}
}

void
doit(int me)
{
	int i;
	int last = 0;

	/* make sure its my turn */
	if (last && (Counter != last + nprocs))
		fprintf(stderr, "%s:ERRORlast:%d Counter:%d nprocs:%d\n",
			Cmd, last, Counter, nprocs);
	Counter++;
	last = Counter;

	/* find next thread to start */
	i = me;
	if (++i >= nprocs)
		i = 0;
	if (Verbose)
		printf("%s:me:%d count:%d unblocking:%d\n",
			Cmd, me, Counter, i);
	taskunblock(i);
}

void
sigcld(void)
{
	auto int status;

	pid_t pid = wait(&status);
	fprintf(stderr, "%s:task died pid %d status:%x\n", pid, status);
}
