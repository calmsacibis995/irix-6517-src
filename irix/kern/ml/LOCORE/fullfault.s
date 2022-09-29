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

#ident	"$Revision: 1.21 $"

#include "ml/ml.h"

LEAF(longway)
EXL_EXPORT(locore_exl_13)	
	/*
	 * We now know that its a trap - not a syscall or interrupt
	 * So: stacks:
	 *	if (on ICS)
	 *		add frame to ICS (only breakpoints etc.)
	 *		save state on ICS
	 *	else if (on IDLE stack)
	 *		PANIC
	 *	else if (from USERMODE)
	 *		create frame on kernel stack
	 *		save state in uarea
	 *	else
	 *		add new frame to kernel stack
	 *		save state on kerstack
 	 *
	 * R10000_SPECULATION_WAR: $sp is not current defined
	 */
	AUTO_CACHE_BARRIERS_DISABLE
	/* we have already saved the AT register */
	lb	k0,VPDA_KSTACK(zero)	# see where we came from
	.set	noreorder
	beq	k0,zero,keruserframe	# from user stack
	addi	k0,-PDA_CURKERSTK	# BDSLOT
	beq	k0,zero,kernextframe	# already on kernel stack
#ifdef ULI
	addi	k0, -PDA_CURULISTK+PDA_CURKERSTK
	bnel	k0, zero, 1f
	addi	k0, -PDA_CURINTSTK+PDA_CURULISTK	# BDSLOT

	j	keruliframe
	nop						# BDSLOT

1:
#else
	addi	k0,-PDA_CURINTSTK+PDA_CURKERSTK		# BDSLOT
#endif
	bne	k0,zero,keridleframe	# not an interrupt
	.set	reorder
	/* was on interrupt stack - like kernel breakpoints 
	 * can also happen early on and even have utlbmisses etc. (p0init)
	 * NOTE in this case we pretend we mark that we're on ICS
	 * even though we're not doing interrupts!
	 */
#ifdef OVFLWDEBUG
	/* make a stab at detecting interrupt stack under/overflow
	 * do this BEFORE accessing memory. don't bother if we're
	 * in the middle of a crash dump since we're running on a
	 * special stack.
	 */
	LA	k0, dump_in_progress
	lw	k0, 0(k0)
	bnez	k0, 1f

	PTR_L	k0, VPDA_INTSTACK(zero)
	PTR_ADDIU k0, k0, 100+EF_SIZE	# some slop
	bleu	sp, k0, stkovflw
	PTR_L	k0, VPDA_LINTSTACK(zero)
	bgtu	sp, k0, stkundflw
1:
#endif

	PTR_SUBU k0,sp,EF_SIZE
	ori	k0,7			# ori/xori is fastest way to make sure
	xori	k0,7			# the stack is dw aligned (for TFP)
	/* R10000_SPECULATION_WAR: stores are dependent on k0 creation */
	sreg	sp,EF_SP(k0)		# generate new stack frame on ICS
	sreg	k1,EF_K1(k0)		# in case we came from utlbmiss
	sreg	gp,EF_GP(k0)
	move	sp,k0
	AUTO_CACHE_BARRIERS_ENABLE	# we now have a stack

	move	k1,k0
	li	k0,AS_DONT_SWITCH	# don't change timer
	j	ecommon


	/*
	 * was on idle stack - PANIC
	 */
keridleframe:
	AUTO_CACHE_BARRIERS_DISABLE	# $sp not set yet
	/* On systems with large memory configurations,
	 * some of the data searched by the scheduler will have K2
	 * addresses.  So we need to be able to handle tlbmisses while
	 * on the idlestack (actually we're getting here due to tlb
	 * invalid fault because only the other half of tlb pair is present
	 * in the tlb).
	 */
#ifdef OVFLWDEBUG
	/* make a stab at detecting idle stack under/overflow
	 * do this BEFORE accessing memory
	 */
	LA	k0, dump_in_progress
	lw	k0, 0(k0)
	bnez	k0, 1f

	PTR_L	k0, VPDA_BOOTSTACK(zero)
	PTR_ADDIU k0, k0, 100+EF_SIZE	# some slop
	bleu	sp, k0, stkovflw
	PTR_L	k0, VPDA_LBOOTSTACK(zero)
	bgtu	sp, k0, stkundflw
