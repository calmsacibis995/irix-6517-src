/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.58 $"


#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/callo.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <ksys/vproc.h>
#include <sys/kabi.h>
#include <sys/systm.h> 
#include <sys/prctl.h> 
#include <sys/ksignal.h>
#include <sys/ktime.h>
#include <sys/signal.h>
#include <sys/ksignal.h>
#include <sys/kopt.h>
#include <sys/ptimers.h>
#include <procfs/prsystm.h>
#include <string.h>
#include <limits.h>
#include <sys/xlate.h>
#include <sys/schedctl.h>
#include <sys/clksupport.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

/*
 * POSIX 1003.1b timers implementation. Also in this file are most
 * of the common functions for handling timestuc and timespecs
 */
static int nanosleep_common(struct timespec *, struct timespec *, int);
int irix5_to_sigevent(enum xlate_mode, void *, int, xlate_info_t *);
int irix5_to_itimerspec(enum xlate_mode, void *, int, xlate_info_t *);
int itimerspec_to_irix5(void *,	int, register xlate_info_t *);
void ptimer_timeout(proc_t *, int);
static void interval_tick_to_ts(__int64_t, struct timespec *, int);
static int dotimer_gettime(timer_t timerid, proc_t *p,struct itimerspec *value);
extern int fastick, fasthz;
struct nanosleepa {
        struct timespec *rqtp;
        struct timespec *rmtp;
};

int
nanosleep(struct nanosleepa *uap)
{
        struct timespec time, remtime;
        int error;
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif

        if (COPYIN_XLATE(uap->rqtp, &time, sizeof time,
						irix5_to_timespec, abi, 1))
                return EFAULT;
        if (time.tv_nsec < 0 || time.tv_nsec >= NSEC_PER_SEC)
                return EINVAL;

	error = nanosleep_common(&time, &remtime,
				 kt_has_fastpriv(curthreadp) ? SVTIMER_FAST : 0);
        if (error == EINTR) {
                if (uap->rmtp) {
                        if (XLATE_COPYOUT(&remtime, uap->rmtp, sizeof remtime,
                                        timespec_to_irix5, abi, 1))
                                return EFAULT;
                }
        }

        return error;
}

static
int
nanosleep_common(struct timespec *ts, struct timespec *rts, int svtimer_flags)
{
        uthread_t *ut = curuthread;
	kthread_t *kt = curthreadp;
	int s, error;
	
	s = ut_lock(ut);
	error = ut_timedsleepsig(ut, &kt->k_timewait, 0, s,
				 svtimer_flags, ts, rts);
	if (error == -1)
		return EINTR;
	return 0;

}


/*
 * libc usage: "sginap(ticks);"
 *	Put the current process to sleep for "ticks" ticks.  If "ticks" is
 *	zero, then just do a context switch, giving up the processor to
 *	any greater or equal priority processes.
 */

struct sginapa {
	long	ticks;
};

int
sginap(struct sginapa *uap, rval_t *rvp)
{
	timespec_t ts, rts;
	
	if (uap->ticks == 0) {
		/*
		 * Zero ticks means a voluntary reschedule
		 */
		if (curuthread->ut_gstate == GANG_UNDEF ||
			curuthread->ut_gstate == GANG_NONE) 
			user_resched(RESCHED_Y);
		return 0;
	}
	tick_to_timespec(uap->ticks, &ts, NSEC_PER_TICK);
	if (nanosleep_common(&ts, &rts, SVTIMER_TRUNC) == EINTR)
		rvp->r_val1 = timespec_to_tick(&rts, NSEC_PER_TICK);
	return 0;
}

struct timercreatea {
	sysarg_t clockid;
	sigevent_t *evp;
	timer_t *timerid;
};

