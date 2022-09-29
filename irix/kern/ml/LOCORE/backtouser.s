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

#ident	"$Revision: 1.26 $"

#include "ml/ml.h"
#if defined(USE_PTHREAD_RSA)
	/*
	 *	k1 == UT_EFRAME
	 *	a0 == VPDA_CURUTHREAD
	 */
LEAF(backtouser_rsa)
	move	s3, k1			/* s3 == UT_EFRAME */
	PTR_L	s0, VPDA_CURUTHREAD(zero)
	/* 
	 * Check if process is nanothreaded, if so, point state saving
	 * at the kusharena: 
	 *      if (curprocp && curprocp->p_shaddr
	 *              && curprocp->p_shaddr->s_sharena  &&
	 *                  curprocp->p_vpid >= 0)
	 *         savep = curprocp->p_shaddr->s_sharena->rsa[curprocp->p_vpid];
	 */
	.set    noat
	lreg	AT, UT_PRDA(s0)
	beq	AT, zero, backtouser_rsa_restart
	lw      AT, PRDA_NID(AT)

	/* range check value of NID */
	
	blez	AT, backtouser_rsa_restart
	addi	s1, AT, -NT_MAXNIDS
	bgtz	s1, backtouser_rsa_restart
	
	PTR_L   s1, UT_SHARENA(s0)

	/* convert NID to RSAID */

	sll	AT, 1			/* convert NID to int16 offset */
	PTR_ADD	AT, s1
	lhu	AT, KUS_NIDTORSA(AT)	/* AT == RSAID */

	/* Now check rsa number against range of values actually allocated
	 * for the current uthread.
	 */
	beq	AT, zero, backtouser_rsa_restart	/* RSAID zero invalid */
	lhu	s4, UT_MAXRSAID(a0)
	sltu	s4, AT, s4		/* range check RSAID */
	beqz	s4, backtouser_rsa_restart
	
	/* finally have valid RSAID */
	
#ifdef DEBUG
	bgez    AT, 1f			# if rsaid < 0, then must be -1 !
	PANIC("bad value in UT_RSAID\n"); 
1:		
	/* temporary debug code */
	bne     s1, zero, 1f
	PANIC("bad value in UT_SHARENA\n"); 
1:		
#endif /* DEBUG */

	daddi   k1,s1,KUS_RSA		# k1 <- &(s_sharena->rsa[0])
	/* NOTE: Each rsa is 640 bytes long (5*128) so we multiply the UT_RSAID
	 * by 128 (shift 7 bits) then add it to base address 5 times.
	 */
	sll     AT,AT,7			# AT == rsaid * 128
	daddu	k1,AT
	sll	AT,AT,2			# AT == rsaid * 4*128
	daddu   k1,AT			# k1 contains s_sharena->rsa[rsaid]

	move	s4, k1
	/* START - in-line backtouser code
	 *
	 * "split-eframe"
	 *	s3 == UT_EFRAME (for kernel modifiable registers)
	 *	s4 == RSA (for user modifiable registers)
	 */
#if TFP_MOVC_CPFAULT_FAST_WAR
	/* The following check is repeated inside tfp_loadfp routine.  We
	 * perform a quick check here to handle the common case and avoid
	 * invoking the tfp_loadfp routine.
	 */
	PTR_L	a0,VPDA_FPOWNER(zero)
	PTR_L	a1,VPDA_CURUTHREAD(zero)
	beq	a0,a1,1f
	jal	tfp_loadfp		# load fp registers if necessary
1:
#endif	/* TFP_MOVC_CPFAULT_FAST_WAR */
#if EVEREST && DEBUG
	lreg	a0,EF_SR(s3)
	jal	isspl0
	bne	v0,zero,1f
	lreg	a2,EF_SR(s3)
	lb	a3,VPDA_CELSHADOW(zero)
	PANIC("backtouser: NOT spl0, SR 0x%x CEL 0x%x");
1:
#endif
#ifdef DEBUG
	
	lreg	a0,EF_SR(s3)
	li	v0, SR_CU1
	and	a0,v0
	beq	a0, zero, 1f
	PTR_L	a0, VPDA_CURUTHREAD(zero)
	PTR_L	v0, VPDA_FPOWNER(zero)
	beq	a0,v0,1f

	move	a2, a0
	move	a3, v0	
	PANIC("backtouser: bad SR_CU1, curuthread 0x%x vpda_fpowner 0x%x");
