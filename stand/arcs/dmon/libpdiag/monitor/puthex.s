#ident	"$Id: puthex.s,v 1.1 1994/07/21 01:15:53 davidl Exp $"
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
| void puthex(unsigned hex_value)                                         |
|									  |
| Description : This routine prints eight hex digit.                      |
|									  |
| Register Usage:                                                         |
|       - v1    save return address                                       |
|       - v0    hex number to print                                       |
|       - t0    digit count                                               |
+------------------------------------------------------------------------*/
LEAF(puthex)
	.set	noreorder
	move	v1, ra
	move	v0, a0
	li	t0, 8			# Number of digits to display
2:
	srl	a0, v0, 28		# Isolate digit
	lb	a0, hexdigit(a0)	# Convert it to hexidecimal
	jal	_putchar		# Print it

	sub	t0, 1			# Entire number printed?
	bne	t0, zero, 2b
	sll	v0, 4			# Set up next nibble

	j	v1			# Yes - done
	nop
	.set	reorder
END(puthex)

/*------------------------------------------------------------------------+
| Hexa-descimal digit string.                                             |
+------------------------------------------------------------------------*/
	.data
	.align	1
hexdigit:
	.ascii  "0123456789ABCDEF"
