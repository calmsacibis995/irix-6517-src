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

/*
 * R4000 atomic operations based on link-load, store-conditional instructions.
 */

#ident	"$Revision: 1.59 $"

#include <ml/ml.h>

/*
 * Implementation of fetch-and-add.
 * a0 - address of 32-bit integer to operate on
 * a1 - value to add to integer
 * new value returned in v0
 */
LEAF(atomicAddInt)
XLEAF(atomicAddUint)
#if (_MIPS_SZLONG == 32)
XLEAF(atomicAddLong)
XLEAF(atomicAddUlong)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	ll	t2,0(a0)
	addu	t1,t2,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t1,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
	nop
#else
	beq	t1,zero,1b
#endif

	# set return value in branch-delay slot
	addu	v0,t2,a1

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicAddInt)

#if MP
/*
 * int  atomicAddIntHot(int *ip, int val, int *pref)
 * uint atomicAddUintHot(uint *ip, uint val, int *pref)
 *
 * Atomically add val to *ip and return the new value.
 *
 * This is a special variant of the normal version of this routine that is
 * used when *ip is a ``hot'' (frequently updated) word on an MP system.  In
 * addition to a pointer to an int to atomically manipulate, we also take a
 * pointer to another byte in the same cache line which we will store into
 * once per LL/SC loop in order to acquire the cache line in dirty exclusive
 * mode.  This helps ensure the success of the SC ...
 */
LEAF(atomicAddIntHot2)
XLEAF(atomicAddUintHot2)
	.set 	noreorder

        # Force cache line to be dirty/exclusive mode because we know that
        # we're going to store into it we want a reasonable chance of having
        # the SC below to succeed when the word is highly contended.  When
        # it's not contended, this extra SW instruction won't introduce a lot
        # of overhead because the cache line will be in our primary cache
        # already or will simply be dragged in exclusive a few instructions
        # sooner than it would have been by the SC.

	sb	zero,0(a2)		# cause prefetch exclusive of cacheline

1:	ll	t2,0(a0)
	addu	t1,t2,a1
	sc	t1,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
#else
	beq	t1,zero,1b
#endif
	sb	zero,0(a2)		# BDSLOT cause prefetch exclusive of cacheline

	j	ra
	addu	v0,t2,a1		# BDSLOT
	.set	reorder
END(atomicAddIntHot2)
#endif /* MP */

#if !defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700) || defined(IP28)
/*
 * Use compiler atomic operation intrinsics for the MipsPRO 7.0 and later
 * compilers.
 *
 * We don't enable the compiler intrinsics for atomic operations on the IP28
 * (Indigo2/R10K) because of a bug in the compiler: #763380,
 * ``t5_no_spec_stores not working for atomic intrinsics.''  When this bug is
 * fixed we can reenable the intrinsics for IP28.
 */

#if (_MIPS_SZLONG == 64)
/*
 * Implementation of long-word fetch-and-add.
 * a0 - address of 32- or64-bit integer to operate on
 * a1 - value to add to integer
 * new value returned in v0
 */
LEAF(atomicAddLong)
XLEAF(atomicAddUlong)
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	LONG_LL	t2,0(a0)
	LONG_ADDU t1,t2,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	LONG_SC	t1,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
	nop
#else
	beq	t1,zero,1b
#endif

	# set return value in branch-delay slot
	LONG_ADDU v0,t2,a1

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicAddLong)
#endif

/*
 * Atomically set bits in a word (atomic version of "word |= arg;")
 * a0 - address of 32-bit integer to operate on
 * a1 - value to 'or' into integer
 * returns old value
 */
LEAF(atomicSetInt)
XLEAF(atomicSetUint)
#if (_MIPS_SZLONG == 32)
XLEAF(atomicSetLong)
XLEAF(atomicSetUlong)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	ll	v0,0(a0)
	or	t2,v0,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicSetInt)

