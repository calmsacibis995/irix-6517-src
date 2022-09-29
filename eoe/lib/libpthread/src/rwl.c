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
 * This module implements read-write locks.
 */

#include "common.h"
#include "asm.h"
#include "intr.h"
#include "pt.h"
#include "vp.h"
#include "rwlattr.h"
#include "rwl.h"

#include <errno.h>
#include <pthread.h>
#include <sys/usync.h>

static int	rwl_rdlock_pshared(pthread_rwlock_t *);
static int	rwl_wrlock_pshared(pthread_rwlock_t *);
static int	rwl_unlock_pshared(pthread_rwlock_t *);

#define RWL_INITIALIZED(rwl)	((rwl)->rwl_rdwaitq.next)


/* Read-write locks.
 *
 * Read-write locks allow a thread to exclusively lock some shared data
 *	while updating that data, but allow any number of threads to have
 *	read-only access to that data.
 *
 *	A private read-write lock contains a simple lock, two separate waitq's
 *	and a busy bit that indicates a thread is on one of the queues.
 *	A shared read-write lock contains a mutex and two separate wait
 *	counters.  Both versions use a read counter that is the number of 
 *	active readers or -1 if a writer owns the lock.
 */


/*
 * pthread_rwlock_init(rwl, attr)
 *
 * Initialize read-write lock.
 */
int
pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *pattr)
{
	pthread_mutexattr_t	mtx_attr;

	rwlock->rwl_attr = pattr ? *(pthread_rwlockattr_t*)pattr : rwlattr_default;
	if (!rwlock->rwl_attr.ra_pshared) {
		lock_init(&rwlock->rwl_slock);
		Q_INIT(&rwlock->rwl_rdwaitq);
		Q_INIT(&rwlock->rwl_wrwaitq);
		rwlock->rwl_busybit = 0;
	} else {
		pthread_mutexattr_init(&mtx_attr);
		pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&rwlock->rwl_mtx, &mtx_attr);
		pthread_mutexattr_destroy(&mtx_attr);
		rwlock->rwl_rwaiters = 0;
		rwlock->rwl_wwaiters = 0;
	}
	rwlock->rwl_rdcount = 0;

	return (0);
}


/*
 * pthread_rwlock_destroy(rwlock)
 *
 *  Specified rwlock is destroyed and removed from the system.
 *  If rwlock is BUSY then error is returned.
 */
int
pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
	if (rwlock->rwl_rdcount) {
		return (EBUSY);
	}

	if (!rwlock->rwl_attr.ra_pshared) {

		/* Avoid destroying rwl if library may be trying to lock it.
	 	*/
		VP_YIELD(intr_destroy_wait());
	}

	return (0);
}


/*
 * pthread_rwlock_tryrdlock(rwlock) - try to lock rwlock for reading
 */
int
pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
	TRACE(T_RWL, ("pthread_rwlock_tryrdlock(%#x)", rwlock));

	/*
	 * If no writer holds the lock (rdcount == -1), allow 
	 * read-only access.
	 */
	if (add_if_greater(&rwlock->rwl_rdcount, -1, 1)) {
		return (0);
	}

	return (EBUSY);
}


/*
 * pthread_rwlock_trywrlock(rwlock) - try to lock rwlock for writing
 */
int
pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
	TRACE(T_RWL, ("pthread_rwlock_trywrlock(%#x)", rwlock));

	/*
	 * If no reader or writer has the lock, allow write access.
	 */
	if (add_if_equal(&rwlock->rwl_rdcount, 0, -1)) {
		return (0);
	}

	return (EBUSY);
}


/*
 * pthread_rwlock_rdlock(rwlock) - lock rwlock for reading
 *
 * If rwlock is busy then the thread will be placed on the
 * read waiting queue.
 */
int
pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	register pt_t	*pt_self = PT;

	/*
	 * If no writer holds the lock (rdcount == -1), allow 
	 * read-only access.
	 */
	if (add_if_greater(&rwlock->rwl_rdcount, -1, 1)) {
		TRACE(T_RWL, ("pthread_rwlock_rdlock(%#x) atomic", rwlock));
		return (0);
	}

	TRACE(T_RWL, ("pthread_rwlock_rdlock(%#x)", rwlock));

	if (rwlock->rwl_attr.ra_pshared) {
		return (rwl_rdlock_pshared(rwlock));
	}