int
timer_create(struct timercreatea *uap)
{
	sigevent_t event;
	timer_t timerid = 0;
	int error = 0;
	struct proc *p = curprocp;
	int s;
	ptimer_info_t	*pt = NULL;

	/*
	 * Check to see if this process has used a valid clock
	 */
	if (uap->clockid != CLOCK_REALTIME &&
	    uap->clockid != CLOCK_SGI_FAST)
		return(EINVAL);
	/*
	 * Check and see is user can use faster clock
	 */
	if (uap->clockid == CLOCK_SGI_FAST) {
		if (!capable_of_fastimer(curthreadp)) {
			return (EPERM);
		}
	}

	/*
	 * If this is this processes first call to timer_create then
	 * allocate the timer array
	 */	
	if (!p->p_ptimers) {
		if ((pt = kern_calloc(_SGI_POSIX_TIMER_MAX,
				      sizeof (struct ptimer_info))) == NULL) {
			return(EAGAIN);
		}
	}
	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);

	if (pt) {
		if (!p->p_ptimers) {
			p->p_ptimers = pt;
			add_exit_callback(current_pid(), 0,
				(void (*)(void *))delete_ptimers, 0);
		} else
			kern_free(pt);
	}
	for (timerid = 0; timerid < _SGI_POSIX_TIMER_MAX; timerid++) {
		if (p->p_ptimers[timerid].clock_type == 0)
			break;
	}
	if (timerid == _SGI_POSIX_TIMER_MAX) {
		mutex_spinunlock(&p->p_itimerlock, s);
		return (EAGAIN);
	}
	p->p_ptimers[timerid].clock_type = uap->clockid;
	mutex_spinunlock(&p->p_itimerlock, s);

	/*
	 * We have a free timer slot, copy in the sigevent information
	 */
	if (uap->evp) {
		if (COPYIN_XLATE(uap->evp, &event, sizeof event,
				 irix5_to_sigevent, get_current_abi(), 1)) {
			s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
			p->p_ptimers[timerid].clock_type = 0;
			mutex_spinunlock(&p->p_itimerlock, s);
			return (EFAULT);
		}
		if (event.sigev_notify != SIGEV_NONE
		    && event.sigev_notify != SIGEV_SIGNAL)
			return (EINVAL);
		
	} else {
		event.sigev_notify = SIGEV_SIGNAL;
		event.sigev_signo = SIGALRM;
		event.sigev_value.sival_int = timerid;
	}
	if (event.sigev_notify == SIGEV_SIGNAL) {
		if (event.sigev_signo < 1 ||
		    event.sigev_signo >= NSIG)
			return (EINVAL);
		s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
		p->p_ptimers[timerid].signo = event.sigev_signo;
		p->p_ptimers[timerid].value = event.sigev_value;
		mutex_spinunlock(&p->p_itimerlock, s);
	} else 
		p->p_ptimers[timerid].signo = 0;
	
	p->p_ptimers[timerid].next_toid = 0;
	p->p_ptimers[timerid].overrun_cnt = 0;
	p->p_ptimers[timerid].interval_tick = 0;

	/*
	 * copy the timerid out to the application
	 */
	if (copyout(&timerid, uap->timerid, sizeof(timerid)))
                error = EFAULT;
	return error;
}

struct timerdeletea {
    	sysarg_t id;
};