/*
 * Atomically set bits in a 64-bit int (atomic version of "long |= arg;")
 * a0 - address of long to operate on
 * a1 - value to 'or' into integer
 * returns old value
 */
LEAF(atomicSetInt64)
XLEAF(atomicSetUint64)
#if (_MIPS_SZLONG == 64)
XLEAF(atomicSetLong)
XLEAF(atomicSetUlong)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	LONG_LL	v0,0(a0)
	or	t2,v0,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	LONG_SC	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicSetInt64)

#endif /* !defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700) || IP28 */

/*
 * Atomically set bits in a cpumask_t (atomic version of "cpumask_t |= arg;")
 * a0 - address of cpumask_t to operate on
 * a1 - value to 'or' into cpumask_t
 */
LEAF(atomicSetCpumask)
	.set 	noreorder

	LoadCpumask a1,0(a1)		# get the bit mask value into a1

	CACHE_BARRIER_AT(0,a0)		# top foactored for sc

	# Load the old value
1:
	LLcpumask   v0,0(a0)		# load current value
	or	t2,v0,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	SCcpumask   t2,0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicSetCpumask)

#if 1
/*
 * The above if *should be:
 *
 *   #if !defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700) || defined(IP28)
 *
 * In order to signify that the following code is only needed for compiles
 * that aren't using the compiler intrinsics from the MipsPRO 7.0 and later
 * compilers.  Because of a mistake in the #if condition at 6.5 MR the
 * following code was accidentally included for all platforms.  On the off
 * chance that some 3rd party code is now using these routines we need to
 * leave them in till IRIX 6.6 when we're allowed to break binary
 * compatibility in the kernel.
 *
 * Also note that we don't enable the compiler intrinsics for atomic
 * operations on the IP28 (Indigo2/R10K) because of a bug in the compiler:
 * #763380, ``t5_no_spec_stores not working for atomic intrinsics.''  When
 * this bug is fixed we can reenable the intrinsics for IP28.
 */

/*
 * Atomically clear bits in a word (atomic version of "word &= ~arg;")
 * a0 - address of 32-bit integer to operate on
 * a1 - mask of bits to clear in integer
 * returns old value
 *
 * bitlock_release_32bit must use ll/sc even though the lock bit is held
 * because the lock bit isn't protecting _all_ the bits in the (integer) word.
 */
LEAF(atomicClearInt)
XLEAF(atomicClearUint)
XLEAF(bitlock_release_32bit)
#if (_MIPS_SZLONG == 32)
XLEAF(atomicClearLong)
XLEAF(atomicClearUlong)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Negate argument
	nor     a1,a1,zero

	# Load the old value
1:
	ll	v0,0(a0)
	and     t2,v0,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicClearInt)

/*
 * Atomically clear bits in a 64-bit int (atomic version of "long &= ~arg;")
 * a0 - address of 64-bit word to operate on
 * a1 - mask of bits to clear
 * returns old value
 *
 * bitlock_release must use ll/sc even though the lock bit is held because
 * the lock bit isn't protecting _all_ the bits in the (long) word.
 */
LEAF(atomicClearInt64)
XLEAF(atomicClearUint64)
XLEAF(bitlock_release)
#if (_MIPS_SZLONG == 64)
XLEAF(atomicClearLong)
XLEAF(atomicClearUlong)
#endif
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Negate argument
	nor     a1,a1,zero

	# Load the old value
1:
	LONG_LL	v0,0(a0)
	and     t2,v0,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	LONG_SC	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicClearInt64)

#endif /* !defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700) */

/*
 * Atomically clear a bit-field in a long, and set it to a specific value
 * (atomic version of (l) = ((l) & ~(f)) | (b))
 * a0 - address of long to operate on (l)
 * a1 - mask of bits to clear in long (f)
 * a2 - value to or into bit-field    (b)
 * returns old value
 */
LEAF(atomicFieldAssignLong)
	.set	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Negate argument (f)
	nor	a1, a1, zero

	# Load the old value
