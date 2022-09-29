/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  ckpt_grow_stack.s 1.1 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>

LEAF(ckpt_grow_stack)
	move	t0, sp			# save stack pointer
	move	sp, a0			# set desired stack pointer
	lw	v0,0(sp)		# reference stack
	move	sp, t0			# restore stack pointer
	j	ra
	END(ckpt_grow_stack)
