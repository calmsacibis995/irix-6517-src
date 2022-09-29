/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Misc support routines for clocks and timers using clock/compare registers.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/pda.h>
#include <sys/psw.h>
#include <sys/prf.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/callo.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/capability.h>
#include <sys/kmem.h>
#include <sys/runq.h>
#include <sys/sema.h>
#include <sys/clock.h>
#include <sys/clksupport.h>
#include <stdarg.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/ddi.h>
#include <sys/cpu.h>
#include <ksys/vhost.h>

#include <sys/calloinfo.h>

#if EVEREST
#include <sys/EVEREST/evintr.h> /* for setrtvector prototype */
#endif

#if defined(SN)
#include <sys/SN/addrs.h>	/* Needed for Hub DMA timeout WAR */
#include <sys/SN/intr.h>
#include <sys/SN/router.h>
#include <sys/SN/agent.h>

#if defined (SN0)
#include <ksys/partition.h>
#include <sys/SN/SN0/sn0drv.h>
#include <sys/SN/SN0/hubstat.h>
#include <sys/SN/SN0/arch.h>
#include "SN/SN0/sn0_private.h"
#endif

extern int gather_craylink_routerstats;

extern int disable_sbe_polling;

extern int hasmetarouter;
#endif

#if IP30
static __int64_t first_timein(callout_info_t *);
static int _set_timer_intr(processorid_t, __int64_t, long, int, clkreg_t);
void flush_console(void);
void trimcompare(uint);
void initcounter(uint);
void resetlatency(void);
uint resetcounter_val(uint);	/* returns latency info */
#include <sys/ds1687clk.h>
#include <ml/RACER/RACERkern.h>
#define acktmoclock()
#define setrtvector(X)
#define ackkgclock()
unsigned int cpufreq_cycles;	/* private.cpufreq_cycles/100000 */
#define cvt_ctime(d)		(((d)*cpufreq_cycles)/(CYCLE_PER_SEC/100000))
#define rem_ctime(d)		(((d)*cpufreq_cycles)%(CYCLE_PER_SEC/100000))

#define cvt_count(d)		((((__uint64_t)(d))*CYCLE_PER_SEC) /	\
				 private.cpufreq_cycles)

/* avoid constant divide by fasthz at runtime */
extern int clk_fclock_fast_freq;
#define CLK_FCLOCK_FAST_FREQ	clk_fclock_fast_freq

#define roundup_ctime(X)	roundup(X,count_min_delta)
#define rounddn(X,Y)		((X) - ((X)%(Y)))
#define rounddn_ctime(X)	rounddn(X,count_min_delta)
unsigned int count_min_delta;

#define CYCLE_PER_USEC		(CYCLE_PER_SEC/1000000)
/* Timeout alignment boundary. We choose a value which ensures that we are
 * already aligned with ctime. This depends on the relative frequencies of
 * the cpu counter and the heart counter.
 */
#define TIMEOUT_ALGNMT_BDRY     (10 * CYCLE_PER_USEC)

#if DEBUG || CLKDEBUG
unsigned long closecount[MAXCPUS];
unsigned long earlycount[MAXCPUS];
unsigned long latecount[MAXCPUS];
unsigned long okcount[MAXCPUS];
#define CLOSECOUNT	closecount[cpuid()]++
#define EARLYCOUNT	earlycount[cpuid()]++
#define LATECOUNT	latecount[cpuid()]++
#define OKCOUNT		okcount[cpuid()]++
#define CLKASSERT(x)	ASSERT_ALWAYS(x)
#define LASTSIZE 32
struct {
	clkreg_t	next;
	uint		intr;
	uint		lctime;
	uint		ctime;
	uint		late;
	int		cvtdelta;
	uint		latency;
} clk_last[MAXCPUS][LASTSIZE];
int lasti[MAXCPUS];
#define update_next_intr(time,curtime,lat)				\
	clk_last[cpuid()][lasti[cpuid()]].next  = time;			\
	clk_last[cpuid()][lasti[cpuid()]].intr  = (uint)private.p_next_intr;\
	clk_last[cpuid()][lasti[cpuid()]].lctime = curtime;		\
	clk_last[cpuid()][lasti[cpuid()]].ctime = (uint)GET_LOCAL_RTC;	\
	clk_last[cpuid()][lasti[cpuid()]].cvtdelta = 			\
		cvt_ctime(time - private.p_next_intr);			\
	clk_last[cpuid()][lasti[cpuid()]].latency = lat;		\
	clk_last[cpuid()][lasti[cpuid()]].late = (uint)latecount[cpuid()];\
	lasti[cpuid()] = (lasti[cpuid()] + 1) % LASTSIZE;		\
	private.p_next_intr = time
#else
#define CLOSECOUNT
#define EARLYCOUNT
#define LATECOUNT
#define OKCOUNT
#define CLKASSERT(x)
#define update_next_intr(time,curtime,lat)				\
	private.p_next_intr = time
#endif
#endif

/*
 * REACT/Pro
 */
#include <sys/frs.h>
#include <sys/rtmon.h>

static void __nanotime(void);
static uint findcpufreq_cycles(void);
static void init_fasthz_variables(void);

#define MAXTRIM_USEC (USEC_PER_SEC/HZ/10*3) /* max trim in usec/sec */
#define MAXTRIM_NSEC (MAXTRIM_USEC*1000)

long rt_ticks_per_sec = HZ;
long rtclk_interval;

/*
 * These are here just for compatibility with non-Everest systems.  These
 * variables are checked at several places in machine-independent code.
 */
int fastclock = 1;			/* enable fast clock (always) */
int fastick;				/* usec/fast tick--bogus but !=0 */
					/* defines resolution of timeout queue */
static int todcinitted = 0;
int audioclock = 0;			/* profiling clock is used by audio */
void timein(void);
void timepoke(void);			/* sets up callout handling */
extern void timein_entry_ithrd(callout_info_t *, struct callout *, int);
lock_t	timelock;			/* lock on time-of-day variables */

clkreg_t realtime_base_RTC;      /* RTC(-tick) when lbolt first ticks */

/*
 * "timebase" is protected by the "timelock" spinlock, but it can be snapshot
 * without holding the spinlock to reduce contention; the "timedata_gen" and
 * "timedata_newgen" generation numbers are used to ensure the data
 * consistency.
 *
 * The same is true for the "adjtime_nanos" and "adjtime_delta" variables.
 *
 * To update the data:
 * - we need to be at splprof() level to avoid interrupts that might
 *   attempt to read the data while we're in the middle of an update.
 *   NOTE: this assumes that no interrupt routine at a higher level will
 *   attempt to read "timebase" or "adjtime" data. Otherwise, the CPU
 *   might freeze.
 * - the "timelock" spinlock needs to be held at splprof() level (to
 *   synchronize with other writers on other processors).
 * - the "newgen" generation number is incremented.
 * - __synchronize() to ensure "newgen" is updated before the data.
 * - update the data.
 * - __synchronize() again to ensure the data is data is updated before the
 *   "gen" generation number.
 * - update the "gen" generation number with "newgen".
 *
 * To snapshot the data:
 * - it's not necessary to be at splprof() level.
 * - snapshot the "gen" generation number.
 * - __synchronize() to ensure the "gen" snapshot is taken before the data
 *   is accessed.
 * - access the data.
 * - __synchronize() again to ensure the data was read before the "newgen"
 *   generation number.
 * - compare the "gen" snapshot and the "newgen" generation number.
 * - if they're not equal, the data isn't consistent and we need to try
 *   again...
 */

static volatile int timedata_gen, timedata_newgen; /* generation numbers */
static volatile timespec_t timebase;	/* adjusted nanosecond time */
					/*  updated once per second */
static volatile clkreg_t timebase_RTC;	/* EV_RTC when timebase last updated */
volatile long long adjtime_nanos;	/* #nanos to adj time by */
					/* during this delta */
volatile long long adjtime_delta;	/* #cycles over which to adjust time */
volatile clkreg_t adjtime_RTC;		/* RTC at moment that adjtime */
					/* period started */
extern int timetrim;		/* trim clock by this many nsec/sec */

extern int fasthz;		/* ticks per second, defines max resolution of
				   timeout queue, and profiling resolution */
extern char qrunflag;		/* need to run streams queues */

extern u_char miniroot;

#define RTSLOW_RATE	0xf0000000L	/* about 90% of the max */
#define RTC_SLOW_RATE	0xf0000000L	/* about 90% of the max */

static int  sec_countdown;    	/* keeps track of second; counts HZ  */
static int  minute_countdown = 0; /* keeps track of minute; counts seconds */
int   	    record_uptime = 0;	/* if this value = 1, then lbolt value is
			 	** written into NVRAM every minute. Can be
				** toggled by syssgi syscall, using flags
				** SGI_RECORD_UPTIME/SGI_UNRECORD_UPTIME.
				** Initial value is 0, so that a maintenance
				** program such as rc2.d/S95avail can read
				** the previous value of nvram (which will give					** uptime of previous boot).
				*/

/* XXX - Fix me!  Needs to be distributed across CPUs */
#if EVEREST
int flag_at_kernelbp[EV_MAX_CPUS];
#else
int flag_at_kernelbp[MAXCPUS];
#endif

unsigned timer_freq;		/* freq of timestamping source */
unsigned timer_unit;		/* duration in nsec of timer tick */
unsigned timer_high_unit;	/* in sec */
unsigned timer_maxsec;		/* duration in sec before wrapping */

#ifndef acktmoclock
extern void acktmoclock(void);
#endif
#ifndef ackkgclock
extern void ackkgclock(void);
#endif
extern void fast_user_prof(eframe_t *);
extern void resetcounter(uint);
extern int  clock(eframe_t *);
extern int  rtodc(void);
extern void _set_count(uint);
extern uint _get_count(void);
#if !TFP
extern uint _get_compare(void);
#endif /* !TFP */
extern void flushbus(void);
extern void queuerun(void);
extern void tlb_tick_actions(void);
extern void tlb_onesec_actions(void);

#if SABLE
#undef	EV_GET_LOCAL
#undef	EV_GET_LOCAL_RTC
#undef  GET_LOCAL_RTC
clkreg_t	pseudo_RTC;
clkreg_t	pseudo_CMPREG=0;
clkreg_t	last_CMPREG_intr=0;

clkreg_t
GET_LOCAL_RTC_FUNC(void)
{
	clkreg_t	lbolt_RTC;

	/* Derive RTC from cpu chip counter interrupts */
	lbolt_RTC = (long long)lbolt*(NSEC_PER_SEC/HZ)/NSEC_PER_CYCLE;
	if (lbolt_RTC > pseudo_RTC)
		pseudo_RTC = lbolt_RTC;
	else
		pseudo_RTC += 10; /* Just bump 10 cyckes */
	if ((pseudo_CMPREG != last_CMPREG_intr) &&
	    (pseudo_RTC >= pseudo_CMPREG)) {
		last_CMPREG_intr = pseudo_CMPREG;
		timepoke(); /* generate interrupt if needed */
	}
	return(pseudo_RTC);
}

