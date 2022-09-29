/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* Copyright(C) 1986, MIPS Computer Systems */

#ident	"$Revision: 3.124 $"

#include "ml/ml.h"

#define  TMP_MSK1       (0x00000000FFFFFFFF)
#define  TMP_MSK2       (0xFFFFFFFF00000000)
#define  INTR_MASK      (0x000000000000FF00)    /* IP7 ... IP0 */


/*
 * save(regs)
 * save process context in pcb - return 0
 */
LEAF(save)
#ifdef R10000_SPECULATION_WAR
	/* mfc0 serializes instruction stream before stores avoiding barrier */
	AUTO_CACHE_BARRIERS_DISABLE
	.set	noreorder
	MFC0(v0,C0_SR)
#endif
	/*
	 * Save C linkage registers for current process
	 */
	sreg	ra,PCB_PC*BPREG(a0)
	sreg	sp,PCB_SP*BPREG(a0)
	sreg	fp,PCB_FP*BPREG(a0)
	/*
	 * ?????
	 * SHOULD SAVE FP EXCEPTION INST REGISTER like:
	 * if (fpowner)
	 *	if (cp1_present)
	 *		swc1	FP_EXCEPT,u_PCB_FPEXCEPT*BPREG
	 */
	/*
	 * Save callee saved registers, all other live registers should have
	 * been saved on call to save() via C calling conventions
	 */
	.set 	noreorder
#ifndef R10000_SPECULATION_WAR
	MFC0(v0,C0_SR)
#endif
	/* s-register saves inserted here instead of NOP_1_4 */
	sreg	s0,PCB_S0*BPREG(a0)
	sreg	s1,PCB_S1*BPREG(a0)
	sreg	s2,PCB_S2*BPREG(a0)
	sreg	s3,PCB_S3*BPREG(a0)
	.set 	reorder
	sreg	s4,PCB_S4*BPREG(a0)
	sreg	s5,PCB_S5*BPREG(a0)
	sreg	s6,PCB_S6*BPREG(a0)
	sreg	s7,PCB_S7*BPREG(a0)
	sreg	v0,PCB_SR*BPREG(a0)
	AUTO_CACHE_BARRIERS_ENABLE

	move	v0,zero			# return 0
	j	ra
	END(save)

#if R4000 || R10000
#define NB_UBPTBL	(NBPDE*2)
#else
#define NB_UBPTBL	NBPDE
#endif

/*
 * resume(ut, oldt, tlbpid, {icachepid})
 * restore state for process thread ut 
 * Map kstack, then restore context from kthread
 * return 1
 * NOTE: ASSUMES that kthread is first in proc struct.
 * If oldt is set, then turn off its k_sqself field
 *
 * k_sqself is set for the in process ONLY if its the kernel
 * process for process 'p', otherwise don't even think about touching the
 * out process.
 * a0 - uthread/kthread pointer
 * a1 - outgoing thread or NULL
 * a2 - tlbpid to run
 * a3 - icachepid to run (TFP only)
 */
LEAF(resume)
	/*
	 * disable interrupts now since p_sqself protects kernel stack
	 * and we don't want any interrupts after we've turned off p_sqself
	 * and before we've mapped in new process
	 */
	.set 	noreorder
	NOINTS(t1,C0_SR)		# disable interrupts
	/*******************************************************************
	 * WARNING: The following FOUR instructions execute before interrupts
	 * are disabled (at least on R4000) so should only load values
	 * which won't change.  But we might as well do useful work waiting
	 * for interrupts to be disabled.
	 * R8000 needs no delay here (it's all in NOINTS macro) but it doesn't
	 * hurt to execute these instructions here.
	 * This replaces use of NOP_0_4 macro.
	 */
	li	t1,1
	li	t2,PDA_CURKERSTK
	LI	t3, KSTACKPAGE
#if R4000 || R10000
#if TLBKSLOTS == 0
	li	s1, PDATLBINDEX		# base of wired entries
#elif TLBKSLOTS == 1
	li	s1, UKSTKTLBINDEX	# base of wired entries
