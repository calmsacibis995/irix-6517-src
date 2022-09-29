/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 3.80 $"

#include "sys/param.h"
#include "sys/types.h"
#include "sys/time.h"
#include "sys/systm.h"
#include "sys/signal.h"
#include "sys/proc.h"
#include <sys/kabi.h>
#include "sys/callo.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/pda.h"
#include "sys/prctl.h"
#include "sys/schedctl.h"
#include "sys/xlate.h"
#ifdef CKPT
#include "sys/ckpt.h"
#endif
#include "os/proc/pproc_private.h"

extern int fastclock;
extern int fastick;
extern int itimer_on_clkcpu;

static int dogetitimer(int, struct itimerval *, uthread_t *);

/* 
 * Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * per-process interval timers.  Subroutines here provide support
 * for adding and subtracting timeval structures and decrement
 * decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 *
 */

/*
 * Get value of an interval timer.  The process virtual and
 * profiling virtual time timers are kept in the u. area, since
 * they can be swapped out.  These are kept internally in the
 * way they are specified externally: in time until they expire.
 *
 * The real time interval timer is kept in the process table slot
 * for the process, and its value (it_value) is kept as an
 * absolute time rather than as a delta, so that it is easy to keep
 * periodic real-time signals from drifting.
 *
 * Virtual time timers are processed in the clock() routine in clock.c
 * The real timer is processed by a timeout routine, called from timein()
 * when a softclock poke is requested.  Since a callout
 * may be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realitexpire, given below),
 * to be delayed in real time past when it is supposed to occur.  It
 * does not suffice, therefore, to reload the real timer .it_value from the
 * real time timers .it_interval.  Rather, we compute the next time in
 * absolute time the timer should go off.
 */

#if _MIPS_SIM == _ABI64
static int itimerval_to_irix5(void *, int, xlate_info_t *);
static int irix5_to_itimerval(enum xlate_mode, void *, int, xlate_info_t *);
static int irix5_to_timerval(enum xlate_mode, void *, int, xlate_info_t *);
static int timerval_to_irix5(void *, int, xlate_info_t *);
#endif

struct getitimera {
	sysarg_t which;
	void *itv;
};

int
getitimer(struct getitimera *uap)
{
	struct itimerval aitv;
	int error;
	proc_t *p = UT_TO_PROC(curuthread);

	if (uap->which >= ITIMER_MAX || uap->which < 0)
		return EINVAL;

	mraccess(&p->p_who);
	error = dogetitimer(uap->which, &aitv, curuthread);
	mraccunlock(&p->p_who);

	if (error)
		return(error);

	if (XLATE_COPYOUT(&aitv, uap->itv, sizeof aitv,
			  itimerval_to_irix5, get_current_abi(), 1))
		return EFAULT;
	return 0;
}

static int
dogetitimer(int which, struct itimerval *aitv, uthread_t *ut)
{
	int s;

	ASSERT(which >= 0 && which < ITIMER_MAX);

	if (which == ITIMER_REAL) {
		proc_t *p = UT_TO_PROC(ut);

		/*
		 * look in both callout lists for the # of ticks remained
		 * until next timer.
		 * ASSERT to make sure process does not appear in both lists
		 * if in callout table then convert callout remained tick 
		 * to normal time and return
		 */
		s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
		*aitv = p->p_realtimer;
		nested_spinunlock(&p->p_itimerlock);

		if (timerisset(&aitv->it_value)) {
			register __int64_t ticknum = 0;
			register int tickval;

			/*
			 * A process might appear on either list.
			 */
			tickval = fastick;

			if (fastclock)
				ticknum = chktimeout_tick(C_FAST,
							  p->p_itimer_toid,
							  NULL,
							  0); 
			if (!ticknum) { 
				ticknum = chktimeout_tick(C_NORM,
							  p->p_itimer_toid,
							  NULL, 0);
				tickval = USEC_PER_TICK;
			}
			if (!ticknum) {	/* not found */
				splx(s);
				return ESRCH;
			}
			tick_to_timeval((int)ticknum, &aitv->it_value, tickval);
		}
		splx(s);
	} else {
		/*
		 * These fields are only changed by the calling process
		 * or the clock interrupt, so splhi is suffieient.
		 */
		s = ut_lock(ut);
		*aitv = ut->ut_timer[which - ITIMER_UTNORMALIZE];
		ut_unlock(ut, s);
	}
	return 0;
}

