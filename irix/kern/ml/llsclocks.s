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

#ident "$Revision: 1.69 $"

#if SN || IP30 

#if defined(SEMASTAT) && defined(SPLMETER)
#error "cant define SPLMETER & SEMASTAT at same time"
#endif

/*
 * Low-level spin lock routines for non-debug version of
 * mutex_t and lock_t spin locks.
 */

/*
 * spin locks:
 *
 * The low bit of a mutex_t lock represents its spin state:
 *	1 = lock is busy
 *	0 = lock is free
 *
 * We take care to manipulate only the low bit, since the others
 * may store lock-specific data.
 */

#include <ml/ml.h>

#ifdef SPLDEBUG
#define SPLNESTED(x,y,z,reg,x1,x2)	; 	\
	NESTED(x,y,z)	; 	\
	move	reg, ra	; 	\
	j	x1	; 	\
	END(x)		; 	\
	NESTED(x2,y,z)

#define SPLLEAF(x,y,x1,x2)			\
	LEAF(x)			; 	\
	move	y, ra		;	\
	j	x1		; 	\
	END(x)			; 	\
	LEAF(x2)
	
#define	SPLEND(x,x2)	END(x2)
	
#else
	
#define SPLNESTED(x,y,z,reg,x1,x2)	NESTED(x,y,z)	
#define	SPLEND(x,x2)	END(x)
#define SPLLEAF(x,y,x1,x2)	LEAF(x)	
#endif			
	
/*
 * int rv = mutex_spinlock_spl(lock_t *sp, int (*splfunc_t)(void))
 * int rv = mutex_bitlock_spl(uint_t *lck, uint_t bit, int (*splfunc_t)(void))
 */
#ifdef SEMASTAT
LOCALSPINSZ=	6			# ra, s0, s1, s2, a2
#else
LOCALSPINSZ=	4			# ra, s0, s1, s2
#endif
SPINSPLFRM=	FRAMESZ((NARGSAVE+LOCALSPINSZ)*SZREG)
RA_SPINOFF=		SPINSPLFRM-(1*SZREG)
S0_SPINOFF=		SPINSPLFRM-(2*SZREG)
S1_SPINOFF=		SPINSPLFRM-(3*SZREG)
S2_SPINOFF=		SPINSPLFRM-(4*SZREG)
#ifdef SEMASTAT
A2_SPINOFF=		SPINSPLFRM-(5*SZREG)
#endif
NESTED(mutex_bitlock_spl, SPINSPLFRM, zero)
	.set	noreorder
	PTR_SUBU sp,SPINSPLFRM
	REG_S	ra,RA_SPINOFF(sp)
	REG_S	s0,S0_SPINOFF(sp)
	REG_S	s1,S1_SPINOFF(sp)
	move	s0,a0				# copy arg
	move	s1,a1				# copy arg

	# call spl routine
1:	jal	a2				# v0 == ospl
	REG_S	a2,S2_SPINOFF(sp)		# BDSLOT (use a2 instead of s2)

#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif

	# See if lock is busy.