#endif /* TLBKSLOTS == 1 */				
#endif /* R4000 || R10000 */		
	
	/* END - hazard, interrupts now guarenteed OFF
	 ******************************************************************/
	
	/* mxc0 in NOINTS will serialize so all addresses are known.  Some
	 * stores here can be executed speculatively, but the addresses are
	 * to known areas that do not get DMAed to (like a0, a1).
	 */
	AUTO_CACHE_BARRIERS_DISABLE	# mtc0 in NOINTS will serialize

	/*
	 * Check for a poke of k_sqself desired.
	 */
	beq	a1,zero,1f
	sb	t2,VPDA_KSTACK(zero)	# BDSLOT (will be on kernel stk)
	/*
	 * Before doing anything else, clear the k_sqself field in the
	 * thread we are leaving.  This says that its
	 * stack is now free, and thus the process can be scheduled.
	 */
	sb	zero,K_SQSELF(a1)
1:
	/*
	 * Conversely, set the k_sqself flag in the proc entry of the
	 * process we are switching to, thus informing the world that
	 * the stack (and uarea) are now in use and inviolate.
	 */
	sb	t1,K_SQSELF(a0)
	AUTO_CACHE_BARRIERS_ENABLE

	/* set us as new current thread (assumes kthread first in uthread) */
	PTR_S	a0, VPDA_CURKTHREAD(zero)
	PTR_S	a0, VPDA_CURUTHREAD(zero)
	.set	reorder

	/*
	 * store our kstk lo in pda for quick access
	 * also drop into tlb
	 * ASSUMES that page stuff part is stored first in a pde!
	 */
	PTE_L	t1, UT_UKSTK(a0)
	PTE_S	t1, VPDA_UKSTKLO(zero)
	/*
	 * wire in user's kernel stack
	 */
	.set	noreorder
#if R4000 || R10000
	/* Only uses one pair of TLB entries */
	TLBLO_FIX_HWBITS(t1)

#if NO_WIRED_SEGMENTS
	or	t3,a2			# merge in tlbpid
#endif
	DMTC0(t3, C0_TLBHI)		# set virtual page number
#if TLBKSLOTS == 1
	mtc0	s1, C0_INX		# set index to wired entry

	/* Need to put the G bit into tlblo_1 */
	LI	t2, TLBLO_G
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	t2, C0_TLBLO_1

	/* now do kernel stack */
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	t1, C0_TLBLO		# LDSLOT set pfn and access bits
	NOP_0_2
	c0	C0_WRITEI		# write entry
#if EXTKSTKSIZE == 1
	lw	t1,VPDA_MAPSTACKEXT(zero)	# get the flag for Kernel 
						# stack ext mapping
	beq	t1,zero,1f
	nop
	PTE_L	t3,VPDA_PDALO(zero)	# get the pde for PDAPAGE
	PTE_L	t1,VPDA_STACKEXT(zero)	# get the pde for Kernel stack ext
	sw	zero,VPDA_MAPSTACKEXT(zero)	# reset the flag for Kernel 
						# stack ext mapping
	li	t2,PDAPAGE		# inline tlbwired code
	TLBLO_FIX_HWBITS(t3)
	TLBLO_FIX_HWBITS(t1)
	and	t2,TLBHI_VPNMASK	# chop offset bits
	DMTC0(t2,C0_TLBHI)		# set VPN and TLBPID
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	t3,C0_TLBLO		# set PPN and access bits
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	t1,C0_TLBLO_1		# set PPN and access bits for 2nd pte.
	mtc0	zero,C0_INX		# set INDEX to wired entry
	NOP_1_1				# let index get through pipe
	c0	C0_WRITEI		# drop it in
	NOP_0_1				# let write get args before mod
1:
#endif	/* EXTKSTKSIZE == 1 */
#endif	/* TLBKSLOTS == 1 */
#if TLBKSLOTS == 0
	mtc0	s1, C0_INX		# set index to wired entry
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	t1, C0_TLBLO		# LDSLOT set pfn and access bits
	PTE_L	t2, VPDA_PDALO(zero)
	TLBLO_FIX_HWBITS(t2)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	t2, C0_TLBLO_1		# LDSLOT set pfn and access bits
	NOP_0_2
	c0	C0_WRITEI		# write entry
#endif	/* TLBKSLOTS == 0 */
#endif	/* R4000 || R10000 */
#if TFP
	/* WARNING: cop0 hazard requires SSNOP between consecutive dmtc0
	 */

	NOP_SSNOP
	dmtc0	zero,C0_TLBSET		# wired entries are in Set 0
	sll	a2, TLBHI_PIDSHIFT	# line up tlb pid bits
	dsll	a3,40			# two shifts force superscaler dispatch
	
	/* now do kernel stack */
	
	dmtc0	t3,C0_BADVADDR
	NOP_SSNOP
	dmtc0	a3, C0_ICACHE		# setup correct IASID
	or	t3,a2			# setup new PID
	NOP_SSNOP
	dmtc0	t3, C0_TLBHI		# set virtual page number
	TLBLO_FIX_HWBITS(t1)		# shift pfn field to correct position
	NOP_SSNOP
	dmtc0	t1, C0_TLBLO		# set pfn and access bits
	TLB_WRITER			# write entry