int
timer_delete(struct timerdeletea *uap)
{
	timer_t timerid;
	struct proc *p = curprocp;
	toid_t toid;
	int s;

	timerid = uap->id;
	/*
	 * The timer passed in to be deleted is not valid if
	 *	1) No timers have yet been allocated by the process
	 *	2) The id is out of range
	 *	3) not a currently active id
	 */
	if (!p->p_ptimers ||
	    timerid < 0 ||
	    timerid >= _SGI_POSIX_TIMER_MAX ||
	    p->p_ptimers[timerid].clock_type == 0)
		return(EINVAL);
	
	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
	/*
	 * if the timer is currently armed then we need to cancel it.
	 */
	while (toid = *(volatile toid_t *)&(p->p_ptimers[timerid].next_toid)) {
		/*
		 * Tell this timer to stop
		 * the next time it tries
		 * to rearm
		 */
		p->p_ptimers[timerid].interval_tick = 0;
		if (untimeout(toid) == 0) {
			/* Need to drop the lock before
			 * calling untimeout_wait as the
			 * timeout function also needs to
			 * acquire the lock
			 */
			mutex_spinunlock(&p->p_itimerlock, s);
			untimeout_wait(toid);
			s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
			/*
			 * if the toid did not change then we are done
			 */
			if (toid == *(volatile toid_t *)&(p->p_ptimers[timerid].next_toid))
				break;
			/* And having dropped the lock, we have to retry */
			continue;
		}
		/* Important to zero this field - if this timer is
		 * reallocated, but not set by timer_settime, then
		 * there will be a bogus toid that we may subsequently
		 * be unable to delete in delete_ptimers
		 */
		p->p_ptimers[timerid].next_toid = 0;
	}
	/*
	 * Mark it as unused
	 */
	
	p->p_ptimers[timerid].clock_type = 0;
	mutex_spinunlock(&p->p_itimerlock, s);
	return (0);

}
struct timersettimea {
    	sysarg_t timerid;
    	sysarg_t flags;
    	struct itimerspec *value;
	struct itimerspec *ovalue;
};

int
timer_settime(struct timersettimea *uap)
{
	timer_t timerid = uap->timerid;
	struct proc *p = curprocp;
	struct itimerspec value, ovalue;
	int64_t early;
	int s;
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif
#ifdef CLOCK_CTIME_IS_ABSOLUTE
	int64_t interval_tick;
#endif
	
	/*
	 * The timer passed in to be set is not valid if
	 *	1) No timers have yet been allocated by the process
	 *	2) The id is out of range
	 *	3) not a currently active id
	 */
	if (!p->p_ptimers ||
	    timerid < 0 ||
	    timerid >= _SGI_POSIX_TIMER_MAX ||
	    p->p_ptimers[timerid].clock_type == 0)
		return(EINVAL);
	if (uap->value) {
		if (COPYIN_XLATE(uap->value, &value, sizeof value,
				 irix5_to_itimerspec, abi, 1)) {
			return EFAULT;
		}
	} else {
		/* value can not be NULL */
		return (EINVAL);
	}
	if ((value.it_value.tv_nsec < 0) ||
	    (value.it_value.tv_nsec >= NSEC_PER_SEC) ||
	    (value.it_interval.tv_nsec < 0) ||
	    (value.it_interval.tv_nsec >= NSEC_PER_SEC))
		return (EINVAL);
	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);

 retry:
	/* If a timeout is set for this timer then stop it */
	if (p->p_ptimers[timerid].next_toid) {
		union	c_tid c_tid;	/* pick this name to use macro in callo.h */
		c_id = p->p_ptimers[timerid].next_toid;
		early = untimeout(c_id);

		if (early == 0) {
			/* A timein thread has already been scheduled to handle
			 * the timer expiration.  We need to wait for it
			 * to complete.
			 */
			mutex_spinunlock(&p->p_itimerlock, s);
			untimeout_wait(c_id);
			s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
			if (c_id != p->p_ptimers[timerid].next_toid)
				goto retry;
		}

		p->p_ptimers[timerid].next_toid = 0;
		if (c_fast)
			tick_to_timespec(early, &ovalue.it_value, NSEC_PER_SEC/fasthz);
		else
			tick_to_timespec(early, &ovalue.it_value, NSEC_PER_TICK);

		/*
		 * We put the old interval_tick value into the ovalue
		 */
		interval_tick_to_ts(p->p_ptimers[timerid].interval_tick,&ovalue.it_interval, p->p_ptimers[timerid].clock_type);

	} else {
		/*
		 * The timer was not armed, so return 0 for ovalue
		 */
		ovalue.it_interval.tv_sec =0;
		ovalue.it_interval.tv_nsec =0;
		ovalue.it_value.tv_sec =0;
		ovalue.it_value.tv_nsec =0;
		
	}
	/*
	 * If the it_value is 0 then we are done as they are disarming
	 * the timer
	 */
	if ((value.it_value.tv_sec == 0) && (value.it_value.tv_nsec == 0))
		goto exit;
	/* Now time to setup the new timer. */

	/*
	 * Check and see if the time wanted was an absolute time,
	 * if so we need to convert it to an interval
	 */
	 
	if (uap->flags & TIMER_ABSTIME) {
		struct timespec ts;
		switch(p->p_ptimers[timerid].clock_type){
		    case CLOCK_REALTIME:

			/* Get the current tod */
			nanotime(&ts);
			timespec_sub(&value.it_value, &ts);
			/*
			 * If the absolute time has already passed then we
			 * send the signal now
			 */
			if (value.it_value.tv_sec < 0 ||
			    (value.it_value.tv_sec == 0 &&
			     value.it_value.tv_nsec == 0 )) {
				if (p->p_ptimers[timerid].signo != 0)
					ksigqueue(PROC_TO_VPROC(p),
						  p->p_ptimers[timerid].signo,
						  SI_TIMER,
						  p->p_ptimers[timerid].value,
						  1);
				/*
				 * If we have already sent the signal
				 * and there is no interval time
				 * to set then we are done
				 */
				if (value.it_interval.tv_sec == 0 &&
				    value.it_interval.tv_nsec == 0) {
					goto exit;
				} else {
					/*
					 * First value is next interval
					 */
					value.it_value = value.it_interval;
				}
				break;
			}
		}
	}
	
	switch(p->p_ptimers[timerid].clock_type) {
	case CLOCK_REALTIME:
		{
			int64_t ticks;
			/*
			 * Check to see if the values are a multiple of the
			 * clock rate.
			 */
			ticks = value.it_value.tv_nsec % NSEC_PER_TICK;
			if (ticks != 0)
				value.it_value.tv_nsec = value.it_value.tv_nsec - ticks + NSEC_PER_TICK;
			ticks = value.it_interval.tv_nsec % NSEC_PER_TICK;
			if (ticks != 0)
				value.it_interval.tv_nsec = value.it_interval.tv_nsec - ticks + NSEC_PER_TICK;
		}
		break;
	case CLOCK_SGI_FAST:
		/* Nothing to adjust as we allow the best possible resolution */
		break;
		
	}
