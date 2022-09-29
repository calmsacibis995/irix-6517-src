/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.24 $"
#ident "$Revision: 1.24 $"

#include "asm.h"
#include "regdef.h"
#include "sgidefs.h"
#include <sys/prctl.h>
#include <sync_internal.h>
		
LOCALSZ=	4		# save ra, gp, a0, a1
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)
SLEEPCOFF=	FRAMESZ-(3*SZREG)
SPINCOFF=	FRAMESZ-(3*SZREG)
SLOCKPOFF=	FRAMESZ-(4*SZREG)

#define slockp	a0
#define spinc	a1
#define sleepc	a2
#define cnt     a2			
	
#define members	t0
#define tmp	t1
	 
/*
 * int _posix_spin_lock (spinlock_t *slockp)
 *
 * Returns:		 POSIX mode	Arena mode
 *		success	       0	    1
 *		failure	      -1	    -1
 */
	.weakext	posix_spin_lock, _posix_spin_lock
NESTED(_posix_spin_lock, FRAMESZ, ra)
	.mask	M_RA, RAOFF-FRAMESZ
	SETUP_GP
	SETUP_GP64(v1, _posix_spin_lock)	
	.set	noreorder
		
	lw	v0, SPIN_FLAGS(slockp)		# Load flags
	andi	tmp, v0, SPIN_FLAGS_DEBUG
	bnez	tmp, lock_debugmode
	lw	spinc, _ushlockdefspin		# BDSLOT
	lw	sleepc, _usyieldcnt
	
smode:	# Standard spin lock mode

	ll	members, 0(slockp)		
	bnez	members, miss1			# Branch if lock is not available
	addiu	spinc, -1			# BDSLOT: decrement spin count
	addiu	members, 1			# lock by becoming 1st member
	sc	members, 0(slockp)
	beqzl	members, smode			# R10k WAR
	nop					# BDSLOT
	
	# We got it, return success value (POSIX = 0, Arena = 1)

	RESTORE_GP64
	j	ra
	andi	v0, v0, SPIN_FLAGS_ARENA

miss1:	# Missed the lock, see if we qualify for spinning

	andi	tmp, v0, SPIN_FLAGS_UP
	beqz	tmp, spinner			# Branch if system is not UP
	addiu	members, 1			# BDSLOT

	# We're not going to spin, block
	
	sc	members, 0(slockp)
	beqzl	members, smode			# R10k WAR
	nop					# BDSLOT

	.set	reorder
	PTR_SUBU sp, FRAMESZ
	SAVE_GP(GPOFF)
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	REG_S	v1, GPOFF(sp)
#endif	
	REG_S	ra, RAOFF(sp)
	REG_S	slockp, SLOCKPOFF(sp)
	jal	_spin_blocker
	REG_L	slockp, SLOCKPOFF(sp)
	REG_L	ra, RAOFF(sp)
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	REG_L	v1, GPOFF(sp)
#endif	
	PTR_ADDU sp, FRAMESZ

unblkd:		# We returned from blocking, decrement the member/waiter count
	
	.set	noreorder	
	ll	members, 0(slockp)
	addiu	members, -1
	sc	members, 0(slockp)
	beqzl	members, unblkd
	nop
	RESTORE_GP64	# XXX	
	j	ra				# v0 is set by _spin_blocker
	nop
	
spinner:	# We are running on an MP system, spin if appropriate
	
	bgtz	spinc, smode			# spin
	nop					# BDSLOT
	addiu	sleepc, -1

	# This is a real spinlock, and spinc spins have elapsed.
	# Call either sginap(0) or nanosleep(x). We call nanosleep every
	# sleepc spins to make sure that we actually pause for a bit..
	
	.set	reorder
	PTR_SUBU sp,FRAMESZ
	SAVE_GP(GPOFF)
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	REG_S	v1, GPOFF(sp)
#endif
	REG_S	ra, RAOFF(sp)
	REG_S	slockp, SLOCKPOFF(sp)
	bgtz	sleepc, yield

		# call nanosleep

	lw	sleepc, _usyieldcnt		# BDSLOT
	REG_S	sleepc, SLEEPCOFF(sp)
	LA	a0, __usnano
	move	a1, zero
	jal	_nanosleep
	b	reload