#define GET_LOCAL_RTC	GET_LOCAL_RTC_FUNC()	
#endif /* SABLE */

/* disable real-time clock, timeout clock, and  profile clock interrupts */
void
stopclocks(void)
{
	slowkgclock();

	/*
	 * Slow down the CPU scheduler clock, and clear the data structure
	 * which tells us that we haven't seen a clock interrupt in N
	 * seconds (to avoid a warning when this CPU starts handling clock
	 * interrupts again).
	 */
	private.p_rtclock_rate = RTSLOW_RATE;
	private.last_sched_intr_RTC = 0;

	acktmoclock();
	ackkgclock();
}

/*
 * rt clock interrupt handler.
 */

extern volatile int counter_intr_panic;
#ifdef DEBUG
clkreg_t	counter_intr_warn_cycles = 2*CYCLE_PER_SEC;
uint		counter_intr_over_dbg = 0;
clkreg_t 	timein_last_called = 0;
#else
clkreg_t	counter_intr_warn_cycles = 10*CYCLE_PER_SEC;
#endif

#ifdef DEBUG
uint verify_not_slow_dbg = 0;
void
verify_not_slow(void)
{
	int s;

	s = splprof();
	if (verify_not_slow_dbg)
	    if (_get_count() > (private.p_rtclock_rate + verify_not_slow_dbg))
		debug(0);
	splx(s);
}
#endif

/* ARGSUSED */
void
counter_intr(eframe_t *ep)
{
	clkreg_t value_RTC;
	long	lost_intr_secs, wraparound_secs;
	clkreg_t	lost_intr_cycles;
	uint    diff;
	void	frs_handle_counter_intr(void);
#if IP30
	int hadslow = 0;
	int needstamp = 0;
#endif

#ifndef IP30	/* IP30 STAMP taken care of later */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_CPUCOUNTER_INTR, NULL, NULL,
			 NULL, NULL);
#endif
	/*
	 * Frame Scheduler
	 */
	if (private.p_frs_flags) {
#if defined(IP30)
		LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_CPUCOUNTER_INTR,
				 NULL, NULL, NULL, NULL);
#endif
		frs_handle_counter_intr();
		goto end_counter_intr;
	}

#if defined(R10000)
	/*
	 * If hw counters overflowed, process overflow intr.
	 */
	if (hwperf_overflowed_intr()) {
		hwperf_intr(ep);
		return;
        }
#endif /* R10000 */
	
	/*
	 * Despite the fact that there is a fastpath exit for a
	 * non-preemptive cpu in locore, this check is necessary
	 * because while we are in evdev_intr for a nonclock
	 * interrupt, the bit in the cause resgister for the clock
	 * interrupt could go high and evdev_intr will call counter_intr.
	 *
	 * For IP30, we still need to handle timeouts which hang off the
	 * same clock.
	 */
	if (private.p_flags & PDAF_NONPREEMPTIVE) {
#ifndef IP30
		resetcounter(private.p_rtclock_rate);	/* clears interrupt */
		goto end_counter_intr;
#else
		
		value_RTC = GET_LOCAL_RTC;

		/* Set to high value so that we don't get interrupted any time
		 * soon.
		 */
		resetcounter(0xffffffff);	/* clears interrupt */

		/* need to setup next timeout if need be. */
		goto setup_next_intr;
#endif /* IP30 */
	}
  
	value_RTC = GET_LOCAL_RTC;

#if IP30
	/* above code is not ported yet */
	ASSERT(private.p_frs_flags == 0);

	/* Interrupt came early for some reason (slight clock inaccuracies,
	 * that some machines have).  If delta is small for resetcounter
	 * (<1000 C0_COUNT ticks) with some padding, then wait a bit,
	 * otherwise re-schedule an interrupt for after p_next_intr.
	 */
	if (private.p_next_intr > value_RTC) {
		clkreg_t delta = private.p_next_intr - value_RTC;
		if (delta < cvt_count(1200)) {
			while (value_RTC < private.p_next_intr)
				value_RTC = GET_LOCAL_RTC;
			resetlatency();
			CLOSECOUNT;
		}
		else {
			value_RTC = rounddn_ctime(value_RTC);
			resetcounter(cvt_ctime(private.p_next_intr-value_RTC));
#if CLKDEBUG		/* interrupt came very early for some reason */
			cmn_err(CE_WARN,"CPU %d early compare RTC %d next %d",
				cpuid(), value_RTC, private.p_next_intr);
#endif
			EARLYCOUNT;
			return;
		}
	}
	/* Make sure we next_intr is within a bit of normal 1ms interrupt
	 * frequency.  This can happen with sloppy clocking or spl problems,
	 * so try to get back on track, by increasing p_next_intr by 50us
	 * which will trim us back back to in sync with ctime.  We do not
	 * trim by more on each interrupt to avoid getting fclocks too
	 * early to audio.
	 */
	else if ((private.p_next_intr+(100*CYCLE_PER_USEC)) < value_RTC) {
		update_next_intr(private.p_next_intr + 50*CYCLE_PER_USEC,
				 value_RTC,42);
		LATECOUNT;
	}
#if CLKDEBUG
	else
		OKCOUNT;
#endif

	/* only do sched tick math on sched ticks */
	if (value_RTC >= private.p_clock_tick) {
		LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_CPUCOUNTER_INTR,
				 NULL, NULL, NULL, NULL);
		needstamp = 1;
#endif /* IP30 */

	if (private.last_sched_intr_RTC) {
		lost_intr_cycles = value_RTC - private.last_sched_intr_RTC;
		if (lbolt > 1000) {	/* don't count early startup */
			diff = _get_count() -
#if TFP
				0x080000000L;
#else
				_get_compare();
#endif
			if (diff > private.counter_intr_over_max)
				private.counter_intr_over_max = diff;
			private.counter_intr_over_sum += diff;
			private.counter_intr_over_count++;
#ifdef DEBUG
			if (counter_intr_over_dbg)
				if (diff >= counter_intr_over_dbg)
					debug(0);
#endif
		}
	} else
		lost_intr_cycles = 0;

	/*
	 *  We want to avoid complaining about a "lost interrupt" if we've
	 *  lost it because the debugger blocks interrupts (because too many
	 *  debugger users would be needlessly concerned), so set a flag before
	 *  we enter the debugger.  This flag really should be a per-cpu flag,
	 *  but that's complicated for locore to figure out, so treat the flag
	 *  as a cpu0 flag.
	 */
	if (lost_intr_cycles >= counter_intr_warn_cycles  && 
	    private.p_rtclock_rate != RTSLOW_RATE  &&
	    (flag_at_kernelbp[cpuid()] == 0) ) {
	    lost_intr_secs = lost_intr_cycles / CYCLE_PER_SEC;
	    wraparound_secs = (unsigned)0xffffffff / 1000000 / 
			      (unsigned)private.cpufreq;
	    cmn_err((counter_intr_panic) ? CE_PANIC : CE_WARN,
		    "CPU %d hasn't seen a scheduler clock interrupt in %d seconds, EPC 0x%x, RA: 0x%x\n         %s",
		    cpuid(), lost_intr_secs, ep->ef_epc, ep->ef_ra,
		    (lost_intr_secs >= wraparound_secs-2 &&
		     lost_intr_secs <= wraparound_secs+2)
			? "Probable lost interrupt!"
#if IP30
			: "Probably caused by splprof or spl7 delaying the interrupt");
#else
			: "Probably caused by splhi or spl7 delaying the interrupt");
#endif
	}
	private.last_sched_intr_RTC = value_RTC;

#if IP30
	/*
	 * For IP30, the counter_intr drives the timeout chain and the
	 * unix sched activity (i.e. clock()) AND the 'fast' clock, which
	 * drives both audio and user/kernel profiling
	 *
	 * NOTE: sched if is above machine independent sched clock checking.
	 */
		private.p_clock_tick += CLK_FCLOCK_SLOW_FREQ;
		private.p_clock_ticked++;	/* protected by spl */
		/* catch-up if we missed a tick, and keep a record of it */
#if CLKDEBUG
		if (private.p_clock_tick <= value_RTC &&
		    flag_at_kernelbp[cpuid()] == 0 && lbolt > 180)
			cmn_err(CE_WARN,
				"CPU %d slow clock %d vs RTC %d next %d",
				cpuid(),private.p_clock_tick,value_RTC,
				private.p_next_intr);
#endif
		while (private.p_clock_tick <= value_RTC) {
			if (flag_at_kernelbp[cpuid()] == 0) {
				if (lbolt > 180)
					private.p_clock_slow++;
				private.p_clock_ticked++;	/* catch-up */
			}
			private.p_clock_tick += CLK_FCLOCK_SLOW_FREQ;
			value_RTC = GET_LOCAL_RTC;
			hadslow = 1;
		}
	}	/* above if > private.p_clock_tick */
	if ((private.p_fclock_tick>1) && (value_RTC >= private.p_fclock_tick)) {
		private.p_fclock_tick += CLK_FCLOCK_FAST_FREQ;
		private.p_fclock_ticked++;	/* protected by spl */
		/* catch-up if we missed a tick, and keep a record of it */
#if CLKDEBUG
		if (private.p_fclock_tick <= value_RTC &&
		    flag_at_kernelbp[cpuid()] == 0 && lbolt > 180)
			cmn_err(CE_WARN,
				"CPU %d slow fclock %d vs RTC %d next %d",
				cpuid(),private.p_clock_tick,value_RTC,
				private.p_next_intr);
#endif
		while (private.p_fclock_tick <= value_RTC) {
			if (flag_at_kernelbp[cpuid()] == 0) {
				if (lbolt > 180)
					private.p_fclock_slow++;
				private.p_fclock_ticked++;
			}

			/* may have passed p_clock_tick, re-sync it */
			if (private.p_clock_tick <= value_RTC) {
				if (flag_at_kernelbp[cpuid()] == 0) {
					private.p_clock_slow++;
					private.p_clock_ticked++;
				}
				private.p_clock_tick += CLK_FCLOCK_SLOW_FREQ;
			}

			private.p_fclock_tick += CLK_FCLOCK_FAST_FREQ;
			value_RTC = GET_LOCAL_RTC;
			hadslow = 1;
		}
	}

	/* If we had a slow interrupt, we need to make p_next_intr sane
	 * again, and reset the count/compare so we do not use an insane
	 * latency in resetcounter().  We use value_RTC, as that will
	 * let expired timeouts timepoke() in set_timer_intr().
	 */
	if (hadslow) {
		resetlatency();
		update_next_intr(rounddn_ctime(value_RTC),value_RTC,2);
	}

