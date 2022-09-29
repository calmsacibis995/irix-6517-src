#ifndef __SYS_DP8573CLK_H__
#define __SYS_DP8573CLK_H__

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
#ident "$Revision: 1.10 $"

struct dp8573_clk {
	volatile unsigned int ck_status;	    /* main status reg */
	union {
	    struct {
		int fill[2];
		volatile unsigned int ck_pflag;    /* periodic flag */
		volatile unsigned int ck_tsavctl;  /* time save control */
	    } rs0;
	    struct {
		volatile unsigned int ck_rtime;     /* real time mode */
		volatile unsigned int ck_outmode;   /* output mode */
		volatile unsigned int ck_int0ctl;   /* interrupt control 0 */
		volatile unsigned int ck_int1ctl;   /* interrupt control 1 */
	    } rs1;
	} rs;
	volatile unsigned int ck_counter[10];	    /* the counters */
	int fill1[4];
	volatile unsigned int ck_cmpram[6];	    /* time compare ram */
	volatile unsigned int ck_timsav[5];	    /* time save ram */
	volatile unsigned int ram[2];			    /* ram */
};

#define ck_pflag0	rs.rs0.ck_pflag
#define ck_tsavctl0	rs.rs0.ck_tsavctl
#define ck_rtime1	rs.rs1.ck_rtime
#define ck_outmode1	rs.rs1.ck_outmode
#define ck_int0ctl1	rs.rs1.ck_int0ctl
#define ck_int1ctl1	rs.rs1.ck_int1ctl

#define RTC_READ	0		/* _clock_func read command */
#define RTC_WRITE	1		/* _clock_func write command */

#if defined(IP20)
#define	CLK_SHIFT	0		/* IP20 uses the LSB */
#else
#define	CLK_SHIFT	24		/* IP6 uses the MSB */
#endif /* IP20 */

/* Clock dependent defs */
#define RTC_24H		0		/* nop: for compatib with IP4 code */
#define RTC_RUN		(3 << (CLK_SHIFT + 3))	/* real time mode reg: clock on! */
#define RTC_RS		(1 << (CLK_SHIFT + 6))	/* main status reg: reg set select */
#define RTC_TIMSAVON	(1 << (CLK_SHIFT + 7))	/* time save ctl: enable time save */
#define RTC_TIMSAVOFF	0		/* time save ctl: disable time save */
#define RTC_STATINIT	0		/* status register: initial value */
#define RTC_INT1INIT	(0x80 << CLK_SHIFT)	/* interrup 1 ctl: initial value */
#define RTC_PFLAGINIT	0		/* periodic flag: intial value */
#define RTC_OUTMODINIT	0		/* output mode: initial value */
#define RTC_BADCLOCK	(1 << CLK_SHIFT)	/* store flag saying clock is bad */

#endif /* __SYS_DP8573CLK_H__ */
