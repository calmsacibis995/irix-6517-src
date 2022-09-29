/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
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
#ident "$Revision: 1.22 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/ktime.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/cmn_err.h"
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>

/*
 * Clock routines.
 *
 *	The input frequency to COUNTER 2 (MASTER) is: 3.6864Mhz.  This works
 *	out to a period of 271.26736111.....nano-sec, which you can
 *	see is not a whole number. 
 *
 * 	On the IP20/IP22, the input frequency to counter 2 is 1Mhz
 *	which gives much nicer numbers all around.
 */
#define COUNTER2_FREQ	(timer_freq)

#define	MASTER_COUNT	((MASTER_FREQ/COUNTER2_FREQ))

#define	SCHED_COUNT	((COUNTER2_FREQ/HZ))
#define	PROF_COUNT	((COUNTER2_FREQ/fasthz))
#define SLOW_COUNT	0

#if defined(IP20) || defined(IP22) || defined(IP32) || defined(IPMHSIM)
#define PROF_PERIOD	200	/* 1/5000  = 200  uSec */
#else
#define PROF_PERIOD	278	/* 1/3600 ~= 278 uSec */
#endif

#define MIN_INTERVAL	50
u_int32_t t0interval, t1interval;
u_int32_t t0timer, t1timer;		/* compare values for timers */

unsigned long cause_ip5_count;
unsigned long cause_ip6_count;

int _mr4kintrcnt;
int _er4kintrcnt;
int _lr4kintrcnt;
int _pr4kintrcnt;

extern void inithrestime(void);
extern int fastick;			/* computed in startkgclock() once */

extern int	fasthz;		/* this paramter can be modified via lboot
				** or at run time using mpadmin()
				** it indicates the profiler clock speed 
				** in ticks/sec
				*/
extern u_int32_t get_r4k_counter(void);	/* compare check */
extern u_int32_t _get_r4k_counter(void); /* no compare check */
extern u_int32_t set_r4k_compare(int);
u_int32_t r4k_compare_shadow;

void ackkgclock_r4000(void);
void ackrtclock_r4000(void);

/*
 * Start the real-time clock (really the programable interval clock!).
 *
 * counter 2 is the master counter, counter 0 is
 * the scheduling counter, and counter 1 is the profiling counter.
 */

void
startrtclock_r4000(void)
{

	int s = spl7();
	t0interval = SCHED_COUNT;
	t0timer = get_r4k_counter() + t0interval;
	r4k_compare_shadow = t0timer;
	set_r4k_compare(r4k_compare_shadow);

	/*
 	 * turning on clock(), take care of time initialization 
	 */
	if (private.p_flags & PDAF_CLOCK)
		inithrestime();
	splx(s);
}

/*
 * Disable clocks 
 */

static int first_call = 1;

void
stopclocks_r4000(void)
{

	t0interval = 0;
	t0timer = 0xffffffff;
	t1interval = 0;
	t1timer = 0xffffffff;
	

	ackrtclock();
	ackkgclock();

first_call = 0;
}

#ifdef DEBUG
/*
 * Get current value of prof clock
 * We assume that the prof clock is free-running
 * We compute the # of mS that has elapsed since the last time we were called
 * Should be called with clock interrupts disabled
 * if the master period is 3600Hz (or 277.777 uS per 'tick') then
 *    full wrap of the profiler will be 64K * 277.777 or 18204.439 mS
 * if the master period is 460800 (or 2.1701 uS per 'tick') then
 *    full wrap of the profiler will be 64K * 2.1701 or 142.222mS
 * On IP20/IP22, the master period is 5000Hz (or 200 uS per
 *    'tick'), so the full wrap of the profiler will be 64K * 13000 mS
 */
extern unsigned short lastpval;
extern int resetpdiff;

int
getprofclock_r4000()
{
	register ushort count;
	register int msec = 0;

	if (private.fclock_freq != SLOW_COUNT)
		return(0);
	count = get_r4k_counter() & 0xffff;

	if (resetpdiff) {
		resetpdiff = 0;
		lastpval = count;
		return(0);
	}
	if (count < lastpval) {
		msec = ((lastpval - count) * PROF_PERIOD) / 1000;
	} else {
		msec = (((64*1024) - (count - lastpval)) * PROF_PERIOD) / 1000;
	}
	lastpval = count;
	return(msec);
}
#endif


/*
 * Kgclock is used as an independent time base for statistics gathering.
 */