struct setitimera {
	usysarg_t which;
	void *itv;
	void *oitv;
};

int
setitimer(struct setitimera *uap)
{
	struct itimerval aitv, aoitv;
	int error;
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif

	if (uap->which >= ITIMER_MAX)
		return EINVAL;

	if (COPYIN_XLATE(uap->itv, &aitv, sizeof aitv,
			 irix5_to_itimerval, abi, 1))
		return EFAULT;

	if (!uap->oitv)
		return dosetitimer(uap->which, &aitv, NULL, curuthread);

	if (error = dosetitimer(uap->which, &aitv, &aoitv, curuthread))
		return error;

	if (XLATE_COPYOUT(&aoitv, uap->oitv, sizeof aoitv,
			  itimerval_to_irix5, abi, 1)) {
		dosetitimer(uap->which, &aoitv, NULL, curuthread);
		return EFAULT;
	}

	return 0;
}

struct ut_timer {
	int	utt_which;
	struct itimerval *utt_itv;
	struct itimerval *utt_oitv;
};

int
setitimer_value(
	int which,
	struct timeval *nitv,
	struct timeval *oitv)
{
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif
	struct timeval tv, otv;
	uthread_t *ut = curuthread;
	proc_t *p = UT_TO_PROC(ut);
	int s;

	if (which != ITIMER_VIRTUAL && which != ITIMER_PROF)
		return EINVAL;

	if (COPYIN_XLATE(nitv, &tv, sizeof tv, irix5_to_timerval, abi, 1))
		return EFAULT;

	mrupdate(&p->p_who);

	s = ut_lock(ut);
	otv = ut->ut_timer[which - ITIMER_UTNORMALIZE].it_value;
	ut->ut_timer[which - ITIMER_UTNORMALIZE].it_value = tv;
	ut_unlock(ut, s);

	if (oitv && XLATE_COPYOUT(&otv, oitv, sizeof otv,
				  timerval_to_irix5, abi, 1)) {
		s = ut_lock(ut);
		ut->ut_timer[which - ITIMER_UTNORMALIZE].it_value = otv;
		ut_unlock(ut, s);
		mrunlock(&p->p_who);
		return EFAULT;
	}

	mrunlock(&p->p_who);
	return 0;
}

int
uthread_setitimer(uthread_t *ut, void *tp)
{
	struct ut_timer *utt = tp;
	int which = utt->utt_which;
	struct itimerval *itv = utt->utt_itv;
	struct itimerval *oitv = utt->utt_oitv;
	int s;

	ASSERT(which == ITIMER_VIRTUAL || which == ITIMER_PROF);

	/*
	 * These fields are only changed by the calling process
	 * or the clock interrupt, so splhi is sufficient.
	 */
	s = ut_lock(ut);
	if (oitv)
		*oitv = ut->ut_timer[which - ITIMER_UTNORMALIZE];
	ut->ut_timer[which - ITIMER_UTNORMALIZE] = *itv;
	ut_unlock(ut, s);

	return 0;
}