setup_next_intr:

	flag_at_kernelbp[cpuid()] = 0;

	/* Set clock to min(p_clock_tick,p_fclock_tick,timeout),
	 * and clear the interrupt.  Will handle any needed
	 * timepoke().
	 */
	_set_timer_intr(cpuid(),first_timein(&CALLTODO(cpuid())),C_NORM,1,
			value_RTC);
	if (needstamp)
		LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT,
			 	 TSTAMP_EV_CPUCOUNTER_INTR, NULL, NULL, NULL);
	return;
#else
	flag_at_kernelbp[cpuid()] = 0;

	resetcounter(private.p_rtclock_rate);	/* clears interrupt */

	clock(ep);
#endif

end_counter_intr:
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT,
			 TSTAMP_EV_CPUCOUNTER_INTR, NULL, NULL, NULL);
}

/* Start the scheduling clock */
void
startrtclock(void)
{
#if !IP30
	/* nonzero means counter_intr() is vector */
	static int sched_clock_vector_set;

	private.p_rtclock_rate = findcpufreq_cycles() / HZ;

	/*
	 * There is a more accurate way of doing this: compute the "error" rate
	 * that we lose from this truncation (cycles % HZ), at every clock tick
	 * accumulate this error rate into an cumulative sum; when that sum
	 * is >= HZ, then decrement HZ from the cumulative sum, and add one
	 * to the interval value for the next interval.
	 * We don't appear to need this extra accuracy because we appear to be
	 * not ticking fast enough, and this extra accuracy will slow down the
	 * longterm interrupt rate, not speed it up.
	 */
	if (sched_clock_vector_set == 0) {	/* call setrtvector () only */
		/* set up counter_intr */
#if EVEREST
		setrtvector((EvIntrFunc)counter_intr);
#else
		setrtvector((intr_func_t)counter_intr);
#endif
		sched_clock_vector_set = 1;
	}

	resetcounter(private.p_rtclock_rate);
#else
	__uint64_t cur_time;

	private.p_rtclock_rate = findcpufreq_cycles() / HZ;

	ASSERT(private.cpufreq_cycles);		/* needed for cvt_ctime */

	if (master_procid == cpuid()) {
		/* For conversion use smaller multipliers to avoid wrapping
		 * when converting a 52-bit heart timer ctime.
	 	 */
		ASSERT((private.cpufreq_cycles%100000) == 0);
		cpufreq_cycles = private.cpufreq_cycles/100000;

		/* calculate minimim Heart count vs CPU count delta value */
		if (!count_min_delta) {
			uint i;

			for (i = 1; rem_ctime(i); i++)
				/*EMPTY*/;

			count_min_delta = i;
		}

		CLKASSERT(count_min_delta && count_min_delta <= 125);
		CLKASSERT((CLK_FCLOCK_SLOW_FREQ % count_min_delta) == 0);
		CLKASSERT(rem_ctime(CLK_FCLOCK_SLOW_FREQ) == 0);
		CLKASSERT(((CYCLE_PER_SEC/1000) % count_min_delta) == 0);
		CLKASSERT(rem_ctime(CYCLE_PER_SEC/1000) == 0);
	}

	/* Whenever we go to the non-RTSLOW_RATE, p_clock_tick must
	 * say when the next clock() should be.  Start clock out
	 * being rounded to the slow rate as common clock frequencies
	 * are multipals of this and it makes time round.
	 *
	 * Parital intervals will be rounded to count_min_delta, but
	 * processors at least for now should all match [f]clock
	 * divisors of CLK_FCLOCK_SLOW_FREQ.  We round again here
	 * though, just in case.
	 */
	private.p_clock_slow = 0;
	cur_time = GET_LOCAL_RTC;
	private.p_clock_tick = cur_time + CLK_FCLOCK_SLOW_FREQ;
	private.p_clock_tick = roundup(private.p_clock_tick,
				       CLK_FCLOCK_SLOW_FREQ);
	private.p_clock_tick = roundup_ctime(private.p_clock_tick);
	update_next_intr(private.p_clock_tick,0,3);
	cur_time = rounddn_ctime(cur_time);
	initcounter(cvt_ctime(private.p_clock_tick - cur_time));
#endif
}

void
startaudioclock(void)
{
	cpu_cookie_t	was_running;
	/*
	 * Check if the audio clock is already on, no reason to restart
	 */
	if (audioclock)
		return;
	/* Audio clock only runs on the master_proc */
	was_running = setmustrun(master_procid);
	audioclock = 1;
	startkgclock();
	restoremustrun(was_running);
}

void
stopaudioclock(void)
{
	cpu_cookie_t	was_running;
	/*
	 * Nothing to do if we do not have an audio clock
	 */
	if (!audioclock)
		return;
	was_running = setmustrun(master_procid);
	audioclock = 0;
	if (private.p_prf_enabled == 0)
		slowkgclock();
	restoremustrun(was_running);
}

void
startprfclk(void)
{
	int s;

	s = splprof();
	atomicAddInt(&private.p_prf_enabled, 1);
	/*
	 * If this is the first call to startprfclk then really start the clock
	 */
	if (private.p_prf_enabled == 1) {
		startkgclock();
	}
	splx(s);
}

void
stopprfclk(void)
{
	int s;

	s = splprof();
	if (private.p_prf_enabled) {
		atomicAddInt(&private.p_prf_enabled, -1);
		/*
		 * If we were the last user of the clock then slow the clock
		 * down.
		 */ 
		if (private.p_prf_enabled == 0)
			if ((cpuid() != master_procid) || (audioclock == 0))
				slowkgclock();
	}
	splx(s);
}

void
ackrtclock(void)
{
	/* 
	 * Don't need to do anything here, since count/compare
	 * interrupt will always be acknowledged as soon as it's
	 * taken (via counter_intr and resetcounter).
	 */
}

void
init_timebase(void)
{
	time = rtodc();   /* time since 1970, in seconds */
	/*
	 * Always set timebase_RTC before timebase.* -- see __nanotime.
	 */
	timebase_RTC = GET_LOCAL_RTC;	/* init for __nanotime() */
	timedata_gen = timedata_newgen = 0;
	timebase.tv_sec  = time;
	timebase.tv_nsec = 0;

	adjtime_RTC = 0;
	adjtime_delta = 0;

#ifndef SABLE
	/*
	 * Verify that the time-of-day chip is advancing.  If it's not,
	 * then tell the world about it now.  We'll have a bad "time"
	 * at this point.
	 */
#if IP30
	/* spin for up to 2 seconds */
	while (GET_LOCAL_RTC < (timebase_RTC + 2*CYCLE_PER_SEC)) {
		if (rtodc() != time)
			return;
	}
	cmn_err(CE_WARN, "System time-of-day chip is not advancing.\n"
			 "  CHECK AND RESET THE DATE!\n");
#else
	/* spin for 2 seconds */
	while (GET_LOCAL_RTC < (timebase_RTC + 2*CYCLE_PER_SEC));
	if (rtodc() == time) {
		cmn_err(CE_WARN, "System time-of-day chip is not advancing.\n"
				 "  CHECK AND RESET THE DATE!\n");
	}
#endif
#endif	/* !SABLE */
}

void
inittodr(time_t base)
{
	long deltat;
#ifdef IP30
	time_t curtime;
#endif

	/* Not the best place to initialize fastick, but it'll work. */
	ASSERT(fasthz != 0);
	fastick = USEC_PER_SEC/fasthz; 

	/*
	 * The disk time is meaningless when running the miniroot kernel,
	 * so don't trouble the user with annoying messages.
	 */
	if (miniroot)
		return;

#ifdef IP30
	curtime = rtodc();	/* resample clk as xFS cheats on base */

	if (curtime < TODRZERO || curtime < base) {
		if (time < TODRZERO)
			cmn_err_tag(50,CE_WARN,"lost battery backup clock");
		else
			cmn_err_tag(51,CE_WARN,"time of day clock behind file "
				    "system time--resetting time");
		settime(base,0);
		wtodc();	/* try and reset clock to filesystem date */
	}
#endif /* IP30 */

	if (base < 5*SECYR) {
		cmn_err_tag(52,CE_WARN, "preposterous time in file system");
		goto check;
	}

	/*
	 * See if we gained/lost two or more days:
	 * If so, assume something is amiss.
	 */
	deltat = time - base;
	if (deltat < 0)
		deltat = -deltat;
	todcinitted = 1;
	if (deltat < 2*SECDAY)
		return;

	cmn_err_tag(53,CE_WARN,"clock %s %d days",
		    time < base ? "lost" : "gained", deltat/SECDAY);

check:
	cmn_err_tag(54,CE_WARN, "CHECK AND RESET THE DATE!");
}


/*
 * This function returns the resolution of the counter in picoseconds
 * this is called by syssgi.
 */
__psint_t
query_cyclecntr(uint *cycle)
{
	*cycle = timer_unit*1000;
	return((__psint_t)CLK_CYCLE_ADDRESS_FOR_USER);
}

/*
 * return the frequency of the clock used by the fastest timeouts. On EVEREST
 * This is the CC clock and is the same as the length of the bus cycle.
 */
int
query_fasttimer(void)
{
#if IP30
	return(NSEC_PER_SEC / fasthz);
#else
	return(NSEC_PER_CYCLE);
#endif
}

/*
 * This function returns the size of the cycle counter
 */
int
query_cyclecntr_size(void)
{
	return(64);
}

void
tick_maint(void)
{
	extern int one_sec;

	if (lbolt == 1  &&  cpuid() == 0) {	/* first tick! (and cpu0) */
		/*
		 *  Initialize realtime_base_RTC to be the RTC value at the
		 *  time of the beginning of the first scheduler clock interval
		 */
		realtime_base_RTC = GET_LOCAL_RTC - CYCLE_PER_SEC/HZ;
	}

	sec_countdown--;
	if (sec_countdown == 0) {
		one_sec = 1;
		/* atomically set timebase_RTC and timebase */
		__nanotime();
#if defined(IP19)
		{
		extern void ecc_interrupt_check(void);

		ecc_interrupt_check();
		}
#endif
	}
}

void (*kdsp_timercallback_ptr)(void) = 0;
void (*isdnaudio_timercallback_ptr)(void) = 0;
void (*midi_timercallback_ptr)(void) = 0;
#if !defined(TSERIALIO_TIMEOUT_IS_THREADED)
void (*tsio_timercallback_ptr)(void) = 0;
#endif
void (*ieee1394_timercallback_ptr)(void) = 0;