#ifdef CLOCK_CTIME_IS_ABSOLUTE
	/* We need to add "1" here to correct for rounding errors if non-zero */
	/* Compute interval */
	interval_tick = timespec_to_tick(&value.it_interval, NSEC_PER_CYCLE);
	if (interval_tick == 0)
		p->p_ptimers[timerid].interval_tick = 0;
	else
		p->p_ptimers[timerid].interval_tick = interval_tick + 1;

	p->p_ptimers[timerid].next_timeout =
		tstoclock(&value.it_value, TIME_ABS);
	p->p_ptimers[timerid].next_toid =
		clock_prtimeout(cpuid(),
				ptimer_timeout,
				p,
				p->p_ptimers[timerid].next_timeout,
				callout_get_pri(),
				timerid);

#else
	ASSERT(p->p_ptimers[timerid].clock_type == CLOCK_REALTIME
	       || p->p_ptimers[timerid].clock_type == CLOCK_SGI_FAST);
	switch(p->p_ptimers[timerid].clock_type) {
	    case CLOCK_REALTIME:
		    p->p_ptimers[timerid].interval_tick =
			timespec_to_tick(&value.it_interval, NSEC_PER_TICK);
		    /*
		     * We add one to the initial timeout to make sure that
		     * we do not return early in the cycle. We do not know
		     * where in the current tick we currently are.
		     */
		    p->p_ptimers[timerid].next_toid =
			prtimeout(cpuid(),
				  ptimer_timeout,
				  p,
				  timespec_to_tick(&value.it_value, NSEC_PER_TICK) + 1,
				  timerid);
		break;
	    case CLOCK_SGI_FAST:
		p->p_ptimers[timerid].interval_tick =
			timespec_to_tick(&value.it_interval, NSEC_PER_SEC/fasthz);

		/*
		 * We add one to the initial timeout to make sure that
		 * we do not return early in the cycle. We do not know
		 * where in the current tick we currently are.
		 */
		p->p_ptimers[timerid].next_toid =
			fast_prtimeout(cpuid(),
				       ptimer_timeout,
				       p,      
				       timespec_to_tick(&value.it_value, NSEC_PER_SEC/fasthz) + 1,
				       timerid);
		break;
	}
		
	
