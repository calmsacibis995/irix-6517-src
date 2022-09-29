#ident	"$Id: set_tod.s,v 1.1 1994/07/21 01:16:07 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"

#ifdef R6000

#include "mk48t02.h"
#define	TODC_YEAR_OFFSET	0x7ff		/* year    00-99         */
#define	TODC_MONTH_OFFSET	0x7fe		/* month   01-12         */
#define	TODC_DAYM_OFFSET	0x7fd		/* date    01-31         */
#define	TODC_DAYW_OFFSET	0x7fc		/* day     01-07         */
#define	TODC_HOUR_OFFSET	0x7fb		/* hour    00-23         */
#define	TODC_MINS_OFFSET	0x7fa		/* min     00-59         */
#define	TODC_SECS_OFFSET	0x7f9		/* sec     00-59         */
#define	TODC_CNTL_OFFSET	0x7f8		/* control               */

#endif /* R6000 */

#if	defined(IP24) | defined(IP22)

#include "sys/ds1286.h"
#define	TODC_YEAR_OFFSET	0xa		/* year    00-99         */
#define	TODC_MONTH_OFFSET	0x9		/* month   01-12         */
#define	TODC_DAYM_OFFSET	0x8		/* date    01-31         */
#define	TODC_DAYW_OFFSET	0x6		/* day     01-07         */
#define	TODC_HOUR_OFFSET	0x4		/* hour    00-23         */
#define	TODC_MINS_OFFSET	0x2		/* min     00-59         */
#define	TODC_SECS_OFFSET	0x1		/* sec     00-59         */
#define	TODC_CNTL_OFFSET	0xb		/* control               */
#define THIS_CENTURY		1970
#define TODC_CONT_WRITE		0		/* Sets TE bit 0x80 */
#define TODC_MONTH_MASK		0x1f
#define TODC_DAYM_MASK		0x3f
#define TODC_HOUR_MASK		0x3f
#define TODC_MINS_MASK		0x7f
#define TODC_SECS_MASK		0x7f
#define TODC_HOUR_KICK		0x0		/* Starts the oscillator */
#endif

	.text

/*------------------------------------------------------------------------+
| void set_tod(unsigned formated_date)                                    |
|									  |
| Description : Set TOD clock chip. This routine takes year, month, date, |
|    day and time information in a compressed format as follow:           |
|                                                                         |
|       - first argument                - second argument                 |
|       +------+------+------+------+   +------+------+------+------+     |
|       | YEAR |  MON | DATE |  DAY |   |  NU  | HOUR |  MIN | SEC  |     |
|       +------+------+------+------+   +------+------+------+------+     |
|       31   24 23  16 15   8 7    0     31  24 23  16 15   8 7    0      |
|                                                                         |
|    Note that the year, month, date, day, hour, minute, and second are   |
|    Binary-Coded-Decimal number NOT HEX-NUMBER.                          |
|                                                                         |
| Register Usage:                                                         |
|       - a0  year/month/date/day       - a1  not-used/hour/min/second.   |
|       - a2, a3  scratch register.     - t0  saved return value.         |
|       - t1  saved argument 1.         - t2  saved argument 2.           |
|       - t3  saved content clock calibrate in control register.          |
|	- v0 & v1							  |
+------------------------------------------------------------------------*/
LEAF(set_tod)
	move	t0, ra			# saved return address.
	move	t1, a0			# saved argument register.
	move	t2, a1

	#+----------------------------------------------------------------+
	#| read and save the clock calibration part of the control reg.   |
	#+----------------------------------------------------------------+
	li	a0, TODC_CNTL_OFFSET
	jal	lb_nvram
	and	t3, v0, 0x3f		# save sign bit and calib. value

	#+----------------------------------------------------------------+
	#| step # 1: set write bit in control register to "1".            |
	#+----------------------------------------------------------------+
_step_1:
	li	a0, TODC_CONT_WRITE	# set write bit in control register.
	li	a1, TODC_CNTL_OFFSET
	jal	sb_nvram

#ifdef R6000
	#+----------------------------------------------------------------+
	#| step # 2: reset the stop bit.                                  |
	#+----------------------------------------------------------------+
_step_2:
	move	a0, zero		# reset stop bit in seconds reg.
	li	a1, TODC_SECS_OFFSET
	jal	sb_nvram

#endif /* R6000 */

	#+----------------------------------------------------------------+
	#| step # 3: set kick-Start bit in hours register to "1".         |
	#+----------------------------------------------------------------+
_step_3:
	li	a0, TODC_HOUR_KICK	# set kick-start bit in hours reg.
	li	a1, TODC_HOUR_OFFSET
	jal	sb_nvram

	#+----------------------------------------------------------------+
	#| step # 4: reset write bit in control register.                 |
	#+----------------------------------------------------------------+
_step_4:
#ifdef R6000
	move	a0, zero		# reset write bit in control register.
#endif
#if	defined(IP24) | defined(IP24)
	li	a0, WTR_TE
#endif
	li	a1, TODC_CNTL_OFFSET
	jal	sb_nvram

	#+----------------------------------------------------------------+
	#| step # 5: wait for 2 second.                                   |
	#+----------------------------------------------------------------+
	li	v0, 0x100000
_step_5:
	sub	v0, 1
	bne	v0, zero, _step_5	# delay for awhile.

	#+----------------------------------------------------------------+
	#| step # 6: set write bit in control register to "1".           |
	#+----------------------------------------------------------------+
_step_6:
	li	a0, TODC_CONT_WRITE	# set write bit in control register.
	li	a1, TODC_CNTL_OFFSET
	jal	sb_nvram

	#+----------------------------------------------------------------+
	#| step # 7: reset kick-Start bit in hours register.              |
	#+----------------------------------------------------------------+
_step_7:
	move	a0, zero		# reset kick-start bit in hours reg.
	li	a1, TODC_HOUR_OFFSET
	jal	sb_nvram

	#+----------------------------------------------------------------+
	#| step # 8: set time and date register.                          |
	#+----------------------------------------------------------------+
_step_8:
#ifdef R6000
	and	a0, t1, TODC_DAYW_MASK	# set day-of-week.
	li	a1, TODC_DAYW_OFFSET
	jal	sb_nvram
#endif

	srl	t1, t1, 8		# align data
	and	a0, t1, TODC_DAYM_MASK	# set day-of-month.
	li	a1, TODC_DAYM_OFFSET
	jal	sb_nvram

	srl	t1, t1, 8		# align data
	and	a0, t1, TODC_MONTH_MASK	# set month.
	li	a1, TODC_MONTH_OFFSET
	jal	sb_nvram

	srl	a0, t1, 8		# align data
	li	a1, TODC_YEAR_OFFSET
	jal	sb_nvram		# set year.

	and	a0, t2, TODC_SECS_MASK	# set second.
	li	a1, TODC_SECS_OFFSET
	jal	sb_nvram

	srl	t2, t2, 8		# align data
	and	a0, t2, TODC_MINS_MASK	# set minutes.
	li	a1, TODC_MINS_OFFSET
	jal	sb_nvram

	srl	t2, t2, 8		# align data
	and	a0, t2, TODC_HOUR_MASK	# set hours.
	li	a1, TODC_HOUR_OFFSET
	jal	sb_nvram

	#+----------------------------------------------------------------+
	#| step # 9: reset write bit in control register.                 |
	#+----------------------------------------------------------------+
_step_9:
	and	a0, t3, 0x3f		# restore clock calibration value 
	li	a1, TODC_CNTL_OFFSET
	jal	sb_nvram

	j	t0
END(set_tod)
