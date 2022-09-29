/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if EVEREST
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
 * 
 * Every lock and unlock operation manipulates the CEL register.
 * This has a desirable side-effect of flushing any data in the
 * CPU board's uncached write buffer.  Device drivers implicitly
 * count on this effect, since they expect PIOs done under a lock
 * to have made it out to the bus by the time the lock is unlocked.
 *
 * We're very careful here about C0_SR manipulations and the order
 * in which they occur relative to CEL manipulations and to obtaining
 * the spin lock.
 */
#ident	"$Revision: 1.67 $"

#include <ml/ml.h>

#include <sys/EVEREST/evintr.h>


#ifdef ALWAYS_SET_IE	
#define MODIFY_UNLOCK_SR(_x)	ori	_x, SR_IE
#else
#define MODIFY_UNLOCK_SR(_x)
#endif
/* This flag is used to indicate code which has been added because the kernel
 * no longer has a kpreempt flag but replies upon "isspl0" to determine if
 * preemption is allowed.  This requires us to atomically update both the
 * C0_SR and the CEL since taking an interrupt in the middle of this 
 * sequence (which does not cause a problem, just enables some interrupts
 * earlier than others) can cause us to not preempt the currently executing
 * kthread since we're only "halfway" to spl0 at the time of the interrupt
 * (i.e. we've dropped CEL allowing the device interrupt to occur but haven't
 * reset some of the other IMASK bits in th SR).
 */		
#define NO_KPREEMPT_FLAG	1	
	
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
SPLNESTED(mutex_bitlock_spl, SPINSPLFRM, zero, a3, _mutex_bitlock_spl, __mutex_bitlock_spl)
	.set	noreorder
	PTR_SUBU sp,SPINSPLFRM
	REG_S	s1,S1_SPINOFF(sp)
	REG_S	s0,S0_SPINOFF(sp)
	move	s1,a1				# copy arg
	move	s0,a0				# copy arg
	REG_S	ra,RA_SPINOFF(sp)

	# call spl routine
1:	jal	a2
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

	# We have the lock.  Restore regs.
#ifdef SEMASTAT
	move	a0,s0
	move	a1,ra
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	move	a3,v0
	j	lstat_update_spin
	PTR_ADDU sp,SPINSPLFRM			# BDSLOT
#elif SPLMETER
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	REG_L	a2,S2_SPINOFF(sp)
	PTR_ADDU sp,SPINSPLFRM
	move	a0,ra
	j	raisespl_spl
	move	a1,v0
#else
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	j	ra
	PTR_ADDU sp,SPINSPLFRM
#endif

3: 	# Lock is busy.  Briefly enable interrupts, and try again.
#ifdef SEMASTAT
	REG_S	a2,A2_SPINOFF(sp)
	jal	splx
	move	a0,v0				# BDSLOT
	REG_L	a2,S2_SPINOFF(sp)		# reload the routine address
	jal	a2				# v0 == ospl
	addu	a2,1				# BDSLOT - Increment spin count
	b	2b
	REG_L	a2,S2_SPINOFF(sp)		# reload the routine address
#elif SPLMETER
	jal	__splx
	move	a0,v0
	b	1b
	REG_L	a2,S2_SPINOFF(sp)		# reload the routine address
#else
	jal	splx
	move	a0,v0

	b	1b
	REG_L	a2,S2_SPINOFF(sp)		# reload the routine address
#endif

	.set	reorder
	SPLEND(mutex_bitlock_spl,__mutex_bitlock_spl)

SPLNESTED(mutex_spinlock_spl, SPINSPLFRM, zero, a2, _mutex_spinlock_spl,__mutex_spinlock_spl)
	.set	noreorder
	PTR_SUBU sp,SPINSPLFRM
	REG_S	s0,S0_SPINOFF(sp)
	REG_S	ra,RA_SPINOFF(sp)
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
#ifdef SEMASTAT
	move	a0,s0
	move	a1,ra
	REG_L	s0,S0_SPINOFF(sp)
	move	a3,v0
	j	lstat_update_spin
	PTR_ADDU sp,SPINSPLFRM			# BDSLOT
#elif SPLMETER
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	a2,S1_SPINOFF(sp)		# address of spl function
	PTR_ADDU sp,SPINSPLFRM
	move	a0,ra
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#else
	REG_L	s0,S0_SPINOFF(sp)
	j	ra
	PTR_ADDU sp,SPINSPLFRM
#endif

3: 	# Lock is busy.  Briefly enable interrupts, and try again.
#ifdef SEMASTAT
	REG_S	a2,A2_SPINOFF(sp)
	jal	splx
	move	a0,v0				# BDSLOT
	REG_L	a1,S1_SPINOFF(sp)		# reload the routine address
	jal	a1				# v0 == ospl
	nop					# BDSLOT
	REG_L	a2,A2_SPINOFF(sp)
	b	2b
	addu	a2,1				# BDSLOT - Update spin count
#elif SPLMETER
	jal	__splx
	move	a0,v0
	b	1b
	REG_L	a1,S1_SPINOFF(sp)		# reload the routine address
#else
	jal	splx
	move	a0,v0
	b	1b
	REG_L	a1,S1_SPINOFF(sp)		# reload the routine address
#endif

	.set	reorder
	SPLEND(mutex_spinlock_spl,__mutex_spinlock_spl)

/*
 * int rv = mutex_spinlock(lock_t *lp)
 */
SPLLEAF(mutex_spinlock,a1,_mutex_spinlock,__mutex_spinlock)
	.set 	noreorder

	# First, set up values needed in the body of spinlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t1 - old CEL value
	# t2 - a proper splhi value for SR
	# t3 - EVINTR_LEVEL_HIGH+1
	# v0 - old IMASK

#if TFP
	DMFC0(v0,C0_SR)	
	GET_CEL(t1)		/* t1 = current CEL */
	ori	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xori	t0,SR_IE

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc

	dmtc0	t0,C0_SR

	/* The following instructions used to be in the loop pre-amble.
	 * Moved them here since we were doing SSNOPS anyway, and this
	 * speeds up the common case of a single pass through the loop.
	 * CODE MUST BE RE-EXECUTABLE SINCE LOOP MAY BE EXECUTED MORE
	 * THAN ONCE.
	 * THERE MUST BE 4 SUPERSCALER CYCLES FROM THE "dmtc" ABOVE UNTIL
	 * THE "PTR_SC" BELOW SO THAT INTERRUPTS ARE DISABLED WHEN THAT
	 * INSTRUCTION EXECUTES.
	 * cycle count analysis (worst case):
	 *	dmtc0			[0]
	 *      LI      (first inst)	[0]  worst case
	 *	LI	(2nd inst)	[1]
	 *	or			[1]
	 * 	xor			[2]
	 *	PTR_LL			[2]
	 *	and			[3]
	 *	bne			[3]
	 *	or			[3]
	 *	PTR_SC			[4]
	 */

	LI	ta0,SPLHI_DISABLED_INTS
	or	t2,v0,ta0
	xor	t2,ta0			/* disable interrupts (splhi) */

	/* Former pre-amble ends here.
	 * See if lock is busy.
	 */
