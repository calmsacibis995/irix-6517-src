/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: setjmp.s,v 1.11 1995/07/25 00:54:20 jwag Exp $"

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * The previous signal state is NOT restored.
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/syscall.h>
#include <setjmp.h>
#include <sys/fpu.h>

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	.align	4
LEAF(setjmp)
XLEAF(_setjmp)
	.set	noreorder
	cfc1	v0,fpc_csr
	REG_S	ra,JB_PC*SZREG(a0)
	REG_S	sp,JB_SP*SZREG(a0)
	REG_S	fp,JB_FP*SZREG(a0)
	REG_S	s0,JB_S0*SZREG(a0)
	REG_S	s1,JB_S1*SZREG(a0)
	REG_S	s2,JB_S2*SZREG(a0)
	REG_S	s3,JB_S3*SZREG(a0)
	REG_S	s4,JB_S4*SZREG(a0)
	REG_S	s5,JB_S5*SZREG(a0)
	REG_S	s6,JB_S6*SZREG(a0)
	REG_S	s7,JB_S7*SZREG(a0)
	.set	reorder
	REG_S	v0,+JB_FPC_CSR*SZREG(a0)
	move	v0,zero
	/*
	 * Since this is compiled -mips2 we must explicitly store each
	 * register part - note that the parts are 'backwards'
	 */
#if NEVER
	s.d	$f20,+JB_F20*4(a0)
	s.d	$f22,+JB_F22*4(a0)
	s.d	$f24,+JB_F24*4(a0)
	s.d	$f26,+JB_F26*4(a0)
	s.d	$f28,+JB_F28*4(a0)
	s.d	$f30,+JB_F30*4(a0)
#endif
	swc1	$f21,+JB_F20*4(a0)
	swc1	$f20,4+JB_F20*4(a0)
	swc1	$f23,+JB_F22*4(a0)
	swc1	$f22,4+JB_F22*4(a0)
	swc1	$f25,+JB_F24*4(a0)
	swc1	$f24,4+JB_F24*4(a0)
	swc1	$f27,+JB_F26*4(a0)
	swc1	$f26,4+JB_F26*4(a0)
	swc1	$f29,+JB_F28*4(a0)
	swc1	$f28,4+JB_F28*4(a0)
	swc1	$f31,+JB_F30*4(a0)
	swc1	$f30,4+JB_F30*4(a0)
	j	ra
	END(setjmp)

/*
 * longjmp(jmp_buf, retval)
 */
	.align	4
LEAF(longjmp)
XLEAF(_longjmp)
	.set	noreorder
	REG_L	v0,+JB_FPC_CSR*SZREG(a0)
	REG_L	ra,JB_PC*SZREG(a0)
	REG_L	sp,JB_SP*SZREG(a0)
	REG_L	s0,JB_S0*SZREG(a0)
	ctc1	v0,fpc_csr
	move	v0,a1			/* return retval */
	.set	reorder
	REG_L	s1,JB_S1*SZREG(a0)
	REG_L	s2,JB_S2*SZREG(a0)
	REG_L	s3,JB_S3*SZREG(a0)
	REG_L	s4,JB_S4*SZREG(a0)
	REG_L	s5,JB_S5*SZREG(a0)
	REG_L	s6,JB_S6*SZREG(a0)
	REG_L	s7,JB_S7*SZREG(a0)
	REG_L	fp,JB_FP*SZREG(a0)
#if NEVER
	l.d	$f20,+JB_F20*4(a0)
	l.d	$f22,+JB_F22*4(a0)
	l.d	$f24,+JB_F24*4(a0)
	l.d	$f26,+JB_F26*4(a0)
	l.d	$f28,+JB_F28*4(a0)
	l.d	$f30,+JB_F30*4(a0)
#endif
	lwc1	$f21,+JB_F20*4(a0)
	lwc1	$f20,4+JB_F20*4(a0)
	lwc1	$f23,+JB_F22*4(a0)
	lwc1	$f22,4+JB_F22*4(a0)
	lwc1	$f25,+JB_F24*4(a0)
	lwc1	$f24,4+JB_F24*4(a0)
	lwc1	$f27,+JB_F26*4(a0)
	lwc1	$f26,4+JB_F26*4(a0)
	lwc1	$f29,+JB_F28*4(a0)
	lwc1	$f28,4+JB_F28*4(a0)
	lwc1	$f31,+JB_F30*4(a0)
	lwc1	$f30,4+JB_F30*4(a0)
	move	t9, ra			/* for PIC - if user wants to
					 * change pc ..
					 */
	bnez	v0,1f			/* unless retval==0 */
	li	v0,1			/* in which case return 1 */
