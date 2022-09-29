#ident	"libsk/ml/genasm.s:  $Revision: 1.6 $"

/* 
 * define a passel of register-manipulation routines, providing C code
 *   access to generic machine registers ('generic' means registers
 *   that are common to all mips processors--r2k, r3k, r4k, and tfp).
 * XLEAF resolves a.k.a. names.
 * All set_* routines return the previous value.
 * Processor-specific register routines are defined in libsk/ml/r*kasm.s.
 * This file contains:
 *   user registers: get_pc and get_sp
 *   C0 registers: get_sr/set_sr, get_cause/set_cause, and get_prid
 *   fp registers: get_fpsr, set_fpsr
 *
 * NOTE: These routines were moved here from libsc/lib/libasm.s because
 * they need to use DMFC0/DMTC0 to access coprocessor 0 registers. These
 * macros are different on TFP.
 */

#include	"ml.h"
#include <asm.h>
#include <regdef.h>
#include <sys/fpu.h>
#include <sys/sbd.h>

LEAF(get_SR)
XLEAF(get_sr)
XLEAF(GetSR)
XLEAF(getsr)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)
	nop
	j	ra
	nop
	.set	reorder
	END(get_SR)

LEAF(set_SR)
XLEAF(set_sr)
XLEAF(SetSR)
XLEAF(setsr)
	.set	noreorder
	MFC0(v0,C0_SR)
	nop
	MTC0(a0,C0_SR)
	nop
	j	ra
	nop
	.set	reorder
	END(set_SR)


LEAF(get_CAUSE)
XLEAF(get_cause)
XLEAF(GetCause)
XLEAF(getcause)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_CAUSE)
	nop
	nop
	j	ra
	nop
	.set	reorder
	END(get_CAUSE)

LEAF(set_CAUSE)
XLEAF(set_cause)
XLEAF(SetCause)
XLEAF(setcause)
	.set	noreorder
	MFC0(v0,C0_CAUSE)
	nop
	MTC0(a0,C0_CAUSE)
	nop
	j	ra
	nop
	.set	reorder
	END(set_CAUSE)

LEAF(get_prid)
	.set	noreorder
	MFC0(v0,C0_PRID)
        MFC0(t0,C0_PRID)
        li      t1, C0_IMP_RM5271
        andi    t0, C0_IMPMASK
        srl     t0, C0_IMPSHIFT
        bne     t0, t1, not_r52xx
        nop
        andi    v0, 0x00ff          /* clear out IMP part */
        ori     v0, (C0_IMP_R5000 << C0_IMPSHIFT) /* dummy in R5K */
not_r52xx:
	nop
	j	ra
	nop
	.set	reorder
	END(get_prid)


LEAF(get_FPSR)
XLEAF(getfpsr)
XLEAF(GetFPSR)
XLEAF(get_fpsr)
	cfc1	v0,fpc_csr
	j	ra
	END(get_FPSR)

LEAF(set_FPSR)
XLEAF(setfpsr)
XLEAF(SetFPSR)
XLEAF(set_fpsr)
	.set	noreorder
	cfc1	v0,fpc_csr
	nop
	nop
	.set	reorder
	ctc1	a0,fpc_csr
	j	ra
	END(set_FPSR)

