/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Counter/Timer functions
 */
#ident "$Revision: 1.151 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/callo.h"
#include "sys/cmn_err.h"
#include "sys/cpu.h"
#include "sys/systm.h"
#include "sys/kmem.h"
#include "sys/time.h"
#include "sys/sbd.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include <sys/prf.h>
#include "sys/proc.h"
#include "sys/errno.h"
#include "sys/strsubr.h"
#include "sys/debug.h"
#include "sys/capability.h"
#include <sys/rtmon.h>
#include <sys/runq.h>
#include "sys/schedctl.h"
#include <sys/calloinfo.h>
#include <sys/prf.h>
#include <sys/ddi.h>
#include <sys/atomic_ops.h>

#ifdef FIX_LOSTINTERRUPT
#define MAXTRIM_USEC (USEC_PER_SEC/HZ/10*3) /* max trim in usec/sec */
#define MAXTRIM_NSEC (MAXTRIM_USEC*1000)
#endif
#define MAXTRIM_NSEC maxtrim_nsec
int maxtrim_nsec = 40000000;	/* 40msec/sec  4% trim adjustment */
int fastick;			/* computed in startkgclock() once */

/*
 * High resolution time of day: "hrestime".  Updators of hrestime must
 * set new_tstamp to the current RTC, update time, and then copy new_tstamp
 * to tstamp.  Accessors must snapshot tstamp, then grab time, and finally
 * repeat the process if the snapshotted tstamp does not equal new_tstamp.
 * This allows accessors to grab the current time reliably without having
 * to take a lock that would become contended -- both in time and in cache.
 *
 * Note that because this algorithm is only ever used on a single CPU
 * machine we do *not* need to use __synchronize().  If this algorithm *were*
 * being used on an multi-CPU machine, then the protocol would have to be
 * expanded as such: 
 *
 *     In order to make this work on current and future hardware, readers
 *     must use a __synchronize() after reading tstamp and before reading
 *     new_tstamp.  Writers must use a __synchronize() after writing
 *     new_tstamp and before writing tstamp.
 */
static volatile struct {
	timespec_t time;		/* time in sec, nanosec since 1970 */
	timespec_t lasttime;		/* last time returned by */
					/*   nanotime_syscall() or set via */
					/*   settime() */
	uint32_t tstamp, new_tstamp;	/* updator/accessor interlock and */
					/*   means to get delta since last */
					/*   call to nanotime_syscall() */
} hrestime;

int todcinitted;		/* TOD clock chip has been set */
int 	fastclock;		/* set by timestamp_init() on boot-up or
				enable_fastclock() */

lock_t	timelock;			/* lock on time-of-day variables */
extern callout_info_t *fastcatodo;	/* fast callout list */
extern callout_info_t *calltodo;	/* callout list */

void timepoke(void);			/* sets up C_NORM callout handling */
void fast_timepoke(void);		/* sets up C_FAST callout handling */

extern void timein_entry_ithrd(callout_info_t *, struct callout *, int);

extern char qrunflag;			/* need to run streams queues */

static int timedeltas = 1;	/* adjust time on this many normal ticks */
static long tickdelta = 0;	/* adjust nsec/hz by this */
static int next_timedeltas = 0;	/* use these starting next tick */
static long next_tickdelta = 0;
extern int timetrim;		/* trim clock by this many nsec/sec */
static long sectimetrim;	/* remainder nsec/sec */
static int nsectick = NSEC_PER_TICK;

#ifdef DEBUG
unsigned short lastpval = 0;
int resetpdiff = 1;
#endif

#ifdef POWER_DUMP
extern int power_button_changed_to_crash_dump;	/* mtune/kernel */
extern void power_dump(int);
#endif

/* for timestamping */
unsigned timer_freq;		/* freq of timestamping source */
unsigned timer_unit;		/* duration in nsec of timer tick */
unsigned timer_high_unit;	/* in sec XXX */
unsigned timer_maxsec;		/* duration in sec before wrapping */

extern void startrtclock_r4000();
extern void stopclocks_r4000();
#ifdef DEBUG
extern int getprofclock_r4000();
#endif
extern void startkgclock_r4000();
extern void slowkgclock_r4000();
extern void r4kcount_intr_r4000();
extern void ackkgclock_r4000();
extern void ackrtclock_r4000();

