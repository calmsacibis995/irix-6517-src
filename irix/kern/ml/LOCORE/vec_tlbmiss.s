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

.extern	soft_trap
.extern	exception_exit

/*
 * TLB miss. 
 * Handles TLBMiss Read and TLBMiss Write
 */
VECTOR(VEC_tlbmiss, M_EXCEPT)
EXL_EXPORT(locore_exl_3)
	.set noreorder
	lreg	a2,EF_BADVADDR(k1)	# arg3 is bad adr
#if R4000 || R10000
	and	k0,s0,SR_IMASK|SR_KERN_SET	# kernel base mode, EXL low
	or	k0,SR_IE		# and interrupts enabled.
#endif
#if TFP
	and	k0,s0,SR_IMASK|SR_KERN_USRKEEP	# kernel base mode, EXL low
	or	k0,SR_IEC|SR_KERN_SET	# enable interrupts
#endif
EXL_EXPORT(elocore_exl_3)
	MTC0(k0,C0_SR)
	NOP_0_4
	jal	tlbmiss			# tlbmiss(ef_ptr, code, vaddr, cause)
	move	s1,a0			# BDSLOT save eframe pointer
	lreg	s0,EF_SR(s1)		# tlbmiss can alter return SR
	bne	v0,zero,2f		# zero if legal address
	nop				# BDSLOT
#if TFP
	and	s2,s0,SR_IMASK|SR_KERN_USRKEEP
	or	s2,SR_KERN_SET
	MTC0( s2, C0_SR)
#endif
#if R4000 || R10000
	# rather than raising the EXL bit to disable interrupts,
	# put the processor into kernel mode with interrupts disabled.
	and	s2,s0,SR_IMASK|SR_KERN_SET|SR_DEFAULT
	MTC0( s2, C0_SR)
	NOP_0_4
#endif
	j	exception_exit
	move	k1,s1			# restore ep BDSLOT
	.set	reorder
2:
	move	a0,s1			# restore ep
	move	a1,v0			# software exception code
	lreg	a3,EF_CAUSE(a0)	# restore cause since tlbmiss can trash
	j	soft_trap		# handle as trap
	END(VEC_tlbmiss)