#endif
	PTR_SUBU k0,sp,EF_SIZE
	ori	k0,7			# ori/xori is fastest way to make sure
	xori	k0,7			# the stack is dw aligned (for TFP)
	/* R10000_SPECULATION_WAR: stores are dependent on k0 creation */
	sreg	sp,EF_SP(k0)		# generate new stack frame on ICS
	sreg	k1,EF_K1(k0)		# in case we came from utlbmiss
	sreg	gp,EF_GP(k0)
	move	sp,k0
	AUTO_CACHE_BARRIERS_ENABLE	# we now have a stack
#if DEBUG
	/* First we verify that the exception is a TLB miss */

	.set	noreorder
	MFC0(k1,C0_CAUSE)
	nop
	andi	k1,CAUSE_EXCMASK
	addi	k1,-EXC_RMISS
	beq	k1,zero,1f		# OK if TLBL
	addi	k1,EXC_RMISS-EXC_WMISS
#ifdef MH_R10000_SPECULATION_WAR
	beq	k1,zero,1f
	addi	k1,EXC_WMISS-EXC_MOD
#endif	/* MH_R10000_SPECULATION_WAR */
	bne	k1,zero,keridlerror	# else better be TLBS
	nop
1:
	/* Then we verify that the virtual address is NOT in user space */

	DMFC0(k1,C0_BADVADDR)
	nop
	nop
	bltz	k1,2f
	nop
	b	keridlerror
	nop
2:	
	.set	reorder
#endif /* DEBUG */
	move	k1,k0
	li	k0,AS_DONT_SWITCH	# don't change timer
	j	ecommon

keridlerror:
	AUTO_CACHE_BARRIERS_DISABLE	# $sp is not set yet.
	.set noreorder
#if defined(SN0)
	move	k1, k0			# eframe pointer
	sreg	ra,EF_RA(k1)

	jal	tfi_save
	nop				# BDSLOT

	SAVESREGS(k1)
	SAVEAREGS(k1)

	DMFC0(a0, C0_EPC)
	sreg	a0,EF_EPC(k1)
	sreg	a0,EF_EXACT_EPC(k1)	

	MFC0(a0, C0_CAUSE)
	sreg	a0,EF_CAUSE(k1)

	DMFC0(a0, C0_BADVADDR)
	sreg	a0,EF_BADVADDR(k1)

	DMFC0(a0, C0_SR)
	sreg	a0,EF_SR(k1)
#endif

#if EVEREST || SN0
	lreg	a3, EF_SP(k1)		# we saved sp in eframe above
#else
	move	a3, sp
#endif
	PTR_L	sp, VPDA_LBOOTSTACK(zero)
	AUTO_CACHE_BARRIERS_ENABLE	# we now have a stack

	/*
	 * We are about to go to idle_err. Clear the EXL bit so that we
	 * can determine any other exceptions later on.
	 */
        MFC0(a0, C0_SR)
	and	a0, ~(SR_IE|SR_EXL)
        MTC0(a0, C0_SR)
EXL_EXPORT(elocore_exl_13)
	nop

	DMFC0(a0, C0_EPC)
	MFC0(a1, C0_CAUSE)

	move	a2, k1

	jal	idle_err		# panic
	nop				# BDSLOT
	/* NOTREACHED */
	.set reorder

	/*
	 * already on kernel user stack - add a new frame
	 */
kernextframe:
EXL_EXPORT(locore_exl_0)
	AUTO_CACHE_BARRIERS_DISABLE	# $sp is not set yet
	/*
	 * make a stab at detecting kernel stack under/overflow
	 * This includes kernel stack extension
	 */
	LA	k0, dump_in_progress
	lw	k0, 0(k0)
	bnez	k0, 1f

	PTR_L	k0,VPDA_CURKTHREAD(zero)
	beq	k0,zero,1f
	PTR_L	k0,K_STACK(k0)
	bleu	sp, k0, stkovflw
