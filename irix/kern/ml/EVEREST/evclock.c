#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/cmn_err.h>
#include <sys/clksupport.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/epc.h>
#include <sys/i8254clock.h>   /* YRREF, LEAPYEAR()... */
#include <sys/EVEREST/nvram.h>

#ifdef DEBUG
int do_rtodc_debug = 0;
int rtodc_display = 1;
#endif
#define	KGCLOCK_STATE_UNINITIALIZED	0
#define	KGCLOCK_STATE_SLOW		1
#define	KGCLOCK_STATE_FAST		2
static int	kgclock_state = KGCLOCK_STATE_UNINITIALIZED;
static int	kgclock_enabled_cnt = 0;
static lock_t	kgclock_lock;
static int	kgclock_lock_init = 0;
extern int	audioclock;
extern int	set_nvreg(uint, u_char);
extern u_char	get_nvreg(uint);
extern void	update_checksum(void);
#if defined(MULTIKERNEL)
int	evmk_kgclock_state = KGCLOCK_STATE_UNINITIALIZED;
#endif

/*
 * Acknowledge interrupt from timeout timer.  The timeout timer 
 * is a count/compare bus cycle resolution timer, one per processor.  
 * It's used to generate a low priority interrupt for timeout queue
 * processing.
 */
void
acktmoclock(void)
{
	EV_SET_LOCAL(EV_CIPL124, EV_CMPINT_MASK);
}


void
ackkgclock(void)
{
	volatile u_char rtc_regC;
	unsigned long s;

#if SABLE
	return;		/* sable doesn't implement NVR_RTCC */
#else
	if (cpuid() == 0) {	/* only one CPU needs to ack the tod chip */
	    s = splprof();
	    rtc_regC = EPC_RTC_READ(NVR_RTCC);
	    splx(s);
	}
#endif /*SABLE */
}


/*
 * Start kgclock, which is used as an independent time base 
 * for statistics gathering ie. the profiling clock which is also
 * use for audio and midi.
 */
extern void epc_intr_reinit(int);

void
startkgclock(void)
{
	unsigned long s;

	s = mutex_spinlock_spl(&kgclock_lock, splprof);

#if defined(MULTIKERNEL)
	/* Only the golden cell controls the kgclock.  Eventually may need
	 * mechanism for sending request to golden cell to change the rate.
	 */
	if (evmk_cellid != evmk_golden_cellid) {
		if (evmk_kgclock_state == KGCLOCK_STATE_UNINITIALIZED)
			setkgvector(epc_intr_proftim);
		evmk_kgclock_state = KGCLOCK_STATE_FAST;
		if (kgclock_state == KGCLOCK_STATE_FAST)
			private.fclock_freq = CLK_FCLOCK_FAST_FREQ;
		else
			private.fclock_freq = CLK_FCLOCK_SLOW_FREQ;

		mutex_spinunlock(&kgclock_lock, s);
		return;			  
	}
#endif /* MULTIKERNEL */
	/* If not already initialized, then do it now -- and make it FAST */
	if (kgclock_state == KGCLOCK_STATE_UNINITIALIZED) {
		unsigned rtc_regB;
		setkgvector(epc_intr_proftim);
		/* turn off the "generate the interrupt" bit */
		rtc_regB = EPC_RTC_READ(NVR_RTCB) & ~NVR_PIE;
		/* configure the todc interrupt rate to FAST */
		EPC_RTC_WRITE(NVR_RTCB, rtc_regB);
		EPC_RTC_WRITE(NVR_RTCA, NVR_OSC_ON|NVR_INTR_FAST);
		/* enable the "generate the interrupt" bit */
		rtc_regB = EPC_RTC_READ(NVR_RTCB) | NVR_PIE;
		EPC_RTC_WRITE(NVR_RTCB, rtc_regB);
		kgclock_state = KGCLOCK_STATE_FAST;
	} else
	/* else if not already FAST, then make it FAST */
	if (kgclock_state != KGCLOCK_STATE_FAST) {
		unsigned rtc_regB;
		/* enable the "generate the interrupt" bit */
		rtc_regB = EPC_RTC_READ(NVR_RTCB) | NVR_PIE;
		EPC_RTC_WRITE(NVR_RTCB, rtc_regB);
		kgclock_state = KGCLOCK_STATE_FAST;
	}
	private.fclock_freq = CLK_FCLOCK_FAST_FREQ;
	kgclock_enabled_cnt++;
	/*
	 * If the profiling clock is not on then it must be audio, and
	 * we only need CPU 0, not the broadcast intr.
	 */
	if (private.p_prf_enabled)
		epc_intr_reinit(1);	/* Direct intr to all CPUs */
	else
		epc_intr_reinit(2);	/* Direct intr to CPU 0 only */

#if defined(MULTIKERNEL)
	/* golden cell updates other cell's info about state of kgclock */
	evmk_replicate( &kgclock_state, sizeof(kgclock_state));
#endif /* MULTIKERNEL */
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
#if defined(MULTIKERNEL)
	if (evmk_cellid != evmk_golden_cellid)
		return;
#endif /* MULTIKERNEL */

	s = mutex_spinlock_spl(&kgclock_lock, splprof);
	if (!audioclock) {
		if (kgclock_state != KGCLOCK_STATE_UNINITIALIZED) {
			/*
			 * slowkgclock is called more 
			 *  than once during startup
			 */
			if (kgclock_enabled_cnt)
				kgclock_enabled_cnt--;	

			if (kgclock_enabled_cnt == 0) {
				unsigned rtc_regB;

				/* Turn off the "generate the interrupt" bit */
				rtc_regB = EPC_RTC_READ(NVR_RTCB) & ~NVR_PIE;
				EPC_RTC_WRITE(NVR_RTCB, rtc_regB);
				kgclock_state = KGCLOCK_STATE_SLOW;
			}
		}
	} else {
		epc_intr_reinit(2);
	}
	private.fclock_freq = CLK_FCLOCK_SLOW_FREQ;

	mutex_spinunlock(&kgclock_lock, s);

	ackkgclock();
#if defined(MULTIKERNEL)
	/* golden cell updates other cell's info about state of kgclock */
	evmk_replicate( &kgclock_state, sizeof(kgclock_state));
#endif /* MULTIKERNEL */
}

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

