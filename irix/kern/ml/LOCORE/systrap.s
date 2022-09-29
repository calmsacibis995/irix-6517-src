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

#ident	"$Revision: 1.28 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

#ifdef ULI
LEAF(ULIsystrap_j)	
	# in ULI mode, system calls are diverted elsewhere
	.set	reorder
	sreg	AT,VPDA_ATSAVE(zero)	# save AT for user mode
	j	ULIsystrap
	END(ULIsystrap_j)
	.set	noreorder
#endif
/*
 * Syscall
 * NOTE: v0 and v1 must get restored on exit from syscall!!
 *
 * R10000_SPECULATION_WAR: $sp is not set yet.  May be entered speculatively.
 * NOTE: contexts of AT register still in register, not in VPDA_ATSAVE
 */
	.align	7	/* for performance, align to cacheline */
NESTED(systrap, 0, zero)
EXL_EXPORT(locore_exl_25)
	AUTO_CACHE_BARRIERS_DISABLE
	.set	noreorder
	.set	noat
#ifdef ULI
	# in ULI mode, system calls are diverted elsewhere
	PTR_L	k0, VPDA_CURULI(zero)
	bne	k0, zero, ULIsystrap_j
	PTR_L	k1, VPDA_CURUTHREAD(zero)	# BDSLOT
#endif
	/*
	 * wire in upage/user kernel stack
	 */
#ifdef TLBDEBUG	
	SVSLOTS
	PTR_L	k1, VPDA_CURUTHREAD(zero)	# BDSLOT
#endif	
#if R4000 || R10000
#if TLBKSLOTS == 1
	LI	k0, KSTACKPAGE
	DMFC0(AT, C0_TLBHI)		# save pid
	DMTC0(k0, C0_TLBHI)		# set virtual page number
	li	k0, UKSTKTLBINDEX	# base of wired entries
	mtc0	k0, C0_INX		# set index to wired entry
	# PTE_L	k0, VPDA_UPGLO(zero)
	PTE_L	k1, VPDA_UKSTKLO(zero)	# prepare for kernel stack
	LI	k0, TLBLO_G		# global bit for odd mapping
	/* TLBLO_FIX_HWBITS(k0) */
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO_1)		# LDSLOT set pfn and access bits
	# and	k0, TLBLO_V		# NOP SLOT check valid bit 
	# beq	k0, zero, jmp_baduk	# panic if not valid

	/* now do kernel stack */
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(k1, C0_TLBLO)		# LDSLOT set pfn and access bits
	and	k1, TLBLO_V		# NOP SLOT check valid bit 
	c0	C0_WRITEI		# write entry
	beq	k1, zero, jmp_baduk	# panic if not valid
	DMTC0(AT, C0_TLBHI)		# BDSLOT restore pid
#endif	/* TLBKSLOTS == 1 */
#endif	/* R4000 || R10000 */
	.set	reorder

	PTR_L	k1,VPDA_CURUTHREAD(zero)
	PTR_L	k1,UT_EXCEPTION(k1)
	PTR_ADDIU k1, U_EFRAME		# address of exception frame

	/*
	 * Came from user mode - initialize kernel stack
	 * store state in u.u_eframe
	 */
systrapfast_bailout2:		
	.set	at
	/* R10000_SPECULATION_WAR: k1 is dependent on 2 PTR_Ls above */
	sreg	sp,EF_SP(k1)		# build initial stack
	sreg	gp,EF_GP(k1)
	LA	gp,_gp
#if TFP
	/*********************************************************************
	 * TFP code starts here
	 *
	 * We save "sregs" first so we can pre-load various 64-bit constants
	 * which the assembler will re-order into the subsequent "save"
	 * sequences saving many superscaler cycles.
	 ********************************************************************/

	.set	noreorder
	SAVESREGS_GETCOP0(k1)		# various COP0 regs in S0..S4
	.set	reorder
	LI	s5,SR_IMASK|SR_KERN_USRKEEP
	LI	s6,SR_KERN_SET|SR_IE
	SAVEAREGS(k1)
	PTR_ADDIU s3, 4			# restart after syscall

	/*
	 * Save rest of state for signal handlers
	 * Callee save regs are all that matters because that's all that
	 * matters across a procedure call boundary.
	 * Signals return through trap interface so it will restore
	 * callee save regs on return from signal.
	 */
	/* SAVEREGS_GETCOP0 returns:
	 *	s0 == C0_SR
	 *	s1 == C0_CONFIG
	 *	s2 == C0_CAUSE
	 *	s3 == C0_EPC
	 */
	sreg	ra,EF_RA(k1)
	SAVEFPREGS(s0,k1,k0)		# save FP temp reg, needs s0 == C0_SR
	sreg	s3,EF_EPC(k1)
	sreg	s3,EF_EXACT_EPC(k1)	
	sreg	v0,EF_V0(k1)		# u_rval1/sysnum
	sreg	v1,EF_V1(k1)		# u_rval2
	sreg	fp,EF_FP(k1)		# is register 's8'
	sreg	s0,EF_SR(k1)
	sreg	s1,EF_CONFIG(k1)