#ifndef IPMHSIM
extern void startrtclock_8254();
extern void stopclocks_8254();
#ifdef DEBUG
extern int getprofclock_8254();
#endif
extern void startkgclock_8254();
extern void slowkgclock_8254();
extern void r4kcount_intr_8254();
extern void ackkgclock_8254();
extern void ackrtclock_8254();
#endif /* IPMHSIM */

#if IP22 || IP32 || IPMHSIM
static void (*__startrtclock)(void);
static void (*__stopclocks)(void);
#ifdef DEBUG
int (*__getprofclock)(void);
#endif
static void (*__startkgclock)(void);
static void (*__slowkgclock)(void);
static void (*__r4kcount_intr)(void);
static void (*__ackkgclock)(void);
static void (*__ackrtclock)(void);

static int first_timer = 1;

static void
init_timer (void)
{
	if (!first_timer) {
		return;
	}
#if !defined(IP32) && !defined(IPMHSIM)
	if (is_ioc1 ()) {	/* The 8254 timer is broken on ioc1 */
		__startrtclock = startrtclock_r4000;
		__stopclocks = stopclocks_r4000;
#ifdef DEBUG
		__getprofclock = getprofclock_r4000;
#endif
		__startkgclock = startkgclock_r4000;
		__slowkgclock = slowkgclock_r4000;
		__r4kcount_intr = r4kcount_intr_r4000;
		__ackkgclock = ackkgclock_r4000;
		__ackrtclock = ackrtclock_r4000;
	} else {
		__startrtclock = startrtclock_8254;
		__stopclocks = stopclocks_8254;
#ifdef DEBUG
		__getprofclock = getprofclock_8254;
#endif
		__startkgclock = startkgclock_8254;
		__slowkgclock = slowkgclock_8254;
		__r4kcount_intr = r4kcount_intr_8254;
		__ackkgclock = ackkgclock_8254;
		__ackrtclock = ackrtclock_8254;
	}
		
	first_timer = 0;
#else
	if (!first_timer) {
		return;
	}
	__startrtclock = startrtclock_r4000;
	__stopclocks = stopclocks_r4000;
#ifdef DEBUG
	__getprofclock = getprofclock_r4000;
#endif
	__startkgclock = startkgclock_r4000;
	__slowkgclock = slowkgclock_r4000;
	__r4kcount_intr = r4kcount_intr_r4000;
	__ackkgclock = ackkgclock_r4000;
	__ackrtclock = ackrtclock_r4000;
	first_timer = 0;
#endif /* !IP32 && !IPMHSIM */
}

void
startrtclock(void)
{
	if (first_timer) {
		init_timer ();
	}

	(*__startrtclock) ();
}

void
stopclocks(void)
{
	if (first_timer)
		init_timer();
	(*__stopclocks) ();
}

#ifdef DEBUG
int
getprofclock(void)
{
	if (first_timer)
		init_timer();
	return ((*__getprofclock) ());
}
#endif /* DEBUG */

void
startkgclock()
{
	if (first_timer)
		init_timer();
	(*__startkgclock) ();
}

void
slowkgclock(void)
{
	if (first_timer)
		init_timer();
	(*__slowkgclock) ();
}

/*ARGSUSED*/
void
r4kcount_intr(struct eframe_s *ep)
{
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_CPUCOUNTER_INTR,
			 NULL, NULL, NULL, NULL);
	if (first_timer)
		init_timer();
#ifdef POWER_DUMP
	if (power_button_changed_to_crash_dump) {
		if (++power_button_changed_to_crash_dump > 200) {
		    power_dump(1);
		    power_button_changed_to_crash_dump = 1;
		}
	}
#endif
	(*__r4kcount_intr) ();
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT,
			 TSTAMP_EV_CPUCOUNTER_INTR, NULL, NULL, NULL);
}

void
ackkgclock(void)
{
	if (first_timer)
		init_timer();
	(*__ackkgclock) ();
}

void
ackrtclock(void)
{
	if (first_timer)
		init_timer();
	(*__ackrtclock) ();
}
#endif	/* IP22 || IP32 || IPMHSIM */

/*
 * Machine-dependent maintenance to perform on every clock tick.
 * Called only on the clock processor, once per clock tick.
 *
 * For MPBUS machines, we need to do all sorts of things
 * to maintain an accurate, forward-moving clock from which
 * unique times can be obtained.
 */
