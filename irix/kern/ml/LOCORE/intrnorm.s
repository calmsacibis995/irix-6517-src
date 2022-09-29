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

#ident	"$Revision: 1.38 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/* R10000_SPECULATION_WAR: $sp is not defined yet */
#ifdef USE_PTHREAD_RSA

	/* Entered from intrnorm with:	
	 *	k0 == UT_SHARENA (and pointer is valid)
	 */			
NESTED(intrnorm_rsasave,0,zero)
#ifdef TFP
	/* On TFP, we don't get an exception code for DBE/IBE, VCE, or
	 * FPE.  Instead we get an "interrupt" exception code with additional
	 * bits sets in the CAUSE register.
	 * We want to handle this case as a non-RSA interrupt.
	 */
	li	s1,CAUSE_BE|CAUSE_VCI|CAUSE_FPI
	and	s1,a3
	bne	s1,zero, no_nanothread
1:
#endif
	PTR_L   k0, UT_SHARENA(k1)
	beq	k0, zero, no_nanothread
	.set	noat
	lreg	AT, UT_PRDA(k1)
	beq	AT, zero, no_fast_nt
	lw      AT, PRDA_NID(AT)
	blez    AT, no_fast_nt		# if rsaid <= 0, then no NID
	.set	noreorder
	addi	AT, -NT_MAXNIDS
	bgtz	AT, no_fast_nt		# make sure <= max value
	addi	AT, NT_MAXNIDS		# restore NID value
	.set	reorder
	.set	at
	
	/*	k0 == UT_SHARENA
	 *	k1 == VPDA_CURUTHREAD
	 *	AT == PRDA_NID (range check already performed)
	 */
	
	/* convert NID to RSAID */

	.set    noat
	sll	AT, 1			/* convert NID to int16 offset */
	PTR_ADD	AT, k0
	lhu	AT, KUS_NIDTORSA(AT)	/* AT == RSAID */

	/* Now check rsa number against range of values actually allocated
	 * for the current uthread.
	 */
	.set	noreorder
	beq	AT, zero, no_fast_nt	/* RSAID zero invalid for saving */
	nada				/* BDSLOT - need original k1 */
	lhu	k1, UT_MAXRSAID(k1)
	sltu	k1, AT, k1		/* range check RSAID */
	beqz	k1, no_fast_nt
	PTR_L	k1, VPDA_CURUTHREAD(zero)	# BDSLOT - restore k1
	.set	reorder
	daddi   k0,k0,KUS_RSA		# k0 <- &(s_sharena->rsa[0])
	
	/* NOTE: Each rsa is 640 bytes long (5*128) so we multiply the UT_RSAID
	 * by 128 (shift 7 bits) then add it to base address 5 times.
	 */
	sll     AT,AT,7			# AT == rsaid * 128
	daddu	k0,AT
	sll	AT,AT,2			# AT == rsaid * 4*128
	daddu   k0,AT			# k0 contains s_sharena->rsa[rsaid]

	/* R10000_SPECULATION_WAR: store to k1 is dependent on k1 load */
	AUTO_CACHE_BARRIERS_DISABLE

	/* set flag so swtch() knows this context is runable if saved
	 * in an RSA.
	 */
	li	AT, 1
	sb	AT, UT_RSA_RUNABLE(k1)
	.set    at
		
	move	k1,k0			# k1 = s_sharena->rsa[rsaid]
	/* NOTE: We will end up with a "split-eframe" with the kernel
	 * modifable registers in the UT_EXCEPTION eframe and the
	 * user's registers in the RSA.  Since this is an interrupt on
	 * a uthread, the interrupt code shouldn't be referencing the
	 * user's registers anyway.
	 */

	/* registers normally saved by intrnorm before invoking ecommon */
	sreg	sp,RSA_SP(k1)		# build initial stack
	PTR_L	sp,VPDA_LINTSTACK(zero)
	AUTO_CACHE_BARRIERS_ENABLE	# we not have $sp set
	sreg	gp,RSA_GP(k1)
	LA	gp,_gp
	
	/* Now we explicitly save all of the user modifiable registers
	 * into the RSA instead of the ut_exception eframe.
	 * NOTE: these registers normally saved by ecommon entry.
	 */
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)
	sreg	AT,RSA_AT(k1)
	.set    at
	sreg	a0,RSA_A0(k1)
	sreg	a1,RSA_A1(k1)
	sreg	a2,RSA_A2(k1)
	sreg	a3,RSA_A3(k1)
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	sreg	a4,RSA_A4(k1)
	sreg	a5,RSA_A5(k1)
	sreg	a6,RSA_A6(k1)
	sreg	a7,RSA_A7(k1)