#ifdef OVFLWDEBUG
	sreg	t0,VPDA_T0SAVE(zero)	# another reg to use
	move	t0,k0
	PTR_L	k0,VPDA_CURKTHREAD(zero)
	lw	k0,K_STACKSIZE(k0)
	PTR_ADDU	k0,k0,t0
	PTR_SUBU	k0,ARGSAVEFRM
	lreg	t0,VPDA_T0SAVE(zero)	# restore t0
	bgtu	sp,k0,stkundflw
	/* to be sure that stack is mapped (paranoid huh?) we tmp change
	 * stack type to IDL so if we get an exception we'll
	 * panic!
	 */
1:
	li	k0, PDA_CURIDLSTK
	ORD_CACHE_BARRIER_AT(-EF_SIZE,sp)
	sb	k0, VPDA_KSTACK(zero)
	sw	sp, zapword
	lw	zero, -EF_SIZE(sp)	# touch the top of eframe
	li	k0, PDA_CURKERSTK
	sb	k0, VPDA_KSTACK(zero)
	sw	zero, zapword
	.set	reorder
#endif
1:
	PTR_SUBU k0,sp,EF_SIZE
	ori	k0,7			# ori/xori is fastest way to make sure
	xori	k0,7			# the stack is dw aligned (for TFP)
	/* R10000_SPECULATION_WAR: stores are dependent on k0 creation */
	sreg	sp,EF_SP(k0)		# save old sp
	sreg	k1,EF_K1(k0)		# in case we came from utlbmiss
	sreg	gp,EF_GP(k0)
	move	sp,k0
	AUTO_CACHE_BARRIERS_ENABLE	# we now have a satck
	move	k1,k0
	li	k0,AS_DONT_SWITCH	# don't change timer
	j	ecommon

	/*
	 * Came from user mode or utlbmiss, initialize kernel stack
	 */
keruserframe:
	AUTO_CACHE_BARRIERS_DISABLE	# $sp not set yet
#ifdef STKDEBUG
#if !R4000 && !R10000
	/*
	 * This doesn't work for R4000. we can't check EPC to be in utlbmiss,
	 * since EPC is not overwritten in that case. We could use k1
	 * here, by doing la k1, utlbfault in the utlb miss handler.
	 * That might be good in general, since we could then disambiguate
	 * the case of taking a tlb miss while the EXL bit is set.
	 * Another thing we could do - look at EXC_RMISS and EXC_WMISS,
	 * which is not really accurate.
	 * With multiple utlbmiss handlers this is harder - just make sure
	 * in the utlbmiss handler somewhere.
	 */
	.set	noreorder
	mfc0	k0,C0_SR
	NOP_1_0
	.set	reorder
	and	k0,SR_PREVMODE
	bne	k0,zero,1f		# from user mode
	lui	k0,0x8000
	ori	k0,0x80
	.set	noat
	.set	noreorder
	mfc0	AT,C0_EPC
	NOP_1_0
	.set	reorder
	sltu	AT,AT,k0
	beq	AT,zero,stkscr1
	.set	at
1:
#endif
#endif
	.set	noreorder
	.set	noat
	/*
	 * wire in upage/user kernel stack
	 */
	SVSLOTS
#if R4000 || R10000
#if TLBKSLOTS == 1
	LI	k0, KSTACKPAGE
	DMFC0(AT, C0_TLBHI)		# save pid
	DMTC0(k0, C0_TLBHI)		# set virtual page number
	li	k0, UKSTKTLBINDEX	# base of wired entries
	mtc0	k0, C0_INX		# set index to wired entry
	# PTE_L	k0, VPDA_UPGLO(zero)
	# nop
	LI	k0, TLBLO_G
	/* TLBLO_FIX_HWBITS(k0) */
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO_1)		# LDSLOT set pfn and access bits
	# and	k0, TLBLO_V		# NOP SLOT check valid bit 
	# beq	k0, zero, jmp_baduk	# panic if not valid

	/* now do kernel stack */
	PTE_L	k0, VPDA_UKSTKLO(zero)
	nop
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO)		# LDSLOT set pfn and access bits
	and	k0, TLBLO_V		# NOP SLOT check valid bit 
	c0	C0_WRITEI		# write entry
	beq	k0, zero, jmp_baduk	# panic if not valid
	DMTC0(AT, C0_TLBHI)		# BDSLOT restore pid