retry_rwlock_rdlock:
	/*
	 * Check for pending signals
	 * (we're not a cancellation point so ignore those).
	 */
	PT_INTR_PENDING(pt_self, PT_INTERRUPTS & ~PT_CANCELLED);

	lock_enter(&rwlock->rwl_slock);

	/*
	 * If the rwlock has become free for reading, then retry.
	 * Otherwise, enqueue ourselves.
	 */
	if (rwlock->rwl_rdcount > -1) {
		lock_leave(&pt_self->pt_lock);
		lock_leave(&rwlock->rwl_slock);
		sched_leave();
		if (add_if_greater(&rwlock->rwl_rdcount, -1, 1)) {
			return (0);
		}
		goto retry_rwlock_rdlock;
	}

	/*
	 * Read access not available.  Put ourself on the read wait queue.
	 */
	rwlock->rwl_busybit = 1;
	pt_self->pt_state = PT_RLWAIT;
	pt_self->pt_sync = (pt_sync_t *)rwlock;
	lock_leave(&pt_self->pt_lock);

	/*
	 * Fix up queues if this rwlock was statically initialized.
	 */
	if (!RWL_INITIALIZED(rwlock)) {
		Q_INIT(&(rwlock)->rwl_rdwaitq);
		Q_INIT(&(rwlock)->rwl_wrwaitq);
	}

	/*
	 * This implements FIFO, priority-based queueing.
	 */
	pt_q_insert_tail(&rwlock->rwl_rdwaitq, pt_self);

	lock_leave(&rwlock->rwl_slock);

	TRACE(T_RWL, ("pthread_rwlock_rdlock(%#x) sleep", rwlock));

	if (sched_block(SCHED_READY) == EINTR) {
		/*
		 * We were interrupted.  Go around loop again (we check for
		 * interrupts at the top).
		 */
		goto retry_rwlock_rdlock;
	}

	TRACE(T_RWL, ("pthread_rwlock_rdlock(%#x) wakeup", rwlock));

	return (0);
}


/*
 * rwl_rdlock_pshared(rwlock)
 *
 * Queue an inter-process rwlock for reading.
 */
static int
rwl_rdlock_pshared(pthread_rwlock_t *rwlock)
{
	usync_arg_t	usarg;

retry_rdlock_pshared:
	pthread_mutex_lock(&rwlock->rwl_mtx);

	/*
	 * If the rwlock has become free for reading, then retry.
	 * Otherwise, enqueue ourselves.
	 */
	if (rwlock->rwl_rdcount > -1) {
		pthread_mutex_unlock(&rwlock->rwl_mtx);
		if (add_if_greater(&rwlock->rwl_rdcount, -1, 1)) {
			return (0);
		}
		goto retry_rdlock_pshared;
	}

	/*
	 * Cannot get rwlock for reading, block until it's available.
	 */
	rwlock->rwl_rwaiters++;
	pthread_mutex_unlock(&rwlock->rwl_mtx);

	TRACE(T_RWL, ("pthread_rwlock_rdlock(%#x) sleep", rwlock));

	usarg.ua_version = USYNC_VERSION_2;
	usarg.ua_addr = (__uint64_t)&rwlock->rwl_rwaiters;
	usarg.ua_policy = USYNC_POLICY_PRIORITY;
	usarg.ua_flags = 0;
	while (pt_usync_cntl(USYNC_INTR_BLOCK, &usarg, FALSE) == -1) {
		if (oserror() != EINTR) {
			panic("rwl_rdlock_pshared",
			      "USYNC_INTR_BLOCK failed");
		}
	}

	TRACE(T_RWL, ("pthread_rwlock_rdlock(%#x) wakeup", rwlock));

	return (0);
}
	

/*
 * pthread_rwlock_wrlock(rwlock) - lock rwlock for writing
 *
 * If rwlock is busy then the thread will be placed on the
 * write waiting queue.
 */
int
pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	register pt_t	*pt_self = PT;

	/*
	 * If no reader or writer has the lock, allow write access.
	 */
	if (add_if_equal(&rwlock->rwl_rdcount, 0, -1)) {
		TRACE(T_RWL, ("pthread_rwlock_wrlock(%#x) atomic", rwlock));
		return (0);
	}

	TRACE(T_RWL, ("pthread_rwlock_wrlock(%#x)", rwlock));

	if (rwlock->rwl_attr.ra_pshared) {
		return (rwl_wrlock_pshared(rwlock));
	}