void
fastick_maint(eframe_t *ep)
{
	ackkgclock();

	/*
	 * Must call certain support code at high spl every ms. on cpu 0
         *
         * XXX It is wasteful to check each pointer individually.
         * The check for prf, kdsp, midi, and isdnvoice could
         * be avoided: we could easily pre-compute the exact set of 
         * function calls that should be made in this routine, so
         * that no checking of any kind needed to be done here
         * for those services.  The set of functions to call
         * would only have to be recomputed when a given service 
         * wanted to CHANGE its pointer, which happens much more 
         * infrequently than the fastick interrupt itself.
         *
         * This was not done in the banyan release in the interests
         * of producing a stable and simple checkin with minimal 
         * code impact.
	 */
        if (cpuid() == master_procid) {

#if !defined(AUDIO_TIMEOUT_IS_THREADED)
		if (kdsp_timercallback_ptr) {
			kdsp_timercallback_ptr();
		}
#endif
		if (isdnaudio_timercallback_ptr) {
			isdnaudio_timercallback_ptr();
		}
		if (midi_timercallback_ptr) {
			midi_timercallback_ptr();
		}
#if !defined(TSERIALIO_TIMEOUT_IS_THREADED)
		if (tsio_timercallback_ptr) {
			tsio_timercallback_ptr();
		}
#endif
		if (ieee1394_timercallback_ptr) {
			ieee1394_timercallback_ptr();
		}

        }

	fast_user_prof(ep);

	if (private.p_prf_enabled)
		/* kernel profiling */
		prfintr(ep->ef_epc, ep->ef_sp, ep->ef_ra, ep->ef_sr);
}

extern volatile int r10k_check_count;
extern volatile int r10k_intervene;
extern volatile __uint64_t r10k_progress_diff;
extern volatile int r10k_progress_nmi;

/*
 * how long do we wait between nmi's?
 */
volatile __uint64_t r10k_nmi_diff = 1800000000;

/*
 * Machine-dependent maintenance to perform once per second.
 * 
 * On Everest systems, reset the running time of day variable
 * to the real-time clock, adjusted and trimmed appropriately.
 */
#ifdef DEBUG
time_t lbolt_diff;
#endif
/* called from clock() periodically to see if system time is
 * drifting from the real time clock chip. Seems to take about
 * about 30 seconds to make a 1 second adjustment, Only do if
 * different by 2 seconds or more.  1 second difference often
 * occurs due to slight differences between kernel and chip, 2 to
 *  play safe.
 */
void
chktimedrift(void)
{
	long todtimediff;

	todtimediff = rtodc() - time;
	if((todtimediff > 2) || (todtimediff < -2))
		(void)doadjtime(todtimediff * USEC_PER_SEC);
}

void
onesec_maint(void)
{
	void set_pwr_fail(uint p);

	tlb_onesec_actions();
	sec_countdown = HZ;

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
#ifdef DEBUG
	lbolt_diff = get_realtime_ticks() - lbolt;
#endif
	if (minute_countdown == 0)
	{
		minute_countdown = 60;
#if defined(EVEREST) || defined(SN0)
		if (record_uptime) 
			set_pwr_fail((lbolt/HZ)/60); /*record time in minutes */
#endif
	}
	else
		minute_countdown--;

#if defined (SN0)
	/* Need to clean hubii merge cache entries so we don't run out of
	 * CRB entries.
	 */

	if (cpuid() == cnodetocpu(cnodeid()))
		hub_merge_clean(COMPACT_TO_NASID_NODEID(cnodeid()));
#endif
}


#if defined (SN0)
#if defined (HUB_II_IFDR_WAR)
/*
 * Every second go and change the timeout compare register in
 * Hub to prevent the DMAs from timing out.
 * Only let one CPU per hub do this.
 */
void
kick_the_hub(void)
{
    if (!nodepda->hub_dmatimeout_need_kick)
	return;
    
    if (!lbolt)
	return;

#if defined(HUB_II_IFDR_WAR)
    if (WAR_II_IFDR_ENABLED) {
	if (cpuid() == cnodetocpu(cnodeid())) {
	    if (nodepda->hub_ifdr_check_count-- <= 0) {
		if (hub_ifdr_check_value(cnodeid()))
		    nodepda->hub_ifdr_needs_fixing = 1;
		if (!nodepda->hub_ifdr_needs_fixing) {
		    nodepda->hub_ifdr_check_count = hub_ifdr_war_freq;
		    return;
		}
			
		/*
		 * If someone's holding it, it probably means a
		 * BTE transfer is taking place.  The BTE will
		 * do the kick for us.
		 */
		if (nested_spintrylock(&(nodepda->hub_dmatimeout_kick_lock))) {
		    /* returns 0 if it succeeds */
		    if (!hub_dmatimeout_kick(cnodeid(), 0)) {
			nodepda->hub_ifdr_needs_fixing = 0;
			nodepda->hub_ifdr_check_count = hub_ifdr_war_freq;
		    }
		    nested_spinunlock(&(nodepda->hub_dmatimeout_kick_lock));
		}
	    }
	} /* if (cpuif...) */
    } /* if (WAR_II_IFDR_ENABLED) */
#endif

}
#endif /* defined (HUB_II_IFDR_WAR) */
#endif /* SN0 */

/*
 * Machine-dependent per-tick activities, called once every
 * clock tick on *every* processor.
 */
void
tick_actions(void)
{
	/*
	 * Perform any outstanding actions.
	 * XXX This shouldn't be necessary, but we are apparently dropping 
	 * XXX inter-cpu interrupts.
	 */
#ifdef NOT_NEEDED
	if (private.p_todolist != NULL)
		doactions();
#endif
#if	R10000
	/*
	 * Check if any cache errors were logged, and if so, call
	 * the cache error handler to print out the logs. The logs
	 * could not be printed when the exception was taken - it 
	 * may have happened while in the cmn_err routines.
	 */
	if (private.p_cacherr) {
	    extern void ecc_log(void);
	    ecc_log();
	}
#endif
#if IP25
	if (0 == cpuid()) {
	    extern void clear_io4_speculation(void);
	    clear_io4_speculation();
	}
#endif

	/* Notify tlb manager that the clock has ticked */
	tlb_tick_actions();


#if	defined(SN0)

#if	defined (HUB_II_IFDR_WAR)
	if (WAR_II_IFDR_ENABLED)
	    kick_the_hub();
#endif

#if	defined(HUB_ERR_STS_WAR)	
	if (WAR_IS_ENABLED(WAR_EXTREME_ERR_STS_BIT))
	    hub_error_status_war();
#endif

	/*
	 * Perform per-hub actions
	 */

	if (cpuid() == cnodetocpu(cnodeid())) {
		extern void memerror_sbe_intr(void *arg);
		extern void sysctlr_check_reenable(void);

		/* Capture hub statistics once every two seconds */

		nodepda->hubticks--;
		if (!nodepda->hubticks)
			capture_hub_stats(cnodeid(), nodepda);

		if (disable_sbe_polling == 0) {
			if (--nodepda->huberror_ticks == 0) 
		        	probe_md_errors(get_nasid());
		}

		/* Poll for ECC errors that don't generate an interrupt */
		if (disable_sbe_polling == 0) {
			memerror_sbe_intr((void *) (__psunsigned_t) 1);
		}
		sysctlr_check_reenable();
	}

	/*
	 * If we own a router, gather its state.
	 */
	if (private.p_routertick && gather_craylink_routerstats) {
		private.p_routertick--;
		if (!private.p_routertick) {
			nodepda_router_info_t 	*npda_rip;
			
			/* Traverse the linked list of router info
			 * pointers to gather stats 
			 */
			npda_rip = nodepda->npda_rip_first;
			while(npda_rip) {
				/* We need to check if router initialization
				 * has been done already before trying to 
				 * gather stats for the router.
				 */
				if (npda_rip->router_infop)
					capture_router_stats(npda_rip->router_infop);
				npda_rip = npda_rip->router_next;
			}
		}
	}
#endif	/* defined(SN0) */

#if IP30
	/* dump conbuf (only happens on master) */
	flush_console();
#endif
	/*
	 * R10k's (versions 3.0 and less) have an occasional tendency to
	 * hang while executing user code. This problem seems to have been
	 * fixed in 3.1's. 
	 * The following code on Origin's periodically (every 5 minutes) 
	 * checks all the cpus in the system and sees if any cpu has not
	 * updated a clock within the last 3 minutes(!). An nmi is sent
	 * to the hung cpu, and that cpu will either recover or panic the
	 * system.
	 */
#if defined (SN0)
	if (r10k_intervene && (--private.p_r10kcheck_cnt == 0)) {
	    clkreg_t value_RTC;			
	    int i;

	    private.p_r10kcheck_cnt = r10k_check_count;

	    /*
	     * If this cpu was targetted by some other cpu, it means 
	     * we suddenly started making progress again: put this in the
	     * syslog.
	     */
	    if ((r10k_progress_nmi == 0) && private.p_nmi_rtc) {
		    cmn_err(CE_NOTE | CE_CPUID, "!cpu now making progress");
		    private.p_nmi_rtc = 0;
	    }

	    for (i = 0; i < maxcpus; i++) {
		register pda_t 	*npda;
		int j;

		if (i == cpuid()) 
		    continue;

		if (pdaindr[i].CpuId == -1)
		    continue;

		npda = pdaindr[i].pda;
		
		value_RTC = GET_LOCAL_RTC;

		if (!npda->last_sched_intr_RTC ||
		    (value_RTC < npda->last_sched_intr_RTC) ||
		    ((value_RTC - npda->last_sched_intr_RTC) < r10k_progress_diff))
		    continue;

		/*
		 * r10k_intervene == 1 means check for user mode only.
		 */
		if ((r10k_intervene == 1) && (npda->p_kstackflag != PDA_CURUSRSTK))
		    continue;

		if (npda->p_flags & (PDAF_NONPREEMPTIVE | PDAF_ISOLATED))
		    continue;

		/*
		 * If we are not doing the nmi and have already printed a
		 * warning indicating the cpu hung, dont check the cpu again.
		 */
		if ((r10k_progress_nmi == 0) && npda->p_hung_rtc)
		    continue;

		if (atomicAddInt(&npda->p_progress_lck, 1) != 1)
		    continue;

		/*
		 * at this time we are about to nmi the hung processor.
		 * just check if a panic is already in progress and abort
		 * the nmi if that is the case
		 */

		for (j = 0; j < maxcpus; j++) {
		    if (pdaindr[j].CpuId == -1) continue;
		    if (pdaindr[j].pda->p_panicking) 
			break;
		}

                /*
                 * someone is panicking... dont do the nmi.
                 */
                if (j != maxcpus)
                    continue;


		if ((npda->p_nmi_rtc == 0) || (r10k_progress_nmi == 0) ||
		    (value_RTC - npda->p_nmi_rtc > r10k_nmi_diff)) {

		    cmn_err(CE_WARN | CE_CPUID, "Process on cpu %d not making progress", i);
		    npda->p_r10k_ack = cpuid() + 1;
		    npda->p_r10k_master_cpu = cpuid();
		    npda->p_nmi_rtc = value_RTC;
		    npda->p_hung_rtc = npda->last_sched_intr_RTC;

		    if (r10k_progress_nmi)
			SEND_NMI((cputonasid(i)), (cputoslice(i)));
		}
		/*
		 * if we are not doing nmi's, dont release the lock. 
		 */
		if (r10k_progress_nmi)
		    npda->p_progress_lck = 0;
	    }
	}

	part_heart_beater();

#endif	/* SN0 */
}


