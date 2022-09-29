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
 * This module implements the cancellation interfaces.  Most of the hard work
 * is handed off to the interrupt module.
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


/*
 * pthread_cancel(pt_id)
 *
 * Send cancellation request to the pthread specified by pt_id.
 */
int
pthread_cancel(pthread_t pt_id)
{
	register pt_t	*pt;

	if ((pt = pt_ref(pt_id)) == NULL) {
		return (ESRCH);
	}
	TRACE(T_CNCL, ("pthread_cancel(%#x)", pt));

	sched_enter();
	lock_enter(&pt->pt_lock);

	/*
	 * Ignore multiple cancel requests.
	 */
	if (pt->pt_cncl_pending) {
		lock_leave(&pt->pt_lock);
		sched_leave();

		pt_unref(pt);
		return (0);
	}

	/*
	 * Thwart multiple attempts.
	 */
	pt->pt_cncl_pending = TRUE;

	/*
	 * If cancellation is disabled we are done.
	 */
	if (!pt->pt_cncl_enabled) {
		lock_leave(&pt->pt_lock);
		sched_leave();

		pt_unref(pt);
		return (0);
	}

	/*
	 * Cancellation is pending and enabled so flag down attempts to
	 * make blocking operations (e.g. syscalls).
	 */
	pt->pt_cancelled = TRUE;

	if (pt == PT) {

		/* Act on request immediately if cancellation is not deferred.
		 */
		lock_leave(&pt->pt_lock);
		if (!pt->pt_cncl_deferred) {
			sched_leave();

			pt_unref(pt);
			pthread_testcancel();

			panic("pthread_cancel", "pt_testcancel() returned");
		}
	} else {

		/* Wake up the target if necessary.
		 */
		intr_notify(pt, INTR_CANCEL);
	}

	sched_leave();

	pt_unref(pt);
	return (0);
}


/*
 * pthread_setcancelstate(state, oldstate)
 *
 * Set enabled state of cancellation for this pthread.
 */
int
pthread_setcancelstate(int state, int *oldstate)
{
	register pt_t	*pt_self = PT;

	TRACE(T_CNCL, ("pthread_setcancelstate(%d) old %d",
		       state, pt_self->pt_cncl_enabled));

	if (oldstate) {
		*oldstate = pt_self->pt_cncl_enabled;
	}

	/*
	 * If there is no change in state we just return.
	 */
	if (state == pt_self->pt_cncl_enabled) {
		return (0);
	}

	/*
	 * If there is a pending cancellation request and we do not have
	 * cancellation deferred then we are cancelled.
	 *
	 * If we are enabling cancellation then the pending request takes
	 * effect now.  Otherwise we are disabling the cancellation too
	 * late; the request has already been sent.  In this case we race
	 * with the signal to pthread_exit() which is safe because if it
	 * wins we will never get there.
	 */
	sched_enter();
	lock_enter(&pt_self->pt_lock);
	if (pt_self->pt_cncl_pending && !pt_self->pt_cncl_deferred) {

		if (state == PTHREAD_CANCEL_ENABLE) {
			pt_self->pt_cncl_enabled = TRUE;
		}
		lock_leave(&pt_self->pt_lock);
		sched_leave();
		pthread_testcancel();

		panic("pthread_setcancelstate","pthread_testcancel() returned");
	}

	pt_self->pt_cncl_enabled = state;
	pt_self->pt_cancelled = (state == PTHREAD_CANCEL_ENABLE)
		? pt_self->pt_cncl_pending	/* may unmask pending cancel */
		: FALSE;			/* mask any pending cancel */

	lock_leave(&pt_self->pt_lock);
	sched_leave();
	return (0);
}


/*
 * pthread_setcanceltype(type, oldtype)
 *
 * Change deferred state of cancellation for this pthread.
 */
int
pthread_setcanceltype(int type, int *oldtype)
{
	register pt_t	*pt_self = PT;

	TRACE(T_CNCL, ("pthread_setcanceltype(%d) old %d",
		       type, pt_self->pt_cncl_deferred));

	if (oldtype) {
		*oldtype = pt_self->pt_cncl_deferred;
	}

	/*
	 * If there is no change in type we just return.
	 */
	if (type == pt_self->pt_cncl_deferred) {
		return (0);
	}

	/*
	 * If there is a pending cancellation request and we have
	 * cancellation enabled then we are cancelled.
	 *
	 * If we are no longer deferring cancellation then the pending
	 * request takes effect now.  Otherwise we are deferring the
	 * cancellation too late; the request has been already been sent.
	 * In this case we race with the signal to pthread_exit() which is
	 * safe because if it wins we will never get there.
	 */
	sched_enter();
	lock_enter(&pt_self->pt_lock);
	if (pt_self->pt_cncl_pending && pt_self->pt_cncl_enabled) {

		if (type == PTHREAD_CANCEL_ASYNCHRONOUS) {
			pt_self->pt_cncl_deferred = FALSE;
		}
		lock_leave(&pt_self->pt_lock);
		sched_leave();
		pthread_testcancel();

		panic("pthread_setcancelstate","pthread_testcancel() returned");
	}

	pt_self->pt_cncl_deferred = (type == PTHREAD_CANCEL_DEFERRED);

	lock_leave(&pt_self->pt_lock);
	sched_leave();
	return (0);
}


/*
 * pthread_testcancel(void)
 *
 * Check and act upon pending cancellation requests.
 */
void
pthread_testcancel(void)
{
	register pt_t	*pt_self = PT;

	TRACE(T_CNCL, ("pthread_testcancel()"));
	ASSERT(pt_self->pt_state == PT_EXEC);

	if (!pt_self->pt_cncl_enabled) {
		return;
	}

	/*
	 * If we have cancellation enabled and we have a request pending we
	 * act on it immediately.  If we are not deferring requests then we
	 * may race with an incoming signal; this is safe because if it
	 * wins we will never get there.
	 *
	 * There is no need to lock.
	 */
	if (pt_self->pt_cncl_pending) {
		sched_enter();
		lock_enter(&pt_self->pt_lock);
		pt_self->pt_cncl_enabled = PTHREAD_CANCEL_DISABLE;
		pt_self->pt_cancelled = FALSE;
		pt_self->pt_cncl_pending = FALSE;
		lock_leave(&pt_self->pt_lock);
		sched_leave();
		pthread_exit(PTHREAD_CANCELED);
	}
}


/*
 * __pthread_cancel_list(void)
 *
 * Return address of head of cancellation handler list.
 */
struct __pthread_cncl_hdlr **
__pthread_cancel_list(void)
{
	return (&PT->pt_cncl_first);
}


/*
 * libc_cncl_list(arg)
 *
 * Libc call back.
 */
void
libc_cncl_list(void *arg)
{
	*(struct __pthread_cncl_hdlr ***)arg = __pthread_cancel_list();
}
