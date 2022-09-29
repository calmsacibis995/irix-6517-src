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

#ident	"$Revision: 1.6 $"

#ifdef _R5000_CVT_WAR
/*
 * R5000 Rev. 1.1 cvt.[sd].l interpretation
 */
#include "ml/ml.h"
#include "sys/traplog.h"
#include "sys/mtext.h"

.extern VEC_trap
.extern VEC_fpe
.extern longway

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

LEAF(handle_ii)
EXL_EXPORT(locore_exl_14)
	/* check if this is the part with the problem */
	.set	noreorder
	mfc0	k1,C0_PRID
	NOP_0_4
	.set	reorder
	and	k1,(C0_IMPMASK | C0_MAJREVMASK)
	subu	k1,(C0_IMP_TRITON << C0_IMPSHIFT) | (1 << C0_MAJREVSHIFT)
	beqz	k1,rev_1_part			
	j	longway				# Not a R5000 Rev. 1.* part

rev_1_part:
	/* fetch illegal instruction */
	/* (TLB entry must be valid if we got this trap) */
	.set	noreorder
	mfc0	k1,C0_EPC
	mfc0	k0,C0_CAUSE
	.set	reorder
	sreg	t0,VPDA_T0SAVE(zero)
	and	t0,k0,CAUSE_BD
	.set	noreorder
	beqzl	t0,1f
	lwu	t0,0(k1)			# instruction at $epc
	lwu	t0,4(k1)			# instruction at $epc + 4
	.set reorder
1:
	/* check for interpreted pseudo-instructions */
	and	k1,t0,CVT_CHECK_MASK
	subu	k1,INTCVT_D_L_ID
	beqz	k1,2f
	subu	k1,INTCVT_S_L_ID-INTCVT_D_L_ID
	beqz	k1,2f
3:
	/* not a matching instruction-->normal trap */
	lreg	t0,VPDA_T0SAVE(zero)
	j	longway

2:
	and	k1,k0,CAUSE_BD
	bnez	k1,3b				# branch delay-->longway
	
	/* check if FPU enabled */
	.set	noreorder
	mfc0	k0,C0_SR
	.set	reorder
	and	k0,SR_CU1			# non-zero if FPU enabled
	beqz	k0,3b				# not enabled-->longway

	/* get global addressability */
	sreg	gp,VPDA_K1SAVE(zero)
	LA	gp,_gp

	/* fetch the source register value */
	srl	k1,t0,(11-3)
	and	k1,(0x1F<<3)		# rs field
	la	k0,11f
	addu	k0,k1
	j	k0

	.set	noreorder

#define FP1REG_TO_K0(reg) \
	b	12f		; \
	dmfc1	k0,$f/**/reg

11:	/* table of FPU register fetches */
	FP1REG_TO_K0(0)
	FP1REG_TO_K0(1)
	FP1REG_TO_K0(2)
	FP1REG_TO_K0(3)
	FP1REG_TO_K0(4)
	FP1REG_TO_K0(5)
	FP1REG_TO_K0(6)
	FP1REG_TO_K0(7)
	FP1REG_TO_K0(8)
	FP1REG_TO_K0(9)
	FP1REG_TO_K0(10)
	FP1REG_TO_K0(11)
	FP1REG_TO_K0(12)
	FP1REG_TO_K0(13)
	FP1REG_TO_K0(14)
	FP1REG_TO_K0(15)
	FP1REG_TO_K0(16)
	FP1REG_TO_K0(17)
	FP1REG_TO_K0(18)
	FP1REG_TO_K0(19)
	FP1REG_TO_K0(20)
	FP1REG_TO_K0(21)
	FP1REG_TO_K0(22)
	FP1REG_TO_K0(23)
	FP1REG_TO_K0(24)
	FP1REG_TO_K0(25)
	FP1REG_TO_K0(26)
	FP1REG_TO_K0(27)
	FP1REG_TO_K0(28)
	FP1REG_TO_K0(29)
	FP1REG_TO_K0(30)
	FP1REG_TO_K0(31)
	.set	reorder
	
12:	
	/* check for problem values */
	dsra	k1,k0,51
	beqz	k1,14f
	addiu   k1, 1
	beqz	k1,14f
13:	
	/* problem value: handle the long way */
	lreg	gp,VPDA_K1SAVE(zero)
	b	3b				# out of range-->longway
	
14:	
	/* convert the value */
	sdc1	$f0,VPDA_FP0SAVE(zero)
	dmtc1	k0,$f0
	and	k1,t0,CVT_CHECK_MASK
	subu	k1,INTCVT_D_L_ID
	.set	noreorder
	beqzl	k1,15f
	cvt.d.l	$f0,$f0
	cvt.s.l $f0,$f0