void
startkgclock_r4000(void)
{
	int ospl;
	int ofasthz = fasthz;

#if !(defined(IP20) || defined(IP22) || !defined(IP32) || !defined(IPMHSIM))
#define MIN_FASTHZ	900
#define DEF_FASTHZ	1200
#define MAX_FASTHZ	1800
	if (!fasthz) {
		fasthz = DEF_FASTHZ;		/* default it if not set */
	} else if (fasthz != MIN_FASTHZ && fasthz != DEF_FASTHZ && 
					fasthz != MAX_FASTHZ) {
		cmn_err(CE_WARN, 
		"%d Hz is not acceptable for the system fast clock.",fasthz);
		fasthz = DEF_FASTHZ;
		cmn_err(CE_CONT, "%d Hz is the alternate freq.\n",fasthz);
	}
#else
#define MIN_FASTHZ	500
#define DEF_FASTHZ	1000
#define MAX_FASTHZ	2500	/* 5000 causes the counter1 value to be 1, */
				/* which is not valid in mode 2 */
	if (!fasthz) {
		fasthz = DEF_FASTHZ;		/* default it if not set */
	} else if (fasthz < MIN_FASTHZ) {	/* clamp to range */
		cmn_err(CE_WARN,
			"%d Hz is too slow for the system fast clock.",fasthz);
		fasthz = MIN_FASTHZ;
		cmn_err(CE_CONT, "%d Hz will be used instead.\n",fasthz);
	} else if (fasthz > MAX_FASTHZ) {
		cmn_err(CE_WARN,
			"%d Hz is too fast for the system fast clock.",fasthz);
		fasthz = MAX_FASTHZ;
		cmn_err(CE_CONT, "%d Hz will be used instead.\n",fasthz);
	} else if ((COUNTER2_FREQ/fasthz) * fasthz != COUNTER2_FREQ) {
		/* Make sure fast rate gives us an integer divide */
		cmn_err(CE_WARN,
			"%d Hz is not an acceptable fast clock frequency.\n",
			fasthz);
		fasthz = DEF_FASTHZ;
		cmn_err(CE_CONT, "%d Hz will be used instead.\n",fasthz);
	}
#endif
	if (!fastick || ofasthz != fasthz)
		fastick = USEC_PER_SEC/fasthz;

	/* flag for profiling clock */
	private.fclock_freq = PROF_COUNT;

	ospl = spl7();
	t1interval = PROF_COUNT;
	if (t0interval == 0) {
		t1timer = get_r4k_counter() + t1interval;
		r4k_compare_shadow = t1timer;
		set_r4k_compare(r4k_compare_shadow);
	} else {
		t1timer = t0timer;
	}
	splx (ospl);
}

/*
 * slowkgclock - keep running as a missed tick counter
 */
void
slowkgclock_r4000(void)
{
	int	s = spl7();

	t1interval = SLOW_COUNT;
	if (t0interval == 0) {
		t1timer = get_r4k_counter() + t1interval;
		r4k_compare_shadow = t1timer;
		set_r4k_compare(r4k_compare_shadow);
	} else {
		t1timer = t0timer;
	}
	splx(s);
	private.fclock_freq = SLOW_COUNT;
	ackkgclock();
}

	
#define MAX_TIMER_DELAY 100000000

/*ARGSUSED*/
void
r4kcount_intr_r4000(struct eframe_s *ep)
{
	u_int32_t base_counter, maxinterval;
	int	icount;
	int	first_time = 1;
#define ABOVE_TIME_LIMIT(a,b) \
	  ( ( (a) - (b) ) < ((u_int32_t) ( - ( maxinterval))) )

#if defined(R10000)
	/*
	 * If hw counters overflowed, process overflow intr.
	 */
	if (
#if defined(R4000)
	    /* the O2 uses a single kernel for R5K and R10K systems */
	    IS_R10000() &&
#endif
	    hwperf_overflowed_intr()) {
		hwperf_intr(ep);
		return;
        }
#endif /* R10000 */

	maxinterval = ((t0interval > t1interval) ? t0interval : t1interval);
	if (maxinterval == 0) { /* clocks disabled */
		set_r4k_compare(r4k_compare_shadow); /* ack interrupt */
		return;
	}

	while (1) {
		base_counter = _get_r4k_counter();
			
		/* check for premature interrupt */
		if (! ABOVE_TIME_LIMIT(base_counter, r4k_compare_shadow)) {
			if (first_time) {
				/* premature interrupt */
				_er4kintrcnt++;
				first_time = 0;
				set_r4k_compare(r4k_compare_shadow); /* ack interrupt */
				base_counter = get_r4k_counter();
				if (! ABOVE_TIME_LIMIT(base_counter,r4k_compare_shadow))
					return;
			} else
				return;
		}
		first_time = 0;

		/* check for missed interrupts */
		if (t0interval != 0) {
			icount = 0;
			while (ABOVE_TIME_LIMIT(base_counter,t0timer)) {
				icount++;
				t0timer += t0interval;
			}
			if (icount) {
				cause_ip5_count += icount;
				if (icount > 1)
					_mr4kintrcnt += (icount - 1);
			}
			r4k_compare_shadow = t0timer;
		}
		if (t1interval != 0) {
			icount = 0;
			while (ABOVE_TIME_LIMIT(base_counter,t1timer)) {
				icount++;
				t1timer += t1interval;
			}
			if (icount) {
				cause_ip6_count += icount;
				if (icount > 1)
					_mr4kintrcnt += (icount - 1);
			}
			if (! ABOVE_TIME_LIMIT(t1timer,t0timer))
				r4k_compare_shadow = t1timer;
		}
		set_r4k_compare(r4k_compare_shadow);
	}
}

#ifdef IPMHSIM
extern int use_sableio;
#endif /* IPMHSIM */

void
ackkgclock_r4000(void)
{
#ifndef IP32
	if (first_call)
#ifdef IPMHSIM
		if (! use_sableio)
#endif /* IPMHSIM */
		*(volatile char *)TIMER_ACK_ADDR = ACK_TIMER1;
#endif
}

void
ackrtclock_r4000(void)
{
#ifndef IP32
	if (first_call)
#ifdef IPMHSIM
		if (! use_sableio)
#endif /* IPMHSIM */
		*(volatile char *)TIMER_ACK_ADDR = ACK_TIMER0;
#endif
}
