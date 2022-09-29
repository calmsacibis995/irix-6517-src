#ident	"$Id: getbcd.s,v 1.1 1994/07/21 01:13:40 davidl Exp $"
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


#include "asm.h"
#include "regdef.h"

#define	CHAR_CR		0x0d
#define CHAR_NL		0x0a
#define	CHAR_BS		0x08
#define	CHAR_SP		0x20

#define	CHAR_0		0x30
#define	CHAR_9		0x39
#define	CHAR_A		0x41
#define	CHAR_F		0x46
#define	CHAR_BEL	0x07

#define MAX_DIGIT	8

	.text

/*------------------------------------------------------------------------+
| unsigned getbcd( void )                                                 |
|									  |
| Description : read and convert ascii string into Binary-Coded-Decimal   |
|   It returns the bcd value in "v0" and the status in "v1".  A non-zero  |
|   status in v1 means error detected.              			  |
|									  |
| Register Usage:                                                         |
|       - t0  accumulated value.        - t1  character count.            |
|       - t2  saved return address.                                       |
|       - a0-a3, v0, v1  ared all used by _getchar() and _putchar().      |
+------------------------------------------------------------------------*/
LEAF(getbcd)
	move	t2, ra			# save return address
	move	t0, zero
	move	t1, zero

_gbcd_next:
	jal	_getchar		# read a character from console.

	#+----------------------------------------------------------------+
	#| parse the input character.                                     |
	#+----------------------------------------------------------------+
	and	v0, 0x7f
	beq	v0, CHAR_BS, _gbcd_backup
	beq	v0, CHAR_CR, _gbcd_done	# check for string terminator.
	beq	v0, CHAR_NL, _gbcd_done

	blt	v0, CHAR_BS, _gbcd_bell	# unprintable char? 

	bge	t1, MAX_DIGIT, _gbcd_overflow # digit overflow?

	blt	v0, CHAR_0, _gbcd_bs_bell # check for number range from 0-9
	bgt	v0, CHAR_9, _gbcd_bs_bell

	#+----------------------------------------------------------------+
	#| got valid number, calculate next Binary-Coded-Ddecimal number. |
	#+----------------------------------------------------------------+
_gbcd_convert:
	sub	a0, v0, CHAR_0
	sll	t0, 4
	or	t0, a0
	addi	t1, 1			# increment digit count
	b	_gbcd_next		

	#+----------------------------------------------------------------+
	#| invalid input. sound bell and back up on space.                |
	#+----------------------------------------------------------------+
_gbcd_overflow:
	li	a0, CHAR_BS
	jal	_putchar		# wipe out character on screen.
	li	a0, CHAR_SP
	jal	_putchar
_gbcd_bs_bell:
	li	a0, CHAR_BS
	jal	_putchar		# back up on invalid char
_gbcd_bell:
	li	a0, CHAR_BEL
	jal	_putchar		# invalid input. sound a bell.
	b	_gbcd_next

	#+----------------------------------------------------------------+
	#| back up one digit.                                             |
	#+----------------------------------------------------------------+
_gbcd_backup:
	beq	t1, zero, _gbcd_next	# if (count == 0) don't do

	li	a0, CHAR_SP		# clear current position first
	jal	_putchar
	li	a0, CHAR_BS
	jal	_putchar
	li	a0, CHAR_BS
	jal	_putchar		# wipe out character on screen.
	li	a0, CHAR_SP
	jal	_putchar
	li	a0, CHAR_BS
	jal	_putchar

	srl	t0, 4			# back up to previous digit.
	subu	t1, 1			# decrement digit count.
	b	_gbcd_next

_gbcd_done:
	li	a0, CHAR_NL
	jal	_putchar

	li	v1, 1
	beq	t1, zero, 1f		# set up status word in "v1"
	move	v1, zero		# 0 == good / 1 == failed.
1:
	move	v0, t0
	j	t2			# return to caller.
END(getbcd)