#endif	/* TFP */

#if BEAST

#if defined (PSEUDO_BEAST)
	.data
resmsg:	.asciiz	"Need to fix beast resume. process.s"
	.text
	LA	a0, resmsg
	jal	printf
	 nop
#else	/* PSEUDO_BEAST */

#error <BOMB! Need beast resume code here >
		
#endif	/* PSEUDO_BEAST */
#endif	/* BEAST */
	
#if NO_WIRED_SEGMENTS
    	NOP_0_4				# delay after C0 op before access data
#else
	.set	reorder
	/*
	 * Restore all wired tlb entries here.
	 * Virtual addresses are stored in proc table entry in tlbhi_tbl.
	 * pde's (tlblo part) are in p_ubptbl.
	 * The first TLBWIREDBASE entries are constant and not reloaded here
	 * TLBKSLOTS wired entries are used for u structure and kernel stack.
	 * when in the kernel, and are restored to the user's upon exit
	 * from the kernel
	 * The other entries are used to map page tables as needed.
	 * The tfault routine drops in entries on double tlb misses.
	 *
	 * R4000/R10000: ubptbl consists of tlbpde_t structs, which are
	 * 2 pde_t structs back to back.
	 */
	PTR_L	t2, UT_EXCEPTION(a0)	# a0 is uthread pointer
#if TLBKSLOTS != 0
	li	t3,TLBWIREDBASE+TLBKSLOTS		#wired entry base
	PTR_ADDIU t1,t2,(U_UBPTBL+(TLBKSLOTS*NB_UBPTBL)) # ptr to tlblo entries
	PTR_ADDIU t2, (U_TLBHI_TBL+(TLBKSLOTS*(_MIPS_SZPTR/8))) # pointer to tlbhi entries
#else
	/* For 16K page size, the kstack shares an entry with the pda.
	 * Hence, we want to start with entry 0 in the u_ubptbl and
	 * u_tlbhi_tbl arrays, and with index 1 in the tlb.
	 */
	li	t3,TLBWIREDBASE		#wired entry base
	PTR_ADDIU t1, t2, U_UBPTBL		# ptr to tlblo entries
	PTR_ADDIU t2, U_TLBHI_TBL	# tlbhi entries
#endif

	LI	s0,NWIREDENTRIES	# loop end count
1:
	PTE_L	ta0,(t1)			# get pte (tlblo)
#if R4000 || R10000
	PTE_L	t0,NBPDE(t1)		# get pte (tlblo_1)
	TLBLO_FIX_HWBITS(ta0)
	TLBLO_FIX_HWBITS(t0)
#endif
	PTR_L	ta1,(t2)		# get virtual address (tlbhi)
	PTR_ADDIU t1,NB_UBPTBL		# bump pointer to pte's
	or	ta1,a2			# merge in tlbpid
	.set	noreorder
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	ta0,C0_TLBLO		# set pfn and access bits
#if R4000 || R10000
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	t0,C0_TLBLO_1		# set pfn and access bits
#endif
	DMTC0(ta1,C0_TLBHI)		# set virtual page number and tlbpid
#if R4000 || R10000	
	mtc0	t3,C0_INX		# set index to wired entry
#elif BEAST
	mtc0	t3,C0_TLBSET		# set index to wired entry
#endif		
	addi	t3,+(1)			# HAZARD SLOT:	 bump index
	c0	C0_WRITEI		# write entry
	bne	t3,s0,1b
	PTR_ADDIU t2,(_MIPS_SZPTR/8)	# BDSLOT: bump ptr to virt. addrs
	.set	reorder
#endif /* !NO_WIRED_SEGMENTS */

	/*
	 * ?????
	 * SHOULD RELOAD FP EXCEPTION INST REGISTER
	 * if (fpowner)
	 *	if (cp1_present) 
	 *		lwc1	FP_EXCEPT,u+PCB_FPEXCEPT
	 */

	.set	noreorder