yield:		# call sginap(0)
	
	REG_S	sleepc, SLEEPCOFF(sp)
	move	a0, zero
	jal	_sginap

reload:		# common reload code
	
	REG_L	sleepc, SLEEPCOFF(sp)
	REG_L	slockp, SLOCKPOFF(sp)
	REG_L	ra, RAOFF(sp)
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	REG_L	v1, GPOFF(sp)
#endif	
	PTR_ADDU sp, FRAMESZ
	lw	spinc, _ushlockdefspin
	lw	v0, SPIN_FLAGS(slockp)		# reload flags
	b	smode
	
lock_debugmode:

	.set	reorder			
	move	a1, ra
	LA	t9, _spin_lock_debug
	RESTORE_GP64
	j	t9
END(_posix_spin_lock)

/*
 * int _spin_unlock (spinlock_t *slockp)
 *
 * Returns:	0 - lock released
 */
	.weakext	spin_unlock, _spin_unlock		
NESTED(_spin_unlock, FRAMESZ, ra)
	.mask	M_RA, RAOFF-FRAMESZ
	.set	noreorder

	lw	v0, SPIN_FLAGS(slockp)		# Load flags
	andi	tmp, v0, SPIN_FLAGS_DEBUG
	bnez	tmp, unlock_debugmode
	nop
	
1:	ll	members, 0(slockp)
	beqz	members, 2f			# branch if already unlocked
	addiu	members, -1			# BDSLOT
	bnez	members, 3f			# Branch if waiters are present
	nop					# BDSLOT
	sc	members, 0(slockp)
	beqzl	members, 1b			# R10k WAR
	nop					# BDSLOT

2:	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	j	ra				# Last member, lock is released
	li	v0, 0

	.set	reorder

3:	# Waiters are present, wake someone up

	SETUP_GPX(t8)
	USE_ALT_CP(t0)
	SETUP_GPX64(GPOFF,t8)
	move	a1, ra				# a0 = slockp, a1= return address
	LA	t9, _spin_unblocker
	j	t9

unlock_debugmode:
	
	SETUP_GPX(t8)
	USE_ALT_CP(t0)
	SETUP_GPX64(GPOFF,t8)			# PIC ? XXX
	move	a1, ra
	LA	t9, _spin_unlock_debug
	j	t9	
END(_spin_unlock)


/*
 * int _spin_lockw (spinlock_t *slockp, int spinc)
 *
 * Returns:		 POSIX mode	Arena mode
 *		success	       0	    1
 *		failure	      -1	    -1
 */
	.weakext	spin_lockw, _spin_lockw		
NESTED(_spin_lockw, FRAMESZ, ra)

	.mask	M_RA, RAOFF-FRAMESZ
	.set	noreorder
		
	lw	v0, SPIN_FLAGS(slockp)		# Load flags
	andi	tmp, v0, SPIN_FLAGS_DEBUG
	bnez	tmp, lock_debugmode_w
	move	cnt, spinc			# BDSLOT
	
smode_w:	# Standard spin lock mode

	ll	members, 0(slockp)		
	bnez	members, miss1_w		# Branch if lock is not available
	addiu	members, 1			# BDSLOT
	sc	members, 0(slockp)
	beqzl	members, smode_w
	nop					# BDSLOT
	
	# We got it, return success value (POSIX = 0, Arena = 1)

	j	ra
	andi	v0, v0, SPIN_FLAGS_ARENA

miss1_w:	# Missed the lock, see if we qualify for spinning

	andi	tmp, v0, SPIN_FLAGS_UP
	beqz	tmp, spinner_w			# Branch if system is not UP
	addiu	cnt, -1				# BDSLOT: decrement spin count
	
miss2_w:	# We're not going to spin, block
	
	sc	members, 0(slockp)
	beqzl	members, smode_w
	nop

	.set	reorder
	SETUP_GPX(t8)
	PTR_SUBU sp, FRAMESZ
	SETUP_GPX64(GPOFF,t8)
	SAVE_GP(GPOFF)
	REG_S	ra, RAOFF(sp)
	REG_S	slockp, SLOCKPOFF(sp)
	jal	_spin_blocker
	REG_L	slockp, SLOCKPOFF(sp)
	REG_L	ra, RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp, FRAMESZ