int
dosetitimer(
	int which,
	struct itimerval *aitv,
	struct itimerval *oitv,
	uthread_t *ut)
{
	proc_t *p;

	ASSERT(which >= ITIMER_REAL && which < ITIMER_MAX);

	if (itimerfix(&aitv->it_value) || itimerfix(&aitv->it_interval))
		return EINVAL;

	/*
	 * Allow only one thread of this process through at a time.
	 */
	p = UT_TO_PROC(ut);
	mrupdate(&p->p_who);

	if (which == ITIMER_REAL) {
		int s;
#ifdef CLOCK_CTIME_IS_ABSOLUTE
		__int64_t next_time;
#endif
		if (oitv) {
			int error;

			if (error = dogetitimer(which, oitv, ut)) {
				mrunlock(&p->p_who);
				return error;
			}
		}

		s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
		untimeout(p->p_itimer_toid);
		p->p_itimer_toid = 0;
		if (timerisset(&aitv->it_value)) {
			/*
			 * check to make sure all criteras met,
			 * before calling fast_timeout
			 */
			uscan_hold(&p->p_proxy);
			if (need_fastimer(&aitv->it_interval, p)) {
#ifdef CLOCK_CTIME_IS_ABSOLUTE
				p->p_itimer_tick = tvtoclock(&aitv->it_interval,
								TIME_REL);
#else
				/* compute interval in fast tick */
				p->p_itimer_tick = fasthzto(&aitv->it_interval);
#endif
				p->p_itimer_unit = FAST_TICK;

			} else {	
				/* compute interval in normal ticks */
				p->p_itimer_tick = hzto(&aitv->it_interval);
				p->p_itimer_unit = SLOW_TICK;
			}

			if (need_fastimer(&aitv->it_value, p)) {
#ifdef CLOCK_CTIME_IS_ABSOLUTE
				next_time = tvtoclock(&aitv->it_value,
							TIME_ABS);
				/*
				 * We set an absolute timeout putting the
				 * current time in arg1 of the callout.
				 * On a 32 bit kernel we need to split the
				 * value into 2 args as each arg is only 32
				 * bits and the counter is 64
				 */
				p->p_itimer_toid = clock_prtimeout(cpuid(), realitexpire, p, next_time, callout_get_pri());
				p->p_itimer_next = next_time;
#else
				p->p_itimer_toid = fast_timeout(realitexpire,
								p, fasthzto(&aitv->it_value));
#endif
			} else {
				p->p_itimer_toid = prtimeout((itimer_on_clkcpu) ? clock_processor : cpuid(), realitexpire, p, hzto(&aitv->it_value));
			}
			uscan_rele(&p->p_proxy);
		} else {
			/* also clear it_interval if it_value is not set */
			timerclear(&aitv->it_interval);
		}
		p->p_realtimer = *aitv;

		mutex_spinunlock(&p->p_itimerlock, s);

	} else /* which != ITIMER_REAL */ {
		struct ut_timer utt;

		utt.utt_which = which;
		utt.utt_itv = aitv;
		utt.utt_oitv = oitv;

		if (ut->ut_pproxy->prxy_shmask & PR_THREADS) {
			uthread_apply(ut->ut_pproxy, UT_ID_NULL,
				      uthread_setitimer, &utt);
		} else {
			/* fast path... */
			uthread_setitimer(ut, &utt);
		}
	}

	mrunlock(&p->p_who);

	return 0;
}

#ifdef CKPT
/*
 * Get and cancel a procs itimer.  Important that we read and clear timer
 * while holding p_itimerlock so that if timer callout fires while we
 * are in here, that no signal gets generated.
 */
