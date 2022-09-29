#ident "$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/include/RCS/mk48t02.h,v 1.1 1994/07/20 22:54:59 davidl Exp $"
/* $Copyright$ */

/*
 * Definitions for use MK48T02 real time clock and nvram
 */

#ifndef FILL3
#define	FILL3(x)	char fill_/**/x[3]
#endif

#define TODC_NVRAM_SIZE		0x7f8	/* NVRAM size on this chip */
#define TODC_MEM_OFFSET		0x3	/* NVRAM byte offset */
#define TODC_MEMX(x)		(x<<2)	/* For byte access */

#ifdef LANGUAGE_C
struct todc_clock {
    struct todc_mem {
	FILL3(0);			/* byte-wide device */
	u_char	value;
    } todc_mem[TODC_NVRAM_SIZE];	/* Non-volitile ram */
    FILL3(1);
    u_char	todc_control;		/* control register */
    FILL3(2);
    u_char	todc_secs;		/* seconds */
    FILL3(3);
    u_char	todc_mins;		/* minutes */
    FILL3(4);
    u_char	todc_hours;		/* hour */
    FILL3(5);
    u_char	todc_dayw;		/* day of the week */
    FILL3(6);
    u_char	todc_daym;		/* day of the month */
    FILL3(7);
    u_char	todc_month;		/* month */
    FILL3(8);
    u_char	todc_year;		/* year */
};
#endif

/*
 * Various defines for specific bits in registers
 */

#define TODC_CONT_WRITE		(1<<7)	/* Write bit in control register */
#define TODC_CONT_READ		(1<<6)	/* Read bit in control register */
#define TODC_CONT_SIGN		(1<<5)	/* Sign bit in control register */

#define TODC_SECS_STOP		(1<<7)	/* Stop bit in seconds register */

#define TODC_HOUR_KICK		(1<<7)	/* Kick start bit in hour register */

#define TODC_DAYW_FREQTEST	(1<<6)	/* Frequency test bit in day of week */

/*
 * Mask bits to mask off unneeded bits from data
 */

#define TODC_SECS_MASK		0x7f	/* STOP bit from seconds */
#define TODC_MINS_MASK		0x7f	/* High bit from minutes */
#define TODC_HOUR_MASK		0x3f	/* Kick/X bit from hours */
#define TODC_DAYW_MASK		0x07	/* Freq/X bits from day of week */
#define TODC_DAYM_MASK		0x3f	/* High bits from day of month */
#define TODC_MONTH_MASK		0x1f	/* High bits from month */