#ifndef SN0	
#ifdef SPLDEBUG
	lreg	sp,PCB_SP*BPREG(a0)
	jal	spldebug_log_event	# log process proc pointer
	move	s6,a0			# BDSLOT  save a0
	jal	spldebug_log_event	# log new C0_SR
	lreg	a0,PCB_SR*BPREG(s6)	# BDSLOT  a0 <- (new)C0_SR
	move	a0,s6			# restore a0
#endif /* SPLDEBUG */
#endif /* !SN0 */	

	/*
	 * Reload callee saved registers, all other live registers
	 * should be reloaded from stack via C calling conventions.
	 * NOTE: assumes that kthread is first in proc AND pcbregs are
	 *	first in kthread.
	 */

	lreg	ra,PCB_PC*BPREG(a0)
	lreg	sp,PCB_SP*BPREG(a0)
	lreg	fp,PCB_FP*BPREG(a0)
	lreg	s0,PCB_S0*BPREG(a0)
	lreg	s1,PCB_S1*BPREG(a0)
	lreg	s2,PCB_S2*BPREG(a0)
	lreg	s3,PCB_S3*BPREG(a0)
	lreg	s4,PCB_S4*BPREG(a0)
	lreg	s5,PCB_S5*BPREG(a0)
	lreg	v0,PCB_SR*BPREG(a0)
	lreg	s6,PCB_S6*BPREG(a0)
	lreg	s7,PCB_S7*BPREG(a0)
	MTC0(v0,C0_SR)
	NOP_0_4
	j	ra
	li	v0,1			# BDSLOT return non-zero
	.set    reorder
	END(resume)

/*
 * resumethread(kthread_t *new, kthread_t *old)
 *
 * Resume an sthread or an ithread
 */
LEAF(resumethread)
	.set	noreorder
	MFC0(v0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)		# no ints while swapping stacks
	NOP_0_2
	li	t1,1	
	li	t2,PDA_CURKERSTK

	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 above will serialize

	/* move to new stack */

	sb	t1,K_SQSELF(a0)
	PTR_S	a0,VPDA_CURKTHREAD(zero)
	PTR_S	zero,VPDA_CURUTHREAD(zero)
	sb	t2,VPDA_KSTACK(zero)
	
	/* if new == old need to make sure sqself still set */
	beq	a0,a1,1f
	lreg	sp,PCB_SP*BPREG(a0)	# BDSLOT
	
	/* No longer need old stack */
	bnel	a1,zero,1f
	sb	zero,K_SQSELF(a1)	# BDSLOT (cancelled when a1 == 0)
1:
	AUTO_CACHE_BARRIERS_ENABLE

#if TFP
	li	a3, 0			# all sys processes use tlbpid 0
	dsll	a3,40			# Shift ASID to correct bits
	/*
	 * Be careful with TFP Hazards here.
	 * We're relying on the preceeding and following instructions.
	 */
	dmtc0	a3, C0_ICACHE		# setup correct IASID
#endif

	/*
	 * Reload callee saved registers, all other live registers
	 * should be reloaded from stack via C calling conventions.
	 */
	lreg	ra,PCB_PC*BPREG(a0)
	lreg	fp,PCB_FP*BPREG(a0)
	lreg	s0,PCB_S0*BPREG(a0)
	lreg	s1,PCB_S1*BPREG(a0)
	lreg	s2,PCB_S2*BPREG(a0)
	lreg	s3,PCB_S3*BPREG(a0)
	lreg	s4,PCB_S4*BPREG(a0)
	lreg	s5,PCB_S5*BPREG(a0)
	lreg	s6,PCB_S6*BPREG(a0)
	lreg	s7,PCB_S7*BPREG(a0)
	MTC0(v0,C0_SR)			# restore interrupts above splhi
	NOP_0_4
	li	v0,1
	j	ra
	.set    reorder
	END(resumethread)


/*
 * restartxthread(kthread_t *new, caddr_t sp, kthread_t *old)
 *
 * restart an xthread after it has called ipsema()
 */
LEAF(restartxthread)
	.set	noreorder
	MFC0(v0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)		# no ints while swapping stacks
	li	t1,1			# BDSLOT
	li	t2,PDA_CURKERSTK

	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 above will serialize

	/* move to new stack */
	sb	t1,K_SQSELF(a0)
	PTR_S	a0,VPDA_CURKTHREAD(zero)
	PTR_S	zero,VPDA_CURUTHREAD(zero)
	sb	t2,VPDA_KSTACK(zero)
	
	/* if new == old need to make sure sqself still set */
	beq	a0,a2,1f		
	move	sp,a1			# BDSLOT
	
	/* No longer need old stack */
	bnel	a2,zero,1f
	sb	zero,K_SQSELF(a2)	# BDSLOT (cancelled when a2 == 0)
