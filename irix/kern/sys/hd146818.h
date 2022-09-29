/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 * 		 Copyright (C) 1988, MIPS Computer Systems, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.5 $"

/*
 * Definitions for use HD146818 real time clock
 */
#ifdef __ANSI_CPP__
#define RTFILL3(x)	char fill_ ##x[3]
#else
#define	RTFILL3(x)	char fill_/**/x[3]
#endif

struct rt_clock {
	unsigned char	rt_secs;		/* current seconds */
	RTFILL3(0);
	unsigned char	rt_seca;		/* alarm seconds */
	RTFILL3(1);
	unsigned char	rt_mins;		/* current minutes */
	RTFILL3(2);
	unsigned char	rt_mina;		/* alarm minutes */
	RTFILL3(3);
	unsigned char	rt_hours;		/* current hours */
	RTFILL3(4);
	unsigned char	rt_houra;		/* alarm hours */
	RTFILL3(5);
	unsigned char	rt_dayw;		/* day of the week */
	RTFILL3(6);
	unsigned char	rt_daym;		/* day of the month */
	RTFILL3(7);
	unsigned char	rt_month;		/* month */
	RTFILL3(8);
	unsigned char	rt_year;		/* year */
	RTFILL3(9);
	unsigned char	rt_rega;		/* register a */
	RTFILL3(10);
	unsigned char	rt_regb;		/* register b */
	RTFILL3(11);
	unsigned char	rt_regc;		/* register c */
	RTFILL3(12);
	unsigned char	rt_regd;		/* register d */
	RTFILL3(13);
	unsigned char	rt_mem[50*4];		/* general purpose battery-bkup ram */
};

#define	RT_MEMX(x)	((x)<<2)

/*
 * Register A bit definitions
 */
#define	RTA_UIP		0x80		/* update in progress */
#define	RTA_DV4M	(0<<4)		/* time base is 4.194304 MHz */
#define	RTA_DV1M	(1<<4)		/* time base is 1.048576 MHz */
#define	RTA_DV32K	(2<<4)		/* time base is 32.768 kHz */
#define	RTA_DVRESET	(7<<4)		/* reset divider chain */
#define	RTA_RSNONE	0		/* disable periodic intr and SQW */

/*
 * Register B bit definitions
 */
#define	RTB_SET		0x80		/* inhibit date update */
#define	RTB_PIE		0x40		/* enable periodic interrupt */
#define	RTB_AIE		0x20		/* enable alarm interrupt */
#define	RTB_UIE		0x10		/* enable update-ended interrupt */
#define	RTB_SQWE	0x08		/* square wave enable */
#define	RTB_DMBINARY	0x04		/* binary data (0 => bcd data) */
#define	RTB_24HR	0x02		/* 24 hour mode (0 => 12 hour) */
#define	RTB_DSE		0x01		/* daylight savings mode enable */

/*
 * Register C bit definitions
 */
#define	RTC_IRQF	0x80		/* interrupt request flag */
#define	RTC_PF		0x40		/* periodic interrupt flag */
#define	RTC_AF		0x20		/* alarm interrupt flag */
#define	RTC_UF		0x10		/* update-ended interrupt flag */

/*
 * Register D bit definitions
 */
#define	RTD_VRT		0x80		/* valid RAM and time flag */