1:
#endif /* DEBUG */		

	/* switch to user CPU timer */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	LI	a1,AS_USR_RUN
	jal	ktimer_switch

	sb	zero, VPDA_KSTACK(zero)
	lreg	gp,RSA_GP(s4)
1:
	/*
	 * wire in users tlb entries for wired slots
	 * NOTE: we can use the t registers
	 * We may NOT actually write the entry for the upage until
	 * ALL the registers are restored
	 * We therefore do slot2 first and all but the WRITEI for slot1
	 * NOTE2: to avoid a race with setup_wired_tlb - we are
	 * carefull to grab the physical address first - then the
	 * virtual - setup_wired_tlb does things in the reverse order
	 * so we're guaranteed that if physical address is 0 the
	 * virtual address is also invalid.
	 */
#if (R4000 || R10000) && TLBKSLOTS == 1
	.set	noreorder
	/* Only need to restore one pair of TLB entries */
	PTR_L	k0, VPDA_CURUTHREAD(zero)
	PTR_L	k0, UT_EXCEPTION(k0)	# LDSLOT
	PTE_L	t0, U_USLKSTKLO_1(k0)	# LDSLOT
	PTE_L	k0, U_USLKSTKLO(k0)	# LDSLOT
	TLBLO_FIX_HWBITS(t0)
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(t0, C0_TLBLO_1)		# LDSLOT
	nop	# shut up assembler warnings
	DMFC0(t1, C0_TLBHI)		# get tlbpid
	PTR_L	t0, VPDA_CURUTHREAD(zero)	# LDSLOT
	PTR_L	t0, UT_EXCEPTION(t0)	# LDSLOT
	PTR_L	t0, U_USLKSTKHI(t0)	# LDSLOT
	andi	t1, TLBHI_PIDMASK	# LDSLOT
	or	t0, t1
	DMTC0(t0, C0_TLBHI)		# set virtual page number
	li	t0, UKSTKTLBINDEX	# base of wired entries
	mtc0	t0, C0_INX		# LDSLOT set index to wired entry
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO)		# set pfn and access bits
	# entry gets written just before RFE
	.set	reorder
