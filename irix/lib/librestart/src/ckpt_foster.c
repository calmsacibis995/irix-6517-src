/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.6 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <mutex.h>
#include <strings.h>
#include <fcntl.h>
#include <ucontext.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <ckpt.h>
#include <ckpt_internal.h>
#include "librestart.h"


static void
ckpt_foster_care_low(void)
{
	char procpath[32];
	int stat;

	/*
	 * Get rid of non-reserved memory
	 */
	sprintf(procpath, "/proc/%d", getpid());
	if (ckpt_relvm(NULL, procpath, 1, 0) < 0)
		exit(1);

	while (wait(&stat) >= 0) {
		if (WIFSTOPPED(stat))
			continue;
		if (WIFEXITED(stat) || WIFSIGNALED(stat)) {
			/*
			 * We'll exit with exact same status as our
			 * foater child, so shell sees its status
			 */
			restartreturn(0, stat, 1);
			/* no return */
			exit(1);
		}
	}
	exit(2);
}

/*
 * Let's get small.  We're going to hang around so let's be as inconspicuous
 * as possible
 */
int
ckpt_foster_care(void)
{
	ucontext_t	ucontext;
	int zfd;

	getcontext(&ucontext);

	if ((zfd = open(DEVZERO, O_RDWR)) < 0) {
		cerror("ckpt_foster_care:open");
		return (-1);
	}
	if (mmap(CKPT_MINRESERVE,
		 CKPT_STKRESERVE-CKPT_MINRESERVE,
		 PROT_READ|PROT_WRITE,
		 MAP_PRIVATE|MAP_LOCAL|MAP_FIXED,
		 zfd,
		 0) == MAP_FAILED) {
		cerror("ckpt_foster_care:mmap");
		close(zfd);
		return (-1);
	}
	close(zfd);
	/*
	 * switch stacks
	 */
	ucontext.uc_stack.ss_sp = (char *)(CKPT_STKRESERVE);
	ucontext.uc_stack.ss_size = CKPT_STKRESERVE-CKPT_MINRESERVE;
	makecontext(&ucontext, ckpt_foster_care_low, 0);
	setcontext(&ucontext);
	/*
	 * No return
	 */
	assert(0);
	return (-1);
}
