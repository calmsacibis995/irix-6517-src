#ident	"$Id: regs_init.s,v 1.1 1994/07/21 01:15:58 davidl Exp $"
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

	.text

/*------------------------------------------------------------------------+
| void regs_init( void )                                                  |
|									  |
| Description : clear all general purpose registers.                      |
+------------------------------------------------------------------------*/
LEAF(regs_init)
	.set	noat
	move	AT, zero
	.set	at
	move	v0, zero
	move	v1, zero
	move	a0, zero
	move	a1, zero
	move	a2, zero
	move	a3, zero
	move	t0, zero
	move	t1, zero
	move	t2, zero
	move	t3, zero
	move	t4, zero
	move	t5, zero
	move	t6, zero
	move	t7, zero
	move	t8, zero
	move	t9, zero
	move	s0, zero
	move	s1, zero
	move	s2, zero
	move	s3, zero
	move	s4, zero
	move	s5, zero
	move	s6, zero
	move	s7, zero
	move	k0, zero
	move	k1, zero
	move	gp, zero
	move	sp, zero
#ifndef DIAG_MONITOR
	move	fp, zero	# can't be cleared in diags monitor.
#endif !DIAG_MONITOR
	j	ra
END(regs_init)
