/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident "$Revision: 1.9 $"

#include <regdef.h>
#include <sys/asm.h>

/*
 * All these are long *
 */

LEAF(_mips2_add_then_test)
	.set	noreorder
	LONG_LL	v0,(a0)
	nop				# picie bug
	LONG_ADDU v0,a1
	move	t0,v0
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_add_then_test
	nop
	j	ra
	nop
	.end	_mips2_add_then_test

LEAF(_mips2_test_and_set)
	.set	noreorder
	LONG_LL	v0,(a0)
	move	t0,a1
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_and_set
	nop
	j	ra
	nop
	.end	_mips2_test_and_set

LEAF(_mips2_test_then_and)
	.set	noreorder
	LONG_LL	v0,(a0)
	and	t0,v0,a1
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_then_and
	nop
	j	ra
	nop
	.end	_mips2_test_then_and

	/* Really is and-not */
LEAF(_mips2_test_then_nand)
	.set	noreorder
	LONG_LL	v0,(a0)
	and	t0,v0,a1
	not	t0
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_then_nand
	nop
	j	ra
	nop
	.end	_mips2_test_then_nand

LEAF(_mips2_test_then_nor)
	.set	noreorder
	LONG_LL	v0,(a0)
	nor	t0,v0,a1
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_then_nor
	nop
	j	ra
	nop
	.end	_mips2_test_then_nor

LEAF(_mips2_test_then_not)
	.set	noreorder
	LONG_LL	v0,(a0)
	not	t0,v0
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_then_not
	nop
	j	ra
	nop
	.end	_mips2_test_then_not

LEAF(_mips2_test_then_xor)
	.set	noreorder
	LONG_LL	v0,(a0)
	xor	t0,v0,a1
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_then_xor
	nop
	j	ra
	nop
	.end	_mips2_test_then_xor

LEAF(_mips2_test_then_or)
	.set	noreorder
	LONG_LL	v0,(a0)
	or	t0,v0,a1
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_then_or
	nop
	j	ra
	nop
	.end	_mips2_test_then_or

LEAF(_mips2_test_then_add)
	.set	noreorder
	LONG_LL	v0,(a0)
	LONG_ADDU t0,v0,a1
	LONG_SC	t0,(a0)
	beqzl	t0,_mips2_test_then_add
	nop
	j	ra
	nop
	.end	_mips2_test_then_add

/*
 * All these are 32-bit quantities *
 */

LEAF(_mips2_add_then_test32)
	.set	noreorder
	ll	v0,(a0)
	nop				# picie bug
	addu	v0,a1
	move	t0,v0
	sc	t0,(a0)
	beqzl	t0,_mips2_add_then_test32
	nop
	j	ra
	nop
	.end	_mips2_add_then_test32

LEAF(_mips2_test_and_set32)
	.set	noreorder
	ll	v0,(a0)
	move	t0,a1
	sc	t0,(a0)
	beqzl	t0,_mips2_test_and_set32
	nop
	j	ra
	nop
	.end	_mips2_test_and_set32

LEAF(_mips2_test_then_and32)
	.set	noreorder
	ll	v0,(a0)
	and	t0,v0,a1
	sc	t0,(a0)
	beqzl	t0,_mips2_test_then_and32
	nop
	j	ra
	nop
	.end	_mips2_test_then_and32

	/* Really is and-not */
LEAF(_mips2_test_then_nand32)
	.set	noreorder
	ll	v0,(a0)
	and	t0,v0,a1
	not	t0
	sc	t0,(a0)
	beqzl	t0,_mips2_test_then_nand32
	nop
	j	ra
	nop
	.end	_mips2_test_then_nand32

LEAF(_mips2_test_then_nor32)
	.set	noreorder
	ll	v0,(a0)
	nor	t0,v0,a1
	sc	t0,(a0)
	beqzl	t0,_mips2_test_then_nor32
	nop
	j	ra
	nop
	.end	_mips2_test_then_nor32