1:
	AUTO_CACHE_BARRIERS_ENABLE

	MTC0(v0,C0_SR)			# restore interrupts above splhi
	NOP_0_4

#if TFP
	li	a3, 0			# all sys processes use tlbpid 0
	dsll	a3,40			# Shift ASID to correct bits
	/* Be careful with TFP Hazards here.
	 * We're relying on the preceeding and following instructions.
	 */
	dmtc0	a3, C0_ICACHE		# setup correct IASID
#endif	/* TFP */
	j	xthread_prologue
	nop
	.set    reorder
	END(restartxthread)

/*
 * loopxthread(kthread_t *new, caddr_t sp)
 *
 * loop an xthread (longjump to the beginning) after it has
 * called ipsema() with an already pending vsema
 */
LEAF(loopxthread)
	.set	noreorder
	j	xthread_loop_prologue
	move	sp,a1			# BDSLOT
	.set	reorder
	END(loopxthread)

/*
 * Start up sthread - this is called on first resume of an sthread.
 * sthread_create has left lots of goodies in the PCB regs.
 * And, they have already been put in the registers by resumethread,
 * so just move them to the right place before calling the function.
 * S0 == arg0
 * S1 == arg1
 * S2 == arg2
 * S3 == arg3
 * FP == function to jmp to
 */
LEAF(sthread_launch)
#if DEBUG
	sb	zero,VPDA_SWITCHING(zero)
#endif
	jal	spl0
	move	a0,s0
	move	a1,s1
	move	a2,s2
	move	a3,s3
	jalr	fp
	jal	sthread_exit
	END(sthread_launch)

/*
 * p0_launch - launch first process
 */
LEAF(p0_launch)
#if DEBUG
	sb	zero,VPDA_SWITCHING(zero)
#endif
	jal	spl0
	jal	p0
	j	proc1
	END(p0_launch)

/*
 * resumeidle - jump to an idle loop using the boot/idle stack
 * Must be called at splhi
 * a0 - if !NULL points to a thread to turn off k_sqself
 */
LOCALSZ=	1			/* Save ra */
RESFRM=		FRAMESZ((NARGSAVE+LOCALSZ)*SZREG)
RAOFF=		RESFRM-(1*SZREG)
NESTED(resumeidle, RESFRM, ra)
	.set	noreorder
	MFC0(v0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)		# no ints while swapping stacks
	NOP_0_3
	li	t1,PDA_CURIDLSTK
	sb	t1,VPDA_KSTACK(zero)		# now on IDLE stack
	PTR_L	sp,VPDA_LBOOTSTACK(zero)	# sp now on IDLE stack

	/*
	 * Check for a poke of k_sqself desired. Note that this occurs
	 * AFTER we have moved to the new stack, otherwise a high priority
	 * interrupt (like HW errors which are above splprof()) will still
	 * be executing on the old process stack with K_SQSELF clear and
	 * runnable on another CPU.
	 */
	beq	a0,zero,1f
	 nop					# BDSLOT
	/*
	 * Before doing anything else, clear the k_sqself field in the
	 * thread we are leaving.  This says that its
	 * stack is now free, and thus the process can be scheduled.
	 */
	CACHE_BARRIER_AT(K_SQSELF,a0)
	sb      zero,K_SQSELF(a0)
1:

	/* null out current thread */
	PTR_S	zero, VPDA_CURKTHREAD(zero)
	PTR_S	zero, VPDA_CURUTHREAD(zero)
#if DEBUG
	sb	zero,VPDA_SWITCHING(zero)
#endif

	MTC0(v0,C0_SR)			# restore interrupts above splhi
	NOP_0_4

	.set	reorder

	/*
	 * now implement:
	 *	resumenewthread(idle(), 0);
	 */
#if NARGSAVE > 0
	/* Only o32 system require us to reserve stack space for callee.
	 * And no need to save "ra" since we never return to caller of
	 * resumeidle().
	 */
	PTR_SUBU sp,RESFRM
	REG_S	ra,RAOFF(sp)
