#if IP22 || IP26 || IP28				/* whole file */
/*
 * Dallas 1[23]86 Time of Day Clock Support
 *
 * $Revision: 1.36 $
 *
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/i8254clock.h>
#include <sys/cpu.h>
#include <sys/ds1286.h>
#include <arcs/types.h>
#include <arcs/time.h>
#include <libsc.h>
#include <libsk.h>

extern int month_days[12];		/* in libsc.a */
extern int use_dallas;

#if IP22
static int _todc_was_stopped;
#endif

static u_int _clock_func(int, char *);
static int counter_not_moving(void);
static void reset_todc(void);

/* Keep track of clock errors.  If we get too many, then it's probably
 * obvious the clock is bad.  cpu_get_tod() is called a lot, and zillions
 * of messages impedes progress.
 */
static int errcnt;

/*
 * cpu_get_tod -- get current time-of-day from clock chip, and return it in
 * ARCS TIMEINFO format.  the libsc routine get_tod will convert it to
 * seconds for those who need it in that form.
 */
void
cpu_get_tod(TIMEINFO *t)
{
	int month, day, year, hours, mins, secs;
	int tries = 0;
	char buf[7];

again:
	if ((_clock_func( WTR_READ, buf ) & WTR_EOSC_N) ||
	    counter_not_moving ()) {
		/* either clock oscillator is not running or
		 * TE is set low.
		 */
		if (errcnt++ >= 6)
			return;
		reset_todc();
#if IP22
		_todc_was_stopped = 1;
#endif
		printf("\nInitialized tod clock.\n");
		if ( tries++ ) {
			printf("Can't set tod clock.\n");
			return;
		}
		goto again;
	}

	secs = (u_int)(buf[ 0 ] & 0xf);
	secs += (u_int)(buf[ 0 ] >> 4) * 10;

	mins = (u_int)(buf[ 1 ] & 0xf);
	mins += (u_int)(buf[ 1 ] >> 4) * 10;

	/*
	** we use 24 hr mode, so bits 4 and 5 represent to 2 ten-hour
	** bits
	*/
	hours = (u_int)( buf[ 2 ] & 0xf );
	hours += (u_int)( ( buf[ 2 ] >> 4 ) & 0x3 ) * 10;

	/*
	** actually day of month
	*/
	day = (u_int)( buf[ 3 ] & 0xf );
	day += (u_int)( buf[ 3 ] >> 4 ) * 10;

	month = (u_int)( buf[ 4 ] & 0xf );
	month += (u_int)( buf[ 4 ] >> 4 ) * 10;

	year = (u_int)( buf[ 5 ] & 0xf );
	year += (u_int)( buf[ 5 ] >> 4 ) * 10;

	/* set up ARCS time structure */
	t->Month = month;
	t->Day = day;
	t->Year = year + DALLAS_YRREF;
	t->Hour = hours;
	t->Minutes = mins;
	t->Seconds = secs;
	t->Milliseconds = 0;

	if ((secs < 0) || (secs >= 60) ||
	    (mins < 0) || (mins >= 60) ||
	    (hours < 0) || (hours >= 24) ||
	    (day < 1) || (day > 31) ||
	    (month < 1) || (month > 12) ||
	    (year < BASEYEAR)) {
		if (errcnt++ < 6) {
		    printf("Warning: time invalid, resetting clock.\n");
#ifdef DEBUG
		    printf("hour %d min %d sec %d month %d day %d year %d \n",
			hours,mins,secs,month,day,year);
#endif
		    reset_todc();
		}
	}
	return;
}

void
cpu_set_tod(TIMEINFO *t)
{
	int year = t->Year - DALLAS_YRREF, sec;
	char buf[7];

        buf[0] = (char)( ( ( t->Seconds / 10 ) << 4 ) | ( t->Seconds % 10 ) );
        buf[1] = (char)( ( ( t->Minutes / 10 ) << 4 ) | ( t->Minutes % 10 ) );
        buf[2] = (char)( ( ( t->Hour / 10 ) << 4 ) | ( t->Hour % 10 ) );
        buf[3] = (char)( ( ( t->Day / 10 ) << 4 ) | ( t->Day % 10 ) );
        buf[4] = (char)( ( ( t->Month / 10 ) << 4 ) | ( t->Month % 10 ) );
        buf[5] = (char)( ( ( year / 10 ) << 4 ) | ( year % 10 ) );
	sec = (t->Seconds+ (t->Minutes * SECMIN) + (t->Hour * SECHOUR) +
		((t->Day-1) * SECDAY));
        buf[6] = (char)get_dayof_week(_rtodc(sec,t->Year,t->Month));

        _clock_func( WTR_WRITE, buf );
}

#define EXTRA_DAYS      3               /* used to get day of week */

LONG
_rtodc(int secs,int year,int month)
{
	register int i;

	/* Count seconds for full months that have elapsed.
	 */
        for (i = 0; i < month-1; i++)		/* only count full months */
                 secs += month_days[i] * SECDAY;

	/*  If operating on a leap year, and Feb has already passed, add in
	 * another day.
	 */
	if (LEAPYEAR(year) && (month > 2))
		secs += SECDAY;

	/* Count previous years back to the epoc.
	 */
	while (--year >= YRREF) {
		secs += SECYR;
		if (LEAPYEAR(year))
			secs += SECDAY;
        }

	return(secs);
}