#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif

2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,1
	bne	ta1,zero,4f
	or	ta0,1

	# Try to mark lock busy (interrupts disabled by now)

	LOCK_SC	ta0,0(a0)	
	beq	ta0,zero,2b
	li	t3,SPLHI_CEL

	# We have the lock.
	# Enable IE and do an splhi.

	.set	reorder
	li	ta0,SR_IMASK
	bge	t1,t3,3f	/* don't lower spl */
	.set	noreorder

	RAISE_CEL(t3,ta3)
3:
	dmtc0	t2,C0_SR
	and	v0,ta0
#ifdef SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	or	a3,v0,t1			# BDSLOT
#elif SPLMETER
	or	v0,t1
	move	a0,ra
	LA	a2,splhi
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Briefly enable interrupts.
	# Don't worry about delay until this takes affect,
	# since the dmtc at label "1:" will have the same delay
	# and hence interrupts will be enabled for a few cycles.
	dmtc0	v0,C0_SR
	b	1b
#ifdef SEMASTAT
	addu	a2,1				# increment spin count
#else
	nop
#endif

#else /* R4000 || R10000 */

	MFC0(v0,C0_SR)
#ifndef NO_MFC_NOPS
	/* Not needed according to architecture spec. Verified on IP19 */
	NOP_0_3
#endif /* NO_MFC_NOPS */	
	GET_CEL(t1)		# current value of CEL register

	or	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xor	t0,SR_IE
	or	t2,v0,SPLHI_DISABLED_INTS

#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif
	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)
#ifdef OBSOLETE
	/* Don't really need to delay waiting for interrupts to be blocked.
	 * If an interrupt hits this small 3 instruction window, then the
	 * "sc" will fail and we simply re-execute the loop.  This costs us
	 * 8 instructions or so when the interrupt hits the window but
	 * saves us 3 instuctions in the typical case.
	 */
	NOP_0_3
#endif /* OBSOLETE */	
	li	t3,SPLHI_CEL

	# See if lock is busy.
2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,1
	bne	ta1,zero,4f
	or	ta0,1

	# Try to mark lock busy
	LOCK_SC	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	li	ta2,SR_IMASK

	# We have the lock.
	# Enable IE and do an splhi.
	.set	reorder
	# Following instruction is macro on TFP.  On R4000, reorder will
	# place it in BDSLOT
	xor	t2,SPLHI_DISABLED_INTS	# BDSLOT
	bge	t1,t3,3f	/* don't lower spl */
	RAISE_CEL(t3,ta3)
3:
	.set	noreorder
	MTC0(t2,C0_SR)
	/* no need to add NOPs here */
	and	v0,ta2
#ifdef SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	or	a3,v0,t1			# BDSLOT
#elif SPLMETER
	or	v0,t1
	move	a0,ra
	LA	a2,splhi
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Briefly enable interrupts.
	# NOPs are needed here to let the interrupt occur before we block
	# them again.
	MTC0(v0,C0_SR)
	NOP_0_4
	b	1b
#ifdef SEMASTAT
	addu	a2,1				# increment spin count
#else
	nop
#endif
#endif /* R4000 || R10000 */
	.set 	reorder
	SPLEND(mutex_spinlock,__mutex_spinlock)	

/*
 * void mutex_spinunlock(lock_t *lp, int s)
 */
SPLLEAF(mutex_spinunlock,a2,_mutex_spinunlock,__mutex_spinunlock)
	.set	noreorder

	LOCK_LW	t0,0(a0)		# fetch lock word
#if DEBUG
	andi	t1,t0,1
	bne	t1,zero,1f
	nop
	break	1
1:
#endif
#if TFP
	# Note we execute other instructions here to fill the LDSLOT of t0
	# These should actually execute in the same superscaler cycle.
	dmfc0	v0,C0_SR		# get current SR
	li	t3,SR_IMASK

	ori	t0,1			# use or/xor to clear, rather than
	xori	t0,1			# two shifts (faster on TFP)
	LOCK_SW	t0,0(a0)		# free lock

#ifdef NO_KPREEMPT_FLAG	
	/* With kernel preemption we need to atomically update CEL amd C0_SR
	 * in order to correctly perform the preemption check (isspl0) in
	 * vec_int() when returning from an interrupt.
	 * Need to do this several cycles before LOWER_CEL since the first
	 * thing LOWER_CEL does is a cached "sb" of the new CEL and this is
	 * before SR_IE disable has taken affect.
	 */
	ori	ta0,v0,SR_IE
	xori	ta0,SR_IE
	dmtc0	ta0,C0_SR
#endif /* NO_KPREEMPT_FLAG */	
	
#ifndef SPLMETER
	# Lower spl
	and	t0,t3,a1		# t0 has old SR_IMASK bits
	andi	a1,OSPL_CEL		# a1 has old CEL

	or	v0,t3
	xor	v0,t3
	or	v0,t0			# v0 has new SR w/old IMASK
	
	LOWER_CEL(a1,v1,a0)
	MODIFY_UNLOCK_SR(v0)
	dmtc0	v0,C0_SR

	/* Following sequence guarentees to insert a cycle between the
	 * prededing "dmtc" and any subsequent "jal" that we may return
	 * to.  Note that new C0_SR may not be in effect until a superscaler
	 * cycle or two after we return.  Since this routine "lowers"
	 * interrupt priority, it shouldn't be a problem (besides, I
	 * understand that the WAIT_FOR_CEL is actually a many cycle
	 * operation, so new SR is probably in affect by the time we return.
	 */
	j	ra
	NOP_SSNOP
	/* no need to do NOP_0_4 here:  we can take an interrupt anytime */
#else
	move	a0,a1
	j	lowerspl
	move 	a1,ra
#endif

#else /* R4000 || R10000 */

	/*
	 * Should we really force the lock bit low, or let the
	 * misuse of the lock to become apparent (that is, only
	 * xori-ing the word just toggles the bit, and the next
	 * caller would find the lock erronously held).
	ori	t0,1			# use or/xor to clear, rather than
	 */
	xori	t0,1			# two shifts (faster on all CPUs)
	LOCK_SW	t0,0(a0)		# free lock

#ifndef SPLMETER
	# Lower spl
	MFC0(v0,C0_SR)			# get current SR
#ifdef NO_KPREEMPT_FLAG	
	/* With kernel preemption we need to atomically update CEL amd C0_SR
	 * in order to correctly perform the preemption check (isspl0) in
	 * vec_int() when returning from an interrupt.
	 * Need to do this several cycles before LOWER_CEL since the first
	 * thing LOWER_CEL does is a cached "sb" of the new CEL and this is
	 * before SR_IE disable has taken affect.
	 */
	ori	ta0,v0,SR_IE
	xori	ta0,SR_IE
	MTC0(ta0,C0_SR)
