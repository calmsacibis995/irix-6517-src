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

#ident	"$Revision: 1.2 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/*
 * TRAP
 * Illegal Instruction, and Overflow.
 * On entry:
 *	a0 points to exception frame (k1 does too, but use a0)
 *	sp points to stack
 *	s0 contains orig SR (with interrupts disabled)
 * 	a1 contains software exception code
 *	a3 - cause register
 * Also handles software exceptions raised by tlbmod and tlbmiss,
 * NOTE: tlbmod and tlbmiss replace the original exception code with
 * an appropriate software exception code.
 */
VECTOR(VEC_trap, M_EXCEPT)
	.set 	noreorder

/* branch here from tlbmod / tlbmiss / resched */
EXPORT(soft_trap)
EXL_EXPORT(locore_exl_24)
#if TFP
	and	a2,s0,SR_IMASK|SR_KERN_USRKEEP	# kernel base mode, EXL low
	or	a2,SR_IE|SR_KERN_SET		# and interrupts enabled.
#endif
#if R4000 || R10000
	and	a2,s0,SR_IMASK|SR_ERL|SR_KERN_SET  # kernel base mode, EXL low
	or	a2,SR_IE		# and interrupts enabled.
	.set	noat
	lui	AT,(SR_DE >> 16)	# propagate the SR_DE bit
	and	AT,AT,s0
	or	a2, a2, AT
	.set	at
#endif
	MTC0(a2,C0_SR)
EXL_EXPORT(elocore_exl_24)
	NOP_0_4
	/*
	 * ENTRY CONDITIONS: interrupts enabled
	 * a0 has ep
	 * a1 contains software exception code
	 * a3 has cause register
	 * s0 has SR
	 */
	move	a2,s0
	jal	trap			# trap(ef_ptr, code, sr, cause)
	move	s1,a0			# BDSLOT save ep
#if TFP
	and	s2,s0,SR_IMASK|SR_KERN_USRKEEP # disable interrupts
	or	s2,SR_KERN_SET
	DMTC0(s2,C0_SR)
#endif	/* TFP */
#if R4000 || R10000
	and	s2,s0,SR_IMASK|SR_KERN_SET	# disable interrupts
	MTC0(s2,C0_SR)
	NOP_0_4
#endif
	.set 	reorder
	move	k1,s1
	j	exception_exit
	END(VEC_trap)
