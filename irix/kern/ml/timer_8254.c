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
#ident "$Revision: 1.18 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/ktime.h"
#include "sys/i8254clock.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/cmn_err.h"

#ifdef IP22
/* avoid function call for INT2/3.  We are always Indigo2 here */
#define is_ioc1()	0
#define STARTRTCLOCK	startrtclock_8254
#define STOPCLOCKS	stopclocks_8254
#define GETPROFCLOCK	getprofclock_8254
#define STARTKGCLOCK	startkgclock_8254
#define SLOWKGCLOCK	slowkgclock_8254
#define R4KCOUNT_INTR	r4kcount_intr_8254
#define ACKKGCLOCK	ackkgclock_8254
#define ACKRTCLOCK	ackrtclock_8254
#else
#define STARTRTCLOCK	startrtclock
#define STOPCLOCKS	stopclocks
#define GETPROFCLOCK	getprofclock
#define STARTKGCLOCK	startkgclock
#define SLOWKGCLOCK	slowkgclock
#define ACKKGCLOCK	ackkgclock
#define ACKRTCLOCK	ackrtclock
#endif

/*
 * Clock routines.
 *
 *	The input frequency to COUNTER 2 (MASTER) is: 1MHz
 *	which gives much nicer numbers all around.
 */
#define	COUNTER2_FREQ	5000

#define	MASTER_COUNT	((MASTER_FREQ/COUNTER2_FREQ))

#define	SCHED_COUNT	((COUNTER2_FREQ/HZ))
#define	PROF_COUNT	((COUNTER2_FREQ/fasthz))
#define SLOW_COUNT	0

#define PROF_PERIOD	200	/* 1/5000  = 200  uSec */

#define MIN_INTERVAL	50

extern void inithrestime(void);
extern int fastick;			/* computed in startkgclock() once */

extern int	fasthz;		/* this paramter can be modified via lboot
				** or at run time using mpadmin()
				** it indicates the profiler clock speed 
				** in ticks/sec
				*/

/*
 * Start the real-time clock (really the programable interval clock!).
 *
 * counter 2 is the master counter, counter 0 is
 * the scheduling counter, and counter 1 is the profiling counter.
 */

void
STARTRTCLOCK(void)
{
	int s = spl7();
	volatile struct pt_clock *pt = (struct pt_clock *)PT_CLOCK_ADDR;

	pt->pt_control = PTCW_SC(0)|PTCW_16B|PTCW_MODE(MODE_RG);
	pt->pt_counter0 = SCHED_COUNT;
	DELAYBUS(4);
	pt->pt_counter0 = SCHED_COUNT>>8;
	pt->pt_control = PTCW_SC(2)|PTCW_16B|PTCW_MODE(MODE_RG);
	pt->pt_counter2 = MASTER_COUNT;
	DELAYBUS(4);
	pt->pt_counter2 = MASTER_COUNT>>8;

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
void
STOPCLOCKS(void)
{

	register volatile struct pt_clock *pt =
			(struct pt_clock *)PT_CLOCK_ADDR;

	pt->pt_control = PTCW_SC(0)|PTCW_16B|PTCW_MODE(MODE_HROS);
	pt->pt_counter0 = SCHED_COUNT;
	DELAYBUS(4);
	pt->pt_counter0 = SCHED_COUNT>>8;
	pt->pt_control = PTCW_SC(1)|PTCW_16B|PTCW_MODE(MODE_HROS);
	pt->pt_counter1 = PROF_COUNT;
	DELAYBUS(4);
	pt->pt_counter1 = PROF_COUNT>>8;
	pt->pt_control = PTCW_SC(2)|PTCW_16B|PTCW_MODE(MODE_HROS);
	pt->pt_counter2 = MASTER_COUNT;
	DELAYBUS(4);
	pt->pt_counter2 = MASTER_COUNT>>8;

	ackrtclock();
	ackkgclock();
}

#ifdef DEBUG
/*
 * Get current value of prof clock
 * We assume that the prof clock is free-running
 * We compute the # of mS that has elapsed since the last time we were called
 * Should be called with clock interrupts disabled
 * The master period is 5000Hz
 *    (or 200 uS per 'tick'), so the full wrap of the profiler will be
 *    64K * 13000 mS
 */
extern unsigned short lastpval;
extern int resetpdiff;

int
GETPROFCLOCK()
{
	register volatile struct pt_clock *pt =
				(struct pt_clock *)PT_CLOCK_ADDR;
	register ushort count, countmsb;
	register int msec = 0;

	if (private.fclock_freq != SLOW_COUNT)
		return(0);
	/*
	 * latch status & count
	 * The DELAYS are there since it seems that w/o them one sometimes
	 * gets wrong results back!
	 */
	pt->pt_control = PTCW_CLCMD|PTCW_SC(1);
	DELAYBUS(10);
	count = pt->pt_counter1;
	DELAYBUS(10);
	countmsb = pt->pt_counter1;
	count |= (countmsb << 8);

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
 * Clear cpu board TIM0 interrupt.
 */
void
ACKRTCLOCK(void)
{
	*(volatile char *)TIMER_ACK_ADDR = ACK_TIMER0;
	flushbus();
}

/*
 * Clear cpu board TIM1 interrupt for IP20/IP22
 */
void
ACKKGCLOCK()
{
	*(volatile char *)TIMER_ACK_ADDR = ACK_TIMER1;
	flushbus();
}

/*
 * Kgclock is used as an independent time base for statistics gathering.
 */
void
STARTKGCLOCK(void)
{
	register volatile struct pt_clock *pt = 
				(struct pt_clock *)PT_CLOCK_ADDR;
	int ospl;
	int ofasthz = fasthz;

	ospl = splprof();

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
	if (!fastick || ofasthz != fasthz)
		fastick = USEC_PER_SEC/fasthz;

	/* flag for profiling clock */
	private.fclock_freq = PROF_COUNT;

	pt->pt_control = PTCW_SC(1)|PTCW_16B|PTCW_MODE(MODE_RG);
	pt->pt_counter1 = PROF_COUNT;
	DELAYBUS(4);
	pt->pt_counter1 = PROF_COUNT>>8;
	pt->pt_control = PTCW_SC(2)|PTCW_16B|PTCW_MODE(MODE_RG);
	pt->pt_counter2 = MASTER_COUNT;
	DELAYBUS(4);
	pt->pt_counter2 = MASTER_COUNT>>8;
	splx(ospl);
}

/*
 * slowkgclock - keep running as a missed tick counter
 */
void
SLOWKGCLOCK(void)
{
	register volatile struct pt_clock *pt = 
				(struct pt_clock *)PT_CLOCK_ADDR;
	int ospl;

	ospl = splprof();

	private.fclock_freq = SLOW_COUNT;
	pt->pt_control = PTCW_SC(1)|PTCW_16B|PTCW_MODE(MODE_RG);
	pt->pt_counter1 = SLOW_COUNT;
	DELAYBUS(4);
	pt->pt_counter1 = SLOW_COUNT>>8;
	ackkgclock();
	splx(ospl);
}

#if IP22
/*ARGSUSED*/
void
r4kcount_intr_8254(struct eframe_s *ep)
{
	clr_r4kcount_intr();
}
#endif
