/*
 * setjmp/longjmp implementation
 *
 * This is identical to libc's except that it doesn't preserve floating
 * point things.
 */

#ident "$Revision: 1.1 $"

#include <sys/regdef.h>
#include <asm.h>
#include <setjmp.h>
#include <sys/sbd.h>
#include "ip25prom.h"
#include "pod_failure.h"

/*
 * setjmp(jmp_buf) -- save current context for non-local goto's
 * return 0
 */
LEAF(setjmp)
	sd	ra,JB_PC*8(a0)
	sd	sp,JB_SP*8(a0)
	sd	fp,JB_FP*8(a0)
	sd	s0,JB_S0*8(a0)
	sd	s1,JB_S1*8(a0)
	sd	s2,JB_S2*8(a0)
	sd	s3,JB_S3*8(a0)
	sd	s4,JB_S4*8(a0)
	sd	s5,JB_S5*8(a0)
	sd	s6,JB_S6*8(a0)
	sd	s7,JB_S7*8(a0)
	move	v0,zero
	j	ra
	END(setjmp)

/*
 * longjmp(jmp_buf, rval)
 */
LEAF(longjmp)
	ld	ra,JB_PC*8(a0)
	ld	sp,JB_SP*8(a0)
	ld	fp,JB_FP*8(a0)
	ld	s0,JB_S0*8(a0)
	ld	s1,JB_S1*8(a0)
	ld	s2,JB_S2*8(a0)
	ld	s3,JB_S3*8(a0)
	ld	s4,JB_S4*8(a0)
	ld	s5,JB_S5*8(a0)
	ld	s6,JB_S6*8(a0)
	ld	s7,JB_S7*8(a0)
	move	v0,a1			/* return rval */
	bnez	v0,1f			/* unless rval==0 */
	li	v0,1			/* in which case return 1 */
1:	j	ra
	END(longjmp)

/*
 * setfault(jump_buf, *old_buf)
 * 	If jump_buf is non-zero, sets up the jumpbuf and switches
 *	on exception handling.
 */
LEAF(setfault)
	.set	noreorder
	.set	noat
	DMFBR(t0, BR_NOFAULT)
	sd	t0, 0(a1)		# Save previous fault buffer
	DMTBR(a0, BR_NOFAULT)
	bnez	a0, setjmp 
	nop
	j	ra
	nop
	END(setfault)

/*
 * restorefault(old_buf)
 *	Simply sets NOFAULT_REG to the value passed.  Used to restore 
 * 	previous fault buffer value.
 */
LEAF(restorefault)
	.set	noreorder
	DMTBR(a0, BR_NOFAULT)
	j	ra
	nop
	END(restorefault)