/*
** Timeout (callout) queue support.  On Everest, there is one 
** timeout queue per processor.  Each timeout queue handles both
** "slow" and "fast" timeouts.  A given timeout entry will be
** handled in c_time "fasthz units" (1ms) from the time the 
** previous entry expires.  The *first* entry in a timeout queue
** is relative to cmp_RTC; that is, it's relative to the
** time-of-day that the timer was last set.
*/

#define HzToFastHz(tim)		(((clkreg_t)(tim)*fasthz)/HZ)
#define FastHzToHz(tim)		(((clkreg_t)(tim)*HZ)/fasthz)
#define CyclesToFastHz(cycles)	((cycles)/(__int64_t)cycles_per_fasthz)
#define CyclesToHz(cycles)	((cycles)/(__int64_t)cycles_per_hz)
#define HzToCycles(tim)		((clkreg_t)(tim)*cycles_per_hz)
#define FastHzToCycles(ticks)	((clkreg_t)(ticks)*cycles_per_fasthz)
#define GetNormalizedRTCvalue()	normalize_RTC_value(GET_LOCAL_RTC)

static int	fasthz_previous;	/* for comparison purposes */
static clkreg_t	cycles_per_fasthz;	/* precompute various useful values */
static clkreg_t	cycles_per_hz;

static void
init_fasthz_variables(void)
{
	/* compute useful should-be-constants once */
	cycles_per_fasthz = (NSEC_PER_SEC/NSEC_PER_CYCLE) / fasthz;
	cycles_per_hz     = (NSEC_PER_SEC/NSEC_PER_CYCLE) / HZ;
	fasthz_previous   = fasthz;		/* but be safe  */
#if CELL_IRIX
	ASSERT(fasthz != 0);
	fastick = USEC_PER_SEC/fasthz; 
#endif
}

/*
 *  Adjust the RTC value to be a multiple of FastHzToCycles(1), for more
 *  accurate interval timers.  The value is truncated back to that multiple,
 *  not rounded. 
 *  This gives callouts a "fasthz" resolution, rather than a 21 nsec 
 *  resolution, but the callout "ticks" were expressed in "fasthz" anyway.
 */
static clkreg_t
normalize_RTC_value(clkreg_t cycles)
{
   	if (fasthz != fasthz_previous)
	    init_fasthz_variables(); 
	return( (cycles/cycles_per_fasthz) * cycles_per_fasthz );
}


/*
 * Set hardware up for a timer interrupt.  Called at splprof.
 */
/*ARGSUSED*/
#if IP30
int
set_timer_intr(processorid_t targcpu, __int64_t new_time, long list)
{
	return _set_timer_intr(targcpu,new_time,list,0,GET_LOCAL_RTC);
}
/*ARGSUSED*/
static int
_set_timer_intr(processorid_t targcpu, __int64_t new_time, long list,
		int atintr, clkreg_t cur_time)
{
#else
int
set_timer_intr(processorid_t targcpu, __int64_t new_time, long list)
{
	clkreg_t cur_time = GET_LOCAL_RTC;
#endif	/* IP30 */
	__int64_t tmp_time;
#ifdef EVEREST
	int	ev_slot, ev_cpu;
#endif
#ifdef IP25
	int cflockspl;
#endif
	int rc = 1;

	ASSERT(targcpu >= 0 && targcpu < maxcpus);

	tmp_time = new_time - cur_time;

	/*
	 * This only happens if someone does a timeout (TIMEPOKE_NOW).
	 * or if the interrupt that we are setting should have already
	 * happened or is about to happen. 
	 */
	if ((new_time == 0) || (tmp_time <= 0)) {
		if (targcpu == cpuid()) {
			timepoke();	/* do it directly */
		} else {		/* else tell other cpu to do it */
			cpuaction(targcpu, (cpuacfunc_t)timepoke, A_NOW);
		}
		rc = 0;
#if IP30	/* have to get next timeout/click set-up. In the case
		 * of a NONPREEMPTIVE processor, we just return.
		 * The timepoke() done earlier will eventually 
		 * setup the next timer intr if needed.
		 */
		if (private.p_flags & PDAF_NONPREEMPTIVE) {
			return (rc);
		} else {
			new_time = private.p_clock_tick;
		}
#else
		return(rc);
#endif
	}
#ifdef EVEREST
	/*
	 * The EVEREST compare reg is 32-bits, and the 21-nsec count register 
	 * rolls over approx. every 90 seconds.  That means that software can
	 * set up for a hardware interrupt in 90sec+10usec, and the hardware
	 * will really interrupt in 10usec.  Bad news!  So solve this in the
	 * simplest possible way by breaking down large time intervals into
	 * shorter intervals that can't possibly cause this problem.
	 */

	if(tmp_time > RTC_SLOW_RATE)
	    new_time = normalize_RTC_value(RTC_SLOW_RATE) + cur_time;

	/*
	 *  Write the RTC COMPARE register with doubleword writes, but
	 *  effectively writing one byte per doubleword register (the
	 *  least significant byte).
	 */
	ev_slot = cpuid_to_slot[targcpu];	/* extract for efficiency */
	ev_cpu  = cpuid_to_cpu [targcpu] << EV_PROCNUM_SHFT;
	EV_CONFIG_LOCK(cflockspl);
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG0, (u_int32_t)new_time);
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG1, (u_int32_t)new_time>> 8);
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG2, (u_int32_t)new_time>>16);
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG3, (u_int32_t)new_time>>24);
	EV_CONFIG_UNLOCK(cflockspl);
#elif SN
        {
	    nasid_t nasid = cputonasid(targcpu);

	    if (cputoslice(targcpu))
		REMOTE_HUB_S(nasid, PI_RT_COMPARE_B, new_time);
	    else
		REMOTE_HUB_S(nasid, PI_RT_COMPARE_A, new_time);
	}
#elif IP30

	ASSERT(issplprof(getsr()));

	/* Hang it off the count/compare intr.  Test fclock first as
	 * if it's running (likely on master with audio) we should
	 * run less code.  Try to line up any timeout (new_time)
	 * with the [f]clock ticks if close (within 50us) to a
	 * [f]clock tick boundary to avoid unneeded interrupts and
	 * count/compare mucking near race conditions. Else, align
	 * timeouts to TIMEOUT_ALGNMT_BDRY.
	 */

	/* We don't want to check fclock on NONPREEMPTIVE cpus. Since
	 * private.p_fclock_tick == 0 for such cases, this check is implicit.
	 */
	if (private.p_fclock_tick) {
		if (private.p_fclock_tick == 1) {
			/* Just started the clock.  Start up as soon as
			 * possible while aligned with p_clock_tick.
	 		 */
			private.p_fclock_tick = private.p_clock_tick;
			while (private.p_fclock_tick >
			       (cur_time + CLK_FCLOCK_FAST_FREQ)) {
				private.p_fclock_tick -= CLK_FCLOCK_FAST_FREQ;
			}
		}
		/* Align with fclock_tick only if very close (within 50us)
		 * to fclock_tick boundary. Else, align with the nearest
		 * TIMEOUT_ALGNMT_BDRY boundary to get better resolution
		 * on timeouts.
		 */
		if ((new_time + 50 * CYCLE_PER_USEC) > private.p_fclock_tick) {
			new_time = private.p_fclock_tick;
		} else {
			/* round up to nearest TIMEOUT_ALGNMT_BDRY boundary */
			new_time = roundup(new_time, TIMEOUT_ALGNMT_BDRY);
		}
	}
	/* Align with clock_tick only if very close (within 50us) to
	 * clock_tick boundary. Else, align with the nearest
	 * TIMEOUT_ALGNMT_BDRY boundary to get better resolution on
	 * timeouts.
	 */
	else if (((new_time + 50 * CYCLE_PER_USEC) > private.p_clock_tick)
		 && (!(private.p_flags & PDAF_NONPREEMPTIVE))) {
		new_time = private.p_clock_tick;
	} else { /* processor is NONPREEMPTIVE or new_time is not close
		  * to [f]clock tick boundary.
		  */
		new_time = roundup(new_time, TIMEOUT_ALGNMT_BDRY);
	}
	
	if (!(private.p_flags & PDAF_NONPREEMPTIVE)) {
		if (new_time > private.p_next_intr) {
			/* If moving forward, we can skip and wait for p_next_intr
			 * unless we are from an interrupt/forced setting, which
			 * need to ensure we move on, and clear IP8.
			 */
			if (atintr) {
				clkreg_t delta = new_time - private.p_next_intr;
				CLKASSERT(rem_ctime(delta) == 0);
				tmp_time = resetcounter_val(cvt_ctime(delta));
				update_next_intr(new_time,cur_time,tmp_time);
			}
		}
		else {
			clkreg_t delta = private.p_next_intr - new_time;
	
			/* If we are not moving forward, we should not have come
			 * from an interrupt context.  If the times equal (common
			 * when launching timeouts) we do not need a change.  We
			 * call trimcompare() here to safely pull compare back
			 * by the passed delta, w/o having to factor in set-up time.
			 */
			CLKASSERT(atintr == 0);
			if (new_time != private.p_next_intr) {
				CLKASSERT(rem_ctime(delta) == 0);
				trimcompare(cvt_ctime(delta));
				update_next_intr(new_time,cur_time,4);
			}
		}
	} else {
		clkreg_t delta;

		delta = new_time - cur_time;
		delta = cvt_ctime(delta);
		/* If delta is greater than the wrap-around time for the
		 * 32-bit cpu compare register, then set delta to
		 * 0xffffffff. This will result in an interrupt being
		 * generated after approx. 40 sec. (for 195Mhz T5). This
		 * will continue till we reach a delta less than the
		 * wrap-around time. This is the simplest way to handle
		 * this issue keeping in mind the hardware limitations.
		 * Besides, an extra interrupt approx. every 40s. is
		 * not too bad.
		 */
		if (delta > 0xffffffff)
			delta = 0xffffffff;
		resetcounter_val(delta);
		update_next_intr(new_time, cur_time, tmp_time);
	}