1:
	LONG_LL	v0, 0(a0)
	and	t2, v0, a1
	or	t2, t2, a2

	# Try to write new one
	AUTO_CACHE_BARRIERS_DISABLE
	LONG_SC	t2, 0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2, zero, 1b
#else
	beq	t2, zero, 1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set reorder
END(atomicFieldAssignLong)	

/*
 * Atomically clear a bit-field in a 32 bit entity
 * and set it to a specific value
 * (atomic version of (l) = ((l) & ~(f)) | (b))
 * a0 - address of long to operate on (l)
 * a1 - mask of bits to clear in Uint (f)
 * a2 - value to or into bit-field    (b)
 * returns old value
 */

LEAF(atomicFieldAssignUint)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# negate argument.
	nor	a1, a1, zero
1:
	# Load old value.
	ll	v0, 0(a0)
	and	t2, v0, a1
	or	t2, t2, a2

	# Try to write new value.
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2, 0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2, zero, 1b
#else
	beq	t2, zero, 1b
#endif
	nop

	# Return
	j	ra
	nop
END(atomicFieldAssignUint)
	
/*
 * Atomically clear bits in a cpumask_t (atomic version of "cpumask_t &= ~arg;")
 * a0 - address of cpumask_t to operate on
 * a1 - value to clear into cpumask_t
 */
LEAF(atomicClearCpumask)
	.set 	noreorder

	LoadCpumask a1,0(a1)		# get the bit mask value into a1
	nor	a1,a1,zero		# negate argument

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	LLcpumask   v0,0(a0)		# load current value
	and	t2,v0,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	SCcpumask   t2,0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicClearCpumask)

/*
 * Implementation of fetch-and-add.
 * a0 - address of 64-bit integer to operate on
 * a1 - value to add to integer
 * new value returned in v0
 */
LEAF(atomicAddInt64)
XLEAF(atomicAddUint64)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

#if _MIPS_SIM == _MIPS_SIM_ABI32
	dsll	a1,a2,32
	dsll	a3,32
	dsrl	a3,32
	or	a1,a3
#endif
	# Load the old value
1:
	lld	t2,0(a0)
	daddu	t1,t2,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	scd	t1,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
	nop
#else
	beq	t1,zero,1b
#endif

	# set return value in branch-delay slot
	daddu	v0,t2,a1
#if _MIPS_SIM == _MIPS_SIM_ABI32
	dsll	v1,v0,32
	dsra	v1,32
	dsra	v0,32
#endif

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicAddInt64)

#if MP
/*
 * int64_t  atomicAddInt64Hot(int64_t *ip, int64_t val, int *pref)
 * uint64_t atomicAddUint64Hot(uint64_t *ip, uint64_t val, int *pref)
 *
 * Atomically add val to *ip and return the new value.
 *
 * This is a special variant of the normal version of this routine that is
 * used when *ip is a ``hot'' (frequently updated) word on an MP system.  In
 * addition to a pointer to an int to atomically manipulate, we also take a
 * pointer to another byte in the same cache line which we will store into
 * once per LL/SC loop in order to acquire the cache line in dirty exclusive
 * mode.  This helps ensure the success of the SC ...
 */
LEAF(atomicAddInt64Hot2)
XLEAF(atomicAddUint64Hot2)
	.set 	noreorder

        # Force cache line to be dirty/exclusive mode because we know that
        # we're going to store into it we want a reasonable chance of having
        # the SC below to succeed when the word is highly contended.  When
        # it's not contended, this extra SW instruction won't introduce a lot
        # of overhead because the cache line will be in our primary cache
        # already or will simply be dragged in exclusive a few instructions
        # sooner than it would have been by the SC.

	sb	zero,0(a2)		# cause prefetch exclusive of cacheline

1:	lld	t2,0(a0)
	daddu	t1,t2,a1
	scd	t1,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
#else
	beq	t1,zero,1b