2:	ll	ta0,0(s0)
	and	ta1,ta0,s1
	bne	ta1,zero,3f
	or	ta0,s1

	# Try to mark lock busy
	sc	ta0,0(s0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	REG_L	ra,RA_SPINOFF(sp)		# reload ra for fast exit

	# We have the lock.
#ifdef SPLMETER
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	REG_L	a2,S2_SPINOFF(sp)		# address of spl function
	PTR_ADDU sp,SPINSPLFRM
	move	a0,ra
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a0,s0
	move	a1,ra
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	move	a3,v0
	j	lstat_update_spin
	PTR_ADDU sp,SPINSPLFRM			# BDSLOT
#else
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	j	ra
	PTR_ADDU sp,SPINSPLFRM
#endif

3: 	# Lock is busy.  Briefly enable interrupts, and try again.
#ifdef SPLMETER
	jal	__splx
	move	a0,v0				# BDSLOT
	b	1b
	REG_L	a2,S2_SPINOFF(sp)		# reload the routine address
#elif SEMASTAT
	REG_S	a2,A2_SPINOFF(sp)
	jal	splx
	move	a0,v0				# BDSLOT
	REG_L	a2,S2_SPINOFF(sp)		# reload the routine address
	jal	a2				# v0 == ospl
	nop					# BDSLOT
	REG_L	a2,A2_SPINOFF(sp)
	b	2b
	addu	a2,1				# BDSLOT - Increment spin count
#else
	jal	splx
	move	a0,v0				# BDSLOT
	b	1b
	REG_L	a2,S2_SPINOFF(sp)		# reload the routine address
#endif

	.set	reorder
	END(mutex_bitlock_spl)

SPLNESTED(mutex_spinlock_spl, SPINSPLFRM, zero, a2, _mutex_spinlock_spl, __mutex_spinlock_spl)
	.set	noreorder
	PTR_SUBU sp,SPINSPLFRM
	REG_S	ra,RA_SPINOFF(sp)
	REG_S	s0,S0_SPINOFF(sp)
	move	s0,a0				# copy arg

	# call spl routine
1:	jal	a1
	REG_S	a1,S1_SPINOFF(sp)		# BDSLOT (use a1 instead of s1)

#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif
	# See if lock is busy.
2:	LOCK_LL	ta0,0(s0)
	andi	ta1,ta0,1
	bne	ta1,zero,3f
	or	ta0,1

	# Try to mark lock busy
	LOCK_SC	ta0,0(s0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	REG_L	ra,RA_SPINOFF(sp)		# reload ra for fast exit

	# We have the lock.
#ifdef SPLMETER
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	a2,S1_SPINOFF(sp)		# address of spl function
	PTR_ADDU sp,SPINSPLFRM
	move	a0,ra
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a0,s0
	move	a1,ra
	REG_L	s0,S0_SPINOFF(sp)
	move	a3,v0
	j	lstat_update_spin
	PTR_ADDU sp,SPINSPLFRM			# BDSLOT
#else
	REG_L	s0,S0_SPINOFF(sp)
	j	ra
	PTR_ADDU sp,SPINSPLFRM
#endif

3: 	# Lock is busy.  Briefly enable interrupts, and try again.
#ifdef SPLMETER
	jal	__splx
	move	a0,v0
	b	1b
	REG_L	a1,S1_SPINOFF(sp)		# reload the routine address
#elif SEMASTAT
	REG_S	a2,A2_SPINOFF(sp)
	jal	splx
	move	a0,v0				# BDSLOT
	REG_L	a1,S1_SPINOFF(sp)		# reload the routine address
	jal	a1				# v0 == ospl
	nop					# BDSLOT
	REG_L	a2,A2_SPINOFF(sp)
	b	2b
	addu	a2,1				# BDSLOT - Update spin count
#else
	jal	splx
	move	a0,v0
	b	1b
	REG_L	a1,S1_SPINOFF(sp)		# reload the routine address
#endif

	.set	reorder
	SPLEND(mutex_spinlock_spl,__mutex_spinlock_spl)

/*
 * int rv = mutex_spinlock(mutex_t *mp)
 */
SPLLEAF(mutex_spinlock,a1,_mutex_spinlock,__mutex_spinlock)
	.set 	noreorder

	# First, set up values needed in the body of spinlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t2 - a proper splhi value for SR
	# v0 - old IMASK

	MFC0(v0,C0_SR)
	or	t0,v0,SR_IE
	xor	t0,SR_IE
	or	t2,v0,~SR_HI_MASK&SR_IMASK

#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif
	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)

	# See if lock is busy.
2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,1
	bne	ta1,zero,3f
	or	ta0,1				# BDSLOT

	# Try to mark lock busy
	LOCK_SC	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
#else
	beq	ta0,zero,2b
#endif
	nop					# BDSLOT

	# We have the lock.
	# Enable IE and do an splhi.
	xor	t2,~SR_HI_MASK&SR_IMASK		# splhi SR value
	MTC0(t2,C0_SR)

#ifdef SPLMETER
	move	a0,ra
	LA	a2,splhi
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	move	a3,v0				# BDSLOT
#else
	j	ra
	nop					# BDSLOT
#endif

3: 	# Lock is busy.  Briefly enable interrupts.
	MTC0(v0,C0_SR)
	b	1b
#ifdef SEMASTAT
	addu	a2,1				# increment spin count
#else
	nop					# BDSLOT
#endif

	.set 	reorder
	SPLEND(mutex_spinlock,__mutex_spinlock)	

/*
 * void mutex_spinunlock(mutex_t *mp, int s)
 */
SPLLEAF(mutex_spinunlock,a2,_mutex_spinunlock,__mutex_spinunlock)
	.set	noreorder

	LOCK_LW	t0,0(a0)			# fetch lock word
#if DEBUG
	andi	t1,t0,1
	bne	t1,zero,1f
	nop					# BDSLOT
	break	1
1:
#endif
	or	t0,1				# clear lock bit
	xori	t0,1
	sync					# needed on R10K, nop on R4x00
	LOCK_SW	t0,0(a0)			# free lock

#ifdef SPLMETER
	move	a0,a1
	j	lowerspl
	move	a1,ra
#else
	MTC0(a1,C0_SR)

	j	ra
	nop					# BDSLOT
#endif
	.set 	reorder
	SPLEND(mutex_spinunlock,__mutex_spinunlock)	

/*
 * nested_spinlock(lock_t *lp)
 *
 * Acquire spin lock.
 * Does not mask any interrupts.
 *
 * Low bit is used as locking bit -- other bits are left untouched.
 *
 * Returns: nothing.
 */
LEAF(nested_spinlock)
	.set	noreorder
	# a0 - contains address of pointer-sized lock
	# See if lock is busy.
#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif
1:	LOCK_LL	ta0,0(a0)
	andi	ta1,ta0,1
#ifdef SEMASTAT
	bne	ta1,zero,8f
#else
	bne	ta1,zero,1b
#endif
	or	ta0,1

	# Try to mark lock busy
	LOCK_SC	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,1b
#else
	beq	ta0,zero,1b
#endif
	nop					# BDSLOT

	# We have the lock.
#ifdef SEMASTAT
	j	lstat_update_spin
	move	a1,ra				# BDSLOT

8:	b	1b
	addu	a2,1				# BDSLOT - increment spin count
	
#else
	j	ra
	nop					# BDSLOT
#endif
	.set	reorder
	END(nested_spinlock)	

/*
 * nested_spinunlock(lock_t *lp)
 *
 * Release lock.  Don't touch any bits except the (low) lock bit.
 *
 * Returns: nothing.
 */
LEAF(nested_spinunlock)
	.set	noreorder
	LOCK_LW	v0,0(a0)
#if DEBUG
	andi	t1,v0,1
	bne	t1,zero,1f
	nop					# BDSLOT
	break	1
1:
#endif
	xor	v0,1				# clear lock bit
	sync					# needed on R10K, nop on R4x00
	j	ra
	LOCK_SW	v0,0(a0)			# free lock
	.set	reorder
	END(nested_spinunlock)	

LEAF(nested_spintrylock)
	.set	noreorder
	# a0 - contains address of pointer-sized lock
	# v0 - returns 1 on success, 0 on failure

1:	LOCK_LL	v0,0(a0)			# see if lock is busy
	andi	ta1,v0,1
	bne	ta1,zero,4f
	ori	v0,1

	# Try to mark lock busy
	LOCK_SC	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop					# BDSLOT

	# We have the lock.
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0
#else
	j	ra
	nop					# BDSLOT
#endif

4:	# trylock failed.

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

#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero
#else
	j	ra
	move	v0,zero				# BDSLOT: returns failure
#endif
	.set	reorder
	END(nested_spintrylock)	

/*
 * int rv = mutex_bitlock(uint *lp, uint bit)
 */
SPLLEAF(mutex_bitlock,a2,_mutex_bitlock,__mutex_bitlock)
#ifndef SEMASTAT
XLEAF(mutex_bitlock_ra)
#endif
	.set 	noreorder

	# First, set up values needed in the body of spinlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t2 - a proper splhi value for SR
	# v0 - old IMASK

	MFC0(v0,C0_SR)
	or	t0,v0,SR_IE
	xor	t0,SR_IE
	or      t2,v0,~SR_HI_MASK&SR_IMASK

#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif
	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)

	# See if lock is busy.