void
tick_maint(void)
{
	extern int one_sec;

#ifdef CDEBUG
	/* every 100mS check for clock tick loss */
	if ((lbolt % 10) == 0) {
		extern int resetpdiff;
		int i;
		if ((i = getprofclock()) > 110) {
			cmn_err(CE_CONT, "Lost time:%dmS/1 sec\n",
							i-100);
			resetpdiff = 1;
		} else if (i < 90 && i > 0) {
			cmn_err(CE_CONT, "Gained time:%dmS/1 sec\n",
							100-i);
			resetpdiff = 1;
		}
	}
#endif

	/* indicate that time is being modified */
	hrestime.new_tstamp = get_timestamp();

	hrestime.time.tv_nsec += nsectick;
	if (hrestime.time.tv_nsec >= NSEC_PER_SEC) {
        	hrestime.time.tv_sec++;
        	hrestime.time.tv_nsec -= NSEC_PER_SEC;
		one_sec = 1;
		time = hrestime.time.tv_sec;	/* advance second counter */
	}

	/*
	 * Stop correcting time when done.
	 *
	 * This big chunk of code is executed only once per
	 * adjtime() system call, and so its presense here
	 * is not as bad as it might seem.
	 */
	if (timedeltas != 0 && --timedeltas == 0) {
		/* check timedeltas before using tickdeltas */
		tickdelta = next_tickdelta;
		timedeltas = next_timedeltas;
		next_timedeltas = 0;
		next_tickdelta = 0;

		/* clamp in case value came from master.d/kernel */
		if (timetrim < -MAXTRIM_NSEC)
			timetrim = -MAXTRIM_NSEC;
		else if (timetrim > MAXTRIM_NSEC)
			timetrim = MAXTRIM_NSEC;
		nsectick = timetrim / HZ;
		sectimetrim = timetrim % HZ;
		if (sectimetrim < 0) {
			/* keep time going forwards */
			sectimetrim += HZ;
			nsectick--;
		}

		nsectick += NSEC_PER_TICK + tickdelta;
	}

	/* this indicates end of updating hrestime */
	hrestime.tstamp = hrestime.new_tstamp;
}

/*
 * Machine-dependent maintenance to perform once per second.
 * Called only on the clock processor, once per second.
 */
void
onesec_maint(void)
{
#if IP20 || IP22 || IP26 || IP28 || IP32 || IPMHSIM
	/*
	 * Machines that use the internal C0_COUNT register
	 * must update_ust every once-in-a-while to keep 32-bit
	 * counter from overflowing. 
	 */
	update_ust_i();
#endif

	tlb_onesec_actions();

	/* See if our time is drifting every 32 seconds
	 * after the TOD chip has been 'initialized'.
	 * Do not correct with the time from the TOD chip if
	 * adjtime() sys call has been run within the last
	 * hour.  
	 *
	 * If adjtime() has been used recently, then set
	 * the TOD chip, but not too often.
	 */
	if ((time%32) == 0 && todcinitted) {
		if (lbolt > lastadjtime) {
			chktimedrift();
		} else if (lastadjtime > lastadjtod) {
			lastadjtod = lbolt+2*DIDADJTIME;
			wtodc();
		}
	}

	/*
	 * left over due to int. arithmetic from doing timetrim % HZ
	 * is trimmed here.
	 * XXX How come we aren't writing new_tstamp, etc. to correct
	 * XXX accessors???
	 */
	hrestime.time.tv_nsec += sectimetrim;
}

/*
 * Machine-dependent maintenance to perform on every fast tick.
 * Called only on the clock processor, once per fast tick.
 * 
 * On MPBUS systems, the main activity is decrementing the
 * remaining fast tick count of the timeout entry at the head
 * of the timeout queue.  We then generate a timeout interrupt
 * if needed (or directly handle the head of queue if possible).
 *
 * If the profiler wants a kick too, then give it to him.
 */
# ifdef DEBUG
long	ftick_num = 0;
long	ftick_found = 0;
long	ftick_expire = 0;
# endif

void fast_user_prof(eframe_t *);