#endif
	sb	zero,0(a2)		# BDSLOT cause prefetch exclusive of cacheline

	j	ra
	daddu	v0,t2,a1		# BDSLOT
	.set	reorder
END(atomicAddInt64Hot2)
#endif /* MP */

/*
 * Implementation of Push operation.
 *
 * a0 - address of queue head
 * a1 - address of head of items to enqueue
 * a2 - address of tail pointer
 *
 * Since head of list and address of tail pointer are specified separately,
 * one can pass a list of items, already linked, to enqueue.
 */
LEAF(atomicPush)
	.set 	noreorder

	# Load the current queue head value
1:
	PTR_L	t0,0(a0)
#if MP
	andi	t1,t0,1
	bne	t1,zero,2f
#endif

	CACHE_BARRIER_AT(0,a2)		# for store to a2

	# Store the current queue head pointer in tail pointer
	PTR_S	t0,0(a2)

	# Link load the current queue head pointer.
	# If the low bit is set, it is in use by alloc-routine -- try again.
	# If current queue head pointer is the same as what we
	# stored into our tail pointer, try to store it into the list head.

	PTR_LL	v0,0(a0)
	bne	v0,t0,1b

	# copy item pointer into temp register -- the stored-from
	# register gets changed as a result of the store-conditional
	# instruction, and we may have to retry the load/store

	move	t2, a1
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_SC	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
2:	nop
#if MP
	b	1b
	nop
#endif
	.set	reorder
END(atomicPush)

/*
 * Grab entire queue.
 *
 * a0 - address of queue head
 *
 */
LEAF(atomicPull)
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)	# top factored for PTR_SC (barrier for a0)
1:
	PTR_LL	v0,0(a0)	# load the address of the first list item
#if MP
	andi	t1,v0,1
	bne	t1,zero,2f
#endif
	move	t0,zero		# bdslot
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_SC	t0,0(a0)	# null the queue header
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t0,zero,1b
#else
	beq	t0,zero,1b
#endif
	nop

	# Return to caller
	j	ra
2:	nop
#if MP
	b	1b
	nop
#endif
	.set	reorder
END(atomicPull)

/*
 * Implementation of atomicIncWithWrap.
 *  Function useful for circular queues
 * a0 - address of 32-bit integer to inc 
 * a1 - wrap value. When a0 gets to this value we instead set it to 0
 * Original value is returned in v0
 */
LEAF(atomicIncWithWrap)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	ll	t2,0(a0)
	addiu	t1,t2,1
	bne	t1,a1,norm
	nop
	move	t1,zero
norm:		
	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t1,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
	nop
#else
	beq	t1,zero,1b
#endif
	# set return value in branch-delay slot
	move	v0,t2	# BDSLOT
	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicIncWithWrap)

/*
 * Implementation of atomicAddWithWrap.  Similar
 * to atomicIncWithWrap, but add an aribtrary value
 * to the index before wrapping *and* return the
 * original value (not the new value).
 *
 * a0 - address of 32-bit integer to inc 
 * a1 - amount of add
 * a2 - wrap value. When a0 gets to this value we instead set it to 0
 * old value returned in v0
 */
LEAF(atomicAddWithWrap)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	ll	t2,0(a0)
	addu	t1,t2,a1
	blt	t1,a2,2f
	nop
	subu	t1,t1,a2
2:		
	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t1,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
	nop
#else
	beq	t1,zero,1b
#endif
	# set return value in branch-delay slot
	move	v0,t2	# BDSLOT
	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicAddWithWrap)

/*
 * Atomically set bits in an int/long word that contains a locking bit.
 *
 * a0 - address of integer/long word on which to operate
 * a1 - lock bit
 * a2 - value to 'or' into memory word
 *
 * returns old value
 */
LEAF(bitlock_set)
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

1:	ll	v0,0(a0)
#if MP
	and	ta1,v0,a1		# test that lock bit is clear
	bne	ta1,zero,1b		# ain't used on SP systems