2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,3f
	or	ta0,a1				# BDSLOT

	# Try to mark lock busy
	sc	ta0,0(a0)
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
#else
	beq	ta0,zero,2b
#endif
	nop					# BDSLOT

	# We have the lock.
	# Enable IE and do an splhi.
	xor     t2,~SR_HI_MASK&SR_IMASK  	# splhi SR value
	MTC0(t2,C0_SR)

#ifdef SPLMETER
	move	a0,ra
	LA	a2,splhi
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	move	a3,v0				# BDSLOT
#else
	j	ra
	nop					# BDSLOT
#endif

3: 	# Lock is busy.  Briefly enable interrupts.
	MTC0(v0,C0_SR)
	b	1b
#ifdef SEMASTAT
	addu	a2,1				# BDSLOT - increment spin count
#else
	nop					# BDSLOT
#endif
	.set 	reorder
	SPLEND(mutex_bitlock,__mutex_bitlock)	

#ifdef SEMASTAT
/*
 * int rv = mutex_bitlock_ra(uint *lp, uint bit, void *ra)
 */
SPLLEAF(mutex_bitlock_ra,a2,_mutex_bitlock,__mutex_bitlock)
	.set 	noreorder

	# First, set up values needed in the body of spinlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t2 - a proper splhi value for SR
	# v0 - old IMASK

	MFC0(v0,C0_SR)
	or	t0,v0,SR_IE
	xor	t0,SR_IE
	or      t2,v0,~SR_HI_MASK&SR_IMASK

	move	t1,a2				# save caller_ra for lstat_update
	li	a2,0				# set action = nospin

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)

	# See if lock is busy.
