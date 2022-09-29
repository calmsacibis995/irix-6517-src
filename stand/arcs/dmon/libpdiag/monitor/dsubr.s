#ident	"$Id: dsubr.s,v 1.1 1994/07/21 01:13:19 davidl Exp $"
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
#include "sys/fpregdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

#include "pdiag.h"
#include "monitor.h"


#define char_SP		0x20	/* ' ' */
#define char_DASH	0x2d	/* '-' */

/*
 * generic subroutines
 *
 */
	.data
msg_crlf:
	.asciiz	"\r\n"
msg_crlf2:
	.asciiz	"\r\n\n"
msg_pause:
	.asciiz "\r\n\nHit a key to continue... "

	.text

/*
 * void show_cpustate(void)
 *
 * display contents of cpu c0 registers stored in s0-s6, and also
 * some cpu bc registers.
 *
 * registers used: a0, t2
 */
LEAF(show_cpustate)
	move	t2, ra

	# PRINT("\n\nCPU C0 Registers:")
	PRINT("\n\n  STATUS      : 0x")
	.set	noreorder
	mfc0	a0, C0_SR
	nop
	.set	reorder
	jal	puthex

	PRINT("  EPC         : 0x")
	.set	noreorder
	mfc0	a0, C0_EPC
	nop
	.set	reorder
	jal	puthex

	PRINT("\n  CAUSE       : 0x")
	.set	noreorder
	mfc0	a0, C0_CAUSE
	nop
	.set	reorder
	jal	puthex

	PRINT("  BADVADDR    : 0x")
	.set	noreorder
	mfc0	a0, C0_BADVADDR
	nop
	.set	reorder
	jal	puthex

	/*
	 * PRINT FPA status registers if SR_CU1 set
	 */
	.set	noreorder
	mfc0	a0, C0_SR
	nop
	.set	reorder
	and	a0, SR_CU1
	beq	a0, zero, 1f		# SR_CU1 set?
	
	PRINT("\n\n  FPA/ErrorReg: 0x")
	.set	noreorder
	cfc1	t0, $30
	nop
	.set	reorder
	PUTHEX(t0)

	PRINT("  FPA/CSR     : 0x")
	.set	noreorder
	cfc1	t0, fcr31
	nop
	.set	reorder
	PUTHEX(t0)
1:
	j	t2
END(show_cpustate)


/* end */