#endif

exit:
	mutex_spinunlock(&p->p_itimerlock, s);
	if (uap->ovalue) {

		/* Here we copyout the time remaining */
		if (XLATE_COPYOUT(&ovalue, uap->ovalue, sizeof ovalue,
				  itimerspec_to_irix5, abi, 1)) {
			return EFAULT;
		} 
	}	

	return (0);
}

struct timergettimea {
    	sysarg_t id;
	struct itimerspec *value;
};

int
timer_gettime(struct timergettimea *uap)
{
	struct proc *p;
	timer_t timerid;


	p = curprocp;
	timerid = (timer_t)uap->id;
	/*
	 * Check if
	 *	1) The process really has timers
	 *	2) The timer id is inside of valid timer id range
	 *	3) The timer has been created
	 */
	if (!p->p_ptimers ||
	    timerid < 0 ||
	    timerid >= _SGI_POSIX_TIMER_MAX ||
	    p->p_ptimers[timerid].clock_type == 0)
		return(EINVAL);

	return(dotimer_gettime(timerid, p,uap->value));
}
extern int fastclock;
static
int
dotimer_gettime(timer_t timerid, proc_t *p, struct itimerspec *value)
{
	struct itimerspec nvalue;
	int s;
	int64_t ticks;
	/*
	 * If the user did not give us a place to put the data then
	 * just return
	 */
	if (value == NULL)
		return(0);

	if (!p->p_ptimers ||
	    timerid < 0 ||
	    timerid >= _SGI_POSIX_TIMER_MAX ||
	    p->p_ptimers[timerid].clock_type == 0)
		return(EINVAL);

	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
	/*
	 * Get number of ticks before next timer expiration
	 */
	ticks = 0;
	if (fastclock) {
		ticks = chktimeout_tick(C_FAST,p->p_ptimers[timerid].next_toid,
					0, 0);
		tick_to_timespec(ticks, &nvalue.it_value, NSEC_PER_SEC/fasthz);
		
	}
	if (!ticks) { 
		ticks = chktimeout_tick(C_NORM, p->p_ptimers[timerid].next_toid,
					0, 0);
		tick_to_timespec(ticks, &nvalue.it_value, NSEC_PER_TICK);
	}

	interval_tick_to_ts(p->p_ptimers[timerid].interval_tick,
			    &nvalue.it_interval,
			    p->p_ptimers[timerid].clock_type);
	mutex_spinunlock(&p->p_itimerlock, s);

	/* Here we copyout the time remaining */
	if (XLATE_COPYOUT(&nvalue, value, sizeof nvalue,
			  itimerspec_to_irix5, get_current_abi(), 1)) {
		return EFAULT;
	} 

	return(0);
}

struct timergetoverruna {
    	sysarg_t	id;
};

