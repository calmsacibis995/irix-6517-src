#ident	"$Id: putchar_duart.s,v 1.1 1994/07/21 01:15:30 davidl Exp $"
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


#include "machine/regdef.h"
#include "machine/asm.h"
#include "machine/cpu.h"
#include "machine/cpu_board.h"

#include "duart.h"
#include "monitor.h"

	.extern	_intr_jmp	# Ctrl_C handler entry point

/*------------------------------------------------------------------------+
| XON/XOFF.                                                               |
+------------------------------------------------------------------------*/
#define	CtlChar(x)	((x) & 0x1f)	/* control character.            */

#define	CTL_Q		0x11
#define	CTL_S		0x13
#define CTL_C		0x03


/*------------------------------------------------------------------------+
| define constant                                                         |
+------------------------------------------------------------------------*/
#define	DELAYCNT	0x08000


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
	li	a1, DUART0_BASE

	#+----------------------------------------------------------------+
	#| check for control-s being enter from the console.              |
	#+----------------------------------------------------------------+
_chk_xon:
	lbu	a3, DOFF_SRA(a1)	# read status register.
	and	a3, SRRXRDY		# receive ready ?
	beq	a3, zero, _xmit_empty

	lbu	a3, DOFF_RHRA(a1)

	beq	a3, CTL_C, _hndl_ctrlc  # handle ctrl-c

	bne	a3, CTL_S, _xmit_empty	# ctl-s character entered ?

_chk_xoff:
	lbu	a3, DOFF_SRA(a1)	# wait for clt-q.
	and	a3, SRRXRDY		# status == receive ready ?
	beq	a3, zero, _chk_xoff

	lbu	a3, DOFF_RHRA(a1)

	beq	a3, CTL_C, _hndl_ctrlc  # handle ctrl-c

	bne	a3, CTL_Q, _chk_xoff

	#+----------------------------------------------------------------+
	#| wait for any char in buffer to be written out.                 |
	#+----------------------------------------------------------------+
_xmit_empty:
	li	a3, DELAYCNT
1:
	lbu	a2, DOFF_SRA(a1)	# read status register.
	and	a2, SREMT		# wait for transmitter empty.
	bne	a2, zero, _xmit_rdy

	subu	a3, a3, 1		# decrement the count
	bne	a3, zero, 1b		# spin till it become 0
2:
	j	ra			# timeout, just return

	#+----------------------------------------------------------------+
	#| check for transmit ready.                                      |
	#+----------------------------------------------------------------+
_xmit_rdy:
	li	a3, DELAYCNT
1:
	lbu	a2, DOFF_SRA(a1)	# read status register.  
	and	a2, SRTXRDY		# wait for transmit ready.
	bne	a2, zero, _send_char

	subu	a3, a3, 1		# decrement the count
	bne	a3, zero, 1b		# spin till it become 0
2:
	j	ra			# timeout, just return

	#+----------------------------------------------------------------+
	#| send the character.                                            |
	#+----------------------------------------------------------------+
_send_char:
	sb	a0, DOFF_THRA(a1)	# send the character


	#+----------------------------------------------------------------+
	#| if \n is found, pad in the \r...                               |
	#+----------------------------------------------------------------+
	bne	a0, 0x0a, _putc_done	# check for character '\n'
	or	a0, zero, 0x0d		# if so, go ahead and print \r.
	b	_xmit_empty

_putc_done:
	j	ra			# and return

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
	li	a1, DUART0_BASE

	#+----------------------------------------------------------------+
	#| wait for any char in buffer to be written out.                 |
	#+----------------------------------------------------------------+
_xmit_empty_b:
	li	a3, DELAYCNT
1:
	lbu	a2, DOFF_SRB(a1)	# read status register.
	and	a2, SREMT		# wait for transmitter empty.
	bne	a2, zero, _xmit_rdy_b

	subu	a3, a3, 1		# decrement the count
	bne	a3, zero, 1b		# spin till it become 0
2:
	j	ra			# timeout, just return


	#+----------------------------------------------------------------+
	#| check for transmit ready.                                      |
	#+----------------------------------------------------------------+
_xmit_rdy_b:
	li	a3, DELAYCNT
1:
	lbu	a2, DOFF_SRB(a1)	# read status register.  
	and	a2, SRTXRDY		# wait for transmit ready.
	bne	a2, zero, _send_char_b

	subu	a3, a3, 1		# decrement the count
	bne	a3, zero, 1b		# spin till it become 0
2:
	j	ra			# timeout, just return

	#+----------------------------------------------------------------+
	#| send the character.                                            |
	#+----------------------------------------------------------------+
_send_char_b:
	sb	a0, DOFF_THRB(a1)	# send the character

	j	ra			# and return
END(_putchar_b)