#endif /* NO_KPREEMPT_FLAG */	
	/* no need to do NOP_0_4 here:  we execute 4 insts before using v0 */
	move	t0,a1			# copy arg
	li	t3,SR_IMASK
	and	t0,t3			# t0 has old SR_IMASK bits
	andi	a1,OSPL_CEL		# a1 has old CEL
	

	or	v0,t3
	xor	v0,t3
	or	v0,t0			# v0 has new SR w/old IMASK

	LOWER_CEL(a1,v1,a0)
	MODIFY_UNLOCK_SR(v0)
	MTC0_AND_JR(v0,C0_SR,ra)
	/* no need to do NOP_0_4 here:  we can take an interrupt anytime */
#else
	move	a0,a1
	j	lowerspl
	move 	a1,ra
#endif
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
	nop

	# We have the lock.
#ifdef SEMASTAT
	j	lstat_update_spin
	move	a1,ra				# BDSLOT

8:	b	1b
	addu	a2,1				# BDSLOT
#else
	j	ra
	nop
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
	nop
	break	1
1:
#endif
	xor	v0,1
	j	ra
	LOCK_SW	v0,0(a0)		# free lock
	.set	reorder
	END(nested_spinunlock)	

LEAF(nested_spintrylock)
	.set	noreorder
	# a0 - contains address of pointer-sized lock
	# v0 - returns 1 on success, 0 on failure

1:	LOCK_LL	v0,0(a0)	# see if lock is busy
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
	nop

	# We have the lock.
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0
#else
	j	ra
	nop
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
	move	v0,zero		# returns failure
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
	# t1 - old CEL value
	# t2 - a proper splhi value for SR
	# t3 - EVINTR_LEVEL_HIGH+1
	# v0 - old IMASK

#if TFP
	DMFC0(v0,C0_SR)
	GET_CEL(t1)		/* t1 = current CEL */
	ori	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xori	t0,SR_IE

#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif
	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc

	dmtc0	t0,C0_SR

	/* The following instructions used to be in the loop pre-amble.
	 * Moved them here since we were doing SSNOPS anyway, and this
	 * speeds up the common case of a single pass through the loop.
	 * CODE MUST BE RE-EXECUTABLE SINCE LOOP MAY BE EXECUTED MORE
	 * THAN ONCE.
	 * THERE MUST BE 4 SUPERSCALER CYCLES FROM THE "dmtc" ABOVE UNTIL
	 * THE "PTR_SC" BELOW SO THAT INTERRUPTS ARE DISABLED WHEN THAT
	 * INSTRUCTION EXECUTES.
	 * cycle count analysis (worst case):
	 *	dmtc0			[0]
	 *      LI      (first inst)	[0]  worst case
	 *	LI	(2nd inst)	[1]
	 *	or			[1]
	 * 	xor			[2]
	 *	PTR_LL			[2]
	 *	and			[3]
	 *	bne			[3]
	 *	or			[3]
	 *	PTR_SC			[4]
	 */

	LI	ta0,SPLHI_DISABLED_INTS
	or	t2,v0,ta0
	xor	t2,ta0			/* disable interrupts (splhi) */

	/* Former pre-amble ends here.
	 * See if lock is busy.
	 */

2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy (interrupts disabled by now)

	sc	ta0,0(a0)	
	beq	ta0,zero,2b
	li	t3,SPLHI_CEL

	# We have the lock.
	# Enable IE and do an splhi.

	.set	reorder
	li	ta0,SR_IMASK
	bge	t1,t3,3f	/* don't lower spl */
	.set	noreorder

	RAISE_CEL(t3,ta3)
3:
	dmtc0	t2,C0_SR
	and	v0,ta0
#ifdef SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	or	a3,v0,t1			# BDSLOT
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Briefly enable interrupts.
	# Don't worry about delay until this takes affect,
	# since the dmtc at label "1:" will have the same delay
	# and hence interrupts will be enabled for a few cycles.
	dmtc0	v0,C0_SR
	b	1b
#ifndef SEMASTAT
	nop
#else
	addu	a2,1
#endif

#else /* R4000 || R10000 */

#ifdef SEMASTAT
	li	a2,0				# set action = nospin
#endif
	MFC0(v0,C0_SR)
#ifndef NO_MFC_NOPS
	/* Not needed according to architecture spec. Verified on IP19 */
	NOP_0_3
#endif /* NO_MFC_NOPS */	
	GET_CEL(t1)		# current value of CEL register
	or	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xor	t0,SR_IE
	or	t2,v0,SPLHI_DISABLED_INTS

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)
#ifdef OBSOLETE
	/* Don't really need to delay waiting for interrupts to be blocked.
	 * If an interrupt hits this small 3 instruction window, then the
	 * "sc" will fail and we simply re-execute the loop.  This costs us
	 * 8 instructions or so when the interrupt hits the window but
	 * saves us 3 instuctions in the typical case.
	 */
	NOP_0_3
#endif /* OBSOLETE */	
	li	t3,SPLHI_CEL

	# See if lock is busy.
2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy
	sc	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	li	ta2,SR_IMASK

	# We have the lock.
	# Enable IE and do an splhi.
	.set	reorder
	# Following instruction is macro on TFP.  On R4000, reorder will
	# place it in BDSLOT
	xor	t2,SPLHI_DISABLED_INTS
	bge	t1,t3,3f	/* don't lower spl */
	RAISE_CEL(t3,ta3)
3:
	.set	noreorder
	MTC0(t2,C0_SR)
	/* no need to add NOPs here */
	and	v0,ta2
#ifdef SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	or	a3,v0,t1			# BDSLOT
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Briefly enable interrupts.
	MTC0(v0,C0_SR)
	NOP_0_4
	b	1b
#ifndef SEMASTAT
	nop
#else
	addu	a2,1
#endif
#endif /* R4000 || R10000 */
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
	# t1 - old CEL value
	# t2 - a proper splhi value for SR
	# t3 - EVINTR_LEVEL_HIGH+1
	# v0 - old IMASK

	move	v1,a2				# save caller_ra for lstat_update
#if TFP
	DMFC0(v0,C0_SR)
	GET_CEL(t1)		/* t1 = current CEL */
	ori	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xori	t0,SR_IE

	li	a2,0				# set action = nospin

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc

	dmtc0	t0,C0_SR

	/* The following instructions used to be in the loop pre-amble.
	 * Moved them here since we were doing SSNOPS anyway, and this
	 * speeds up the common case of a single pass through the loop.
	 * CODE MUST BE RE-EXECUTABLE SINCE LOOP MAY BE EXECUTED MORE
	 * THAN ONCE.
	 * THERE MUST BE 4 SUPERSCALER CYCLES FROM THE "dmtc" ABOVE UNTIL
	 * THE "PTR_SC" BELOW SO THAT INTERRUPTS ARE DISABLED WHEN THAT
	 * INSTRUCTION EXECUTES.
	 * cycle count analysis (worst case):
	 *	dmtc0			[0]
	 *      LI      (first inst)	[0]  worst case
	 *	LI	(2nd inst)	[1]
	 *	or			[1]
	 * 	xor			[2]
	 *	PTR_LL			[2]
	 *	and			[3]
	 *	bne			[3]
	 *	or			[3]
	 *	PTR_SC			[4]
	 */

	LI	ta0,SPLHI_DISABLED_INTS
	or	t2,v0,ta0
	xor	t2,ta0			/* disable interrupts (splhi) */

	/* Former pre-amble ends here.
	 * See if lock is busy.
	 */