retry_rwlock_wrlock:
	/*
	 * Check for pending signals
	 * (we're not a cancellation point so ignore those).
	 */
	PT_INTR_PENDING(pt_self, PT_INTERRUPTS & ~PT_CANCELLED);

	lock_enter(&rwlock->rwl_slock);

	/*
	 * If the rwlock has become free for writing, then retry.
	 * Otherwise, enqueue ourselves.
	 */
	if (!rwlock->rwl_rdcount) {
		lock_leave(&pt_self->pt_lock);
		lock_leave(&rwlock->rwl_slock);
		sched_leave();
		if (add_if_equal(&rwlock->rwl_rdcount, 0, -1)) {
			return (0);
		}
		goto retry_rwlock_wrlock;
	}

	/*
	 * Write access not available.  Put ourself on the write wait queue.
	 */
	rwlock->rwl_busybit = 1;
	pt_self->pt_state = PT_WLWAIT;
	pt_self->pt_sync = (pt_sync_t *)rwlock;
	lock_leave(&pt_self->pt_lock);

	/*
	 * Fix up queues if this rwlock was statically initialized.
	 */
	if (!RWL_INITIALIZED(rwlock)) {
		Q_INIT(&(rwlock)->rwl_rdwaitq);
		Q_INIT(&(rwlock)->rwl_wrwaitq);
	}

	/*
	 * This implements FIFO, priority-based queueing.
	 */
	pt_q_insert_tail(&rwlock->rwl_wrwaitq, pt_self);

	lock_leave(&rwlock->rwl_slock);

	TRACE(T_RWL, ("pthread_rwlock_wrlock(%#x) sleep", rwlock));

	if (sched_block(SCHED_READY) == EINTR) {
		/*
		 * We were interrupted.  Go around loop again (we check for
		 * interrupts at the top).
		 */
		goto retry_rwlock_wrlock;
	}

	TRACE(T_RWL, ("pthread_rwlock_wrlock(%#x) wakeup", rwlock));

	return (0);
}


/*
 * rwl_wrlock_pshared(rwlock)
 *
 * Queue an inter-process rwlock for writing.
 */
static int
rwl_wrlock_pshared(pthread_rwlock_t *rwlock)
{
	usync_arg_t	usarg;

retry_wrlock_pshared:
	pthread_mutex_lock(&rwlock->rwl_mtx);

	/*
	 * If the rwlock has become free for writing, then retry.
	 * Otherwise, enqueue ourselves.
	 */
	if (!rwlock->rwl_rdcount) {
		pthread_mutex_unlock(&rwlock->rwl_mtx);
		if (add_if_equal(&rwlock->rwl_rdcount, 0, -1)) {
			return (0);
		}
		goto retry_wrlock_pshared;
	}

	/*
	 * Cannot get rwlock for writing, block until it's available.
	 */
	rwlock->rwl_wwaiters++;
	pthread_mutex_unlock(&rwlock->rwl_mtx);

	TRACE(T_RWL, ("pthread_rwlock_wrlock(%#x) sleep", rwlock));

	usarg.ua_version = USYNC_VERSION_2;
	usarg.ua_addr = (__uint64_t)&rwlock->rwl_wwaiters;
	usarg.ua_policy = USYNC_POLICY_PRIORITY;
	usarg.ua_flags = 0;
	while (pt_usync_cntl(USYNC_INTR_BLOCK, &usarg, FALSE) == -1) {
		if (oserror() != EINTR) {
			panic("rwl_wrlock_pshared",
			      "USYNC_INTR_BLOCK failed");
		}
	}

	TRACE(T_RWL, ("pthread_rwlock_wrlock(%#x) wakeup", rwlock));

	return (0);
}
	

/*
 * pthread_rwlock_unlock(rwlock) - unlock rwlock
 */
