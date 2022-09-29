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
 * This module implements mutex locks.
 *
 * Key points:
	* The fast lock/unlock path is a test and set operation which
	  is interlocked with a busy bit to indicate waiters.
	* Mutex locks block the pthread context if the lock is already held.
	* Priority inheritance and ceiling is implemented.
	* Register pressure caused the fast/slow path split.
 */

#include "common.h"
#include "asm.h"
#include "intr.h"
#include "pt.h"
#include "pthreadrt.h"
#include "vp.h"
#include "mtxattr.h"
#include "mtx.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <mutex.h>
#include <sys/usync.h>

/*
 * Lock to manage access to the owner of a recursive pshared mutex.
 * This lock keeps other threads in the same process from obtaining
 * the lock while a check for a recursize pshared mutex is being done.
 * Note that it is only necessary to obtain this lock in the case
 * of setting a new owner for the mutex.  Clearing the owner in the
 * unlock case does not require grabbing the mutex.
 */
static slock_t	pshared_pid_lock;

static int	mtx_slow_lock(pthread_mutex_t *, pt_t *);
static int	mtx_slow_unlock(pthread_mutex_t *);

static int	mtx_trylock_pshared(pthread_mutex_t *);
static int	mtx_lock_pshared(pthread_mutex_t *);
static int	mtx_unlock_pshared(pthread_mutex_t *);

static int	mtx_prioq_set(pthread_mutex_t *, int);
static void	mtx_prioq_reset(pthread_mutex_t *, int);
static void	mtx_inheritq(pthread_mutex_t *, pt_t *);

/*
 * Flags for mtx_{set,reset}_prioq
 *
 * The mtx_prioq_set() interface always updates the caller's priority,
 * so the PRIOQ_PRIO flag need not be set.
 */
#define	PRIOQ_PRIO		0x0001	/* update priority */
#define	PRIOQ_QUEUE		0x0002	/* update queue */
#define	PRIOQ_UNLOCKED		0x0004	/* mutex is unlocked */

#define MUTEX_INITIALIZED(m)	((m)->mtx_waitq.next)

#define MTX_BUSY_BIT		1
#define OWNER_NOT_BUSY(owner)	\
			(!((__psunsigned_t)(owner) & MTX_BUSY_BIT))
#define OWNER_ADD_BUSY(owner)	\
			(pt_t *)((__psunsigned_t)(owner) | MTX_BUSY_BIT)
#define OWNER_STRIP_BUSY(owner)	\
			(pt_t *)((__psunsigned_t)(owner) & ~MTX_BUSY_BIT)
#define MTX_OWNER(mtx)		\
			OWNER_STRIP_BUSY((mtx)->mtx_owner)


/* Mutexes.
 *
 * Mutexes are user visible locks.
 *	A mutex contains a simple lock, an owner field, a waitq
 *	and scheduling data.
 *	To permit fast acquisition and release of mutexes a busy bit is
 *	added to the owner field whenever a thread is waiting.
 *	If the owner field is 0 the mutex is unlocked, otherwise it is
 *	currently locked and the busy bit indicates whether or not there
 *	are waiters.  The busy bit is never set unless there are waiters.
 *	The busy bit is only set or cleared under the simple lock.
 */

void
mtx_bootstrap()
{
	lock_init(&pshared_pid_lock);
}


/*
 * pthread_mutex_init(mutex, attr)
 *
 * Initialize mutex.
 */
int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *pattr)
{
	mutex->mtx_attr = pattr ? *(pthread_mutexattr_t *)pattr
				: mtxattr_default;
	if (!mutex->mtx_attr.ma_pshared) {
		lock_init(&mutex->mtx_slock);
		mutex->mtx_owner = 0;
		Q_INIT(&mutex->mtx_waitq);
	} else {
		mutex->mtx_pid = 0;
		mutex->mtx_thread = 0;
		mutex->mtx_waiters = 0;
	}
	mutex->mtx_inhprev = mutex->mtx_inhnext = 0;
	mutex->mtx_lckcount = 1;

	return (0);
}


/*
 * pthread_mutex_destroy(mutex)
 *
 *  Specified mutex is destroyed and removed from the system.
 *  If mutex is BUSY then error is returned. We don't expect to
 *  have threads waiting if mutex is BUSY.  Release of the
 *  mutex will make the first thread on the list a new owner
 *  and make the thread ready.
 */
int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (!mutex->mtx_attr.ma_pshared) {
		if (mutex->mtx_owner) {
			return (EBUSY);
		}

		/* Avoid destroying mtx if library may be trying to lock it.
		 */
		VP_YIELD(intr_destroy_wait());
	} else if (mutex->mtx_thread) {
		return (EBUSY);
	}

	return (0);
}


