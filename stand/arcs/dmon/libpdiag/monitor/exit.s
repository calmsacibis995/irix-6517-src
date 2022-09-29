#ident	"$Id: exit.s,v 1.1 1994/07/21 01:13:38 davidl Exp $"
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
#include "monitor.h"

	.text
/*
 * void exit(int exitno)
 * 
 * Terminate existence gracefully
 * Much scaled down version of SA's exit()
 *
 * NOTE: no close(fd) here.
 */
LEAF(exit)
	move	s0, a0			# save exitno
	la	a0, msg_exit
	jal	_puts
	move	a0, s0
	li	a1, 1			# min 1 digit
	jal	putdec
	la	a0, msg_called
	jal	_puts

	move	a0, s0
	j	_exit
END(exit)

/*------------------------------------------------------------------------+
| Routine name: _exit(int exitno)                                         |
|									  |
| Description : Jump to the beginning of boot prom.			  |
+------------------------------------------------------------------------*/
LEAF(_exit)
	.set	noreorder
	li	t0, SR_BEV
	mtc0	t0, C0_SR		# clear and set BEV
	mtc0	zero, C0_CAUSE		# clear software interrupts
	.set	reorder

	la	ra, PROM_RESET		# branch to reset vector
	j	ra
END(_exit)

	.data

msg_exit:	.asciiz "\n\r\n\rexit("
msg_called:	.asciiz ") called\n\r"
