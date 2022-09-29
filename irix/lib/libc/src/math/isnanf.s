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
#ident "$Id: isnanf.s,v 1.3 1994/10/26 20:46:13 jwag Exp $"

.weakext isnanf _isnanf

#include "synonyms.h"
#include <regdef.h>
#include <asm.h>


/*  int isnanf (float fsrc)
 *
 *  return true (1) if argument is a Nan; otherwise return false (0);
 *
 *  A NaN has exponent 0xff (i.e. all ones) and a non-zero fraction.
 */

	.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

LEAF(isnanf)
	.set	noreorder
	mfc1	v0, $f12
	li	t1, 0xff		/* t1 = max exponent */
	sll	v0, 1			/* get rid of sign bit */
	srl	t0, v0, 23+1		/* then get right-shifted exp */
	beq	t0, t1, maxexp		/* more checks if max exponent */
	sll	v0, 8			/* v0 = left shifted fraction */

	/* return false */

	j	ra
	li	v0,0

/* We have the maximum exponent.  But a zero fraction is infinity instead
 * of a NaN.  Note that v0 contains left shifted fraction.  If ANY bit is
 * set, then we have a NaN.
 */

maxexp:
	j	ra
	sltu	v0, zero, v0

END(isnanf)

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

LEAF(isnanf)
	.set	noreorder
	dmfc1	v0, $f12
	li	t1, 0xff		/* t1 = max exponent */
	dsrl	t0, v0, 23		/* get right-shifted sign + exp */
	and	t0, t1			/* get rid of sign bit */
	beq	t0, t1, maxexp		/* more checks if max exponent */
	dsll32	v0, 9			/* v0 = left shifted fraction */

	/* return false */

	j	ra
	li	v0,0

/* We have the maximum exponent.  But a zero fraction is infinity instead
 * of a NaN.  Note that v0 contains left shifted fraction.  If ANY bit is
 * set, then we have a NaN.
 */

maxexp:
	j	ra
	sltu	v0, zero, v0

END(isnanf)

#endif