/*
 * pthread_mutex_trylock(mutex) - try to lock mutex
 */
int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	register pt_t	*pt_self = PT;

	TRACE(T_MTX, ("pthread_mutex_trylock(%#x)", mutex));

	if (mutex->mtx_attr.ma_pshared)
		return (mtx_trylock_pshared(mutex));

	if (cmp0_and_swap(&mutex->mtx_owner, pt_self)) {

		ASSERT(pt_self == MTX_OWNER(mutex));

		if (mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_NONE) {
			return (mtx_prioq_set(mutex, PRIOQ_QUEUE));
		}
		return (0);
	}

	/*
	 * If it's a recursive mutex owned by this thread, allow
	 * it to increment the lock.
	 * (Requires X/OPEN standard clarification.)
	 */
	if (mutex->mtx_attr.ma_type == PTHREAD_MUTEX_RECURSIVE &&
			MTX_OWNER(mutex) == pt_self) {
		mutex->mtx_lckcount++;
		return (0);
	}

	return (EBUSY);
}


/*
 * mtx_trylock_pshared(mutex)
 *
 * Try to lock an inter-process mutex.
 */
static int
mtx_trylock_pshared(pthread_mutex_t *mutex)
{
	register int	ret;
	register int	is_recursive;
	register int	has_ceiling;
	register pt_t	*pt_self = PT;

	TRACE(T_MTX, ("mtx_trylock_pshared(%#x)", mutex));

	has_ceiling = mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_PROTECT;
	if (has_ceiling && (ret = mtx_prioq_set(mutex, PRIOQ_UNLOCKED)) != 0) {
		return (ret);
	}

	is_recursive = mutex->mtx_attr.ma_type == PTHREAD_MUTEX_RECURSIVE;
	if (is_recursive) {
		sched_enter();
		lock_enter(&pshared_pid_lock);
	}

	if (cmp0_and_swap(&mutex->mtx_thread, pt_self)) {

		ASSERT(pt_self == mutex->mtx_thread);
		/*
		 * mtx_pid must be zero or we cannot guarantee that
		 * the check for a recursive relock can reliably be
		 * detected
		 */
		ASSERT(mutex->mtx_pid == 0);

		mutex->mtx_pid = PRDA_PID;
		if (is_recursive) {
			lock_leave(&pshared_pid_lock);
			sched_leave();
		}
		if (has_ceiling) {
			return (mtx_prioq_set(mutex, PRIOQ_QUEUE));
		}
		return (0);
	} else if (is_recursive && pt_self == mutex->mtx_thread) {
		/*
		 * Looks like the owner is locking the mutex
		 * again, but we need to validate that we
		 * have a consistent pid/owner
		 */
		SYNCHRONIZE();
		if (mutex->mtx_pid == PRDA_PID) {
			/* consistent pid/owner */
			lock_leave(&pshared_pid_lock);
			sched_leave();
			++mutex->mtx_lckcount;
			return (0);
		}
	}

	if (is_recursive) {
		lock_leave(&pshared_pid_lock);
		sched_leave();
	}

	if (has_ceiling) {
		mtx_prioq_reset(NULL, PRIOQ_PRIO);
	}

	return (EBUSY);
}


/*
 * pthread_mutex_lock(mutex)
 *
 * Mutex lock fast path can be used if the mutex is currently unlocked.
 * If mutex is busy then the thread will be placed on the
 * waiting list (in priority, fifo, .. order). In the case of:
 *  - mutex ceiling priority, change the priority of the owner to the
 *    ceiling priority of the mutex.
 *  - mutex inheritance priority, upgrade the priority of the owner if
 *    calling thread must wait.
 */
int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
	register pt_t	*pt_self = PT;

	if (mutex->mtx_attr.ma_pshared)
		return (mtx_lock_pshared(mutex));

	/*
	 * Attempt to acquire mutex atomically.
	 */
	if (cmp0_and_swap(&mutex->mtx_owner, pt_self)) {

		TRACE(T_MTX, ("pthread_mutex_lock(%#x) atomic", mutex));

		if (mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_NONE) {
			return (mtx_prioq_set(mutex, PRIOQ_QUEUE));
		}
		return (0);
	}

	/*
	 * Check for recursive and errorcheck mutexes.
	 */
	if (MTX_OWNER(mutex) == pt_self) {
		if (mutex->mtx_attr.ma_type == PTHREAD_MUTEX_RECURSIVE) {
			mutex->mtx_lckcount++;
			return (0);
		}
		if (mutex->mtx_attr.ma_type == PTHREAD_MUTEX_ERRORCHECK) {
			return (EDEADLK);
		}
	}

	return (mtx_slow_lock(mutex, pt_self));
}