#endif
	sreg	s0,RSA_S0(k1)
	sreg	s1,RSA_S1(k1)
	sreg	s2,RSA_S2(k1)
	sreg	s3,RSA_S3(k1)
	sreg	s4,RSA_S4(k1)
	sreg	s5,RSA_S5(k1)
	sreg	s6,RSA_S6(k1)
	sreg	s7,RSA_S7(k1)

	sreg	ra,RSA_RA(k1)

	AUTO_CACHE_BARRIERS_DISABLE
	ORD_CACHE_BARRIER_AT(RSA_V0,k1)
	sreg	v0,RSA_V0(k1)
	sreg	v1,RSA_V1(k1)
	sreg	t0,RSA_T0(k1)
	mflo	t0
	sreg	t1,RSA_T1(k1)
	mfhi	t1
	sreg	t2,RSA_T2(k1)
	sreg	t3,RSA_T3(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	sreg	t4,RSA_T4(k1)
	sreg	t5,RSA_T5(k1)
	sreg	t6,RSA_T6(k1)
	sreg	t7,RSA_T7(k1)
#endif
	sreg	t8,RSA_T8(k1)
	sreg	t9,RSA_T9(k1)
	sreg	fp,RSA_FP(k1)
	sreg	t0,RSA_MDLO(k1)
	sreg	t1,RSA_MDHI(k1)
	
	.set	noreorder
	DMFC0(a0,C0_EPC)
	.set	reorder
	sreg	a0,RSA_EPC(k1)

	/*
	 * Finally point back to UT_EXCEPTION eframe for the non-user
	 * modifable register saves.
	 */
	PTR_L	k1, VPDA_CURUTHREAD(zero)
	PTR_L	k1, UT_EXCEPTION(k1)
	PTR_ADDIU k1, U_EFRAME		# address of exception frame
	li	k0,AS_INT_RUN		# user to kernel intr timer
	j       ecommon_allsaved

	.set reorder
	AUTO_CACHE_BARRIERS_ENABLE
	END(intrnorm_rsasave)
#endif /* USE_PTHREAD_RSA */
		
VECTOR(intrnorm, M_EXCEPT)
	AUTO_CACHE_BARRIERS_DISABLE	# $sp is not defined yet

	
#if defined (R10000) && defined (R10000_MFHI_WAR)
	.set	noreorder
	.set	noat
	/*
	 * Check if we have to do the workaround for the R10k mult/div bug.
	 * Since the bug can happen only if the fp is in use, skip the war
	 * if this is not the fp owner or the war is not enabled.
	 */
	lb	k1, VPDA_R10KWAR_BITS(zero)
	xori	k1, R10K_MFHI_WAR_DISABLE
	beqz	k1, intr_start			# war disabled:	 get out
 	 PTR_L	k1, VPDA_FPOWNER(zero)		# BDSLOT
	beqz	k1, intr_start			# no fp owner, get out
	 PTR_L	k0, VPDA_CURUTHREAD(zero)	# BDSLOT
	bne	k1, k0, intr_start		# fpowner, do war
	 nop					# BDSLOT
	DMFC0(k1, C0_EPC)
	bgez	k1, do_mfhi_war		# not user level epc, so we are done.
	 nop
	.set	reorder
	.set	at
#endif /* R10000 && R10000_MFHI_WAR*/
intr_start:		
	
#if EVEREST || SN
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_INTRNORM, uli_tstamps)
#endif
	/*
	 * A Non-Preemptive CPU isn't supposed to take scheduler clock
	 * interrupts.  Ideally, we should mask off these interrupts using the
	 * SRB_SCHEDCLK bit, but this is complicated and error-prone because of
	 * all the spots in the kernel which turn on that bit (like spl0) and
	 * which expect the bit to be on (like issplhi).  The next best
	 * approach is to recognize when we've taken such an interrupt on a
	 * Non-Preemptive CPU, and to fast-path a quick exit from the kernel.
	 */
	.set	noreorder
	lw	k0,VPDA_PFLAGS(zero)
	DMFC0(k1,C0_CAUSE)
#ifndef TFP
	andi	k1,SRB_SCHEDCLK
	beq	k1,zero,1f		# ...and if this is a sched clock intr
#endif
#if R10000
	/*
	 * For the R10000 the interrupt could be due to a 
	 * performance counter overflowing. Need to check that
	 * before the sched clock intr.
	 */
	MFPC(k1,PRFCNT0)
	bltz	k1,1f			# counter 0 overflow
	nop
	MFPC(k1,PRFCNT1)
	bltz	k1,1f			# counter 1 overflow
	nop
	/*
	 * For the R12000 we also have counters 2 and 3 to check.
	 */
	MFC0(k1,C0_PRID)
	andi	k1,C0_IMPMASK
	subu	k1,(C0_IMP_R12000 << C0_IMPSHIFT)
	bnez	k1,not_r12k
	nop
	MFPC(k1,PRFCNT2)
	bltz	k1,1f			# counter 2 overflow
	nop
	MFPC(k1,PRFCNT3)
	bltz	k1,1f			# counter 3 overflow
	nop
not_r12k:
#endif	/* R10000 */
	andi	k0,PDAF_NONPREEMPTIVE	# if CPU is Non-Preemptive...
	beq	k0,zero,1f
#if TFP
	li	k0,SRB_SCHEDCLK		# too big for "andi" on TFP
	and	k1,k0
	beq	k1,zero,1f		# ...and if this is a sched clock intr
	NOP_NADA			# BDSLOT
	DMTC0(zero,C0_COUNT)		#    reset interrupt by clearing COUNT
