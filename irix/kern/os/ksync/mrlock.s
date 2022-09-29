/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.22 $"

#include <ml/ml.h>

#define	MR_LL	ll
#define	MR_SC	sc

/*
 * int mrcheckupd(mrlock_t *)
 *
 * Atomically:
 *	if (!(mrp->mr_lbits & MR_UPD))
 *		atomicAddUint(&mrp->mr_lbits,MR_ACCINC-MR_WAITINC);
 *
 * Return: MR_UPD if failed, else 0
 */
LEAF(mrcheckupd)
	.set 	noreorder
	CACHE_BARRIER_AT(MR_LBITS,a0)		# top factor cache barrier for a0

1:	MR_LL	t2,MR_LBITS(a0)
	li	t3,MR_UPD
	li	t0,MR_WAITINC
	addi	t2,MR_ACCINC	# we are now using for access
	sub	t2,t2,t0	# remove one waiter
	and	v0,t2,t3	# t3 == MR_UPD
	bne	v0,zero,11f
	nop

	AUTO_CACHE_BARRIERS_DISABLE
	MR_SC	t2,MR_LBITS(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

11:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 23 NOP's here since we already have 9 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop;
#endif
	j	ra
	nop
	.set	reorder
END(mrcheckupd)

/*
 * int mr_tryaccess_noinherit(mrlock_t *)
 *
 * Attempts to acquire access rights, does not support priority inheritance
 *
 * Returns 1 on success; 0 on failure.
 */
LEAF(mr_tryaccess_noinherit)
#ifndef SEMAMETER
XLEAF(mrtryaccess_noinherit)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(MR_LBITS,a0)	# top factor cache barrier for a0

	# Return failure iff: another thread has this lock for update,
	# or the access count has already hit the high-water mark
	# (the bit just above MR_ACCMAX is reserved to always be zero so
	# the access count can never overflow to the control bits).

1:	MR_LL	t2,MR_LBITS(a0)
	li	t1,MR_UPD|MR_ACCMAX
	and	t0,t2,t1
	bne	t0,zero,2f
	addi	v0,t2,MR_ACCINC

	# Try to increment the MR_ACC ``counter''
	# v0 will be set to 1 on success, 0 on failure

	AUTO_CACHE_BARRIERS_DISABLE
	MR_SC	v0,MR_LBITS(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b	# sc failed
#else
	beq	v0,zero,1b	# sc failed
#endif
	nop
	j	ra
	li	v0,1		# BDSLOT

2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 26 NOP's here since we already have 6 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop;
#endif
 	j	ra
	move	v0,zero		# BDSLOT
	.set	reorder
END(mr_tryaccess_noinherit)

/*
 * int mr_tryupdate_noinherit(mrlock_t *)
 *
 * Try to acquire for update, does not support priority inheritance.
 *
 * Returns 1 on success; 0 on failure.
 */
LEAF(mr_tryupdate_noinherit)
#ifndef SEMAMETER
XLEAF(mrtryupdate_noinherit)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(MR_LBITS,a0)	# top factor cache barrier for a0

	# Return failure iff: another thread has this lock for
	# update, or any threads have the lock for access.

1:	MR_LL	v0,MR_LBITS(a0)
	li	t1,MR_UPD|MR_ACC
	and	t0,v0,t1
	bne	t0,zero,2f

	ori	v0,MR_UPD	# turn on update bit

	AUTO_CACHE_BARRIERS_DISABLE
	MR_SC	v0,MR_LBITS(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b	# sc failed
#else
	beq	v0,zero,1b	# sc failed
#endif
	nop
	j	ra
	li	v0,1		# BDSLOT

2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 26 NOP's here since we already have 6 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop;
#endif
	j	ra
	move	v0,zero		# BDSLOT
	.set	reorder
END(mr_tryupdate_noinherit)

/*
 * void mr_unlock_noinherit(mrlock_t *mrp)
 *
 * Decrements access-count or zeros update-bit.
 * DOES NOT CALL MRVSEMA, ONLY TO BE USED WITH
 * mr_try{update,access}_noinherit()
 */
LEAF(mr_unlock_noinherit)
#ifndef SEMAMETER
XLEAF(mrunlock_noinherit)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(MR_LBITS,a0)	# top factor cache barrier for a0
	li	t3,MR_WAIT	# CONSTANT
1:
	MR_LL	t2,MR_LBITS(a0)
	andi	t1,t2,MR_ACC
	beq	t1,zero,4f
	and	t0,t2,t3	# BDSLOT -- t0 holds # of waiters

	# Lock is currently held for access -- decrement access count,

	bne	t0,zero,5f	# any waiters?
	subu	t2,MR_ACCINC	# BDSLOT

	# no one waiting
2:
	AUTO_CACHE_BARRIERS_DISABLE
	MR_SC	t2,MR_LBITS(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	j 	ra
	nop

4:
	# at this point, t3 == MR_WAIT; t0 == # of waiters
	beql	t0,zero,2b	# no threads waiting? -- bail
	xori	t2,MR_UPD	# BDSLOT: turn off MR_UPD if branch taken
				# impossible to take this branch if ACC-mode

	# beql, two instructions above, above prevents need for nops here
	# to prevent R10K LL/SC Hazard workaround.

5:
	# Either threads waiting (fall through immediately above)
	# or non-zero wait count: either case waiting is not allowed
	# in this version without priority inheritance support
	move	a2,a0
	PANIC("Waiters found in mrlock 0x%x with no pri inheritance support")

END(mr_unlock_noinherit)


/*
 * void mrlock(mrlock_t *mrp, int type, int flags)
 *
 * Call _mraccess OR _mrupdate depending on the type of lock to be acquired.
 *
 * Done this way so that _mraccess/_mrupdate has the correct value for ra.
 */

LEAF(mrlock)
	.set	noreorder
	li	v0,1		# test for MR_ACCESS
	bne	a1,v0,5f
#ifdef SEMAMETER
	move	a1,ra		# BDSLOT
	j	_mraccess
	move	a2,zero
5:
	j	_mrupdate
	nop
#else
	move	a1,zero		# BDSLOT
	j	mraccessf
	nop			# BDSLOT
5:
	j	mrupdatef
	nop			# BDSLOT
#endif
END(mrlock)