#ifdef DEBUG
	/* make kp efr print correct code */
	sreg	s2,EF_CAUSE(k1)
#endif	/* DEBUG */
#ifdef STKDEBUG
	/* make sure from user mode */
	and	a1,s0,SR_PREVMODE
	beq	a1,zero,kernelsyscall	# whoops! kernel did a syscall!
#endif

	LI	sp,KERNELSTACK-ARGSAVEFRM # set initial stack

	/* switch to system CPU timer */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	LI	a1,AS_SYS_RUN
	jal	ktimer_switch

	lreg	a0,EF_V0(k1)		#  get sysnum register
	and	s2,s0,s5		# kernel base mode, EXL low
	or	s2,s6			# and interrupts enabled.
	li	k1,PDA_CURKERSTK
	sb	k1,VPDA_KSTACK(zero)	# now on kernel stack

	/*
	 * Dispatch to syscall
	 * Register setup:
	 *	s0 -- SR register at time of exception
	 *	s2 -- SR register with interrupts enabled
	 *	a0 -- sysnum register
	 */
	.set noreorder
	/* Note hazard between dmtc use of miscBus and jal use of miscBus */
	dmtc0	s2,C0_SR
	xori	s2,SR_IE		# DMTC HAZ - s2 == turn off interrupts
	NOP_SSNOP
	jal	syscall			# syscall(ef_ptr, sysnum)
	NOP_SSNOP			# BDSLOT

	/* two dmtc0 must not execute in same cycle. Also, interrupts must be
	 * blocked before we restore "k1" and proceed (takes 3 cycles after
	 * "dmtc" which disables interrupts).
	 */
	dmtc0	s2,C0_SR		# now we actually disable ints
	NOP_SSNOP			# DMTC HAZ
	NOP_SSNOP			# DMTC HAZ
	NOP_SSNOP			# DMTC HAZ
	NOP_SSNOP			# DMTC HAZ
	/* Interrupts disabled by now */
	.set	reorder
	PTR_L	k1, VPDA_CURUTHREAD(zero)
	PTR_L	k1, UT_EXCEPTION(k1)
	PTR_ADDIU k1, U_EFRAME		# address of exception frame
	beq	v0,zero,1f		# If syscall() returns non-zero, then
					# return through the trap machanism.
	j	exception_exit

1:

#if TFP_MOVC_CPFAULT_FAST_WAR
	/* The following checks are repeated inside tfp_loadfp routine. We
	 * perform a quick check here to handle the common case and avoid
	 * invoking the tfp_loadfp routine.
	 */
	PTR_L	a2,VPDA_FPOWNER(zero)
	PTR_L	a3,VPDA_CURUTHREAD(zero)
	beq	a2,a3,1f
	PTR_L	a2,UT_PPROXY(a3)	# uthread -> proxy structure
	lb	a2,PRXY_ABI(a2)
	and	a2,ABI_IRIX5_64 | ABI_IRIX5_N32
	beq	a2,zero,1f
	jal	tfp_loadfp		# load fp registers if necessary
1:
#endif	/* TFP_MOVC_CPFAULT_FAST_WAR */

	/* switch to user CPU timer */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	LI	a1,AS_USR_RUN
	jal	ktimer_switch

	lreg	s4,EF_CONFIG(k1)
	lreg	v0,EF_V0(k1)		# u_rval1
	lreg	v1,EF_V1(k1)		# u_rval2
	lreg	fp,EF_FP(k1)		# is register 's8'

	sb	zero,VPDA_KSTACK(zero)	# switching to user stack
	lreg	gp,EF_GP(k1)
	RESTOREAREGS(k1)
	RESTOREFPREGS_TOUSER(s2)	# s2 == work reg
	lreg	ra,EF_RA(k1)
	lreg	sp,EF_SP(k1)
	.set	noat
	.set	noreorder
	dmtc0	s4,C0_CONFIG
	RESTORESREGS_SR_EPC(k1)		# restore s-regs, C0_SR, C0_EPC
	eret