2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy (interrupts disabled by now)

	sc	ta0,0(a0)	
	beq	ta0,zero,2b
	li	t3,SPLHI_CEL

	# We have the lock.
	# Enable IE and do an splhi.

	.set	reorder
	li	ta0,SR_IMASK
	bge	t1,t3,3f	/* don't lower spl */
	.set	noreorder

	RAISE_CEL(t3,ta3)
3:
	dmtc0	t2,C0_SR
	and	v0,ta0

	move	a1,ra
	PTR_ADDU a1,v1,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	j	lstat_update_spin
	or	a3,v0,t1			# BDSLOT

4: 	# Lock is busy.  Briefly enable interrupts.
	# Don't worry about delay until this takes affect,
	# since the dmtc at label "1:" will have the same delay
	# and hence interrupts will be enabled for a few cycles.
	dmtc0	v0,C0_SR
	b	1b
	addu	a2,1

#else /* R4000 || R10000 */

	li	a2,0				# set action = nospin

	MFC0(v0,C0_SR)
#ifndef NO_MFC_NOPS
	/* Not needed according to architecture spec. Verified on IP19 */
	NOP_0_3
#endif /* NO_MFC_NOPS */	
	GET_CEL(t1)		# current value of CEL register
	or	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xor	t0,SR_IE
	or	t2,v0,SPLHI_DISABLED_INTS

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)
#ifdef OBSOLETE
	/* Don't really need to delay waiting for interrupts to be blocked.
	 * If an interrupt hits this small 3 instruction window, then the
	 * "sc" will fail and we simply re-execute the loop.  This costs us
	 * 8 instructions or so when the interrupt hits the window but
	 * saves us 3 instuctions in the typical case.
	 */
	NOP_0_3
#endif /* OBSOLETE */	
	li	t3,SPLHI_CEL

	# See if lock is busy.
2:	ll	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy
	sc	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	li	ta2,SR_IMASK

	# We have the lock.
	# Enable IE and do an splhi.
	.set	reorder
	# Following instruction is macro on TFP.  On R4000, reorder will
	# place it in BDSLOT
	xor	t2,SPLHI_DISABLED_INTS
	bge	t1,t3,3f	/* don't lower spl */
	RAISE_CEL(t3,ta3)
3:
	.set	noreorder
	MTC0(t2,C0_SR)
	/* no need to add NOPs here */
	and	v0,ta2

	move	a1,ra
	PTR_ADDU a1,v1,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	j	lstat_update_spin
	or	a3,v0,t1			# BDSLOT

4: 	# Lock is busy.  Briefly enable interrupts.
	MTC0(v0,C0_SR)
	NOP_0_4
	b	1b
	addu	a2,1
#endif /* R4000 || R10000 */
	.set 	reorder
	SPLEND(mutex_bitlock_ra,__mutex_bitlock)	

/*
 * int rv = mutex_bittrylock_ra(uint_t *lock, uint bit, void *ra)
 *
 * Returns: old priority level on success, 0 on failure.
 */
SPLLEAF(mutex_bittrylock_ra,a2,_mutex_bittrylock,__mutex_bittrylock)
	.set 	noreorder

	# First, set up values needed in the body of bitlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t1 - old CEL value
	# t2 - a proper splhi value for SR
	# t3 - EVINTR_LEVEL_HIGH+1
	# v0 - old IMASK

#if TFP
	DMFC0(v0,C0_SR)	
	lbu	t1,VPDA_CELSHADOW(zero)	/* t1 = current CEL */
	ori	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xori	t0,SR_IE

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc

	dmtc0	t0,C0_SR

	/* The following instructions used to be in the loop pre-amble.
	 * Moved them here since we were doing SSNOPS anyway, and this
	 * speeds up the common case of a single pass through the loop.
	 * CODE MUST BE RE-EXECUTABLE SINCE LOOP MAY BE EXECUTED MORE
	 * THAN ONCE.
	 * THERE MUST BE 4 SUPERSCALER CYCLES FROM THE "dmtc" ABOVE UNTIL
	 * THE "PTR_SC" BELOW SO THAT INTERRUPTS ARE DISABLED WHEN THAT
	 * INSTRUCTION EXECUTES.
	 * cycle count analysis (worst case):
	 *	dmtc0			[0]
	 *      LI      (first inst)	[0]  worst case
	 *	LI	(2nd inst)	[1]
	 *	or			[1]
	 * 	xor			[2]
	 *	PTR_LL			[2]
	 *	and			[3]
	 *	bne			[3]
	 *	or			[3]
	 *	PTR_SC			[4]
	 */

	LI	ta0,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
	or	t2,v0,ta0
	xor	t2,ta0			/* disable interrupts (splhi) */

	/* Former pre-amble ends here.
	 * See if lock is busy.
	 */

2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy (interrupts disabled by now)

	LOCK_SC	ta0,0(a0)	
	beq	ta0,zero,2b
	li	t3,EVINTR_LEVEL_HIGH+1

	# We have the lock.
	# Enable IE and do an splhi.

	.set	reorder
	li	ta0,SR_IMASK
	bge	t1,t3,3f	/* don't lower spl */
	.set	noreorder

	RAISE_CEL(t3,ta3)
3:
	dmtc0	t2,C0_SR
	and	v0,ta0

	/*
	 * Ensure non-zero value for successful return.  (Sometimes both the
	 * old interrupt mask and CEL can be zero ...)
	 */
	ori	v0,OSPL_TRY1

	move	a1,ra
	li	a2,0
	j	lstat_update
	or	a3,v0,t1

4: 	# Lock is busy.  Just punt.
	dmtc0	v0,C0_SR
	PTR_ADDU a1,a2,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	li	a2,3
	j	lstat_update
	move	a3,zero

#else /* R4000 || R10000 */

	MFC0(v0,C0_SR)
#ifndef NO_MFC_NOPS
	/* Not needed according to architecture spec. Verified on IP19 */
	NOP_0_3
#endif /* NO_MFC_NOPS */	
	lbu	t1,VPDA_CELSHADOW(zero)	# current value of CEL register
	or	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xor	t0,SR_IE
	or	t2,v0,SPLHI_DISABLED_INTS

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)
#ifdef OBSOLETE
	/* Don't really need to delay waiting for interrupts to be blocked.
	 * If an interrupt hits this small 3 instruction window, then the
	 * "sc" will fail and we simply re-execute the loop.  This costs us
	 * 8 instructions or so when the interrupt hits the window but
	 * saves us 3 instuctions in the typical case.
	 */
	NOP_0_3
#endif /* OBSOLETE */	
	li	t3,SPLHI_CEL

	# See if lock is busy.
2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy
	LOCK_SC	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	li	ta2,SR_IMASK

	# We have the lock.
	# Enable IE and do an splhi.
	.set	reorder
	# Following instruction is macro on TFP.  On R4000, reorder will
	# place it in BDSLOT
	xor	t2,SPLHI_DISABLED_INTS	# BDSLOT
	bge	t1,t3,3f	/* don't lower spl */
	RAISE_CEL(t3,ta3)