/*
 * mtx_slow_lock(mutex, pt_self)
 *
 * Slow path function reduces Mips register saves in fast path.
 */
static int
mtx_slow_lock(pthread_mutex_t *mutex, pt_t *pt_self)
{
	register pt_t	*mutex_owner;

	TRACE(T_MTX, ("pthread_mutex_lock(%#x)", mutex));

retry_mutex_lock:
	/*
	 * We're in the slow path so check for pending signals
	 * (we're not a cancellation point so ignore those).
	 */
	PT_INTR_PENDING(pt_self, PT_INTERRUPTS & ~PT_CANCELLED);

	lock_enter(&mutex->mtx_slock);

	/* If the mutex has become free then retry from the top, otherwise
	 * try to set the busy bit before enqueuing ourselves.
	 */
	if ((mutex_owner = mutex->mtx_owner) == 0
	    || (OWNER_NOT_BUSY(mutex_owner)
	        && !cmp_and_swap(&mutex->mtx_owner,
				 mutex_owner, OWNER_ADD_BUSY(mutex_owner)))) {
		lock_leave(&pt_self->pt_lock);
		lock_leave(&mutex->mtx_slock);
		sched_leave();
		if (cmp0_and_swap(&mutex->mtx_owner, pt_self)) {

			TRACE(T_MTX, ("pthread_mutex_lock(%#x) atomic", mutex));

			if (mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_NONE) {
				return (mtx_prioq_set(mutex, PRIOQ_QUEUE));
			}
			return (0);
		}
		goto retry_mutex_lock;
	}
	STAT_INCR(STAT_MTX_SLOW);

	/*
	 * Another thread owns the mutex and the busy bit is set.
	 * Put ourself on the wait queue.
	 */
	pt_self->pt_state = PT_MWAIT;
	pt_self->pt_sync = (pt_sync_t *)mutex;
	lock_leave(&pt_self->pt_lock);

	/*
	 * Fix up queue if this mutex was statically initialized.
	 */
	if (!MUTEX_INITIALIZED(mutex)) {
		Q_INIT(&(mutex)->mtx_waitq);
	}

	/*
	 * This implements FIFO, priority-based queueing.
	 */
	pt_q_insert_tail(&mutex->mtx_waitq, pt_self);

	if (mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_INHERIT
	    && mutex->mtx_attr.ma_priority < pt_self->pt_priority) {
		mtx_giveinheritance(mutex, pt_self->pt_priority);
	} else {
		lock_leave(&mutex->mtx_slock);
	}

	TRACE(T_MTX, ("pthread_mutex_lock(%#x) sleep", mutex));

	if (sched_block(SCHED_READY) == EINTR) {
		/*
		 * We were interrupted.  Go around loop again (we check for
		 * interrupts at the top).
		 */
		ASSERT(pt_self != MTX_OWNER(mutex));
		goto retry_mutex_lock;
	}

	ASSERT(pt_self == MTX_OWNER(mutex));
	TRACE(T_MTX, ("pthread_mutex_lock(%#x) wakeup", mutex));

	return (0);
}


/*
 * mtx_lock_pshared(mutex)
 *
 * Lock an inter-process mutex.
 */