int
ckptitimer(int which, struct itimerval *aitv, uthread_t *ut)
{
	int s;
	proc_t *p;

	if (which != ITIMER_REAL) {
		s = ut_lock(ut);
		*aitv = ut->ut_timer[which - ITIMER_UTNORMALIZE];
		ut_unlock(ut, s);
		return 0;
	}

	p = UT_TO_PROC(ut);
	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);

	*aitv = p->p_realtimer;

	if (timerisset(&p->p_realtimer.it_value)) {
		__int64_t ticknum = 0;
		int tickval;

		if (fastclock) {
			ticknum = chktimeout_tick(C_FAST, p->p_itimer_toid, NULL, 0);
			tickval = fastick;
		}
		if (!ticknum) {
			ticknum = chktimeout_tick(C_NORM, p->p_itimer_toid, NULL, 0);
			tickval = USEC_PER_TICK;
		}
		if (!ticknum) {
			/*
			 * Racing with expiration.  The timeout is being
			 * processed.  Tell caller to try again.
			 */
			mutex_spinunlock(&p->p_itimerlock, s);
			return EAGAIN;
		}
		tick_to_timeval((int)ticknum, &aitv->it_value, tickval);
	}
	untimeout(p->p_itimer_toid);
	p->p_itimer_toid = 0;
	timerclear(&p->p_realtimer.it_interval);
	timerclear(&p->p_realtimer.it_value);
	mutex_spinunlock(&p->p_itimerlock, s);


	return 0;
}
#endif

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 */
void
realitexpire(register struct proc *p)
{
	processorid_t timeout_cpuid;
	int s;

	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
	if (timerisset(&p->p_realtimer.it_value)) {
		sigtopid(p->p_pid, SIGALRM, SIG_ISKERN, 0, 0, 0);
	} else {
		/* process exits early */
		mutex_spinunlock(&p->p_itimerlock, s);
		return;
	}
		
	if (!timerisset(&p->p_realtimer.it_interval)) {
		timerclear(&p->p_realtimer.it_value);
		mutex_spinunlock(&p->p_itimerlock, s);
		return;
	}

	/*
	 * If this is a NonPreemptive CPU, and if this process isn't mustrun
	 * on this CPU, then move the timeout to interrupt some other CPU.
	 *
	 * XXX Which kthread is of interest?  p could have lots of them.
	 */
	uscan_hold(&p->p_proxy);
	if (private.p_flags & PDAF_NONPREEMPTIVE  &&
	    UT_TO_KT(prxy_to_thread(&p->p_proxy))->k_mustrun == PDA_RUNANYWHERE) {
		timeout_cpuid = 0;	/* cpu0 is safe */
	} else {
		timeout_cpuid = (itimer_on_clkcpu) ? clock_processor : cpuid();
	}
	uscan_rele(&p->p_proxy);
		    
	if (p->p_itimer_unit == FAST_TICK) {
#ifdef  CLOCK_CTIME_IS_ABSOLUTE
		    /* add the interval to the last time set */
		p->p_itimer_next += p->p_itimer_tick;
		p->p_itimer_toid = clock_prtimeout(cpuid(),
						   realitexpire,
						   p,
						   p->p_itimer_next,
						   callout_get_pri());
#else
		ASSERT(fastclock);
		p->p_itimer_toid = fast_prtimeout(timeout_cpuid,
						  realitexpire, p,
						  p->p_itimer_tick);
#endif
	}
	else 
		p->p_itimer_toid = prtimeout(timeout_cpuid,
					     realitexpire,
					     p,
					     p->p_itimer_tick); 
		
	mutex_spinunlock(&p->p_itimerlock, s);
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable, and
 * fix it to have at least minimal value (i.e. if it is less
 * than the resolution of the clock, round it up.)
 */
int
itimerfix(struct timeval *tv)
{
	ASSERT(curprocp);	/* must be part of a system call */

	if (tv->tv_sec < 0 || tv->tv_sec > 100000000 ||
	    tv->tv_usec < 0 || tv->tv_usec >= USEC_PER_SEC)
		return (EINVAL);
	if (tv->tv_usec != 0 && tv->tv_usec < USEC_PER_TICK) { 
		if (kt_needs_fastimer(tv, curthreadp)) {
#ifndef  CLOCK_CTIME_IS_ABSOLUTE
			if (tv->tv_usec < fastick)
				tv->tv_usec = fastick;
#endif
		} else
			tv->tv_usec = USEC_PER_TICK;
	}
	return (0);
}

/*
 * Decrement an interval timer by a specified number of microseconds,
 * which must be less than a second, i.e. < 1000000.
 * If the timer expires, then reload it.  In this case,
 * carry over (usec - old value) to reduce the value reloaded
 * into the timer so that the timer does not drift.
 * This routine assumes that it is called in a context where
 * the timers on which it is operating cannot change in value.
 */
int
itimerdecr(register struct itimerval *itp, int usec)
{
	int s;

	/*
	 * Multi-threaded processes can have one thread set
	 * another's itimer value, so we must protect itimerval
	 * with a lock when setting/changing.
	 */
	if (curuthread->ut_pproxy->prxy_shmask & PR_THREADS) {
		s = ut_lock(curuthread);
		ASSERT(s);
	} else
		s = 0;

	if (itp->it_value.tv_usec < usec) {
		if (itp->it_value.tv_sec == 0) {
			/* expired, and already in next interval */
			usec -= itp->it_value.tv_usec;
			goto expire;
		}
		itp->it_value.tv_usec += USEC_PER_SEC;
		itp->it_value.tv_sec--;
	}
	itp->it_value.tv_usec -= usec;
	usec = 0;
	if (timerisset(&itp->it_value)) {
		if (s)
			ut_unlock(curuthread, s);
		return (1);
	}

	/* expired, exactly at end of interval */
expire:
	if (timerisset(&itp->it_interval)) {
		itp->it_value = itp->it_interval;
		itp->it_value.tv_usec -= usec;
		if (itp->it_value.tv_usec < 0) {
			itp->it_value.tv_usec += USEC_PER_SEC;
			itp->it_value.tv_sec--;
		}
	} else
		itp->it_value.tv_usec = 0;		/* sec is already 0 */

	if (s)
		ut_unlock(curuthread, s);
	return (0);
}

/*
 * Add and subtract routines for timevals.
 * N.B.: subtract routine doesn't deal with
 * results which are before the beginning,
 * it just gets very confused in this case.
 * Caveat emptor.
 */
void
timevaladd(struct timeval *t1, struct timeval *t2)
{
	t1->tv_sec += t2->tv_sec;
	t1->tv_usec += t2->tv_usec;
	timevalfix(t1);
}

void
timevalsub(struct timeval *t1, struct timeval *t2)
{
	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

void
timevalfix(struct timeval *t1)
{
	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += USEC_PER_SEC;
	}
	if (t1->tv_usec >= USEC_PER_SEC) {
		t1->tv_sec++;
		t1->tv_usec -= USEC_PER_SEC;
	}
}

/*
 * convert from (sec,usec) to HZ ticks
 */
time_t
hzto(struct timeval *tv)
{
	time_t ticks, sec;

	/*
         * If number of milliseconds will fit in 32 bit arithmetic,
         * then compute number of milliseconds to time and scale to
         * ticks.  Otherwise just compute number of hz in time, rounding
         * times greater than representible to maximum value.  Times are
	 * rounded up to the next higher multiple of the available resolution.
         *
         * Maximum value for any timeout in 10ms ticks is < 250 days.
         */

	sec = tv->tv_sec;
	if (sec < (0x7fffffff/1000 - 1000)) {
		ticks = (sec*1000 + (tv->tv_usec+USEC_PER_TICK-1)/1000)
			/(USEC_PER_TICK/1000);
	} else if (sec < 0x7fffffff/HZ) {
		ticks = sec*HZ + (tv->tv_usec+USEC_PER_TICK-1)/USEC_PER_TICK;
	} else {
		ticks = 0x7fffffff;
	}

	return (ticks);
}

/*
 * convert a timeval to a number of "fast ticks"
 */
time_t
fasthzto(struct timeval *tv)
{
	time_t ticks;
	extern int fasthz;

	if (tv->tv_sec < 0x7fffffff/fasthz) 
		ticks = tv->tv_sec*fasthz 
			+ (tv->tv_usec + (USEC_PER_SEC/fasthz)-1)/fastick;
	else
		ticks = 0x7fffffff;
	return(ticks);
}

/*
 * tick_to_timeval()
 * tickval is assumed to be in USEC.
 * max value in an integer is 0x7fffffff
 * ticknum could be at most 0x7fffffff
 */
void
tick_to_timeval(int ticknum, struct timeval *tv, int tickval)
{
	int ticks_per_sec;

	ASSERT(tickval < USEC_PER_SEC);
	ticks_per_sec = USEC_PER_SEC/tickval;
	tv->tv_sec = ticknum / ticks_per_sec;
	tv->tv_usec = (ticknum % ticks_per_sec)*tickval;
}

#if _MIPS_SIM == _ABI64
/* ARGSUSED */
static int itimerval_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register struct irix5_itimerval *i5_itv;
	register struct itimerval *itv = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_itimerval) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_itimerval));
	info->copysize = sizeof(struct irix5_itimerval);

	i5_itv = info->copybuf;

	timeval_to_irix5(&itv->it_interval, &i5_itv->it_interval);
	timeval_to_irix5(&itv->it_value, &i5_itv->it_value);

	return 0;
}

/* ARGSUSED */
static int
irix5_to_itimerval(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_itimerval, itimerval);

	irix5_to_timeval(&target->it_interval, &source->it_interval);
	irix5_to_timeval(&target->it_value, &source->it_value);

	return 0;
}

/* ARGSUSED */
static int
irix5_to_timerval(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_timeval, timeval);

	irix5_to_timeval(target, source);

	return 0;
}

/* ARGSUSED */
static int timerval_to_irix5(
	void *from,
	int count,
	xlate_info_t *info)
{
	struct irix5_timeval *i5_tv;
	struct timeval *itv = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_timeval) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_timeval));

	info->copysize = sizeof(struct irix5_timeval);

	i5_tv = info->copybuf;

	timeval_to_irix5(itv, i5_tv);

	return 0;
}
#endif	/* _ABI64 */
