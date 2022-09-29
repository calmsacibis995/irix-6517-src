#ident	"$Id: date.s,v 1.1 1994/07/21 01:13:11 davidl Exp $"
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
#include "sys/sbd.h"

#include "monitor.h"				/* defines NVRAM_BASE */

#ifdef  R6000
#include "mk48t02.h"
#define	TODC_YEAR_OFFSET	0x7ff		/* year    00-99         */
#define	TODC_MONTH_OFFSET	0x7fe		/* month   01-12         */
#define	TODC_DAYM_OFFSET	0x7fd		/* date    01-31         */
#define	TODC_DAYW_OFFSET	0x7fc		/* day     01-07         */
#define	TODC_HOUR_OFFSET	0x7fb		/* hour    00-23         */
#define	TODC_MINS_OFFSET	0x7fa		/* min     00-59         */
#define	TODC_SECS_OFFSET	0x7f9		/* sec     00-59         */
#define	TODC_CNTL_OFFSET	0x7f8		/* control               */
#define THIS_CENTURY		1900
#endif

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
#define TODC_CONT_READ		WTR_TE		/* Sets TE bit 0x80 */
#define TODC_MONTH_MASK		0x1f
#define TODC_DAYM_MASK		0x3f
#define TODC_HOUR_MASK		0x3f
#define TODC_MINS_MASK		0x7f
#define TODC_SECS_MASK		0x7f
#endif


#define	SPACE_CHAR		0x20		/* space character       */
#define	SEMICOL_CHAR		0x3a		/* semicolon             */

#undef  LB_NVRAM
#define LB_NVRAM(_reg,x) \
        lbu     _reg,NVRAM_BYTE0(x);

#undef  SB_NVRAM
#define SB_NVRAM(_reg,x) \
        sb	_reg,NVRAM_BYTE0(x);

	.text
	.align	2

/*------------------------------------------------------------------------+
| Routine name: date( void )                                              |
|                                                                         |
| Description : Print Time-Of-Day information.  The format is similar to  |
|    that of the UNIX's date command.                                     |
|                                                                         |
| Register Usage:                                                         |
|       - t1  save return address       - a0  argument passing.           |
|                                                                         |
|    t0, v0-v1, a0-a3  are used byte putdec(), _puts() and _putchar().    |
+------------------------------------------------------------------------*/
LEAF(date)
	move	t1, ra			# saved return address.

	#+----------------------------------------------------------------+
	#| set read bit in control register to stop time-keeper registers |
	#| from being update while reading is in progress.                |
	#+----------------------------------------------------------------+
_set_rd_bit:
	LB_NVRAM(v0, TODC_CNTL_OFFSET)
	or	a0, v0, TODC_CONT_READ	# set read bit in control register.
	SB_NVRAM(a0, TODC_CNTL_OFFSET)

#ifdef R6000
	#+----------------------------------------------------------------+
	#| read and display day of the week.                              |
	#+----------------------------------------------------------------+
_disp_dayw:
	LB_NVRAM(v0, TODC_DAYW_OFFSET)

	and	v0, 0x07		# take modulo 7 of day.
	la	a0, daytbl		# get daytbl origin
	sll	v0, 2			#   offset to table.
	addu	v0, a0
	lw	a0, (v0)
	jal	_puts			#   display day of week.

#endif
	#+----------------------------------------------------------------+
	#| read and display month of year.                                |
	#+----------------------------------------------------------------+
_disp_month:
	LB_NVRAM(v0, TODC_MONTH_OFFSET)

	and	v0, TODC_MONTH_MASK	# mask off unwanted bits.
	and	a0, v0, 0x0f		# get least significant BCD
	srl	v0, 4
	mul	v0, 10			# calculate most significant BCD.
	add	v0, a0			# month = ((v0 >> 4)*10) + (v0 & 0x0f)

	bleu	v0, 12, 1f		# valid month?
	move	v0, zero
1:
	la	a0, montbl		# get montbl origin
	sll	v0, 2			#   offset to table.
	addu	v0, a0
	lw	a0, (v0)
	jal	_puts			#   display month of year.

	#+----------------------------------------------------------------+
	#| read and display date of the month.                            |
	#+----------------------------------------------------------------+
