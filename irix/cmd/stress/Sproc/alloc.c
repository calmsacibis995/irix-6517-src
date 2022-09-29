/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.11 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <ulocks.h>
#include <errno.h>
#include <malloc.h>
#include <wait.h>
#include "stress.h"

extern void dumpmi(struct mallinfo *mi);
char *Cmd;

/*
 * test lock allocation to the limit
 * 1) check that can alloc up to max #, use them and realloc them
 * 2) check that recovery after failure (not freeing) doesn't cause
 *	problems for next invocation
 * 3) show that sproc guys can alloc new pages of locks
 */
#define OVERHEAD	7
usptr_t *arena;
int nlocks = 4096;
int asize = (1024*1024);
int quit;
int childonly = 0;
int verbose = 0;
char *filename;
struct mallinfo mi;

void
TestLocks(int nofree, int expect, pid_t pid)
{
	int i;
	ulock_t *lock;

	if(!(lock = (ulock_t *)malloc(nlocks * sizeof(ulock_t)))) {
		errprintf(3, "malloc of locks array size=%d fails",
			nlocks * sizeof(ulock_t));
		return;
	}
	for(i = 0; i < nlocks; i++) {
		lock[i] = usnewlock(arena);
		if (lock[i] == NULL) {
			if (verbose)
				printf("alloc:%d:cannot alloc %dth lock:%s\n",
					pid, i, strerror(oserror()));
			break;
		}
		if (verbose && (i % 100) == 0)
			printf("alloc:%d:Testing lock %3d, addr = 0%x\n",
					pid, i, lock[i]);
		ussetlock(lock[i]);
		usunsetlock(lock[i]);
	}

	/* overhead differs between software/hardware locks
	 * just check to be sure got at least expected #
	 */
	if (i >= expect) {
		if (verbose)
			printf("alloc:%d:Lock test passed. expected %d & got %d locks\n",
				pid, expect, i);
	} else
		errprintf(1, "expected %d and got %d locks", expect, i);
		/* NOTREACHED */

	if (!nofree) {
		if (verbose)
			printf("alloc:%d:freeing %d locks\n", get_pid(), i);
		while (--i >= 0)
			usfreelock(lock[i], arena);
	}
}

void
Child(int nlocks)
{
	slaveexinit();

	if (verbose)
		printf("alloc:%d:child alloc and %s\n",
			get_pid(), quit ? "leave" : "dealloc");
	TestLocks(quit, nlocks, get_pid());
	if (verbose)
		printf("alloc:%d:child is done\n", get_pid());
}

int
main(int argc, char **argv)
{
	int c, status;
	pid_t pid;

	Cmd = errinit(argv[0]);
	while((c = getopt(argc, argv, "m:dvn:c")) != EOF)
	switch (c) {
	case 'd':
		usconfig(CONF_LOCKTYPE, US_DEBUG);
		break;
	case 'n':
		nlocks = atoi(optarg);
		break;
	case 'c':
		childonly++;
		break;
	case 'v':
		verbose++;
		break;
	case 'm':
		asize = atoi(optarg) * 1024;
		break;
	default:
		fprintf(stderr, "Usage:alloc [-v][-c][-d][-n #][-m size (kb)]\n");
		exit(-1);
	}
	usconfig(CONF_INITSIZE, asize);
	filename = tempnam(NULL, "alloc");
	if ((arena = usinit(filename)) == NULL) {
		errprintf(1, "usinit");
		/* NOTREACHED */
	}
	quit = 0;	/* dealloc locks */

	if (!childonly) {
		if (verbose)
			printf("alloc:%d:parent alloc and dealloc\n", get_pid());
		TestLocks(quit, nlocks - OVERHEAD, get_pid());
	}
	mi = usmallinfo(arena);
	dumpmi(&mi);

	/*
	 * in case parent decides to exit - this makes sure that all
	 * kids will be killed
	 */
	parentexinit(0);

	pid = sproc((void(*)(void *))Child, PR_SALL, nlocks - OVERHEAD);
	if (pid < 0) {
		unlink(filename);
		if (errno != EAGAIN) {
			errprintf(1, "sproc failed");
			/* NOTREACHED */
		} 
		printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
		exit(0);
	}

	/* wait for child */
	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if ((pid >= 0) && WIFSIGNALED(status)) {
			/*
			 * if anyone dies abnormally - get out
			 * But don't abort - the process that got the signal
			 * hopefully aborted - that core dump is more
			 * interesting
			 */
			errprintf(2, "proc %d died of signal %d\n",
				pid, WTERMSIG(status));
			unlink(filename);
			if (!WCOREDUMP(status))
				abort();
			exit(1);
		}
	}

	mi = usmallinfo(arena);
	dumpmi(&mi);

	quit = 1;
	if (!childonly) {
		if (verbose)
			printf("alloc:%d:parent alloc and leave\n", get_pid());
		TestLocks(quit, nlocks - OVERHEAD, get_pid());
	}
	mi = usmallinfo(arena);
	dumpmi(&mi);

	pid = sproc((void(*)(void *))Child, PR_SALL,
			childonly ? nlocks - OVERHEAD : 0);
	if (pid < 0) {
		unlink(filename);
		if (errno != EAGAIN) {
			errprintf(1, "sproc failed");
			/* NOTREACHED */
		} 
		printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
		exit(0);
	}

	/* wait for child */
	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if ((pid >= 0) && WIFSIGNALED(status)) {
			/*
			 * if anyone dies abnormally - get out
			 * But don't abort - the process that got the signal
			 * hopefully aborted - that core dump is more
			 * interesting
			 */
			errprintf(2, "proc %d died of signal %d\n",
				pid, WTERMSIG(status));
			unlink(filename);
			if (!WCOREDUMP(status))
				abort();
			exit(1);
		}
			
	}

	mi = usmallinfo(arena);
	dumpmi(&mi);

	printf("%s:%d:PASSED\n", Cmd, getpid());
	unlink(filename);
	return 0;
}

void
dumpmi(struct mallinfo *mi)
{
	size_t size;

	if (verbose) {
		printf("Mallinfo:arena:%d ordblks:%d smblks:%d hblkhd:%d\n",
			mi->arena, mi->ordblks, mi->smblks, mi->hblkhd);
		printf("\thblks:%d usmblks:%d fsmblks:%d uordblks:%d\n",
			mi->hblks, mi->usmblks, mi->fsmblks, mi->uordblks);
		printf("\tfordblks:%d keepcost:%d\n",
			mi->fordblks, mi->keepcost);
	}

	size = mi->hblkhd + mi->usmblks + mi->fsmblks + mi->uordblks +
		mi->fordblks;
	if (mi->arena != size)
		errprintf(ERR_EXIT, "size %d != arena %d\n", size, mi->arena);
}
