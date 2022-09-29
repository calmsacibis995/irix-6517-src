/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: strlen.s,v 1.8 1994/11/11 19:25:46 jwag Exp $"

#include "sgidefs.h"

/*  Use fast unrolled strlen in all redwood libc's as it's on R4400's and
 * up.  For MIPS1, the normal case we use the old strlen.  The unrolled
 * code does not respect load delay slots that are needed on the R2000 +
 * R3000.
 */
#if _MIPS_ISA != _MIPS_ISA_MIPS1

/* Note that the unrolling you see here is not arbitrary.
 * This loop runs optimally at 1 byte/cycle on TFP, which requires
 * that one branch and one load happen every cycle.  The load must
 * be the value that will be checked next cycle, and that load must
 * not happen if the branch breaks out of the loop.  This requirement
 * is what forces the structure beq, (delay-slot), lbu.  Because the
 * routine cannot load speculatively past the end of the string,
 * the loads cannot be put in the delay slots of the branches.

 * At least three replications (and three pointers) are necessary so
 * that the pointer increment can happen two cycles earlier than the
 * lbu.  Note that since the pointer increment sits in the branch delay
 * slot ahead of the lbu, this requires three replications instead of
 * the usual two.  I found four replications easier to code, but three
 * should be quite straightforward.

 * Note that we take one extra cycle going from the bottom of the loop
 * back up to the top, since the load cannot be dispatched with the
 * branch in this case.  This cycle accounts for one in nine cycles of
 * steady state execution of this loop (11%).  If the number of
 * replications was reduces to four in the loop body, that cycle would
 * account for one in five cycles (20%).

 * A final word about branch misprediction misses on TFP: Unless this
 * function is invoked from within a loop with just one invokation, the
 * return at the end will always mispredict.  Any string of 12 or more
 * characters will suffer 2 more branch mispredicts in the loop.  Any
 * string of less than 12 characters will suffer, on average, 1 more
 * mispredict, unless it is the same length as the previous invokation,
 * in which case it suffers just the mispredict from the return.  Each
 * branch misprediction is 3 or 4 cycles.
 */

#include <sys/asm.h>
#include <sys/regdef.h>

	.set	noreorder
	.align	4
LEAF(strlen)
	lbu	v1,0(a0)	# [0]
	beq	v1,zero,2f	# [1]
	PTR_ADD	v0,a0,0		# [1]
	lbu	v1,1(a0)	# [1]
	beq	v1,zero,3f	# [3]
	PTR_ADD	a1,a0,1		# [3]
	lbu	v1,2(a0)	# [3]
	beq	v1,zero,4f	# [4]
	PTR_ADD	a2,a0,2		# [4]
	lbu	v1,3(a0)	# [4]
	beq	v1,zero,5f	# [5]
	PTR_ADD	a3,a0,3		# [5]
	
1:	lbu	v1,4(v0)	# [0]
	beq	v1,zero,2f	# [1]
	PTR_ADD	v0,4		# [1]
	lbu	v1,4(a1)	# [1]
	beq	v1,zero,3f	# [2]
	PTR_ADD	a1,4		# [2]
	lbu	v1,4(a2)	# [2]
	beq	v1,zero,4f	# [3]
	PTR_ADD	a2,4		# [3]
	lbu	v1,4(a3)	# [3]
	beq	v1,zero,5f	# [4]
	PTR_ADD	a3,4		# [4]
	lbu	v1,4(v0)	# [4]
	beq	v1,zero,2f	# [5]
	PTR_ADD	v0,4		# [5]
	lbu	v1,4(a1)	# [5]
	beq	v1,zero,3f	# [6]
	PTR_ADD	a1,4		# [6]
	lbu	v1,4(a2)	# [6]
	beq	v1,zero,4f	# [7]
	PTR_ADD	a2,4		# [7]
	lbu	v1,4(a3)	# [7]
	bne	v1,zero,1b	# [8]
	PTR_ADD	a3,4		# [8]

5:	j	ra
	PTR_SUBU	v0,a3,a0
	
2:	j	ra
	PTR_SUBU	v0,v0,a0

3:	j	ra
	PTR_SUBU	v0,a1,a0

4:	j	ra
	PTR_SUBU	v0,a2,a0

	.set	reorder
	.end	strlen
#else

#include <sys/asm.h>
#include <sys/regdef.h>

LEAF(strlen)
	PTR_SUBU	v0,a0,1
1:	lbu	v1,1(v0)
	PTR_ADD	v0,1
	bne	v1,zero,1b
	PTR_SUBU	v0,v0,a0
	j	ra
	.end	strlen

#endif