#endif	/* (R4000 || R10000) && TLBKSLOTS == 1 */

	lreg	v0,RSA_MDLO(k1)
	lreg	v1,RSA_MDHI(k1)
	mtlo	v0
	mthi	v1
	lreg	v0,RSA_V0(k1)
	lreg	v1,RSA_V1(k1)
	lreg	t0,RSA_T0(k1)
	lreg	t1,RSA_T1(k1)
	lreg	t2,RSA_T2(k1)
	lreg	t3,RSA_T3(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	lreg	t4,RSA_T4(k1)
	lreg	t5,RSA_T5(k1)
	lreg	t6,RSA_T6(k1)
	lreg	t7,RSA_T7(k1)
#endif
	lreg	t8,RSA_T8(k1)
	lreg	t9,RSA_T9(k1)
	lreg	fp,RSA_FP(k1)
	
	lreg	a0,RSA_A0(k1)
	lreg	a1,RSA_A1(k1)
	lreg	a2,RSA_A2(k1)
	lreg	a3,RSA_A3(k1)
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	lreg	a4,RSA_A4(k1)
	lreg	a5,RSA_A5(k1)
	lreg	a6,RSA_A6(k1)
	lreg	a7,RSA_A7(k1)
#endif		
	.set	noreorder
#if TFP
	lreg	s2,EF_CONFIG(s3)
	lreg	sp,RSA_SP(k1)
	lreg	ra,RSA_RA(k1)
	.set	noat
	lreg	AT,RSA_AT(k1)
	dmtc0	s2,C0_CONFIG
	.set	reorder
	RESTOREFPREGS_TOUSER(s0)	# load FP regs, s0 temp
	.set	noreorder
#else /* !TFP */
	mfc0	s2, C0_SR		# BDSLOT s2 = current C0_SR
	lreg	ra,RSA_RA(k1)
	.set	noat
	lreg	AT,RSA_AT(k1)		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# now have users $sp
	lreg	sp,RSA_SP(k1)

	/* in-line modified version of RESTORESREGS_SR_EPC which
	 * loads registers from split-eframe.
	 */
	ori	s2,SR_EXL
	mtc0	s2,C0_SR
#endif /* !TFP */
	
EXL_EXPORT(locore_exl_17)
	lreg	s6,EF_SR(s3)		/* C0_SR loaded from UT_EFRAME */
	lreg	s7,RSA_EPC(k1)
	lreg	s0,RSA_S0(k1)
#if TFP
	dmtc0	s6,C0_SR
#else
	mtc0	s6,C0_SR
#endif
	lreg	s1,RSA_S1(k1)
	lreg	s2,RSA_S2(k1)
	lreg	s3,RSA_S3(k1)
	DMTC0(s7,C0_EPC)
	lreg	s4,RSA_S4(k1)
	lreg	s5,RSA_S5(k1)
	lreg	s6,RSA_S6(k1)
	lreg	s7,RSA_S7(k1)
	
#if TLBKSLOTS == 1
	nop				# LDSLOT
	c0	C0_WRITEI		# write entry
	NOP_0_4
#endif	/* TLBKSLOTS == 1 */
	eret
EXL_EXPORT(elocore_exl_17)
	/* END - in-line backtouser code */
	AUTO_CACHE_BARRIERS_ENABLE
	END(backtouser_rsa)	
#endif /* USE_PTHREAD_RSA */

	.set	reorder
LEAF(backtouser)
	/*
	 * backtouser - restore all registers and re-wire user's tlb
	 * entries for their slots
	 * Entry:
	 *	k1 - eframe which MUST be u.u_eframe and NOT on the users kernel
	 *		stack
	 */
#if defined(USE_PTHREAD_RSA)
	PTR_L	a0, VPDA_CURUTHREAD(zero)
	/*
	 * If UT_RSA_RUNABLE == 1, then must resume from the RSA.  All other
	 * values cause us to resume from the UT_EXCEPTION area.
	 */
	.set	noreorder
	lb	a2, UT_RSA_RUNABLE(a0)
	addi	a2, -1
	beq	a2, zero, backtouser_rsa
	sb	zero, UT_RSA_RUNABLE(a0)	# BDSLOT - clear flag
	.set	reorder
	
backtouser_rsa_restart:		
#endif /* USE_PTHREAD_RSA */	
	
#if defined(ULI) && defined(DEBUG)
	PTR_L	a0, VPDA_CURULI(zero)
	beq	a0, zero, 1f
	move	a2, k1
	PANIC("backtouser during ULI, ep 0x%x\n");
1:
#endif
#if TFP_MOVC_CPFAULT_FAST_WAR
	/* The following check is repeated inside tfp_loadfp routine.  We
	 * perform a quick check here to handle the common case and avoid
	 * invoking the tfp_loadfp routine.
	 */
	PTR_L	a0,VPDA_FPOWNER(zero)
	PTR_L	a1,VPDA_CURUTHREAD(zero)
	beq	a0,a1,1f
	jal	tfp_loadfp		# load fp registers if necessary
1:
#endif	/* TFP_MOVC_CPFAULT_FAST_WAR */
#if DEBUG
	lreg	a0,EF_SR(k1)
	jal	isspl0
	bne	v0,zero,1f
	lreg	a2,EF_SR(k1)
#if EVEREST
	lb	a3,VPDA_CELSHADOW(zero)
	PANIC("backtouser: NOT spl0, SR 0x%x CEL 0x%x");
#else
	PANIC("backtouser: NOT spl0, SR 0x%x");
#endif
1:
#endif 
#ifdef DEBUG
	/* check to make sure we don't go backtouser with SR_CU1 set unless
	 * we also own the FP.
	 */
	lreg	a0,EF_SR(k1)
	li	v0, SR_CU1
	and	a0,v0
	beq	a0, zero, 1f
	PTR_L	a0, VPDA_CURUTHREAD(zero)
	PTR_L	v0, VPDA_FPOWNER(zero)
	beq	a0,v0,1f

	move	a2, a0
	move	a3, v0	
	PANIC("backtouser: bad SR_CU1, curuthread 0x%x vpda_fpowner 0x%x");
1:
#endif /* DEBUG */		

	/* switch to user CPU timer */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	LI	a1,AS_USR_RUN
	jal	ktimer_switch

	sb	zero, VPDA_KSTACK(zero)
	lreg	gp,EF_GP(k1)
1:
	/*
	 * wire in users tlb entries for wired slots
	 * NOTE: we can use the t registers
	 * We may NOT actually write the entry for the upage until
	 * ALL the registers are restored
	 * We therefore do slot2 first and all but the WRITEI for slot1
	 * NOTE2: to avoid a race with setup_wired_tlb - we are
	 * carefull to grab the physical address first - then the
	 * virtual - setup_wired_tlb does things in the reverse order
	 * so we're guaranteed that if physical address is 0 the
	 * virtual address is also invalid.
	 */
#if (R4000 || R10000) && TLBKSLOTS == 1
	.set	noreorder
	/* Only need to restore one pair of TLB entries */
	PTR_L	k0, VPDA_CURUTHREAD(zero)
	PTR_L	k0, UT_EXCEPTION(k0)	# LDSLOT
	PTE_L	t0, U_USLKSTKLO_1(k0)	# LDSLOT
	PTE_L	k0, U_USLKSTKLO(k0)	# LDSLOT
	TLBLO_FIX_HWBITS(t0)
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(t0, C0_TLBLO_1)		# LDSLOT
	nop				# shut up assembler warnings
	DMFC0(t1, C0_TLBHI)		# get tlbpid
	PTR_L	t0, VPDA_CURUTHREAD(zero)	# LDSLOT
	PTR_L	t0, UT_EXCEPTION(t0)	# LDSLOT
	PTR_L	t0, U_USLKSTKHI(t0)	# LDSLOT
	andi	t1, TLBHI_PIDMASK	# LDSLOT
	or	t0, t1
	DMTC0(t0, C0_TLBHI)		# set virtual page number
	li	t0, UKSTKTLBINDEX	# base of wired entries
	mtc0	t0, C0_INX		# LDSLOT set index to wired entry
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO)		# set pfn and access bits
	# entry gets written just before RFE
	.set	reorder