3:
	.set	noreorder
	MTC0(t2,C0_SR)
	/* no need to add NOPs here */
	and	v0,ta2

	/*
	 * Ensure non-zero value for successful return.  (Sometimes both the
	 * old interrupt mask and CEL can be zero ...)
	 */
	ori	v0,OSPL_TRY1
	PTR_ADDU a1,a2,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	li	a2,0
	j	lstat_update
	or	a3,v0,t1

4: 	# Lock is busy.  Just punt.
	MTC0(v0,C0_SR)
	PTR_ADDU a1,a2,1			# Use RA+1 to indicate MR spinlock
	PTR_SUBU a0,4				# Convert to mrlock address + 1
	li	a2,3
	j	lstat_update
	move	a3,zero

#endif /* R4000 || R10000 */
	.set 	reorder
	SPLEND(mutex_bittrylock_ra,__mutex_bittrylock)	
#endif /* SEMASTAT */

/*
 * void mutex_bitunlock(uint *lp, uint bit, int s)
 */
SPLLEAF(mutex_bitunlock,a3,_mutex_bitunlock,__mutex_bitunlock)
	.set	noreorder

	lw	t0,0(a0)		# fetch lock word
#if DEBUG
	and	t1,t0,a1
	bne	t1,zero,1f
	nop
	break	1
1:
#endif
#if TFP
	# Note we execute other instructions here to fill the LDSLOT of t0
	# These should actually execute in the same superscaler cycle.
	dmfc0	v0,C0_SR		# get current SR
	li	t3,SR_IMASK

	xor	t0,a1
	sw	t0,0(a0)		# free lock

#ifndef SPLMETER
#ifdef NO_KPREEMPT_FLAG	
	/* With kernel preemption we need to atomically update CEL amd C0_SR
	 * in order to correctly perform the preemption check (isspl0) in
	 * vec_int() when returning from an interrupt.
	 * Need to do this several cycles before LOWER_CEL since the first
	 * thing LOWER_CEL does is a cached "sb" of the new CEL and this is
	 * before SR_IE disable has taken affect.
	 */
	ori	ta0,v0,SR_IE
	xori	ta0,SR_IE
	dmtc0	ta0,C0_SR
#endif /* NO_KPREEMPT_FLAG */	
	
	# Lower spl
	and	t0,t3,a2		# t0 has old SR_IMASK bits
	andi	a2,OSPL_CEL		# a2 has old CEL

	or	v0,t3
	xor	v0,t3
	or	v0,t0			# v0 has new SR w/old IMASK

	LOWER_CEL(a2,v1,a0)
	MODIFY_UNLOCK_SR(v0)
	dmtc0	v0,C0_SR
	/* Following sequence guarentees to insert a cycle between the
	 * prededing "dmtc" and any subsequent "jal" that we may return
	 * to.  Note that new C0_SR may not be in effect until a superscaler
	 * cycle or two after we return.  Since this routine "lowers"
	 * interrupt priority, it shouldn't be a problem (besides, I
	 * understand that the WAIT_FOR_CEL is actually a many cycle
	 * operation, so new SR is probably in affect by the time we return.
	 */
	j	ra
	NOP_SSNOP
	/* no need to do NOP_0_4 here:  we can take an interrupt anytime */
#else
	move	a0,a2
	j	lowerspl
	move	a1,ra
#endif

#else /* R4000 || R10000 */

	xor	t0,a1
	sw	t0,0(a0)		# free lock

#ifndef SPLMETER
	# Lower spl
	MFC0(v0,C0_SR)			# get current SR
#ifdef NO_KPREEMPT_FLAG	
	/* With kernel preemption we need to atomically update CEL amd C0_SR
	 * in order to correctly perform the preemption check (isspl0) in
	 * vec_int() when returning from an interrupt.
	 * Need to do this several cycles before LOWER_CEL since the first
	 * thing LOWER_CEL does is a cached "sb" of the new CEL and this is
	 * before SR_IE disable has taken affect.
	 */
	ori	ta0,v0,SR_IE
	xori	ta0,SR_IE
	MTC0(ta0,C0_SR)
#endif /* NO_KPREEMPT_FLAG */	
	/*
	 * no need to do NOP_0_4 here:
	 * we execute 4 insts before using v0
	 */
	move	t0,a2			# copy arg
	li	t3,SR_IMASK
	and	t0,t3			# t0 has old SR_IMASK bits
	andi	a2,OSPL_CEL		# 21 has old CEL

	or	v0,t3
	xor	v0,t3
	or	v0,t0			# v0 has new SR w/old IMASK

	LOWER_CEL(a2,v1,a0)
	MODIFY_UNLOCK_SR(v0)
	MTC0_AND_JR(v0,C0_SR,ra)
	/* no need to do NOP_0_4 here:  we can take an interrupt anytime */
#else
	move	a0,a2
	j	lowerspl
	move	a1,ra
#endif
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
	nop

	# We have the lock.
#ifdef SEMASTAT
	j	lstat_update_spin
	move	a1,ra				# BDSLOT

8:	b	1b
	addu	a2,1				# BDSLOT - increment spin count
#else
	j	ra
	nop
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
	nop
	break	1
1:
#endif
	xor	v0,a1
	j	ra
	sw	v0,0(a0)
	.set	reorder
	END(nested_bitunlock)	

/*
 * int rv = mutex_64bitlock(__uint64_t *lp, __uint64_t bit)
 */
SPLLEAF(mutex_64bitlock,a2,_mutex_64bitlock,__mutex_64bitlock)
	.set 	noreorder

	# First, set up values needed in the body of spinlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t1 - old CEL value
	# t2 - a proper splhi value for SR
	# t3 - EVINTR_LEVEL_HIGH+1
	# v0 - old IMASK

#if TFP
	DMFC0(v0,C0_SR)
	GET_CEL(t1)		/* t1 = current CEL */
	ori	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xori	t0,SR_IE
#ifdef SEMASTAT
	li	a2,0				# action = nospin
#endif

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc

	dmtc0	t0,C0_SR

	/* The following instructions used to be in the loop pre-amble.
	 * Moved them here since we were doing SSNOPS anyway, and this
	 * speeds up the common case of a single pass through the loop.
	 * CODE MUST BE RE-EXECUTABLE SINCE LOOP MAY BE EXECUTED MORE
	 * THAN ONCE.
	 * THERE MUST BE 4 SUPERSCALER CYCLES FROM THE "dmtc" ABOVE UNTIL
	 * THE "PTR_SC" BELOW SO THAT INTERRUPTS ARE DISABLED WHEN THAT
	 * INSTRUCTION EXECUTES.
	 * cycle count analysis (worst case):
	 *	dmtc0			[0]
	 *      LI      (first inst)	[0]  worst case
	 *	LI	(2nd inst)	[1]
	 *	or			[1]
	 * 	xor			[2]
	 *	PTR_LL			[2]
	 *	and			[3]
	 *	bne			[3]
	 *	or			[3]
	 *	PTR_SC			[4]
	 */

	LI	ta0,SPLHI_DISABLED_INTS
	or	t2,v0,ta0
	xor	t2,ta0			/* disable interrupts (splhi) */

	/* Former pre-amble ends here.
	 * See if lock is busy.
	 */