#else	/* !TFP */
	/*********************************************************************
	 * Non-TFP code starts here
	 ********************************************************************/

	SAVEAREGS(k1)
	/*
	 * In order to support N32 user apps running on O32 compiled
	 * kernels, we need to save $8, $9, $10, and $11 to the eframe
	 * so that syscall can pick it up.
	 */
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	sreg	t0, EF_T0(k1)
	sreg	t1, EF_T1(k1)
	sreg	t2, EF_T2(k1)
	sreg	t3, EF_T3(k1)
#endif

	.set 	noreorder
	DMFC0(a0,C0_EPC)
	.set	reorder
	/*
	 * Save rest of state for signal handlers
	 * Callee save regs are all that matters because that's all that
	 * matters across a procedure call boundary.
	 * Signals return through trap interface so it will restore
	 * callee save regs on return from signal.
	 */
	SAVESREGS(k1)
	sreg	ra,EF_RA(k1)
	.set	noreorder
	MFC0(s0,C0_SR)
	.set	reorder			# is around ifdef for nop if PDEBUG
	SAVEFPREGS(s0,k1,k0)		# save FP temp reg, needs s0 == C0_SR
	PTR_ADDIU a0, 4			# restart after syscall
	sreg	a0,EF_EPC(k1)
	sreg	a0,EF_EXACT_EPC(k1)	
	sreg	v0,EF_V0(k1)		# u_rval1/sysnum
	sreg	v1,EF_V1(k1)		# u_rval2
	sreg	fp,EF_FP(k1)		# is register 's8'
	sreg	s0,EF_SR(k1)
#ifdef DEBUG
	/* make kp efr print correct code */
	.set	noreorder
	MFC0(v0,C0_CAUSE)
	NOP_1_1
	sreg	v0,EF_CAUSE(k1)
	.set	reorder
#endif	/* DEBUG */
#ifdef STKDEBUG
	/* make sure from user mode */
	and	a1,s0,SR_PREVMODE
	beq	a1,zero,kernelsyscall	# whoops! kernel did a syscall!
#endif

	LI	sp,KERNELSTACK-ARGSAVEFRM # set initial stack
	AUTO_CACHE_BARRIERS_ENABLE	# we now have a stack frame

	/* switch to system CPU timer */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	LI	a1,AS_SYS_RUN
	jal	ktimer_switch

	lreg	a0,EF_V0(k1)		# get sysnum register
	li	k1,PDA_CURKERSTK
	sb	k1,VPDA_KSTACK(zero)	# now on kernel stack

	/*
	 * Dispatch to syscall
	 * Register setup:
	 *	s0 -- SR register at time of exception
	 *	a0 -- sysnum (from v0)
	 */
	.set noreorder

	and	a2,s0,SR_IMASK|SR_KERN_SET	# kernel base mode, EXL low
	or	a2,SR_IE		# and interrupts enabled.
to_syscall:
	MTC0(a2,C0_SR)			# BDSLOT
EXL_EXPORT(elocore_exl_25)
	/* No need to wait for interrupts to be enabled before proceeding.
	 * Might as well execute meaningful instructions rather than NOPs.
	 */
	jal	syscall			# syscall(ef_ptr, sysnum)
	and	s2,s0,SR_IMASK|SR_KERN_SET|SR_DEFAULT	# BDSLOT s2=disable intr mask
	MTC0( s2, C0_SR)
	NOP_0_4
	PTR_L	k1, VPDA_CURUTHREAD(zero)
	PTR_L	k1, UT_EXCEPTION(k1)
	PTR_ADDIU k1, U_EFRAME		# address of exception frame

	.set	reorder
#ifdef STKDEBUG
	.set	noreorder
	MFC0(a0,C0_SR)
	NOP_1_0
	.set reorder
	and	a0, SR_IEC
	beq	a0,zero,noint1
	PANIC("syscall exit with interrupts")
noint1:
#endif
	bne	v0,zero,j_except_exit	# If syscall() returns non-zero, then
					# return through the trap machanism.
#if EVEREST && DEBUG
	lreg	a0,EF_SR(k1)
	jal	isspl0
	bne	v0,zero,97f
	lreg	a2,EF_SR(k1)
	lbu	a3,VPDA_CELSHADOW(zero)
	PANIC("systrap: NOT spl0, SR 0x%x CEL 0x%x");