2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,3f
	or	ta0,a1				# BDSLOT

	# Try to mark lock busy
	sc	ta0,0(a0)
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
#else
	beq	ta0,zero,2b
#endif
	nop					# BDSLOT

	# We have the lock.
	# Enable IE and do an splhi.
	xor     t2,~SR_HI_MASK&SR_IMASK  	# splhi SR value
	MTC0(t2,C0_SR)

	PTR_ADDU a1,t1,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	j	lstat_update_spin
	move	a3,v0				# BDSLOT

3: 	# Lock is busy.  Briefly enable interrupts.
	MTC0(v0,C0_SR)
	b	1b
	addu	a2,1				# BDSLOT - increment spin count
	.set 	reorder
	SPLEND(mutex_bitlock_ra,__mutex_bitlock)	

/*
 * int rv = mutex_bittrylock_ra(uint_t *lock, uint bit, void *ra)
 *
 * Returns: old priority level on success, 0 on failure.
 */
LEAF(mutex_bittrylock_ra)
	.set 	noreorder

	MFC0(v0,C0_SR)
	or	t0,v0,SR_IE
	xor	t0,SR_IE
	or	t2,v0,~SR_HI_MASK&SR_IMASK

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)

	# See if lock is busy.
2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,3f
	or	ta0,a1

	# Try to mark lock busy
	sc	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
#else
	beq	ta0,zero,2b
#endif
	nop					# BDSLOT

	# We have the lock.
	# Enable IE and do an splhi
	xor	t2,~SR_HI_MASK&SR_IMASK		# splhi SR value
	MTC0(t2,C0_SR)

	PTR_ADDU a1,a2,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	li	a2,0
	j	lstat_update
	move	a3,v0

3: 	# Lock is busy.  Re-enable interrupts and return failure.
	MTC0(v0,C0_SR)
	PTR_ADDU a1,a2,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	li	a2,3
	j	lstat_update
	move	a3,zero

	.set 	reorder
	END(mutex_bittrylock_ra)

#endif /* SEMASTAT */

/*
 * void mutex_bitunlock(uint *lp, uint bit, int s)
 */
