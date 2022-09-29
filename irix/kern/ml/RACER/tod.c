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

#ident "$Revision: 1.31 $"

/* tod.c - Dallas ds1687 time of day clock handling, and auto-power-on */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/ktime.h>

#include <sys/ds1687clk.h>
#include <sys/RACER/IP30nvram.h>

#include "RACERkern.h"

static int	_clock_func(int func, unsigned short *ptr);
static void	secs_to_calendar_time(int, ushort_t *, ushort_t *,
			ushort_t *, ushort_t *, ushort_t *, ushort_t *);

extern u_char miniroot;

/* +4 since jan 1, 1970 is a thursday, +1 since sunday is day 1 */
#define dayofweek(secs)		(((secs / SECDAY) + 4) % 7 + 1)

lock_t ip30clocklock;

static int month_secs[2][12] = {
	{
		31 * SECDAY,	/* January */
		28 * SECDAY,	/* February */
		31 * SECDAY,	/* March */
		30 * SECDAY,	/* April */
		31 * SECDAY,	/* May */
		30 * SECDAY,	/* June */
		31 * SECDAY,	/* July */
		31 * SECDAY,	/* August */
		30 * SECDAY,	/* September */
		31 * SECDAY,	/* October */
		30 * SECDAY,	/* November */
		31 * SECDAY	/* December */
	},
	{
		31 * SECDAY,	/* January */
		29 * SECDAY,	/* February (leap) */
		31 * SECDAY,	/* March */
		30 * SECDAY,	/* April */
		31 * SECDAY,	/* May */
		30 * SECDAY,	/* June */
		31 * SECDAY,	/* July */
		31 * SECDAY,	/* August */
		30 * SECDAY,	/* September */
		31 * SECDAY,	/* October */
		30 * SECDAY,	/* November */
		31 * SECDAY	/* December */
	}
};

/* read the time of day clock */
int
rtodc(void)
{
	int month, year, secs, ly;
	unsigned short buf[7];
	int i;

	while (!_clock_func(RTC_READ, buf))
		DELAY(100);

	/* Sum up seconds from 00:00:00 GMT 1970
	 */
	secs  = buf[0];
	secs += buf[1] * SECMIN;
	secs += buf[2] * SECHOUR;
	secs += (buf[3]-1) * SECDAY;

	month = buf[4];
	year  = buf[5];

	ly = LEAPYEAR(year);
	for (i = 0; i < month-1; i++)
		 secs += month_secs[ly][i];

	while (--year >= YRREF) {
		secs += SECYR;
		if (LEAPYEAR(year))
			secs += SECDAY;
	}

	return secs;
}

/* write the time of day clock */
void
wtodc(void)
{
	unsigned short buf[7];

	secs_to_calendar_time(time,
		&buf[5], &buf[4], &buf[3], &buf[2], &buf[1], &buf[0]);
	buf[6] = dayofweek(time);

	(void)_clock_func(RTC_WRITE, buf);
}

/* read/write time register from/to the ds1687 */
static int
_clock_func(int func, unsigned short *ptr)
{
	ioc3reg_t	old;
	int		s = mutex_spinlock_spl(&ip30clocklock, splprof);

	old = slow_ioc3_sio();

	if (func == RTC_READ) {
		ASSERT((RD_DS1687_RTC(RTC_CTRL_B) & RTC_INHIBIT_UPDATE) == 0x0);

		/* access all registers through bank 1 */
		WR_DS1687_RTC(RTC_CTRL_A,
			RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);

		/*  Use RTC_INCR_IN_PROGRESS instead of RTC_UPDATING 
		 * so we don't need to wait as long in case the bit is set
		 */
		if (RD_DS1687_RTC(RTC_X_CTRL_A) & RTC_INCR_IN_PROGRESS) {
			restore_ioc3_sio(old);
			mutex_spinunlock(&ip30clocklock, s);
			return 0;
		}

		ptr[0] = RD_DS1687_RTC(RTC_SEC);
		ptr[1] = RD_DS1687_RTC(RTC_MIN);
		ptr[2] = RD_DS1687_RTC(RTC_HOUR);
		ptr[3] = RD_DS1687_RTC(RTC_DATE);
		ptr[4] = RD_DS1687_RTC(RTC_MONTH);
		/*
		 * do not combine the next 2 lines of code, otherwise, the
		 * compiler may choose to write to the address port twice,
		 * then read from the data port twice which, of course,
		 * will get the wrong result
		 */
		ptr[5] = RD_DS1687_RTC(RTC_YEAR);
		ptr[5] += RD_DS1687_RTC(RTC_CENTURY) * 100;
		ptr[6] = RD_DS1687_RTC(RTC_DAY);
	}
	else {
		WR_DS1687_RTC(RTC_CTRL_A,
			RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);
		WR_DS1687_RTC(RTC_CTRL_B, RTC_INHIBIT_UPDATE |
			RTC_BINARY_DATA_MODE | RTC_24HR_MODE);

		/* set the clock */
		WR_DS1687_RTC(RTC_SEC, *ptr++);
		WR_DS1687_RTC(RTC_MIN, *ptr++);
		WR_DS1687_RTC(RTC_HOUR, *ptr++);
		WR_DS1687_RTC(RTC_DATE, *ptr++);
		WR_DS1687_RTC(RTC_MONTH, *ptr++);
		WR_DS1687_RTC(RTC_YEAR, *ptr % 100);
		WR_DS1687_RTC(RTC_CENTURY, *ptr++ / 100);
		WR_DS1687_RTC(RTC_DAY, *ptr++);

		/* 24-hour mode, use binary date for input/output */
		WR_DS1687_RTC(RTC_CTRL_B, RTC_BINARY_DATA_MODE | RTC_24HR_MODE);

		/* clear all pending interrupt status */
		WR_DS1687_RTC(RTC_X_CTRL_A, 0x0);
		(void)RD_DS1687_RTC(RTC_CTRL_C);
	}

	restore_ioc3_sio(old);

	mutex_spinunlock(&ip30clocklock, s);

	return 1;
}


