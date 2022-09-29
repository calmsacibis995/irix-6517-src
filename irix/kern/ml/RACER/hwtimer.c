/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.57 $"

/*
 * hwtimer.c - counter and timer support
 *
 *	CPU C0_COUNT -	scheduler clock
 *			profiler/fast clock
 *	Heart timer - 	locore timestamps, UST and CTIME (is absolute)
 *			free-running cycle counter (user mappable)
 *	CPU frequency computation
 *
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/sbd.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/timers.h>
#include <sys/hwperfmacros.h>

#include <sys/RACER/IP30.h>
#include "RACERkern.h"

extern ulong _ticksper1024inst(void);

extern int fastick;
extern int fasthz;

/*
 * Instructions per DELAY loop.  The loop body is a branch with a decrement
 * in the delay slot:
 *      1:      bgtz    s0,1b
 *              subu    s0,1
 */
#define IPL     	2
#define INST_COUNT	1024
void
delay_calibrate(void)
{
	register ulong tpi;

	tpi = _ticksper1024inst();	/* error in minus direction */
	private.decinsperloop =
		(10 * (100000000/HEART_COUNT_RATE) * IPL * tpi)/INST_COUNT;
}

#ifdef US_DELAY_DEBUG
/* rough check of us_delay.  Is not too accurate as done in C.  Caller needs
 * to convert from C0_COUNT ticks @ 1/2 freqency of CPU to determine time.
 */
__uint32_t us_before, us_after;
void
check_us_delay(void)
{
	ulong ticks;
	int i;
	__uint32_t delay_tbl[] = {
	    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 100, 1000, 10000, 100000, -1 };
	volatile ulong ht_t0;

	us_delay(1);		/* prime cache */

	for (i = 0; delay_tbl[i] != -1; i++) {
		ht_t0 = HEART_PIU_K1PTR->h_count;
		us_delay(delay_tbl[i]);
		ticks = HEART_PIU_K1PTR->h_count - ht_t0;
		printf("us_delay(%d) took a little less than 0x%x - 0x%x = "
			"%u cpu ticks. (%dm_%3du_%3dn secs)\n",
			delay_tbl[i],
			(__psunsigned_t)us_before,(__psunsigned_t)us_after,
			us_after-us_before,
			(ticks * HEART_COUNT_NSECS)/1000000,
			((ticks * HEART_COUNT_NSECS)%1000000)/1000,
			((ticks * HEART_COUNT_NSECS)%1000000)%1000);
	}
	printf("decinsperloop=%d\n", private.decinsperloop);
	ticks = _ticksper1024inst();
	printf("_ticksper1024inst=%d heart ticks (%d.%03d usecs)\n",
		ticks,
		(ticks*HEART_COUNT_NSECS)/1000,
		(ticks*HEART_COUNT_NSECS)%1000);
	return;
}
#endif	/* US_DELAY_DEBUG */

/*
 * delay, but first make sure bus is clear, so delay is guaranteed
 * to be relative to bus, not CPU
 */
void
us_delaybus(uint us)
{
	flushbus();
	us_delay(us);
}

/*
 * Find the PClk frequency (in Hz) of the running CPU
 */
uint pclk_freq;		/* in case we want to see it in symmon/dbx */

static uint
find_pclk_freq(void)
{
	heart_piu_t *heart = HEART_PIU_K1PTR;
	uint start, cycles;
	__uint64_t us_end, count;
	int s;

	/* If we know the frequency, then just return it (same both CPUs) */
	if (pclk_freq)
		return pclk_freq;

	s = splprof();			/* make sure no intrs */

	/* Wait for 1 tick so we know about where we are */
	us_end = heart->h_count;
	while ((count = heart->h_count) == us_end)
		; /*empty*/

	start = _get_count();		/* do not start clock over */

	/* inline delay by reading heart for 1000 us (1ms) */
	us_end = count + ((1000L*HEART_COUNT_RATE)/1000000);

	while (heart->h_count < us_end)
		; /*empty*/

	cycles = _get_count() - start;

	splx(s);

	/* round cycles to nearest 100 kHz */
	cycles = ((cycles+50)/100)*100;
    
	/* processor increments every other cycle */
	pclk_freq = cycles * 1000 * 2; 

	return pclk_freq;
}