#endif
	or	t2,v0,a2

	# Try to set the new bits
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(bitlock_set)

/*
 * Atomically clear bits in an int/long word that contains a locking bit.
 *
 * a0 - address of integer/long word on which to operate
 * a1 - lock bit
 * a2 - value to 'or' into memory word
 * a2 - mask of bits to clear in word
 *
 * returns old value
 */
LEAF(bitlock_clr)
	.set 	noreorder
	nor     a2,a2,zero		# a2 = ~a2

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

1:	ll	v0,0(a0)
#if MP
	and	ta1,v0,a1		# test that lock bit is clear
	bne	ta1,zero,1b		# ain't used on SP systems
#endif
	and     t2,v0,a2

	# Try to set the new bits
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(bitlock_clr)

/*
 * Acquire a bit lock in a long
 *
 * a0 - address of long word containing the bitlock
 * a1 - bit used as lock
 * returns old value	
 */
LEAF(bitlock_acquire)
	.set	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load current value
1:	
	LONG_LL	v0, 0(a0)
	and	t2, v0, a1
	bne	t2, zero, 1b

	# bit is zero, we can now try to set it

	or	t2, v0, a1
	AUTO_CACHE_BARRIERS_DISABLE
	LONG_SC	t2, 0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2, zero, 1b
#else
	beq	t2, zero, 1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set reorder
END(bitlock_acquire)	

/*
 * Acquire a bit lock in a 32-bit word
 *
 * a0 - address of the word containing the bitlock
 * a1 - bit used as lock
 * returns old value	
 */
LEAF(bitlock_acquire_32bit)
	.set	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load current value
1:	
	ll	v0, 0(a0)
	and	t2, v0, a1
	bne	t2, zero, 1b

	# bit is zero, we can now try to set it

	or	t2, v0, a1
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2, 0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2, zero, 1b
#else
	beq	t2, zero, 1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set reorder
END(bitlock_acquire_32bit)	

/*
 * Conditionally acquire a bit lock in a long
 * (Try to acquire only a few times, and then give up)
 * a0 - address of long word containing the bitlock
 * a1 - bit used as lock
 * returns 0 if acquired, 1 if NOT acquired	
 */
LEAF(bitlock_condacq)
	.set	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Initialize retry  counter
	li	t3, 5
1:	nop
	bltz	t3, 2f		# jump to "too many retries"
	subu	t3, 1

	# Load current value
3:	
	LONG_LL	v0, 0(a0)
	and	t2, v0, a1
	bne	t2, zero, 4f	# lock taken, jump to "spin a bit"

	# bit is zero, we can now try to set it

	or	t2, v0, a1		# BDSLOT
	AUTO_CACHE_BARRIERS_DISABLE
	LONG_SC	t2, 0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2, zero, 3b
#else
	beq	t2, zero, 3b
#endif
	nop				# BDSLOT

	# Return to caller with success code (lock acquired)
	j	ra
	or	v0, zero, zero		# BDSLOT

4:	
	# Lock is busy, spin a bit

	li	ta1, 15
5:
	nop
	bgez	ta1, 5b
	subu	ta1, 1

	# Now try again
	
	b	1b
	nop

2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 18 NOP's here since we already have 14 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop;
#endif
	# We exhausted our allowed number of retries
	# Return to caller with failure code (not acquired)
	j	ra
	li	v0, 1			# BDSLOT
	
	.set reorder
END(bitlock_condacq)	

LEAF(bitlock_condacq_32bit)
	.set	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Initialize retry  counter
	li	t3, 5
1:	nop
	bltz	t3, 2f		# jump to "too many retries"
	subu	t3, 1

	# Load current value
3:	
	ll	v0, 0(a0)
	and	t2, v0, a1
	bne	t2, zero, 4f	# lock taken, jump to "spin a bit"

	# bit is zero, we can now try to set it

	or	t2, v0, a1		# BDSLOT
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2, 0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2, zero, 3b
#else
	beq	t2, zero, 3b
