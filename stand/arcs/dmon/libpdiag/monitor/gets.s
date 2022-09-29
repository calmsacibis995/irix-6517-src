#ident	"$Id: gets.s,v 1.1 1994/07/21 01:14:11 davidl Exp $"
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

#define	CR_CHAR		0x0d
#define NL_CHAR		0x0a
#define	BS_CHAR		0x08
#define	SP_CHAR		0x20

	.text
	.align	2

/*------------------------------------------------------------------------+
| int _gets(char *buf)					  		  |
|									  |
| Description : Read a string from console and place it in read buffer.   |
|		Returns number of characters read.			  |
|									  |
| Register Usage:                                                         |
|       - t0  read buffer pointer.      - t1  character counter.          |
|       - t2  saved return address.                                       |
|       - a0-a3, v0, v1  ared all used by _getchar() and _putchar().      |
+------------------------------------------------------------------------*/
LEAF(_gets)
	move	t2, ra			# save return address
	move	t0, a0
	move	t1, zero
1:
	jal	_getchar		# read a character from console.
	and	v0, 0x7f

	bne	v0, BS_CHAR, 2f		# check for back space char.

	beq	t1, zero, 1b		# if (count == 0) don't do <BS>

	li	a0, BS_CHAR
	jal	_putchar		# wipe out character on screen.
	li	a0, SP_CHAR
	jal	_putchar
	li	a0, BS_CHAR
	jal	_putchar

	subu	t1, 1
	subu	t0, 1
	b	1b
2:
	addi	t1, 1			# increment char count.
	sb	v0, (t0)		# write character to buffer
	addiu	t0, 1			# advance buffer offset.

	beq	v0, CR_CHAR, 3f		# check for string terminator.
	bne	v0, NL_CHAR, 1b
3:
	sb	zero, -1(t0)		# put NULL character at end of string.
4:
	sub	v0, t1, 1
	j	t2			# return to caller.
END(_gets)
