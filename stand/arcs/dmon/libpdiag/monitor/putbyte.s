#ident	"$Id: putbyte.s,v 1.1 1994/07/21 01:15:24 davidl Exp $"
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
| void putbyte(char  byte_value)                                          |
|									  |
| Description : This routine prints  the least significant byte of a hex  |
|    number passed in "a0".                                               |
|									  |
| Register Usage:                                                         |
|       - v1    save return address                                       |
|       - v0    hex number to print                                       |
+------------------------------------------------------------------------*/
LEAF(putbyte)
	.set	noreorder
	move	v1, ra
	move	v0, a0

	srl	a0, v0, 4		# print the most-significant nibble.
	and	a0, 0x0f
	lb	a0, nibdigit(a0)
	jal	_putchar
	nop

	and	a0, v0, 0x0f		# print the least-significant nibble.
	lb	a0, nibdigit(a0)
	jal	_putchar
	nop

	j	v1			# Yes - done
	nop
	.set	reorder
END(putbyte)

/*------------------------------------------------------------------------+
| Hexa-descimal digit string.                                             |
+------------------------------------------------------------------------*/
	.data
	.align	1
nibdigit:
	.ascii  "0123456789ABCDEF"