_disp_daym:
	LB_NVRAM(v0, TODC_DAYM_OFFSET)

	and	a0, v0, TODC_DAYM_MASK
	li	a1, 2
	jal	putbcd			# display date.

	la	a0, SPACE_CHAR
	jal	_putchar		# pad in a space char.

	#+----------------------------------------------------------------+
	#| read and display hours.                                        |
	#+----------------------------------------------------------------+
_disp_hour:
	LB_NVRAM(v0, TODC_HOUR_OFFSET)

	and	a0, v0, TODC_HOUR_MASK
	li	a1, 2
	jal	putbcd			# display hours.

	la	a0, SEMICOL_CHAR
	jal	_putchar		# pad in a ':' char.

	#+----------------------------------------------------------------+
	#| read and display minute.                                       |
	#+----------------------------------------------------------------+
_disp_min:
	LB_NVRAM(v0, TODC_MINS_OFFSET)

	and	a0, v0, TODC_MINS_MASK
	li	a1, 2
	jal	putbcd			# display hours.

	la	a0, SEMICOL_CHAR
	jal	_putchar		# pad in a ':' char.

	#+----------------------------------------------------------------+
	#| read and display seconds.                                      |
	#+----------------------------------------------------------------+
_disp_sec:
	LB_NVRAM(v0, TODC_SECS_OFFSET)

	and	a0, v0, TODC_SECS_MASK
	li	a1, 2
	jal	putbcd			# display hours.

	la	a0, GMTMsg		# display "Greenwich-Mean-Time" msg
	jal	_puts

	#+----------------------------------------------------------------+
	#| read and display year.                                         |
	#+----------------------------------------------------------------+
_disp_year:
	LB_NVRAM(v0, TODC_YEAR_OFFSET)

	and	a0, v0, 0x0f		# get least significant BCD
	srl	v0, 4
	mul	v0, 10			# calculate most significant BCD.
	add	v0, a0			# year = ((v0 >> 4)*10) + (v0 & 0x0f)

	bleu	v0, 99, 1f		# valid year?
	move	v0, zero
1:
	addi	a0, v0, THIS_CENTURY
	move	a1, zero
	jal	putudec			# display hours.

	#+----------------------------------------------------------------+
	#| reset read bit in control register to resume the updating of   |
	#| the time-keeper registers.                                     |
	#+----------------------------------------------------------------+
_reset_rd_bit:
	LB_NVRAM(v0, TODC_CNTL_OFFSET)

	li	a0, TODC_CONT_READ	# reset read bit in control register.
	not	a0

	and	a0, v0
	SB_NVRAM(a0, TODC_CNTL_OFFSET)

	j	t1
END(date)


/*------------------------------------------------------------------------+
| Month, Day tables                                                       |
+------------------------------------------------------------------------*/
	.data
	.align	2
montbl:
	.word	BadVal
	.word	JanMsg
	.word	FebMsg
	.word	MarMsg
	.word	AprMsg
	.word	MayMsg
	.word	JuneMsg
	.word	JulyMsg
	.word	AugMsg
	.word	SeptMsg
	.word	OctMsg
	.word	NovMsg
	.word	DecMsg
	.word	BadVal

daytbl:
	.word	BadVal
	.word	MonMsg
	.word	TuesMsg
	.word	WedMsg
	.word	ThursMsg
	.word	FriMsg
	.word	SatMsg
	.word	SunMsg
	.word	BadVal

GMTMsg:	.asciiz	" GMT "

BadMMsg: .asciiz  "BadMonCode "
JanMsg:	 .asciiz  "Jan "
FebMsg:	 .asciiz  "Feb "
MarMsg:	 .asciiz  "Mar "
AprMsg:	 .asciiz  "Apr "
MayMsg:	 .asciiz  "May "
JuneMsg: .asciiz  "Jun "
JulyMsg: .asciiz  "Jul "
AugMsg:	 .asciiz  "Aug "
SeptMsg: .asciiz  "Sep "
OctMsg:	 .asciiz  "Oct "
NovMsg:	 .asciiz  "Nov "
DecMsg:	 .asciiz  "Dec "

BadDMsg: .asciiz  "BadDayCode "
MonMsg:  .asciiz  "Mon "
TuesMsg: .asciiz  "Tue "
WedMsg:	 .asciiz  "Wed "
ThursMsg:.asciiz  "Thu "
FriMsg:  .asciiz  "Fri "
SatMsg:  .asciiz  "Sat "
SunMsg:  .asciiz  "Sun "

BadVal:	 .asciiz  "--- "