97:
#endif 
	/* switch to user CPU timer */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	LI	a1,AS_USR_RUN
	jal	ktimer_switch

	lreg	v0,EF_V0(k1)		# u_rval1
	lreg	v1,EF_V1(k1)		# u_rval2
	lreg	fp,EF_FP(k1)		# is register 's8'

	sb	zero,VPDA_KSTACK(zero)	# switching to user stack
	lreg	gp,EF_GP(k1)
	RESTOREAREGS(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	lreg	t0, EF_T0(k1)
	lreg	t1, EF_T1(k1)
	lreg	t2, EF_T2(k1)
	lreg	t3, EF_T3(k1)
#endif

	/* Eventually all R4000's should use this path.
	 * s2 == current C0_SR (so macro can set SR_EXL)
	 */
	.set	noreorder
	RESTORESREGS_SR_EPC(k1, s2)

	lreg	ra,EF_RA(k1)
	.set	noat
	RESTOREFPREGS_TOUSER(AT)	# AT == work reg
	AUTO_CACHE_BARRIERS_DISABLE	# we loose kernel $sp here
	lreg	sp,EF_SP(k1)

	/*
	 * wire in user's tlb entries for wired slots
	 * NOTE: we can use the t registers
	 * We do the slot containing the user's kernel stack first, since
	 * we are pulling info out of the user's upage!
	 * NOTE2: to avoid a race with setup_wired_tlb - we are
	 * carefull to grab the physical address first - then the
	 * virtual - setup_wired_tlb does things in the reverse order
	 * so we're guaranteed that if physical address is 0 the
	 * virtual address is also invalid.
	 */
#if TLBKSLOTS == 0
	/* If we don't wire down 2nd level page maps in the fixed tlb
	 * entries, then we have nothing to restore.
	 * NOTE: Some machines (i.e. TFP) can't wire these entries.
	 */
	eret
#else	/* TLBKSLOTS != 0 */

	DMFC0(t1, C0_TLBHI)		# get tlbpid
	/* Only one pair of page map entries to restore */

	PTR_L	k0, VPDA_CURUTHREAD(zero)	# LDSLOT
	PTR_L	k0, UT_EXCEPTION(k0)	# LDSLOT
	PTE_L	t0, U_USLKSTKLO_1(k0)	# LDSLOT
	PTE_L	k0, U_USLKSTKLO(k0)	# LDSLOT
	TLBLO_FIX_HWBITS(t0)
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(t0, C0_TLBLO_1)		# LDSLOT
	PTR_L	t0, VPDA_CURUTHREAD(zero)	# LDSLOT
	PTR_L	t0, UT_EXCEPTION(t0)	# LDSLOT
	PTR_L	t0, U_USLKSTKHI(t0)	# LDSLOT
	andi	t1, TLBHI_PIDMASK	# LDSLOT
	or	t0, t1
	DMTC0(t0, C0_TLBHI)		# set virtual page number
	li	t0, UKSTKTLBINDEX	# base of wired entries
	mtc0	t0, C0_INX		# set index to wired entry
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO)		# set pfn and access bits
	nop				# NOPSLOT
	c0	C0_WRITEI		# write entry
	NOP_0_4				# NOP gets EPC/TLB into cp0 before eret
	ERET_PATCH(locore_eret_7)	# patchable eret
#endif	/* TLBKSLOTS == 0 */
#endif	/* TFP */
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	.set	at
#ifdef DEBUG
kernelsyscall:
	SPPANIC("kernel did syscall")
	/* NOTREACHED */
#endif /* DEBUG */

jmp_baduk:
	j	_baduk
	/* NOTREACHED */

j_except_exit:
	/* s0 == C0_SR at time of exception
	 * sp == stack to use on reschedule
	 * k1 => eframe
	 */
	j	exception_exit
		
#ifdef CELL
/*
 * syscall_rewind(sysnum)
 *	is used by Cellular IRIX to fake system call entry for the current
 * 	uthread. a0 contains the system call number and a1 must point to
 *	the exception frame. The eframe context must have been set by the
 *	caller. The stack pointer is reset and hence all stack context is
 *	lost. In particular, this is used to complete a remote exec on the
 *	target cell via SYS_rexec_complete.
 */ 
EXPORT(syscall_rewind)
	.set	noreorder
	LI	sp,KERNELSTACK-ARGSAVEFRM	# set initial stack
	lreg	s0,EF_SR(a1)
	and	a2,s0,SR_IMASK|SR_KERN_SET	# kernel base mode, EXL low
	j	to_syscall
	or	a2,SR_IE			# and interrupts enabled.
	/* NOTREACHED */
	.set	reorder
#endif /* CELL */

	END(systrap)

VECTOR(VEC_syscall, M_EXCEPT)
EXL_EXPORT(locore_exl_6)
	PANIC("VEC_syscall")
EXL_EXPORT(elocore_exl_6)
	END(VEC_syscall)