2:	lld	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy (interrupts disabled by now)

	scd	ta0,0(a0)	
	beq	ta0,zero,2b
	li	t3,SPLHI_CEL

	# We have the lock.
	# Enable IE and do an splhi.

	.set	reorder
	li	ta0,SR_IMASK
	bge	t1,t3,3f	/* don't lower spl */
	.set	noreorder

	RAISE_CEL(t3,ta3)
3:
	dmtc0	t2,C0_SR
	and	v0,ta0
#ifdef SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	or	a3,v0,t1
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Briefly enable interrupts.
	# Don't worry about delay until this takes affect,
	# since the dmtc at label "1:" will have the same delay
	# and hence interrupts will be enabled for a few cycles.
	dmtc0	v0,C0_SR
	b	1b
#ifdef SEMASTAT
	addu	a2,1				# BDSLOT - increment spin count
#else
	nop
#endif

#else /* R4000 || R10000 */

	MFC0(v0,C0_SR)
	PTR_SRL	a0,PTR_SCALESHIFT
	PTR_SLL	a0,PTR_SCALESHIFT	# (double)word-align lock addr in a0
#ifndef NO_MFC_NOPS	
	/* Not needed according to architecture spec. Verified on IP19 */
	NOP_0_3
#endif /* NO_MFC_NOPS */	
	GET_CEL(t1)		# current value of CEL register
	or	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xor	t0,SR_IE
	or	t2,v0,SPLHI_DISABLED_INTS

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)
#ifdef OBSOLETE
	/* Don't really need to delay waiting for interrupts to be blocked.
	 * If an interrupt hits this small 3 instruction window, then the
	 * "sc" will fail and we simply re-execute the loop.  This costs us
	 * 8 instructions or so when the interrupt hits the window but
	 * saves us 3 instuctions in the typical case.
	 */
	NOP_0_3
#endif /* OBSOLETE */	
	li	t3,SPLHI_CEL
#ifdef SEMASTAT
	li	a2,0				# action = nospin
#endif

	# See if lock is busy.
2:	lld	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy
	scd	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	li	ta2,SR_IMASK

	# We have the lock.
	# Enable IE and do an splhi.
	.set	reorder
	# Following instruction is macro on TFP.  On R4000, reorder will
	# place it in BDSLOT
	xor	t2,SPLHI_DISABLED_INTS	 # BDSLOT
	bge	t1,t3,3f	/* don't lower spl */
	RAISE_CEL(t3,ta3)
3:
	.set	noreorder
	MTC0(t2,C0_SR)
	/* no need to add NOPs here */
	and	v0,ta2
#ifdef SEMASTAT
	move	a1,ra
	j	lstat_update_spin
	or	a3,v0,t1
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Briefly enable interrupts.
	MTC0(v0,C0_SR)
	NOP_0_4
	b	1b
#ifdef SEMASTAT
	addu	a2,1				# BDSLOT - increment spin count
#else
	nop
#endif
#endif /* R4000 || R10000 */
	.set 	reorder
	SPLEND(mutex_64bitlock,__mutex_64bitlock)	

/*
 * void mutex_64bitunlock(__uint64_t *lp, __uint64_t bit, int s)
 */
SPLLEAF(mutex_64bitunlock,a3,_mutex_64bitunlock, __mutex_64bitunlock)
	.set	noreorder

	ld	t0,0(a0)		# fetch lock word
#if DEBUG
	and	t1,t0,a1
	bne	t1,zero,1f
	nop
	break	1
1:
#endif
#if TFP
	# Note we execute other instructions here to fill the LDSLOT of t0
	# These should actually execute in the same superscaler cycle.
	dmfc0	v0,C0_SR		# get current SR
	li	t3,SR_IMASK

	xor	t0,a1
	sd	t0,0(a0)		# free lock

#ifndef SPLMETER
#ifdef NO_KPREEMPT_FLAG	
	/* With kernel preemption we need to atomically update CEL amd C0_SR
	 * in order to correctly perform the preemption check (isspl0) in
	 * vec_int() when returning from an interrupt.
	 * Need to do this several cycles before LOWER_CEL since the first
	 * thing LOWER_CEL does is a cached "sb" of the new CEL and this is
	 * before SR_IE disable has taken affect.
	 */
	ori	ta0,v0,SR_IE
	xori	ta0,SR_IE
	dmtc0	ta0,C0_SR
#endif /* NO_KPREEMPT_FLAG */	
	# Lower spl
	and	t0,t3,a2		# t0 has old SR_IMASK bits
	andi	a2,OSPL_CEL		# a2 has old CEL

	or	v0,t3
	xor	v0,t3
	or	v0,t0			# v0 has new SR w/old IMASK

	LOWER_CEL(a2,v1,a0)
	dmtc0	v0,C0_SR
	/* Following sequence guarentees to insert a cycle between the
	 * prededing "dmtc" and any subsequent "jal" that we may return
	 * to.  Note that new C0_SR may not be in effect until a superscaler
	 * cycle or two after we return.  Since this routine "lowers"
	 * interrupt priority, it shouldn't be a problem (besides, I
	 * understand that the WAIT_FOR_CEL is actually a many cycle
	 * operation, so new SR is probably in affect by the time we return.
	 */
	j	ra
	NOP_SSNOP
	/* no need to do NOP_0_4 here:  we can take an interrupt anytime */
#else
	move	a0,a2
	j	lowerspl
	move	a1,ra
#endif

#else /* R4000 || R10000 */

	xor	t0,a1
	sd	t0,0(a0)		# free lock

#ifndef SPLMETER
	# Lower spl
	MFC0(v0,C0_SR)			# get current SR
#ifdef NO_KPREEMPT_FLAG	
	/* With kernel preemption we need to atomically update CEL amd C0_SR
	 * in order to correctly perform the preemption check (isspl0) in
	 * vec_int() when returning from an interrupt.
	 * Need to do this several cycles before LOWER_CEL since the first
	 * thing LOWER_CEL does is a cached "sb" of the new CEL and this is
	 * before SR_IE disable has taken affect.
	 */
	ori	ta0,v0,SR_IE
	xori	ta0,SR_IE
	MTC0(ta0,C0_SR)
#endif /* NO_KPREEMPT_FLAG */	
	/*
	 * no need to do NOP_0_4 here:
	 * we execute 4 insts before using v0
	 */
	move	t0,a2			# copy arg
	li	t3,SR_IMASK
	and	t0,t3			# t0 has old SR_IMASK bits
	andi	a2,OSPL_CEL		# 21 has old CEL

	or	v0,t3
	xor	v0,t3
	or	v0,t0			# v0 has new SR w/old IMASK

	LOWER_CEL(a2,v1,a0)
	MODIFY_UNLOCK_SR(v0)
	MTC0_AND_JR(v0,C0_SR,ra)
	/* no need to do NOP_0_4 here:  we can take an interrupt anytime */