15:	
	.set	reorder
	
	/* set the destination register */
	srl	k1,t0,(6-3)
	and	k1,(0x1F<<3)		# rd field
	la	k0,21f
	addu	k0,k1
	dmfc1	k1,$f0
	ldc1	$f0,VPDA_FP0SAVE(zero)
	j	k0

	.set	noreorder

#define K1_TO_FP1REG(reg) \
	b	22f		; \
	dmtc1	k1,$f/**/reg

21:	/* table of FPU register fetches */
	K1_TO_FP1REG(0)
	K1_TO_FP1REG(1)
	K1_TO_FP1REG(2)
	K1_TO_FP1REG(3)
	K1_TO_FP1REG(4)
	K1_TO_FP1REG(5)
	K1_TO_FP1REG(6)
	K1_TO_FP1REG(7)
	K1_TO_FP1REG(8)
	K1_TO_FP1REG(9)
	K1_TO_FP1REG(10)
	K1_TO_FP1REG(11)
	K1_TO_FP1REG(12)
	K1_TO_FP1REG(13)
	K1_TO_FP1REG(14)
	K1_TO_FP1REG(15)
	K1_TO_FP1REG(16)
	K1_TO_FP1REG(17)
	K1_TO_FP1REG(18)
	K1_TO_FP1REG(19)
	K1_TO_FP1REG(20)
	K1_TO_FP1REG(21)
	K1_TO_FP1REG(22)
	K1_TO_FP1REG(23)
	K1_TO_FP1REG(24)
	K1_TO_FP1REG(25)
	K1_TO_FP1REG(26)
	K1_TO_FP1REG(27)
	K1_TO_FP1REG(28)
	K1_TO_FP1REG(29)
	K1_TO_FP1REG(30)
	K1_TO_FP1REG(31)
	.set	reorder
	
22:	
#ifdef notyet
	/* check if branch delay slot */		
	.set	noreorder
	mfc0	k0,C0_CAUSE
	.set	reorder
	and	k0,CAUSE_BD
	bnez	k0,24f			# -->handle branch interpretation
#endif /* notyet */

	/* not a delay slot: advance PC and return */
	.set	noreorder
	dmfc0	k0,C0_EPC
	addu	k0,4
	dmtc0	k0,C0_EPC
	.set	reorder
	
23:	
	/* restore registers */
	lreg	gp,VPDA_K1SAVE(zero)
	lreg	t0,VPDA_T0SAVE(zero)
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)
	.set	at
	.set	noreorder
	eret
EXL_EXPORT(elocore_exl_14)
	.set	reorder
		
#ifdef notyet
24:	
	/* interpret branch */
	/* XXXX */
	b	23b
#endif /* notyet */
	END(handle_ii)

/*
 * VEC_ii
 * Illegal Instruction, when interpreting R5000 cvt.[sd].l instructions
 *
 * On entry:
 *	a0 points to exception frame (k1 does too, but use a0)
 *	sp points to stack
 *	s0 contains orig SR (with interrupts disabled)
 * 	a1 contains software exception code
 *	a3 - cause register
 */
VECTOR(VEC_ii, M_EXCEPT)	
EXL_EXPORT(locore_exl_15)
	/* check if this is the part with the problem */
	lw	t0,R5000_cvt_war
	bnez	t0,war_on
	j	VEC_trap			# fall through

war_on:
	/* fetch illegal instruction */
	/* (TLB entry must be valid if we got this trap) */
	lreg	t1,EF_EPC(a0)
	and	t0,a3,CAUSE_BD
	.set	noreorder
	beqzl	t0,1f
	lwu	t0,0(t1)			# instruction at $epc
	lwu	t0,4(t1)			# instruction at $epc + 4
	.set reorder
1:
	/* check for interpreted pseudo-instructions */
	and	t2,t0,CVT_CHECK_MASK
	subu	t2,INTCVT_D_L_ID
	beqz	t2,2f
	subu	t2,INTCVT_S_L_ID-INTCVT_D_L_ID
	beqz	t2,2f
	j	VEC_trap

2:	
	/* patch the cause */
	li	a1,EXC_FPE
	and	a3,~CAUSE_EXCMASK
	or	a3,a1
	sreg	a3,EF_CAUSE(a0)
	j	VEC_fpe	
EXL_EXPORT(elocore_exl_15)
	END(VEC_ii)
#endif	/* _R5000_CVT_WAR */
