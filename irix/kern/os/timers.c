/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/timers.h>
#include <sys/kthread.h>

/*
 *	Code to maintain and read high resolution kernel timers.
 */


/*
 * Snapshot current time.
 */
static __inline void
ktimer_snap(ktimersnap_t *snap)
{
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
	*snap = GET_LOCAL_RTC;
#else
	/*
	 * For platforms with high-resolution timers which wrap in a fairly
	 * short amount of time, snapshot the current value of "time" as
	 * well as the high-resolution timer so we can tell when it has
	 * wrapped and just use the lower resolution value instead.  See
	 * ktimer_delta() below.
	 */
	snap->secs = time;
	snap->rtc = get_timestamp();
#endif
}


/*
 * Return TRUE if to time snapshots are equal.
 */
static __inline int
ktimer_equal(const ktimersnap_t *t0, const ktimersnap_t *tN)
{
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
	return *t0 == *tN;
#else
	return t0->secs == tN->secs && t0->rtc == tN->rtc;
#endif
}


/*
 * Return the amount of time in RTC ticks between time t0 and time tN.
 */
static __inline uint64_t
ktimer_delta(const ktimersnap_t *t0, const ktimersnap_t *tN)
{
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
	return *tN - *t0;
#else
	/*
	 * Both times are represented as [time, RTC] snapshots.  If the
	 * high-resolution timer would have wrapped between those two times,
	 * just use the scaled seconds portion.
	 *
	 * XXX Should we try to use the RTC snapshots to more accurately
	 * XXX estimate the time?  We'd need to know how many bits of
	 * XXX precision the RTC *really* had to be able to do this ...
	 */
	time_t secs = tN->secs - t0->secs;
	if (secs < timer_maxsec)
		return tN->rtc - t0->rtc;
	else {
		#pragma mips_frequency_hint NEVER
		return (uint64_t)secs * timer_freq;
	}
#endif
}


/*
 * Convert a 64-bit high-resolution counter into a timerspec_t.
 */
static __inline void
rtc_to_ts(uint64_t t, timespec_t *ts)
{
	time_t sec;
	long nsec;

	sec = t / timer_freq;
	nsec = (t % timer_freq) * timer_unit;

	ts->tv_sec = sec;
	ts->tv_nsec = nsec;
}


/*
 * Start a ktimer up in timer mode "timer".  Assumes that the ktimerpkg_t
 * has already been initialized to zeros for new processes or to snapshots
 * of some previous ktimerpkg_t for migrating and restarting threads, etc.
 * Thread should not be runable or able to be switched out if the current
 * thread.
 */
void
ktimer_init(kthread_t *kt, int timer)
{
	ktimerpkg_t *kp;

	ASSERT(kt);
	ASSERT(issplhi(getsr()) || issplprof(getsr()) || kt != curthreadp);
	ASSERT(timer < MAX_PROCTIMER);

	kp = &kt->k_timers;
	kp->kp_curtimer = timer;
	ktimer_snap(&kp->kp_snap);
}


/*
 * Switch from current timer to a new timer.  We snapshot the current time,
 * add the delta between the previous timer switch and now to the
 * accumulator for the previous mode, and save the current time as new timer
 * switch time.  Interupts should be disabled or the indicated thread should
 * not be the current thread in order to prevent timer corruption.  Also note
 * that if we're changing the thread's timer to some non-RUN timer, then the
 * thread's lock had better be held or the thread may be made runable before
 * the call to ktimer_switch().  See thread_block() as a canonical example of
 * this.
 */