#else
	li	k0,0xf8000000		#    then reset that interrupt
	DMTC0(k0,C0_COMPARE)
	NOP_0_1			
#endif	/* TFP */
	eret
	.set	reorder
1:
#endif	/* EVEREST || SN */
#if EVEREST && (R4000 || R10000)
	/* Need to see if the SR_IE bit is still set.  The bit may have just
	 * been cleared in the C0_SR but has not yet taken affect.  This is
	 * critical with the addition of kernel preemption since on interrupt
	 * exit we perform a preemption check based upon isspl0() and we may
	 * miss an interrupt thread preemption.
	 */
	.set	noreorder
	MFC0(k0, C0_SR)
	.set	reorder
	lbu	k1,VPDA_CELSHADOW(zero)
	andi	k0,SR_IE
	beq	k0,zero,int_eret	# return immediately if IE now clear
	LA	k0,EV_CEL
	sd	k1,(k0)			# Make sure value was written
	.set	noreorder
	/* For some unknown reason, if we have DELAY_CEL_WRITES defined
	 * we need to load the CEL register twice (perhaps because we just
	 * wrote the value above for the first time) and then have a
	 * one instruction delay between the second read and checking the
	 * CAUSE register.
	 * NOTE: Apparently the SCHEDCLK bit in the CAUSE register is visible
	 * even when the bit is blocked (i.e. clear) in the C0_SR, though
	 * it won't cause an interrupt.  Since evintr will check anyway,
	 * let's perform the check here and avoid an uneeded long code path.
	 * We should be able to remove the check from evintr if this works OK.
	 */
#ifdef DELAY_CEL_WRITES	
	sb	k1,VPDA_CELHW(zero)
	ld	k1,(k0)			# wait for new value to arrive
#endif	
	ld	k1,(k0)			# wait for new value to arrive
	/* Following check is identical to the one performed in
	 * intr() for determining if interrupt is valid.
	 */
	MFC0(k1, C0_SR)
	DMFC0(k0, C0_CAUSE)
	andi	k1, SR_IMASK
	and	k1,k0
	bne	k1, zero, 1f
	nop
int_eret:		
	eret
	.set	reorder
1:
#endif /* EVEREST && (R4000 || R10000) */
#ifdef SN0
	/* This should probably just be !EVEREST but safer to keep it this way
	 * for now.
	 */
	/* Need to see if the SR_IE bit is still set.  The bit may have just
	 * been cleared in the C0_SR but has not yet taken affect.  This is
	 * critical with the addition of kernel preemption since on interrupt
	 * exit we perform a preemption check based upon isspl0() and we may
	 * miss an interrupt thread preemption.
	 */
	.set	noreorder
	MFC0(k1, C0_SR)
	andi	k0,k1,SR_IE
	beq	k0,zero,int_eret	# return immediately if IE now clear
	andi	k1, SR_IMASK		# BDSLOT 
	/* Following check is identical to the one performed in
	 * intr() for determining if interrupt is valid.
	 */
	DMFC0(k0, C0_CAUSE)
	and	k1,k0
	bne	k1, zero, 1f
	nop
int_eret:		
	eret
	.set	reorder
1:
#endif /* SN0 */		
	/*
	 * got an interrupt - put on ICS
	 * If (on ICS) 
	 *	save state on ICS & add new frame
	 * else if (on KERNEL stack)
	 *	save state on kernel stack & twiddle k_depth
	 * else if in USERMODE
	 *	save state in u
	 * else if on IDL stack
	 *	save state on idle stack & twiddle p_idlstkdepth
	 */
#ifdef ULI
	PTR_L	k0, VPDA_CURULI(zero)
	bne	k0, zero, intuliframe
#endif
	lb	k0,VPDA_KSTACK(zero)
	.set	noreorder
	addi	k0,-PDA_CURINTSTK
	beq	k0,zero,intnextframe
	addi	k0,-PDA_CURKERSTK+PDA_CURINTSTK	# BDSLOT
	.set	reorder
	/*
	 * For MCCHIP systems
	 *
	 * set the bus burst and delay values to give CPU priority
	 * over dma so that we can handle the interrupt fast.  This
	 * can give up to a 10x interrupt service time increase.
	 */
#ifdef MCCHIP
#ifdef IP20			/* rev A MC only on IP20 */
	lw	k1,mc_rev_level
	beq	k1,zero,1f
#endif	/* IP20 */
	.set	noat
	CLI	AT,PHYS_TO_COMPATK1(CPUCTRL0)	# MC base
	lhu	k1,i_dma_burst
	/* R10000_SPECULATION_WAR: these stores are ok, dependent and to IO */
	sw	k1,(LB_TIME-CPUCTRL0)(AT)	# store LB_TIME
	lhu	k1,i_dma_delay
	sw	k1,(CPU_TIME-CPUCTRL0)(AT)	# store CPU_TIME
	lw	zero,0(AT)			# wbflushm
	.set	at
