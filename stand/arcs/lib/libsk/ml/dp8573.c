#if defined(IP20)	/* whole file */
/*
 * DP 8573 Time of Day Clock Support
 *
 * $Revision: 1.10 $
 *
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/i8254clock.h>
#include <sys/dp8573clk.h>
#include <arcs/types.h>
#include <arcs/time.h>
#include <libsc.h>

int		rtclock_ok;

extern int	month_days[12];		/* in libsc.a */

static void	_clock_func(int, char *);
static void	reset_todc(void);
static int	counter_not_moving (void);

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
	char	buf[ 8 ];
	u_int tries = 0;

again:
	_clock_func( RTC_READ, buf );

	if ( !((int)buf[ 4 ] << CLK_SHIFT & RTC_RUN) ||
				(!rtclock_ok && counter_not_moving ()))
	{
		if (errcnt++ >= 6)
			return;
		printf("\nInitializing tod clock.\n");
		reset_todc();
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

	month = (u_int)( buf[ 5 ] & 0xf );
	month += (u_int)( buf[ 5 ] >> 4 ) * 10;

	year = (u_int)( buf[ 6 ] & 0xf );
	year += (u_int)( buf[ 6 ] >> 4 ) * 10;

	/* set up ARCS time structure */
	t->Month = month;
	t->Day = day;
	t->Year = year + YRREF;
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
		printf("Warning: time invalid, resetting clock to epoch.\n");
		reset_todc();
	   }
	}
	return;
}

static void
reset_todc()
{
	char buf[8];

	bzero(buf,sizeof(buf));
	buf[6] = buf[5] = 1;		/* month/day min is 1 */
	_clock_func( RTC_WRITE, buf );
}

static void
_clock_func(int func, char * ptr)
{
	register struct dp8573_clk *clock;
	char *state;
	int leap, i;

	clock = (struct dp8573_clk *)RT_CLOCK_ADDR;

	if (func == RTC_READ) {
		/* clear the time save RAM */
		for (i = 0; i < 5; i++)
			clock->ck_timsav[i] = 0;
		/* latch the Time Save RAM */
		clock->ck_status = RTC_STATINIT;
		clock->ck_tsavctl0 = RTC_TIMSAVON;
		clock->ck_tsavctl0 = RTC_TIMSAVOFF;
		for (i = 0; i < 5; i++) {
			if (i == 4)		/* keep the format the same */
				state = ptr++;	/* as for IP4		    */
			*ptr++ = (char) (clock->ck_timsav[i] >> CLK_SHIFT);
		}
		*ptr = (char) (clock->ck_counter[6] >> CLK_SHIFT);
		clock->ck_status |= RTC_RS;
		*state = (char) (clock->ck_rtime1 >> CLK_SHIFT);
	} else {
		clock->ck_status = RTC_STATINIT;
		clock->ck_tsavctl0 = RTC_TIMSAVOFF;
		clock->ck_pflag0 = RTC_PFLAGINIT;
		clock->ck_status = RTC_RS;
		clock->ck_int1ctl1 = RTC_INT1INIT;
		clock->ck_outmode1 = RTC_OUTMODINIT;

		clock->ck_rtime1 &= ~RTC_RUN;
		for (i = 0; i < 7; i++) {
			if (i == 4)
				ptr++;
			clock->ck_counter[i] = *ptr++ << CLK_SHIFT;
		}
		leap = clock->ck_counter[6] % 4;
		clock->ck_rtime1 = RTC_RUN | (leap << CLK_SHIFT);
	}
		
}

static int
counter_not_moving ()
{
	register struct dp8573_clk 		*clock = RT_CLOCK_ADDR;
	register int				delay_count = 1000000;
	register unsigned char			initial;


	rtclock_ok = 1;
	clock->ck_status = RTC_STATINIT;
	initial = (unsigned char)(clock->ck_counter[0] >> CLK_SHIFT);

	while (delay_count--)
	    if ((unsigned char)(clock->ck_counter[0] >> CLK_SHIFT) != initial)
		    return (0);

	rtclock_ok = 0;
	return (1);
}	/* counter_not_moving */
#endif
