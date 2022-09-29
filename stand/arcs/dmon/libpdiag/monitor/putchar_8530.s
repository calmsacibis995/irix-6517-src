#ident	"$Id: putchar_8530.s,v 1.1 1994/07/21 01:15:26 davidl Exp $"
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


#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

#include "scc_cons.h"
#include "scc.h"

	.extern	_intr_jmp	# CTRL_C handler

/*------------------------------------------------------------------------+
| XON/XOFF.                                                               |
+------------------------------------------------------------------------*/
#define	CtlChar(x)	((x) & 0x1f)	/* control character.            */

#define	CHAR_Q		0x51
#define	CHAR_S		0x53

#define	CTL_Q		0x11
#define	CTL_S		0x13
#define CTL_C		0x03


/*------------------------------------------------------------------------+
| define constant                                                         |
+------------------------------------------------------------------------*/
#ifdef SABLE
#define	DELAYCNT	0x1
#else
#define	DELAYCNT	0x08000
#endif


/*------------------------------------------------------------------------+
| goto code section, make word aligned, and set on reorder flag.          |
+------------------------------------------------------------------------*/

	.text

#ifdef DIAG_MONITOR

/*------------------------------------------------------------------------+
| Routine name: void putchar(char)                                        |
| Description : jmp to _putchar() routine.                                |
+------------------------------------------------------------------------*/
LEAF(putchar)
	j	_putchar
END(putchar)


/*------------------------------------------------------------------------+
| Routine name: void putchar_b(char)                                      |
| Description : jmp to _putchar_b() routine.                              |
+------------------------------------------------------------------------*/
LEAF(putchar_b)
	j	_putchar_b
END(putchar_b)


/*------------------------------------------------------------------------+
| Routine name: void putc(char)                                           |
| Description : jmp to _putchar() routine.                                |
+------------------------------------------------------------------------*/
LEAF(putc)
	j	_putchar
END(putc)


/*------------------------------------------------------------------------+
| Routine name: void putc_b(char)                                         |
| Description : jmp to _putchar_b() routine.                              |
+------------------------------------------------------------------------*/
LEAF(putc_b)
	j	_putchar_b
END(putc_b)

#endif DIAG_MONITOR


/*------------------------------------------------------------------------+
| Routine name: void _putchar(char)                                       |
| Description : Prints a character on console port.                       |
| Register Usage:                                                         |
|       - a0	character to print.                                       |
|       - a1	UART's base address.                                      |
|       - a2	UART's read value.                                        |
|       - a3	loop counter.                                             |
|    Note: These register are not save before they are being use.         |
+------------------------------------------------------------------------*/
LEAF(_putchar)

	li	a1,SCC_BASE	# Duart base address

	/*
	 * Before printing check for control C, Q, or S
	 */
	lw	a2, SCC_PTRA(a1)	# get channel A status
	and	a2, SCCR_STATUS_RxDataAvailable
	beq	a2, zero, _xmit_empty	# No data in input buffer

	lw	a2, SCC_DATAA(a1)	# Get character from port A
	and	a2, 0x7F
	beq	a2, CTL_C, _hndl_ctrlc	# handle control C here
	bne	a2, CTL_S, _xmit_empty  # check for CTRL S

_chk_xoff:
	lw	a2, SCC_PTRA (a1)	# wait for ctrl q
	and	a2, SCCR_STATUS_RxDataAvailable
	beq	a2, zero, _chk_xoff	# loop until a character is typed

	lw	a2, SCC_DATAA(a1)	# Read input buffer
	and	a2, 0x7F
	beq	a2, CTL_C, _hndl_ctrlc	# Is it a ctrl-C ?
	bne	a2, CTL_Q, _chk_xoff	# Is it a ctrl-Q ?

_xmit_empty:

	li	a3,DELAYCNT
1:
	lw	a2,SCC_PTRA(a1)		# get channel A status
	and	a2,SCCR_STATUS_TxReady	# check if TBE is set
	bne	a2,zero,4f		# transmitter ready

	sub	a3,1			# decrement count
	beq	a3,zero,4f		# spin until 0,
					#  then just write the character
	b	1b
4:
	sw	a0,SCC_DATAA(a1)	# put char to SCC
	NOP_DELAY

	bne	a0, 0x0a, _putc_done	# check for character '\n'
	or	a0, zero, 0x0d		# if so, go ahead and print \r
	b	_xmit_empty		#

_putc_done:
	j	ra

_hndl_ctrlc:
	la	a0, _intr_jmp		# hard wire ctrl-c to this routine
	j	a0

END(_putchar)

/*------------------------------------------------------------------------+
| Routine name: void _putchar_b(char)                                     |
| Description : Prints a character on console port B. No XON/XOFF and     |
|		no padding '\r' after '\n'.                               |
| Register Usage:                                                         |
|       - a0	character to print.                                       |
|       - a1	UART's base address.                                      |
|       - a2	UART's read value.                                        |
|       - a3	loop counter.                                             |
|    Note: These register are not saved before they are being used.       |
+------------------------------------------------------------------------*/
LEAF(_putchar_b)

	li	a1,SCC_BASE|K1BASE	# Duart base address

	li	a3,DELAYCNT
1:
	lw	a2,SCC_PTRB(a1)		# get channel A status
	and	a2,SCCR_STATUS_TxReady	# check if TBE is set
	bne	a2,zero,4f		# transmitter ready

	sub	a3,1			# decrement count
	beq	a3,zero,4f		# spin until 0,
					#  then just write the character
	b	1b
4:
	sw	a0,SCC_DATAB(a1)	# put char to SCC
	NOP_DELAY

	j	ra

END(_putchar_b)