1:
#endif
	.set	noreorder
	beq	k0,zero,intkerframe	# go if PDA_CURKERSTK
	addi	k0,-PDA_CURIDLSTK+PDA_CURKERSTK	# BDSLOT
	beq	k0,zero,intidlframe	# go if PDA_CURIDLSTK
	li	k0,PDA_CURINTSTK	# BDSLOT (for use below)

	/* from USERMODE */
#ifdef STKDEBUG
	MFC0(k0,C0_SR)
	NOP_1_1
	and	k0,SR_PREVMODE
	bne	k0,zero,1f
	li	k0,PDA_CURINTSTK	# BDSLOT (for use below)
	j	stkscr2			# from kernel mode
	nop				# BDSLOT
1:
#endif
	.set	noat
	/*
	 * Save the instruction at epc in case this is a floating point
	 * interrupt.  We need to do this while the tlb still has the
	 * epc mapped.  If the interrupt happened in a branch delay, then
	 * we need to save the next instruction too.
	 */

	sb	k0,VPDA_KSTACK(zero)	# now on interrupt stack
	
	/*
	 * wire in process kernel stack
	 */
	SVSLOTS
#if R4000 || R10000
#if TLBKSLOTS == 1
	# KERNELSTACK alone in tlb slot pair, with the former at the
	# even page address so load it into EntryHi (as VPN2)
	LI	k0, KSTACKPAGE

	# PTE_L	k1, VPDA_UPGLO(zero)
	LI	k1,TLBLO_G		# Just the G bit for the 2nd mapping
	DMFC0(AT, C0_TLBHI)		# LDSLOT save virtual page number/pid
	/* TLBLO_FIX_HWBITS(k1) */
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(k1, C0_TLBLO_1)		# LDSLOT set pfn and access bits
	DMTC0(k0, C0_TLBHI)		# set virtual page number
	li	k0, UKSTKTLBINDEX	# base of wired entries

	# and	k1, TLBLO_V		# NOP SLOT check valid bit 
	# beq	k1, zero, _baduk	# panic if not valid

	/* now do kernel stack */
	PTE_L	k1, VPDA_UKSTKLO(zero)
	mtc0	k0, C0_INX		# set index to wired entry
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(k1, C0_TLBLO)		# LDSLOT set pfn and access bits
	and	k1, TLBLO_V		# NOP SLOT check valid bit
	c0	C0_WRITEI		# write entry
	beq	k1, zero, _baduk	# panic if not valid
	DMTC0(AT, C0_TLBHI)		# BDSLOT restore pid
#endif	/* TLBKSLOTS == 1 */
#endif	/* R4000 || R10000 */

	.set	at
	.set	reorder
	PTR_L	k1, VPDA_CURUTHREAD(zero)
#ifdef USE_PTHREAD_RSA
	/* 
	 * Check if process is nanothreaded and if register saving from locore
	 * is enabled.  If so, we use a "split eframe" and save user
	 * registers directly into the sharena structure.
	 */
	.set	noreorder
	lb	k0, UT_RSA_LOCORE(k1)
	bne	k0, zero, intrnorm_rsasave

	/* This entry point is used when its' possible we're a nanothread
	 * process (have a sharena, etc) and that we can't or don't want to
	 * save registers directly into the RSA. We indicate this to swtch()
	 * code by setting "2" into ut_rsa_runable.
	 * NOTE: swtch() will actually check for the existance of a ut_sharena
	 * and valid nid values.
	 */
no_fast_nt:		
	li	k0, 2			# BDSLOT
	sb	k0, UT_RSA_RUNABLE(k1)

	/* This entry point could be placed at "no_fast_nt" but it does avoid
	 * a couple of instruction if we know we don't want to be considered
	 * as saveable into the RSA (i.e. no sharena or TFP FP interrupt).
	 */
	.set	reorder
no_nanothread:
	
#endif /* USE_PTHREAD_RSA */
	
	PTR_L	k1, UT_EXCEPTION(k1)
	PTR_ADDIU k1, U_EFRAME		# address of exception frame
	
nanothread:	
	/* R10000_SPECULATION_WAR: store to k1 is dependent on k1 load */
	sreg	sp,EF_SP(k1)		# build initial stack
	PTR_L	sp,VPDA_LINTSTACK(zero)
	AUTO_CACHE_BARRIERS_ENABLE	# we not have $sp set
	sreg	gp,EF_GP(k1)
	LA	gp,_gp
	li	k0,AS_INT_RUN		# user to kernel intr timer
	j	ecommon

EXPORT(_baduk)
	AUTO_CACHE_BARRIERS_DISABLE	# called before $sp is set
	/*
	 * Bad access
	 */
	/* make sure a good frame - this works except in VERY early cases */
	PTR_L	sp,VPDA_LBOOTSTACK(zero)
	ASMASSFAIL("invalid kstk");
	/* NOTREACHED */
	AUTO_CACHE_BARRIERS_ENABLE

	/* from kernel mode - mark save state in k_depth */