#else
	move	a0,a2
	j	lowerspl
	move	a1,ra
#endif
#endif
	.set 	reorder
	SPLEND(mutex_64bitunlock,__mutex_64bitunlock)	

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
	nop

	# We have the lock.
#ifdef SEMASTAT
	j	lstat_update_spin
	move	a1,ra				# BDSLOT

8:	b	1b
	addu	a2,1				# BDSLOT - increment spin count
#else
	j	ra
	nop
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
	nop
	break	1
1:
#endif
	xor	v0,a1
	j	ra
	sd	v0,0(a0)
	.set	reorder
	END(nested_64bitunlock)	

/*
 * int rv = mutex_spintrylock(mutex_t *mp)
 */
SPLLEAF(mutex_spintrylock, a1, _mutex_spintrylock, __mutex_spintrylock)
	.set 	noreorder

	# First, set up values needed in the body of spinlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t1 - old CEL value
	# t2 - a proper splhi value for SR
	# t3 - EVINTR_LEVEL_HIGH+1
	# v0 - old IMASK

#if TFP
	DMFC0(v0,C0_SR)	
	lbu	t1,VPDA_CELSHADOW(zero)	/* t1 = current CEL */
	ori	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xori	t0,SR_IE

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc

	dmtc0	t0,C0_SR

	/* The following instructions used to be in the loop pre-amble.
	 * Moved them here since we were doing SSNOPS anyway, and this
	 * speeds up the common case of a single pass through the loop.
	 * CODE MUST BE RE-EXECUTABLE SINCE LOOP MAY BE EXECUTED MORE
	 * THAN ONCE.
	 * THERE MUST BE 4 SUPERSCALER CYCLES FROM THE "dmtc" ABOVE UNTIL
	 * THE "PTR_SC" BELOW SO THAT INTERRUPTS ARE DISABLED WHEN THAT
	 * INSTRUCTION EXECUTES.
	 * cycle count analysis (worst case):
	 *	dmtc0			[0]
	 *      LI      (first inst)	[0]  worst case
	 *	LI	(2nd inst)	[1]
	 *	or			[1]
	 * 	xor			[2]
	 *	PTR_LL			[2]
	 *	and			[3]
	 *	bne			[3]
	 *	or			[3]
	 *	PTR_SC			[4]
	 */

	LI	ta0,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
	or	t2,v0,ta0
	xor	t2,ta0			/* disable interrupts (splhi) */

	/* Former pre-amble ends here.
	 * See if lock is busy.
	 */

2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,1
	bne	ta1,zero,4f
	or	ta0,1

	# Try to mark lock busy (interrupts disabled by now)

	LOCK_SC	ta0,0(a0)	
	beq	ta0,zero,2b
	li	t3,EVINTR_LEVEL_HIGH+1

	# We have the lock.
	# Enable IE and do an splhi.

	.set	reorder
	li	ta0,SR_IMASK
	bge	t1,t3,3f	/* don't lower spl */
	.set	noreorder

	RAISE_CEL(t3,ta3)
3:
	dmtc0	t2,C0_SR
	and	v0,ta0

	/*
	 * Ensure non-zero value for successful return.  (Sometimes both the
	 * old interrupt mask and CEL can be zero ...)
	 */
	ori	v0,OSPL_TRY1
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	or	a3,v0,t1
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Just punt.
	dmtc0	v0,C0_SR
#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero
#else
	j	ra
	move	v0,zero
#endif

#else /* R4000 || R10000 */

	MFC0(v0,C0_SR)
#ifndef NO_MFC_NOPS	
	/* Not needed according to architecture spec. Verified on IP19 */
	NOP_0_3
#endif /* NO_MFC_NOPS */	
	lbu	t1,VPDA_CELSHADOW(zero)	# current value of CEL register
	or	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xor	t0,SR_IE
	or	t2,v0,SPLHI_DISABLED_INTS

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)
#ifdef OBSOLETE
	/* Don't really need to delay waiting for interrupts to be blocked.
	 * If an interrupt hits this small 3 instruction window, then the
	 * "sc" will fail and we simply re-execute the loop.  This costs us
	 * 8 instructions or so when the interrupt hits the window but
	 * saves us 3 instuctions in the typical case.
	 */
	NOP_0_3
#endif /* OBSOLETE */	
	li	t3,SPLHI_CEL

	# See if lock is busy.
2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,1
	bne	ta1,zero,4f
	or	ta0,1

	# Try to mark lock busy
	LOCK_SC	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	li	ta2,SR_IMASK

	# We have the lock.
	# Enable IE and do an splhi.
	.set	reorder
	# Following instruction is macro on TFP.  On R4000, reorder will
	# place it in BDSLOT
	xor	t2,SPLHI_DISABLED_INTS	# BDSLOT
	bge	t1,t3,3f	/* don't lower spl */
	RAISE_CEL(t3,ta3)
3:
	.set	noreorder
	MTC0(t2,C0_SR)
	/* no need to add NOPs here */
	and	v0,ta2

	/*
	 * Ensure non-zero value for successful return.  (Sometimes both the
	 * old interrupt mask and CEL can be zero ...)
	 */
	ori	v0,OSPL_TRY1
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	or	a3,v0,t1
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Just punt.
	MTC0(v0,C0_SR)
	NOP_0_2
#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero
#else
	j	ra
	move	v0,zero
#endif
#endif /* R4000 || R10000 */
	.set 	reorder
	SPLEND(mutex_spintrylock,__mutex_spintrylock)	

/*
 * int ospl = mutex_spintrylock_spl(lock_t *lock, splfunc_t);
 */
SPLNESTED(mutex_spintrylock_spl, SPINSPLFRM, zero, a2, _mutex_spintrylock_spl, __mutex_spintrylock_spl)
	.set	noreorder
	PTR_SUBU sp,SPINSPLFRM
	REG_S	ra,RA_SPINOFF(sp)
	REG_S	s1,S1_SPINOFF(sp)
	move	s1,a1		# remember spl routine address
	REG_S	s0,S0_SPINOFF(sp)
	move	s0,a0

	# call spl routine
	jal	s1
	nop

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
	nop
#ifdef SEMASTAT
	li	a2,0
#endif

	# We have the lock.
	/*
	 * Ensure non-zero value for successful return.  (Sometimes both the
	 * old interrupt mask and CEL can be zero ...)
	 */
	ori	v0,OSPL_TRY1

#ifdef SEMASTAT
2:	move	a0,s0
	REG_L	ra,RA_SPINOFF(sp)
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	move	a1,ra
	move	a3,v0
	j	lstat_update
	PTR_ADDU sp,SPINSPLFRM			# BDSLOT
#elif SPLMETER
2:	REG_L	ra,RA_SPINOFF(sp)
	REG_L	s0,S0_SPINOFF(sp)

	move	a2,s1				# address of spl function
	REG_L	s1,S1_SPINOFF(sp)
	PTR_ADDU sp,SPINSPLFRM
	move	a0,ra
	j	raisespl_spl
	move	a1,v0				# BDSLOT
#else
2:	REG_L	ra,RA_SPINOFF(sp)
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	j	ra
	PTR_ADDU sp,SPINSPLFRM
