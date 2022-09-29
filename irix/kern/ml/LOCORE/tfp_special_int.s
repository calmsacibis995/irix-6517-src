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

#if TFP
/*
 * tfp_special_int 
 *
 * On TFP, Bus Errors, FP exceptions, and VCEs occur as interrupt
 * exceptions but with special bits set in the CAUSE register.  Vector to
 * the correct exception handler.
 *
 * Entry:
 *	k1 - exception pointer
 *	a0 - exception pointer
 *	a1 - exception code
 *	a3 - cause register
 *	s0 - sr at time of exception
 */
VECTOR(tfp_special_int, M_EXCEPT)
	li	s1,CAUSE_BE
	and	s1,a3
	bne	s1,zero,tfp_be_int	# would be IBE or DBE on other CPUs
	li	s1,CAUSE_FPI
	and	s1,a3
	bne	s1,zero,fpint		# FPE exception on other CPUs

	/*
	 * Virtual Coherency Interrupt (from FP) and TLBX both use
	 * the same bit in the Cause register. VCI shouldn't occur
	 * since we use 16K pagesize, so the only possibility is TLBX.
	 * If that happens, we're in bad shape, so just sit here and
	 * wait for an NMI.
	 */
	.set	noreorder
vci_tlbx:
	b	vci_tlbx
	nop

fpint:
	/* TFP - Floating Point Interrupt
	 */
	and	a2,s0,SR_IMASK|SR_KX|SR_KERN_USRKEEP # kernel base mode, EXL low
	or	a2,SR_KERN_SET

	/* INTERRUPTS disabled - or we get another immediate FP	interrupt.
	 */
	DMTC0(a2,C0_SR)
	/*
	 * ENTRY CONDITIONS: interrupts disabled
	 * a0 has ep
	 */
	jal	fp_intr			# fp_intr(ef_ptr)
	move	s1,a0			# BDSLOT save ep

tfp_int_cleanup:

	and	s2,s0,SR_IMASK|SR_KERN_USRKEEP	# disable interrupts
	or	s2,SR_KERN_SET
	DMTC0(s2,C0_SR)
	.set 	reorder
	move	k1,s1			# s1 has ep
	j	tfp_special_int_exit

	/*
	 * TFP Bus Error Interrupt
	 */
tfp_be_int:
	.set 	noreorder
#define TFP_BUSERROR_DEBUG 1
#if TFP_BUSERROR_DEBUG
        LI      k0, K1BASE
        sb      zero, 0x10(k0)
#endif
				# kernel base mode, EXL low, ints disabled.
	and	a2,s0,SR_IMASK|SR_KERN_USRKEEP
	or	a2,SR_KERN_SET
	DMTC0(a2,C0_SR)
	/*
	 * ENTRY CONDITIONS: interrupts enabled
	 * a0 has ep
	 * a1 contains software exception code
	 * a3 has cause register
	 * s0 has SR
	 */
#ifdef MCCHIP
	SAVE_ERROR_STATE(a2,s1)
	clearbuserrm
#endif
	move	a2,s0
	jal	trap			# trap(ef_ptr, code, sr, cause)
	move	s1,a0			# BDSLOT save ep
	.set	reorder
	b	tfp_int_cleanup
	END(tfp_special_int)
#endif /* TFP */
