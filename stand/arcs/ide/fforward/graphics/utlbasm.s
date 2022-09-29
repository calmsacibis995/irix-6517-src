#ident	"arcstests/utlbasm.s:  $Revision: 1.11 $"

/*
 * utlbasm.s -- exception handling and assembler for utlb test
 */

#include <ml.h>
#include <sys/sbd.h>
#include <fault.h>

	.text

#if R4000 || R10000
LEAF(set_ctxt)
	.set	noreorder
	DMTC0	(a0,C0_CTXT)
	.set	reorder
	j	ra
	END(set_ctxt)

LEAF(set_tlblo)
	.set	noreorder
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0	(a0,C0_TLBLO)
	nop
	c0      C0_WRITER
	j	ra
	.set	reorder
	END(set_tlblo)
#endif

/*
 * UTLB Miss exception vector entrypoint - this address gets put
 * in the SPB as the UTLBMissVector. It is called from the firmware
 * UTLB miss exception vector handler that has been copied to 0x80000000.
 *
 * On entry:
 *     k0 contains the address of this entrypoint
 *     k1 contains the link (ra) to the firmware vector
 */
LEAF(local_ge_vec)
	.set	noat
	_get_gpda(k0,k1)			# k0 <- pda
	sreg	AT,GPDA_AT_SV(k0)
	.set	at
	sreg	v0,GPDA_V0_SV(k0)
	sreg	k1,GPDA_RTN_ADDR_SV(k0)
	li	v0,EXCEPT_NORM
	/*FALLSTHROUGH*/

/*
 * common exception handling code
 */
XLEAF(exception)
	/*
	 * Save various registers so we can print informative messages
	 * for faults (whether on normal stack or fault stack)
	 */
	sreg	v0,GPDA_EXC_SV(k0)
	.set	noreorder
	CACHE_BARRIER	# should not be needed, but this is not perf code
	DMFC0	(v0,C0_EPC)
	nop
	sreg	v0,GPDA_EPC_SV(k0)
	MFC0	(v0,C0_SR)
	nop
	sreg	v0,GPDA_SR_SV(k0)
	DMFC0	(v0,C0_BADVADDR)
	nop
	sreg	v0,GPDA_BADVADDR_SV(k0)
	MFC0	(v0,C0_CAUSE)
	nop
	.set	reorder
	sreg	v0,GPDA_CAUSE_SV(k0)
	sreg	sp,GPDA_SP_SV(k0)
	lreg	sp,GPDA_FAULT_SP(k0)	# use "fault" stack
	/*
	 * Only save registers if on regular stack
	 * then change mode to fault mode
	 */
	lreg	v0,GPDA_STACK_MODE(k0)
	sreg	v0,GPDA_MODE_SV(k0)
	beq	v0,MODE_FAULT,nosave	# was in fault mode
	li	v0,MODE_FAULT
	sreg	v0,GPDA_STACK_MODE(k0)	# now in fault mode
	# Load K1 with address of pda[0].regs[0]
	lreg	k1,GPDA_REGS(k0)
	#
	lreg	v0,GPDA_EPC_SV(k0)
	sreg	v0,ROFF(R_EPC)(k1)
	lreg	v0,GPDA_SR_SV(k0)
	sreg	v0,ROFF(R_SR)(k1)
	lreg	v0,GPDA_AT_SV(k0)
	sreg	v0,ROFF(R_AT)(k1)
	lreg	v0,GPDA_V0_SV(k0)
	sreg	v0,ROFF(R_V0)(k1)
	lreg	v0,GPDA_EXC_SV(k0)
	sreg	v0,ROFF(R_EXCTYPE)(k1)
	lreg	v0,GPDA_BADVADDR_SV(k0)
	sreg	v0,ROFF(R_BADVADDR)(k1)
	lreg	v0,GPDA_CAUSE_SV(k0)
	sreg	v0,ROFF(R_CAUSE)(k1)
	lreg	v0,GPDA_SP_SV(k0)
	sreg	v0,ROFF(R_SP)(k1)
	sreg	zero,ROFF(R_ZERO)(k1)	# we don't trust anything
	sreg	v1,ROFF(R_V1)(k1)
	sreg	a0,ROFF(R_A0)(k1)
	sreg	a1,ROFF(R_A1)(k1)
	sreg	a2,ROFF(R_A2)(k1)
	sreg	a3,ROFF(R_A3)(k1)
	sreg	t0,ROFF(R_T0)(k1)
	sreg	t1,ROFF(R_T1)(k1)
	sreg	t2,ROFF(R_T2)(k1)
	sreg	t3,ROFF(R_T3)(k1)
	sreg	ta0,ROFF(R_TA0)(k1)
	sreg	ta1,ROFF(R_TA1)(k1)
	sreg	ta2,ROFF(R_TA2)(k1)
	sreg	ta3,ROFF(R_TA3)(k1)
	sreg	s0,ROFF(R_S0)(k1)
	sreg	s1,ROFF(R_S1)(k1)
	sreg	s2,ROFF(R_S2)(k1)
	sreg	s3,ROFF(R_S3)(k1)
	sreg	s4,ROFF(R_S4)(k1)
	sreg	s5,ROFF(R_S5)(k1)
	sreg	s6,ROFF(R_S6)(k1)
	sreg	s7,ROFF(R_S7)(k1)
	sreg	t8,ROFF(R_T8)(k1)
	sreg	t9,ROFF(R_T9)(k1)
	li	v0,0xbad00bad		# make it obvious we can't save this
	sreg	v0,ROFF(R_K0)(k1)
	li	v0,0xbad11bad
	sreg	k1,ROFF(R_K1)(k1)
	sreg	fp,ROFF(R_FP)(k1)
	sreg	gp,ROFF(R_GP)(k1)
	sreg	ra,ROFF(R_RA)(k1)
	mflo	v0
	sreg	v0,ROFF(R_MDLO)(k1)
	mfhi	v0
	sreg	v0,ROFF(R_MDHI)(k1)
	.set	noreorder
