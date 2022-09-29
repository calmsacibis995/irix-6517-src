/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.36 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/SN/agent.h>
#include <sys/cmn_err.h>
#include <sys/clock.h>
#include <sys/debug.h>
#include <sys/clksupport.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/SN/addrs.h>
#include <sys/pda.h>
#include <sys/rtmon.h>
#include "klclock.h"
#include "sn_private.h"
#include <sys/SN/nvram.h>

#ifdef DEBUG
int do_rtodc_debug = 0;
int rtodc_display = 1;
#endif

#define	KGCLOCK_STATE_UNINITIALIZED	0
#define	KGCLOCK_STATE_INITIALIZED		1
static int	kgclock_state = KGCLOCK_STATE_UNINITIALIZED;
static int	kgclock_enabled_cnt = 0;
static lock_t	kgclock_lock;
static int	kgclock_lock_init = 0;
extern int	set_nvreg(uint, u_char);
extern u_char	get_nvreg(uint);
extern void	update_checksum(void);

void prof_intr(eframe_t *, void *);

/*
 * hub_rtc_init
 *	This code enables the RTC interrupt for the hub.
 *	It needs to be called once for each hub that has
 *	attached CPUs.
 */
void
hub_rtc_init(cnodeid_t cnode)
{
	/*
	 * We only need to initialize the current node.
	 * If this is not the current node then it is a cpuless
	 * node and timeouts will not happen there.
	 */
	if (get_compact_nodeid() == cnode) {
		LOCAL_HUB_S(PI_RT_EN_A, 1);
		LOCAL_HUB_S(PI_RT_EN_B, 1);
		LOCAL_HUB_S(PI_PROF_EN_A, 0);
		LOCAL_HUB_S(PI_PROF_EN_B, 0);
	}	
		
}

/*
 * Acknowledge interrupt from timeout timer.  The timeout timer 
 * is a count/compare bus cycle resolution timer, one per processor.  
 * It's used to generate a low priority interrupt for timeout queue
 * processing.
 */
void
acktmoclock(void)
{
#if SABLE
	return;		/* sable doesn't implement NVR_RTCC */
#else
	/*
	 * Acknowledge the tmo interrupt so that we dont get interrupted
	 * again!
	 */
	if ((cputoslice(cpuid())) == 0)
		LOCAL_HUB_S(PI_RT_PEND_A, 0);
	else  
		LOCAL_HUB_S(PI_RT_PEND_B, 0);
#endif /* SABLE */

}


void
ackkgclock(void)
{
	volatile u_char rtc_regC;

#if SABLE
	return;		/* sable doesn't implement NVR_RTCC */
#else
	/*
	 * Acknowledge the prof interrupt
	 */
	if ((cputoslice(cpuid())) == 0)
		LOCAL_HUB_S(PI_PROF_PEND_A, 0);
	else  
		LOCAL_HUB_S(PI_PROF_PEND_B, 0);
#endif /*SABLE */

}


void
startkgclock(void)
{
	unsigned long s;

	s = mutex_spinlock_spl(&kgclock_lock, splprof);
	
	/* If not already initialized, then do it now -- and make it FAST */
	if (kgclock_state == KGCLOCK_STATE_UNINITIALIZED) {
		setkgvector(prof_intr);
		kgclock_state = KGCLOCK_STATE_INITIALIZED;
	}
	/* If this CPU already has the profiling clock set then
	 * return now
	 */
	if (private.fclock_freq == CLK_FCLOCK_FAST_FREQ) {
		mutex_spinunlock(&kgclock_lock, s);
		return;
	}
	private.fclock_freq = CLK_FCLOCK_FAST_FREQ;
	nodepda->prof_count++;
	/*
	 * Make sure that the profiling intr is enabled, and on
	 * cpu "A" we start the clock running.
	 */
	if ((cputoslice(cpuid())) == 0)
		LOCAL_HUB_S(PI_PROF_EN_A, 1);
	 else 
		LOCAL_HUB_S(PI_PROF_EN_B, 1);
	/*
	 *	First one in starts the clock
	 */
	if ((nodepda->prof_count == 1) || (cpuid() == cnodetocpu(cnodeid()))) {
		clkreg_t	new_time;
		
		new_time = LOCAL_HUB_L(PI_RT_COUNT);
		nodepda->next_prof_timeout = new_time + CLK_FCLOCK_FAST_FREQ;
		LOCAL_HUB_S(PI_PROFILE_COMPARE, nodepda->next_prof_timeout);
	}
	kgclock_enabled_cnt++;
	mutex_spinunlock(&kgclock_lock, s);
}