void (*kdsp_timercallback_ptr)(void) = 0;
void (*isdnaudio_timercallback_ptr)(void) = 0;
void (*midi_timercallback_ptr)(void) = 0;
void (*tsio_timercallback_ptr)(void) = 0;
void (*ieee1394_timercallback_ptr)(void) = 0;

/* called only on the fastclock processor */
void
fastick_maint(eframe_t *ep)
{
	register struct callout *p1, *p2;
	register caddr_t arg;

	ASSERT(issplprof(ep->ef_sr));
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_PROFCOUNTER_INTR, NULL, NULL,
			 NULL, NULL);
	ackkgclock();
# ifdef DEBUG
	ftick_num++;
# endif
        /*
        ** XXX It is wasteful to check each pointer individually.
        ** The check for p_prf_enabled, kdsp, midi, and isdnvoice could
        ** be avoided: we could easily pre-compute the exact set of 
        ** function calls that should be made in this routine, so
        ** that no checking of any kind needed to be done here
        ** for those services.  The set of functions to call
        ** would only have to be recomputed when a given service 
        ** wanted to CHANGE its pointer, which happens much more 
        ** infrequently than the fastick interrupt itself.
        **
        ** This was not done in the banyan release in the interests
        ** of producing a stable and simple checkin with minimal 
        ** code impact.
        */
	if (kdsp_timercallback_ptr) {
            kdsp_timercallback_ptr();
	}
	if (isdnaudio_timercallback_ptr) {
	    isdnaudio_timercallback_ptr();
	}
	if (midi_timercallback_ptr) {
	    midi_timercallback_ptr();
	}
	if (tsio_timercallback_ptr) {
	    tsio_timercallback_ptr();
	}
	if (ieee1394_timercallback_ptr) {
	    ieee1394_timercallback_ptr();
	}
	if (CI_TODO_NEXT(fastcatodo) && (private.p_flags & PDAF_FASTCLOCK)) {
		/* fast itimer processing */
		nested_spinlock(&CI_LISTLOCK(fastcatodo));
		p1 = CI_TODO_NEXT(fastcatodo);
		if (p1 != 0 && --p1->c_time <= 0) {
# ifdef DEBUG
			ftick_found++;
# endif
			/* 
			 * Don't want to do a timepoke to service itimer
			 * so make sure that we are either: 
			 * 	from user mode or
			 * 	in the kernel but at low spl
			 *
			 * Add a condition where if the next entry is
			 * also expired then we have to go thru timepoke
			 * so that they all get serviced at the same time.
			 */
			nested_spinunlock(&CI_LISTLOCK(fastcatodo));
			/* setup soft intr to handle timeout */
			fast_timepoke();
		} else
			nested_spinunlock(&CI_LISTLOCK(fastcatodo));
	}

	fast_user_prof(ep);

	/* if profiling, give prfintr a ring */
	if (private.p_prf_enabled)
		prfintr(ep->ef_epc, ep->ef_sp, ep->ef_ra, ep->ef_sr);
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_PROFCOUNTER_INTR, NULL,
			 NULL, NULL);
}


/*
 * Machine-dependent per-tick activities, called once every
 * clock tick on *every* processor.
 *
 * On MPBUS systems, adjust the callout queue as needed.  
 */
void
tick_actions(void)
{
	register struct callout *p1;
	int s;
	callout_info_t *callout_info = &CALLTODO(cpuid());
	/*
	 * Perform any outstanding actions.
	 * XXX This shouldn't be necessary, but we are apparently dropping 
	 * XXX inter-cpu interrupts.
	 */
#ifdef MP
	if (private.p_actionlist.todolist != NULL)
		doactions();
#endif /* MP */

	/*
	 * Decrement tick count at head of todo list and
	 * then determine if a timpoke is needed.
	 */
	s = mutex_spinlock_spl(&CI_LISTLOCK(callout_info), splprof);
	p1 = CI_TODO_NEXT(callout_info);
	if (p1 != 0 && --p1->c_time <= 0) {
		timepoke();
	}
	mutex_spinunlock(&CI_LISTLOCK(callout_info), s);

	/* Notify tlb manager that the clock has ticked */
	tlb_tick_actions();
}