intkerframe:
	AUTO_CACHE_BARRIERS_DISABLE	# called before $sp is set
#ifdef STKDEBUG
	.set	noreorder
	mfc0	k0,C0_SR
	NOP_1_0
	.set	reorder
	and	k0,SR_PREVMODE
	beq	k0,zero,1f
	j	stkscr3			# from user mode
1:
#endif
	
	li	k1,PDA_CURINTSTK
	sb	k1,VPDA_KSTACK(zero)	# now on interrupt stack

	PTR_SUBU k1,sp,EF_SIZE		# k1 has exception frame ptr
	ori	k1,7			# ori/xori is fastest way to make sure
	xori	k1,7			# the stack is dw aligned (for TFP)
	/* R10000_SPECULATION_WAR: store to k1 is dependent on above math */
	sreg	sp,EF_SP(k1)		# save old sp
	PTR_L	sp,VPDA_LINTSTACK(zero)	# sp now on interrupt stack
	AUTO_CACHE_BARRIERS_ENABLE	# we now have a stack
	PTR_L	k0,VPDA_CURKTHREAD(zero)
	PTR_S	k1,K_EFRAME(k0)
	li	k0,AS_INT_RUN		# intr from kernel mode
	j	ecommon


	/* from kernel mode/IDL stack - mark save state in p_idlstkdepth */
intidlframe:
	AUTO_CACHE_BARRIERS_DISABLE	# called before $sp is set
#ifdef STKDEBUG
	.set	noreorder
	mfc0	k0,C0_SR
	NOP_1_0
	.set	reorder
	and	k0,SR_PREVMODE
	beq	k0,zero,1f
	j	stkscr3			# from user mode
1:
#endif
	li	k1,PDA_CURINTSTK
	sb	k1,VPDA_KSTACK(zero)	# now on interrupt stack

	PTR_L	k1,VPDA_LINTSTACK(zero)
	sreg	sp,EF_SP(k1)		# save old sp
	move	sp, k1			# sp now on interrupt stack
	AUTO_CACHE_BARRIERS_ENABLE	# we now have a stack
	li	k0, 1
	sb	k0,VPDA_IDLSTKDEPTH(zero)	# nesting
#ifdef IDLESTACK_FASTINT
	lb	k0, VPDA_INTR_RESUMEIDLE(zero)
	beq	k0,zero,1f
#ifdef TFP
	.set	noreorder
	DMFC0(s0,C0_SR)
	DMFC0(s1,C0_CONFIG)
	DMFC0(s2,C0_CAUSE)
	DMFC0(s3,C0_EPC)
	.set	reorder
#endif /* TFP */		
	li	k0,AS_INT_RUN		# intr from idle
	j	ecommon_allsaved
1:	
#endif /* !IDLESTACK_FASTINT */	
	li	k0,AS_INT_RUN		# intr from idle
	j	ecommon


#ifdef ULI
	/* handle a nested interrupt while in ULI mode */
	.set	reorder
intuliframe:
	lb	k1, VPDA_KSTACK(zero)
#ifdef DEBUG
	bne	k1, PDA_CURINTSTK, 1f
	INCREMENT(real_nested_int_during_uli, k0, AT)
1:
#endif
	# if we're already on intstk, this is a doubly nested interrupt
	# on top of a ULI, so handle it like any other nested intr
	beq	k1, PDA_CURINTSTK, intnextframe

#if DEBUG
	beq	k1, PDA_CURULISTK, 1f
	move	a2, k1
	PANIC("bad kstackflag during ULI: %d\n")
1:
	INCREMENT(nested_int_during_uli, AT, k1)
#endif
	# reload saved intstack
	PTR_L	k0, VPDA_CURULI(zero)
#ifdef TFP
	# restore kernel's config reg
	ld	k1, ULI_SAVED_CONFIG(k0)
	.set	noreorder
	dmtc0	k1, C0_CONFIG
	.set	reorder
#endif
	# NOTE: don't put any cop0 instructions here near the
	# above dmtc0
	PTR_L	k1, ULI_INTSTK(k0)

	PTR_SUBU k1, EF_SIZE
	ori	k1, 7
	xori	k1, 7

#ifdef OVFLWDEBUG
	/* make a stab at detecting interrupt stack under/overflow
	 * do this BEFORE accessing memory
	 */
	PTR_L	k0, VPDA_INTSTACK(zero)
	PTR_ADDIU k0, k0, 100	# some slop
	bgtu	k1, k0, 1f

	j	stkovflw
1:
	PTR_L	k0, VPDA_LINTSTACK(zero)
	bleu	k1, k0, 1f

	j	stkundflw
1:
#endif OVFLWDEBUG

	sreg	sp, EF_SP(k1)
	move	sp, k1

	sreg	gp,EF_GP(k1)
	LA	gp,_gp

	li	k0, PDA_CURINTSTK
	sb	k0, VPDA_KSTACK(zero)	# back on interrupt stack

	# We'll need more registers to do the tlb switch, so
	# do full register save crud now and bypass it in ecommon later
