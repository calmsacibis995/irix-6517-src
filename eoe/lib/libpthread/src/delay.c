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
 * This module implements a timeout service.  It allows a pthread context
 * to block for a period without blocking the VP.
 *
 * Key points:
	* We use the event thread for the timeouts.
	* There is a sorted list of timeouts with a sentinel at the end.
	* Timeout callbacks are run in the context of the timeout
	  thread with the id of the requesting pthread.
	* Races are avoided by requiring early wakeups to cancel
	  the timeout and wait if callback is in progess.
 */

#include "common.h"
#include "delay.h"
#include "event.h"
#include "pt.h"
#include "sys.h"
#include "vp.h"

#include <errno.h>
#include <sys/timers.h>
#include <values.h>

static int	timeout_expire(const timespec_t *, timespec_t *);
static int	timeout_calc(const timespec_t *,
			     const timespec_t *, timespec_t *);

static slock_t		timeout_lock;
static timeout_t	timeout_head;


/*
 * timeout_bootstrap()
 *
 * Also called after fork() in child.
 * XXX fork() leaks
 *	timeout_t requests
 */
void
timeout_bootstrap(void)
{
	lock_init(&timeout_lock);
	timeout_head.to_abstime.tv_sec = MAXINT;
	timeout_head.to_abstime.tv_nsec = 1000000000;
	Q_INIT(&timeout_head.to_queue);
}


/*
 * timeout_evaluate(timeto)
 *
 * Run any expired timeouts and return next timeout or 0.
 */
timespec_t *
timeout_evaluate(register timespec_t *timeto)
{
	struct timeval	ctime;
	timespec_t	stime;
	int		next;

	if (Q_EMPTY(&timeout_head.to_queue)) {
		return (0);
	}

	(void) gettimeofday(&ctime, NULL);
	timeval_to_timespec(&stime, &ctime);

	TRACE(T_DLY, ("time_remaining(%#x, %#x)", ctime.tv_sec, ctime.tv_usec));

	/* Run any expired timeouts and compute next timeout.
	 */
	sched_enter();
	lock_enter(&timeout_lock);
	next = timeout_expire(&stime, timeto);
	sched_leave();
	return (next ? timeto : 0);
}


/*
 * timeout_enqueue(new, ts_now, abstime, callback)
 *
 * Place timeout event on timeout queue.
 */
void
timeout_enqueue(
	register timeout_t	*new,
	const timespec_t	*ts_now,
	const timespec_t	*abstime,
	void (*callback)(struct pt *))
{
	register timeout_t	*tout;
	register pt_t		*pt = PT;
	register time_t		seconds;

	TRACE(T_DLY, ("timeout_enqueue(0x%p, %d.%d, %d.%d, 0x%p)",
		new, ts_now->tv_sec, ts_now->tv_nsec,
		abstime->tv_sec, abstime->tv_nsec, callback));
	ASSERT(sched_entered());

	new->to_func = callback;
	new->to_abstime = *abstime;
	new->to_pt = pt;
	new->to_state = D_PENDING;

	lock_enter(&timeout_lock);

	for (tout = Q_NEXT(&timeout_head, timeout_t*); ;
	     tout = Q_NEXT(tout, timeout_t*)) {

		seconds = abstime->tv_sec - tout->to_abstime.tv_sec;
		if (seconds < 0
		    || seconds == 0
		       && abstime->tv_nsec <= tout->to_abstime.tv_nsec) {
			break;
		}
	}

	Q_ADD_LAST(&tout->to_queue, new);

	/*
	 * Check to see if there are any expired timeouts.
	 */
	(void)timeout_expire(ts_now, (timespec_t *)NULL);

	/* Wakeup timeout thread
	 */
	evt_prompt();
}


/*
 * timeout_expire(now, time_to)
 *
 * Consumes timeout_lock.
 * Process expired timeouts by executing callbacks.
 * Return TRUE if timeouts remain.
 */