#else
<<BOMB>>
#endif
#if SABLE
	pseudo_CMPREG = new_time;
#endif
	/*
	 * Now read back another config register (can't read COMPARE) to
	 * guarantee that the new COMPARE value has arrived and is ready
	 *
	 * Don't need to do this for TFP since uncached stores are
	 * sychronous and complete before starting execution of the next
	 * instruction.
	 */
#if defined(IP19) || defined(IP25)
	wbflush();
#endif
#if EVEREST || SN0
	if (GET_LOCAL_RTC > new_time) {
		/*
		 * There is a timing window:  we were very close to a fasthz
		 * interval boundary as we tried to trigger an interrupt on
		 * the very next fasthz boundary -- but we rolled past that
		 * time before we could write the COMPARE register.
		 * So just handle the timeout right away.
	 	 */
		if (targcpu == cpuid()) {
			timepoke();	/* do it directly */
		} else {		/* else tell other cpu to do it */
			cpuaction(targcpu, (cpuacfunc_t)timepoke, A_NOW);
		}
		rc = 0;
	}
#endif
	return rc;
}

/*
 *  Because Everest uses only per-processor callout queues, and not a
 *  central fastcatodo fast queue, we use an architecture-specific
 *  routine that dotimeout() calls to figure out which queue to use.
 */
struct callout *
timeout_get_queuehead(long list, struct callout *pnew)
{
	struct callout *queuehead;
	clkreg_t temp_tim;

	/*
	 * C_FAST implies C_NORM on Everest platforms.
	 */
	queuehead = CI_TODO(&CALLTODO(pnew->c_cpuid));

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
	if (++queuehead->c_cid == 0)
		++queuehead->c_cid;
	pnew->c_cid = queuehead->c_cid;

	/*
	 * All timeout times get converted absolute RTC cycle counts 
	 */
	if (pnew->c_time <= 0) {		/* special-case adjustment */
		/* tell set_timer_intr(): "NOW!" if needed */
		if (pnew->c_time == TIMEPOKE_NOW) {
			pnew->c_time = 0;
			return (queuehead);
		} else
			pnew->c_time = 1;
	}

	/*
	 * We will convert the units to cycles and then if needed
	 * add the current time to the requested time
	 * to get an absolute time to set the timer to.
	 */
	switch (list) {
	    case C_FAST_ISTK:
	    case C_FAST:
		temp_tim = pnew->c_time;
		if (temp_tim > 0x7fffffff) {
	    		/* Caller asking for >1 year (for fasthz=1200/sec); */
	    		/* so presume error and use max long.  		    */
		    temp_tim = 0x7fffffff;
		}
		pnew->c_time = FastHzToCycles(temp_tim) +
		    GetNormalizedRTCvalue();
		break;
	    case C_NORM_ISTK:
	    case C_NORM:
		temp_tim = pnew->c_time;
		if (temp_tim >  0x7fffffff) {
	    		/* Caller asking for >1 year ); */
	    		/* so presume error and maxlong */
	    		temp_tim =  0x7fffffff;
		}
		pnew->c_time = HzToCycles(temp_tim) +
		    GetNormalizedRTCvalue();
		break;
	    case C_CLOCK_ISTK:
	    case C_CLOCK:

		    /* Already in right format */
		break;
	}

	return (queuehead);
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
    callout_info_t *callout_info;
    int	i;
    
    for (i=0; i<maxcpus; i++) {
	if (!cpu_enabled(i))
		continue;
	callout_info = &CALLTODO(i);
	if ((CI_FLAGS(callout_info) & CA_ENABLED) == 0)
		continue;

	if (rv = do_chktimeout_tick(callout_info, id, fun, arg)) {
		/*
		 * Find out how long in cycles until timeout
		 */
	    rv = rv - GET_LOCAL_RTC;
		/*
		 *  Everest only has one list, not NORM and FAST,
		 *  but the caller thinks there are two lists, so
		 *  return what the caller expects:  either a hz
		 *  value or a fasthz value.
		 */
	    switch(list) {
		case C_NORM_ISTK:
		case C_NORM:
		    rv = CyclesToHz(rv);
		    break;
		case C_FAST_ISTK:
		case C_FAST:    
		    rv = CyclesToFastHz(rv);
		    break;
		case C_CLOCK_ISTK:
		case C_CLOCK:
		    break;
	    }
	    if (rv <= 0)
		rv = 1;
	    break;
	}
    }
    return(rv);
}

/*
 * For compatibility with timer.c machines.
 */
toid_t
fast_timeout(void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;
	va_list ap;

	va_start(ap, tim);

	retval = dotimeout(cpuid(), fun, arg, tim, callout_get_pri(), C_FAST, ap);

	va_end(ap);

	if (retval == NULL)
		cmn_err(CE_PANIC,
			"Timeout table overflow.\n Tune ncallout to a higher value.");

	return(retval);
}

toid_t
fast_timeout_nothrd(void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;
	va_list ap;

	va_start(ap, tim);

	retval = dotimeout(cpuid(), fun, arg, tim, 0, C_FAST_ISTK, ap);

	va_end(ap);

	if (retval == NULL)
		cmn_err(CE_PANIC,
			"Timeout table overflow.\n Tune ncallout to a higher value.");

	return(retval);
}


/*
 *  For a given callout entry c_time value, return the equivalent in hz.
 *  We always return a value >0, because a zero value would confuse
 *  the higher level routines which might use the return value as a "found
 *  callout entry" flag, and a negative value would be even more confusing.
 */
/* ARGSUSED */
int
callout_time_to_hz(__int64_t callout_time, int callout_cpu, int fast)
{
	int retval;

	ASSERT(callout_cpu >= 0  &&  callout_cpu < maxcpus);
	/*
	 * As our c_time's are in absolute RTC cycles
	 * Then all we need to do is a simple subtraction
	 */
	retval = CyclesToHz(callout_time - GET_LOCAL_RTC);
	if (retval <= 0)
		retval = 1;

	return(retval);
}

/*
 * Called to handle the Everest clock comparator interrupt.
 * These interrupts are set to go off at MOST about once per millisecond.
 */
void
timein_body(callout_info_t *cip)
{
	struct callout *phead, *p1;
	clkreg_t value_RTC;
	int s;
	lock_t *ci_listlock;

	phead = CI_TODO(cip);
	ci_listlock = &CI_LISTLOCK(cip);
	for(;;) {
		s = mutex_spinlock_spl(ci_listlock, splprof);
		p1 = phead->c_next;
		if (p1 == NULL) {
			/*
			 * Disable the timeout interrupt by writing to the
			 * first byte.  Writing a new value to all four bytes
			 * will reenable the interrupt.
			 */
			DISABLE_TMO_INTR();
#if SABLE
			pseudo_CMPREG = last_CMPREG_intr = 0;
#endif
			mutex_spinunlock(ci_listlock, s);
			break;
		}

		/* Get the current value of the RTC */
		value_RTC = GET_LOCAL_RTC;
#if DEBUG
		timein_last_called = value_RTC;
#endif
#if 0
		if((value_RTC - p1->c_time) > 100)
		    printf("timein delayed by %ld cycles\n",value_RTC - p1->c_time );
#endif

		/* Have enough fasticks gone by since timer was set? */
		if (value_RTC < p1->c_time) { 
			/* not yet ready for next item on queue */
			if (set_timer_intr(CI_CPU(cip), p1->c_time, C_NORM)) {
				mutex_spinunlock(ci_listlock, s);
				break;
			}
		}

		/* Remove head of queue and process it */
		phead->c_next = p1->c_next;

		/* Call the timeout routine */
		if (C_IS_ISTK(p1->c_flags)) {
			mutex_spinunlock(ci_listlock, s);
			timein_entry(p1);
		} else {
			timein_entry_ithrd(cip, p1, s); /* unlocks ci_listlock */
		}
	}
}

/*
 * Actual interrupt handler
 */
void
timein(void)
{
	void frs_handle_timein(void);

	/*
	 * Frame Scheduler
	 */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_RTCCOUNTER_INTR,
			 NULL, NULL, NULL, NULL);

#ifndef IP30	/* IP30 does not handle this yet */
	if (private.p_frs_flags) {
		frs_handle_timein();
	} else
#endif
	{
		if (fasthz != fasthz_previous)
			init_fasthz_variables();

		acktmoclock();		/* ack hardware timer interrupt */
		acksoftclock();		/* ack software interrupt */

		timein_body(&CALLTODO(cpuid()));
	}

	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT,
			 TSTAMP_EV_RTCCOUNTER_INTR, NULL, NULL, NULL);
}

/*
 * Migration of timeouts
 */
volatile toid_t *add_migrated_timeout(toid_t oldid, long sec, void *);

void
migrate_timeouts(processorid_t fromcpu, processorid_t tocpu)
{
    struct callout* phead;
    struct callout* p;
    struct callout tempco;
    toid_t timeout_val;
    int s, s1;
    volatile toid_t *newid;
    callout_info_t *callout_info_from;

    /* Do nothing if same cpu. This can happen if a user marks the
     * clock cpu as restricted 
     */
    if (fromcpu == tocpu)
	    return;
    /*
     * Disable the timeout interrupt by writing to the
     * first byte.  Writing a new value to all four bytes
     * will reenable the interrupt. This assumes that the caller
     * does a mustrun to the from cpu before they call here.
     */
    DISABLE_TMO_INTR();

    callout_info_from = &CALLTODO(fromcpu);

    phead = CI_TODO(callout_info_from);

    for (;;) {
	void *allocate_migrate_timeout(void);
	void free_migrate_timeout(void *);
	void *ptr;
	/*
	 * Allocate migrate struct before grabbing the lock as it
	 * might sleep.
	 */
	ptr = allocate_migrate_timeout();
	s1 = splhi();
	s = mutex_spinlock_spl(&CI_LISTLOCK(callout_info_from), splprof);
	p = phead->c_next;
	
	if (p == NULL) {
	    mutex_spinunlock(&CI_LISTLOCK(callout_info_from), s);
	    splx(s1);
	    /*
	     * Release unused migrate_timeout struct
	     */
	    free_migrate_timeout(ptr);
	    break;
	}
	phead->c_next = p->c_next;
	tempco = *p;
	mutex_spinunlock(&CI_LISTLOCK(callout_info_from), s);
	callout_free(p);
	newid = add_migrated_timeout(tempco.c_id , 
				     (tempco.c_time - GET_LOCAL_RTC)/CYCLE_PER_SEC,
				     ptr);

	if (C_IS_ISTK(tempco.c_flags))
		timeout_val = clock_prtimeout_nothrd(tocpu,
					      tempco.c_func,
					      tempco.c_arg,
					      tempco.c_time,
					      tempco.c_arg1,
					      tempco.c_arg2,
					      tempco.c_arg3);
	else
		timeout_val = clock_prtimeout(tocpu,
				      tempco.c_func,
				      tempco.c_arg,
				      tempco.c_time,
				      tempco.c_pl,
				      tempco.c_arg1,
				      tempco.c_arg2,
				      tempco.c_arg3);
	*newid = timeout_val;
	splx(s1);
    }
}