SPLLEAF(mutex_bitunlock,a3,_mutex_bitunlock,__mutex_bitunlock)
	.set	noreorder

	lw	t0,0(a0)			# fetch lock word
#if DEBUG
	and	t1,t0,a1
	bne	t1,zero,1f
	nop					# BDSLOT
	break	1
1:
#endif
	xor	t0,a1
	sync					# needed on R10K, nop on R4x00
	sw	t0,0(a0)			# free lock

#ifdef SPLMETER
	move	a0,a2
	j	lowerspl
	move	a1,ra
#else
	MTC0(a2,C0_SR)

	j	ra
	nop					# BDSLOT
#endif
	.set 	reorder
	SPLEND(mutex_bitunlock,__mutex_bitunlock)	


/*
 * nested_bitlock(uint *lock, uint bit)
 *
 * Acquire bit lock.
 * Does not mask out interrupts.
 *
 * Returns: nothing.
 */
LEAF(nested_bitlock)
	.set	noreorder
	# a0 - contains address of (integer-sized) lock
	# See if lock is busy.
#ifdef SEMASTAT
	li	a2,0				# action = nospin
#endif
1:	ll	ta0,0(a0)
	and	ta1,ta0,a1
#ifdef SEMASTAT
	bne	ta1,zero,8f
#else
	bne	ta1,zero,1b
#endif
	or	ta0,a1

	# Try to mark lock busy
	sc	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,1b
#else
	beq	ta0,zero,1b
#endif
	nop					# BDSLOT

	# We have the lock.
#ifdef SEMASTAT
	j	lstat_update_spin
	move	a1,ra				# BDSLOT

8:	b	1b
	addu	a2,1				# BDSLOT - increment spin count
#else
	j	ra
	nop					# BDSLOT
#endif
	.set	reorder
	END(nested_bitlock)	

/*
 * nested_bitunlock(uint *lock, uint bit)
 *
 * Release bit; doesn't affect ipl.
 */
LEAF(nested_bitunlock)
	.set	noreorder
	lw	v0,0(a0)
#if DEBUG
	and	t1,v0,a1
	bne	t1,zero,1f
	nop					# BDSLOT
	break	1
1:
#endif
	xor	v0,a1
	sync					# needed on R10K, nop on R4x00
	j	ra
	sw	v0,0(a0)			# BDSLOT
	.set	reorder
	END(nested_bitunlock)	

/*
 * int rv = mutex_64bitlock(__uint64_t *lp, __uint64_t bit)
 */
LEAF(mutex_64bitlock)
	.set 	noreorder

	# First, set up values needed in the body of spinlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t2 - a proper splhi value for SR
	# v0 - old IMASK

	MFC0(v0,C0_SR)
	or	t0,v0,SR_IE
	xor	t0,SR_IE
	or	t2,v0,~SR_HI_MASK&SR_IMASK

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)

#ifdef SEMASTAT
	li	a2,0				# action = nospin
#endif

	# See if lock is busy.
2:	lld	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,3f
	or	ta0,a1

	# Try to mark lock busy
	scd	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
#else
	beq	ta0,zero,2b
#endif
	nop					# BDSLOT

	# We have the lock.
	# Enable IE and do an splhi.
	xor	t2,~SR_HI_MASK&SR_IMASK		# splhi SR value
	MTC0(t2,C0_SR)

#ifdef SPLMETER
	move	a0,ra
	LA	a2,splhi
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	move	a3,v0				# BDSLOT
#else
	j	ra
	nop					# BDSLOT
#endif

3: 	# Lock is busy.  Briefly enable interrupts.
	MTC0(v0,C0_SR)
	b	1b
#ifdef SEMASTAT
	addu	a2,1				# BDSLOT - increment spin count
#else
	nop					# BDSLOT
#endif
	.set 	reorder
	END(mutex_64bitlock)	

/*
 * void mutex_64bitunlock(__uint64_t *lp, __uint64_t bit, int s)
 */
LEAF(mutex_64bitunlock)
	.set	noreorder

	ld	t0,0(a0)			# fetch lock word
#if DEBUG
	and	t1,t0,a1
	bne	t1,zero,1f
	nop					# BDSLOT
	break	1