#endif

3: 	# Lock is busy.
#ifndef SPLMETER
	jal	splx
	move	a0,v0
#else
	jal	__splx
	move	a0,v0
#endif
#ifdef SEMASTAT
	li	a2,3
#endif
	b	2b
	move	v0,zero

	.set	reorder
	SPLEND(mutex_spintrylock_spl,__mutex_spintrylock_spl)

/*
 * int rv = mutex_bittrylock(uint_t *lock, uint bit)
 *
 * Returns: old priority level on success, 0 on failure.
 */
SPLLEAF(mutex_bittrylock,a2,_mutex_bittrylock,__mutex_bittrylock)
#ifndef SEMASTAT
XLEAF(mutex_bittrylock_ra)
#endif
	.set 	noreorder

	# First, set up values needed in the body of bitlock:
	# t0 - previous SR value with the IE bit zeroed.
	# t1 - old CEL value
	# t2 - a proper splhi value for SR
	# t3 - EVINTR_LEVEL_HIGH+1
	# v0 - old IMASK

#if TFP
	DMFC0(v0,C0_SR)	
	lbu	t1,VPDA_CELSHADOW(zero)	/* t1 = current CEL */
	ori	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xori	t0,SR_IE

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc

	dmtc0	t0,C0_SR

	/* The following instructions used to be in the loop pre-amble.
	 * Moved them here since we were doing SSNOPS anyway, and this
	 * speeds up the common case of a single pass through the loop.
	 * CODE MUST BE RE-EXECUTABLE SINCE LOOP MAY BE EXECUTED MORE
	 * THAN ONCE.
	 * THERE MUST BE 4 SUPERSCALER CYCLES FROM THE "dmtc" ABOVE UNTIL
	 * THE "PTR_SC" BELOW SO THAT INTERRUPTS ARE DISABLED WHEN THAT
	 * INSTRUCTION EXECUTES.
	 * cycle count analysis (worst case):
	 *	dmtc0			[0]
	 *      LI      (first inst)	[0]  worst case
	 *	LI	(2nd inst)	[1]
	 *	or			[1]
	 * 	xor			[2]
	 *	PTR_LL			[2]
	 *	and			[3]
	 *	bne			[3]
	 *	or			[3]
	 *	PTR_SC			[4]
	 */

	LI	ta0,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
	or	t2,v0,ta0
	xor	t2,ta0			/* disable interrupts (splhi) */

	/* Former pre-amble ends here.
	 * See if lock is busy.
	 */

2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy (interrupts disabled by now)

	LOCK_SC	ta0,0(a0)	
	beq	ta0,zero,2b
	li	t3,EVINTR_LEVEL_HIGH+1

	# We have the lock.
	# Enable IE and do an splhi.

	.set	reorder
	li	ta0,SR_IMASK
	bge	t1,t3,3f	/* don't lower spl */
	.set	noreorder

	RAISE_CEL(t3,ta3)
3:
	dmtc0	t2,C0_SR
	and	v0,ta0

	/*
	 * Ensure non-zero value for successful return.  (Sometimes both the
	 * old interrupt mask and CEL can be zero ...)
	 */
	ori	v0,OSPL_TRY1
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	or	a3,v0,t1
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Just punt.
	dmtc0	v0,C0_SR
#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero
#else
	j	ra
	move	v0,zero
#endif

#else /* R4000 || R10000 */

	MFC0(v0,C0_SR)
#ifndef NO_MFC_NOPS
	/* Not needed according to architecture spec. Verified on IP19 */
	NOP_0_3
#endif /* NO_MFC_NOPS */	
	lbu	t1,VPDA_CELSHADOW(zero)	# current value of CEL register
	or	t0,v0,SR_IE	# 4 cycles between MFC0 and using v0
	xor	t0,SR_IE
	or	t2,v0,SPLHI_DISABLED_INTS

	# Now, let's go for the lock.
1:
	# Disable interrupts during ll/sc
	MTC0(t0,C0_SR)
#ifdef OBSOLETE
	/* Don't really need to delay waiting for interrupts to be blocked.
	 * If an interrupt hits this small 3 instruction window, then the
	 * "sc" will fail and we simply re-execute the loop.  This costs us
	 * 8 instructions or so when the interrupt hits the window but
	 * saves us 3 instuctions in the typical case.
	 */
	NOP_0_3
#endif /* OBSOLETE */	
	li	t3,SPLHI_CEL

	# See if lock is busy.
2:	LOCK_LL	ta0,0(a0)
	and	ta1,ta0,a1
	bne	ta1,zero,4f
	or	ta0,a1

	# Try to mark lock busy
	LOCK_SC	ta0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	ta0,zero,2b
	nop
#else
	beq	ta0,zero,2b
#endif
	li	ta2,SR_IMASK

	# We have the lock.
	# Enable IE and do an splhi.
	.set	reorder
	# Following instruction is macro on TFP.  On R4000, reorder will
	# place it in BDSLOT
	xor	t2,SPLHI_DISABLED_INTS	# BDSLOT
	bge	t1,t3,3f	/* don't lower spl */
	RAISE_CEL(t3,ta3)
3:
	.set	noreorder
	MTC0(t2,C0_SR)
	/* no need to add NOPs here */
	and	v0,ta2

	/*
	 * Ensure non-zero value for successful return.  (Sometimes both the
	 * old interrupt mask and CEL can be zero ...)
	 */
	ori	v0,OSPL_TRY1
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	or	a3,v0,t1
#elif SPLMETER
	or	a1,v0,t1
	move	a0,ra
	j	raisespl_spl
	LA	a2,splhi
#else
	j	ra
	or	v0,t1
#endif

4: 	# Lock is busy.  Just punt.
	MTC0(v0,C0_SR)
	NOP_0_2
#ifdef SEMASTAT
	move	a1,ra
	li	a2,3
	j	lstat_update
	move	a3,zero
#else
	j	ra
	move	v0,zero
#endif
#endif /* R4000 || R10000 */
	.set 	reorder
	SPLEND(mutex_bittrylock,__mutex_bittrylock)	

/*
 *	int nested_bittrylock(uint *lp, uint lock-bit);
 */
LEAF(nested_bittrylock)
	.set	noreorder
	# See if lock is busy.
1:	ll	v0,0(a0)
	and	ta1,v0,a1
	bne	ta1,zero,2f
	or	v0,a1

	# Try to mark lock busy
	sc	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop

	# We have the lock.
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0				# BDSLOT
#else
	j	ra
	nop
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
	move	v0,zero		# returns failure
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
	or	v0,a1

	# Try to mark lock busy
	scd	v0,0(a0)	
#ifdef R10K_LLSC_WAR
	beql	v0,zero,1b
#else
	beq	v0,zero,1b
#endif
	nop

	# We have the lock.
#ifdef SEMASTAT
	move	a1,ra
	li	a2,0
	j	lstat_update
	move	a3,v0				# BDSLOT
#else
	j	ra
	nop
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
	move	v0,zero		# returns failure
#endif
	.set	reorder
	END(nested_64bittrylock)	

#endif EVEREST