#endif
	nop				# BDSLOT

	# Return to caller with success code (lock acquired)
	j	ra
	or	v0, zero, zero		# BDSLOT

4:	
	# Lock is busy, spin a bit

	li	ta1, 15
5:
	nop
	bgez	ta1, 5b
	subu	ta1, 1

	# Now try again
	
	b	1b
	nop

2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 18 NOP's here since we already have 14 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop;
#endif
	# We exhausted our allowed number of retries
	# Return to caller with failure code (not acquired)
	j	ra
	li	v0, 1			# BDSLOT
	
	.set reorder
END(bitlock_condacq_32bit)	
	

/*
 * void *mutex_set_always(__psunsigned_t *p, proc_t *replace)
 *
 * Replaces p's contents with "replace", regardless of whether
 * p's contents were null or not.  This is used by rtlocks to
 * enable a process to assign an owner to a mutex.
 *	
 * Returns: old contents.
 */
LEAF(mutex_set_always)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

1:	PTR_LL	v0,0(a0)
	move	t0,a1
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_SC	t0,0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t0,zero,1b
#else
	beq	t0,zero,1b
#endif
	nop
	j	ra
	nop
	.set	reorder
END(mutex_set_always)	
	
/*
 * Set a word and return the old value.
 * a0 - address of word
 * a1 - new value
 */
LEAF(test_and_set_int)
#if (_MIPS_SZLONG == 32)
XLEAF(test_and_set_long)
#endif
#if (_MIPS_SZPTR == 32)
XLEAF(test_and_set_ptr)
#endif
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	ll	v0,0(a0)
	move	t2,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(test_and_set_int)

/*
 * Set a word and return the old value.
 * a0 - address of word
 * a1 - new value
 */
#if (_MIPS_SZPTR == 64)
LEAF(test_and_set_ptr)
#if (_MIPS_SZLONG == 64)
XLEAF(test_and_set_long)
#endif
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)

	# Load the old value
1:
	lld	v0,0(a0)
	move	t2,a1

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	scd	t2,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t2,zero,1b
#else
	beq	t2,zero,1b
#endif
	nop

	# Return to caller
	j	ra
	nop
	.set	reorder
END(test_and_set_ptr)
#endif

/*
 * Swap data with memory
 * a0 - address of word
 * a1 - new value
 * returns old value
 */
LEAF(swap_int)
#if (_MIPS_SZPTR == 32)
XLEAF(swap_ptr)
#endif
#if (_MIPS_SZLONG == 32)
XLEAF(swap_long)
#endif
	.set 	noreorder
	AUTO_CACHE_BARRIERS_DISABLE

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:	ll	v0,0(a0)
	move	t0,a1
	sc	t0,0(a0)
#ifdef R10K_LLSC_WAR
	beql	t0,zero,1b
#else
	beq	t0,zero,1b
#endif
	nop
	j	ra
	nop
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
END(swap_int)

/*
 * Set a word to a new value if it equals a particular value
 * a0 - address of word
 * a1 - old value
 * a2 - new value
 * returns 1 if the values were swapped
 */
LEAF(compare_and_swap_int)
#if (_MIPS_SZPTR == 32)
XLEAF(compare_and_swap_ptr)
XLEAF(compare_and_swap_kt)
#endif
#if (_MIPS_SZLONG == 32)
XLEAF(compare_and_swap_long)
#endif
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:
	# Load the old value
	ll	t0,0(a0)

	# Check for match
	bne	t0,a1,2f
	move	v0,a2

	# Try to set to the new value
	AUTO_CACHE_BARRIERS_DISABLE
	sc	v0,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop

	# Return success to caller
	j	ra
	nop
2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 28 NOP's here since we already have 4 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; 
#endif
	# Return failure to caller
	j	ra
	move	v0,zero
	.set	reorder
END(compare_and_swap_int)

/*
 * Increment a word in memory only if it is > 0.
 * a0 - address of word
 * returns 0 if the value was not incremented, non-zero otherwise
 */