#endif	/* TLBKSLOTS == 1 */
#endif	/* R4000 || R10000 */

	.set	reorder
	.set	at
	PTR_L	k0, VPDA_CURUTHREAD(zero)
	PTR_L	k0, UT_EXCEPTION(k0)
	PTR_ADDIU k0, U_EFRAME		# address of exception frame
	/* R10000_SPECULATION_WAR: stores are dependent on k0 creation */
	sreg	k1,EF_K1(k0)		# save k1 in case we came from utlbmiss
	move	k1,k0
	sreg	sp,EF_SP(k1)		# save sp
	sreg	gp,EF_GP(k1)
	LA	gp,_gp
	li	k0,PDA_CURKERSTK	# on kernel stack now
	sb	k0,VPDA_KSTACK(zero)
	LI	sp,KERNELSTACK-ARGSAVEFRM
	AUTO_CACHE_BARRIERS_ENABLE
	li	k0,AS_SYS_RUN		# from proc user to sys timer
	j	ecommon

	/*
	 * Stack Under/Overflow and other stk anomalies
	 * must get into debugger without causing exception
	 */
EXPORT(stkovflw)
	LA	a0,8f
	b	stk1
EXPORT(stkundflw)
	LA	a0,7f
	b	stk1
EXPORT(stkscr1)
	LA	a0,6f
	b	stk1
EXPORT(stkscr2)
	LA	a0,5f
	b	stk1
EXPORT(stkscr3)
	LA	a0,4f
	b	stk1
stk1:
	sw	sp,ovflwword		/* poke something for LAs if can't
					 * really print
					 */
	.set noreorder
	DMFC0(a1,C0_EPC)
	.set reorder
	move	a2,sp
	move	a3,k1			# in case from utlbmiss
	move	s0,ra
	/* make sure a good frame - this works except in VERY early cases */
	lb	s1,VPDA_KSTACK(zero)
	li	k0,PDA_CURIDLSTK	# on kernel stack now
	sb	k0,VPDA_KSTACK(zero)
	PTR_L	sp,VPDA_LBOOTSTACK(zero)
	jal	printf
	LA	a0,3f
	move	a1,s0
	move	a2,s1
	jal	printf
#if R4000 || R10000
	/*
	 * must turn off EXL since jumping to debug() causes a bkpnt exc
	 * and it wants to look at EPC
	 */
#if IP32
	li	a0,3<<3
	sw	a0,VPDA_CURSPL(zero)
	ld	a0,VPDA_SPLMASKS(a0)
	li	k0,CRM_INTMASK|K1BASE
	sd	a0,0(k0)
#endif
	.set noreorder
	MFC0(a0,C0_SR)
	NOP_1_0
	lui	k0,(SR_DE >> 16)	# propagate the SR_DE bit
	ori	k0,SR_ERL
	and	a0,k0,a0
	or	a0,SR_IEC|SR_IMASK8|SR_KERN_SET
	MTC0(a0,C0_SR)
EXL_EXPORT(elocore_exl_0)
	.set reorder
#endif
#if TFP
	/*
	 * must turn off EXL since jumping to debug() causes a bkpnt exc
	 * and it wants to look at EPC
	 */
	.set	noreorder
	DMFC0(k0,C0_SR)
	li	k1,SR_EXL|SR_IEC
	not	k1
	and	k0,k1
	DMTC0(k0,C0_SR)
	.set	reorder
#endif
	LA	a0,2f
	jal	panic

	.data
8:	.asciiz	"Kernel/Interrupt Stack Overflow @0x%x sp:0x%x k1:0x%x\12\15"
7:	.asciiz	"Kernel/Interrupt Stack Underflow @0x%x sp:0x%x k1:0x%x\12\15"
6:	.asciiz	"kernel mode on exception but not KERSTK! @0x%x sp:0x%x k1:0x%x\12\15"
5:	.asciiz	"kernel mode on interrupt but not KERSTK! @0x%x sp:0x%x k1:0x%x\12\15"
4:	.asciiz	"KERSTK on interrupt but not kernel mode! @0x%x sp:0x%x k1:0x%x\12\15"
3:	.asciiz "ra:0x%x stkflag:%d\n"
2:	.asciiz "stack underflow/overflow"

EXPORT(ovflwword)
	.word	0
EXPORT(zapword)
	.word	0

	.text
jmp_baduk:
	j	_baduk

	END(longway)