/*
** Timeout (callout) queue support.  On MPBUS systems, there is
** one queue per processor plus one "fast queue" per system.  The
** fast queue is handled on a designated "fast clock" processor.
** A given timeout entry on the fast queue will be handled in 
** c_time "fasthz units" (1ms) from the time the previous entry
** expires.  The *first* entry in the fast queue is relative to
** the current time.  Timeout entries on the normal queues are
** handled just as the fast queue, only the c_time entry is in
** "hz units" rather than "fasthz units".
**
** When fast timeouts are enabled, the fast clock processor takes
** interrupts at regular fasthz intervals.  Whenever it takes an
** interrupt, it updates the c_time member of the head of the fast 
** queue (to keep it relative to the current time of day).
*/

/*
 *  Use cpu-specific callout queues for C_NORM, and a single queue for C_FAST.
 *  While we're at it, update the id information in the new element and in
 *  the queuehead.
 */
struct callout *
timeout_get_queuehead(
        long list,
	struct callout *pnew)
{
	struct callout *queuehead;
	int isfast = (list == C_FAST || list == C_FAST_ISTK);

	/*
	 * The return id is a 32-bit number:
	 *	bits 0-23	id
	 *	bits 24		listid
	 *	bits 25-31	cpuid
	 *
	 * If a timeout is done every 10ms, then it will take about
	 * 46 hours until we wrap.
	 *
	 * DDI/DKI requires dtimeout/itimeout to return a non-zero
	 * identifier.
	 */
	if (isfast)
		queuehead = CI_TODO(fastcatodo);
	else
		queuehead = CI_TODO(&CALLTODO(pnew->c_cpuid));
	if (++queuehead->c_cid == 0)
		++queuehead->c_cid;
	pnew->c_cid = queuehead->c_cid;
	pnew->c_fast = isfast;
	if (pnew->c_time <= 0)	/* special-case adjustment */
		pnew->c_time = (pnew->c_time == TIMEPOKE_NOW) ? 0 : 1;

	return( queuehead );
}

/*
 * check a function timeout call from the callout table
 * if found fun and arg match the 1st arg then return c_tim
 * used by itimer codes 
 * there can only be 1 instance of this searched fun with the given arg
 */
__int64_t
chktimeout_tick(int list, toid_t id, void (*fun)(), void *arg)
{
	register __int64_t rv;
	int i;

	if (list == C_FAST || list == C_FAST_ISTK) {
		rv = do_chktimeout_tick(fastcatodo, id, fun, arg);
	} else {
		callout_info_t *callout_info;

		for (i=0; i<maxcpus; i++) {
			callout_info = &CALLTODO(i);
			if (rv = do_chktimeout_tick(callout_info, id, fun, arg))
				break;
		}
	}
	return(rv);
}

/*
 * fast_timeout is called to arrange that fun(arg) is called in tim/FASTHZ sec
 * An entry is sorted into the callout structure.
 * The time in each structure entry is the number of FASTHZ's more
 * than the previous entry. In this way, decrementing the
 * first entry has the effect of updating all entries.
 * If the fast clock is not on, fast_timeout will enable it for you
 * WARNING: Fast timeout function comes back on the fast clock processor,
 * so driver must be semaphored since the fast clock processor can be moved 
 * around by user.
 *
 */
toid_t
fast_timeout(void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;
	va_list ap;

	if (!fastclock)
		enable_fastclock();
	va_start(ap, tim);

	retval = dotimeout(cpuid(), fun, arg, tim, callout_get_pri(), C_FAST, ap);

	if (retval == NULL)
		cmn_err(CE_PANIC,"Timeout table overflow.\n Tune ncallout to a higher value.");
	return(retval);
}

toid_t
fast_timeout_nothrd(void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;
	va_list ap;

	if (!fastclock)
		enable_fastclock();
	va_start(ap, tim);

	retval = dotimeout(cpuid(), fun, arg, tim, 0, C_FAST_ISTK, ap);

	if (retval == NULL)
		cmn_err(CE_PANIC,"Timeout table overflow.\n Tune ncallout to a higher value.");
	return(retval);
}


/*
 * Set a timer interrupt to go off in ticks fast ticks.
 * The only case we need to consider here is a NOW request.
 * Anything else is handled automatically by the regularly
 * scheduled clock interrupts.
 */
/*ARGSUSED1*/
int
set_timer_intr(processorid_t targcpu, __int64_t ticks, long list)
{
	if (ticks == 0) {
		if (list == C_FAST || list == C_FAST_ISTK)
			fast_timepoke();
		else
			timepoke();
	}
	return 0;
}