LEAF(compare_and_inc_int_gt_zero)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:
	# Load the old value
	ll	v0,0(a0)

	# Return failure if value is zero
	ble	v0,zero,2f
	addu	v0,1 

	# Try to set to the new value
	AUTO_CACHE_BARRIERS_DISABLE
	sc	v0,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop

	# Return success to caller
	j	ra
	nop
2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 28 NOP's here since we already have 4 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; 
#endif
	# Return failure to caller
	j	ra
	move	v0,zero
	.set	reorder
END(compare_and_inc_int_gt_zero)

/*
 * Decrement a word in memory only if it is > a specified value
 * a0 - address of word
 * a1 - compare value
 * returns 1 if the value was decremented, zero otherwise
 */
LEAF(compare_and_dec_int_gt)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:
	# Load the old value
	ll	v0,0(a0)

	# Return failure if value was less than or equal to compare value
	addiu	v0,-1 
	blt	v0,a1,2f
	nop
	# Try to set to the new value
	AUTO_CACHE_BARRIERS_DISABLE
	sc	v0,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop

	# Return success to caller
	j	ra
	nop
2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 27 NOP's here since we already have 5 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop;
#endif
	# Return failure to caller
	j	ra
	move	v0,zero
	.set	reorder
END(compare_and_dec_int_gt)

#if MP

/*
 * int compare_and_dec_int_gt_hot2(volatile int *ip, int val, volatile int *jp)
 *
 * Decrement *ip only if it is greater than val.  Returns 1 if *ip was
 * decremented, zero otherwise.
 *
 * This is a special variant of the normal version of this routine that is
 * used when *ip is a ``hot'' (frequently updated) word on an MP system.  In
 * addition to a pointer to an int to atomically manipulate, we also take a
 * pointer to another word in the same cache line which we will store into
 * once per LL/SC loop in order to acquire the cache line in dirty exclusive
 * mode.  This helps ensure the success of the SC ...
 */
LEAF(compare_and_dec_int_gt_hot2)
        .set    noreorder
        CACHE_BARRIER_AT(0, a0)         # top factored for sc (barrier for a0)
1:
        ll      v0, 0(a0)               # Load the old value
        addiu   v0, -1                  # Return failure if value was less
        blt     v0, a1, 2f              #   than or equal to compare value
        nop
        AUTO_CACHE_BARRIERS_DISABLE
        sc      v0, 0(a0)               # Try to set the new value
        AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
        beql    v0, zero, 1b            # Loop on SC failure
#else
        beq     v0, zero, 1b            # Loop on SC failure
#endif
        # On SC failure, force cache line to be dirty/exclusive mode in the
        # branch delay slot of the LL/SC loop branch.  We do this because
        # we're probably going to want to try to store into it on the next
        # LL/SC loop also and we want a reasonable chance of having the SC
        # in the next loop succeed when the word is highly contended.  When
        # it's not contended, this extra SW instruction won't introduce a lot
        # of overhead because the cache line will be in our primary cache
        # dirty exclusive already because of the SC instruction.  Note in
        # particular that we *don't* want to do this at the head of the LL/SC
        # loop as we do for some of the other ``hot word'' atomic routine
        # versions because we sometimes don't update the word (when it is less
        # than or equal to the comparison value).
        #
        sw      zero, 0(a2)             # BDS: force cache line dirty/exclusive

        j       ra                      # Return success to caller
        nop                             # BDS (v0 already equal to 1 from SC)
2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 27 NOP's here since we already have 5 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop;
#endif
        j       ra                      # Return failure to caller
        move    v0, zero                # BDS
        .set    reorder
END(compare_and_dec_int_gt_hot2)

#endif /* MP */

	
/*
 * Swap data with memory
 * a0 - address of word
 * a1 - new value
 * returns old value
 */