/*  Called from prom_reinit(), in case syssgi(SET_AUTOPWRON) was done.
 * If time of the day alarm vars are to be set then program RTC to
 * power system back on using time-of-day alarm, unless the current
 * time is already past that value.
 */
void
set_autopoweron(void)
{
	extern time_t	autopoweron_alarm;
	ushort_t	_year, _month, _day, _hour, _min, _sec;
	ushort_t	year, month, day, hour, min, sec;
	int		leap_year;

	/* return if autopoweron is not requested, or time has already past.
	 */
	if (time > autopoweron_alarm)
		return;

	secs_to_calendar_time(time,&_year,&_month,&_day,&_hour,&_min,&_sec);
	secs_to_calendar_time(autopoweron_alarm,&year,&month,&day,&hour,&min,
			      &sec);

	leap_year = LEAPYEAR(_year);
	if (autopoweron_alarm - time < month_secs[leap_year][month - 1]) {
		ioc3reg_t	old;
		uchar_t		rtc_ctrl;
		int		s = mutex_spinlock_spl(&ip30clocklock, splprof);

		old = slow_ioc3_sio();

		/* access all registers through bank 1 */
		WR_DS1687_RTC(RTC_CTRL_A, RTC_SELECT_BANK_1|RTC_OSCILLATOR_ON);

		WR_DS1687_RTC(RTC_SEC_ALARM, sec);
		WR_DS1687_RTC(RTC_MIN_ALARM, min);
		WR_DS1687_RTC(RTC_HOUR_ALARM, hour);
		WR_DS1687_RTC(RTC_DATE_ALARM, day);

		rtc_ctrl = RD_DS1687_RTC(RTC_X_CTRL_B);
		rtc_ctrl |= RTC_WAKEUP_ALARM_INTR_ENABLE |
			    RTC_KICKSTART_INTR_ENABLE;	/* always write KSE */
		WR_DS1687_RTC(RTC_X_CTRL_B, rtc_ctrl);

		restore_ioc3_sio(old);

		mutex_spinunlock(&ip30clocklock, s);
	}
}

/* break down seconds since YRREF to year/month/day/hour/min/sec */
static void
secs_to_calendar_time(
	int year_secs,
	ushort_t *year,
	ushort_t *month,
	ushort_t *day,
	ushort_t *hour,
	ushort_t *min,
	ushort_t *sec)
{
	int leap_year;

	*year = YRREF;
	while (1) {
		int secyr = SECYR;

		if (LEAPYEAR(*year))
			secyr += SECDAY;
		if (year_secs < secyr)
			break;
		year_secs -= secyr;
		(*year)++;
	}

	leap_year = LEAPYEAR(*year);
	for (*month = 0; year_secs >= month_secs[leap_year][*month];
	     year_secs -= month_secs[leap_year][(*month)++])
		;
	(*month)++;

	*day = year_secs / SECDAY;
	(*day)++;
	year_secs %= SECDAY;

	*hour = year_secs / SECHOUR;
	year_secs %= SECHOUR;

	*min = year_secs / SECMIN;
	*sec = year_secs % SECMIN;
}

