#ident	"$Id: getchar_duart.s,v 1.1 1994/07/21 01:14:00 davidl Exp $"
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
#include "machine/cpu.h"
#include "machine/asm.h"
#include "machine/cpu_board.h"

#include "duart.h"
#include "monitor.h"

#define	C_LOW	0x20	/* ' ' space */
#define	C_HI	0x7e	/* '~' */

	.text

#ifdef DIAG_MONITOR

/*------------------------------------------------------------------------+
| Routine name: char getchar(void)                                        |
| Description : jump to _getchar() routine.                               |
+------------------------------------------------------------------------*/
LEAF(getchar)
	j	_getchar
END(getchar)

/*------------------------------------------------------------------------+
| Routine name: char getchar(void)                                        |
| Description : jump to _getchar_b() routine.                             |
+------------------------------------------------------------------------*/
LEAF(getchar_b)
	j	_getchar_b
END(getchar_b)

/*------------------------------------------------------------------------+
| Routine name: char getc(void)                                           |
| Description : jump to _getchar() routine.                               |
+------------------------------------------------------------------------*/
LEAF(getc)
	j	_getchar
END(getc)

/*------------------------------------------------------------------------+
| Routine name: char getc_b(void)                                         |
| Description : jump to _getchar_b() routine.                             |
+------------------------------------------------------------------------*/
LEAF(getc_b)
	j	_getchar_b
END(getc_b)

#endif DIAG_MONITOR


/*------------------------------------------------------------------------+
| Routine name: char _getchar(void)                                       |
| Description : read a character from console port.  This routine polling |
|   on receive-ready signal before reading and returning the character to |
|   the calling routine. It also echo the character if it is printable.   |
| Register Usage:                                                         |
|       - a0  scrtach register         - a1  DUART' base address          |
|       - v0  return character.        - v1  saved return address.        |
+------------------------------------------------------------------------*/
LEAF(_getchar)
	move	v1, ra			# save return address

_base_addr:
	li	a1, DUART0_BASE

	#+----------------------------------------------------------------+
	#| check ready status, read char and print-it.                    |
	#+----------------------------------------------------------------+
_read_status:
	lbu	a0, DOFF_SRA(a1)	# read status register.
	and	a0, SRRXRDY		# receive ready ?
	beq	a0, zero, _read_status

_read_char:
	lbu	v0, DOFF_RHRA(a1)	# read the character.

_echo_char:
	blt	v0, C_LOW, _done	# echo printable character.
	bgt	v0, C_HI, _done
	move	a0, v0
	jal	_putchar
_done:
	j	v1
END(_getchar)



/*------------------------------------------------------------------------+
| Routine name: char _getchar_b(void)                                     |
| Description : read a character from console port B. This routine polling|
|   on receive-ready signal before reading and returning the character to |
|   the calling routine. It DOES NOT echo the character.   		  |
| Register Usage:                                                         |
|       - a0  scrtach register         - a1  DUART' base address          |
|       - v0  return character.        - v1  saved return address.        |
+------------------------------------------------------------------------*/
LEAF(_getchar_b)
	# move	v1, ra			# save return address

_base_addr_b:
	li	a1, DUART0_BASE

	#+----------------------------------------------------------------+
	#| check ready status, read char and print-it.                    |
	#+----------------------------------------------------------------+
_read_status_b:
	lbu	a0, DOFF_SRB(a1)	# read status register.
	and	a0, SRRXRDY		# receive ready ?
	beq	a0, zero, _read_status_b

_read_char_b:
	lbu	v0, DOFF_RHRB(a1)	# read the character.
/*
 * Do not echo the character.
 *
# 	and	v0, 0x0ff
#
# _echo_char_b:
# 	blt	v0, C_LOW, _done_b	# echo printable character.
# 	nop
# 	bge	v0, C_HI, _done_b
# 	move	a0, v0
# 	jal	_putchar
# 	nop
# 
# _done_b:
#	j	v1
*/
	j	ra
END(_getchar_b)


/*------------------------------------------------------------------------+
| Routine name: char may_getchar(void)                                    |
| Description : read a character from console port (no-wait).             |
| Register Usage:                                                         |
|       - a0  scrtach register         - a1  DUART' base address          |
+------------------------------------------------------------------------*/
LEAF(may_getchar)
	li	a1, DUART0_BASE
	lbu	a0, DOFF_SRA(a1)	# read status register.
	and	a0, SRRXRDY		# receive ready ?
	move	v0, zero		# return 0 if no char
	beq	a0, zero, 1f
	lbu	v0, DOFF_RHRA(a1)	# return read character.
1:
	j	ra
END(may_getchar)

