/*
 * ckpt_pthread.c
 *
 *      Routines for saving/restoring process pthreads
 *
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.6 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/procfs.h>
#include <task.h>
#include <sys/ckpt_procfs.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <ckpt.h>
#include <ckpt_internal.h>
#include "librestart.h"

extern void ckpt_update_proclist_states(unsigned long, uint_t);
extern void ckpt_perror(char *, int);

extern int *errfd_p;
extern pid_t *errpid_p;
extern int *__errnoaddr;
#define ERRNO *__errnoaddr

/*
 * Routines to recreate uthreads in a multi-thread process
 *
 * We wait to now to recreate the underlying uthreads so we can use generic
 * blocking semantics on the process until this point.
 */
static int pthread_init = 0;
static int pthread_count = 0;
static int thread_zero = 0;
static long long thread_stk[256]; /* need just enough stack to allocate and */
				  /* switch to a privte one */
static int pthread_errfd;
static pid_t pthread_errpid;
/*
 * Since the stacks are (necessarily) mpped local, we need to stage some
 * parametes through some static buffers
 */
static ckpt_ta_t thread_ta;


/*
 * crated thread comes here running on it's own, low memory stack
 */
static void
ckpt_thread_restore(ckpt_ta_t *tp)
{
	ckpt_ta_t target_low;		/* local copy of target args */
	int dummy = 1;
	char status = -1;

	target_low = *tp;
	/*
	 * It's okay to reuse stack now
	 */
#ifdef NOYET
	/*
	 * Issue regarding shared fds..particularly, shared file offset
	 */
	prctl(PR_THREAD_CTL, PR_THREAD_UNBLOCK, 0);
#endif
	if (ckpt_restore_mappings(&target_low,
				  0,
				  target_low.tid,
				  1,			/* local only! */
				  &dummy,		/* depend_sync */
				  &dummy,		/* prattach */
				  &dummy) < 0) {	/* brk */
		/*
		 * If we get here, restoring memory failed
		 */
		ckpt_update_proclist_states(tp->pindex, CKPT_PL_ERR);
		(void)write(tp->cprfd, &status, 1);
		return;
	}
	/*
	 * Alert master that we're done restoring
	 */
	prctl(PR_THREAD_CTL, PR_THREAD_UNBLOCK, 0);

	restartreturn(-target_low.blkcnt, 0, 0);
}
/*
 * created thread context comes here and initiates its own memory
 * restoration
 *
 * Basically get thread running on its a private stack
 */
/* ARGSUSED */
static void
ckpt_thread_launch(void *arg, size_t stacksize)
{
	ckpt_ta_t *tp = (ckpt_ta_t *)arg;

	ckpt_free(tp->index);

	ckpt_stack_switch(ckpt_thread_restore, tp);
}

int
ckpt_read_thread(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	prthread_t prt;
	ckpt_thread_inst_t cti;
	ckpt_uti_t *uti = &cti.cti_uti;
	off_t ifd_offset;
	off_t sfd_offset;

	assert(saddrlist == NULL);
	assert(!IS_SPROC(&tp->pi));

	if (read(tp->sfd, &cti, sizeof(cti)) < 0) {
		ckpt_perror("read thread instance", ERRNO);
		return (-1);
	}
	if (cti.cti_magic != magic) {
		ckpt_perror("thread magic", 0);
		return (-1);
	}
	if ((uti->uti_flags & CKPT_UTI_PTHREAD)&&(pthread_init == 0)) {
		/*
		 * register to create pthreads
		 */
		if (prctl(PR_INIT_THREADS) < 0) {
			ckpt_perror("pthread init", ERRNO);
			return (-1);
		}
		pthread_errfd = tp->errfd;
		errfd_p = &pthread_errfd;
		pthread_errpid = *errpid_p;
		errpid_p = &pthread_errpid;

		pthread_init = 1;
	}
	if (cti.cti_tid == 0) {
		/*
		 * Thread zero is valid.  Since we are currently on thread
		 * 0, we don't need to create a thread, but we may need to
		 * set scheduling params
		 */
		assert(get_tid() == 0);

		thread_zero = 1;

		if ((uti->uti_flags & (CKPT_UTI_SCHED|CKPT_UTI_PTHREAD)) ==
				      (CKPT_UTI_SCHED|CKPT_UTI_PTHREAD)) {

			if (prctl(PR_THREAD_CTL,
				  PR_THREAD_SCHED,
				  0,
				  &uti->uti_sched) < 0) {
				ckpt_perror("pthread scheduler init", ERRNO);
				return (-1);
			}
		}
		tp->blkcnt = uti->uti_blkcnt;

		return (0);
	}
	/*
	 * Need to create a pthread.  Set up a restart arg struct.
	 * Get "private" fds, but share other struct pointers
	 */
	ifd_offset = lseek(tp->ifd, 0, SEEK_CUR);
	sfd_offset = lseek(tp->sfd, 0, SEEK_CUR);

	thread_ta = *tp;

	thread_ta.blkcnt = uti->uti_blkcnt;
	thread_ta.tid = cti.cti_tid;

	prt.prt_entry = ckpt_thread_launch;
	prt.prt_arg = (caddr_t)&thread_ta;
	prt.prt_flags = 0;
	prt.prt_stklen = sizeof(thread_stk);
	prt.prt_stkptr = (caddr_t)thread_stk + sizeof(thread_stk);

	if (nsproctid(	ckpt_thread_launch,
			PR_THREADS|PR_NOLIBC,
			&prt,
			(uti->uti_flags&CKPT_UTI_SCHED)? &uti->uti_sched : NULL,
			cti.cti_tid) < 0) {
		ckpt_perror("pthread create", ERRNO);
		return (-1);
	}
	pthread_count++;
	/*
	 * wait for thread to get on it's private stack
	 */
	prctl(PR_THREAD_CTL, PR_THREAD_BLOCK);

	lseek(tp->ifd, ifd_offset, SEEK_SET);
	lseek(tp->sfd, sfd_offset, SEEK_SET);

	return (0);
}

void
ckpt_pthread_wait(void)
{
	/*
	 * wait for ny pthreads we created to finish mapping
	 */
#ifdef NOTYET
	while (pthread_count-- > 0)
		prctl(PR_THREAD_CTL, PR_THREAD_BLOCK);
#endif
}

void
ckpt_pthread0_check(void)
{
	/*
	 * If we're running pthreads and thread 0 is *not* supposed to exist
	 * then exit now...the other threads are all safely launched
	 */
	if (pthread_init && !thread_zero)
		(void)prctl(PR_THREAD_CTL, PR_THREAD_EXIT);
}
