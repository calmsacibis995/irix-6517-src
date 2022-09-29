#ident	"$Id: putbcd.s,v 1.1 1994/07/21 01:15:21 davidl Exp $"
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

	.text

/*------------------------------------------------------------------------+
| void putbcd( unsigned bcd_value, int num_digits )                       |
|									  |
| Description : This routine prints Binary Coded Decimal number.          |
|		Num_digits defaults to 8.				  |
|									  |
| Register Usage:                                                         |
|       - v0  saved number to print     - v1  save return address         |
|       - t0  digit count                                                 |
+------------------------------------------------------------------------*/
LEAF(putbcd)
	.set	noreorder
	move	v1, ra			# saved return address.
	move	v0, a0			# save bcd in v0.

	bleu	a1, 8, 1f		# check arg #2 for digit count.
	nop
	move	a1, zero
1:
	li	t0, 8			# count non-zero digit.
2:
	rol	a0, 4			# rotate MSB to LSB
	and	a3, a0, 0x0f		# check for non-zero digit.
	bne	a3, zero, 3f		# if found terminate loop,
	sub	t0, 1			# else decrement digit count.
	bne	t0, zero, 2b		# countinue until count == 0.
	nop
3:
	addi	t0, 1
	bgt	t0, a1, 4f
	nop
	move	t0, a1
4:
	li	a3, 8			# align number. Shift first digit
	sub	a3, t0			# to be printed to MSB of word.
	sll	a3, 2
	sll	v0, a3

	#+----------------------------------------------------------------+
	#| bcd print loop.                                                |
	#+----------------------------------------------------------------+
5:
	rol	v0, 4			# move digit to be printed to LSB
	and	a0, v0, 0xf
	lb	a0, bcddigit(a0)	# get BCD digit.
	jal	_putchar
	nop
	sub	t0, 1
	bne	t0, zero, 5b
	nop
6:
	j	v1			# Yes - done
	nop
	.set	reorder
END(putbcd)



/*------------------------------------------------------------------------+
| Hexa-descimal digit string.                                             |
+------------------------------------------------------------------------*/
	.data
	.align	1
bcddigit:
	.ascii  "0123456789ABCDEF"
