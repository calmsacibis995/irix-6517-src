#ifndef __SYS_DS1286CLK_H__
#define __SYS_DS1286CLK_H__

#if defined(_LANGUAGE_C)
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.11 $"

struct ds1286_clk {
	volatile unsigned int hundreth_sec;	/* .01 second register */
	volatile unsigned int sec;		/* second register */
	volatile unsigned int min;		/* minute register */
	volatile unsigned int min_alarm;     	/* minute alaram register */
	volatile unsigned int hour;   		/* hour register */
	volatile unsigned int hour_alarm;   	/* hour alarm register */
	volatile unsigned int day;   		/* day register */
	volatile unsigned int day_alarm;   	/* day alarm register */
	volatile unsigned int date;   		/* date register */
	volatile unsigned int month;   		/* month register */
	volatile unsigned int year;   		/* year register */
	volatile unsigned int command;   	/* command register */
	volatile unsigned int watch_hundreth_sec;   	/* watchdog alarm in .01 							   seconds register 
							*/
	volatile unsigned int watch_sec;   	/* watchdog alarm in seconds 
						   register 
						*/
	volatile unsigned int ram[50];	    	/* user register */
};

typedef struct ds1286_clk      ds1286_clk_t;

/* The DS1386 comes in both 8K and 32K flavors.
 * The 8K version is used in Guinness.
 */
#define	DS1386_NVRAM_SIZE	8192-sizeof(ds1286_clk_t)/sizeof(int)
struct ds1386_clk {
	ds1286_clk_t ds1286;			/* compatible with ds1286 */
	volatile unsigned int nvram[DS1386_NVRAM_SIZE]; /* non-volatile ram */
};
typedef struct ds1386_clk      ds1386_clk_t;

#endif

#define DALLAS_YRREF		1940		/* must be a leap year */

/* Offset of 1[23]86 ram over clock control registers
 */
#define DS1286_RAMOFFSET	0x38

/* 
 * Battery backup (real time) clock (accessed through HPC3)
 */
#define RTDS_CLOCK_ADDR   RT_CLOCK_ADDR

#define WTR_READ	0		/* _clock_func read command */
#define WTR_WRITE	1		/* _clock_func write command */

/* Clock dependent defs for Watchdog Timekeeper Registers */
#define WTR_TDF			0x01	/* time of day alarm has occured (ro) */
#define WTR_WAF			0x02	/* watchdog alarm has occured (ro) */
#define WTR_DEAC_TDM		0x04	/* deactivates time of day interrupt */
#define WTR_DEAC_WAM		0x08	/* deactivates watchdog interrupt */
#define WTR_PULSE_MODE_INT	0x10	/* both intrs pulse on activation */
#define WTR_SOURCE_INTB		0x20	/* INTB -> source/sink current */
#define WTR_TIME_DAY_INTA	0x40	/* INTA -> time of day alarm intr */
#define	WTR_EOSC_N		0x80    /* disable clock oscillator */
#define	WTR_TE			0x80    /* disable counters updating */

/* The following are used by timer on INT2 */
#define RTC_RS          	(1 << (CLK_SHIFT + 6))  /* main status reg:*/
#define RTC_TIMSAVON    	(1 << (CLK_SHIFT + 7)) 	/* enable tim select */

#endif /* __SYS_DS1286CLK_H__ */
