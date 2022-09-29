#ident	"$Id: duart.s,v 1.1 1994/07/21 01:13:35 davidl Exp $"
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
#include "monitor.h"			/* defines DUART0_BASE */

#define	THE_END		0xdead

	.text
	.align	2

/*------------------------------------------------------------------------+
| Routine name: duart_init( void )                                        |
|                                                                         |
| Description : use table to initialize duart channels A/B for polled IO. |
|                                                                         |
| Register Usage:                                                         |
|       - a0  init value.               - a1  pointer to duart register.  |
|       - v0  DUART base address.       - v1  pointer to duart init table.|
+------------------------------------------------------------------------*/
LEAF(duart_init)
	li	v0, DUART0_BASE		# generate duart base address.
	la	v1, init_table		# point to duart init table

init_loop:
	lhu	a0, (v1)		# get reg_offset/reg_value
	addu	v1, 2			# bump to next entry
	beq	a0, THE_END,1f		# done?

	srl	a1, a0, 8		# get register offset.
	addu	a1, v0			# a1 = base addr + reg offset.
	sb	a0, (a1)		# write to duart register
	j	init_loop
1:
	j	ra			# return


	/*----------------------------------------------------------------+
	| Table for loading DUART control registers:                      |
	|                                                                 |
	|    In each entry of the table, the first byte is the offset of  |
	| the register  from  DUART0_BASE  and  the second byte (low      |
	| order byte) is the data to be loaded into the register.         |
	+----------------------------------------------------------------*/
	     .align 2
init_table:

	#+----------------------------------------------------------------+
	#|  Channel A Setup                                               |
	#+----------------------------------------------------------------+
	.byte	DOFF_CRA,  RES_RX	# cra - reset receiver
	.byte	DOFF_CRA,  RES_TX	# cra - reset transmitter
	.byte	DOFF_CRA,  RES_ERR	# cra - reset error status
	.byte	DOFF_CRA,  RES_BRK		# cra - reset break interrupt
	.byte	DOFF_ACR,  0x80		# use baud rate set2
	.byte	DOFF_CRA,  SEL_MR1		# cra - point to mr1
	.byte	DOFF_MRA,  0x13		# cr1a - no parity, 8 bits per char
	.byte	DOFF_MRA,  0x07		# mr2a - 1 stop bit
	.byte	DOFF_CRA,  SEL_MR1	# cra - point to mode register 1
	.byte	DOFF_CSRA, b9600	# csra - RxC = 9600, TxC = 9600


	#+----------------------------------------------------------------+
	#|  Channel B Setup                                               |
	#+----------------------------------------------------------------+
	.byte	DOFF_CRB,  RES_RX	# crb - reset receiver
	.byte	DOFF_CRB,  RES_TX	# crb - reset transmitter
	.byte	DOFF_CRB,  RES_ERR	# crb - reset error status
	.byte	DOFF_CRB,  RES_BRK	# crb - reset break interrupt
	.byte	DOFF_CRB,  SEL_MR1	# crb - point to mr1
	.byte	DOFF_MRB,  0x13		# mr1b - no parity, 8 bits per char
	.byte	DOFF_MRB,  0x07		# mr2b - 1 stop bit
	.byte	DOFF_CRB,  SEL_MR1	# crb - point to mode register 1
	.byte	DOFF_CSRB, b9600	# csrb - RxC = 9600, TxC = 9600


	#+----------------------------------------------------------------+
	#|  Interrupt setup                                               |
	#+----------------------------------------------------------------+
	.byte	DOFF_IMR, 0x00		# imr - Disable all interrupts


	#+----------------------------------------------------------------+
	#|  Output Port - Turn all bits on                                |
	#+----------------------------------------------------------------+
	.byte	DOFF_SOPBC, 0x03	# sopbc - Set output port bits command


	#+----------------------------------------------------------------+
	#|  Turn on both channels                                         |
	#+----------------------------------------------------------------+
	.byte	DOFF_OPCR, 0x00		# opcr - Complement of opr register
	.byte	DOFF_CRA,  EN_TX|EN_RX	# cra - Enable xmitter and receiver
	.byte	DOFF_CRB,  EN_TX|EN_RX	# crb - Enable xmitter and receiver

	.half	THE_END

	.align	2
END(duart_init)