rtodc(void)
{
	u_char month, day, hours, mins;
	u_char month2, day2, hours2, mins2;
	u_char tod_cycle, wrote_tod_cycle;
	unsigned secs, secs2;
	int year, year2, i;
	int bogus_rtodc_time = 0;
	time_t save_time;
	unsigned long s;
	int rtc_regA, rtc_regB, rtc_regD;

#if SABLE
	return(0);		/* RTC not implemented */
#else /* SABLE */
	s = splprof();			/* protect against profiler intr */
	/* enable oscillator */
	rtc_regA = EPC_RTC_READ(NVR_RTCA);
	EPC_RTC_WRITE(NVR_RTCA, (rtc_regA & ~NVR_DV_MASK) | NVR_OSC_ON);

	/* turn off the SET bit, turn on DM and 2412, just to be safe */
	rtc_regB = EPC_RTC_READ(NVR_RTCB);
	EPC_RTC_WRITE(NVR_RTCB, (rtc_regB & ~NVR_SET) | NVR_DM | NVR_2412);

	rtc_regD = EPC_RTC_READ(NVR_RTCD);
	if ((rtc_regD & NVR_VRT) == 0) {
		cmn_err(CE_WARN,
			"IO4 NVRAM/time-of-day chip reports invalid RAM or time\n    Low battery?");
		return((time_t)SECYR*(1993-1970));  /* something reasonable */
	}

	secs =	EPC_RTC_READ(NVR_SEC);
	mins =	EPC_RTC_READ(NVR_MIN);
	hours = EPC_RTC_READ(NVR_HOUR);	/* 24 hr */
	day =	EPC_RTC_READ(NVR_DAY);
	month =	EPC_RTC_READ(NVR_MONTH);
	year =	EPC_RTC_READ(NVR_YEAR);
read_again:
	/*
	 *  During our byte-by-byte read of the todc, the values might have
	 *  incremented and rolled over the minutes (et al).  We don't know
	 *  exactly how the hardware updates the fields, so we need to check
	 *  every field with a second read, and keep re-reading until we get
	 *  two sets of identical values.
	 */
	secs2  = EPC_RTC_READ(NVR_SEC);
	mins2  = EPC_RTC_READ(NVR_MIN);
	hours2 = EPC_RTC_READ(NVR_HOUR);	/* 24 hr */
	day2   = EPC_RTC_READ(NVR_DAY);
	month2 = EPC_RTC_READ(NVR_MONTH);
	year2  = EPC_RTC_READ(NVR_YEAR);
	if (secs != secs2  ||  mins != mins2    ||  hours != hours2  ||
	    day  != day2   ||  month != month2  ||  year != year2) {
		secs  = secs2;
		mins  = mins2;
		hours = hours2;
		day   = day2;
		month = month2;
		year  = year2;
		goto read_again;
	}
	splx(s);
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
		month = 12;
		day   = 1;
		year  = 1993;
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
	int rtc_regA, rtc_regB;
	unsigned long s;

#ifdef DEBUG
	if (do_rtodc_debug) {
	    rtodc_display = 1;
	    rtodc();
	}
#endif

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

	s = splprof();				/* protect against prof intr */
	rtc_regB = EPC_RTC_READ(NVR_RTCB);	/* save current RTCB value */
	EPC_RTC_WRITE(NVR_RTCB, NVR_SET);	/* hold off updates */
	EPC_RTC_WRITE(NVR_RTCB, NVR_DM|NVR_2412); /* binary data, 24-hr mode */
	rtc_regA = EPC_RTC_READ(NVR_RTCA);
	/* enable oscillator */
	EPC_RTC_WRITE(NVR_RTCA, (rtc_regA & ~NVR_DV_MASK) | NVR_OSC_ON);
	EPC_RTC_WRITE(NVR_SEC, secs);
	EPC_RTC_WRITE(NVR_MIN, mins);
	EPC_RTC_WRITE(NVR_HOUR, hours);
	EPC_RTC_WRITE(NVR_DAY, day);
	EPC_RTC_WRITE(NVR_MONTH, month);
	EPC_RTC_WRITE(NVR_YEAR, year);	/* NVR_YEAR max is 99 */
	/* enable update transfers (from chip internal to RTC data regs) */
	EPC_RTC_WRITE(NVR_RTCB, (rtc_regB & ~NVR_SET)|NVR_DM|NVR_2412);
	splx(s);
#ifdef DEBUG
	if (do_rtodc_debug) {
	    rtodc();
	    rtodc_display = 0;
	}
#endif
}