/*  Called from clock() periodically to see if system time is drifting from
 * the real time clock chip.  Only do if different by two seconds or more.
 * One second difference often occurs due to slight differences between
 * kernel and chip--two to play it safe.
 */
void
chktimedrift(void)
{
	int todtimediff;

	todtimediff = rtodc() - time;
	if ((todtimediff > 2) || (todtimediff < -2))
		(void)doadjtime(todtimediff * USEC_PER_SEC);
}

void
cpu_clearpower(void)
{
	int		s = mutex_spinlock_spl(&ip30clocklock, splprof);
	ioc3reg_t	old;

	old = slow_ioc3_sio();

	WR_DS1687_RTC(RTC_CTRL_A, RTC_BANK(RTC_X_CTRL_A) | RTC_OSCILLATOR_ON);
	WR_DS1687_RTC(RTC_X_CTRL_A, 0x0);
	flushbus();

	restore_ioc3_sio(old);

	mutex_spinunlock(&ip30clocklock, s);
}

/* function to shut off flat panel, set in device independent graphics main initializiation routines  */
int (*fp_poweroff)(void);

/*  Can be called via a timeout from powerintr(), direct from prom_reboot
 * in arcs.c, or from mdboot() as a result of a uadmin call.
 */
void
cpu_soft_powerdown(void)
{
	uchar_t rtc_ctrl;

	/* shut off the flat panel, if one attached */
	if (fp_poweroff)
	  (*fp_poweroff)();

	/* Make sure seral console has drained */
	conbuf_flush(CONBUF_UNLOCKED|CONBUF_DRAIN);

	/* Remove heart interrupt.  For early interrupts, there is not
	 * currently a 'clean' way to do this.
	 */
	heart_imr_bits_rmw(master_procid, 1L<<IP30_HVEC_ACFAIL, 0);

	(void)mutex_spinlock_spl(&ip30clocklock, splprof);
	(void)slow_ioc3_sio();

	/* Make sure the kickstart is enabled, preserve RTC_WAKEUP_ALARM_INTR.
	 */
	WR_DS1687_RTC(RTC_CTRL_A, RTC_BANK(RTC_X_CTRL_A) | RTC_OSCILLATOR_ON);
	rtc_ctrl = RD_DS1687_RTC(RTC_X_CTRL_B);
	/* preserve RTC_CRYSTAL_125_PF and RTC_WAKEUP_ALARM_INTR */
	rtc_ctrl &= RTC_CRYSTAL_125_PF | RTC_WAKEUP_ALARM_INTR |
		    RTC_PWR_ACTIVE_ON_FAIL;
	rtc_ctrl |= RTC_KICKSTART_INTR_ENABLE | RTC_AUX_BAT_ENABLE;
	WR_DS1687_RTC(RTC_X_CTRL_B, rtc_ctrl);

	while (1) {
		/* Poweroff the system, then latch the scratch register to
		 * redirect any power down noise.  Wait 10 seconds.  If we
		 * didn't power-off in that amount of time, try again as it
		 * may have tripped before we lost enough power to start over.
		 * The write to RTC_X_CTRL_A will clear this interrupt.
		 */
		WR_DS1687_RTC(RTC_X_CTRL_A, RTC_POWER_INACTIVE);
		(void)RD_DS1687_RTC(NVRAM_REG_OFFSET);
		us_delay(10 * 1000 * 1000);
	}
	/* NOTREACHED + ip30clocklock is never released */
}

/* Set rtc rd/wr pulse width ~135ns.  Should hold ip30clocklock when calling.
 */
ioc3reg_t
slow_ioc3_sio(void)
{
	volatile ioc3reg_t *ptr = (volatile ioc3reg_t *)
		(IOC3_PCI_DEVIO_K1PTR + IOC3_SIO_CR);
	ioc3reg_t old;

	old = *ptr;
	*ptr = (old & ~SIO_SR_CMD_PULSE) | (0x8 << SIO_CR_CMD_PULSE_SHIFT);

	return old;
}

/* Reset superIO/parallel rd/wr pulse width ~75ns */
void
restore_ioc3_sio(ioc3reg_t old)
{
	/* latch scratch register to redirect any power down noise */
	(void)RD_DS1687_RTC(NVRAM_REG_OFFSET);

	*(volatile ioc3reg_t *)(IOC3_PCI_DEVIO_K1PTR + IOC3_SIO_CR) = old;
}