#ifdef TFP
	.set	noreorder
	SAVESREGS_GETCOP0(k1)		# save sregs and get COP0 regs
	.set	reorder
	/* NOTE!!! we must not use s[0-3] between here and the
	 * jump to ecommon_allsaved!!
	 */
#endif
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)	# recall saved AT
	sreg	AT,EF_AT(k1)
	.set	at
	SAVEAREGS(k1)
#ifndef TFP
	SAVESREGS(k1)
#endif
	sreg	ra,EF_RA(k1)
	jal	tfi_save		# save tregs, v0,v1, mdlo,mdhi

	# restore curproc to its original state before the ULI
	PTR_L	t1, VPDA_CURULI(zero)
	PTR_L	t0, ULI_SAVED_CURUTHREAD(t1)
	PTR_S	t0, VPDA_CURUTHREAD(zero)

	lh	a0, ULI_SAVED_ASIDS(t1)
	jal	uli_setasids
	j	ecommon_allsaved
#endif /* ULI */

	/* nested interrupt */
intnextframe:
	AUTO_CACHE_BARRIERS_DISABLE	# called before $sp is set
#ifdef OVFLWDEBUG
	/* make a stab at detecting interrupt stack under/overflow
	 * do this BEFORE accessing memory
	 */
	PTR_L	k0, VPDA_INTSTACK(zero)
	PTR_ADDIU k0, k0, 100+EF_SIZE	# some slop
	bgtu	sp, k0, 1f

	j	stkovflw
1:
	PTR_L	k0, VPDA_LINTSTACK(zero)
	bleu	sp, k0, 1f

	j	stkundflw
1:
#endif /* OVFLWDEBUG */

	# save last frame for debugging
	PTR_SUBU k1,sp,EF_SIZE
	ori	k1,7			# ori/xori is fastest way to make sure
	xori	k1,7			# the stack is dw aligned (for TFP)
	sreg	sp,EF_SP(k1)		# save old sp
	move	sp,k1			# k1==sp has exception frame pointer
	AUTO_CACHE_BARRIERS_ENABLE	# we have a Stack now
	li	k0,AS_DONT_SWITCH	# nested intr: don't change timer
	j	ecommon

	.data
	lmsg:	.asciiz "intrnorm.s"

/* fastick_callback_required_flags: see label prof_check
 * for explanation.
 */
	.globl	fastick_callback_required_flags
	.sdata	
fastick_callback_required_flags:
	.word	0

	.text
#if defined (R10000) && defined (R10000_MFHI_WAR)

/*
 * The following is a workaround for a R10k bug. (pv # 516598)
 * If the interrupt occurs on a integer multiply/divide, there are chances
 * that the mfhi register is not loaded correctly, and the mult/div has to be
 * re-executed.
 * If the mult/divide is in a branch delay slot, then the branch may not
 * be executed correctly (along with the mfhi problem). So, we need to
 * reexecute both the branch and the mult/div operation.
 *
 * The following code is the workaround. If the epc is in user land, and
 * epc-4 and epc-8 is on the same page, we inspect them and do the WAR.
 * If not, we check if the previous page is already in the tlb and then
 * look at those instructions.
 * The workaround will not kick in if the EPC or EPC-4 are the first 
 * instructions on the page AND the previous page is not already in the tlb.
 * Without being sure that accesses to the previous page are valid, its not
 * safe to do the access.
 *
 * In some cases, the access to instructions around EPC could take a tlbmiss
 * and these do not affect the workaround or the proper handling of the
 * interrupt.
 * Since all the checks need to be done with just registers k0, k1 and
 * AT, and has to be done in assembly, the workaround is a bit complex.
 */
	.set	noreorder
	.set	noat

do_mfhi_war:
	sreg	AT, VPDA_ATSAVE(zero)	#BDSLOT

	.set	at
	PTR_ADDIU	 k1, -4		# k1 = epc - 4
	and	k1, TLBHI_VPNMASK	
	.set	noat

	DMFC0(AT, C0_TLBHI)		# save tlbhi
	and	k0, AT, TLBHI_PIDMASK	# k0 = pid
	or	k0, k1			# k0 = pid | tlbhi vpn
	DMTC0(k0, C0_TLBHI)		# set tlbhi for a probe
	c0	C0_PROBE		
	mfc0	k0, C0_INX		# k0 = tlb index of match
	bltz	k0, war_end		# page not in tlb? we are done.
	 nop
	c0	C0_READI		# read the tlb at set index
	/*
	 * Is the tlbpage valid?
	 */
	MFC0(k0, C0_PGMASK)		# get pagemask
	INT_SRL	k0, 1			
	ori	k0, MIN_POFFMASK	#no need for at.
	PTR_ADDU k0, 1
	DMFC0(k1, C0_EPC)
	PTR_ADDIU k1, -4
	
	and	k0, k1
	
	bnez	k0, odd_page
	 nop
	DMFC0(k0,C0_TLBLO)		# Even:	 load tlblo
	b	check_valid_tlb
	 nop

odd_page:
	DMFC0(k0, C0_TLBLO_1)

