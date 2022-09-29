/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.10 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

#if R4000 || R10000
/*
 * FPE
 * Floating Point Exception.
 * Entry:
 *	k1 - exception pointer
 *	a0 - exception pointer
 *	a1 - exception code
 *	a3 - cause register
 *	s0 - sr at time of exception
 */
VECTOR(VEC_fpe, M_EXCEPT)
EXL_EXPORT(locore_exl_9)
	# Panic on floating point exceptions in kernel mode.
	and	t0,s0,SR_PREVMODE
	bne	t0,zero,1f		# From user

	and	a2,s0,SR_IMASK|SR_KERN_SET	# kernel base mode, EXL low
	or	a2,SR_IE		# and interrupts enabled.
	.set 	noreorder
	MTC0(a2,C0_SR)
EXL_EXPORT(elocore_exl_9)
	NOP_0_4
	.set	reorder

	move	a2,s0			# status reg at exc. time
	jal	trap
	/* NOTREACHED */
EXL_EXPORT(locore_exl_22)
1:
	# Before enabling interrupts, save the faulting instruction
	# and the branch delay slot instruction in the pda.
	lreg	t0,EF_CAUSE(k1)
	lreg	t1,EF_EPC(k1)		# addr of faulting inst.
	lw	t2,0(t1)		# faulting inst.
	PTR_L	t3,VPDA_CURUTHREAD(zero) # ptr to current user thread
	sw	t2,UT_EPCINST(t3)
	bgez	t0,1f			# skip if BD not set in cause
	lw	t2,4(t1)		# inst in BD slot.
	sw	t2,UT_BDINST(t3)
1:
	.set 	noreorder
	and	a2,s0,SR_IMASK|SR_KERN_SET|SR_FR  # kernel base mode, EXL low
	or	a2,SR_IE		# and interrupts enabled.
	MTC0(a2,C0_SR)
EXL_EXPORT(elocore_exl_22)
	NOP_0_4
	/*
	 * ENTRY CONDITIONS: interrupts enabled
	 * a0 has ep
	 */
	jal	fp_intr			# fp_intr(ef_ptr)
	move	s1,a0			# BDSLOT save ep

	and	s2,s0,SR_IMASK|SR_KERN_SET		# disable interrupts
	MTC0(s2,C0_SR)
	NOP_0_4

	.set 	reorder
	move	k1,s1			# s1 has ep
	j	exception_exit
	END(VEC_fpe)

#endif	/* R4000 || R1000 */
