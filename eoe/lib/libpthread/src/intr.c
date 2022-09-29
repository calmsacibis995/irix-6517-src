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
 * This module implements the interrupt mechanism.  The action of interrupting
 * a pthread may require active intervention to make it run when it is
 * waiting in the library or blocked in the kernel.
 *
 * Key points:
	* The model is to make the pthread run and have it detect and
	  deal with the interruption.
	* Many interrupts can be finessed providing the pthread is running
	  and must enter the scheduler otherwise it must be forced.
	* Kernel blocked threads are unwedged using the no-op SIGPTINTR.
 */

#include "common.h"
#include "pt.h"
#include "pthreadrt.h"
#include "sys.h"
#include "vp.h"
#include "sig.h"
#include "intr.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>


/* Synchronise interrupt procesing with mutex and condition destruction.
 * The potential race is that a pthread which was waiting, wakes up
 * just as it is notified of an interruption _and_ the object it was
 * waiting on is destroyed.  The notify code will lock the wait lock
 * and recheck the state.  We need to ensure the destroy does not
 * invalidate the wait lock until the notify code is complete.
 */
unsigned long	intr_destroy_sync;

static void intr_handler(int, int, struct sigcontext *);


/* intr_bootstrap()
 */
void
intr_bootstrap(void)
{
	struct sigaction act;
	extern int	_sigaction(int, struct sigaction *, struct sigaction *);

	act.sa_handler = intr_handler;
	sigfillset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	(void)_sigaction(SIGPTINTR, &act, NULL);	/* call libc version */
}


/* intr_handler(sig, code, sc)
 *
 * Signal handler for signal sent to interrupt system calls.
 */
/* ARGSUSED */
static void
intr_handler(int sig, int code, struct sigcontext *sc)
{
	ASSERT(sig == SIGPTINTR);

	/* Our purpose is to break a pthread making a blocking system call
	 * out of the kernel.  The pthread will determine why it was broken
	 * out and will restart the call if necessary.
	 */
}


/* intr_check(flags)
 *
 * Returns TRUE if should retry, FALSE if fall through.
 */
int
intr_check(int flags)
{
	register pt_t	*pt_self = PT;
	int		retry = TRUE;
	int		old_blocked = pt_self->pt_blocked;
	int		cncl_check;

	TRACE(T_INTR, ("intr_check(%#x) flags %#x",
			flags, pt_self->pt_flags));

	cncl_check = (flags & PT_CANCELLED) || old_blocked >= PT_CNCL_THRESHOLD;

	pt_self->pt_blocked = 0;

	if ((flags & PT_SIGNALLED) && pt_self->pt_signalled) {

		/* System calls may need to be restarted except
		 * if there is a cancel we should act on.
		 */
		retry = sig_deliver_pending();
	}

	sched_enter();
	lock_enter(&pt_self->pt_lock);

	if (cncl_check && pt_self->pt_cancelled) {
		lock_leave(&pt_self->pt_lock);
		sched_leave();
		pthread_testcancel();

		/*NOTREACHED*/
	}
	pt_self->pt_blocked = old_blocked;

	lock_leave(&pt_self->pt_lock);
	sched_leave();

	return (retry);
}


/* intr_notify(pt, reason)
 *
 * Assumes pt's lock is held upon entry.
 */