check_valid_tlb:

	#restore the PAGEMASK to 16k
	li	k1, TLBPGMASK_MASK
	MTC0(k1, C0_PGMASK)
	
	andi	k0, TLBLO_V
	beqz	k0, war_end		# tlb entry not valid:	 done
	 nop

	DMTC0(AT, C0_TLBHI)	# restore old tlbhi.. not  necessary?
	DMFC0(k1, C0_EPC)	# Reload k1 with the EPC.

	lw	k0, -4(k1)	#k0 = contents of epc-4
	
	 /*
	  * check if the instruction is one of the integer mult/divs.
	  * for this, the major opcode has to be SPECIAL (0).
	  * The function fields have to be between 0x18 and 0x1f (inclusive)
	  */
	srl	AT, k0, 26	#At = major opcode of instruction.
	bnez	AT, war_end	#If opcode != special, we are done.
	 andi	AT, k0, 0x38	#BDSLOT:  get the upper 3 bits of func code.
	 /*
	  * If the upper 3 bits of the 6 bit function code are set to binary
	  * 011, we are dealing with an affected instruction.
	  */
	xori	AT, 0x18	
	bnez	AT, war_end	# not an integer mult/div. Get out.
	 nop

	/*
	 * we now check if we have to do the workaround. execute the
	 * instruction in the patch buffer and compare the mflo's. If they
	 * match, we assume that this instruction was indeed the one that
	 * was executing prior to the interrupt and we have to do the WAR.
	 * We do the war in most cases even if the MFHI's are equal. This
	 * is to take care of the mult/div's which are in the branch delay
	 * slot. If those encounter the bug, the branch is never taken.
	 * There could be cases where the MFHI's are indeed equal but we did
         * encounter the bug. so we do the workaround unless the instruction
	 * prior to the mult/div is a jal or jalr. Since re-exceuting jal/jalr
	 * is not always safe, we do the WAR in those cases ONLY if the mfhi's
	 * are not equal (meaning, we did hit the bug)
	 */

	PTR_L	k1, VPDA_MFHI_PATCH_BUF(zero)
	beqz	k1, war_end
	 nop
	sw	k0, 4(k1)
	cache	CACH_PD | C_HWBINV, 0(k1)
	cache	CACH_PI | C_HINV, 0(k1)

	DMFC0(zero, C0_EPC)	# force previous cache op to complete.
				# and flush the instruction pipeline.

	lreg	AT, VPDA_ATSAVE(zero)
	mfhi	k0
	sreg	k0, VPDA_MFHI_REG(zero); 
	mflo	k0
	sreg	k0, VPDA_MFLO_REG(zero);

	jalr	k0, k1
	 nop
	
	lreg	k0, VPDA_MFLO_REG(zero)
	mflo	k1
	mtlo	k0

	beq	k0, k1, init_war
	 lreg	k0, VPDA_MFHI_REG(zero)
	/*
	 * The mflo's are not equal, so the mult/div was not the instruction
	 * we executed previous to the intr. This can happen if we got to the
	 * epc by branching from elsewhere.
	 */
	mthi	k0
	j	war_end
	 nop

init_war:
	mfhi	k1
	bne	k0, k1, dowar
	 li	AT, 1		# the mfhi's are not equal
	li	AT, 0

dowar:		
	/*
	 * VPDA_MFHI_REG will have 0 if the mfhi's are equal and 1 if not.
	 */
	sreg	AT, VPDA_MFHI_REG(zero)
	mthi	k0	

check_branch:		
	DMFC0(k1, C0_EPC)	# Reload k1 with the EPC.	
	PTR_ADDIU	k0, k1, -4
	andi	k0, (_PAGESZ-1)
	bnez	k0, still_in_tlb	# epc - 4 and epc - 8 are in same page
	 nop
	
	.set	at
	PTR_ADDIU	 k1, -8
	and	k1, TLBHI_VPNMASK
	.set	noat

	DMFC0(AT, C0_TLBHI)		# save tlbhi
	and	k0, AT,  TLBHI_PIDMASK	# k0 = pid
	or	k0, k1			# k0 = pid | tlbhi vpn
	DMTC0(k0, C0_TLBHI)		# set tlbhi for a probe
	c0	C0_PROBE		
	mfc0	k0, C0_INX		# k0 = tlb index of match

	bltzl	k0, war_done		# page not in tlb? we are done.
	 li	AT, 4
	c0	C0_READI		# read the tlb at set index
	/*
	 * Is the tlbpage valid?
	 */
	MFC0(k0, C0_PGMASK)
	INT_SRL	k0, 1
	ori	k0, MIN_POFFMASK	#no need for at.
	PTR_ADDU k0, 1
	DMFC0(k1, C0_EPC)
	PTR_ADDIU k1, -8
	and	k0, k1
	bnez	k0, odd_page_branch
	 nop
	DMFC0(k0,C0_TLBLO)		# Even:	 load tlblo	
	b	check_valid_branch
	 nop
odd_page_branch:	
	DMFC0(k0, C0_TLBLO_1)
