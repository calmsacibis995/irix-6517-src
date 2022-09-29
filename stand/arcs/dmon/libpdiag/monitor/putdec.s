#ident	"$Id: putdec.s,v 1.1 1994/07/21 01:15:33 davidl Exp $"
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


#include "regdef.h"
#include "asm.h"

#define	MAX_DIVISOR	1000000000	/* bigest divisor for integer.  */
#define MAX_DIGITS	10		/* maximun number of digits     */

#define	NEG_CHAR	0x2d		/* character '-'                */
#define	CHAR_0		0x30		/* character '0'                */
#define CHAR_SP		0x20		/* <sp>				*/

	.text
	.align	2

/*------------------------------------------------------------------------+
| void putdec(int value, int min_width)             	  		  |
| 		                                                          |
| Description : prints signed decimal number with optional width. Right   |
| 		justified.                                                |
| 		                                                          |
| Note: Assume that _putchar() only clobbers a0-a3.			  |
| 		                                                          |
| Register Usage:                                                         |
|       - a0  decimal number/quotient.  - a1  min widths/scratch reg.     |
|       - v0  remainder.                - v1  devisor.                    |
|       - t0  saved return address.	                                  |
+------------------------------------------------------------------------*/
LEAF(putdec)
	move	t0, ra			# save return address
	move	v0, a0			# save a0/a1 before _putchar()
	move	v1, a1

	/*----------------------------------------------------------------+
	| check for neg number. If found, take absolute value & print '-' |
	+----------------------------------------------------------------*/
_pd_sign:
	bge	v0, zero, _pd_start	# if (num is negative number) then
	not	v0, a0			#    take 2's complement of number
	addiu	v0, 1
	li	a0, NEG_CHAR		# print '-' sign
	jal	_putchar		# clobbers a0-a3

_pd_start:
	ble	v1, zero, _pd_divisor	# check for valid min. width
	bgt	v1, MAX_DIGITS, _pd_divisor # > MAX_DIGITS ? 

	/*----------------------------------------------------------------+
	| prefix spaces to right justify.  				  |
	+----------------------------------------------------------------*/
	move	a0, v0			# get number of digits
	jal	ndigits			# clobber no regs & return in v0
	sub	v1, v1, v0		# get no. of spaces to prefix
	move	v0, a0			# save a0 in v0
	ble	v1, zero, _pd_divisor	# 

	/*
	 * prefix spaces to right justify
	 */
1:
	li	a0, CHAR_SP		# print space to right justify
	jal	_putchar		# clobbers only a0-a3
	sub	v1, 1
	bne	v1, zero, 1b

	/*
 	 * calculate starting divisor to use
	 */
_pd_divisor:
	move	a0, v0			# get number of digits
	jal	ndigits			# a0 is unchanged
	subu	v0, v0, 1		# (number of digits - 1) in v0
	li	v1, 1			# 1st digit's divisor in v1
	beq	v0, zero, 2f
1:
	mul	v1, 10			# this will be inlined by assembler
	sub	v0, 1
	bne	v0, zero, 1b
2:
	/*----------------------------------------------------------------+
	| loop on printing quotient and shifting divisor to lower digit.  |
	+----------------------------------------------------------------*/
	move	v0, a0			# v0 is the remainder
_pd_loop:
	divu	zero, v0, v1		# REAL machine instruction!
	mflo	a0			# read quotient
	mfhi	v0			# read remainder

_pd_print:
	and	a0, 0x0f
	addu	a0, CHAR_0		# convert to ascii value
	jal	_putchar

	divu	v1,10 			# get next dividor 
	bne	v1, zero, _pd_loop	# done when divisor == 0.

_pd_done:
	move	v0, zero
	j	t0			# return to caller
END(putdec)


/*------------------------------------------------------------------------+
| Routine name: void putudec(unsigned value, int min_width)         	  |
| 		                                                          |
| Description : prints unsigned decimal number with optional width. Right |
|		justified.						  |
| 		                                                          |
| Note: Assume that _putchar() only clobbers a0-a3.			  |
| 		                                                          |
| Register Usage:                                                         |
|       - a0  decimal number/quotient.  - a1  min widths/scratch reg.     |
|       - v0  remainder.                - v1  devisor.                    |
|       - t0  saved return address.	                                  |
+------------------------------------------------------------------------*/
LEAF(putudec)
	move	t0, ra			# save return address
	move	v0, a0			# save a0/a1 before _putchar()
	move	v1, a1

	j	_pd_start		# share the common code w/ putdec()
END(putudec)