/*
 *  For a given callout entry c_time value, return the equivalent in hz.
 *  For non-Everest platforms, c_time is hz.  Guarantee that we return
 *  a positive value, to avoid confusing higher level routines which don't
 *  like zero (because that looks like untimeout() and chktimeout_tick()
 *  didn't find a callout entry) and negative (because the user might be
 *  confused by a negative tick count).
 */
/*ARGSUSED2*/
int
callout_time_to_hz(__int64_t callout_time, int callout_cpuid, int fast)
{
	extern int fasthz;
	if (callout_time <= 0)
		return(1);
	if (fast)
		return(callout_time / (fasthz / HZ));
	return(callout_time);
}

#ifdef R4000
extern volatile uint call_cleanup;
extern volatile uint in_cleanup;
extern void ecc_cleanup(void);

#endif

/*
 * timein_body()
 *
 * This routine processes the todo list for the specified callout
 * info.  For each expired entry in the todo list, it will do one
 * of two things:
 * 	- if the entry is designated istk, it will invoke the entry's
 *	  callout function
 *	- if the entry is designated ithrd, it will move the entry to
 *	  the tail of the ithrd queue and (if needed) kick awake any
 *	  ithreads waiting to handle callouts
 */
void
timein_body(callout_info_t *cip)
{
	register struct callout *list, *p1, *p2;
	register int s;

	list = CI_TODO(cip);
	for (;;) {
		s = mutex_spinlock_spl(&CI_LISTLOCK(cip), splprof);
		p1 = list->c_next;

		/* no more work to do */
		if (p1 == 0 || p1->c_time > 0) {
			mutex_spinunlock(&CI_LISTLOCK(cip), s);
			break;
		}
		p2 = p1->c_next;	/* advance to next item */
		list->c_next = p2;

		if (p2)			/* & carry forward overflow */
			p2->c_time += p1->c_time;

		/*
		 * We've now removed p1 from the todo list.  It's
		 * either going to be executed straight away (if
		 * it's an istk callout) or else moved to the ithread
		 * queue and handled by the timein ithreads.
		 */
		if (C_IS_ISTK(p1->c_flags)) {
			mutex_spinunlock(&CI_LISTLOCK(cip), s);
			timein_entry(p1);
		} else {
			timein_entry_ithrd(cip, p1, s); /* unlocks CI_LISTLOCK */
		}
	}
}

/*
 * timein()
 *
 * Handle soft intr set up via one of the timepoke interfaces.
 */
void
timein(void)
{
#ifdef R4000
	uint ospl;
#endif

	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_RTCCOUNTER_INTR, NULL, NULL,
			 NULL, NULL);
	acksoftclock();

	if (private.p_timein & PDA_FTIMEIN) {
		private.p_timein &= ~PDA_FTIMEIN;
		timein_body(fastcatodo);
	}
	if (private.p_timein & PDA_TIMEIN) {
		private.p_timein &= ~PDA_TIMEIN;
		timein_body(&CALLTODO(cpuid()));
	}
#ifdef R4000
	ospl = splecc();
	if (call_cleanup && !in_cleanup) {
		splxecc(ospl);
		ecc_cleanup();
	} else
		splxecc(ospl);
#endif

	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_RTCCOUNTER_INTR, NULL,
			 NULL, NULL);
}

/* system call to adjust the time trimmer
 *
 * The numbers here are chosen so that time never goes backwards.
 * By limiting the adjustment to less the amount we have just incremented
 * time above, we keep time advancing, although we also limit the maximum
 * adjustment to less than 0.3%.
 * Setting timetrim is done on the clock processor only, to avoid having to
 * need timelock in clock()
 */
int
settimetrim(int val)
{
	int s;

	if (!_CAP_ABLE(CAP_TIME_MGT))
		return EPERM;
	ASSERT(cpuid() == clock_processor);

	if (val < -MAXTRIM_NSEC || val > MAXTRIM_NSEC)
		return EINVAL;

	s = mutex_spinlock(&timelock);
	timetrim = val;
	if (timedeltas == 0)		/* tell clock() to get new values */
		timedeltas = 1;
	mutex_spinunlock(&timelock, s);
	return 0;
}

/* system call to fetch the time trimmer
 */
