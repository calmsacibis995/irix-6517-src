/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/* NOTES
 *
 * This module implements the fork() and exit() functionality.
 *
 * Key points:
	* fork() needs to run the atfork handlers
	* Child must clean up pthread library state.
	* exit() needs to single thread atexit and make sure any
	  runtime exit code is run (e.g. C++ streamio)
 */


#include "common.h"
#include "asm.h"
#include "pt.h"
#include "pthreadrt.h"
#include "ptdbg.h"
#include "q.h"
#include "sig.h"
#include "vp.h"

#include <errno.h>
#include <pthread.h>
#include <rld_interface.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>


static pt_t	*exiting_pt;	/* exit() synchronisation variable */

/*
 * Single handler queue for fork() handlers.
 * Using 3 queues would save space for unused entries but waste it
 * with additional malloc()'s and be slower.
 */
static pthread_mutex_t	fh_mtx = PTHREAD_MUTEX_INITIALIZER;
static Q_DECLARE(handlers);

typedef struct fh {
	q_t	fh_q;
	void	(*fh_prepare)(void);	/* LIFO in parent before fork() */
	void	(*fh_parent)(void);	/* FIFO in parent after fork() */
	void	(*fh_child)(void);	/* FIFO in child after fork() */
} fh_t;


/*
 * pthread_atfork(prepare, parent, child)
 */
int
pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	fh_t	*fh;

	if (!(fh = _malloc(sizeof(fh_t)))) {
		return (ENOMEM);
	}
	fh->fh_prepare = prepare;
	fh->fh_parent = parent;
	fh->fh_child = child;

	pthread_mutex_lock(&fh_mtx);

	Q_ADD_LAST(&handlers, fh);

	pthread_mutex_unlock(&fh_mtx);

	return (0);
}


/*
 * libc_fork(void)
 */
pid_t
libc_fork(void)
{
	pid_t		fork_pid;	/* fork() return value */
	int		fork_errno;
	q_t		snap_q;		/* handler (sub-) list */
	fh_t		*fh;
	extern pid_t	__fork(void);

	TRACE(T_MISC, ("fork"));

	/* Grab a copy of the list head and tail; since handlers cannot
	 * be popped we can traverse this (sub) list without holding
	 * the lock and thus avoid deadlock.
	 */
	pthread_mutex_lock(&fh_mtx);
	snap_q = handlers;
	pthread_mutex_unlock(&fh_mtx);

	for (fh = (fh_t *)snap_q.prev; fh != (fh_t *)&handlers;
	     fh = (fh_t *)(((q_t *)fh)->prev)) {

		if (fh->fh_prepare)
			(*fh->fh_prepare)();
	}

	sched_enter();

	/* Sync up the malloc lock so that we can alloc/free memory
	 * in the child.
	 */
	libc_fork_prepare();

	if (!(fork_pid = __fork())) {

		/*
		 * Running in child...
		 */

		VP_RESCHED = 0;

		libc_fork_child();

		if (prctl(PR_INIT_THREADS)) {
			panic("fork", "Couldn't INIT_THREADS");
		}

		exiting_pt = 0;			/* reset for exit()s */
		pt_fork_child();

		ASSERT(sched_entered() == 1);
		sched_leave();

		/* Remind rld that we are multithreaded.
		 */
		_rld_new_interface(_RLD_PTHREADS_START, &jt_rld_pt);

		for (fh = (fh_t *)snap_q.next; fh != (fh_t *)&handlers;
		     fh = (fh_t *)((q_t *)fh)->next) {

			if (fh->fh_child)
				(*fh->fh_child)();

			if (fh == (fh_t *)snap_q.prev)
				break;
		}

		return (0);
	}

	/*
	 * Running in parent...
	 */

	/* If the fork() failed we need to protect errno from
	 * the handlers.
	 */
	fork_errno = oserror();

	libc_fork_parent();

	sched_leave();

	TRACE(T_MISC, ("fork() ret %#d", fork_pid));
	ASSERT(fork_pid);

	for (fh = (fh_t *)snap_q.next; fh != (fh_t *)&handlers;
	     fh = (fh_t *)((q_t *)fh)->next) {

		if (fh->fh_parent)
			(*fh->fh_parent)();

		if (fh == (fh_t *)snap_q.prev)
			break;
	}

	setoserror(fork_errno);

	return (fork_pid);
}


/*
 * exit_certain(code)
 *
 * Handler to trap thread termination during exit();
 */
static void
exit_certain(void *arg)
{
	int	code = *(int *)arg;

	_exit(code);
}


/*
 * exit_cleanup()
 *
 * atfork()/ fini routines/ stdio flush
 */
static void
exit_cleanup(void)
{
	extern void	_exithandle(void);
	extern void	_cleanup(void);
	extern void	_DYNAMIC_LINK();

	_exithandle();				/* atexit() */
	if (_DYNAMIC_LINK) {
		_rld_new_interface(_SHUT_DOWN);	/* e.g. C++ */
	}
	_cleanup();				/* streamio */
}


/*
 * libc_exit(code)
 *
 * Multithreaded exit().
 * Ensure proper exit semantics by the first thread.
 * Subsequent callers wait forever - the first thread will destroy them
 * when it _exit()s.
 */
void
libc_exit(int code)
{
	pt_t		*pt_self = PT;

	/* Early exits have no pthread context.
	 */
	if (!pt_self) {
		exit_cleanup();
		exit_certain(&code);
	}

	cmp0_and_swap(&exiting_pt, pt_self);
	if (exiting_pt != pt_self) {
		VP_YIELD((sched_yield(), 1));	/* permit preemption */
	}
	pthread_cleanup_push(exit_certain, &code);
	exit_cleanup();
#	pragma set woff 1209
	pthread_cleanup_pop(TRUE);	/* run handler */
#	pragma reset woff 1209
}