static int
mtx_lock_pshared(pthread_mutex_t *mutex)
{
	register int	ret;
	register int	is_recursive;
	register int	has_ceiling;
	register pt_t	*pt_self = PT;
	int		handoff;
	usync_arg_t	usarg;

	TRACE(T_MTX, ("mtx_lock_pshared(%#x)", mutex));

	has_ceiling = mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_PROTECT;
	if (has_ceiling && (ret = mtx_prioq_set(mutex, PRIOQ_UNLOCKED)) != 0) {
		return (ret);
	}

	is_recursive = mutex->mtx_attr.ma_type == PTHREAD_MUTEX_RECURSIVE;

	for (;;) {
		if (is_recursive) {
			sched_enter();
			lock_enter(&pshared_pid_lock);
		}

		if (cmp0_and_swap(&mutex->mtx_thread, pt_self)) {
			/*
			 * Got the mutex
			 */
			ASSERT(mutex->mtx_thread == pt_self);
			ASSERT(mutex->mtx_pid == 0);

			mutex->mtx_pid = PRDA_PID;
			if (is_recursive) {
				lock_leave(&pshared_pid_lock);
				sched_leave();
			}
			if (has_ceiling) {
				return (mtx_prioq_set(mutex, PRIOQ_QUEUE));
			}
			return (0);
		} else if (is_recursive && mutex->mtx_thread == pt_self) {
			/*
			 * Looks like the owner is locking the mutex
			 * again, but we need to validate that we
			 * have a consistent pid/owner
			 */
			SYNCHRONIZE();
			if (mutex->mtx_pid == PRDA_PID) {
				/* consistent pid/owner */
				lock_leave(&pshared_pid_lock);
				sched_leave();
				++mutex->mtx_lckcount;
				return (0);
			}
		}

		test_then_add32(&mutex->mtx_waiters, 1);
		/*
		 * Check to make sure we didn't miss an unlock
		 * before mtx_waiters was incremented
		 */
		if (cmp0_and_swap(&mutex->mtx_thread, pt_self)) {
			/*
			 * Got the mutex this time
			 */
			ASSERT(mutex->mtx_thread == pt_self);
			ASSERT(mutex->mtx_pid == 0);

			test_then_add32(&mutex->mtx_waiters, -1);
			mutex->mtx_pid = PRDA_PID;
			if (is_recursive) {
				lock_leave(&pshared_pid_lock);
				sched_leave();
			}
			if (has_ceiling) {
				return (mtx_prioq_set(mutex, PRIOQ_QUEUE));
			}
			return (0);
		}

		if (is_recursive) {
			lock_leave(&pshared_pid_lock);
			sched_leave();
		}

		/*
		 * Cannot get mutex, block until it's available
		 */
		usarg.ua_version = USYNC_VERSION_2;
		usarg.ua_addr = (__uint64_t)mutex;
		usarg.ua_policy = USYNC_POLICY_PRIORITY;
		usarg.ua_flags = 0;
		while ((handoff = pt_usync_cntl(USYNC_INTR_BLOCK,
						&usarg, FALSE)) == -1) {
			if (oserror() != EINTR) {
				panic("mtx_lock_pshared",
				      "USYNC_INTR_BLOCK failed");
			}
		}
		test_then_add32(&mutex->mtx_waiters, -1);
		if (handoff == 1) {
			/*
			 * We've been handed the mutex.  No one
			 * will unlock it, so we can just take it
			 * over.
			 */
			mutex->mtx_lckcount = 1;
			if (is_recursive) {
				sched_enter();
				lock_enter(&pshared_pid_lock);
			}
			mutex->mtx_thread = PT;
			mutex->mtx_pid = PRDA_PID;
			if (is_recursive) {
				lock_leave(&pshared_pid_lock);
				sched_leave();
			}
			if (has_ceiling) {
				return (mtx_prioq_set(mutex, PRIOQ_QUEUE));
			}
			return (0);
		}
	}
	/*NOTREACHED*/
}


/*
 * pthread_mutex_unlock(mutex)
 *
 * Mutex unlock fast path can be used if there are no waiters.
 * Otherwise we take the slower path and hand the mutex to the first waiter.
 */
int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	register pt_t	*mutex_owner;

	TRACE(T_MTX, ("pthread_mutex_unlock(%#x)", mutex));

	if (mutex->mtx_attr.ma_pshared)
		return (mtx_unlock_pshared(mutex));

	/*
	 * Check for errors.
	 */
	if (mutex->mtx_attr.ma_type == PTHREAD_MUTEX_RECURSIVE ||
	    mutex->mtx_attr.ma_type == PTHREAD_MUTEX_ERRORCHECK) {
		if (MTX_OWNER(mutex) != PT) {
			return (EPERM);
		}
	}

	ASSERT(PT == MTX_OWNER(mutex));

	/*
	 * Check for recursive mutexes.
	 */
	if (mutex->mtx_attr.ma_type == PTHREAD_MUTEX_RECURSIVE) {
		if (mutex->mtx_lckcount > 1) {
			mutex->mtx_lckcount--;
			return (0);
		}
	}

	/*
	 * Remove mutex from our inherit chain, and (possibly)
	 * reset our priority.
	 */
	if (mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_NONE) {
		ASSERT(mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_PROTECT
		       || mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_INHERIT);

		mtx_prioq_reset(mutex, PRIOQ_PRIO|PRIOQ_QUEUE);
	}

	/*
	 * Attempt fast unlock if the mutex is not busy (has waiters).
	 */
	if (OWNER_NOT_BUSY(mutex_owner = mutex->mtx_owner)
	    && cmp_and_swap(&mutex->mtx_owner, mutex_owner, 0)) {
		return (0);
	}
	return (mtx_slow_unlock(mutex));
}