int
pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	register pt_t	*pt;
	pt_t		*wt, *dt;
	int		thr_count;

	TRACE(T_RWL, ("pthread_rwlock_unlock(%#x)", rwlock));

	/*
	 * atomic_unlock() atomically reads rdcount, if it is less than
	 * 1, it does nothing, if it is equal to 1, it sets it to -1,
	 * if it is greater than 1, it decrements it by 1.  It then
	 * returns the result.  This has the following effect:
	 *
	 * If this is a nested read unlock, the counter is decremented.
	 * If this is the last read unlock, it now owns the lock until
	 * the queues can be checked.
	 * If this is a write unlock, it keeps the lock until the queues
	 * can be checked.
	 */
	if (atomic_unlock(&rwlock->rwl_rdcount, 1, -2, -1) != -1) {
		return (0);
	}

	if (rwlock->rwl_attr.ra_pshared) {
		return (rwl_unlock_pshared(rwlock));
	}

	sched_enter();
	lock_enter(&rwlock->rwl_slock);

	/*
	 * This is a true unlock.  If there are write waiters, schedule 
	 * them first.  If there are only read waiters, schedule them all.
	 */
	if (rwlock->rwl_busybit) {
		if (!Q_EMPTY(&rwlock->rwl_wrwaitq)) {

			/*
			 * Relinquish rwlock to first write waiter.
			 */
		    	pt = pt_dequeue(&rwlock->rwl_wrwaitq);

			pt->pt_wait = 0;
			pt->pt_state = PT_DISPATCH;

			/*
			 * If the queues are now empty, clear the busy bit.
			 */
			if (Q_EMPTY(&rwlock->rwl_wrwaitq) &&
			    Q_EMPTY(&rwlock->rwl_rdwaitq)) {
				rwlock->rwl_busybit = 0;
			}

			lock_leave(&rwlock->rwl_slock);

			sched_dispatch(pt);
			sched_leave();

			return (0);

		} else if (!Q_EMPTY(&rwlock->rwl_rdwaitq)) {

			/*
			 * Mark pts for dispatch and detach from q.
			 */
			thr_count = 0;
			for (wt = Q_HEAD(&rwlock->rwl_rdwaitq, pt_t*);
			     !Q_END(&rwlock->rwl_rdwaitq, wt); 
			     wt = Q_NEXT(wt, pt_t*)) {
				wt->pt_state = PT_DISPATCH;
				thr_count++;
			}
			wt = Q_HEAD(&rwlock->rwl_rdwaitq, pt_t*);
			Q_INIT(&rwlock->rwl_rdwaitq);

			/*
			 * Queues should now be empty, clear the busy bit and
			 * reset rdcount to the number of waiters on the queue.
			 */
			rwlock->rwl_busybit = 0;
			rwlock->rwl_rdcount = thr_count;
			lock_leave(&rwlock->rwl_slock);

			/*
			 * Now dispatch original members.
			 */
			while (!Q_END(&rwlock->rwl_rdwaitq, wt)) {
				dt = wt;
				wt = Q_NEXT(wt, pt_t*);
				dt->pt_wait = 0;
				sched_dispatch(dt);
			}
			sched_leave();

			return (0);
		}
	}

	/*
	 * Either nobody is waiting or the waiter dequeued itself.
	 * Clear the busy bit and unlock the lock.
	 */
	rwlock->rwl_busybit = 0;
	rwlock->rwl_rdcount = 0;
	lock_leave(&rwlock->rwl_slock);

	sched_leave();

	return (0);
}


/*
 * rwl_unlock_pshared(rwlock)
 *
 * Unlock an inter-process rwlock.
 */
static int
rwl_unlock_pshared(pthread_rwlock_t *rwlock)
{
	usync_arg_t     usarg;
	int		num_waiters;

	pthread_mutex_lock(&rwlock->rwl_mtx);

	/*
	 * This is a true unlock.  If there are write waiters, schedule 
	 * them first.  If there are only read waiters, schedule them all.
	 */
	if (rwlock->rwl_wwaiters) {
		rwlock->rwl_wwaiters--;
		pthread_mutex_unlock(&rwlock->rwl_mtx);
		usarg.ua_version = USYNC_VERSION_2;
		usarg.ua_addr = (__uint64_t)&rwlock->rwl_wwaiters;
		usarg.ua_flags = 0;
		while (pt_usync_cntl(USYNC_UNBLOCK, &usarg, FALSE) == -1) {
			if (oserror() != EINTR) {
				panic("rwl_unlock_pshared",
					"USYNC_UNBLOCK failed");
			}
		}
	} else if (rwlock->rwl_rwaiters) {
		num_waiters = rwlock->rwl_rwaiters;
		rwlock->rwl_rdcount = num_waiters;
		rwlock->rwl_rwaiters = 0;
		pthread_mutex_unlock(&rwlock->rwl_mtx);
		usarg.ua_version = USYNC_VERSION_2;
		usarg.ua_addr = (__uint64_t)&rwlock->rwl_rwaiters;
		usarg.ua_flags = 0;
		usarg.ua_count = num_waiters;
		while (pt_usync_cntl(USYNC_UNBLOCK_ALL, &usarg, FALSE) == -1) {
			if (oserror() != EINTR) {
				panic("rwl_unlock_pshared",
					"USYNC_UNBLOCK_ALL failed");
			}
		}
	} else {
		rwlock->rwl_rdcount = 0;
		pthread_mutex_unlock(&rwlock->rwl_mtx);
	}

	return (0);
}