/* Stop the profiling clock */
void
slowkgclock(void)
{
	unsigned long s;

	if (kgclock_lock_init == 0) {	/* initialize lock the first time */
		spinlock_init(&kgclock_lock, "kgclock");
		kgclock_lock_init = 1;
	}

	s = mutex_spinlock_spl(&kgclock_lock, splprof);
	/*
	 * If we are the audio CPU and audio is on the do not
	 * do anything.
	 */
	if (cpuid() == master_procid && audioclock) {
		mutex_spinunlock(&kgclock_lock, s);
		return;
	}

	if  (cputoslice(cpuid()) == 0)
		LOCAL_HUB_S(PI_PROF_EN_A, 0);
	else
		LOCAL_HUB_S(PI_PROF_EN_B, 0);

	if (kgclock_state != KGCLOCK_STATE_UNINITIALIZED) {
		/*
		 * slowkgclock is called more 
		 *  than once during startup
		 */
		if (kgclock_enabled_cnt)
			kgclock_enabled_cnt--;
		nodepda->prof_count--;
	}

	private.fclock_freq = CLK_FCLOCK_SLOW_FREQ;
	mutex_spinunlock(&kgclock_lock, s);
	ackkgclock();
}

#define BCD_TO_INT(x) (((x>>4) * 10) + (x & 0xf))
#define INT_TO_BCD(x) (uchar_t)(((x/10) << 4) | (uchar_t)(x % 10))

static int month_days[12] = {
	31,	/* January */
	28,	/* February */
	31,	/* March */
	30,	/* April */
	31,	/* May */
	30,	/* June */
	31,	/* July */
	31,	/* August */
	30,	/* September */
	31,	/* October */
	30,	/* November */
	31	/* December */
};
static volatile uchar_t *ioc3base;
#define TOD_SGS_M48T35	1
#define TOT_DALLAS_DS1386 2
static int todchip_type;
#include <sys/SN/klconfig.h>	
rtodc(void)
{
	u_char tod_cycle, wrote_tod_cycle;
	int month, day, hours, mins, secs, year;
	int bogus_rtodc_time = 0;
	time_t save_time;
	int i;

#if SABLE 
	return(0);		/* RTC not implemented */

#else /* SABLE */

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
	if (get_console_nasid() != master_nasid)
	    return 0;
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */
	if (ioc3base == NULL) {
		int testval;
		ioc3base = (uchar_t *)KL_CONFIG_CH_CONS_INFO(master_nasid)->memory_base;
		PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_DISABLE);
		PCI_OUTB(RTC_DAL_DAY_ADDR, 0xff);
		PCI_OUTB(RTC_DAL_CONTROL_ADDR,RTC_DAL_UPDATE_ENABLE );

		testval = PCI_INB(RTC_DAL_DAY_ADDR);
		if (testval == 0xff) {
			todchip_type = TOD_SGS_M48T35;
		} else {
			todchip_type = TOT_DALLAS_DS1386;
		}

	}

	/* turn off the update bit for syncronious read */

	switch(todchip_type) {
	case TOD_SGS_M48T35:
		PCI_OUTB(RTC_SGS_CONTROL_ADDR, RTC_SGS_READ_PROTECT);
		secs = BCD_TO_INT(PCI_INB(RTC_SGS_SEC_ADDR));
		mins = BCD_TO_INT(PCI_INB(RTC_SGS_MIN_ADDR));
		hours = BCD_TO_INT(PCI_INB(RTC_SGS_HOUR_ADDR));
		day = BCD_TO_INT(PCI_INB(RTC_SGS_DATE_ADDR));
		month =	BCD_TO_INT(PCI_INB(RTC_SGS_MONTH_ADDR));
		year =	BCD_TO_INT(PCI_INB(RTC_SGS_YEAR_ADDR));
		PCI_OUTB(RTC_SGS_CONTROL_ADDR, 0);
		break;
	case TOT_DALLAS_DS1386:
		PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_DISABLE);
		secs = BCD_TO_INT(PCI_INB(RTC_DAL_SEC_ADDR));
		mins = BCD_TO_INT(PCI_INB(RTC_DAL_MIN_ADDR));
		hours = BCD_TO_INT(PCI_INB(RTC_DAL_HOUR_ADDR));
		day = BCD_TO_INT(PCI_INB(RTC_DAL_DATE_ADDR));
		month =	BCD_TO_INT(PCI_INB(RTC_DAL_MONTH_ADDR));
		year =	BCD_TO_INT(PCI_INB(RTC_DAL_YEAR_ADDR));
		PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_ENABLE);
		break;
	default:
		cmn_err(CE_WARN, "Unknown time of day part detected\n");
	}
	/*
	 * Once upon a time the offset subtracted from the current year
	 * to get a 2 digit year to write to the TOD chip was set to
	 * YRREF (1970), which is a BAD thing since 1970 is not a leap year.
	 * Thus the TOD chip got two years out of sync with leap years since
	 * it tries to keep track of leap years too (year%4 == 0).  This
	 * results in leap year kinds of problems every two years.  But
	 * we are stuck with this now since changing the offset to a 
	 * different value will result in a time shift corresponding to the
	 * change between the new offset and 1970.  So we have to try to
	 * compensate for the bad choice of 1970.  FOR NEW MACHINES PLEASE
	 * USE A PROPER OFFSET LIKE 1968 IF YOU HAVE TWO DIGIT YEAR TOD
	 * CHIPS!!!!  In order to compensate we need to know when the TOD
	 * chip was last written to - time periods when the TOD chip and
	 * real time would be in sync wrt leap time, and when they are
	 * out of sync.  [wrote_]tod_cycle provides this information.
	 */
	tod_cycle = (((year%4 == 2) && (month >= 3)) ||
		     (year%4 == 3) ||
		     ((year%4 == 0) && (month < 3))) + 1;
	wrote_tod_cycle = get_nvreg(NVOFF_TOD_CYCLE);
	if ((wrote_tod_cycle != 1) && (wrote_tod_cycle != 2)) {
		/*
		 * either nvram is messed up, or this is the first time
		 * running this system with this fix in place, so we have
		 * no idea what time period the TOD was last written to.
		 * So we make the assumption it is the same time period
		 * as now.
		 */
		wrote_tod_cycle = tod_cycle;
		cmn_err(CE_WARN, "Check the date to make sure it is correct\n");
		bogus_rtodc_time = 1;
	}
	/* Clock only holds years 0-99
	 * so subtract off the YRREF
	 */
	year += YRREF;