/* ARGSUSED */
int
timer_getoverrun(struct timergetoverruna *uap, rval_t *rvp)
{
	timer_t timerid;
	struct proc *p = curprocp;
	
	timerid = uap->id;
	if (!p->p_ptimers ||
	    timerid < 0 ||
	    timerid >= _SGI_POSIX_TIMER_MAX ||
	    p->p_ptimers[timerid].clock_type == 0)
		return(EINVAL);
	
	rvp->r_val1 = p->p_ptimers[timerid].overrun_cnt;
	return (0);
}
void
delete_ptimers(void)
{
	struct proc *p = curprocp;
	ptimer_info_t *pt;
	toid_t toid;
	int i, s;
	int notdone = 0;
			
	if (p->p_ptimers) {
		
	again:
		s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
		/*
		 * Walk the list of active timers and remove any outstanding
		 * timeouts
		 */
		for (i = 0; i < _SGI_POSIX_TIMER_MAX; i++) {
			if (p->p_ptimers[i].clock_type != 0) {
				if (toid = *(volatile toid_t *)&(p->p_ptimers[i].next_toid)){
					/*
					 * Tell this timer to stop
					 * the next time it tries
					 * to rearm
					 */
					p->p_ptimers[i].interval_tick = 0;
					/* Need to drop the lock before
					 * calling untimeout_wait as the
					 * timeout function also needs to
					 * aquire the lock
					 */
					if (untimeout(toid) == 0) {
						mutex_spinunlock(&p->p_itimerlock, s);
						untimeout_wait(toid);
						s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
						/*
						 * If the value of the toid
						 * did not change then
						 * the timeout is really gone
						 */
						if (toid != *(volatile toid_t *)&(p->p_ptimers[i].next_toid))
							notdone = 1;
						else
							p->p_ptimers[i].next_toid = 0;	
					}
				}
			}
		}
		if (notdone) {
			/*
			 * We would get in here if a timeout was
			 * currently running as a thread when we
			 * called untimeout. We only do this once
			 * as it is not possible for the timer
			 * to rearm itself more then once.
			 */
			 
			static struct timespec short_delay = {0,500000};
			mutex_spinunlock(&p->p_itimerlock, s);
			nano_delay(&short_delay);
			notdone = 0;
			goto again;
		}

		pt = p->p_ptimers;
		p->p_ptimers = 0;
		mutex_spinunlock(&p->p_itimerlock, s);
		(void) kern_free(pt);
	}
}

/*
 * POSIX interval timer expired:
 * send process whose timer expired the right signal
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 *
 */
/* ARGSUSED */
void
ptimer_timeout(proc_t *p, int timerid)
{
	/*REFERENCED*/
	processorid_t timeout_cpuid;
	int s;
	int sigtest = 0;
	int sigtosend = 0;
	sigval_t valtosend;
	int	 pending;

	ASSERT(p);
again:
	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);

	/* Check if a timer is currently set for this CPU
	 * If so we need to check to see if that signal is already pending
	 * for the process. If it is then we just add one to the
	 * overrun_cnt as posix says that we can not have the same timer signal
	 * queued two times. If it is not already pending then send it.
	 */
	ASSERT(p->p_ptimers);
	if (p->p_ptimers[timerid].signo) {
		if (p->p_ptimers[timerid].next_toid){
			/*
			 * We cant call sigispend on with a spinlock held.
			 * Drop the lock, & check if the signal is pending.
			 * Then start over. We should do this just once.
			 */
			if (sigtest != p->p_ptimers[timerid].signo) {
				sigtest = p->p_ptimers[timerid].signo;
				mutex_spinunlock(&p->p_itimerlock, s);
				pending = sigispend(p, sigtest);
				goto again;
			}
			if (pending) {
				/*
				 * Posix sets a max value to the overrun count.
				 */
				if (p->p_ptimers[timerid].overrun_cnt <
				    _POSIX_DELAYTIMER_MAX) 
					p->p_ptimers[timerid].overrun_cnt++;
			} else {
				/* Avoid sending signal while holding spin lock.
				 */
				sigtosend = p->p_ptimers[timerid].signo;
				valtosend = p->p_ptimers[timerid].value;
				/*
				 * Clear the overrun as we just sent the signal
				 */
				p->p_ptimers[timerid].overrun_cnt = 0;
			}
		} else {
			/* process exits early */
			mutex_spinunlock(&p->p_itimerlock, s);
			return;
		}
	}
	/*
	 * Check to see if we need to rearm the timer.
	 * If interval_tick is not set then this timer is not
	 * repeated and we can exit
	 */
	if (p->p_ptimers[timerid].interval_tick == 0) {
		/* Clear the next_timeout and next_toid so that
		 * it is clear that this timer is not armed
		 */
		p->p_ptimers[timerid].next_timeout = 0;
		p->p_ptimers[timerid].next_toid = 0;
		p->p_ptimers[timerid].overrun_cnt = 0;
		mutex_spinunlock(&p->p_itimerlock, s);
		if (sigtosend) {
			ksigqueue(PROC_TO_VPROC(p), sigtosend, SI_TIMER,
					valtosend, 1);
		}
		return;
	}

	/*
	 * If this is a NonPreemptive CPU, and if this process isn't mustrun
	 * on this CPU, then move the timeout to interrupt some other CPU.
	 * Note: we look only at the first uthread of the process in making
	 * this determination.
	 */
	uscan_hold(&p->p_proxy);
	if (private.p_flags & PDAF_NONPREEMPTIVE  &&
	    UT_TO_KT(prxy_to_thread(&p->p_proxy))->k_mustrun == PDA_RUNANYWHERE) {
		timeout_cpuid = 0;	/* cpu0 is safe */
	} else {
		timeout_cpuid = cpuid();
	}
	uscan_rele(&p->p_proxy);
