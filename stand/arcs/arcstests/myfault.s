#ident	"arcstests/myfault.s:  $Revision: 1.8 $"

/*
 * myfault.s -- exception handling model for programs running on libsc
 */

#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/asm.h>
#include <sys/signal.h>		/* for BRK_KERNELBP */

#include <fault.h>
#include <arcs/spb.h>
#include <arcs/debug_block.h>
#include "ml.h"


	.text

/*
 * General exception vector entrypoint - this address gets put
 * in the SPB as the GEVector. It is called from the firmware
 * general exception vector handler has been copied to 0x80000080.
 *
 * On entry:
 *     k0 contains the address of this entrypoint
 *     k1 contains the link (ra) to the firmware vector
 */
LEAF(my_norm_vec)
	.set	noat
	move	k0,AT			# clobber k0
	.set	at
	sw	k0,_at_save
	sw	v0,_v0_save

	/*
	 * Check if this is a "kernel breakpoint" that is to be handled
	 * by the debug monitor
	 */
	.set	noreorder
	mfc0	v0,C0_CAUSE
	nop
	.set	reorder
	and	k0,v0,CAUSE_EXCMASK
	bne	k0,+EXC_BREAK,2f	# not even a break inst
	.set	noreorder
	mfc0	k0,C0_EPC
	.set	reorder
	and	v0,CAUSE_BD
	beq	v0,zero,1f		# not in branch delay slot
	addu	k0,4			# advance to bd slot
1:
	lw	k0,0(k0)		# fetch faulting instruction
	lw	v0,kernel_bp		# bp inst used by symmon
	bne	v0,k0,2f		# not symmon's break inst

	.set	noat
	lw	AT,+SPB_DEBUGADDR	# address of debug block
	beq	AT,zero,2f		# no debug block
	lw	AT,DB_BPOFF(AT)		# breakpoint handler
	beq	AT,zero,2f		# no handler
	lw	v0,_v0_save		# restore v0
	lw	k0,_at_save		# save AT in k0
	j	AT			# enter breakpoint handler
	.set	at
2:
	li	v0,EXCEPT_NORM
	j	exception
	END(my_norm_vec)

/*
 * UTLB Miss exception vector entrypoint - this address gets put
 * in the SPB as the UTLBMissVector. It is called from the firmware
 * UTLB miss exception vector handler that has been copied to 0x80000000.
 *
 * On entry:
 *     k0 contains the address of this entrypoint
 *     k1 contains the link (ra) to the firmware vector
 */
LEAF(my_utlb_vec)
	.set	noat
	move	k0,AT
	.set	at
	sw	k0,_at_save
	sw	v0,_v0_save
	li	v0,EXCEPT_UTLB

/*
 * common exception handling code
 */