check_valid_branch:

	#restore the PAGEMASK to 16k
	li	k1, TLBPGMASK_MASK
	MTC0(k1, C0_PGMASK)

	andi	k0, TLBLO_V
	beqzl	k0, war_done		# tlb not valid: just do mult war
	 li	AT, 4

	DMTC0(AT, C0_TLBHI)	# restore old tlbhi.. not  necessary?
	DMFC0(k1, C0_EPC)	# Reload k1 with the EPC.

still_in_tlb:

	lw	k0, -8(k1)
	
branch_check:

	 /*
	  * At this point, we know we are dealing with an integer mult/div
	  * at epc-4. We have to check if epc-8 is one the branch 
	  * instructions.
	  * These are instructions with opcode 1 through 7, opcode
	  * 0x14 through 0x17, and some instructions with opcode 0.
	  */
br_special:		
	srl	AT, k0, 26	# At = opcode of instruction at epc-8
	bnez	AT, br_regimm	# opcode not zero, check for other cases.
	 nop			# BDSLOT
	 /*
	  * If the opcode is 0, there are 2 branch type instructions
	  * possible. jalr with function code 8, and jr with func code 9.
	  */
	andi	k0, 0x3f	# k0 = instruction function code.
	xori	k0, 0x8
	beqz	k0, war_done	# we need to subtract 8 from epc
	 li	AT, 8
	xori	k0, 0x1		#check for jalr
	bnez	k0, war_done    #epc-8 is not branch type inst. subtract 4.
	 li	AT, 4

	/*
	 * TAKE CARE OF JALR (which is not always safe to redo)
	 * If the mfhi's were equal, its very likely that the previous mult
	 * and the jalr did execute correctly. So skip the workaround.
	 */
	lreg	AT, VPDA_MFHI_REG(zero)
	bnez	AT, war_end		# Do not do the war.
	 nop
	b	war_done
	 li	AT, 8

	
	 /*
	  * epc-8 does not have opcode 0. Check if the opcode matches the
	  * other branch type instructions.
	  */
br_regimm:	
	xori	AT, 1		#check for opcode of 1
	bnez	AT, check_jal
	 srl	k0, 16		# k0 now has 16 upper bits of instruction.
	andi	k0, 0x8		# if bit 3 is set, this is a trap not branch.
	beqz	k0, war_done	# bit 3 not set, this is a branch.
	 li	AT, 8
	b	war_done
	 li	AT, 4
	 /*
	  * epc-8 does not have opcode 0 or 1. Check if the opcode matches the
	  * other branch type instructions.
	  */
check_jal:
	li	k0, 2		#jal is 3. we have xor'd the lower bit
	bne	k0, AT, not_jal
	 nop
	/*
	 * TAKE CARE OF JAL (which is not always safe to redo)
	 * If the mfhi's were equal, its very likely that the previous mult
	 * and the jalr did execute correctly. So skip the workaround.
	 */
	lreg	AT, VPDA_MFHI_REG(zero)
	bnez	AT, war_end
	 nop
	b	war_done
	 li	AT, 8
	
not_jal:		
	sltiu	k0, AT, 8	# if opcode less than 8, this is a branch.
	beqz	k0, brlikely	# opcode >= 8, so check for other conditions.
	 nop			# BDSLOT
	b	war_done	# opcode is a branch.
	 li	AT, 8		# BDSLOT

brlikely:	
	 /*
	  * opcode is greater than 8. Check for the other branch types.
	  * opcode 0x14 through 0x17
	  */
	ori	AT, 0x3		
	xori	k0, AT, 0x17	
	beqz	k0, war_done	# if k0 = 0, this is a branch.
	 li	AT, 8		# BDSLOT: subtract value is 8.

brcop:		
	/*
	 * Now check if it is any of the cop* branches 
	 */
	lw	k0, -8(k1)	# reload epc-8
	srl	k0, 21		# take 8 upper bits of instruction.
	xori	k0, 0x228	# if this is a cop1 and BC bit set
	beqz	k0, war_done	# we have a branch.
	 li	AT, 8
	li	AT, 4		# not a branch:	 subtract value is 4.

war_done:
	/*
	 * At this point we have identified that we need the workaround.
	 * register AT has the amount we need to reduce the EPC by.
	 */
	
	DMFC0(k1, C0_EPC)
	PTR_SUBU  k1, AT
	DMTC0(k1, C0_EPC)
	 /*
	  * update a counter indicating we are doing the workaround.
	  */
	lw	k0, VPDA_MFHI_CNT(zero)
	addiu	k0, 1
	sw	k0, VPDA_MFHI_CNT(zero)
	
	xori	AT, 8		# if AT is 8, update another counter 
	bnez	AT, war_end
	 lw	AT, VPDA_MFHI_BRCNT(zero)
	addiu	AT, 1
	b	war_end
	 sw	AT, VPDA_MFHI_BRCNT(zero)

war_end:
	j	intr_start
	 lreg	AT, VPDA_ATSAVE(zero)
	.set	reorder
	.set	at	
#endif	/* R10000 && R10000_MFHI_WAR*/

	END(intrnorm)



