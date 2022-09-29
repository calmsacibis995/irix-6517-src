/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Assembler routines for debugging spinlocks.
 */

#ident	"$Revision: 1.4 $"

#include <ml/ml.h>

/*
 * try_spinlock(lock_t *lock);
 *
 * Returns 1 if lock acquired, 0 otherwise.
 */
LEAF(try_spinlock)
	.set	noreorder
	# a0 - contains address of pointer-sized lock
	# v0 - returns 1 on success, 0 on failure

	CACHE_BARRIER		# top factor, on retries of sc a0 is ok
1:	LOCK_LL	v0,0(a0)	# see if lock is busy
	andi	ta1,v0,1
	bne	ta1,zero,2f
	ori	v0,1

	# Try to mark lock busy
	AUTO_CACHE_BARRIERS_DISABLE
	LOCK_SC	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop
	AUTO_CACHE_BARRIERS_ENABLE

	# We have the lock.
	j	ra
	nop

2:	j	ra
	move	v0,zero		# returns failure
	.set	reorder
	END(try_spinlock)

/*
 *	int nested_bittrylock(uint *lp, uint lock-bit);
 */
LEAF(try_bitlock)
	.set	noreorder
	# See if lock is busy.
	CACHE_BARRIER		# top factor, on retries of sc a0 is ok
1:	ll	v0,0(a0)
	and	ta1,v0,a1
	bne	ta1,zero,2f
	or	v0,a1

	AUTO_CACHE_BARRIERS_DISABLE
	# Try to mark lock busy
	sc	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop
	AUTO_CACHE_BARRIERS_ENABLE

	# We have the lock.
	j	ra
	nop

2: 	# trylock failed.
	j	ra
	move	v0,zero		# returns failure
	.set	reorder
	END(try_bitlock)	

/*
 *	int nested_64bittrylock(__ulong64_t *lp, __ulong64_t lock-bit);
 */
LEAF(try_64bitlock)
	.set	noreorder
	# See if lock is busy.
	CACHE_BARRIER		# top factor, on retries of sc a0 is ok
1:	lld	v0,0(a0)
	and	ta1,v0,a1
	bne	ta1,zero,2f
	or	v0,a1

	# Try to mark lock busy
	AUTO_CACHE_BARRIERS_DISABLE
	scd	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop
	AUTO_CACHE_BARRIERS_ENABLE

	# We have the lock.
	j	ra
	nop

2: 	# trylock failed.
	j	ra
	move	v0,zero		# returns failure
	.set	reorder
	END(try_64bitlock)	