static int freq[] = {
	100000000,
	150000000,
	175000000,
	180000000,		/* Misc lab frequencies */
	185000000,
	190000000,
	195000000,		/* Initial MR back to 195 like pacecar :-( */
	200000000,		/* Target initial MR */
	112500000,		/* Target 3.1 T5 */
	225000000,
	240000000,
	250000000,
	270000000,		/* 270Mhz TREX */
	275000000,		/* TREX target is 275-300Mhz */
	300000000,
	315000000,
	325000000,
	333000000,		/* 333Mhz TREX */
	350000000,
	360000000,		/* 360Mhz TREX Shrink */
	375000000,
	385000000,		/* 385Mhz TREX Shrink */
	390000000,		/* 390Mhz TREX Shrink */
	400000000,		/* there is talk of a 400Mhz TREX shrink */
	425000000,
	450000000,
	500000000
};

/*
 * Find the frequency (in MHz) of the running CPU
 *
 * The initmasterpda() function stuffs this value into
 * private.cpufreq array (*half* the internal clock (PClk) rate).
 */
uint
findcpufreq_hz(void)
{
	unsigned int pclk = find_pclk_freq();
	int closest=-1,i,clock,tmp;

	/* round frequency to table */
	for (i=0; i < (sizeof(freq)/sizeof(int)); i++) {
		if (pclk > freq[i])
			tmp = pclk - freq[i];
		else
			tmp = freq[i] - pclk;
		if (closest == -1) {
			clock = i;
			closest = tmp;	
			continue;
		}
		if (tmp < closest) {
			closest = tmp;
			clock = i;
		}
		
	}
	pclk = freq[clock];

	return pclk;
}

int
cpu_mhz_rating(void)
{
	return findcpufreq_hz() / 1000000;
}

int
findcpufreq(void)
{
	return (findcpufreq_hz()/1000000) / 2;
}

/* Clear the heart count/compare interrupt.
 */
/*ARGSUSED*/
void
heartclock_intr(eframe_t *ep)
{
	HEART_PIU_K1PTR->h_clr_isr = HEART_INT_TIMER;
}

static void
fasthzerr(int wrong, char *err, int instead)
{
	cmn_err(CE_WARN, "%d Hz is %s for the system fast clock.", wrong, err);
	cmn_err(CE_WARN, "%d Hz will be used instead.", instead);
}

#define MIN_FASTHZ	500
#define DEF_FASTHZ	1000
#define MAX_FASTHZ	2500

/* global to avoid fasthz divide for each fast tick in clksupport.c */
int clk_fclock_fast_freq = CYCLE_PER_SEC / DEF_FASTHZ;

void
startkgclock(void)
{
	int s;

	if (!fasthz) {
		fasthz = DEF_FASTHZ;		/* default it if not set */
	}
	else if (fasthz < MIN_FASTHZ) {	/* clamp to range */
		fasthzerr(fasthz, "too slow", MIN_FASTHZ);
		fasthz = MIN_FASTHZ;
	}
	else if (fasthz > MAX_FASTHZ) {
		fasthzerr(fasthz, "too fast", MAX_FASTHZ);
		fasthz = MAX_FASTHZ;
	}

	/* On odd freq CPUs, down the road MAX_FASTHZ may not divide out */
	if ((HEART_COUNT_RATE % fasthz) || (private.cpufreq_cycles % fasthz)) {
		fasthzerr(fasthz, "unacceptable", DEF_FASTHZ);
		fasthz = DEF_FASTHZ;
	}

	fastick = USEC_PER_SEC/fasthz;
	clk_fclock_fast_freq = CYCLE_PER_SEC/fasthz;

	/* Setting private.p_fclock_tick starts the "fast clock" on
	 * the next count/compare tick.   We try to poke the clock
	 * to let the first fast clock happen in ~1-2ms.
	 */
	s = splprof();
	private.p_fclock_slow = 0;
	private.p_fclock_tick = 1;
	set_timer_intr(cpuid(),-1,C_NORM);
	splx(s);
}

/* Stop fast ticks.
 */
void
slowkgclock(void)
{
	int s;

	s = splprof();
	private.p_fclock_tick = 0;
	splx(s);
}

/*  Set-up the timestamp units.
 *	timer_freq - frequency of the 64 bit counter timestamping source.
 *	the counter is actually 52 bits but since it wraparounds in ~11 years,
 *		it will do the job nicely.
 */
void
timestamp_init(void)
{
	timer_freq = HEART_COUNT_RATE;
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;		/* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}
