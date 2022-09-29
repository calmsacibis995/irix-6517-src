/* ds1287.h - Dallas Semiconductor ds1287 parallel/timer chip header  */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/* Copyright 1988, Mizar, Inc. */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,02dec88,mcl  cosmetics.
01a,08jul88,mcl  written.
*/

#ifndef __INCds1287h
#define __INCds1287h

#ifdef __cplusplus
extern "C" {
#endif

/*
This file contains constants for the Dallas Semiconductor ds1287
real-time clock chip.
*/

/* equates for alarm registers */

#define	DISABLE_ALARM		0xff	/* sets any alarm byte to "don't care" */

/* equates for register A */

#define	FREQUENCY_NONE		0x00	/* NONE */
#define	FREQUENCY_8192		0x03	/* 8192 Hz, 122.070  usec */
#define	FREQUENCY_4096		0x04	/* 4096 Hz, 244.141  usec */
#define	FREQUENCY_2048		0x05	/* 2048 Hz, 488.281  usec */
#define	FREQUENCY_1024		0x06	/* 1024 Hz, 976.5625 usec */
#define	FREQUENCY_512		0x07	/*  512 Hz, 1.953125 msec */
#define	FREQUENCY_256		0x08	/*  256 Hz, 3.90625  msec */
#define	FREQUENCY_128		0x09	/*  128 Hz, 7.8125   msec */
#define	FREQUENCY_64		0x0a	/*   64 Hz, 15.625   msec */
#define	FREQUENCY_32		0x0b	/*   32 Hz, 31.25    msec */
#define	FREQUENCY_16		0x0c	/*   16 Hz, 62.5     msec */
#define	FREQUENCY_8		0x0d	/*    8 Hz, 125      msec */
#define	FREQUENCY_4		0x0e	/*    4 Hz, 250      msec */
#define	FREQUENCY_2		0x0f	/*    2 Hz, 500      msec */
#define	RTC_DISABLE		0x00
			    /* disable oscillator, disable count down chain */
#define	RTC_ENABLE		0x20
			    /* enable oscillator, enable count down chain */
#define	RTC_HOLD		0x60
			    /* enable oscillator, disable count down chain */
#define	UPDATE_IN_PROGRESS	0x80
			    /* 1: do not access chip, try again later */

/* equates for register B */

#define	DAYLIGHT_SAVINGS_EN	0x01	/* 1: enable daylight savings mode */
#define	HOUR_MODE_24		0x02	/* 24-hour mode */
#define	HOUR_MODE_12		0x00	/* 12-hour mode */
#define	DATA_MODE_BINARY	0x04	/* binary mode */
#define	DATA_MODE_DECIMAL	0x00	/* decimal mode */
#define	SQUARE_WAVE_EN		0x08	/* 1: drive SQWE pin at FREQUENCY */
#define	UIE_INTERRUPT_EN	0x10	/* 1: enable update-ended interrupt */
#define	ALARM_INTERRUPT_EN	0x20	/* 1: enable alarm interrupt */
#define	PERIODIC_INTERRUPT_EN	0x40	/* 1: enable periodic interrupt */
#define	SET_CLOCK		0x80	/* 1: hold clock so that it can be set */

/* equates for register C */

#define	UIE_INTERRUPT_FLAG	0x10	/* 1: update-ended interrupt active */
#define	ALARM_INTERRUPT_FLAG	0x20	/* 1: alarm interrupt active */
#define	PERIODIC_INTERRUPT_FLAG	0x40	/* 1: periodic interrupt active */
#define	INT_REQUEST_FLAG	0x80	/* 1: one of the above is true */

/* equates for register D */

#define	VALID_RAM_AND_TIME	0x80	/* 0: dead battery */

/* equates for nonvolatile RAM */

#define	DS1287_NVRAM_LEN	0x32	/* number of bytes of nonvolatile RAM */

#ifdef __cplusplus
}
#endif

#endif /* __INCds1287h */
