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

#ident	"$Revision: 1.1 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

.extern	soft_trap
.extern	exception_exit

/*
 * TLB mod.
 * Entry:
 *	k1 - exception pointer
 *	a0 - exception pointer
 *	a1 - exception code
 *	a3 - cause register
 *	s0 - sr at time of exception
 */
VECTOR(VEC_tlbmod, M_EXCEPT)
EXL_EXPORT(locore_exl_2)
	.set	noreorder
	lreg	a2,EF_BADVADDR(k1)	# arg3 is bad addr
#if R4000 || R10000
	and	k0,s0,SR_IMASK|SR_KERN_SET	# kernel base mode, EXL low
	or	k0,SR_IE		# and interrupts enabled.
EXL_EXPORT(elocore_exl_2)
	MTC0(k0,C0_SR)
	NOP_0_4
	jal	tlbmod			# tlbmod(ef_ptr, code, vaddr, cause)
	move	s1,a0			# BDSLOT save eframe pointer
	bne	v0,zero,1f		# zero if legal to modify
	nop				# BDSLOT

	/* successful */
	# rather than raising the EXL bit to disable interrupts,
	# put the processor into kernel mode with interrupts disabled.
	and	s2,s0,SR_IMASK|SR_KERN_SET|SR_DEFAULT
	MTC0(s2, C0_SR)
	NOP_0_4
#endif
#if TFP
	and	s2,s0,SR_IMASK|SR_KERN_USRKEEP	# kernel base mode, EXL low
	or	s2,SR_IEC|SR_KERN_SET	# enable interrupts
	MTC0(s2,C0_SR)
	jal	tlbmod			# tlbmod(ef_ptr, code, vaddr, cause)
	move	s1,a0			# BDSLOT save eframe pointer
	bne	v0,zero,1f		# zero if legal to modify
	xori	s2,SR_IEC		# BDSLOT turn off interrupt enable

	/* successful */
	MTC0(s2, C0_SR)
#endif
	j	exception_exit
	move	k1,s1			# BDSLOT restore ep
	.set	reorder
1:
	move	a0,s1			# restore ep
	move	a1,v0			# move software exception code
	lreg	a3,EF_CAUSE(a0)		# restore cause since tlbmod can trash
	j	soft_trap		# and handle as trap
	END(VEC_tlbmod)