/*
 * mtx_slow_unlock(mutex)
 *
 * Slow path function reduces Mips register saves in fast path.
 */
static int
mtx_slow_unlock(pthread_mutex_t *mutex)
{
	register pt_t	*pt;

	/*
	 * The mutex is busy so we take the slow path and look at the queue.
	 */
	sched_enter();
	lock_enter(&mutex->mtx_slock);

	/*
	 * If there are waiters we relinquish the mutex to the first one.
	 * Nb: if the mutex is busy then it must have been initialised.
	 */
	if (!Q_EMPTY(&mutex->mtx_waitq)) {

		/*
		 * Relinquish mutex to first waiter.
		 */
	    	pt = pt_dequeue(&mutex->mtx_waitq);

		pt->pt_wait = 0;
		pt->pt_state = PT_DISPATCH;

		/*
		 * Reset mutex ma_priority field.
		 */
		if (mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_INHERIT) {
			register pt_t	 *pt_next =
					(pt_t *)mutex->mtx_waitq.next;

			mutex->mtx_attr.ma_priority =
				(pt_next != (pt_t *)&mutex->mtx_waitq)
					? pt_next->pt_priority
					: PX_PRIO_MIN;
		}

		/*
		 * Set the new owner and reapply the busy bit if required.
		  */
		mutex->mtx_owner = !Q_EMPTY(&mutex->mtx_waitq)
					? OWNER_ADD_BUSY(pt)
					: pt;
		lock_leave(&mutex->mtx_slock);

		/*
		 * Queue this mutex to pt_minh{prev,next} -- bump
		 * pt's priority if this is a PTHREAD_PRIO_PROTECT mutex.
		 */
		if (mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_NONE) {
			mtx_inheritq(mutex, pt);
		}

		sched_dispatch(pt);

	} else {
		/*
		 * Mark mutex as unlocked.
		 */
		ASSERT(mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_INHERIT
		       || mutex->mtx_attr.ma_priority == PX_PRIO_MIN);

		mutex->mtx_owner = 0;
		lock_leave(&mutex->mtx_slock);
	}

	sched_leave();

	return (0);
}


/*
 * mtx_unlock_pshared(mutex)
 *
 * Unlock an inter-process mutex.
 */
static int
mtx_unlock_pshared(pthread_mutex_t *mutex)
{
	register int	is_recursive;
	register int	has_ceiling;

	TRACE(T_MTX, ("mtx_unlock_pshared(%#x)", mutex));

	is_recursive = mutex->mtx_attr.ma_type == PTHREAD_MUTEX_RECURSIVE;
	if (is_recursive
	    && mutex->mtx_attr.ma_type == PTHREAD_MUTEX_ERRORCHECK) {
		if (mutex->mtx_thread != PT || mutex->mtx_pid != PRDA_PID) {
			return (EPERM);
		}
	}

	ASSERT(mutex->mtx_thread == PT && mutex->mtx_pid == PRDA_PID);

	if (is_recursive && mutex->mtx_lckcount > 1) {
		--mutex->mtx_lckcount;
		return (0);
	}

	/*
	 * Free the mutex
	 */

	/*
	 * Zero'ing mtx_pid is necessary to guarantee that
	 * recursive relocks can be detected reliably in
	 * the lock_pshared and trylock_pshared functions.
	 */
	has_ceiling = mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_PROTECT;
	if (has_ceiling) {
		mtx_prioq_reset(mutex, PRIOQ_QUEUE);
	}
	mutex->mtx_pid = 0;
	SYNCHRONIZE();
	mutex->mtx_thread = 0;
	SYNCHRONIZE();

	/*
	 * Check for waiters blocked in the kernel and release one
	 */
	if (mutex->mtx_waiters) {
		usync_arg_t	usarg;

		usarg.ua_version = USYNC_VERSION_2;
		usarg.ua_addr = (__uint64_t)mutex;
		usarg.ua_flags = 0;
		while (pt_usync_cntl(USYNC_UNBLOCK, &usarg, FALSE) == -1) {
			if (oserror() != EINTR) {
				panic("mtx_unlock_pshared",
					"USYNC_UNBLOCK failed");
			}
		}
	}

	if (has_ceiling) {
		mtx_prioq_reset(NULL, PRIOQ_PRIO);
	}

	return (0);
}


pt_t *
mtx_get_owner(pthread_mutex_t *mutex)
{
	return (MTX_OWNER(mutex));
}


/*
 * Insert mutex in pt's minherit queue.
 */
