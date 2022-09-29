#ident	"$Id: gets_nv.s,v 1.1 1994/07/21 01:14:13 davidl Exp $"
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
	.align 2

/*------------------------------------------------------------------------+
| int gets_nv( char *nv_ptr )                                             |
|                                                                         |
| Description : read ad string from console and place it in read buffer   |
|     located in NVRAM. Returns number of chars read.                     |
|                                                                         |
| Note: nv_ptr should be a kseg1 address pointer to the NVRAM.            |
|                                                                         |
| Register Usage:                                                         |
|       - t0  pointer to nv_ram.        - t1  character counter.          |
|       - t2  saved return address.                                       |
|       - a0-a3, v0, v1  ared all used by _getchar() and _putchar().      |
+------------------------------------------------------------------------*/
LEAF(gets_nv)
	move	t2, ra			# save return address
	move	t0, a0
	move	t1, zero
1:
	jal	_getchar		# read a character from console.
	and	v0, 0x7f

	bne	v0, BS_CHAR, 2f		# check for back space char.

	beq	t1, zero, 1b		# if (count == 0) don't do

	li	a0, BS_CHAR
	jal	_putchar		# wipe out character on screen.
	li	a0, SP_CHAR
	jal	_putchar
	li	a0, BS_CHAR
	jal	_putchar

	subu	t1, 1
	subu	t0, 4
	b	1b
2:
	addi	t1, 1			# increment char count.
	sb	v0, (t0)		# write character to buffer in NV_RAM
	addiu	t0, 4			# advance buffer offset.

	beq	v0, CR_CHAR, 3f		# check for string terminator.
	bne	v0, NL_CHAR, 1b
3:
	sb	zero, -4(t0)		# put NULL character at end of string.
4:
	sub	v0, t1, 1
	j	t2			# return to caller.
END(gets_nv)
