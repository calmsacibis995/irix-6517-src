#include <sys/types.h>
#include <sys/sema.h>
#include <sys/ktime.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/callo.h>
#include <sys/i8254clock.h>
#include <sys/ds17287.h>
#include <sys/mace.h>
#include <sys/cpu.h>
#include <sys/cmn_err.h>
#include <sys/PCI/pciio.h>
#if _SABLE_CLOCK
#include <sys/MHSIM.h>
#endif

extern u_char miniroot;

/* START of Time Of Day clock chip stuff (to "END of Time Of Day clock chip" */
static void _clock_func(int func, char *ptr);
static void setbadclock(void), clearbadclock(void);
static int isbadclock(void);
static int get_dayof_week(time_t);
static int clock_initted = 0;

/*
 * make sure the clock is in the right mode:
 *     BCD data
 *     24 hour mode
 *     Kickstart and wakeup flags cleared
 *     Kickstart interrupt enabled
 *     Wakeup interrupt disabled
 */
static void
rtc_init(void)
{
    volatile ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int s;
    long long isa_msk;

    if (clock_initted)
	return;

    s = splhi();
    clock_initted = 1;
    clock->registerb &= ~(DS_REGB_DM|DS_REGB_PIE|DS_REGB_AIE|DS_REGB_UIE);
    clock->registerb |= DS_REGB_2412;
    clock->registera |= DS_REGA_DV0;
    clock->ram[DS_BANK1_XCTRLA] &= ~(DS_XCTRLA_KF|DS_XCTRLA_WF);
    clock->ram[DS_BANK1_XCTRLB] &= 
	~(DS_XCTRLB_WIE|DS_XCTRLB_E32K|DS_XCTRLB_RCE);
    clock->ram[DS_BANK1_XCTRLB] |= DS_XCTRLB_KSE|DS_XCTRLB_PRS;
    clock->registera &= 
	~(DS_REGA_DV0|DS_REGA_RS3|DS_REGA_RS2|DS_REGA_RS1|DS_REGA_RS0);
    /*
    clock->registerc &= ~DS_REGC_IRQF;
    */
    /* 
     * These seemingly misplaced pciio_pio_* calls are part of the CRIME 1.1
     * PIO read bug workaround.  
     */
    isa_msk = pciio_pio_read64((uint64_t *)(PHYS_TO_K1(ISA_INT_MSK_REG)));
    isa_msk |= ISA_INT_RTC_IRQ;
    pciio_pio_write64((uint64_t)(isa_msk), (uint64_t *)(PHYS_TO_K1(ISA_INT_MSK_REG)));
    splx(s);
}

/*
 * Get the time/date from the ds17287, and reinitialize
 * with a guess from the file system if necessary.
 */
void
inittodr(time_t base)
{
    register uint todr;
    long deltat;
    int TODinvalid = 0;
    extern int todcinitted;
    int s,dow;
    char buf[8];

    s = splprof();
    spsema(timelock);

    rtc_init();
    todr = rtodc(); /* returns seconds from 1970 00:00:00 GMT */
    settime(todr,0);

    if (miniroot) {	/* no date checking if running from miniroot */
	svsema(timelock);
	splx(s);
	return;
    }

    /*
     * If day of week not set then set it.
     */
    dow = get_dayof_week(todr);
    _clock_func(WTR_READ, buf);
    if (dow != buf[6]) {
	buf[6] = dow;
	_clock_func( WTR_WRITE, buf );
    }

    /*
     * Complain if the TOD clock is obviously out of wack. It
     * is "obvious" that it is screwed up if the clock is behind
     * the 1988 calendar currently on my wall.  Hence, TODRZERO
     * is an arbitrary date in the past.
     */
    if (todr < TODRZERO) {
	if (todr < base) {
	    cmn_err_tag(63,CE_WARN, "lost battery backup clock--using file system time");
	    TODinvalid = 1;
	} else {
	    cmn_err_tag(64,CE_WARN, "lost battery backup clock");
	    setbadclock();
	}
    }

    /*
     * Complain if the file system time is ahead of the time
     * of day clock, and reset the time.
     */
    if (todr < base) {
	if (!TODinvalid)	/* don't complain twice */
	    cmn_err_tag(65,CE_WARN, "time of day clock behind file system time--resetting time");
	settime(base, 0);
	resettodr();
	setbadclock();
    }
    todcinitted = 1; 	/* for chktimedrift() in clock.c */

    /*
     * See if we gained/lost two or more days: 
     * If so, assume something is amiss.
     * XXX: why? are you assuming that a machine never
     * gets turned off for a 3 day weekend?
     */
    deltat = todr - base;
    if (deltat < 0)
	deltat = -deltat;
    if (deltat > 2*SECDAY)
	cmn_err_tag(66,CE_WARN,"clock %s %d days",
		    time < base ? "lost" : "gained", deltat/SECDAY);

    if (isbadclock())
	cmn_err_tag(67,CE_WARN, "CHECK AND RESET THE DATE!");


    svsema(timelock);
    splx(s);
}


