
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

#ident "$Revision: 1.2 $"

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/resource.h>
#include <sys/lock.h>
#include <sys/sysmacros.h>

/*
** This program checks out the auto-locking of memory on new growth
** 1. plock(DATLOCK)
** 2. sbreak some random number of page  higher(below process limit)
** 3. access the new pages
** 4. decrement the stack 1 page lower
** 5. access the new stwack
** 6. free all sbreak memory
** 7. repeat 2-6 until stack memory exceed proc limit.
** Running this on a DEBUG kernel with assertions on to make sure that newly 
** allocated memory is auto-locked.
*/

volatile int *sp;
int *osp;

static void segvcatcher(void);

main(argc, argv)
char **argv;
{
	struct rlimit stack_rlp, rss_rlp;
	int i, count, oldbrk;
	
	sp = (volatile int *)&i;
	osp = (int *)sp;

	getrlimit(RLIMIT_STACK, &stack_rlp);
	getrlimit(RLIMIT_DATA, &rss_rlp);
	sigset(SIGSEGV, segvcatcher);

	plock(DATLOCK);
	for (count=0; count <stack_rlp.rlim_cur; count +=4096) {
		if (sbrk(10*4096) == -1)
			perror("autolock: fail sbreak(+)");
		sp -= 1024;
		*sp = 0;
		if (sbrk(-5*4096) == -1) 
			perror("autolock: fail sbreak(-)");
		if (sbrk(5*4096) == -1)
			perror("autolock: fail sbreak(+)");
	}
}

static void
segvcatcher(void)
{
	printf("Caught SIGSEGV, old stack is 0x%x, last stack is 0x%x\n",osp, sp);
	exit(0);
}	