XLEAF(exception)
	/*
	 * Save various registers so we can print informative messages
	 * for faults (whether on normal stack or fault stack)
	 */
	sw	v0,_exc_save
	.set	noreorder
	mfc0	v0,C0_EPC
	nop
	sw	v0,_epc_save
	mfc0	v0,C0_SR
	nop
	sw	v0,_sr_save
	mfc0	v0,C0_BADVADDR
	nop
	sw	v0,_badvaddr_save
	mfc0	v0,C0_CAUSE
	nop
	.set	reorder
	sw	v0,_cause_save
	sw	sp,_sp_save
	lw	sp,_fault_sp		# use "fault" stack
	/*
	 * Only save registers if on regular stack
	 * then change mode to fault mode
	 */
	lw	v0,_stack_mode
	sw	v0,_mode_save
	beq	v0,MODE_FAULT,nosave	# was in fault mode
	li	v0,MODE_FAULT
	sw	v0,_stack_mode		# now in fault mode
	lw	v0,_epc_save
	sw	v0,_regs+R_EPC*4
	lw	v0,_sr_save
	sw	v0,_regs+R_SR*4
	lw	v0,_at_save
	sw	v0,_regs+R_AT*4
	lw	v0,_v0_save
	sw	v0,_regs+R_V0*4
	lw	v0,_exc_save
	sw	v0,_regs+R_EXCTYPE*4
	lw	v0,_badvaddr_save
	sw	v0,_regs+R_BADVADDR*4
	lw	v0,_cause_save
	sw	v0,_regs+R_CAUSE*4
	lw	v0,_sp_save
	sw	v0,_regs+R_SP*4
	sw	zero,_regs+R_ZERO*4	# we don't trust anything
	sw	v1,_regs+R_V1*4
	sw	a0,_regs+R_A0*4
	sw	a1,_regs+R_A1*4
	sw	a2,_regs+R_A2*4
	sw	a3,_regs+R_A3*4
	sw	t0,_regs+R_T0*4
	sw	t1,_regs+R_T1*4
	sw	t2,_regs+R_T2*4
	sw	t3,_regs+R_T3*4
	sw	t4,_regs+R_T4*4
	sw	t5,_regs+R_T5*4
	sw	t6,_regs+R_T6*4
	sw	t7,_regs+R_T7*4
	sw	s0,_regs+R_S0*4
	sw	s1,_regs+R_S1*4
	sw	s2,_regs+R_S2*4
	sw	s3,_regs+R_S3*4
	sw	s4,_regs+R_S4*4
	sw	s5,_regs+R_S5*4
	sw	s6,_regs+R_S6*4
	sw	s7,_regs+R_S7*4
	sw	t8,_regs+R_T8*4
	sw	t9,_regs+R_T9*4
	li	k0,0xbad00bad		# make it obvious we can't save this
	sw	k0,_regs+R_K0*4
	sw	k1,_regs+R_K1*4
	sw	fp,_regs+R_FP*4
	sw	gp,_regs+R_GP*4
	sw	ra,_regs+R_RA*4
	mflo	v0
	sw	v0,_regs+R_MDLO*4
	mfhi	v0
	sw	v0,_regs+R_MDHI*4
	.set	noreorder
	mfc0	v0,C0_INX
	nop
	sw	v0,_regs+R_INX*4
	mfc0	v0,C0_RAND		# save just to see it change
	nop
	sw	v0,_regs+R_RAND*4
	mfc0	v0,C0_TLBLO
	nop
	sw	v0,_regs+R_TLBLO*4
	mfc0	v0,C0_TLBHI
	nop
	sw	v0,_regs+R_TLBHI*4
	mfc0	v0,C0_CTXT
	nop
	.set	reorder
	sw	v0,_regs+R_CTXT*4
	lw	v0,_sr_save
	and	v0,SR_CU1
	beq	v0,zero,nosave
	swc1	$f0,_regs+R_F0*4
	swc1	$f1,_regs+R_F1*4
	swc1	$f2,_regs+R_F2*4
	swc1	$f3,_regs+R_F3*4
	swc1	$f4,_regs+R_F4*4
	swc1	$f5,_regs+R_F5*4
	swc1	$f6,_regs+R_F6*4
	swc1	$f7,_regs+R_F7*4
	swc1	$f8,_regs+R_F8*4
	swc1	$f9,_regs+R_F9*4
	swc1	$f10,_regs+R_F10*4
	swc1	$f11,_regs+R_F11*4
	swc1	$f12,_regs+R_F12*4
	swc1	$f13,_regs+R_F13*4
	swc1	$f14,_regs+R_F14*4
	swc1	$f15,_regs+R_F15*4
	swc1	$f16,_regs+R_F16*4
	swc1	$f17,_regs+R_F17*4
	swc1	$f18,_regs+R_F18*4
	swc1	$f19,_regs+R_F19*4
	swc1	$f20,_regs+R_F20*4
	swc1	$f21,_regs+R_F21*4
	swc1	$f22,_regs+R_F22*4
	swc1	$f23,_regs+R_F23*4
	swc1	$f24,_regs+R_F24*4
	swc1	$f25,_regs+R_F25*4
	swc1	$f26,_regs+R_F26*4
	swc1	$f27,_regs+R_F27*4
	swc1	$f28,_regs+R_F28*4
	swc1	$f29,_regs+R_F29*4
	swc1	$f30,_regs+R_F30*4
	swc1	$f31,_regs+R_F31*4
	cfc1	v0,$30
	sw	v0,_regs+R_C1_EIR*4
	cfc1	v0,$31
	sw	v0,_regs+R_C1_SR*4
nosave:
	jal	my_exception_handler
	j	_resume
	END(exception)

/*
 * _resume -- resume execution of mainline code
 */