int
get_dayof_week(LONG year_secs)
{
        int days,yr=YRREF;
        int year_day;

        days = 0;
        for (;;) {
                register int secyear = SECYR;

                if (LEAPYEAR(yr))
                        secyear += SECDAY;

                if (year_secs >=secyear)
                        year_day = secyear;
                else
                        year_day = (int)year_secs;
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


static void
reset_todc(void)
{
#if IP22			/* only use Indy announce date on IP22 */
	int start_year = 1993 - DALLAS_YRREF;
	char buf[7];

	bzero(buf,sizeof(buf));
	buf[2] = 16;			/* date/time to 11:00 July 12, 1993 */
	buf[3] = 12;			/* guess what date that is! */
	buf[4] = 7;
	buf[5] = (start_year/10 << 4) + start_year%10;
	buf[6] = 6;			/* that day was Monday */
	_clock_func(WTR_WRITE,buf);
#else
	/* with DALLAS_YRREF, cannot use the real epoch, so try 1990 */
	int start_year = 1995 - DALLAS_YRREF;
	char buf[7];

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 1;
	buf[4] = 1;
	buf[5] = (start_year/10 << 4) + start_year%10;
	buf[6] = 1;		/* monday */
	_clock_func(WTR_WRITE,buf);
#endif
}

static u_int
_clock_func(int func, char * ptr)
{
	register ds1286_clk_t *clock;

	clock = RTDS_CLOCK_ADDR; /* Need new real time clock address for HPC3 */
	if (func == WTR_READ) {
		/* freeze external time of the day registers for reading */
		clock->command &= ~WTR_TE;
		/* read data */
		*ptr++ = (char)clock->sec;
		*ptr++ = (char)clock->min;
		*ptr++ = (char)clock->hour & 0x3f;
		*ptr++ = (char)clock->date & 0x3f;
		*ptr++ = (char)clock->month & 0x1f;
		*ptr++ = (char)clock->year;
		*ptr++ = (char)clock->day & 0x7;
		/* unfreeze external time of the day registers */
		clock->command |= WTR_TE;

		return clock->month;	
	} else {
		/* start clock oscilltor */ 
		clock->month &= ~WTR_EOSC_N;

                /* freeze external time of the day registers for writing */
		clock->command &= ~WTR_TE;

		clock->sec = *ptr++;
		clock->min = *ptr++;
		clock->hour = *ptr++ & 0x3f;		/* make sure 24 hr */
		clock->date = *ptr++;
		clock->month = *ptr++ & 0x1f;		/* make sure ESQW=0 */
		clock->year = *ptr++;
		clock->day = *ptr++;
		clock->command |= WTR_TE;

		clock->ram[RT_RAM_FLAGS] &= RT_FLAGS_INVALID;

		return 0;
	}
}

static int
counter_not_moving(void)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int delay_count = 1000000;
	unsigned char initial;
	static int rtclock_ok;

	if (rtclock_ok)
		return(0);

	initial = (unsigned char)clock->hundreth_sec;

	while (delay_count--)
		if ((unsigned char)clock->hundreth_sec != initial) {
			rtclock_ok = 1;
			return (0);
		}

	return (1);
}

/* Disable watchdog output.  Done when power is switched off.
 */
void
ip22_power_switch_off(void)
{
	register ds1286_clk_t *clock = (ds1286_clk_t *)RTDS_CLOCK_ADDR;
	int command = clock->command;

	/* disable watchdog and auto power alarm then reset WAF to zero */
	command |= WTR_DEAC_WAM|WTR_DEAC_TDM;
	command &= ~(WTR_PULSE_MODE_INT|WTR_TIME_DAY_INTA|WTR_SOURCE_INTB);
#if DEBUG
	printf("ip22_power_switch_off(): Write 0x%x to 0x%x\r\n", command, &clock->command);
#endif
	clock->command = command;
	clock->watch_hundreth_sec = 0x00;	
	clock->watch_sec = 0x00;	
}

/* Enable watchdog output.  Done when power is turned on.
 */
void
ip22_power_switch_on(void)
{
	register ds1286_clk_t *clock = (ds1286_clk_t *)RTDS_CLOCK_ADDR;
	int command = clock->command;
	char *buf;

	buf = (char *)cpu_get_nvram_offset(NVOFF_AUTOPOWER,NVLEN_AUTOPOWER);
	if ((*buf != 'y') && (*buf != 'Y')) {
#if DEBUG
	    printf("ip22_power_switch_on(): autopower=(*0x%x) %s\r\n",
		buf, buf);
#endif
		ip22_power_switch_off();		/* make sure */
		return;
	}

	/* Activate watchdog INT output on INTA after 1/10 second.
	 * Disable autopower alarm.
	 */
	command &= ~(WTR_DEAC_WAM|WTR_PULSE_MODE_INT|WTR_TIME_DAY_INTA|
		     WTR_SOURCE_INTB);
	command |= WTR_DEAC_TDM;
#if DEBUG
	printf("ip22_power_switch_on(): Write 0x%x to 0x%x\r\n", command, &clock->command);
#endif
	clock->command = command;
	clock->watch_hundreth_sec = 0x10;
	clock->watch_sec = 0x0;
}

#if IP22 || IP26 || IP28
int
cpu_restart_rtc(void)		/* make sure clock is running */
{
	TIMEINFO t;
	cpu_get_tod(&t);

#if IP22
	return (_todc_was_stopped);
#else
	return(0);
#endif
}
#endif

#endif	/* IP22 || IP26 || IP28 */