static int
timeout_expire(register const timespec_t *now,
		register timespec_t *time_to)
{
	timeout_t	*to_head;
	timeout_t	*to_expire;
	timeout_t	*to_exec;
	int		not_empty;

	TRACE(T_DLY, ("timeout_expire()"));
	ASSERT(lock_held(&timeout_lock));

	/* Remove expired timeouts
	 */
	to_expire = to_head = Q_HEAD(&timeout_head, timeout_t*);

	/* If first entry has not yet passed stop.
	 */
	if (timeout_calc(now, &to_head->to_abstime, time_to)) {
		not_empty = !Q_EMPTY(&timeout_head.to_queue);
		lock_leave(&timeout_lock);
		return (not_empty);
	}

	/* Skip to the first non-expired timeout
	 */
	do {
		to_head->to_state = D_EXEC;
		to_head = Q_NEXT(to_head, timeout_t*);
	} while (!timeout_calc(now, &to_head->to_abstime, time_to));

	/* Reset the timeout queue to start after the expired entries
	 */
	timeout_head.to_queue.next = (q_t *)to_head;
	to_head->to_queue.prev = (q_t *)&timeout_head;

	not_empty = !Q_EMPTY(&timeout_head.to_queue);

	lock_leave(&timeout_lock);

	/* Now run the expire list callbacks.
	 * The last entry in the expire list points to the head of the
	 * new timeout list so we can use it to terminate the list walk.
	 */
	ASSERT(to_expire != to_head);
	do {
		to_exec = to_expire;
		to_expire = Q_NEXT(to_exec, timeout_t*);
		(*to_exec->to_func)(to_exec->to_pt);	/* run callback */
		to_exec->to_state = D_DEAD;		/* release */

	} while (to_expire != to_head);

	return (not_empty);
}


/*
 * timeout_cancel(tout_cncl)
 *
 * Remove timeout request without execution.
 * May only be called by owner of timeout.
 */
void
timeout_cancel(timeout_t *tout_cncl)
{
	register volatile timeout_t *tout = tout_cncl;

	TRACE(T_DLY, ("timeout_cancel(%#x)", tout));
	ASSERT(tout->to_state <= D_DEAD);

	if (tout->to_state == D_DEAD) {
		return;
	}

	sched_enter();
	lock_enter(&timeout_lock);

	/* If the timeout is still pending we can remove it otherwise
	 * it may be being run so we must wait until it has finished.
	 */
	if (tout->to_state == D_PENDING) {
		Q_UNLINK(tout);
		lock_leave(&timeout_lock);
		sched_leave();
	} else {
		lock_leave(&timeout_lock);
		sched_leave();
		VP_YIELD(tout->to_state == D_EXEC);
	}
}


/*
 * timeout_needed(now, abstime)
 *
 */
int
timeout_needed(timespec_t *now, const timespec_t *abstime)
{
	struct timeval	ctime;

	(void) gettimeofday(&ctime, NULL);
	timeval_to_timespec(now, &ctime);

	return (timeout_calc(now, abstime, 0));
}


/*
 * timeout_calc(begin, end, tdiff)
 *
 * Compute timeout interval between two absolute times.
 * Return FALSE if timeout has passed or zero length, otherwise TRUE.
 */
static int
timeout_calc(	register const timespec_t *begin,
		register const timespec_t *end,
		register timespec_t *tdiff)
{
	register time_t	tv_sec;
	register long	tv_nsec;

	tv_nsec = end->tv_nsec - begin->tv_nsec;
	tv_sec = end->tv_sec - begin->tv_sec;

	if (tv_nsec < 0) {
		tv_sec--;
		tv_nsec += 1000000000;
	}

	/* test for expired timeout */
	if (tv_sec < 0) {
		return (FALSE);
	}

	if (tdiff) {
		tdiff->tv_sec = tv_sec;
		tdiff->tv_nsec = tv_nsec;
	}

	/* test for timeout of zero */
	if (tv_sec == 0) {
		return (tv_nsec != 0);
	}

	return (TRUE);
}