LEAF(_resume)
	lw	v0,_regs+R_SR*4
	and	v0,SR_CU1
	beq	v0,zero,1f	
	.set	noreorder
	mtc0	v0,C0_SR
	.set	reorder
	lwc1	$f0,_regs+R_F0*4
	lwc1	$f1,_regs+R_F1*4
	lwc1	$f2,_regs+R_F2*4
	lwc1	$f3,_regs+R_F3*4
	lwc1	$f4,_regs+R_F4*4
	lwc1	$f5,_regs+R_F5*4
	lwc1	$f6,_regs+R_F6*4
	lwc1	$f7,_regs+R_F7*4
	lwc1	$f8,_regs+R_F8*4
	lwc1	$f9,_regs+R_F9*4
	lwc1	$f10,_regs+R_F10*4
	lwc1	$f11,_regs+R_F11*4
	lwc1	$f12,_regs+R_F12*4
	lwc1	$f13,_regs+R_F13*4
	lwc1	$f14,_regs+R_F14*4
	lwc1	$f15,_regs+R_F15*4
	lwc1	$f16,_regs+R_F16*4
	lwc1	$f17,_regs+R_F17*4
	lwc1	$f18,_regs+R_F18*4
	lwc1	$f19,_regs+R_F19*4
	lwc1	$f20,_regs+R_F20*4
	lwc1	$f21,_regs+R_F21*4
	lwc1	$f22,_regs+R_F22*4
	lwc1	$f23,_regs+R_F23*4
	lwc1	$f24,_regs+R_F24*4
	lwc1	$f25,_regs+R_F25*4
	lwc1	$f26,_regs+R_F26*4
	lwc1	$f27,_regs+R_F27*4
	lwc1	$f28,_regs+R_F28*4
	lwc1	$f29,_regs+R_F29*4
	lwc1	$f30,_regs+R_F30*4
	lwc1	$f31,_regs+R_F31*4
	lw	v0,_regs+R_C1_EIR*4
	ctc1	v0,$30
	lw	v0,_regs+R_C1_SR*4
	ctc1	v0,$31
1:
	lw	a0,_regs+R_A0*4
	lw	a1,_regs+R_A1*4
	lw	a2,_regs+R_A2*4
	lw	a3,_regs+R_A3*4
	lw	t0,_regs+R_T0*4
	lw	t1,_regs+R_T1*4
	lw	t2,_regs+R_T2*4
	lw	t3,_regs+R_T3*4
	lw	t4,_regs+R_T4*4
	lw	t5,_regs+R_T5*4
	lw	t6,_regs+R_T6*4
	lw	t7,_regs+R_T7*4
	lw	s0,_regs+R_S0*4
	lw	s1,_regs+R_S1*4
	lw	s2,_regs+R_S2*4
	lw	s3,_regs+R_S3*4
	lw	s4,_regs+R_S4*4
	lw	s5,_regs+R_S5*4
	lw	s6,_regs+R_S6*4
	lw	s7,_regs+R_S7*4
	lw	t8,_regs+R_T8*4
	lw	t9,_regs+R_T9*4
	lw	k1,_regs+R_K1*4
	lw	gp,_regs+R_GP*4
	lw	fp,_regs+R_FP*4
	lw	ra,_regs+R_RA*4
	lw	v0,_regs+R_MDLO*4
	mtlo	v0
	lw	v1,_regs+R_MDHI*4
	mthi	v1
	lw	v0,_regs+R_INX*4
	.set	noreorder
	mtc0	v0,C0_INX
	lw	v1,_regs+R_TLBLO*4
	nop
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	v1,C0_TLBLO
	lw	v0,_regs+R_TLBHI*4
	nop
	mtc0	v0,C0_TLBHI
	lw	v1,_regs+R_CTXT*4
	nop
	mtc0	v1,C0_CTXT
	lw	v0,_regs+R_CAUSE*4
	nop
	mtc0	v0,C0_CAUSE		# for software interrupts
	lw	v1,_regs+R_SR*4
	nop
#ifdef R4000
	and	v1,~(SR_KSU_KS|SR_IEC)	# not ready for these yet!
#endif
#ifdef TFP
#error hmmmmmmmmm
#endif
	mtc0	v1,C0_SR
	.set	reorder
	lw	sp,_regs+R_SP*4
	lw	v0,_regs+R_V0*4
	lw	v1,_regs+R_V1*4
	li	k0,MODE_NORMAL
	sw	k0,_stack_mode		# returning to normal stack
	lw	k0,_regs+R_EPC*4
	.set	noat
	.set	noreorder
	lw	AT,_regs+R_AT*4
	j	k1
	nop
	.set	reorder
	.set	at
	END(_resume)

kernel_bp:
	break	BRK_KERNELBP

/*
 * exception state
 */
	BSS(_epc_save,4)		# epc at time of exception
	BSS(_at_save,4)			# at at time of exception
	BSS(_v0_save,4)			# v0 at time of exception
	BSS(_exc_save,4)		# exc at time of exception
	BSS(_badvaddr_save,4)		# badvaddr at time of exception 
	BSS(_cause_save,4)		# cause reg at time of exception
	BSS(_sp_save,4)			# sp at time of exception
	BSS(_mode_save,4)		# prom mode at time of exception
	BSS(_sr_save,4)			# sr at time of exceptions
	BSS(_fault_sp,4)		# stack for nofault handling
	BSS(_stack_mode,4)		# running regular or fault stack
	BSS(_regs,NREGS*4)		# client register contents