unblkd_w:	# We returned from blocking, decrement the member/waiter count
	
	.set	noreorder	
	ll	members, 0(slockp)
	addiu	members, -1
	sc	members, 0(slockp)
	beqzl	members, unblkd_w
	nop
	j	ra				# v0 is set by _spin_blocker
	nop
	
spinner_w:	# We are running on an MP system, spin if appropriate
	
	bgtz	cnt, smode_w			# spin
	nop

	# spinc spins have elapsed.  Call sginap(0)
	
	.set	reorder
	SETUP_GPX(t8)
	PTR_SUBU sp, FRAMESZ
	SETUP_GPX64(GPOFF,t8)			# PIC ? XXX
	SAVE_GP(GPOFF)
	REG_S	ra, RAOFF(sp)
	REG_S	slockp, SLOCKPOFF(sp)
	REG_S	spinc, SPINCOFF(sp)	
	move	a0, zero
	jal	_sginap

	REG_L	spinc, SPINCOFF(sp)		
	REG_L	slockp, SLOCKPOFF(sp)
	REG_L	ra, RAOFF(sp)
	RESTORE_GP64				# XXX PIC ???
	PTR_ADDU sp, FRAMESZ
	lw	v0, SPIN_FLAGS(slockp)		# reload flags
	move	cnt, spinc
	b	smode_w
		
lock_debugmode_w:

	.set	reorder	
	SETUP_GPX(t8)
	USE_ALT_CP(t0)
	SETUP_GPX64(GPOFF,t8)			# PIC ? XXX
	move	a2, ra
	LA	t9, _spin_lockw_debug
	j	t9
END(_spin_lockw)	
	
/*
 * int _spin_lockc (spinlock_t *slockp, int spinc)
 *
 * Returns:		 POSIX mode	Arena mode
 *		success	       0	    1
 *		failure	      -1	    0
 */
	.weakext	spin_lockc, _spin_lockc		
NESTED(_spin_lockc, FRAMESZ, ra)

	.mask	M_RA, RAOFF-FRAMESZ
	.set	noreorder
		
	lw	v0, SPIN_FLAGS(slockp)		# Load flags
	andi	tmp, v0, SPIN_FLAGS_DEBUG
	bnez	tmp, lock_debugmode_c
	nop
	
smode_c:	# Standard spin lock mode

	ll	members, 0(slockp)		
	bnez	members, miss1_c		# Branch if lock is not available
	addiu	members, 1			# BD: lock by becoming 1st member
	sc	members, 0(slockp)
	beqzl	members, smode_c
	nop					# BDSLOT
	
	# We got it, return success value (POSIX = 0, Arena = 1)

	j	ra
	andi	v0, v0, SPIN_FLAGS_ARENA

miss1_c:	# Missed the lock, see if we qualify for spinning

	andi	tmp, v0, SPIN_FLAGS_UP
	beqz	tmp, spinner_c			# Branch if system is not UP
	addiu	spinc, -1			# BDSLOT: decrement spin count
	
miss2_c:	# We could not acquire the lock
	nop; nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	
	andi	v0, v0, SPIN_FLAGS_ARENA	# Prepare failure return code
	bnez	v0, miss3_c
	nop

	# XXX set errno to EBUSY here
	j	ra
	li	v0, -1				# POSIX return -1
	
miss3_c:				
	j	ra				# Arena, return 0
	li	v0, 0
	
spinner_c:	# We are running on an MP system, spin if appropriate
	
	bgtz	spinc, smode_c			# spin
	nop
	j	miss2_c				# spins expired, block
	nop
	
lock_debugmode_c:
	
	.set	reorder	
	SETUP_GPX(t8)
	USE_ALT_CP(t0)
	SETUP_GPX64(GPOFF,t8)			# PIC ? XXX
	move	a2, ra
	LA	t9, _spin_lockc_debug
	j	t9
END(_spin_lockc)

/*
 * DEBUG lock routine atomic operations
 *
 * When debugging is enabled for a lock, the debug atomic operations below
 * are invoked by the debug lock routines in ulock.c.
 */
	 	
/*
 * int _r4k_dbg_spin_lock (spinlock_t *slockp)
 *
 * Debug mode lock acquire routine
 *
 * Returns:	0 if lock was acquired
 *		number of waiters (including caller) if lock is unavailable
 */
