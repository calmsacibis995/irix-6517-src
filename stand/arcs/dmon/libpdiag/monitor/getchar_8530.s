/*
 * $Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/libpdiag/monitor/RCS/getchar_8530.s,v 1.1 1994/07/21 01:13:43 davidl Exp $
 *
 */
/*------------------------------------------------------------------------+
| Copyright Unpublished, MIPS Computer Systems, Inc. All Rights Reserved. |
| This software contains proprietary and confidential information of MIPS |
| and  its  suppliers.  Use,  disclosure  or reproduction  is  prohibited |
| without the prior express written consent of MIPS.                      |
+------------------------------------------------------------------------*/
#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

#include "scc.h"
#include "scc_cons.h"

#define	C_LOW		0x20	/* ' ' space */
#define	C_HI		0x7e	/* '~' */

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
	li	a1,SCC_BASE|K1BASE	# get channel A address

_read_status:
	lw	a0, SCC_PTRA(a1)	# get channel A status
	and	a0,a0,SCCR_STATUS_RxDataAvailable
					# see if DataAvailable set
	beq	a0,zero, _read_status	# poll again if not set

	lw	v0, SCC_DATAA(a1)	# get char and load to v0
	and	v0, 0x7f
	blt	v0, C_LOW, _done
	bgt	v0, C_HI, _done
	move	a0,v0			# store char to a0
	jal	_putchar		# echo char

_done:
	j	v1			# return to the calling addr
		
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

	li	a1,SCC_BASE|K1BASE	# get channel A address

_read_status_b:
	lw	a0, SCC_PTRB(a1)	# get channel A status
	and	a0,a0,SCCR_STATUS_RxDataAvailable
					# see if DataAvailable set
	beq	a0,zero, _read_status	# poll again if not set

	lw	v0, SCC_DATAB(a1)	# get char and load to v0
/*
	and	v0,0x7f			# only want useful bits
	move	a0,v0			# store char to a0
	jal	_putchar		# echo char
*/
	
	j	ra			# return to the calling addr
		
END(_getchar_b)


/*------------------------------------------------------------------------+
| Routine name: char may_getchar(void)                                    |
| Description : read a character from console port (no-wait).             |
| Register Usage:                                                         |
|       - a0  scrtach register         - a1  DUART' base address          |
+------------------------------------------------------------------------*/
LEAF(may_getchar)

	li	a1,SCC_BASE|K1BASE	# get channel A address

	lw	a0, SCC_PTRA(a1)	# get channel A status
	and	a0,a0,SCCR_STATUS_RxDataAvailable
					# see if DataAvailable set
	move	v0, zero		# return a zero if not data available
	beq	a0,zero, 1f	 	# poll again if not set

	lw	v0, SCC_DATAA(a1)	# get char and load to v0
	and	v0,0x7f			# only want useful bits
1:
	j	ra			# return to the calling addr
		
END(may_getchar)