int
gettimetrim(void)
{
	return timetrim;
}



/* guts of adjtime.  separate routine so it can be called from
 *	chktimedrift() if we discover that our time differs from the
 *	realtime clock chip.
 *
 * Note that the maximum slew rates must be chosen to keep the clock from
 *	ever going backwards.  Since the current time is advanced every
 *	msec, the maximum negative slew rate is less than 1msec/tick when
 *	clock() is called, or 1msec/10msec, or 100msec/sec, or 6000ms/minute.
 * It should also be less than the "timetrim" adjustment.
 *
 * doadjtime() is done on the clock processor only, to avoid having 
 * to need timelock in clock()
 */
#define TICKADJ	(240000/(60*HZ))	/* 240 msec/minute */
#define TINYADJ (USEC_PER_SEC/HZ/10)
#define SMALLADJ_LEN (HZ*60*3)
#define SMALLADJ (TICKADJ*SMALLADJ_LEN)
#define BIGADJ (TICKADJ*10)

long
doadjtime(long nval)			/* microseconds to adjust */
{
	int	ntimedeltas;
	long	oval;
	int	negative, s;

	ASSERT(cpuid() == clock_processor);

	negative = (nval < 0);
	if (negative)
		nval = -nval;
	if (nval <= TINYADJ) {		/* do tiny adjustments at once */
		ntimedeltas = 1;
	} else if (nval <= SMALLADJ) {    /* spread out modest ones */
		ntimedeltas = SMALLADJ_LEN;
		nval /= SMALLADJ_LEN;
	} else {			/* take forever with giants */
		ntimedeltas = nval/BIGADJ;
		nval = BIGADJ;
	}
	if (negative)
		nval = -nval;

	s = mutex_spinlock(&timelock);

	/* if it has been < 1 tick since the last adjustment, get the
	 * one that was not even started.
	 */
	if (next_timedeltas == 0)
		oval = tickdelta * timedeltas;
	else
		oval = next_tickdelta * next_timedeltas;

	next_timedeltas = ntimedeltas;
	next_tickdelta = nval*1000;	/* convert to nsec */
	timedeltas = 1;			/* start adjusting on next tick */

	mutex_spinunlock(&timelock, s);

	return oval/1000;		/* convert to usec */
}

/*
 * system time, should only be called by user program
 * The kernel calls nanotime()
 */
void
nanotime_syscall(timespec_t *tvp)
{
	timespec_t temp;
	uint32_t tstamp, elapsed;
	int nsec, ospl;

	ospl = spl7();	
	do {
		/* follow protocol to avoid collision with clock() */
		tstamp = hrestime.tstamp;
		temp = hrestime.time;
	} while (tstamp != hrestime.new_tstamp);

	elapsed = get_timestamp() - tstamp;

	/* 
	 * the time portion between main clock ticks has to be adjusted
	 * and trimmed also.
	 */
	nested_spinlock(&timelock);
	nsec = (elapsed*timetrim)/timer_freq; 	/* timetrim portion */
	if (timedeltas)			/* adjtime portion */
		nsec += (elapsed*tickdelta)/(timer_freq/HZ);
	nsec = elapsed*timer_unit + temp.tv_nsec + nsec;
	tvp->tv_sec = temp.tv_sec + ((unsigned)nsec / NSEC_PER_SEC);
	tvp->tv_nsec = (unsigned)nsec % NSEC_PER_SEC;

	/* guarantee time going forward */
	if (hrestime.lasttime.tv_sec > tvp->tv_sec
	    || (hrestime.lasttime.tv_sec == tvp->tv_sec
		&& hrestime.lasttime.tv_nsec >= tvp->tv_nsec)) {
		tvp->tv_sec = hrestime.lasttime.tv_sec;
		tvp->tv_nsec = hrestime.lasttime.tv_nsec + 1000;
	} else
	if (tvp->tv_sec > time)
		time = tvp->tv_sec;
	hrestime.lasttime = *tvp;
	mutex_spinunlock(&timelock, ospl);	
}

/*
 * lower resolution version of nanotime_syscall
 * used by kernel internal routines because it has less overhead.
 * does not use hi-res timer and does not handle
 * time going backward, unique time, time trim, adjtime
 */