/* 
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.
 */

void
resettodr(void) {
    wtodc();
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

static void _clock_func(int func, char *ptr);

int
rtodc()
{
    uint month, day, year, hours, mins, secs;
    register int i;
    char buf[8];

    _clock_func(WTR_READ, buf);

    secs = (uint)(buf[0] & 0xf);
    secs += (uint)(buf[0] >> 4) * 10;

    mins = (uint)(buf[1] & 0xf);
    mins += (uint)(buf[1] >> 4) * 10;

    /*
     * we use 24 hr mode, so bits 4 and 5 represent the 2 tens-hour bits
     */
    hours = (uint)(buf[2] & 0xf);
    hours += (uint)((buf[2] >> 4) & 0x3) * 10;

    /*
     * actually day of month
     */
    day = (uint)(buf[3] & 0xf);
    day += (uint)(buf[3] >> 4) * 10;

    month = (uint)(buf[4] & 0xf);
    month += (uint)(buf[4] >> 4) * 10;

    /*
     * calculate the actual year (buf[7] has century (ie. 19 for 20th 
     * century.
     */
    year = (uint)(buf[5] & 0xf);
    year += (uint)(buf[5] >> 4) * 10;
    year += (uint)(((buf[7] & 0xf) + ((buf[7] >> 4) * 10)) * 100);

    /*
     * Sum up seconds from 00:00:00 GMT 1970
     */
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
    return(secs);
}

void
wtodc(void)
{
    register long year_secs = time;
    register month, day, hours, mins, secs, year, dow;
    char buf[8];

    /*
     * calculate the day of the week
     */
    dow = get_dayof_week(year_secs);

    /*
     * Whittle the time down to an offset in the current year,
     * by subtracting off whole years as long as possible.
     */
    year = YRREF;
    for (;;) {
	register secyr = SECYR;
	if (LEAPYEAR(year))
	    secyr += SECDAY;
	if (year_secs < secyr)
	    break;
	year_secs -= secyr;
	year++;
    }
    /*
     * Break seconds in year into month, day, hours, minutes, seconds
     */
    if (LEAPYEAR(year))
	month_days[1]++;
    for (month = 0;
	 year_secs >= month_days[month]*SECDAY;
	 year_secs -= month_days[month++]*SECDAY)
	continue;
    month++;
    if (LEAPYEAR(year))
	month_days[1]--;

    for (day = 1; year_secs >= SECDAY; day++, year_secs -= SECDAY)
	continue;

    for (hours = 0; year_secs >= SECHOUR; hours++, year_secs -= SECHOUR)
	continue;

    for (mins = 0; year_secs >= SECMIN; mins++, year_secs -= SECMIN)
	continue;

    secs = year_secs;

    buf[0] = (char)(((secs / 10) << 4) | (secs % 10));
    buf[1] = (char)((( mins / 10) << 4) | (mins % 10));
    buf[2] = (char)((( hours / 10) << 4) | (hours % 10));
    buf[3] = (char)(((day / 10) << 4) | (day % 10));
    buf[4] = (char)((( month / 10) << 4) | (month % 10));

    /* 
     * since we now have a century counter, the actual year is what
     * gets stored in the dallas chip.  Figure out what year in the
     * century it is. We figure this stuff out of order because I'm lazy
     * and don't want to change the lower level code in _clock_func().
     */
    buf[5] = (char)((((year % 100) / 10) << 4) | ((year % 100) % 10));
    buf[7] = (char)(((year / 1000) << 4) | ((year / 100) % 10));

    /*
     * Now that we are done with year stuff, plug in day_of_week
     */
    buf[ 6 ] = (char) dow;

    _clock_func( WTR_WRITE, buf );

    /*
     * clear the flag in nvram that says whether or not the clock
     * date is correct or not.
     */
    clearbadclock();
}

#if 0
static char
_to_bcd(int i)
{
    return((char)(((i / 10) << 4) | (i % 10)));
}
#endif

static void
_clock_func(int func, char *ptr)
{
#if !SABLE_CLOCK
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int s = splhi();

    if (!clock_initted) {
	rtc_init();
    }
	    
    if (func == WTR_READ) {
	REG_RDORWR8(&clock->registerb, DS_REGB_SET);
	REG_RDORWR8(&clock->registera, DS_REGA_DV0);

	/* read data */
	*ptr++ = pciio_pio_read8(&clock->sec);
	*ptr++ = pciio_pio_read8(&clock->min);
	*ptr++ = pciio_pio_read8(&clock->hour) & 0x3f;
	*ptr++ = pciio_pio_read8(&clock->date) & 0x3f;
	*ptr++ = pciio_pio_read8(&clock->month) & 0x1f;
	*ptr++ = pciio_pio_read8(&clock->year);
	*ptr++ = pciio_pio_read8(&clock->day) & 0x7;
	*ptr++ = pciio_pio_read8(&clock->ram[DS_BANK1_CENTURY]);
    } else {
	/* make sure clock oscilltor is going. */
	REG_RDORWR8(&clock->registera, DS_REGA_DV1|DS_REGA_DV0);
	REG_RDANDWR8(&clock->registera, ~DS_REGA_DV2);
	/* freeze updates */
	REG_RDORWR8(&clock->registerb, DS_REGB_SET);

	/* write data */
	pciio_pio_write8(*ptr++, &clock->sec);
	pciio_pio_write8(*ptr++, &clock->min);
	pciio_pio_write8(*ptr++ & 0x3f, &clock->hour);    /* make sure 24 hr */
	pciio_pio_write8(*ptr++, &clock->date);
	pciio_pio_write8(*ptr++, &clock->month);
	pciio_pio_write8(*ptr++, &clock->year); 
	pciio_pio_write8(*ptr++, &clock->day);
	pciio_pio_write8(*ptr++, &clock->ram[DS_BANK1_CENTURY]);
    }

    /* allow updates, leave in 24 hour format */
    REG_RDANDWR8(&clock->registerb, ~DS_REGB_SET);
#else
    register sable_clk_t *clock = 
	(sable_clk_t *)PHYS_TO_K1(SABLE_NVRAM_BASE);
    int s = splhi();

    if (func == WTR_READ) {
	/* read data */
	*ptr++ = _to_bcd(clock->sec);
	*ptr++ = _to_bcd(clock->min);
	*ptr++ = _to_bcd(clock->hour & 0x3f);
	*ptr++ = _to_bcd(clock->date & 0x3f);
	*ptr++ = _to_bcd(clock->month & 0x1f);
	*ptr++ = _to_bcd(clock->year);
	*ptr++ = _to_bcd(clock->day & 0x7);
	*ptr++ = _get_century();
    }
#endif

    splx(s);
}

int
read_dallas_nvram(char *buf, int off, int len)
{
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int s;

    /*
     * although the part has 114 bytes of RAM, the first byte is
     * reserved by the kernel for the RT_RAM_FLAGS byte.  We'll
     * use the rest of it for our own purposes.
     */
    if (RT_RAM_BASE + off + len > DS_RAM_SIZE || off < RT_RAM_BASE)
	return 1;
    s = splhi();
    REG_RDANDWR8(&clock->registera, ~DS_REGA_DV0);
    while (len--)
	buf[len] = pciio_pio_read8(&clock->ram[(off+len) + RT_RAM_BASE]);
    splx(s);
    return 0;
}

int
write_dallas_nvram(char *buf, int off, int len)
{
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int s;

    /*
     * although the part has 114 bytes of RAM, the first byte is
     * reserved by the kernel for the RT_RAM_FLAGS byte.  We'll
     * use the rest of it for our own purposes.
     */
    if (RT_RAM_BASE + off + len > DS_RAM_SIZE || off < RT_RAM_BASE)
	return 1;
    s = splhi();
    REG_RDANDWR8(&clock->registera, ~DS_REGA_DV0);
    while (len--)
	pciio_pio_write8(buf[len], &clock->ram[(off+len) + RT_RAM_BASE]);
    splx(s);
    return 0;
}

static void
setbadclock(void)
{
#if !SABLE_CLOCK
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int s = splhi();

    REG_RDANDWR8(&clock->registera, ~DS_REGA_DV0);
    REG_RDORWR8(&clock->ram[RT_RAM_FLAGS], RT_FLAGS_INVALID);
    splx(s);
#endif
}

static void
clearbadclock(void)
{
#if !SABLE_CLOCK
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int s = splhi();

    REG_RDANDWR8(&clock->registera, ~DS_REGA_DV0);
    REG_RDANDWR8(&clock->ram[RT_RAM_FLAGS], ~RT_FLAGS_INVALID);

    splx(s);
#endif
}

static int
isbadclock(void)
{
#if !SABLE_CLOCK
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int rc, s = splhi();

    REG_RDANDWR8(&clock->registera, ~DS_REGA_DV0);
    rc = pciio_pio_read8(&clock->ram[RT_RAM_FLAGS]) & RT_FLAGS_INVALID;
    splx(s);
    return(rc);
#else
    return(0);
#endif
}

/*
 * Called from prom_reinit(), in case syssgi(SET_AUTOPWRON) was done.
 * If time of the day alarm vars are to be set then program
 * RTC to power system back on using time-of-day alarm, unless the
 * current time is already past that value.
 */
void
set_autopoweron(void)
{
    register ds17287_clk_t *clock = (ds17287_clk_t *) RTDS_CLOCK_ADDR;
    int month, day, hours, mins, year, day_of_week;
    extern time_t autopoweron_alarm;
    long year_secs;

    if((unsigned long)time > (unsigned long)autopoweron_alarm)
	return;	/* not requested, or requested time is already past */

    year_secs = autopoweron_alarm;

    /*
     * calculate the day of the week 
     */
    day_of_week = get_dayof_week(year_secs);
    /*
     * Whittle the time down to an offset in the current year, by
     * subtracting off whole years as long as possible. 
     */
    year = YRREF;
    for (;;) {
	register secyr = SECYR;
	if (LEAPYEAR(year))
	    secyr += SECDAY;
	if (year_secs < secyr)
	    break;
	year_secs -= secyr;
	year++;
    }
    /*
     * Break seconds in year into month, day, hours, minutes,
     * seconds 
     */
    if (LEAPYEAR(year))
	month_days[1]++;
    for (month = 0;
	 year_secs >= month_days[month] * SECDAY;
	 year_secs -= month_days[month++] * SECDAY)
	continue;
    month++;
    if (LEAPYEAR(year))
	month_days[1]--;

    for (day = 1; year_secs >= SECDAY; day++, year_secs -= SECDAY)
	continue;

    for (hours = 0; year_secs >= SECHOUR; hours++, year_secs -= SECHOUR)
	continue;

    for (mins = 0; year_secs >= SECMIN; mins++, year_secs -= SECMIN)
	continue;

    if (!((mins > 59) || (hours > 23) || (day_of_week > 7))) {
	/* Set alarm to activate when minutes, hour and day match.
	 */
	pciio_pio_write8((char) (((mins / 10) << 4) | (mins % 10)),
			 &clock->min_alarm);
	pciio_pio_write8((char) (((hours / 10) << 4) | (hours % 10)),
			 &clock->hour_alarm);
	/*
	 * The following should be fixed to support setting a meaningful
	 * date alarm value.
	 */
#ifndef XXX
	REG_RDORWR8(&clock->registera, DS_REGA_DV0);
	pciio_pio_write8(DS_ALARM_DONT_CARE, &clock->ram[DS_BANK1_DATE_ALARM]);
#else
	pciio_pio_write8((((day_of_week / 10) << 4) | (day_of_week % 10)),
		 &clock->day_alarm);
#endif
	/*
	 * we don't care about the seconds.
	 */
	pciio_pio_write8(DS_ALARM_DONT_CARE, &clock->sec_alarm);

	/*
	 * enable wakeup interrupt.
	 */
	REG_RDORWR8(&clock->ram[DS_BANK1_XCTRLB], DS_XCTRLB_WIE);
    }
}

/*
 * converts year_secs to the day of the week 
 * (i.e. Sun = 1 and Sat = 7)
 */
static int
get_dayof_week(time_t year_secs)
{
    int days,yr=YRREF;
    int year_day;
#define EXTRA_DAYS      3		/* used to get day of week */

    days = 0;
    for (;;) {
	register secyear = SECYR;
	if (LEAPYEAR(yr))
	    secyear += SECDAY;

	if (year_secs >=secyear)
	    year_day = secyear;
	else
	    year_day = year_secs;
	for (;;) {
	    if (year_day < SECDAY)
		break;

	    year_day -= SECDAY;
	    days++;
	}

	if (year_secs < secyear)
	    break;
	year_secs -= secyear;
	yr++;
    }

    if (days > EXTRA_DAYS) {
	days -=EXTRA_DAYS;
	days %= 7;
	days %= 7;
    } else
	days +=EXTRA_DAYS+1;
    return(days+1);

}

/* END of Time Of Day clock chip stuff */

/*	called from  clock() periodically to see if system time is 
	drifting from the real time clock chip. Seems to take about
	about 30 seconds to make a 1 second adjustment, judging
	by my debug printfs.  Only do if different by 2 seconds or more.
	1 second difference often occurs due to slight differences between
	kernel and chip, 2 to play safe.
	Empty routine if not IP6 because other machines rtdoc() routines do
	not return time since the epoch; also the TOD chips may not be as accurate.
	Main culprit seems to be graphics stuff keeping interrupts off for
	'long' times; it's easy to lose a tick when they are happening every
	milli-second.  Olson, 8/88
*/
void
chktimedrift(void)
{
    long todtimediff;

    todtimediff = rtodc() - time;
    if((todtimediff > 2) || (todtimediff < -2))
	(void)doadjtime(todtimediff * USEC_PER_SEC);
}

#if 0
static char
_get_century()
{
#if !SABLE_CLOCK
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    char cent;

    /* clock->registera |= DS_REGA_DV0;	select bank 1 */
    REG_RDORWR8(&clock->registera, DS_REGA_DV0);
    cent = pciio_pio_read8(&clock->ram[DS_BANK1_CENTURY]);
#else
    char cent = 0x19;
#endif
    return(cent);
}

static void
_set_century(char century)
{
#if !SABLE_CLOCK
    register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;

    /* clock->registera |= DS_REGA_DV0;	select bank 1 */
    REG_RDORWR8(&clock->registera, DS_REGA_DV0);
    pciio_pio_write8(century, &clock->ram[DS_BANK1_CENTURY]);
#endif
}
#endif
