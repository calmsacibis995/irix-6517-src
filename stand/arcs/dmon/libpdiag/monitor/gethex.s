#ident	"$Id: gethex.s,v 1.1 1994/07/21 01:14:09 davidl Exp $"
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
	.align	2

/*------------------------------------------------------------------------+
| usigned gethex( void )                                                  |
|									  |
| Description : This routine  reads and converts ascii string  into hexa- |
|   decimal number. It returns the converted value in "v0" and the status |
|   in "v1".  A non-zero  status in v1 means error detected.              |
|									  |
| Register Usage:                                                         |
|       - t0  accumulated value.        - t1  character count.            |
|       - t2  saved return address.                                       |
|       - a0-a3, v0, v1  ared all used by _getchar() and _putchar().      |
+------------------------------------------------------------------------*/
LEAF(gethex)
	move	t2, ra			# save return address
	move	t0, zero
	move	t1, zero

_gh_next:
	jal	_getchar		# read a character from console.

	#+----------------------------------------------------------------+
	#| parse the input character.                                     |
	#+----------------------------------------------------------------+
	and	v0, 0x7f
	beq	v0, CHAR_BS, _gh_backup	# check for back space char.
	beq	v0, CHAR_CR, _gh_done	# check for string terminator.
	beq	v0, CHAR_NL, _gh_done

	blt	v0, CHAR_BS, _gh_bell	# unprintable char? 

	bge	t1, MAX_DIGIT, _gh_overflow # hexadecimal digit overflow?

	blt	v0, CHAR_0, _gh_bs_bell	# check for number range from 0-9
	sub	a1, v0, CHAR_0
	ble	v0, CHAR_9, _gh_convert

	and	a0, v0, 0x5f		# check for number range from A-F.
	blt	a0, CHAR_A, _gh_bs_bell
	sub	a1, a0, CHAR_A-0x0A
	bgt	a0, CHAR_F, _gh_bs_bell

	#+----------------------------------------------------------------+
        #| got valid number, calculate next hexadecimal number.           |
        #+----------------------------------------------------------------+
_gh_convert:
	sll	t0, 4
	add	t0, a1
	addi	t1, 1			# next digit.
	b	_gh_next		# increment digit count and go read

	#+----------------------------------------------------------------+
	#| invalid input. sound bell and back up on space.                |
	#+----------------------------------------------------------------+
_gh_overflow:
	li	a0, CHAR_BS
	jal	_putchar		# wipe out character on screen.
	li	a0, CHAR_SP
	jal	_putchar
_gh_bs_bell:
	li	a0, CHAR_BS
	jal	_putchar		# back up on invalid char
_gh_bell:
	li	a0, CHAR_BEL
	jal	_putchar		# sound a bell.
	b	_gh_next

	#+----------------------------------------------------------------+
	#| back up one digit.                                             |
	#+----------------------------------------------------------------+
_gh_backup:
	beq	t1, zero, _gh_next	# if (count == 0) don't do

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
	b	_gh_next

_gh_done:
	li	a0, CHAR_NL
	jal	_putchar

	li	v1, 1
	beq	t1, zero, 1f		# set up status word in "v1"
	move	v1, zero		# 0 == good / 1 == failed.
1:
	move	v0, t0
	j	t2			# return to caller.
END(gethex)

