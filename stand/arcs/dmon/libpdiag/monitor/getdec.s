#ident	"$Id: getdec.s,v 1.1 1994/07/21 01:14:06 davidl Exp $"
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
#define	CHAR_BEL	0x07

#define MAX_DIGIT	10

	.text
	.align	2

/*------------------------------------------------------------------------+
| int getdec( void )                                                      |
|									  |
| Description : This routine reads and converts ascii string into decimal |
|   number. It returns the converted value in "v0" and the status in "v1".|
|   A non-zero  status in v1 means error detected.                        |
|									  |
| Register Usage:                                                         |
|       - t0  accumulated value.        - t1  character count.            |
|       - t2  saved return address.                                       |
|       - a0-a3, v0, v1  ared all used by _getchar() and _putchar().      |
+------------------------------------------------------------------------*/
LEAF(getdec)
	move	t2, ra			# save return address
	move	t0, zero
	move	t1, zero

_gd_next:
	jal	_getchar		# read a character from console.

	#+----------------------------------------------------------------+
	#| parse the input character.                                     |
	#+----------------------------------------------------------------+
	and	v0, 0x0ff
	beq	v0, CHAR_BS, _gd_backup	# check for back space char.
	beq	v0, CHAR_CR, _gd_done	# check for string terminator.
	beq	v0, CHAR_NL, _gd_done

	blt	v0, CHAR_BS, _gd_bell	# unprintable char? 
	bge	t1, MAX_DIGIT, _gd_overflow # decimal digit overflow?

	blt	v0, CHAR_0, _gd_bs_bell	# invalid char?
	bgt	v0, CHAR_9, _gd_bs_bell

	#+----------------------------------------------------------------+
	#| got valid number, calculate next decimal number.               |
	#+----------------------------------------------------------------+
	sub	v0, CHAR_0		# convert ascii to decimal.
	mul	t0, 10			# this will inlined by assembler
	add	t0, v0

	addi	t1, 1			# increment digit count
	b	_gd_next		# and go read next digit

	#+----------------------------------------------------------------+
	#| invalid input. sound bell and back up on space.                |
	#+----------------------------------------------------------------+
_gd_overflow:
	li	a0, CHAR_BS
	jal	_putchar		# wipe out character on screen.
	li	a0, CHAR_SP
	jal	_putchar
_gd_bs_bell:
	li	a0, CHAR_BS
	jal	_putchar		# back up on invalid char
_gd_bell:
	li	a0, CHAR_BEL
	jal	_putchar		# invalid input. sound a bell.
	b	_gd_next

	#+----------------------------------------------------------------+
	#| back up one digit.                                             |
	#+----------------------------------------------------------------+
_gd_backup:
	beq	t1, zero, _gd_next	# if (count == 0) don't do

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

	divu	t0, 10			# back up to previous digit.
	subu	t1, 1			# decrement digit count.
	b	_gd_next

_gd_done:
	li	a0, CHAR_NL
	jal	_putchar

	li	v1, 1
	beq	t1, zero, 1f		# set up status word in "v1"
	move	v1, zero		# 0 == good / 1 == failed.
1:
	move	v0, t0
	j	t2			# return to caller.
END(getdec)