#ifdef  CLOCK_CTIME_IS_ABSOLUTE

	/* add the interval to the last time set */
	p->p_ptimers[timerid].next_timeout += p->p_ptimers[timerid].interval_tick;
	p->p_ptimers[timerid].next_toid =
		clock_prtimeout(timeout_cpuid,
				ptimer_timeout,
				p,
				p->p_ptimers[timerid].next_timeout,
				callout_get_pri(),
				timerid);
#else /* CLOCK_CTIME_IS_ABSOLUTE */
	ASSERT(p->p_ptimers[timerid].clock_type == CLOCK_REALTIME
	       || p->p_ptimers[timerid].clock_type == CLOCK_SGI_FAST);
	switch(p->p_ptimers[timerid].clock_type) {
	    case CLOCK_REALTIME:
		p->p_ptimers[timerid].next_toid =
			prtimeout(cpuid(),
				  ptimer_timeout,
				  p,
				  p->p_ptimers[timerid].interval_tick,
				  timerid);
		break;
	    case CLOCK_SGI_FAST:
		p->p_ptimers[timerid].next_toid = 
			fast_prtimeout(cpuid(),
				       ptimer_timeout,
				       p,      
				       p->p_ptimers[timerid].interval_tick,
				       timerid);
		break;
	}
	
#endif /* CLOCK_CTIME_IS_ABSOLUTE */
	mutex_spinunlock(&p->p_itimerlock, s);
	if (sigtosend) {
		ksigqueue(PROC_TO_VPROC(p), sigtosend, SI_TIMER, valtosend, 1);
	}
}

/* ARGSUSED */
static
void
interval_tick_to_ts(__int64_t ticks, struct timespec *value, int type)
{
#ifdef CLOCK_CTIME_IS_ABSOLUTE

	tick_to_timespec(ticks, value, NSEC_PER_CYCLE);

	/*
	 * For CLOCK_REALTIME we need to Round to the
	 * correct tick. CLOCK_SGI_FAST is already
	 * in the correct format
	 */

        if (type == CLOCK_REALTIME) {
                long rem = value->tv_nsec % NSEC_PER_TICK;
                if (rem)
                        value->tv_nsec -= rem;
        }
#else
	switch(type) {
	case CLOCK_REALTIME:
		tick_to_timespec(ticks, value, NSEC_PER_TICK);
		break;
	case CLOCK_SGI_FAST:
		tick_to_timespec(ticks, value, NSEC_PER_SEC/fasthz );
		break;
	}
#endif

}

/*
 * tick_to_timespec:
 * 	convert ticks to struct timespec (nanosecs)
 * 	tickval is the number of NSEC per tick
 */
void
tick_to_timespec(int64_t ticknum, timespec_t *ts, int tickval)
{
	int ticks_per_sec;
	ASSERT(tickval < NSEC_PER_SEC);
	ticks_per_sec = NSEC_PER_SEC/tickval;
	ts->tv_sec = ticknum / ticks_per_sec;
	ts->tv_nsec =  (ticknum % ticks_per_sec) * tickval;
}

/*
 * timespec_to_tick:
 *	Convert a timespec to ticks.
 * 	tickval is the number of NSEC per tick
 *
 */