/* system call to adjust the time trimmer
 *
 * The numbers here are chosen so that time never goes backwards.
 * By limiting the adjustment to less the amount we have just incremented
 * time above, we keep time advancing, although we also limit the maximum
 * adjustment to less than 0.3%.
 */
int
settimetrim(int val)
{
	int s;

	if (!_CAP_ABLE(CAP_TIME_MGT))
		return EPERM;

	if (val < -MAXTRIM_NSEC || val > MAXTRIM_NSEC)
		return EINVAL;

	s = mutex_spinlock_spl(&timelock, splprof);

	timetrim = val;

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


/*
 * Throttles on how fast the time can be adjusted.  If the adjustment is
 * tiny (less than a 10th of a tick), double (or halve) the rate of change
 * for that amount of time.  If the adjustment is moderate (less than a
 * second), spread the adjustment out over 3 minutes.  If the adjustment
 * is large (more than 1 second), we'll take our time by slightly altering
 * (by 1/32nd) the pace of time.
 * 
 */
#define TINYADJ (USEC_PER_SEC/HZ/10)	/* 10th of a tick */
#define SMALLADJ USEC_PER_SEC		/* 1 second */
#define SMALLADJ_DELTA ((long long)3*60*CYCLE_PER_SEC)	/* spread over 3 min */
#define BIGADJ_FACTOR 32		/* change <= 1 usec every 32 usecs */
long
doadjtime(long usecs)
{
	long long  new_adjtime_delta; /* #cycles new adj will take place over */
	clkreg_t	value_RTC;    /* current value of RTC */
	long long  remaining_delta;   /* #cycles remaining in this adj period */
	long	   oval;	      /* #usecs remaining from previous adj */
	int	   negative, s;

	negative = (usecs < 0);
	if (negative)
		usecs = -usecs;

	if (usecs < TINYADJ) /* do tiny adjustments quickly (double clock) */
		new_adjtime_delta = usecs * (1000/NSEC_PER_CYCLE);
	else if (usecs <= SMALLADJ) /* spread out modest ones over couple min */
		new_adjtime_delta = SMALLADJ_DELTA;
	else			/* take forever with giants */
		new_adjtime_delta =  ((long long)usecs*BIGADJ_FACTOR) 
				   * (1000/NSEC_PER_CYCLE);

	if (negative)
		usecs = -usecs;


	/*
	 * Ensure timebase adequately reflects previous adjustments
	 * up to this moment.
	 */
	s = splprof();
	__nanotime();
	value_RTC = timebase_RTC;

	nested_spinlock(&timelock);

	ASSERT(issplprof(getsr()));
	timedata_newgen++;
	ASSERT(timedata_newgen != timedata_gen);
	__synchronize();

	remaining_delta = adjtime_RTC + adjtime_delta - value_RTC;
	if (remaining_delta > 0)
		oval = ((remaining_delta * adjtime_nanos) / adjtime_delta)
			/ 1000;
	else
		oval = 0;

	adjtime_nanos = (long long)usecs * 1000;

	if (new_adjtime_delta < 0)
		adjtime_delta = 0;
	else
		adjtime_delta = new_adjtime_delta;
	adjtime_RTC   = value_RTC;

	__synchronize();
	timedata_gen = timedata_newgen;

	mutex_spinunlock(&timelock, s);

	return oval;
}

/*
 * Set timebase with monotonically non-decreasing time, based off the RTC.
 * The time set is modified according to adjtime and timetrim.
 */
static void
__nanotime(void)
{
	clkreg_t	value_RTC;	/* current value of RTC */
	long long	cycles_RTC;	/* #cycles since timebase last sync */
	long long	trim_nsec;	/* #nanos req because of trimming */
	long long	adj_nsec;	/* #nanos req because of adjustment */
	unsigned  	time_sec = 0;	/* #secs passed since last timebase */
	long long	time_nsec;	/* #nanos passed since last timebase */
	int s;

	/*
	 * timelock serves two purposes: First, it guarantees that adjtime
	 * variables are consistent.  Second, it guarantees that times will 
	 * be unique, even across simultaneous invokations of nanotime on 
	 * different processors.  We just have to hold the lock long
	 * enough to guarantee that value_RTC will be different.
	 */
	s = mutex_spinlock_spl(&timelock, splprof);

	ASSERT(issplprof(getsr()));
	timedata_newgen++;
	ASSERT(timedata_newgen != timedata_gen);
	__synchronize();

	value_RTC = GET_LOCAL_RTC;
	if (timebase_RTC == 0) {
		/* things haven't completely initialized yet */
		timebase_RTC = value_RTC;
		timebase.tv_sec  = 0;
		timebase.tv_nsec = 0;
		__synchronize();
		timedata_gen = timedata_newgen = 0;
		mutex_spinunlock(&timelock, s);
		return;
	}

	cycles_RTC = value_RTC - timebase_RTC;

	trim_nsec = (cycles_RTC * timetrim) / CYCLE_PER_SEC;

	if ( value_RTC < (adjtime_RTC + adjtime_delta) )
		adj_nsec = (cycles_RTC * adjtime_nanos) / adjtime_delta;
	else
		adj_nsec = adjtime_nanos;

	/*
	 * Adjust the adjustment parameters so we won't do
	 * redundant time adjustments.  Make calculations locally
	 * first so we don't get strange, transient globals.
	 */
	if (adjtime_nanos != 0) {
		long long a_nanos = adjtime_nanos - adj_nsec;
		long long a_delta = adjtime_delta - cycles_RTC;
		clkreg_t a_RTC = value_RTC;

		if ( (adj_nsec >= 0  &&  a_nanos <= 0)  ||
		     (adj_nsec <  0  &&  a_nanos >= 0) ) {
			/* all done! */
			a_nanos = 0;
			a_RTC   = 0;
			a_delta = 0;
		} else
		if (a_delta < 0)
			a_delta = 0;

		adjtime_delta = a_delta;
		adjtime_nanos = a_nanos;
		adjtime_RTC   = a_RTC;
	}

	time_nsec =   (cycles_RTC * NSEC_PER_CYCLE) /* from timebase to now */
		    + adj_nsec 			    /*  plus adjustment     */
		    + trim_nsec			    /*  plus trim           */
		    + timebase.tv_nsec;		    /*  plus base nanosecs  */

#ifdef	DEBUG
	if (time_nsec < timebase.tv_nsec) {
		printf("XXX CPU %d, clock moving backwards: timebase.tv_nsec=%ld time_nsec=%ld adj_nsec=%ld, trim_nsec=%ld\n", cpuid(), (long) timebase.tv_nsec, (long) time_nsec, (long) adj_nsec, (long) trim_nsec);
	}
#endif	/* DEBUG */

	while ( time_nsec >= NSEC_PER_SEC ) {	    /* guarantee < 1 second */
		time_nsec -= NSEC_PER_SEC;
		time_sec++;
	}

	timebase_RTC = value_RTC;
	timebase.tv_nsec = time_nsec;
	if (time_sec) {
		timebase.tv_sec += time_sec;
		time = timebase.tv_sec;
	}

	__synchronize();
	timedata_gen = timedata_newgen;

	mutex_spinunlock(&timelock, s);
}

/*
 * Return a monotonically non-decreasing time, based off the RTC.
 * The time returned is modified according to adjtime and timetrim.
 * Uniqueness is not ensured.
 */
#ifdef	DEBUG
int check_monotony = 0;
#endif	/* DEBUG */

void
nanotime_syscall(timespec_t *tvp)
{
	clkreg_t	value_RTC;	/* current value of RTC */
	long long	cycles_RTC;	/* #cycles since timebase last sync */
	long long	trim_nsec;	/* #nanos req because of trimming */
	unsigned  	time_sec;	/* #secs passed since last timebase */
	long long	time_nsec;	/* #nanos passed since last timebase */
	int s;

	uint		snap_sec, snap_nsec;	/* snapshots of timebase */
	long long	snap_adj_delta;	/* snapshop of adjtime delta */
	long long	snap_adj_nanos;	/* #nanos req because of adjustment */
	clkreg_t	snap_timebase_RTC, snap_adjtime_RTC; /* snapshops of various RTC values */
	int		snap_gen;

	s = splprof();	

	/* Get a consistent snapshot of the data */
	do {
		snap_gen = timedata_gen;
		__synchronize();

		snap_timebase_RTC = timebase_RTC;
		snap_sec = timebase.tv_sec;
		snap_nsec = timebase.tv_nsec;

		value_RTC = GET_LOCAL_RTC;

		snap_adjtime_RTC = adjtime_RTC;
		snap_adj_delta	= adjtime_delta;
		snap_adj_nanos	= adjtime_nanos;

		__synchronize();
	} while (snap_gen != timedata_newgen);

	cycles_RTC = value_RTC - snap_timebase_RTC;

	/* 
	 * the time portion between main clock ticks has to be adjusted
	 * and trimmed also.
	 */
	trim_nsec = (cycles_RTC * timetrim) / CYCLE_PER_SEC;

	/*
	 * Note that these are 64-bit values, so value_RTC won't wrap.
	 */
	if ((value_RTC < (snap_adjtime_RTC + snap_adj_delta)) &&
	    snap_adj_delta)
		snap_adj_nanos = ((cycles_RTC * snap_adj_nanos) /
				  snap_adj_delta);

	time_nsec =  (cycles_RTC * NSEC_PER_CYCLE)  /* from timebase to now */
		    + snap_adj_nanos		    /*  plus adjustment     */
		    + trim_nsec			    /*  plus trim           */
		    + snap_nsec;		    /*  plus base nanosecs  */

	time_sec = 0;
	while ( time_nsec >= NSEC_PER_SEC ) {	    /* guarantee < 1 second */
		time_nsec -= NSEC_PER_SEC;
		time_sec++;
	}

	tvp->tv_sec = snap_sec + time_sec;
	tvp->tv_nsec = time_nsec;

#ifdef	DEBUG
	if (check_monotony) {
		static timespec_t last_time[MAXCPUS] = { { 0, 0 }, };
		int cpu;

		cpu = cpuid();
		if (tvp->tv_sec < last_time[cpu].tv_sec ||
		    (tvp->tv_sec == last_time[cpu].tv_sec &&
		     tvp->tv_nsec < last_time[cpu].tv_nsec)) {
			printf("Time on CPU %d going backwards: last=(%d,%d), now=(%d,%d), trim_nsec=%ld, snap_adj_nanos=%ld\n", cpu, last_time[cpu].tv_sec, last_time[cpu].tv_nsec, tvp->tv_sec, tvp->tv_nsec, (long) trim_nsec, (long) snap_adj_nanos);
		}
		last_time[cpu] = *tvp;
	}
#endif	/* DEBUG */

	if (tvp->tv_sec > time) {
		int s2;
		s2 = mutex_spinlock_spl(&timelock, splprof);
		if (tvp->tv_sec > time) {
			time = tvp->tv_sec;
		}
		mutex_spinunlock(&timelock, s2);
	}

	splx(s);
}

/*
 * Return a monotonically non-decreasing time, based off the RTC.
 * The time returned is *not* modified according to adjtime and timetrim.
 * This is a "lightweight" time-of-day for general internal use by the
 * kernel.
 */
void
nanotime(timespec_t *tv)
{
	long long	cycles_RTC;	/* #cycles since timebase last sync */
	register uint   snap_sec, snap_nsec;
	clkreg_t	snap_timebase_RTC;
	int		snap_gen;
	long long	time_nsec;

	if (timebase_RTC == 0) {
		/* things haven't completely initialized yet */
		tv->tv_sec = tv->tv_nsec = 0;
		return;
	}

	/*
	 *  To avoid having to grab a global lock, just snapshot the
	 *  interesting variables, then ensure that they haven't changed
	 *  while we were in the middle of reading them.
	 */
	do {
		snap_gen = timedata_gen;
		__synchronize();
		snap_timebase_RTC = timebase_RTC;
		snap_sec          = timebase.tv_sec;
		snap_nsec         = timebase.tv_nsec;
		__synchronize();
	} while (snap_gen != timedata_newgen);


	cycles_RTC = GET_LOCAL_RTC - snap_timebase_RTC;
	time_nsec =   (cycles_RTC * NSEC_PER_CYCLE) /* from timebase to now */
		    + snap_nsec;		    /*  plus base nanosecs  */

	while ( time_nsec >= NSEC_PER_SEC ) {	    /* guarantee < 1 second */
		time_nsec -= NSEC_PER_SEC;
		snap_sec++;
	}

	tv->tv_sec  = snap_sec;
	tv->tv_nsec = time_nsec;
}

#if _MIPS_SIM == _ABI64
/* Used by snoop code.  Can't change the size of snoopheader while board
 * firmware, like ipg, knows about it.
 */
void
irix5_microtime(struct irix5_timeval *tvp)
{
	timespec_t	localtime;

	nanotime_syscall(&localtime);		/* use fully adjusted time */
	tvp->tv_sec  = localtime.tv_sec;
	tvp->tv_usec = localtime.tv_nsec / 1000;
}
#endif	/* _MIPS_SIM == _ABI64 */

void
microtime(struct timeval *tvp)
{
	timespec_t	localtime;

	nanotime_syscall(&localtime);		/* use fully adjusted time */
	tvp->tv_sec  = localtime.tv_sec;
	tvp->tv_usec = localtime.tv_nsec / 1000;
}


/*
 * set the time for the several system calls that like that game.
 * (local time only - no distribution)
 */
void
local_settime(long sec, long usec)
{
	int s = mutex_spinlock_spl(&timelock, splprof);

	while ( usec >= USEC_PER_SEC ) {   /* be careful about usec value */
		usec -= USEC_PER_SEC;
		sec++;
	}

	ASSERT(issplprof(getsr()));
	timedata_newgen++;
	ASSERT(timedata_newgen != timedata_gen);
	__synchronize();
	
	timebase_RTC = GET_LOCAL_RTC;
	time = sec;
	timebase.tv_sec  = sec;
	timebase.tv_nsec = usec*1000;
	adjtime_RTC = 0;
	adjtime_delta = 0;
	adjtime_nanos = 0;

	__synchronize();
	timedata_gen = timedata_newgen;

	mutex_spinunlock(&timelock, s);
}

/*
 * distributed settime - set local time then call VHOST
 */
void
settime(long sec, long usec)
{
	local_settime(sec, usec);
	VHOST_SET_TIME(cellid());
}


/*
 * Adjust the time to UNIX based time
 * called when the root file system is mounted with the time
 * obtained from the root. Call inittodr to try to get the actual 
 * time from the battery backed-up real-time clock.
 */
void
clkset(register time_t	oldtime)
{
	inittodr(oldtime);
}

/*
 * For compatibility with timer.c systems.  Not needed in clksupport.c, since
 * there is no real fast timer.  The RTC is used instead.
 */
void
enable_fastclock(void)
{
}

/*
 * Figure out max # of passes in clean_dcache() clean_icache()
 * before allowing interrupts back on.
 * We want to preempt every 250us.
 * On Everest, the only flushing we'll do is when we write 
 * instruction space.
 * XXX- currently, the only effect of cache_preempt_limit
 * is to set up private.cpufreq_cycles, if it is not already
 * set up, as a side effect of calling findcpufreq_cycles().
 */
void
cache_preempt_limit(void)
{
#ifndef IP30		/* private.cpufreq_cycles has already been done */
	/*REFERENCED*/
	long freq = findcpufreq_cycles();
#endif
}

/*
 * compute an accurate "lbolt" using timebase_RTC
 */
time_t
get_realtime_ticks(void)
{
	long long elapsed_cycles;
	elapsed_cycles = GET_LOCAL_RTC - realtime_base_RTC;
	return( CyclesToHz(elapsed_cycles) );
}

/*
 * convert a timespec to a RTC delta.  This is used for posix timers in
 * os/ptimer.c
 */
__int64_t
tstoclock(struct timespec *tv, int type)
{
	__int64_t total_time;

	/* convert the seconds to ns */
	total_time = (__int64_t)tv->tv_sec * NSEC_PER_SEC;

	/* convert the useconds to ns */
	total_time += (__int64_t)tv->tv_nsec;

	/*
	 * Now that it all has been converted to NSECs do the
	 * division once.  We truncate, but only once. The
	 * addition of one is to compensate for the truncate as
	 * timers are not allowed to return early according to
	 * POSIX, so an extra 21ns is added to be safe.
	 */
	total_time = (total_time / NSEC_PER_CYCLE) + 1;

	/*
	 * If the time requested was absolute then add in the
	 * current RTC
	 */
	if (type == TIME_ABS)
		total_time += GET_LOCAL_RTC;

	return (total_time);
}

/*
 * convert a timeval to a RTC delta. This is used for itimers in os/time.c
 */
__int64_t
tvtoclock(struct timeval *tv, int type)
{
	__int64_t total_time;

	/* convert the seconds to ns */
	total_time = (__int64_t)tv->tv_sec * NSEC_PER_SEC;

	/* convert the useconds to ns */
	total_time += (__int64_t)tv->tv_usec *  NSEC_PER_USEC;

	/*
	* Now that it all has been converted to NSECs do the
	* division once.  We truncate, but only once. The
	* addition of one is to compensate for the truncate as
	* timers are not allowed to return early according to
	* POSIX, so an extra 21ns is added to be safe.
	*/
	total_time = (total_time / NSEC_PER_CYCLE) + 1;

	/*
	 * If the time requested was absolute then add in the
	 * current RTC
	 */
	if (type == TIME_ABS)
		total_time += GET_LOCAL_RTC;

	return (total_time);
}

void
timepoke(void)
{
#ifndef IP30	/* IP30 does not handle this yet */
	/*
	 * Frame Scheduler
	 */
	if (private.p_frs_flags)
		return;
#endif

	setsoftclock();
}

#if !IP30		/* IP30 vs EVEREST/SN0 routines */
/*
 * Find the frequency (in Hz) of the running CPU
 */
static uint
findcpufreq_cycles(void)
{
	clkreg_t begin_RTC, end_RTC;
	uint    cycles;
	int	s;

	/*  If we know the frequency, then just return it  */
	if (private.cpufreq_cycles)
		return( private.cpufreq_cycles );

#if SABLE
	return( private.cpufreq_cycles = 50000000 );
#else
	/*  Else, compute the frequency directly  */
	s = splhi();
	begin_RTC = GET_LOCAL_RTC;
	_set_count(0);
	
	end_RTC = begin_RTC + CYCLE_PER_SEC;

	/* spin for 1 second */
	while (GET_LOCAL_RTC < end_RTC);
	/* get raw cycles, massage a bit to get a nicer value */
	cycles = _get_count();
#if defined (SN0)
{
	extern uint cpu_cycles_adjust(uint);
	/*
	 * call this for all architectures later on.
	 */
	cycles = cpu_cycles_adjust(cycles);
}
#endif

	private.cpufreq_cycles = cycles;
	splx(s);

	return( cycles );
#endif	/* SABLE */
}


/*
 * find the frequency (in MHz) of the running CPU
 */
int
findcpufreq(void)
{
	return( (findcpufreq_cycles() + 500000) / 1000000 );
}

/*
 * rt clock interrupt handler before clock interrupts are
 * actually enabled (there's no way to shut off the R4000
 * count/compare interrupts)
 */
/*ARGSUSED*/
void
early_counter_intr(eframe_t *ep)
{
	resetcounter(RTSLOW_RATE);	/* clears interrupt */
}

/* delay support for EVEREST/SN0 */
/* For compatibility with non-Everest systems. */
void
delay_calibrate(void)
{
}

/*
 * delay, but first make sure bus is clear, so delay is guaranteed
 * to be relative to bus, not CPU
 */
void
us_delaybus(uint us)
{
	flushbus();
	if (us > 0)
		us_delay(us-1);	/* sub off time for flushbus (approx) */
	return;
}

/*
 * Spin in a tight loop for "us" microseconds.
 */
void
us_delay(uint us)
{
	register clkreg_t stop_RTC;
	register clkreg_t current_RTC;

	current_RTC = GET_LOCAL_RTC;
	stop_RTC = current_RTC + ((long long)us * CYCLE_PER_SEC)/USEC_PER_SEC;

#ifndef SABLE
	while ((current_RTC=GET_LOCAL_RTC) < stop_RTC) {
		;
	}
#endif

	return;
}
#else /* IP30 */
/* Use tuned IP30 clock frequency code.
 */
static uint
findcpufreq_cycles(void)
{
	if (!private.cpufreq_cycles)
		private.cpufreq_cycles = findcpufreq_hz() >> 1;

	return private.cpufreq_cycles;
}

/* Return first member of the timeout chain.  Only called from splprof()
 * and is read-only so we do not need the callout list lock.
 */
static __int64_t
first_timein(callout_info_t *cip)
{
	struct callout *p1;

	p1 = (CI_TODO(cip))->c_next;
	if (p1 == NULL)
		return 0x7fffffffffffffff;	/* far in the future */
	return p1->c_time;
}
#endif /* !IP30 */