1:
#endif
	xor	t0,a1
	sd	t0,0(a0)		# free lock

#ifdef SPLMETER
	move	a0,a2
	j	lowerspl
	move	a1,ra
#else
	MTC0(a2,C0_SR)

	j	ra
	nop					# BDSLOT
#endif
	END(mutex_64bitunlock)	

/*
 * nested_64bitlock(__uint64_t *lock, __uint64_t bit)
 *
 * Acquire bit lock.
 * Does not mask out interrupts.
 *
 * Returns: nothing.
 */
LEAF(nested_64bitlock)
	.set	noreorder
	# a0 - contains address of (integer-sized) lock
	# See if lock is busy.
#ifdef SEMASTAT
	li	a2,0				# action = nospin
#endif
1:	lld	ta0,0(a0)
	and	ta1,ta0,a1
#ifdef SEMASTAT
	bne	ta1,zero,8f
#else
	bne	ta1,zero,1b
#endif
	or	ta0,a1

	# Try to mark lock busy
	scd	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,1b
#else
	beq	ta0,zero,1b
#endif
	nop					# BDSLOT

	# We have the lock.
#ifdef SEMASTAT
	j	lstat_update_spin
	move	a1,ra				# BDSLOT

8:	b	1b
	addu	a2,1				# BDSLOT - increment spin count
#else
	j	ra
	nop					# BDSLOT
#endif
	.set	reorder
	END(nested_64bitlock)	

/*
 * nested_64bitunlock(__uint64_t *lock, __uint64_t bit)
 *
 * Release bit; doesn't affect ipl.
 */
LEAF(nested_64bitunlock)
	.set	noreorder
	ld	v0,0(a0)
#if DEBUG
	and	t1,v0,a1
	bne	t1,zero,1f
	nop					# BDSLOT
	break	1
1:
#endif
	xor	v0,a1
	j	ra
	sd	v0,0(a0)
	.set	reorder
	END(nested_64bitunlock)	

/*
 * mutex_spintrylock(lock_t *lp)
 *
 * Attempt to acquire spin lock.
 * Low bit is used as locking bit -- other bits are left untouched.
 *
 * Returns: ospl on success, 0 on failure.
 *
 * NOTE: reserves stack space for s1 but does not use it.
 */
LEAF(mutex_spintrylock)
	.set	noreorder

	MFC0(v0,C0_SR)
	or	t0,v0,SR_IE
	xor	t0,SR_IE
	or	t2,v0,~SR_HI_MASK&SR_IMASK

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)

	# See if lock is busy.
2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,1
	bne	ta1,zero,4f
	or	ta0,1

	# Try to mark lock busy
	LOCK_SC	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
#else
	beq	ta0,zero,2b
#endif
	nop					# BDSLOT

	# We have the lock.
	# Enable IE and do an splhi.
	xor	t2,~SR_HI_MASK&SR_IMASK		# splhi SR value
	MTC0(t2,C0_SR)

#ifdef SPLMETER
	move	a0,ra
	LA	a2,splhi
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0
#else
	j	ra
	nop					# BDSLOT
#endif

4: 	# Lock is busy.  Just punt.
	MTC0(v0,C0_SR)

#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero
#else
	j	ra
	move	v0,zero
#endif
	.set 	reorder
	END(mutex_spintrylock)	

/*
 * int ospl = mutex_spintrylock_spl(lock_t *lock, splfunc_t);
 */
NESTED(mutex_spintrylock_spl, SPINSPLFRM, zero)
	.set	noreorder
	PTR_SUBU sp,SPINSPLFRM
	REG_S	ra,RA_SPINOFF(sp)
	REG_S	s1,S1_SPINOFF(sp)
	move	s1,a1		# remember spl routine address
	REG_S	s0,S0_SPINOFF(sp)
	move	s0,a0

	# call spl routine
	jal	s1
	nop					# BDSLOT

	# See if lock is busy.
