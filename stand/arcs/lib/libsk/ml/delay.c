/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.46 $"

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/debug.h>
#include <sys/i8254clock.h>
#include <sys/time.h>
#include <sys/ktime.h>
#include <libsc.h>
#ifdef SN0
#include <rtc.h>
#endif

/*
 * CAUTION: IP2[268] and IP30 run off the prom, they DO NOT like
 * initialized globals or statics
 * BSS is zero for all platforms, so no need to 0 initialization
 */
#ifndef SABLE
int sk_sable; 		/* set this if running under sable */
#else
int sk_sable = 1;
#endif

#ifdef IP22
int _ioc1_ticksper1024inst(void);
#endif
ulong _ticksper1024inst(void);

#define INST_COUNT	1024

/*
 * Instructions per DELAY loop.  The loop body is a branch with a decrement
 * in the delay slot:
 *	1:	bgtz	s0,1b
 *		subu	s0,1
 */
#define	IPL	2

#if !EVEREST
static ulong
delay_calibrate(void)
{
    register ulong loopcount;
    register ulong tpi;

#if IP22
    if (is_ioc1() == 1) {		/* guinness p0 */
	tpi = (ulong) _ioc1_ticksper1024inst();
    }
    else
#endif
    {
	tpi = _ticksper1024inst();
    }

    if (!tpi) {
	/*
	** The timer's broken.  Just use some reasonable
	** value that's on the large side.
	**
	** 4 == ~50Mhz R4000.
	*/
	tpi = 10 * 4;
    }

    /*
    ** Calculate 100 * number of deci-nanoseconds per loop.
    ** This is used to determine how high us_delay should
    ** count.
    **
    ** We now calculate 10 * deci-nanoseconds since as processors
    ** got faster we lost significant digits (50Mhz R4000 varies
    ** is usually 3, sometimes 4) making the rounding of the
    ** calculations more significant.
    ** 
    ** dns/loop = (dns/sec)/(ticks/sec) * inst/loop * ticks/inst
    */
#if !IP30
    loopcount = (10 * (100000000/MASTER_FREQ) * IPL * tpi)/INST_COUNT;
#else
    loopcount = (10 * (100000000/HEART_COUNT_RATE) * IPL * tpi)/INST_COUNT;
#endif	/* !IP30 */
    ASSERT (loopcount);

    return loopcount;
}
#endif /* !EVEREST */

/*
 * Want these two to be global such that the IP22/26 PROM can reset them to 0
 * after it decides to run in cache.  K0SEG does not necessary imply running
 * cached since there are 3 bits in the R4000 coprocessor 0 config register
 * determining the cache algorithm.
 */
ulong decinsperloop, uc_decinsperloop;

/*
 * Spin in a tight loop for "us" microseconds.
 */
void
us_delay(register uint us)
{
#if EVEREST
	register evreg_t stop_RTC;

	if (sk_sable)
		return;

	stop_RTC = EV_GET_LOCAL(EV_RTC) + 
		(us * (CYCLE_PER_SEC / USEC_PER_SEC));

	while (EV_GET_LOCAL(EV_RTC) < stop_RTC)
		;

	return;
#elif SN0
#ifndef SABLE
	rtc_sleep(us);
#endif
	return;
#else
    extern void delayloop(ulong,ulong);
    register ulong n, d;

#ifdef SABLE
    if (sk_sable)
	return;
#endif

    /*  Use KSEG1 instead of KSEG0 as it can be compat, 0xa8, 0x98, etc in 64
     * bit mode.
     */
    if (IS_KSEG1(getpc())) {
	    if (!uc_decinsperloop) {
		    uc_decinsperloop = delay_calibrate();
#if IP30
		    /* uncache fudge factor */
		    uc_decinsperloop = (uc_decinsperloop * 999)/1000;
#endif
	    }
	    d = uc_decinsperloop;
    } else {
	    if (!decinsperloop)
		    decinsperloop = delay_calibrate();
	    d = decinsperloop;
    }

    n = (ulong)(10 * 100) * (ulong)us;
    delayloop(n,d);		/* call assembly for scaled routines */
#endif /* EVEREST */
}
