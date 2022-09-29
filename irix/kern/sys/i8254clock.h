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

#ident	"$Revision: 3.23 $"

/*
 * General time definitions
 */
#define	SECMIN	((unsigned)60)			/* seconds per minute */
#define	SECHOUR	((unsigned)(60*SECMIN))		/* seconds per hour */
#define	SECDAY	((unsigned)(24*SECHOUR))	/* seconds per day */
#define	SECYR	((unsigned)(365*SECDAY))	/* sec per reg year */

#define	YRREF		1970
#define	LEAPYEAR(x)	(((x) % 4) == 0)
#define	TODRZERO	(1<<26)
#if IP20 || IP22 || IP26 || IP28
#define BASEYEAR	0
#else
#define	BASEYEAR	72			/* MUST be a leap year */
#endif

/*
 * Definitions for 8254 programmable interval timer
 *
 * NOTE: counter2 is clocked at MASTER_FREQ (defined below), the
 * output of counter2 is the clock for both counter0 and counter1.
 * For the IP20/IP22, counter0 output is tied to interrupt 2 for the
 * scheduling clock, and interrupt 3 for the 'fast' clock.
 * For all other CPUs, counter0 output is tied to Interrupt 2 to act 
 * as the scheduling clock and the output of counter1 is tied to 
 * Interrupt 4 to act as the profiling clock.
 */
#if !(IP20 || IP22 || IP26 || IP28)

#ifndef LANGUAGE_ASSEMBLY	/* included in some standalone assembler files */
struct pt_clock {
	volatile unsigned char	pt_counter0;		/* counter 0 port */
	char fill0[3];
	volatile unsigned char	pt_counter1;		/* counter 1 port */
	char fill1[3];
	volatile unsigned char	pt_counter2;		/* counter 2 port */
	char fill2[3];
	volatile unsigned char	pt_control;		/* control word */
};
#else
# define PT_COUNTER0	0		/* assembly structure offsets */
# define PT_COUNTER1	4
# define PT_COUNTER2	8
# define PT_CONTROL	12
#endif

#else /* !(IP20 || IP22 || IP26 || IP28) */

#ifdef _MIPSEB
#ifndef LANGUAGE_ASSEMBLY	/* included in some standalone assembler files */
struct pt_clock {
	char fill0[3];
	unsigned char	pt_counter0;		/* counter 0 port */
	char fill1[3];
	unsigned char	pt_counter1;		/* counter 1 port */
	char fill2[3];
	unsigned char	pt_counter2;		/* counter 2 port */
	char fill3[3];
	unsigned char	pt_control;		/* control word */
};
#else  /* LANGUAGE_ASSEMBLY */
# define PT_COUNTER0	3		/* assembly structure offsets */
# define PT_COUNTER1	7
# define PT_COUNTER2	11
# define PT_CONTROL	15
#endif /* LANGUAGE_ASSEMBLY */
#endif /* _MIPSEB */

#ifdef _MIPSEL
#ifndef LANGUAGE_ASSEMBLY	/* included in some standalone assembler files */
struct pt_clock {
	unsigned char	pt_counter0;		/* counter 0 port */
	char fill0[3];
	unsigned char	pt_counter1;		/* counter 1 port */
	char fill1[3];
	unsigned char	pt_counter2;		/* counter 2 port */
	char fill2[3];
	unsigned char	pt_control;		/* control word */
	char fill3[3];
};
#else  /* LANGUAGE_ASSEMBLY */
# define PT_COUNTER0	0		/* assembly structure offsets */
# define PT_COUNTER1	4
# define PT_COUNTER2	8
# define PT_CONTROL	12
#endif /* LANGUAGE_ASSEMBLY */
#endif /* _MIPSEL */

#endif /* !(IP20 || IP22 || IP26 || IP28) */

/*
 * control word definitions
 */
#define	PTCW_SC(x)	((x)<<6)	/* select counter x */
#define	PTCW_RBCMD	(3<<6)		/* read-back command */
#define	PTCW_CLCMD	(0<<4)		/* counter latch command */
#define	PTCW_LSB	(1<<4)		/* r/w least signif. byte only */
#define	PTCW_MSB	(2<<4)		/* r/w most signif. byte only */
#define	PTCW_16B	(3<<4)		/* r/w 16 bits, lsb then msb */
#define	PTCW_MODE(x)	((x)<<1)	/* set mode to x */
#define	PTCW_BCD	0x1		/* operate in BCD mode */

/*
 * Mode definitions
 */
#define	MODE_ITC	0		/* interrupt on terminal count */
#define	MODE_HROS	1		/* hw retriggerable one-shot */
#define	MODE_RG		2		/* rate generator */
#define	MODE_SQW	3		/* square wave generator */
#define	MODE_STS	4		/* software triggered strobe */
#define	MODE_HTS	5		/* hardware triggered strobe */

#if IP20 || IP22 || IP26 || IP28
#define MASTER_FREQ	1000000		/* master frequency */
#endif
#if !defined(MASTER_FREQ)
#define	MASTER_FREQ	3686400		/* master frequency */
#endif

/* 
 * prom non-volatile ram conventions
 */
#define NVTOD_VALID	1	/* flag bit that indicates clock ok if set */