LEAF(_mips2_test_then_not32)
	.set	noreorder
	ll	v0,(a0)
	not	t0,v0
	sc	t0,(a0)
	beqzl	t0,_mips2_test_then_not32
	nop
	j	ra
	nop
	.end	_mips2_test_then_not32

LEAF(_mips2_test_then_xor32)
	.set	noreorder
	ll	v0,(a0)
	xor	t0,v0,a1
	sc	t0,(a0)
	beqzl	t0,_mips2_test_then_xor32
	nop
	j	ra
	nop
	.end	_mips2_test_then_xor32

LEAF(_mips2_test_then_or32)
	.set	noreorder
	ll	v0,(a0)
	or	t0,v0,a1
	sc	t0,(a0)
	beqzl	t0,_mips2_test_then_or32
	nop
	j	ra
	nop
	.end	_mips2_test_then_or32

LEAF(_mips2_test_then_add32)
	.set	noreorder
	ll	v0,(a0)
	addu	t0,v0,a1
	sc	t0,(a0)
	beqzl	t0,_mips2_test_then_add32
	nop
	j	ra
	nop
	.end	_mips2_test_then_add32
	
	.weakext	is_mips2, _is_mips2
LEAF(_is_mips2)
	.set	noreorder
	li	v0,-2
	bgezl	v0,ret
	addi	v0,1
	addi	v0,1
ret:
	j	ra
	nop
	.end	_is_mips2
	.set	reorder

/*
 * abilock_t is a 32 bit field
 */
	.weakext	acquire_lock, _acquire_lock
LEAF(_acquire_lock)
	.set	noreorder
1:
	INT_LL	v0,0(a0)	# abi_lock field
	ori	t0,1		# LOCKED
	INT_SC	t0,0(a0)
	beqzl	t0,1b
	nop
	j	ra
	nop
	.end	_acquire_lock

LOCALSZ=	4		# save ra, gp, a0, t2
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)
A0OFF=		FRAMESZ-(3*SZREG)
T2OFF=		FRAMESZ-(4*SZREG)
#define cnt	t1
#define lckp	a0

	.weakext	spin_lock, _spin_lock
NESTED(_spin_lock, FRAMESZ, ra)
	.mask	M_RA, RAOFF-FRAMESZ
	SETUP_GP
	SETUP_GP64(v1,_spin_lock)
	.set	noreorder
	INT_L	cnt, _ushlockdefspin
	# we have ll/sc
1:
	INT_LL	v0,0(lckp)	# abi_lock field
	ori	t0,1		# LOCKED
	bnezl	v0, 3f		# lock already taken
	addiu	cnt, -1		# This gets squashed in non-taken case
	INT_SC	t0,0(lckp)
	beqzl	t0,1b
				# In this case we go ahead and
				# decr count. Since we will spin
				# till we get the lock anyway
				# it doesn't do any harm and
				# may help in really bad ll/sc
				# thrashing situations
	addiu	cnt, -1		# This gets squashed in non-taken case
	RESTORE_GP64
	j	ra
	nop

3:	/* lock is taken or busy */
	bgtz	cnt, 1b			# cnt > 0 -> keep trying
	nop				# BDSLOT

	/* call nanosleep(&__usnano) */
	.set	reorder
	PTR_SUBU sp,FRAMESZ
	SAVE_GP(GPOFF)
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	/* for non old ABI, gp (saved in v1) is callee saved */
	REG_S	v1, GPOFF(sp)
#endif
	REG_S	ra, RAOFF(sp)
	REG_S	lckp, A0OFF(sp)
	LA	a0, __usnano
	move	a1, zero
	jal	_nanosleep
	REG_L	lckp, A0OFF(sp)
	REG_L	ra, RAOFF(sp)
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	REG_L	v1, GPOFF(sp)
#endif
	PTR_ADDU sp, FRAMESZ
	INT_L	cnt, _ushlockdefspin
	b	1b
	.end	_spin_lock

#undef cnt
#undef lckp
