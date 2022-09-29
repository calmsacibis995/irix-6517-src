/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * System specific assembler routines.
 */

#include "common.h"
#include "asm.h"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/syscall.h>
#include <setjmp.h>
#include <sys/fpu.h>
#include <sys.s>


FRAMESZ=	(((NARGSAVE)*SZREG)+ALSZ)&ALMASK

/*
 * pt_crt0()
 *
 * First routine run by a thread.  We could have jumped to pt_start()
 * directly, but we want to set the RA to 0 so it appears that this is
 * the top of the stack.
 *
 * Thread's start function and start arg are saved in the thread context's
 * s0 and s1 registers.
 */
	.extern	pt_start
NESTED(pt_crt0, FRAMESZ, ra)
	.set	noreorder
	PTR_SUBU	sp, FRAMESZ	# decrement sp for arg save area
	move		a0, v0		# pass old pt pointer
	move		a1, s0		# pt's start routine
	move		a2, s1		# pt's start arg
	li		ra, 0		# signal end of stack
	LA		t9, pt_start
	j		t9
	nop
	.set	reorder
END(pt_crt0)


/*
 * pt_longjmp(pt, func, stack)
 *
 * Calls func() with pt as arg (a0) and running on stack.
 */
NESTED(pt_longjmp, FRAMESZ, ra)
	.set	noreorder
	move		sp, a2
	PTR_SUBU	sp, FRAMESZ	# decrement sp for arg save area
	li		ra, 0		# XXX
	j		a1
	move		t9, a1		# XXX
	.set	reorder
END(pt_longjmp)


/*
 * gp_reg()
 *
 * Return the gp register.
 */
LEAF(gp_reg)
	move	v0, gp
	j	ra
END(gp_reg)


/* R10K ll/sc bug workaround
 *	Under load an R10K can mess up the ll/sc sequence.
 *	The bug occurs when there are multiple ll/sc pairs in the
 *	instruction pipeline.
 *	The workaround is to flush the pipeline using nops or using
 *	the R10K branch prediction logic (e.g. beql).
 *
 * We add the nop to the macro since beql nullifies the delay slot if
 * the branch is not taken.
 */
#define BEQZ_R10KWAR(r, l)	beql r, zero, l; nop

/*
 * add_if_greater(__uint32_t *destp, __uint32_t oldval, __uint32_t addval)
 *
 * Atomically adds addval to *destp if *destp is greater than oldval.
 *
 * Returns 0 if failed, 1 if succeeded
 */
LEAF(add_if_greater)
	.set	noreorder
1:	ll	t0, 0(a0)		# get current value
	ble	t0, a1, 2f
	addu	v0, t0, a2		# BDSLOT

	sc	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra
	nop				# BDSLOT
2:
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra
	move	v0, zero		# BDSLOT
	.set	reorder
	END(add_if_greater)


/*
 * add_if_less(__uint32_t *destp, __uint32_t oldval, __uint32_t addval)
 *
 * Atomically adds addval to *destp if *destp is less than oldval.
 *
 * Returns 0 if failed, 1 if succeeded
 */
LEAF(add_if_less)
	.set	noreorder
1:	ll	t0, 0(a0)		# get current value
	bge	t0, a1, 2f
	addu	v0, t0, a2		# BDSLOT

	sc	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra
	nop				# BDSLOT
2:
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra
	move	v0, zero		# BDSLOT
	.set	reorder
	END(add_if_less)


/*
 * add_if_equal(__uint32_t *destp, __uint32_t oldval, __uint32_t addval)
 *
 * Atomically adds addval to *destp if *destp is equal to oldval.
 *
 * Returns 0 if failed, 1 if succeeded
 */
LEAF(add_if_equal)
	.set	noreorder
1:	ll	t0, 0(a0)		# get current value
	bne	t0, a1, 2f
	addu	v0, t0, a2		# BDSLOT

	sc	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra
	nop				# BDSLOT
2:
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra
	move	v0, zero		# BDSLOT
	.set	reorder
	END(add_if_equal)


