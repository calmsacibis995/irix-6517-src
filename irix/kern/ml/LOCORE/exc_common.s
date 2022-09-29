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


LEAF(ecommon)
EXL_EXPORT(locore_exl_16)
	/*
	 * This is common state saving code, at this point 
	 *	sp - points to new stack frame
	 *	at - has been saved in PDA
	 *	k0 - contains new timer mode to switch to
	 *	k1 - contains pointer to exception save area
	 * sp & gp have already been saved in save area
	 * k1 if necessary has already been saved
	 *
	 * R10000_SPECULATION_WAR: $sp always set by ecommon.
	 */
#if TFP
	/*********************************************************************
	 * TFP code starts here
	 *
	 ********************************************************************/
	.set	noreorder
	SAVESREGS_GETCOP0(k1)		# save sregs and get COP0 regs

	/* WARNING: Above macro ends with "dmfc0" which can lead to a
	 * COP0 hazard condition.  Note that SAVEAREGS introduces plenty of
	 * cycles before the jal, whose use of the "miscBus" could interfere
	 * with the dmfc0 use of the miscBus.
	 */
	.set	reorder
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)	# recall saved AT
	sreg	AT,EF_AT(k1)
	.set	at
	SAVEAREGS(k1)
	sreg	ra,EF_RA(k1)
	jal	tfi_save		# save tregs, v0,v1, mdlo,mdhi

EXPORT(ecommon_allsaved)
	/*
	 * Switch to new timer mode(k0).  Don't do the switch if either
	 * curkthread == NULL or newtimer&AS_DONT_SWITCH != 0.  Save the old
	 * timer mode in s7.  VEC_int() will use this to restore the old mode
	 * on interrupt exit.  All of this is extremely complicated and should
	 * probably be handled simply by saving and restoring the timer mode.
	 * Sadly we don't have the time right now to make the changes
	 * necessary for that simplification.
	 */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	andi	a1,k0,MAX_PROCTIMERMASK
	beq	a0,zero,1f
	lw	s7,K_CURTIMER(a0)
	andi	t0,k0,AS_DONT_SWITCH
	bne	t0,zero,1f
	jal	ktimer_switch
1:
	/* SAVEREGS_GETCOP0 returns:
	 *	s0 == C0_SR
	 *	s1 == C0_CONFIG
	 *	s2 == C0_CAUSE
	 *	s3 == C0_EPC
	 */
	.set	noreorder
	sreg	s0,EF_SR(k1)		# s0 == SR
	sreg	s1,EF_CONFIG(k1)	# s1 == CONFIG
	MFC0_BADVADDR_NOHAZ(s4)
	sreg	s2,EF_CAUSE(k1)
	sreg	s3,EF_EPC(k1)		# s3 == EPC
	sreg	s3,EF_EXACT_EPC(k1)	# s3 == EPC
	.set	reorder
	sreg	s4,EF_BADVADDR(k1)
	move	a3,s2			# a3 = (s2 == CAUSE)
	move	a0,s4			# a0 = (s4 == BADVADDR)
	and	a1,a3,CAUSE_EXCMASK
	SAVEFPREGS(s0,k1,k0)		# save FP temp reg, needs s0 == C0_SR

#else	/* !TFP */

	/*********************************************************************
	 * non-TFP code starts here
	 *
	 ********************************************************************/
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)	# recall saved AT
	sreg	AT,EF_AT(k1)
	.set	at
	SAVEAREGS(k1)
	SAVESREGS(k1)
	sreg	ra,EF_RA(k1)
	jal	tfi_save		# save tregs, v0,v1, mdlo,mdhi

EXPORT(ecommon_allsaved)
	/*
	 * Switch to new timer mode(k0).  Don't do the switch if either
	 * curkthread == NULL or newtimer&AS_DONT_SWITCH != 0.  Save the old
	 * timer mode in s7.  VEC_int() will use this to restore the old mode
	 * on interrupt exit.  All of this is extremely complicated and should
	 * probably be handled simply by saving and restoring the timer mode.
	 * Sadly we don't have the time right now to make the changes
	 * necessary for that simplification.
	 */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	andi	a1,k0,MAX_PROCTIMERMASK
	beq	a0,zero,1f
	lw	s7,K_CURTIMER(a0)
	andi	t0,k0,AS_DONT_SWITCH
	bne	t0,zero,1f
	jal	ktimer_switch
1:
	.set noreorder
	MFC0(a3,C0_CAUSE)
	dmfc0   a0,C0_EPC

	.set reorder
	sreg	a3,EF_CAUSE(k1)
	.set noreorder
	MFC0(s0,C0_SR)
	NOP_0_1
	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 sill serialize
	sreg	a0,EF_EPC(k1)
	sreg	a0,EF_EXACT_EPC(k1)	
	DMFC0(a0,C0_BADVADDR)		# LDSLOT
	sreg	s0,EF_SR(k1)
	.set	reorder
	SAVEFPREGS(s0,k1,k0)		# save FP temp reg, needs s0 == C0_SR
	sreg	a0,EF_BADVADDR(k1)
	and	a1,a3,CAUSE_EXCMASK
	AUTO_CACHE_BARRIERS_ENABLE

#endif	/* !TFP */

	/*********************************************************************
	 * COMMON code re-starts here
	 *
	 ********************************************************************/
	/*
	 * Dispatch to appropriate exception handler
	 * Register setup:
	 *	s0 -- SR register at time of exception
	 *	a0 -- exception frame pointer
	 *	k1 -- exception frame pointer
	 *	a1 -- cause code
	 *	a3 -- cause register
	 */
#if (R4000 || R10000) && _MIPS_SIM == _ABI64
	sll	a2,a1,1		/* a0 == 8 byte pointer offset */
	PTR_L	a2, VPDA_CAUSEVEC(a2)
#else
	PTR_L	a2,causevec(a1)
#endif
	move	a0,k1
	j	a2
	/* NOTREACHED */
EXL_EXPORT(elocore_exl_16)

	END(ecommon)