LEAF(_r4k_dbg_spin_lock)
	.set	noreorder
1:	ll	members, 0(slockp)
	move	v0, members
	addiu	members, 1			# increment member count, since
	sc	members, 0(slockp)		# we are the locker or a waiter
	beqzl	members, 1b
	nop					# BDSLOT
	j	ra
	nop
	.set	reorder
END(_r4k_dbg_spin_lock)

/*
 * int _r4k_dbg_spin_trylock (spinlock_t *slockp)
 *
 * Debug mode try lock acquire routine (doesn't effect waiter count)
 *
 * Returns:	0 if lock was acquired
 *		number of waiters if lock is unavailable
 */
LEAF(_r4k_dbg_spin_trylock)
	.set	noreorder
1:	ll	members, 0(slockp)
	move	v0, members
	bnez	v0, 2f
	addiu	members, 1			# lock by becoming 1st member
	sc	members, 0(slockp)
	beqzl	members, 1b
	nop					# BDSLOT
2:	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	j	ra
	nop
	.set	reorder
END(_r4k_dbg_spin_trylock)	
	
/*
 * int _r4k_dbg_spin_unlock (spinlock_t *slockp)
 *
 * Debug mode lock release routine
 *
 * Returns:	0 if lock was released and there were no waiters, otherwise
 *		number of waiters is returned and the lock is still locked
 */
LEAF(_r4k_dbg_spin_unlock)
	.set	noreorder

1:	ll	members, 0(slockp)
	beqz	members, 2f			# branch if already unlocked
	li	v0, 0				# BDSLOT
		
	addiu	members, -1			# Decrement member count
	bnez	members, 2f			# Branch if waiters are present
	move	v0, members			# BDSLOT
	
	sc	members, 0(slockp)		# Last member, lock is released
	beqzl	members, 1b
	nop					# BDSLOT

2:	nop; nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra
	nop
	.set	reorder
END(_r4k_dbg_spin_unlock)

/*
 * void _r4k_dbg_spin_rewind (spinlock_t *slockp)
 *
 * Rewinds the waiter count
 */
LEAF(_r4k_dbg_spin_rewind)
	.set	noreorder

1:	ll	members, 0(slockp)		
	addiu	members, -1			# thread is dropping membership
	sc	members, 0(slockp)
	beqzl	members, 1b
	nop
	j	ra	
	li	v0, 0
	.set	reorder
END(_r4k_dbg_spin_rewind)

/*
 * _r4k_cas(destp, oldval, newval, usptr *)
 * void *destp
 * ptrdiff_t oldval
 * ptrdiff_t newval
 * Returns 0 if failed, 1 if succedded
 *
 */
LEAF(_r4k_cas)
	.set	noat
	.set	noreorder
	LONG_LL	t0, 0(a0)		# get current value
	nop
	bne	t0, a1, 1f
	move	v0, a2			# BDSLOT
	LONG_SC	v0, 0(a0)

	nop; nop; nop; nop; nop; nop; nop; nop; nop	# R10K WAR
	nop; nop; nop; nop; nop; nop; nop; nop; nop	# R10K WAR
	nop; nop; nop; nop; nop; nop; nop		# R10K WAR

	j	ra
	nop				# BDSLOT
1:
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop

	j	ra
	move	v0, zero		# BDSLOT
	.set	at
	.set	reorder
END(_r4k_cas)

/*
 * _r4k_cas32(destp, oldval, newval, usptr *)
 * void *destp
 * unsigned oldval
 * unsigned newval
 * Returns 0 if failed, 1 if succedded
 *
 */
LEAF(_r4k_cas32)
	.set	noat
	.set	noreorder
	INT_LL	t0, 0(a0)		# get current value
	nop
	bne	t0, a1, 1f
	move	v0, a2			# BDSLOT
	INT_SC	v0, 0(a0)

	nop; nop; nop; nop; nop; nop; nop; nop; nop	# R10K WAR
	nop; nop; nop; nop; nop; nop; nop; nop; nop	# R10K WAR
	nop; nop; nop; nop; nop; nop; nop		# R10K WAR

	j	ra
	nop				# BDSLOT
1:
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	j	ra
	move	v0, zero		# BDSLOT
	.set	at
	.set	reorder
END(_r4k_cas32)