1:	LOCK_LL	ta0,0(s0)
	andi	ta1,ta0,1
	bne	ta1,zero,3f
	or	ta0,1

	# Try to mark lock busy
	LOCK_SC	ta0,0(s0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,1b
#else
	beq	ta0,zero,1b
#endif
	nop					# BDSLOT

#ifdef SEMASTAT
	li	a2,0
#endif

	# We have the lock.
2:
#ifdef SPLMETER
	REG_L	ra,RA_SPINOFF(sp)
	REG_L	s0,S0_SPINOFF(sp)
	move	a2,s1				# address of spl function
	REG_L	s1,S1_SPINOFF(sp)
	PTR_ADDU sp,SPINSPLFRM
	move	a0,ra
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a0,s0
	REG_L	ra,RA_SPINOFF(sp)
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	move	a1,ra
	move	a3,v0
	j	lstat_update
	PTR_ADDU sp,SPINSPLFRM			# BDSLOT
#else
	REG_L	ra,RA_SPINOFF(sp)
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	j	ra
	PTR_ADDU sp,SPINSPLFRM
#endif

3: 	# Lock is busy.  
#ifdef SPLMETER
	jal	__splx
	move	a0,v0
#else
	jal	splx
	move	a0,v0
#endif
#ifdef SEMASTAT
	li	a2,3
#endif
	b	2b
	move	v0,zero

	.set	reorder
	END(mutex_spintrylock_spl)

/*
 * int rv = mutex_bittrylock(uint_t *lock, uint bit)
 *
 * Returns: old priority level on success, 0 on failure.
 */
LEAF(mutex_bittrylock)
#ifndef SEMASTAT
XLEAF(mutex_bittrylock_ra)
#endif

	.set 	noreorder

	MFC0(v0,C0_SR)
	or	t0,v0,SR_IE
	xor	t0,SR_IE
	or	t2,v0,~SR_HI_MASK&SR_IMASK

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)

	# See if lock is busy.
2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,3f
	or	ta0,a1

	# Try to mark lock busy
	sc	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
#else
	beq	ta0,zero,2b
#endif
	nop					# BDSLOT

	# We have the lock.
	# Enable IE and do an splhi
	xor	t2,~SR_HI_MASK&SR_IMASK		# splhi SR value
	MTC0(t2,C0_SR)

#ifdef SPLMETER
	move	a0,ra
	LA	a2,splhi
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#elif SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0
#else
	j	ra
	nop					# BDSLOT
#endif

3: 	# Lock is busy.  Re-enable interrupts and return failure.
	MTC0(v0,C0_SR)
#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero
#else
	j	ra
	move	v0,zero				# BDSLOT
#endif
	.set 	reorder
	END(mutex_bittrylock)	

/*
 *	int nested_bittrylock(uint *lp, uint lock-bit);
 */
LEAF(nested_bittrylock)
	.set	noreorder
	# See if lock is busy.
1:	ll	v0,0(a0)
	and	ta1,v0,a1
	bne	ta1,zero,2f
	or	v0,a1				# BDSLOT

	# Try to mark lock busy
	sc	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop					# BDSLOT

	# We have the lock.
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0				# BDSLOT
#else
	j	ra
	nop					# BDSLOT
#endif

2: 	# trylock failed.

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

#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero				# BDSLOT
#else
	j	ra
	move	v0,zero				# BDSLOT: returns failure
#endif
	.set	reorder
	END(nested_bittrylock)	

/*
 *	int nested_64bittrylock(__ulong64_t *lp, __ulong64_t lock-bit);
 */
LEAF(nested_64bittrylock)
	.set	noreorder
	# See if lock is busy.
1:	lld	v0,0(a0)
	and	ta1,v0,a1
	bne	ta1,zero,2f
	or	v0,a1				# BDSLOT

	# Try to mark lock busy
	scd	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop					# BDSLOT

	# We have the lock.
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0				# BDSLOT
#else
	j	ra
	nop					# BDSLOT
#endif

2: 	# trylock failed.

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

#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero				# BDSLOT
#else
	j	ra
	move	v0,zero				# BDSLOT: returns failure
#endif
	.set	reorder
	END(nested_64bittrylock)	

#endif /* SN || IP30 */