static void
mtx_insert_inhq(pthread_mutex_t *mutex, register pt_t *pt)
{
	register schedpri_t	pri = mutex->mtx_attr.ma_priority;
	register mtx_t		*next, *prev;

	ASSERT(lock_held(&pt->pt_lock));

	TRACE(T_INH, ("Q: pt %#x p/n (%#x %#x)",
		pt, pt->pt_minhprev, pt->pt_minhnext));

	for (prev = pt->pt_minhprev;
	     prev != (mtx_t *)pt;
	     prev = prev->mtx_inhprev) {
		if (prev->mtx_attr.ma_priority >= pri)
			break;
	}

	/*
	 * Go backwards from end of list -- the majority of mutexes inserted
	 * here will have the minimal priority associated with them.
	 */
	mutex->mtx_inhprev = prev;
	if (prev == (mtx_t *)pt) {
		next = pt->pt_minhnext;
		pt->pt_minhnext = mutex;
	} else {
		next = prev->mtx_inhnext;
		prev->mtx_inhnext = mutex;
	}

	mutex->mtx_inhnext = next;
	if (next == (mtx_t *)pt)
		pt->pt_minhprev = mutex;
	else
		next->mtx_inhprev = mutex;

	TRACE(T_INH, ("Q: pt %#x p/n (%#x %#x)",
		pt, pt->pt_minhprev, pt->pt_minhnext));
	TRACE(T_INH, ("Q: mx %#x p/n (%#x %#x)",
		mutex, mutex->mtx_inhprev, mutex->mtx_inhnext));
}


/*
 * Manage priority changes and the inheritance queue of the caller.
 * If the PRIOQ_QUEUE flag, the mutex is queued on the caller's
 * inheritance queue.
 *
 * For PROCESS_PRIVATE mutexes, it is called only when the lock
 * is acquired without sleeping -- mtx_inheritq is
 * used to queue up if the thread had to sleep waiting for the mutex.
 *
 * PROCESS_SHARED mutexes always call this routine when they acquire
 * the mutex.  Additionally, they call it without the PRIOQ_QUEUE flag
 * to bump themselves up to the priority ceiling before getting the
 * lock to avoid priority inversion.
 *
 * Only mutexes through which pt might inherit priority need be queued.
 *
 * PTHREAD_PRIO_PROTECT mutexes grant inheritance immediately, but they
 * must be kept on the queue so we'll know pt's mutex inheritance
 * ceiling if another mutex held by this thread is released.
 *
 * PTHREAD_PRIO_INHERIT mutexes don't grant immediate inheritance, but
 * might in the future.
 */
static int
mtx_prioq_set(pthread_mutex_t *mutex, int flags)
{
	register pt_t 		*pt = PT;
	register int		prio_change = 0;

	sched_enter();
	lock_enter(&pt->pt_lock);

	ASSERT(mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_NONE);

	/*
	 * If the caller has requested queueing,
	 * add this mutex to the list of mutexes owned by pt since pt
	 * does/might inherit priority through it.
	 *
	 * Threads always inherit through PTHREAD_PRIO_PROTECT mutexes,
	 * and might inherit through PTHREAD_PRIO_INHERIT mutexes.
	 */
	ASSERT((flags & PRIOQ_QUEUE|PRIOQ_UNLOCKED)
		!= (PRIOQ_QUEUE|PRIOQ_UNLOCKED));
	if (flags & PRIOQ_QUEUE) {
		mtx_insert_inhq(mutex, pt);
	}

	/*
	 * P1003.1c/D10 13.3.3.4 Errors
	 *
	 * "The mutex was created with the protocol attribute having
	 * the value PTHREAD_PRIO_PROTECT and the calling thread's
	 * priority is higher than the mutex's current priority ceiling."
	 */
	if (mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_PROTECT) {
		if (!pt->pt_noceiling &&
		    mutex->mtx_attr.ma_priority < pt->pt_schedpriority) {
			lock_leave(&pt->pt_lock);
			sched_leave();
			if (!(flags & PRIOQ_UNLOCKED)) {
				pthread_mutex_unlock(mutex);
			}
			return (EINVAL);
		}

		if (pt->pt_priority < mutex->mtx_attr.ma_priority) {
			pt->pt_priority = mutex->mtx_attr.ma_priority;
			prio_change = 1;
		}
	}

	if (prio_change) {
		vp_setpri(pt, pt->pt_priority, SETPRI_NOPREEMPT);
		/* vp_setpri releases pt->pt_lock */
	} else {
		lock_leave(&pt->pt_lock);
	}

	sched_leave();

	return (0);
}


