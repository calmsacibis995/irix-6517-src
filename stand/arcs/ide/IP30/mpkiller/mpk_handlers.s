/****************************************************************************
 *
 * Title:	mpk_handlers.s
 *
 ****************************************************************************
 *
 * Description:	Mpkiller exception handler code for standalone mode.
 *
 * Special notes:
 *	For T5/newt and Godzilla/VHDL cosim, r25/t9 is used as a handler
 *	vector in addition to the original vector table. Set and keep r25
 *	to zero, if only the vector table is desired.
 *
 *	If an excpetion handler is invoked using r25, k0 is the exception
 *	vector offset upon the entry of handler.
 *
 *  For the IDE version of mpkiller, r25 is used to save sp.
 *
 * Compile options:
 *
 * Copyright (C) 1996 by Silicon Graphics, Inc. All rights reserved.
 *
 ***************************************************************************/

/*
 * $Id: mpk_handlers.s,v 1.1 1996/09/30 17:58:57 lian Exp $
 */
#include <asm.h>
#include "t5.h"

	.extern	Exit_MPK

	.text
	.set	noat
	.set	noreorder

LEAF(MPK_TLB_Refill)
	li	k0,TLB_HndlrV_Offs		# store index at top of stack
	sw	k0,-4(sp)
	dla	k1,MPK_Common_Hndlr
	j	k1
END(MPK_TLB_Refill)

	.align	7
LEAF(MPK_XTLB_Refill)
	li	k0,XTLB_HndlrV_Offs		# store index at top of stack
	sw	k0,-4(sp)
	dla	k1,MPK_Common_Hndlr
	j	k1
END(MPK_XTLB_Refill)

	.align	7
LEAF(MPK_CacheError)
	li	k0,CacheError_HndlrV_Offs	# store index at top of stack
	sw	k0,-4(sp)
	dla	k1,MPK_Common_Hndlr
	j	k1
END(MPK_CacheError)

	.align	7
LEAF(MPK_Others_Excs)
	li	k0,Others_HndlrV_Offs		# store index at top of stack
	sw	k0,-4(sp)
	dla	k1,MPK_Common_Hndlr
	j	k1
END(MPK_Others_Excs)

	.align	7
LEAF(MPK_Common_Hndlr)
	mfc0	k0,C0_Cause	# get the mask
	li	k1,Cause_ExcMsk
	and	k0,k1
	srl	k0,Cause_ExcShf
	li	k1,1
	sll	k1,k0

	dla	k0,MPK_Krnl_Data_Table
	lw	k0,OK_Exceptions_NOSK_Offs(k0)
	and	k0,k1
	bne	k0,zero,1f
	nop

	dla	k0,MPK_Krnl_Data_Table
	lw	k0,OK_Exceptions_SKIP_Offs(k0)
	and	k0,k1
	beq	k0,zero,MPK_unexpected_exception

	mfc0	k1,C0_EPC	# load epc
	addiu	k1,k1,4		# add 4 to it
	mtc0	k1,C0_EPC
1:
	lw	k1,-4(sp)
	dla	k0,MPK_Krnl_Data_Table
	daddu	k1,k0
	ld	k0,(k1)
	j	k0
END(MPK_Common_Hndlr)

	.align	7
LEAF(MPK_Null_Hndlr)
	eret
	nop
END(MPK_Null_Hndlr)

	.align	7
LEAF(MPK_unexpected_exception)
	halt(FAIL)
	nop
END(MPK_unexpected_exception)

	.data
	.align	7
EXPORT(MPK_Krnl_Data_Table)
	.dword	MPK_Null_Hndlr	# TLB_Refill_HndlrV_Offs
	.dword	MPK_Null_Hndlr	# XTLB_Refill_HndlrV_Offs
	.dword	MPK_Null_Hndlr	# CacheError_HndlrV_Offs
	.dword	MPK_Null_Hndlr	# Others_HndlrV_Offs
	.word	0		# OK_Exceptions_SKIP_Offs
	.word	0		# OK_Exceptions_NOSK_Offs

