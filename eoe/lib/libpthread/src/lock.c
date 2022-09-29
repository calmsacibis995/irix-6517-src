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
 * This module implements the low level simple locks.
 *
 * Key points:
	* On an SP these locks block.
	* On an MP these locks spin.
 */

#include "common.h"
#include "asm.h"
#include "mtx.h"
#include "pt.h"
#include "vp.h"

#include <errno.h>
#include <pthread.h>
#include <sys/usync.h>
#include <mutex.h>
#include <unistd.h>
#include <stdlib.h>

extern int __usync_cntl(int, void*);

int		sched_spins = 1000; 	/* Spins waiting for an slock */

static void	lock_sp(slock_t *);
static void	lock_mp(slock_t *);

void	(*sl_lock)(slock_t *)		= lock_mp;

/* Simple locks.
 *
 * Simple locks are the most primitive locks.
 * Single processor locks (sp):
 *	The lockwd field is 0 if the lock is unlocked, otherwise it is one
 *	less than the number of waiters for the lock (i.e, if a thread holds
 *	the lock but nobody is waiting, lockwd is 1).
 *	If lockers find that the lock is already held they block
 *	immediately.  Unlockers decrement lockwd, waking up one waiter.
 *	
 * Multi-processor locks (mp):
 *	The lockwd field is 0 if the lock is unlocked, otherwise it is
 *      non-zero.  Threads spin and then block (if they are bound)
 *      or sleep (unbound) until they can acquire the lock.
 */


void
lock_bootstrap(void)
{
	char		*pt_spins = getenv("PT_SPINS");
	int		spins;
	ASSERT(sched_proc_count != 0);
	
	if (pt_spins) {
		spins = (int)strtol(pt_spins, 0, 0);
		if (spins >= 0) {
			sched_spins = spins;
		}
	}

	if (sched_proc_count == 1 || sched_spins == 0) {
		sl_lock = lock_sp;
	}
}

#define MP_LOCK		1uL

/* lock_sp(slock)
 * 
 * Acquire the lock.
 * If it is already held, block immediately.
 */ 
static void
lock_sp(slock_t *slock)
{
	ASSERT(sched_entered());

	if (test_then_add(&slock->sl_lockwd, MP_LOCK) > 0) {
		usync_arg_t usarg;
		usarg.ua_version = USYNC_VERSION_2;
		usarg.ua_addr = (__uint64_t) slock;
		usarg.ua_policy = USYNC_POLICY_PRIORITY;
		usarg.ua_flags = 0;
		if (__usync_cntl(USYNC_INTR_BLOCK, &usarg) == -1) {
			panic("lock_sp", "USYNC_INTR_BLOCK failed");
		}
	} 

	ASSERT(slock->sl_lockwd >= 1);
}

static struct timespec nano = { 0, 1000 };

/* lock_mp(slock)
 * 
 * Acquire the lock.
 * If it is already held, spin wait.
 */ 
static void
lock_mp(slock_t *slock) 
{
	register int	spins;
	int		i;
	pt_t	*pt_self = PT;
	extern int __nanosleep(const struct timespec *, struct timespec *);

	ASSERT(sched_entered());

	if (cmp0_and_swap(&slock->sl_lockwd, (void*)MP_LOCK)) {
		return;
	}

	if (pt_self && pt_is_bound(pt_self)) {

		(void)lock_sp(slock);
		return;
	}

	spins = sched_spins;

	for (;;) {

		for (i = 0; i < spins; i++) {
			if (cmp0_and_swap(&slock->sl_lockwd, (void*)MP_LOCK)) {
				return;
			}
		}

		/*
		 * Avoid a cancellation point, use libc.
		 */
		(void)__nanosleep(&nano, (struct timespec *)NULL);
	}
}


/* lock_leave(slock)
 * 
 * If anybody is waiting for the lock, wake one of them up.
 */
void
lock_leave(slock_t *slock)
{
	ASSERT(sched_entered());

	if (test_then_add(&slock->sl_lockwd, -1) > 1) {
		usync_arg_t	usarg;
		usarg.ua_version = USYNC_VERSION_2;
		usarg.ua_addr = (__uint64_t)slock;
		usarg.ua_flags = 0;
		if (__usync_cntl(USYNC_UNBLOCK, &usarg) == -1) {
			panic("lock_leave", "USYNC_UNBLOCK failed");
		}
	} 

	ASSERT((long)slock->sl_lockwd >= 0);
}


/*  lock_tryenter(slock)
 *
 * Attempt to acquire lock; report success or failure.
 * This routine works for both SP and MP machines.
 */
int
lock_tryenter(slock_t *slock)
{
	ASSERT(sched_entered());
	return (cmp0_and_swap(&slock->sl_lockwd, (void*)MP_LOCK));
}


/*  lock_held(slock)
 *
 * Returns TRUE if the lock is held, FALSE if not.
 * This routine works for both SP and MP machines.
 */
int
lock_held(slock_t *slock)
{
	return (slock->sl_lockwd > 0);
}