void
intr_notify(register pt_t *pt, intr_reasons_t reason)
{
	pt_states_t	old_state;

	TRACE(T_INTR, ("intr_notify(%#x, %d) state %d flags %#x",
		       pt, reason, pt->pt_state, pt->pt_flags));
	ASSERT(lock_held(&pt->pt_lock));

	/* Disable destruction of wait locks while we're looking.
	 */
	intr_destroy_disable();

retry_notify:
	switch (old_state = pt->pt_state) {

	case PT_EXEC:

		/* If the pthread is not blocked or we cannot cancel it
		 * we just ensure it eventually enters the scheduler.
		 */
		if (!pt->pt_blocked
		    || reason == INTR_CANCEL
		   		 && pt->pt_blocked < PT_CNCL_THRESHOLD) {

			if (pt_is_bound(pt) || pt->pt_policy == SCHED_FIFO) {

				prctl(PR_THREAD_CTL, PR_THREAD_KILL,
					pt->pt_vp->vp_pid, SIGPTRESCHED);
				TRACE(T_INTR, ("intr_notify() intr bound"));
			}

			lock_leave(&pt->pt_lock);
			intr_destroy_enable();
			break;
		}

		intr_destroy_enable();

		/* Deliver SIGPTINTR until target unblocks.  In the
		 * (rare) case where the signal arrives before the
		 * syscall is made it will be lost so we retry until
		 * the status changes to unblocked.
		 */
		prctl(PR_THREAD_CTL, PR_THREAD_KILL,
			pt->pt_vp->vp_pid, SIGPTINTR);
		TRACE(T_INTR, ("intr_notify() intr busy %#x %d",
					pt, pt->pt_vp->vp_pid));
		VP_YIELD(pt->pt_state == PT_EXEC
			 && (reason == INTR_CANCEL && pt->pt_cancelled
			     || reason != INTR_CANCEL)
			 && PT_BLOCKED(pt)
			 && prctl(PR_THREAD_CTL, PR_THREAD_KILL,
				  pt->pt_vp->vp_pid, SIGPTINTR) == 0);

		lock_leave(&pt->pt_lock);
		break;

	case PT_MWAIT:
	case PT_RLWAIT:
	case PT_WLWAIT:
		if (reason == INTR_CANCEL && pt->pt_cncl_deferred) {
			lock_leave(&pt->pt_lock);
			intr_destroy_enable();
			break;
		}

		/* FALLTHROUGH */

	case PT_CVTWAIT:
	case PT_CVWAIT:
	case PT_JWAIT:
	case PT_SWAIT:

		/* Break out of wait.
		 */
		ASSERT(&pt->pt_sync_slock);
		lock_enter(&pt->pt_sync_slock);
		if (pt->pt_state != old_state) {

			/* Thread woke up - retry based on new state.
			 */
			lock_leave(&pt->pt_sync_slock);
			goto retry_notify;
		}

		/* Thread still waiting, wake it up so that it
		 * can do the right thing.
		 */
		Q_UNLINK(pt);
		pt->pt_wait = EINTR;		/* sleep interrupted */
		pt->pt_state = PT_DISPATCH;
		lock_leave(&pt->pt_lock);
		lock_leave(&pt->pt_sync_slock);
		sched_dispatch(pt);
		TRACE(T_INTR, ("intr_notify() did wakeup"));

		intr_destroy_enable();
		break;

	case PT_DISPATCH:
	case PT_READY:
	case PT_DEAD:

		/* Leave interrupt pending.
		 */
		lock_leave(&pt->pt_lock);

		intr_destroy_enable();
		break;

	default:
		panic("intr_notify", "unknown state");
	}
}


/* ------------------------------------------------------------------ */

/* libc_blocking()
 *
 * Blocking call prologue: cancellation, blocking, signals and VP starve.
 */
int
libc_blocking(int cnclpt)
{
	register pt_t	*pt_self = PT;
	int		flags = 0;

	TRACE( T_LIBC, ("blocking %d", cnclpt) );
	if (cnclpt) {
		pt_self->pt_blocked += PT_CNCL_THRESHOLD;
		if (pt_self->pt_flags & PT_CANCELLED) {
			(void)intr_check(PT_CANCELLED);
		}
		flags = PT_CANCELLED;	/* so we can undo it */
	}
	pt_self->pt_blocked++;

	while (pt_self->pt_flags & PT_SIGNALLED) {
		(void)intr_check(PT_SIGNALLED);
	}

	if (!pt_is_bound(pt_self)) {
		VP_RESERVE();
	}

	return (flags);
}


/* libc_unblocking(success, flags)
 *
 * Blocking call epilogue: VP starve, signals and cancellation.
 * Return indicating whether call should be restarted.
 */
int
libc_unblocking(int interrupted, int flags)
{
	register pt_t	*pt_self = PT;
	int		rv = 0;

	TRACE( T_LIBC, ("unblocking %d 0x%x", interrupted, flags) );
	if (!pt_is_bound(pt_self)) {		/* VP reserved */
		VP_RELEASE();
	}

	if (interrupted && (pt_self->pt_flags & PT_SIGNALLED)) {
		pt_self->pt_blocked--;
		rv = intr_check(PT_SIGNALLED);
	} else {
		pt_self->pt_blocked--;
	}

	if (flags & PT_CANCELLED) {
		pt_self->pt_blocked -= PT_CNCL_THRESHOLD;
		if (interrupted && (pt_self->pt_flags & PT_CANCELLED)) {
			(void)intr_check(PT_CANCELLED);
		}
	}

	return (rv);
}


/* libc_cncl_test()
 *
 * Cancellation test, used when a cancellation point does not block.
 */
int
libc_cncl_test(void)
{
	pthread_testcancel();
	return (0);
}


/* ------------------------------------------------------------------ */