void
ktimer_switch(kthread_t *kt, int newtimer)
{
	ktimerpkg_t *kp;
	ktimersnap_t snap;
	int curtimer;

	ASSERT(issplprof(getsr()) || kt != curthreadp);
	ASSERT(newtimer < MAX_PROCTIMER);

	ASSERT(kt);
	kp = &kt->k_timers;

	curtimer = kp->kp_curtimer;
	ASSERT(curtimer < MAX_PROCTIMER);

	ktimer_snap(&snap);
#if defined(CELL_IRIX)
	__add_and_fetch((uint64_t *)&kp->kp_timer[curtimer],
			ktimer_delta(&kp->kp_snap, &snap));
#else
	kp->kp_timer[curtimer] += ktimer_delta(&kp->kp_snap, &snap);
#endif
	kp->kp_snap = snap;
	kp->kp_curtimer = newtimer;
}


/*
 * Return kp->timer[timer] as the number of accumulated RTC ticks.  If the
 * timer is currently running, we have to add the time since it was started.
 * If the indicated kthread's timer is switched while we're reading it, then
 * we need to loop till we get a clear picture of the timer.
 */
static __inline uint64_t
_ktimer_readticks(const kthread_t *kt, int timer)
{
	const volatile ktimerpkg_t *kp;
	ktimersnap_t snap1, snap2, now;
	uint64_t accum;

	ASSERT(kt);
	ASSERT(timer < MAX_PROCTIMER);

	kp = &kt->k_timers;
	do {
		snap1 = kp->kp_snap;
		accum = kp->kp_timer[timer];
		if (kp->kp_curtimer != timer) {
			/*
			 * We're either reading someone else' timer or our
			 * own, non-SYS timer.  In the first case, if we race
			 * with a ktimer_switch() it doesn't matter since the
			 * entire process of reading another thread's timers
			 * is racey.  In the second case, the only timer that
			 * could potentially get changed as we're trying to
			 * read it is the interrupt timer and that's just as
			 * racey as reading another thread's timers.  So just
			 * return the current accumulated ticks for the
			 * requested timer.
			 */
			return accum;
		}
		ktimer_snap(&now);
		snap2 = kp->kp_snap;
	} while (!ktimer_equal(&snap1, &snap2));
	accum += ktimer_delta(&snap1, &now);
	return accum;
}

uint64_t
ktimer_readticks(const kthread_t *kt, int timer)
{
	return _ktimer_readticks(kt, timer);
}

/*
 * Return kp->timer[timer] as a timespec_t.
 */
void
ktimer_read(const kthread_t *kt, int timer, timespec_t *ts)
{
	rtc_to_ts(_ktimer_readticks(kt, timer), ts);
}


/*
 * Return a measure of the amount of time (if any) a thread has been
 * sleeping/waiting.  The only guarantee offered about this measure is that
 * it is proportional to the passage of time.  The units are not defined.
 * This is mostly useful for sorting threads based on how long they've been
 * asleep.  The return value is intentionally defined to be ``long'' so that
 * it will occupy four bytes on low-end platforms and eight bytes on high-end
 * platforms.
 */
long
ktimer_slptime(const kthread_t *kt)
{
	const ktimerpkg_t *kp;

	ASSERT(kt);

	kp = &kt->k_timers;
	ASSERT(kp->kp_curtimer < MAX_PROCTIMER);

	switch (kp->kp_curtimer) {
	    default: {
		long dt;

#if defined(CLOCK_CTIME_IS_ABSOLUTE)
		dt = GET_LOCAL_RTC - kp->kp_snap;
#else
		dt = time - kp->kp_snap.secs;
#endif
		/* clamp the return value to >= 0 */
		return dt < 0 ? 0 : dt;
	    }
	    case AS_USR_RUN:
	    case AS_SYS_RUN:
	    case AS_INT_RUN:
		return 0;
	}
}


#if defined(CELL_IRIX)
/*
 * Add ticks to the specified timer and return the result.
 */
uint64_t
ktimer_accum(kthread_t *kt, int timer, uint64_t ticks)
{
	ktimerpkg_t *kp;

	ASSERT(kt);
	ASSERT(timer < MAX_PROCTIMER);

	kp = &kt->k_timers;
	return __add_and_fetch((uint64_t *)&kp->kp_timer[timer], ticks);
}
#endif /* CELL_IRIX */
