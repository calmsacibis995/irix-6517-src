#ident	"$Id: puts.s,v 1.1 1994/07/21 01:15:56 davidl Exp $"
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

	.text

#ifdef DIAG_MONITOR

/*------------------------------------------------------------------------+
| Routine name: puts(char *ptr)                                           |
| Description : Jump to _puts()						  | 
+------------------------------------------------------------------------*/
LEAF(puts)
	j	_puts
END(puts)

#endif DIAG_MONITOR


/*------------------------------------------------------------------------+
| Routine name: _puts(char *ptr)                                          |
|									  |
| Description : Prints a string of ascii to console.                      |
|									  |
| Register Usage:                                                         |
|       - v0	pointer to the string to print.                           |
|       - v1	save return address.                                      |
|									  |
| Note: These register are not saved before they are being used.          |
+------------------------------------------------------------------------*/
LEAF(_puts)
	move	v1, ra			# save return address
	move	v0, a0
	beq	a0, zero, 2f		# check for null ptr
1:
	lbu	a0, (v0)		# load byte to be printed.
	beq	a0, zero, 2f		# done ?
	jal	_putchar		# dump it out to console
	addiu	v0, 1			# next byte.
        b	1b			# loop back.
2:
	j	v1			# return to caller.
END(_puts)
