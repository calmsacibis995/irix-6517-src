#ident	"arcstests/utlbasm.s:  $Revision: 1.2 $"

/*
 * utlbasm.s -- exception handling and assembler for utlb test
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/sbd.h>
#include <fault.h>
#include "ml.h"

	.text
LEAF(set_ctxt)
	.set	noreorder
	mtc0    a0,C0_CTXT
	.set	reorder
	j	ra
	END(set_ctxt)

LEAF(set_tlblo)
	.set	noreorder
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	a0,C0_TLBLO
	nop
	c0      C0_WRITER
	j	ra
	.set	reorder
	END(set_tlblo)

/*
 * UTLB Miss exception vector entrypoint - this address gets put
 * in the SPB as the UTLBMissVector. It is called from the firmware
 * UTLB miss exception vector handler that has been copied to 0x80000000.
 *
 * On entry:
 *     k0 contains the address of this entrypoint
 *     k1 contains the link (ra) to the firmware vector
 */
LEAF(local_utlb_vec)
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
	/* finally call the alternate exception handler
	 */
	jal	local_exception_handler

	j	_resume
	END(exception)