void
nanotime(timespec_t *tvp)
{
	timespec_t temp;
	uint32_t tstamp;
	int ospl;

	ospl = spl7();	
	do {
		/* follow protocol to avoid collision with clock() */
		tstamp = hrestime.tstamp;
		temp = hrestime.time;
	} while (tstamp != hrestime.new_tstamp);
	splx(ospl);

	*tvp = temp;
}

#if _MIPS_SIM == _ABI64
/* Needed by snoop.  Can't change the size of snoopheader while board
 * firmware like ipg board knows about it.
 */
void
irix5_microtime(struct irix5_timeval *tvp)
{
	timespec_t ts;
	nanotime_syscall(&ts);
	TIMESPEC_TO_TIMEVAL(&ts, tvp);
}
#endif

void
microtime(struct timeval *tvp)
{
	timespec_t ts;

	nanotime_syscall(&ts);
	TIMESPEC_TO_TIMEVAL(&ts, tvp);
}


/*
 * set the time for the several system calls that like that game.
 */
void
settime(long sec, long usec)
{
	int s;

	ASSERT(cpuid() == clock_processor);
	s = mutex_spinlock(&timelock);
	time = sec;
	/* follow protocol to avoid read error */
	hrestime.new_tstamp = get_timestamp();
	hrestime.time.tv_nsec = usec*1000;
	hrestime.time.tv_sec = sec;
	next_timedeltas = 0;
	next_tickdelta = 0;
	timedeltas = 0;
	tickdelta = 0;
	hrestime.lasttime = hrestime.time;
	hrestime.tstamp = hrestime.new_tstamp;
	mutex_spinunlock(&timelock, s);
}



/*
 * Adjust the time to UNIX based time
 * called when the root file system is mounted with the time
 * obtained from the root. Call inittodr to try to get the actual 
 * time from the battery backed-up real-time clock.
 */
void
clkset(time_t oldtime)
{
	/* set hrestime there also */
	inittodr(oldtime);
	restoremustrun(cpuid);
}

/*
 * Hooks used by the performance monitoring module (prf.c) to crank up
 * and shut down the profiling clock per CPU. Previously, the fast clock
 * was intertwined with the profiling clock code (because they use the
 * same hardware clock), but it really needs to be separated out.
 * These routines provide the abstraction needed by the profiler.
 */
void
startprfclk() 
{
	if (++private.p_prf_enabled == 1) {
		if (!fastclock) 
			enable_fastclock();
		if (!(private.p_flags & PDAF_FASTCLOCK))
			startkgclock();
	}
}

void
stopprfclk() 
{
   	int		ospl;

	ospl = spl7();
	if (private.p_prf_enabled && --private.p_prf_enabled == 0 &&
	    !(private.p_flags & PDAF_FASTCLOCK))
		slowkgclock();
	splx(ospl);
}



/* called by clkstart() and sysmp(MP_FASTCLOCK) or startprfclk() */
void
enable_fastclock(void)
{
	int s;

	/*
	* switch over to fast clock processor
	* this allows us to simply raise spl to
	* keep any race conditions with prfintr()
	* from occurring
	*/
	s = spl7();
	fastclock = 1;	
	private.p_flags |= PDAF_FASTCLOCK;
	setkgvector(fastick_maint);
	startkgclock();
	splx(s);
	restoremustrun(was_running);
}


#if defined(MCCHIP) || defined(IP32) || defined(IPMHSIM)
/*
 * This function returns the size of the cycle counter
 */
int
query_cyclecntr_size(void)
{
	return(32);
}


#endif /* MCCHIP */

/*
 * return the frequency of the clock used by the fastest timeouts. On most
 * systems this is the value of fasthz, a higher speed clock.
 */
int
query_fasttimer(void)
{
	extern int fasthz;
	return(NSEC_PER_SEC / fasthz);
}

void
timepoke(void)
{
	private.p_timein |= PDA_TIMEIN;
	setsoftclock();
}

void
fast_timepoke(void)
{
	ASSERT(private.p_flags & PDAF_FASTCLOCK);

	private.p_timein |= PDA_FTIMEIN;
	setsoftclock();
}

void
inithrestime(void)
{
	extern int fasthz;

	hrestime.new_tstamp = get_timestamp();
	hrestime.tstamp = hrestime.new_tstamp;
	if (!fastick)
		fastick = USEC_PER_SEC/fasthz;
}