LEAF(swap_int64)
#if (_MIPS_SZPTR == 64)
XLEAF(swap_ptr)
#endif
#if (_MIPS_SZLONG == 64)
XLEAF(swap_long)
#endif
	.set 	noreorder
	AUTO_CACHE_BARRIERS_DISABLE

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:	lld	v0,0(a0)
	move	t0,a1
	scd	t0,0(a0)
#ifdef R10K_LLSC_WAR
	beql	t0,zero,1b
#else
	beq	t0,zero,1b
#endif
	nop
	j	ra
	nop
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
END(swap_int64)

/*
 * Set a word to a new value if it equals a particular value
 * a0 - address of word
 * a1 - old value
 * a2 - new value
 * returns 1 if the values were swapped
 */
LEAF(compare_and_swap_int64)
#if (_MIPS_SZPTR == 64)
XLEAF(compare_and_swap_ptr)
XLEAF(compare_and_swap_kt)
#endif
#if (_MIPS_SZLONG == 64)
XLEAF(compare_and_swap_long)
#endif
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:
	# Load the old value
	lld	t0,0(a0)

	# Check for match
	bne	t0,a1,2f
	move	v0,a2

	# Try to set to the new value
	AUTO_CACHE_BARRIERS_DISABLE
	scd	v0,0(a0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop

	# Return success to caller
	j	ra
	nop
2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 28 NOP's here since we already have 4 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; 
#endif
	# Return failure to caller
	j	ra
	move	v0,zero
	.set	reorder
END(compare_and_swap_int64)

/*
 * Load the word from given address, and swap it to new value only if
 * the current value is > parameter a1.
 * if (*a0 > a1) { 
 *	*a0 = a2;
 *	return 1;
 * } else {
 *	return 0;
 * }
 * 
 * a0 - address of word
 * a1 - value to compare for conditional check.
 * a2 - new value to be Swapped.
 * Return 1 is successful in swapping, 0 otherwise.
 */
LEAF(comparegt_and_swap_int)
	.set 	noreorder

	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:
	# Load the old value
	ll	t1,0(a0)	# BD Slot

	# Conditional check --  else part.
	ble	t1,a1,2f
	move	v0,a2

	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	v0,0(a0)	# BD Slot
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop

	# Return success.
	j	ra
	nop

	# Return to caller
2:
#ifdef R10K_LLSC_WAR
	# We need 32 instructions between an LL and any possible path to
	# another LL to avoid the LL/SC hazard in early R10K parts.
	# We use 28 NOP's here since we already have 4 instructions
	# in this speculative path root.
	#
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; nop; nop; nop; nop;
	nop; nop; nop; nop; 
#endif
	j	ra
	move	v0,zero		# BD slot
	.set	reorder
END(comparegt_and_swap_int)

LEAF(store_atomic)
	.set    noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:
	ll      ta0,0(a0)
	AUTO_CACHE_BARRIERS_DISABLE
	sc      ta0,0(a0)
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql     ta0,zero,1b
#else
	beq     ta0,zero,1b
#endif
	nop
	j       ra
	nop

	.set    reorder
	END(store_atomic)

/*
 * Implementation of atomicIncPort
 * Used to avoid locking overhead on MP systems
 *
 * a0 -- address of 32-bit integer to increment
 * a1 -- wrap value
 * a2 -- new value
 *	  When *a0 becomes greater than a1, it is set to a2
 * v0 -- return value; previous contents of *a0
 */
LEAF(atomicIncPort)
	.set 	noreorder
	CACHE_BARRIER_AT(0,a0)		# top factored for sc (barrier for a0)
1:
	ll	t2,0(a0)		# get old value
	addiu	t1,t2,1
	ble	t1,a1,nrm
	 nop
	move	t1, a2			# reset if wrapped
nrm:		
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t1,0(a0)		# set new value
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	t1,zero,1b
	nop
#else
	beq	t1,zero,1b
#endif
	 move	v0,t2			# BDSLOT
	j	ra
	 nop
	.set	reorder
END(atomicIncPort)