#endif	/* (R4000 || R10000) && TLBKSLOTS == 1 */

#if TFP
	jal	tfi_restore
	RESTOREAREGS(k1)
	.set	noreorder
	lreg	s2,EF_CONFIG(k1)
	lreg	sp,EF_SP(k1)
	lreg	ra,EF_RA(k1)
	.set	noat
	lreg	AT,EF_AT(k1)
	dmtc0	s2,C0_CONFIG
	.set	reorder
	RESTOREFPREGS_TOUSER(s0)	# load FP regs, s0 temp
	.set	noreorder
	RESTORESREGS_SR_EPC(k1)		# restore "sregs", C0_SR, C0_EPC
	eret
#else /* !TFP */
#ifdef R4000_FAST_SYSCALL
	.set	noreorder
	jal	tfi_restore
	mfc0	s2, C0_SR		# BDSLOT s2 = current C0_SR
	RESTOREAREGS(k1)
	lreg	ra,EF_RA(k1)
	.set	noat
	lreg	AT,EF_AT(k1)		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# now have users $sp
	lreg	sp,EF_SP(k1)
	RESTORESREGS_SR_EPC(k1, s2)

#else	/* !R4000_FAST_SYSCALL */

	jal	tfi_restore
	RESTOREAREGS(k1)
	RESTORESREGS(k1)
	.set	noreorder
	lreg	ra,EF_RA(k1)
	NOP_0_1
	lreg	k0,EF_EPC(k1)
	.set	noat
	lreg	AT,EF_AT(k1)		# LDSLOT
	NOP_0_4

	DMTC0( k0, C0_EPC)
	lreg	sp,EF_SP(k1)

	lreg	k1,EF_SR(k1)
	nop
	MTC0(k1,C0_SR)
	NOP_0_4

#endif	/* !R4000_FAST_SYSCALL */
	
EXL_EXPORT(locore_exl_11)
#if TLBKSLOTS == 1
	nop				# LDSLOT
	c0	C0_WRITEI		# write entry
	NOP_0_4
#endif	/* TLBKSLOTS == 1 */
	
	ERET_PATCH(locore_eret_10)
EXL_EXPORT(elocore_exl_11)
	
#endif /* !TFP */
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	.set	at
	END(backtouser)