#endif	
	jal	idle
	move	a0, v0
	li	a1, 0
#if DEBUG
	li	t0, 1
	sb	t0,VPDA_SWITCHING(zero)
#endif
	jal	resumenewthread
	/* NOTREACHED */
	END(resumeidle)


/*
 *  toidlestk(kthread, finishroutine, arg0, arg1)- jump to the boot/idle stack
 *  Must be called at splhi
 */
NESTED(toidlestk, RESFRM, ra)
	.set	noreorder
	MFC0(v0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)		# no ints while swapping stacks
	NOP_0_3
	li	t1, PDA_CURIDLSTK
	PTR_L	sp, VPDA_LBOOTSTACK(zero)
	/*
	 * Place the process on the idle stack
	 */
	sb	t1, VPDA_KSTACK(zero)

	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 above will serialize
	/* null out current thread */
	PTR_S	zero, VPDA_CURKTHREAD(zero)
	PTR_S	zero, VPDA_CURUTHREAD(zero)
	sb	zero, K_SQSELF(a0)
	AUTO_CACHE_BARRIERS_ENABLE

	PTR_SUBU sp, RESFRM		# set up stack frame

	MTC0(v0,C0_SR)			# restore interrupts above splhi
	NOP_0_4

	.set	reorder
	/* since we never return, we can trash s regs */
	move	s0, a1
	move	a0, a2
	move	a1, a3
	j	s0
	END(toidlestk)

/*
 * setjmp(jmp_buf) -- save current context for non-local goto's
 * return 0
 */
LEAF(setjmp)
	sreg	ra,JB_PC*BPREG(a0)
	sreg	sp,JB_SP*BPREG(a0)
	sreg	fp,JB_FP*BPREG(a0)
	sreg	s0,JB_S0*BPREG(a0)
	sreg	s1,JB_S1*BPREG(a0)
	sreg	s2,JB_S2*BPREG(a0)
	sreg	s3,JB_S3*BPREG(a0)
	sreg	s4,JB_S4*BPREG(a0)
	sreg	s5,JB_S5*BPREG(a0)
	sreg	s6,JB_S6*BPREG(a0)
	sreg	s7,JB_S7*BPREG(a0)
#if EVEREST
	lbu	v0,VPDA_CELSHADOW(zero)
	sreg	v0,JB_CEL*BPREG(a0)
#endif /* EVEREST */
	.set    noreorder
	MFC0(v0,C0_SR)
	NOP_1_4
	.set    reorder
	sreg	v0,JB_SR*BPREG(a0)
	move	v0,zero
	j	ra
	END(setjmp)


/*
 * _longjmp(jmp_buf)
 */
LEAF(longjmp)
	.set	noreorder
	lreg	ra,JB_PC*BPREG(a0)
	lreg	sp,JB_SP*BPREG(a0)
	lreg	fp,JB_FP*BPREG(a0)
	lreg	s0,JB_S0*BPREG(a0)
	lreg	s1,JB_S1*BPREG(a0)
	lreg	s2,JB_S2*BPREG(a0)
	lreg	s3,JB_S3*BPREG(a0)
	lreg	s4,JB_S4*BPREG(a0)
	lreg	s5,JB_S5*BPREG(a0)
	lreg	s6,JB_S6*BPREG(a0)
	lreg	v0,JB_SR*BPREG(a0)
	lreg	s7,JB_S7*BPREG(a0)
#if EVEREST
	lreg	t0,JB_CEL*BPREG(a0)
	LI	v1,EV_CEL
	FORCE_NEW_CEL(t0,v1)
#endif
	MTC0(v0,C0_SR)
	NOP_0_4
	j	ra
	li	v0,1		/* return non-zero */
	.set 	reorder
	END(longjmp)
#ifdef USE_PTHREAD_RSA
	
/*
 * void save_rsa_fp( uthread_t *, rsa_t *)
 *
 * Moves fp registers from U_PCB into the register save area (RSA).
 */	
LEAF(save_rsa_fp)
	PTR_L	a2, UT_EXCEPTION(a0)
	
	/* save "special" FP registers */
	
	lw	t0, PCB_FPC_CSR+U_PCB(a2)
	sw	t0, RSA_FPC_CSR(a1)	

	/* save FP registers */
	PTR_ADDI t0, a2, PCB_FPREGS+U_PCB
	PTR_ADDI t1, a1, RSA_FPREGS
	PTR_ADDI t2, a2, PCB_FPREGS+U_PCB+(32*8)
	.set noreorder
