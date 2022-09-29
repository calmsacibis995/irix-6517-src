/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * R4000 atomic operations based on link-load, store-conditional instructions.
 */

#ident	"$Revision: 1.1 $"

#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/fpu.h"
#include "sys/softfp.h"
#include "sys/signal.h"
#include "sys/sbd.h"

/*
 * Implementation of fetch-and-add.
 * a0 - address of 32-bit integer to operate on
 * a1 - value to add to integer
 * new value returned in v0
 */
LEAF(atomicAddInt)
XLEAF(atomicAddUint)
	.set 	noreorder

	# Load the old value
1:
	ll	t2,0(a0)
	addu	t1,t2,a1

	# Try to set the new one
	sc	t1,0(a0)	
	beq	t1,zero,1b

	# set return value in branch-delay slot
	addu	v0,t2,a1

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicAddInt)

/*
 * Implementation of long-word fetch-and-add.
 * a0 - address of 32- or64-bit integer to operate on
 * a1 - value to add to integer
 * new value returned in v0
 */
LEAF(atomicAddLong)
XLEAF(atomicAddUlong)
	.set 	noreorder

	# Load the old value
1:
	LONG_LL	t2,0(a0)
	LONG_ADDU t1,t2,a1

	# Try to set the new one
	LONG_SC	t1,0(a0)	
	beq	t1,zero,1b

	# set return value in branch-delay slot
	LONG_ADDU v0,t2,a1

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicAddLong)

/*
 * Atomically set bits in a word (atomic version of "word |= arg;")
 * a0 - address of 32-bit integer to operate on
 * a1 - value to 'or' into integer
 * returns old value
 */
LEAF(atomicSetInt)
XLEAF(atomicSetUint)
	.set 	noreorder

	# Load the old value
1:
	ll	v0,0(a0)
	or	t2,v0,a1

	# Try to set the new one
	sc	t2,0(a0)	
	beq	t2,zero,1b
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicSetInt)

/*
 * Atomically set bits in a long (atomic version of "long |= arg;")
 * a0 - address of long to operate on
 * a1 - value to 'or' into integer
 * returns old value
 */
LEAF(atomicSetLong)
XLEAF(atomicSetUlong)
	.set 	noreorder

	# Load the old value
1:
	LONG_LL	v0,0(a0)
	or	t2,v0,a1

	# Try to set the new one
	LONG_SC	t2,0(a0)	
	beq	t2,zero,1b
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicSetLong)


/*
 * Atomically clear bits in a word (atomic version of "word &= ~arg;")
 * a0 - address of 32-bit integer to operate on
 * a1 - mask of bits to clear in integer
 * returns old value
 */
LEAF(atomicClearInt)
XLEAF(atomicClearUint)
	.set 	noreorder

	# Negate argument
	nor     a1,a1,zero

	# Load the old value
1:
	ll	v0,0(a0)
	and     t2,v0,a1

	# Try to set the new one
	sc	t2,0(a0)	
	beq	t2,zero,1b
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicClearInt)

/*
 * Atomically clear bits in a long (atomic version of "long &= ~arg;")
 * a0 - address of long to operate on
 * a1 - mask of bits to clear in integer
 * returns old value
 */
LEAF(atomicClearLong)
XLEAF(atomicClearUlong)
	.set 	noreorder

	# Negate argument
	nor     a1,a1,zero

	# Load the old value
1:
	LONG_LL	v0,0(a0)
	and     t2,v0,a1

	# Try to set the new one
	LONG_SC	t2,0(a0)	
	beq	t2,zero,1b
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicClearLong)




