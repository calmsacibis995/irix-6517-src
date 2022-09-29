/**************************************************************************
 *									  *
 *		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
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
 * This module implements POSIX semaphore interfaces.
 *
 * Key points:
	* implement the pshared is false case as a pthread synchronisation
	* invoked by libc iff pshared is false
	* additional state for the pthread version hangs off the semaphore
	  in a special dynamically allocated structure
	* duplicate of the mutex path in most respects
 */

#include "common.h"
#include "intr.h"
#include "mtx.h"
#include "pt.h"
#include "pthreadrt.h"
#include "q.h"
#include "sys.h"
#include "vp.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore_internal.h>
#include <stdlib.h>


/*
 * Extended user state for non-shared unnamed semaphores
 */
typedef struct pxustate {
        slock_t         px_slock;
        q_t             px_waitq;
        int             px_prepost;
} pxustate_t;


/* libc_sem_init(sem, pshared, value)
 *
 * The extended user state is allocated for non-shared semaphores
 * so that the blocking/unblocking operations can be managed by
 * pthreads instead of the kernel (which is what the libc uses).
 */
int
libc_sem_init(sem_t *sem)
{
	pxustate_t	*xustate = (pxustate_t *)_malloc(sizeof(pxustate_t));

	ASSERT(sem->sem_flags & SEM_FLAGS_NOSHARE);

	if (!xustate) {
		setoserror(ENOSPC);
		return (-1);
	}

	lock_init(&xustate->px_slock);
	Q_INIT(&xustate->px_waitq);
	xustate->px_prepost = 0;

	sem->sem_xustate = (__psunsigned_t)xustate;

	return (0);
}


/* libc_sem_destroy(sem)
 *
 * Deallocates a extended part of unnamed semaphore.
 */
int
libc_sem_destroy(sem_t *sem)
{
	pxustate_t	*xustate = (pxustate_t *)sem->sem_xustate;

	ASSERT(sem->sem_flags & SEM_FLAGS_NOSHARE);
	ASSERT(xustate);

	sched_enter();
	lock_enter(&xustate->px_slock);
	if (!Q_EMPTY(&xustate->px_waitq)) {
		lock_leave(&xustate->px_slock);
		sched_leave();
		setoserror(EBUSY);
		return (-1);
	}
	sched_leave();

	_free(xustate);

	return (0);
}


/* libc_sem_wait(sem)
 *
 * Grab a prepost or wait in the scheduler.
 */
int
libc_sem_wait(sem_t *sem)
{
	register pt_t	*pt_self = PT;
	pxustate_t	*xustate;
	int	oldstate;
	int	enable_cancel = 0;

	TRACE(T_SEM, ("libc_sem_wait(%#x)", sem));
	ASSERT(sem->sem_flags & SEM_FLAGS_NOSHARE);

	if (sem->sem_flags & SEM_FLAGS_NOCNCL
	    && pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate) == 0
	    && oldstate == PTHREAD_CANCEL_ENABLE) {
		enable_cancel = 1;
	}

	/* Semaphore is unavailable, block.
	 */
retry_wait:
	PT_INTR_PENDING(pt_self, PT_INTERRUPTS);

	xustate = (pxustate_t *)sem->sem_xustate;
	lock_enter(&xustate->px_slock);

	if (xustate->px_prepost) {
		TRACE(T_SEM, ("libc_sem_wait(%#x) preposted", sem));
		xustate->px_prepost--;
		lock_leave(&pt_self->pt_lock);
		lock_leave(&xustate->px_slock);
		sched_leave();
		if (enable_cancel) {
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
		}
		return (0);
	}

	/* Semaphore is not available.
	 * Put ourself on the wait queue.
	 */
	pt_self->pt_state = PT_SWAIT;
	pt_self->pt_sync = (pt_sync_t *)xustate;
	lock_leave(&pt_self->pt_lock);

	/* This implements FIFO, priority-based queueing.
	 */
	pt_q_insert_tail(&xustate->px_waitq, pt_self);
	lock_leave(&xustate->px_slock);

	if (sched_block(SCHED_READY) == EINTR) {
		goto retry_wait;
	}
	TRACE(T_SEM, ("sem_wait(%#x) posted", sem));

	if (enable_cancel) {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
	}
	return (0);
}


/* libc_sem_trywait(sem)
 *
 * Grab a prepost or return failure.
 */
int
libc_sem_trywait(sem_t *sem)
{
	pxustate_t	*xustate = (pxustate_t *)sem->sem_xustate;

	TRACE(T_SEM, ("libc_sem_trywait(%#x)", sem));
	ASSERT(sem->sem_flags & SEM_FLAGS_NOSHARE);
	ASSERT(xustate);

	sched_enter();
	lock_enter(&xustate->px_slock);

	if (xustate->px_prepost > 0) {
		xustate->px_prepost--;

		lock_leave(&xustate->px_slock);
		sched_leave();

		TRACE(T_SEM, ("libc_sem_trywait(%#x) pre-posted", sem));

		return (0);
	}

	lock_leave(&xustate->px_slock);
	sched_leave();

	TRACE(T_SEM, ("libc_sem_trywait(%#x) failed", sem));

	setoserror(EAGAIN);
	return (-1);
}


/* libc_sem_post(sem)
 *
 * If any waiters wake up the first one otherwise bump prepost.
 */
int
libc_sem_post(sem_t *sem)
{
	register pt_t	*pt;
	pxustate_t	*xustate = (pxustate_t *)sem->sem_xustate;

	ASSERT(sem->sem_flags & SEM_FLAGS_NOSHARE);
	TRACE(T_SEM, ("libc_sem_post(%#x)", sem));

	/* Wakeup a waiting process
	 */
	sched_enter();
	lock_enter(&xustate->px_slock);

	/* If there are waiters we wake-up the first one.
	 */
	if (!Q_EMPTY(&xustate->px_waitq)) {
		pt = pt_dequeue(&xustate->px_waitq);

		pt->pt_wait = 0;
		pt->pt_state = PT_DISPATCH;

		lock_leave(&xustate->px_slock);

		sched_dispatch(pt);
	} else {
		/* Nobody to wake-up, prepost.
		 */
		xustate->px_prepost++;

		lock_leave(&xustate->px_slock);
	}

	sched_leave();
	return (0);
}


/* libc_sem_getvalue(sem, sval)
 */
int
libc_sem_getvalue(sem_t *sem, int *sval)
{
	pxustate_t	*xustate = (pxustate_t *)sem->sem_xustate;

	*sval = xustate->px_prepost + sem->sem_value;
	return (0);
}