1:	ld	t3, (t0)
	sd	t3, (t1)
	daddi	t0, 8
	bne	t0, t2, 1b
	daddi	t1, 8	

	.set reorder
	j	ra
	END(save_rsa_fp)	
/*
 * void restore_rsa_fp( uthread_t *, rsa_t *)
 *
 * Moves registers from register save area (RSA) to uthread structures
 * (U_EFRAME, U_PCB),
 */	
LEAF(restore_rsa_fp)
	PTR_L	a2, UT_EXCEPTION(a0)

	.set reorder

	/* save "special" FP registers */
	
	lw	t0, RSA_FPC_CSR(a1)	
	sw	t0, PCB_FPC_CSR+U_PCB(a2)

	/* save FP registers */
	PTR_ADDI t1, a2, PCB_FPREGS+U_PCB
	PTR_ADDI t0, a1, RSA_FPREGS
	PTR_ADDI t2, a1, RSA_FPREGS+(32*8)
	.set noreorder
1:	ld	t3, (t0)
	sd	t3, (t1)
	daddi	t0, 8
	bne	t0, t2, 1b
	daddi	t1, 8	

	.set reorder
	j	ra
	END(restore_rsa_fp)	
/*
 * void syscall_save_rsa( uthread_t *, rsa_t *)
 *
 * Moves registers from uthread structures (U_EFRAME, U_PCB) into the
 * register save area (RSA).
 * NOTE: Different from save_rsa() in that we're invoked from a syscall
 * so we don't save registers which are allowed to be modified within a
 * syscall.  This entry point is simply an optimization.
 */	
LEAF(syscall_save_rsa)
	PTR_L	a2, UT_EXCEPTION(a0)

	/* setup "successful" status from dyield call */
	
	li	t0, 1
	sreg	t0, RSA_V0(a1)
	sreg	zero, RSA_A3(a1)

	/* Now save rest of registers */
	
	lreg	t0, EF_S0+U_EFRAME(a2)
	sreg	t0, RSA_S0(a1)
	lreg	t0, EF_S1+U_EFRAME(a2)
	sreg	t0, RSA_S1(a1)
	lreg	t0, EF_S2+U_EFRAME(a2)
	sreg	t0, RSA_S2(a1)
	lreg	t0, EF_S3+U_EFRAME(a2)
	sreg	t0, RSA_S3(a1)
	lreg	t0, EF_S4+U_EFRAME(a2)
	sreg	t0, RSA_S4(a1)
	lreg	t0, EF_S5+U_EFRAME(a2)
	sreg	t0, RSA_S5(a1)
	lreg	t0, EF_S6+U_EFRAME(a2)
	sreg	t0, RSA_S6(a1)
	lreg	t0, EF_S7+U_EFRAME(a2)
	sreg	t0, RSA_S7(a1)
	lreg	t0, EF_GP+U_EFRAME(a2)
	sreg	t0, RSA_GP(a1)
	lreg	t0, EF_SP+U_EFRAME(a2)
	sreg	t0, RSA_SP(a1)
	lreg	t0, EF_FP+U_EFRAME(a2)
	sreg	t0, RSA_FP(a1)
	lreg	t0, EF_RA+U_EFRAME(a2)
	sreg	t0, RSA_RA(a1)

	lreg	t0, EF_MDLO+U_EFRAME(a2)
	sreg	t0, RSA_MDLO(a1)
	lreg	t0, EF_MDHI+U_EFRAME(a2)
	sreg	t0, RSA_MDHI(a1)
	lreg	t0, EF_EPC+U_EFRAME(a2)
	sreg	t0, RSA_EPC(a1)

	/* save "special" FP registers */
	
	lw	t0, PCB_FPC_CSR+U_PCB(a2)
	sw	t0, RSA_FPC_CSR(a1)	

	/* save FP registers */
	PTR_ADDI t0, a2, PCB_FPREGS+U_PCB
	PTR_ADDI t1, a1, RSA_FPREGS
	PTR_ADDI t2, a2, PCB_FPREGS+U_PCB+(32*8)
	.set noreorder
1:	ld	t3, (t0)
	sd	t3, (t1)
	daddi	t0, 8
	bne	t0, t2, 1b
	daddi	t1, 8	

	.set reorder
	j	ra
	END(syscall_save_rsa)	
#endif /* USE_PTHREAD_RSA */