__int64_t
timespec_to_tick(timespec_t *ts, int tickval)
{
	__int64_t ticks, ticks_per_sec;

	ASSERT(tickval < NSEC_PER_SEC);
	ticks_per_sec = NSEC_PER_SEC/tickval;
	ticks = (ticks_per_sec * ts->tv_sec) + (ts->tv_nsec/tickval);
	return (ticks);
	
}

/*
 * Add and subtract routines for timespec_'s.
 */
void
timespec_add(timespec_t *t1, timespec_t *t2)
{
	t1->tv_sec += t2->tv_sec;
	t1->tv_nsec += t2->tv_nsec;
	if (t1->tv_nsec >= NSEC_PER_SEC) {
		t1->tv_sec++;
		t1->tv_nsec -= NSEC_PER_SEC;
	}
}

void
timespec_sub(timespec_t *t1, timespec_t *t2)
{
	t1->tv_sec -= t2->tv_sec;
	t1->tv_nsec -= t2->tv_nsec;
	if (t1->tv_nsec < 0) {
		t1->tv_sec--;
		t1->tv_nsec += NSEC_PER_SEC;
	}
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
int
irix5_to_timespec(
        enum xlate_mode mode,
        void *to,
        int count,
        register xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_timespec, timespec);

        target->tv_sec = source->tv_sec;
        target->tv_nsec = source->tv_nsec;

        return 0;
}

/*ARGSUSED*/
int
timespec_to_irix5(
        void *from,
        int count,
        register xlate_info_t *info)
{
        register struct irix5_timespec *i5_timespec;
        register struct timespec *timespec = from;

        ASSERT(count == 1);
        ASSERT(info->smallbuf != NULL);

        if (sizeof(struct irix5_timespec) <= info->inbufsize)
                info->copybuf = info->smallbuf;
        else
                info->copybuf = kern_malloc(sizeof(struct irix5_timeval));
        info->copysize = sizeof(struct irix5_timespec);
        i5_timespec = info->copybuf;

        i5_timespec->tv_sec = timespec->tv_sec;
        i5_timespec->tv_nsec = timespec->tv_nsec;

        return 0;
}


struct irix5_sigevent {
   	app32_int_t	sigev_notify;
	app32_ptr_t	sigev_notifyinfo;
	app32_ptr_t	sigev_value;
	app32_ulong_t	sigev_reserved[13];
	app32_ulong_t	sigev_pad[6]; 
};

/*ARGSUSED*/
int
irix5_to_sigevent(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_sigevent, sigevent);

	target->sigev_notify = source->sigev_notify;
	target->sigev_signo = (int)source->sigev_notifyinfo;
	target->sigev_value.sival_ptr = (void *)(__psint_t)(int)source->sigev_value;
	return 0;
}

struct irix5_itimerspec {
	struct irix5_timespec it_interval;
	struct irix5_timespec it_value;
};
    
/*ARGSUSED*/
int
irix5_to_itimerspec(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_itimerspec, itimerspec);

	target->it_interval.tv_sec = source->it_interval.tv_sec;
	target->it_interval.tv_nsec = source->it_interval.tv_nsec;
	target->it_value.tv_sec = source->it_value.tv_sec;
	target->it_value.tv_nsec = source->it_value.tv_nsec;
	return 0;
}

/*ARGSUSED*/
int
itimerspec_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register struct irix5_itimerspec *i5_itimerspec;
	register struct itimerspec *itimerspec = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_itimerspec) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_itimerspec));
	info->copysize = sizeof(struct irix5_itimerspec);
	i5_itimerspec = info->copybuf;

	i5_itimerspec->it_interval.tv_sec = itimerspec->it_interval.tv_sec;
	i5_itimerspec->it_interval.tv_nsec = itimerspec->it_interval.tv_nsec;
	i5_itimerspec->it_value.tv_sec = itimerspec->it_value.tv_sec;
	i5_itimerspec->it_value.tv_nsec = itimerspec->it_value.tv_nsec;

	return 0;
}

#endif /* _ABI64 */
