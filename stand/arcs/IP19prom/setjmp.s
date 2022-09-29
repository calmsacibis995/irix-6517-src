/*
 * setjmp/longjmp implementation
 *
 * This is identical to libc's except that it doesn't preserve floating
 * point things.
 */

#ident "$Revision: 1.5 $"

#include <regdef.h>
#include <asm.h>
#include <setjmp.h>
#include <sys/sbd.h>
#include "ip19prom.h"
#include "pod_failure.h"

/*
 * setjmp(jmp_buf) -- save current context for non-local goto's
 * return 0
 */
LEAF(setjmp)
	sw	ra,JB_PC*4(a0)
	sw	sp,JB_SP*4(a0)
	sw	fp,JB_FP*4(a0)
	sw	s0,JB_S0*4(a0)
	sw	s1,JB_S1*4(a0)
	sw	s2,JB_S2*4(a0)
	sw	s3,JB_S3*4(a0)
	sw	s4,JB_S4*4(a0)
	sw	s5,JB_S5*4(a0)
	sw	s6,JB_S6*4(a0)
	sw	s7,JB_S7*4(a0)
	move	v0,zero
	j	ra
	END(setjmp)

/*
 * longjmp(jmp_buf, rval)
 */
LEAF(longjmp)
	lw	ra,JB_PC*4(a0)
	lw	sp,JB_SP*4(a0)
	lw	fp,JB_FP*4(a0)
	lw	s0,JB_S0*4(a0)
	lw	s1,JB_S1*4(a0)
	lw	s2,JB_S2*4(a0)
	lw	s3,JB_S3*4(a0)
	lw	s4,JB_S4*4(a0)
	lw	s5,JB_S5*4(a0)
	lw	s6,JB_S6*4(a0)
	lw	s7,JB_S7*4(a0)
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
	mfc1	t0, NOFAULT_REG 
	nop
	sw	t0, 0(a1)		# Save previous fault buffer

	mtc1	a0, NOFAULT_REG 
	nop
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
	j	ra	
	mtc1	a0, NOFAULT_REG 
	END(restorefault)

