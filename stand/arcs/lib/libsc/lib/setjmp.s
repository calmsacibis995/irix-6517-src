/*
 * setjmp/longjmp implementation
 *
 * This is identical to libc's except that it doesn't preserve floating
 * point things.
 */

#ident "$Revision: 1.3 $"

#include <ml.h>
#include <regdef.h>
#include <asm.h>
#include <setjmp.h>

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
	sreg	s8,JB_S8*BPREG(a0)
	sreg	t9,JB_T9*BPREG(a0)
	move	v0,zero
	j	ra
	END(setjmp)

/*
 * longjmp(jmp_buf, rval)
 */
LEAF(longjmp)
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
	lreg	s7,JB_S7*BPREG(a0)
	lreg	s8,JB_S8*BPREG(a0)
	lreg	t9,JB_T9*BPREG(a0)
	move	v0,a1			/* return rval */
	bnez	v0,1f			/* unless rval==0 */
	li	v0,1			/* in which case return 1 */
1:	j	ra
	END(longjmp)