1:	j	ra
	END(longjmp)

#else /*  (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */
	.align	4
LEAF(setjmp)
XLEAF(_setjmp)
	.set 	noreorder
	cfc1	v0,fpc_csr
	REG_S	ra,JB_PC*SZREG(a0)
	REG_S	sp,JB_SP*SZREG(a0)
	REG_S	s0,JB_S0*SZREG(a0)
	REG_S	s1,JB_S1*SZREG(a0)
	REG_S	s2,JB_S2*SZREG(a0)
	REG_S	s3,JB_S3*SZREG(a0)
	REG_S	s4,JB_S4*SZREG(a0)
	REG_S	s5,JB_S5*SZREG(a0)
	REG_S	s6,JB_S6*SZREG(a0)
	REG_S	s7,JB_S7*SZREG(a0)
	REG_S	s8,JB_S8*SZREG(a0)
	REG_S	gp,JB_GP*SZREG(a0)
	REG_S	v0,+JB_FPC_CSR*SZREG(a0)
	move	v0,zero
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
	s.d	$f24,+JB_F24*SZREG(a0)
	s.d	$f25,+JB_F25*SZREG(a0)
	s.d	$f26,+JB_F26*SZREG(a0)
	s.d	$f27,+JB_F27*SZREG(a0)
	s.d	$f28,+JB_F28*SZREG(a0)
	s.d	$f29,+JB_F29*SZREG(a0)
	s.d	$f30,+JB_F30*SZREG(a0)
#else /* _MIPS_SIM_NABI32 */
	s.d	$f20,+JB_F20*SZREG(a0)
	s.d	$f22,+JB_F22*SZREG(a0)
	s.d	$f24,+JB_F24*SZREG(a0)
	s.d	$f26,+JB_F26*SZREG(a0)
	s.d	$f28,+JB_F28*SZREG(a0)
	s.d	$f30,+JB_F30*SZREG(a0)
#endif
	j	ra
	s.d	$f31,+JB_F31*SZREG(a0)
	.set	reorder
	END(setjmp)

/*
 * longjmp(jmp_buf, retval)
 */
	.align	4
LEAF(longjmp)
XLEAF(_longjmp)
	.set	noreorder
	REG_L	v0,+JB_FPC_CSR*SZREG(a0)
	REG_L	ra,JB_PC*SZREG(a0)
	REG_L	sp,JB_SP*SZREG(a0)
	REG_L	s0,JB_S0*SZREG(a0)
	ctc1	v0,fpc_csr
	move	v0,a1			/* return retval */
	REG_L	s1,JB_S1*SZREG(a0)
	REG_L	s2,JB_S2*SZREG(a0)
	REG_L	s3,JB_S3*SZREG(a0)
	REG_L	s4,JB_S4*SZREG(a0)
	REG_L	s5,JB_S5*SZREG(a0)
	REG_L	s6,JB_S6*SZREG(a0)
	REG_L	s7,JB_S7*SZREG(a0)
	REG_L	s8,JB_S8*SZREG(a0)
	REG_L	gp,JB_GP*SZREG(a0)
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
	l.d	$f24,+JB_F24*SZREG(a0)
	l.d	$f25,+JB_F25*SZREG(a0)
	l.d	$f26,+JB_F26*SZREG(a0)
	l.d	$f27,+JB_F27*SZREG(a0)
	l.d	$f28,+JB_F28*SZREG(a0)
	l.d	$f29,+JB_F29*SZREG(a0)
	l.d	$f30,+JB_F30*SZREG(a0)
#else /* _MIPS_SIM_NABI32 */
	l.d	$f20,+JB_F20*SZREG(a0)
	l.d	$f22,+JB_F22*SZREG(a0)
	l.d	$f24,+JB_F24*SZREG(a0)
	l.d	$f26,+JB_F26*SZREG(a0)
	l.d	$f28,+JB_F28*SZREG(a0)
	l.d	$f30,+JB_F30*SZREG(a0)
#endif
	bnez	v0,1f			/* unless retval==0 */
	move	t9, ra			/* for PIC - if user wants to
					 * change pc ..
					 */

	li	v0,1			/* in which case return 1 */
1:	j	ra
	l.d	$f31,+JB_F31*SZREG(a0)
	.set	reorder
	END(longjmp)
#endif