/*
 * Queue mutex to pt's queue.  Give pt boosted priority if
 * the mutex protocol is PTHREAD_PRIO_PROTECT.
 */
static void
mtx_inheritq(pthread_mutex_t *mutex, register pt_t *pt)
{
	ASSERT(mutex->mtx_attr.ma_protocol != PTHREAD_PRIO_NONE);

	lock_enter(&pt->pt_lock);

	/*
	 * We checked pt_schedpriority when first attempting to
	 * acquire the lock.
	 */
	if (mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_PROTECT) {
		if (pt->pt_priority < mutex->mtx_attr.ma_priority) {
			pt->pt_priority = mutex->mtx_attr.ma_priority;
		}
	}

	mtx_insert_inhq(mutex, pt);

	lock_leave(&pt->pt_lock);
}

#define DQ_INHQ(m,pt) {					\
	register mtx_t *prev = (m)->mtx_inhprev;	\
	register mtx_t *next = (m)->mtx_inhnext;	\
							\
	if (prev == (struct mtx *)(pt))			\
		(pt)->pt_minhnext = next;		\
	else						\
		prev->mtx_inhnext = next;		\
							\
	if (next == (struct mtx *)(pt))			\
		(pt)->pt_minhprev = prev;		\
	else						\
		next->mtx_inhprev = prev;		\
	(m)->mtx_inhprev = (m)->mtx_inhnext = 0;	\
}

/*
 * Remove mutex from our inherit chain, and (possibly)
 * reset our priority.
 */
static void
mtx_prioq_reset(pthread_mutex_t *mutex, int flags)
{
	register schedpri_t	inheritpri;
	register pt_t		*pt_self = PT;
	register int		prio_change = 0;

	TRACE(T_INH, ("mtx_prioq_reset(%#x, %#x)", mutex, flags));

	sched_enter();
	lock_enter(&pt_self->pt_lock);

	if (flags & PRIOQ_QUEUE) {
		TRACE(T_INH, ("pt %#x p/n (%#x %#x)",
			pt_self, pt_self->pt_minhprev, pt_self->pt_minhnext));
		TRACE(T_INH, ("mx %#x p/n (%#x %#x)",
			mutex, mutex->mtx_inhprev, mutex->mtx_inhnext));

		DQ_INHQ(mutex, pt_self);

		TRACE(T_INH, ("pt %#x p/n (%#x %#x)",
			pt_self, pt_self->pt_minhprev, pt_self->pt_minhnext));
		TRACE(T_INH, ("mx %#x p/n (%#x %#x)",
			mutex, mutex->mtx_inhprev, mutex->mtx_inhnext));
	}

	if (flags & PRIOQ_PRIO) {
		inheritpri = MTX_INHERITPRI(pt_self);

		if (inheritpri < pt_self->pt_schedpriority) {
			if (pt_self->pt_priority != pt_self->pt_schedpriority) {
				pt_self->pt_priority
				= pt_self->pt_schedpriority;
				prio_change = 1;
			}
		} else {
			if (pt_self->pt_priority != inheritpri) {
				pt_self->pt_priority = inheritpri;
				prio_change = 1;
			}
		}
	}

	if (prio_change) {
		vp_setpri(pt_self, pt_self->pt_priority, 0);
		/* vp_setpri releases pt_lock */
	} else {
		lock_leave(&pt_self->pt_lock);
	}

	sched_leave();
}


/*
 * pt_self has just been queued on front of mutex's m_waitq --
 * give mutex's owner pt_self priority if pt_self's > pt's.
 */