#ifdef DEBUG
	if (rtodc_display)
	    cmn_err(CE_CONT,"rtodc: %d:%d:%d %d/%d/%d\n",
		    hours,mins,secs, year,month,day);
#endif
	
	/*
	 *  We've seen some tod chips which go crazy.  Try to recover.
	 */
	if (secs > 59  ||  mins > 59  ||  hours > 23  ||  day > 31  ||
	    month > 12) {
		bogus_rtodc_time = 1;
		cmn_err(CE_WARN,"rtodc: preposterous time in tod chip: %d:%d:%d %d/%d/%d\n  CHECK AND RESET THE DATE!",
			hours,mins,secs, year,month,day);
		secs  = 0;
		mins  = 0;
		hours = 0;
		month = 5;
		day   = 18;
		year  = 1998;
	}

	/* Sum up seconds from 1970 00:00:00 GMT */
	secs += mins * SECMIN;
	secs += hours * SECHOUR;
	secs += (day-1) * SECDAY;
	if (LEAPYEAR(year))
		month_days[1]++;
	for (i = 0; i < month-1; i++)
		 secs += month_days[i] * SECDAY;
	if (LEAPYEAR(year))
		month_days[1]--;

	while (--year >= YRREF) {
		secs += SECYR;
		if (LEAPYEAR(year))
			secs += SECDAY;
	}
	/*
	 * if we have crossed over the critical time boundaries for
	 * when the real time and the TOD time are in/out of sync wrt
	 * leap year, then we have to change things.
	 */
	if (tod_cycle > wrote_tod_cycle) {
		secs -= SECDAY;
		bogus_rtodc_time = 1;
	} else if (tod_cycle < wrote_tod_cycle) {
		secs += SECDAY;
		bogus_rtodc_time = 1;
	}
	if (bogus_rtodc_time) {
		/*
		 *  The tod chip has a completely bogus time.  Reset it.
		 */
		save_time = time;	/* don't disturb "time" */
		time = secs;		/* this is what wtodc() uses */
		wtodc();
		time = save_time;
	}
	return((time_t)secs);
#endif /* SABLE */
}