#if R4000 || R10000
	CACHE_BARRIER	# should not be needed, but this is not perf code
	DMFC0	(v0,C0_INX)
	nop
	sreg	v0,ROFF(R_INX)(k1)
	DMFC0	(v0,C0_RAND)		# save just to see it change
	nop
	sreg	v0,ROFF(R_RAND)(k1)
	DMFC0(v0,C0_TLBLO_1)
	nop
	sreg	v0,ROFF(R_TLBLO1)(k1)
	DMFC0	(v0,C0_CTXT)
	nop
	sreg	v0,ROFF(R_CTXT)(k1)
#endif
	DMFC0	(v0,C0_TLBLO)
	nop
	sreg	v0,ROFF(R_TLBLO)(k1)
	DMFC0	(v0,C0_TLBHI)
	nop
	sreg	v0,ROFF(R_TLBHI)(k1)
	.set	reorder
	lreg	v0,GPDA_SR_SV(k0)
	and	v0,SR_CU1
	beq	v0,zero,nosave
        swc1    $f0,ROFF(R_F0)(k1)
        swc1    $f1,ROFF(R_F1)(k1)
        swc1    $f2,ROFF(R_F2)(k1)
        swc1    $f3,ROFF(R_F3)(k1)
        swc1    $f4,ROFF(R_F4)(k1)
        swc1    $f5,ROFF(R_F5)(k1)
        swc1    $f6,ROFF(R_F6)(k1)
        swc1    $f7,ROFF(R_F7)(k1)
        swc1    $f8,ROFF(R_F8)(k1)
        swc1    $f9,ROFF(R_F9)(k1)
        swc1    $f10,ROFF(R_F10)(k1)
        swc1    $f11,ROFF(R_F11)(k1)
        swc1    $f12,ROFF(R_F12)(k1)
        swc1    $f13,ROFF(R_F13)(k1)
        swc1    $f14,ROFF(R_F14)(k1)
        swc1    $f15,ROFF(R_F15)(k1)
        swc1    $f16,ROFF(R_F16)(k1)
        swc1    $f17,ROFF(R_F17)(k1)
        swc1    $f18,ROFF(R_F18)(k1)
        swc1    $f19,ROFF(R_F19)(k1)
        swc1    $f20,ROFF(R_F20)(k1)
        swc1    $f21,ROFF(R_F21)(k1)
        swc1    $f22,ROFF(R_F22)(k1)
        swc1    $f23,ROFF(R_F23)(k1)
        swc1    $f24,ROFF(R_F24)(k1)
        swc1    $f25,ROFF(R_F25)(k1)
        swc1    $f26,ROFF(R_F26)(k1)
        swc1    $f27,ROFF(R_F27)(k1)
        swc1    $f28,ROFF(R_F28)(k1)
        swc1    $f29,ROFF(R_F29)(k1)
        swc1    $f30,ROFF(R_F30)(k1)
        swc1    $f31,ROFF(R_F31)(k1)
	cfc1	v0,$30
	sreg	v0,ROFF(R_C1_EIR)(k1)
	cfc1	v0,$31
	sreg	v0,ROFF(R_C1_SR)(k1)
nosave:
	/* finally call the alternate exception handler
	 */
	jal	local_exception_handler

	j	_resume
	END(local_ge_vec)