void
mtx_giveinheritance(struct mtx *mutex, schedpri_t pri)
{
	register pt_t	*pt_to;

	/*
	 * On entry, mutex's interlock is held and the pthread from
	 * which the priority is being willed is on mutex's mtx_waitq.
	 *
	 * The mutex interlock protects mtx_waitq/pt_queue;
	 * the owning pt's pt_lock protects pt_minherit/mtx_minh{next,prev}.
	 */
	ASSERT(VP->vp_occlude == 1);

	ASSERT(mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_INHERIT);

	pt_to = MTX_OWNER(mutex);
	for ( ; ; ) {
		if (mutex->mtx_attr.ma_priority >= pri) {
			lock_leave(&mutex->mtx_slock);
			return;
		}

		TRACE(T_INH, ("mtx_will %d via %#x to %#x", pri, mutex, pt_to));

		if (lock_tryenter(&pt_to->pt_lock)) {
			break;
		}

		intr_destroy_disable();
		lock_leave(&mutex->mtx_slock);
		lock_enter(&pt_to->pt_lock);
		lock_enter(&mutex->mtx_slock);
		intr_destroy_enable();

		if (mutex->mtx_attr.ma_priority >= pri) {
			goto bug_out;
		}

		if (MTX_OWNER(mutex) != pt_to) {
			pt_t *pt_from;

			lock_leave(&pt_to->pt_lock);

			/*
			 * "Wow, things sure have changed around here..."
			 */
			pt_to = MTX_OWNER(mutex);
			if (!pt_to) {
				lock_leave(&mutex->mtx_slock);
				return;
			}

			/*
			 * Are there any threads still waiting for this mutex?
			 * If so, try to give priority; if not, we're done.
			 */
			pt_from = (pt_t *)mutex->mtx_waitq.next;
			if (pt_from == (pt_t *)&mutex->mtx_waitq) {
				lock_leave(&mutex->mtx_slock);
				return;
			}

			pri = pt_from->pt_priority;
			continue;
		}

		break;
	}

	ASSERT(mutex->mtx_attr.ma_priority < pri);

	/*
	 * pt_to could have unlinked mutex from its inherit-queue while
	 * we were grabbing its pt_lock, and be waiting to grab mutex's
	 * s_lock from pthread_mutex_unlock right now -- be careful!
	 */
	if (!mutex->mtx_inhprev) {
		goto bug_out;
	}

	/*
	 * Mutex priority field gets set to pri, and resorted on
	 * owning pthreads pt_minh-list.
	 */
	mutex->mtx_attr.ma_priority = pri;

	if (pt_to->pt_minhnext != mutex) {
		TRACE(T_INH, ("pt %#x p/n (%#x %#x)",
			pt_to, pt_to->pt_minhprev, pt_to->pt_minhnext));
		TRACE(T_INH, ("mx %#x p/n (%#x %#x)",
			mutex, mutex->mtx_inhprev, mutex->mtx_inhnext));

		DQ_INHQ(mutex, pt_to);
		mtx_insert_inhq(mutex, pt_to);
	}

	/*
	 * If pt_to is already running at sufficient priority,
	 * there's nothing left to do.
	 */
	if (pri <= pt_to->pt_priority) {
		goto bug_out;
	}

	/*
	 * Will priority to pt_to.
	 */
	TRACE(T_INH, ("mtx_will %d to %#x", pri, pt_to));
	pt_to->pt_priority = pri;
	lock_leave(&mutex->mtx_slock);

	/*
	 * Push pri to vp, if appropriate.
	 * Also will call this routine with a new mutex/pri/pt tuplet
	 * if pt_to is sleeping on another PTHREAD_PRIO_INHERIT mutex.
	 */
	vp_setpri(pt_to, pri, 0);	/* releases pt_lock */
	ASSERT(VP->vp_occlude == 1);
	return;

bug_out:
	lock_leave(&mutex->mtx_slock);
	lock_leave(&pt_to->pt_lock);
	return;
}


#ifdef _POSIX_THREAD_PRIO_PROTECT
int
pthread_mutex_setprioceiling(pthread_mutex_t *mutex, int new, int *old)
{
	int error;

	sched_enter();
	lock_enter(&PT->pt_lock);
	PT->pt_noceiling = 1;
	lock_leave(&PT->pt_lock);
	sched_leave();

	pthread_mutex_lock(mutex);

	sched_enter();
	lock_enter(&PT->pt_lock);
	PT->pt_noceiling = 0;
	lock_leave(&PT->pt_lock);
	sched_leave();

	if (mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_PROTECT) {
		pthread_mutexattr_getprioceiling(&mutex->mtx_attr, old);
		error = pthread_mutexattr_setprioceiling(&mutex->mtx_attr, new);
	} else {
		error = EPERM;
	}

	pthread_mutex_unlock(mutex);
	return (error);
}


int
pthread_mutex_getprioceiling(const pthread_mutex_t *mutex, int *old)
{
	*old = mutex->mtx_attr.ma_priority;

	return (0);
}
#endif /* _POSIX_THREAD_PRIO_PROTECT */

/*
 * Internal version of usync_cntl to allow us a back-door to
 * the libc system call stub with superimposed blocking
 * management.
 */
int
pt_usync_cntl(int op, void *arg, int cncl)
{
	int	ret;
	int	flags;
	extern int __usync_cntl(int, void *);

	do {
		flags = libc_blocking(cncl);
		ret = __usync_cntl(op, arg);
	} while (libc_unblocking(ret == -1 && oserror() == EINTR, flags));
	return (ret);
}