void
wtodc(void)
{
	u_char wrote_tod_cycle;
	ulong year_secs;
	int i, month, day, hours, mins, secs, year;
	unsigned long s;

	year_secs = (unsigned) time;
	year = YRREF;	/* 1970 */
	for (;;) {
		secs = SECYR;
		if (LEAPYEAR(year))
			secs += SECDAY;
		if (year_secs < secs)
			break;
		year_secs -= secs;
		year++;
	}

	/*
	 * Break seconds in year into month, day, hours, minutes, seconds
	 */
	day = (year_secs / SECDAY) + 1;
	hours = year_secs % SECDAY;
	if (LEAPYEAR(year))
		month_days[1]++;
	for (month = 1; day > (i = month_days[month-1]); day -= i)
		month++;
	if (LEAPYEAR(year))
		month_days[1]--;
	mins = hours % SECHOUR;
	hours /= SECHOUR;
	secs = mins % SECMIN;
	mins /= SECMIN;
	/* Clock only holds years 0-99
	 * so subtract off the YRREF
	 */
	year -= YRREF;
	/*
	 * keep track of time period we last wrote the TOD chip to
	 * compensate for leap year skew - see rtodc() notes.
	 */
	wrote_tod_cycle = (((year%4 == 2) && (month >= 3)) ||
			   (year%4 == 3) ||
			   ((year%4 == 0) && (month < 3))) + 1;
	set_nvreg(NVOFF_TOD_CYCLE, wrote_tod_cycle);
	update_checksum();
	if ((month == 2) && (day == 29) && (year%4 == 2)) {
		/*
		 * Not sure how the TOD chip handles this illegal
		 * combination (as it sees things), so make it
		 * something it will like.
		 */
		month = 3;
		day = 1;
	}
	s = splprof(); /* protect against prof intr */
	switch(todchip_type) {
	case TOD_SGS_M48T35:
		PCI_OUTB(RTC_SGS_CONTROL_ADDR, RTC_SGS_WRITE_ENABLE);
		PCI_OUTB(RTC_SGS_SEC_ADDR, INT_TO_BCD(secs));
		PCI_OUTB(RTC_SGS_MIN_ADDR, INT_TO_BCD(mins));
		PCI_OUTB(RTC_SGS_HOUR_ADDR, INT_TO_BCD(hours));
		PCI_OUTB(RTC_SGS_DATE_ADDR, INT_TO_BCD(day));
		PCI_OUTB(RTC_SGS_MONTH_ADDR, INT_TO_BCD(month));
		PCI_OUTB(RTC_SGS_YEAR_ADDR, INT_TO_BCD(year));
		PCI_OUTB(RTC_SGS_CONTROL_ADDR, 0);
		break;
	case TOT_DALLAS_DS1386:
		PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_DISABLE);
		PCI_OUTB(RTC_DAL_SEC_ADDR, INT_TO_BCD(secs));
		PCI_OUTB(RTC_DAL_MIN_ADDR, INT_TO_BCD(mins));
		PCI_OUTB(RTC_DAL_HOUR_ADDR, INT_TO_BCD(hours));
		PCI_OUTB(RTC_DAL_DATE_ADDR, INT_TO_BCD(day));
		PCI_OUTB(RTC_DAL_MONTH_ADDR, INT_TO_BCD(month));
		PCI_OUTB(RTC_DAL_YEAR_ADDR, INT_TO_BCD(year));
		PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_ENABLE);
		break;
	}
	splx(s);
#ifdef DEBUG
	if (do_rtodc_debug) {
	    rtodc();
	    rtodc_display = 0;
	}
#endif
}


/*
 * timer_freq is the frequency of the 32 bit counter timestamping source.
 * timer_high_unit is the XXX
 * Routines that references these two are not called as often as
 * other timer primitives. This is a compromise to keep the code
 * well contained.
 * Must be called early, before main().
 */

void
timestamp_init(void)
{
        timer_freq = CYCLE_PER_SEC;     /* RTC time source */
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;   /* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}

/* ARGSUSED */
void
prof_intr(eframe_t *ep, void *arg)
{
	/*
	 * RTMON timestamp for profiling intr entry
	 */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_PROFCOUNTER_INTR, NULL, NULL,
			 NULL, NULL);
	fastick_maint(ep);
	/*
	 * if this cpu is set up for the profiling counter then
	 * rearm it if we are CPU 'A' if we are CPU 'B' then
	 * the rearm on cpu 'A' is good enough
	 */
	if (private.fclock_freq == CLK_FCLOCK_FAST_FREQ) {
		if ((nodepda->prof_count == 1) 
		    || (cpuid() == cnodetocpu(cnodeid()))) {
			clkreg_t	new_time;
			
			new_time = LOCAL_HUB_L(PI_RT_COUNT);
			nodepda->next_prof_timeout += CLK_FCLOCK_FAST_FREQ;
			/*
			 * Check to make sure we did not already miss the
			 * interrupt, it we did then delay and do it
			 */
			if (new_time > nodepda->next_prof_timeout){
				nodepda->next_prof_timeout = new_time +
					CLK_FCLOCK_FAST_FREQ;
			}
			LOCAL_HUB_S(PI_PROFILE_COMPARE,
					nodepda->next_prof_timeout);
		}	
	}

	/*
	 * RTMON timestamp for profiling intr exit
	 */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_PROFCOUNTER_INTR, NULL,
			 NULL, NULL);
}