/*
 * atomic_unlock(__uint32_t *destp, __uint32_t val1,
 *               __uint32_t addval1, __unit32_t addval2)
 *
 * Atomically unlocks *destp (a rwlock).  If *destp is less than val1,
 * it does nothing. If it is equal to val1, it adds addval1.  If it is
 * greater than val1 adds addval2.

 * Returns the result of the operation
 */
LEAF(atomic_unlock)
	.set	noreorder
1:	ll	v0, 0(a0)		# get current value
	blt	v0, a1, 3f
	bgt	v0, a1, 2f
	addu	t0, v0, a2		# BDSLOT
	move	v0, t0

	sc	t0, 0(a0)
	BEQZ_R10KWAR(t0, 1b)		# retry if sc failed
	j	ra
	nop				# BDSLOT
2:
	addu	t0, v0, a3		# BDSLOT
	move	v0, t0

	sc	t0, 0(a0)
	BEQZ_R10KWAR(t0, 1b)		# retry if sc failed
	j	ra
	nop				# BDSLOT
3:
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(atomic_unlock)


/*
 * ref_if_same(__uint32_t *pt_ref, unsigned short gen_num)
 *
 * Atomically add 1 to *pt_ref if the top two bytes of *pt_ref are the same
 * as gen_num.
 *
 * Return 0 if numbers didn't match, not 0 otherwise.
 */
LEAF(ref_if_same)
	.set	noreorder
1:	ll	t0, 0(a0)		# get current value
	srl	t1, t0, 16		# shift *pt_ref right by two bytes
	bne	t1, a1, 2f		# compare generation numbers
	addiu	v0, t0, 1		# BDSLOT -- add 1 to *pt_ref

	sc	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra
	nop				# BDSLOT
2:
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra			# generation numbers differ - return 0
	move	v0, zero		# BDSLOT
	.set	reorder
	END(ref_if_same)


/*
 * unref_and_test(__uint32_t *pt_ref)
 *
 * Atomically decrement *pt_ref and if the bottom two bytes of *pt_ref are
 * 0 then add 0x00010000 to *pt_ref.
 *
 * Return 0 if just decremented, not 0 if also incremented generation number.
 */
LEAF(unref_and_test)
	.set	noreorder
1:	ll	t0, 0(a0)		# get current value
	addiu	v0, t0, -1		# decrement *pt_ref
	andi	t1, v0, 0x0000FFFF	# remove all but bottom two bytes
	bne	t1, zero, 2f		# see if reference count went to 0
	lui	t2, 1			# BDSLOT - create 0x00010000

	addu	v0, v0, t2		# increment generation number
	sc	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra
	nop				# BDSLOT
2:
	sc	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra
	move	v0, zero		# BDSLOT
	.set	reorder
	END(unref_and_test)


/*
 * cmp_and_swap(void *destp, void *oldval, void *newval)
 *
 * Atomically replace *destp with newval if it equals oldval.
 *
 * Returns 0 if failed, 1 if succeeded
 */
LEAF(cmp_and_swap)
	.set	noat
	.set	noreorder
1:	PTR_LL	t0, 0(a0)		# get current value
	bne	t0, a1, 2f
	move	v0, a2			# BDSLOT

	PTR_SC	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra			# return TRUE
	nop				# BDSLOT
2:
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra			# return FALSE
	move	v0, zero		# BDSLOT
	.set	at
	.set	reorder
	END(cmp_and_swap)


/*
 * cmp0_and_swap(void *destp, void *newvalue);
 *
 * Atomically replace *destp with newval if it equals 0.
 *
 * Returns 0 if failed, 1 if succeeded
 */
LEAF(cmp0_and_swap)
	.set	noat
	.set	noreorder
1:	PTR_LL	t0, 0(a0)		# get current value
	bne	t0, zero, 2f
	move	v0, a1			# BDSLOT

	PTR_SC	v0, 0(a0)
	BEQZ_R10KWAR(v0, 1b)		# retry if sc failed
	j	ra			# return TRUE
	nop				# BDSLOT

2:
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop; \
	nop; nop; nop; nop; nop; nop; nop; nop
	j	ra			# return FALSE
	move	v0, zero		# BDSLOT
	.set	at
	.set	reorder
	END(cmp0_and_swap)
