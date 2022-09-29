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
#ident "$Id: isnand.s,v 1.3 1994/10/26 20:42:00 jwag Exp $"


.weakext isnan _isnan
.weakext isnand _isnand

#include "synonyms.h"
#include <regdef.h>
#include <sys/asm.h>


/*  int isnand (double dsrc)
 *
 *  return true (1) if argument is a Nan; otherwise return false (0);
 *
 *  A NaN has exponent 0x7ff and a non-zero fraction.
 */

	.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

LEAF(isnand)
XLEAF(isnan)
	.set	noreorder
	mfc1	v0, $f13
	li	t1, 0x7ff		/* t1 = max exponent */
	sll	v0, 1			/* get rid of sign bit */
	srl	t0, v0, 20+1		/* then get right-shifted exp */
	beq	t0, t1, maxexp		/* more checks if max exponent */
	sll	v0, 11			/* v0 = left shift MSB fraction */
retFalse:
	j	ra
	li	v0,0

/* We have the maximum exponent.  But a zero fraction is infinity instead
 * of a NaN.  Note that v0 contains the (left shifted) MSB of the fraction.
 * If ANY bit is set, then we have a NaN, if all zero then must check LSB
 * of fraction.
 */

maxexp:
	bne	v0, zero, retval	/* if != 0, go return true */
	nop
	mfc1	v0, $f12		/* check LSB of fraction */
	nop
/*
 * If v0 is non-zero, then we have a Nan.  Return true.  If v0 is zero,
 * then it's not a NaN, so we return false;
 */

retval:
	j	ra
	sltu	v0, zero, v0		/* v0 = return value */

END(isnand)

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32 ) */

LEAF(isnand)
XLEAF(isnan)
	.set	noreorder
	dmfc1	v0, $f12
	li	t1, 0x7ff		/* t1 = max exponent */
	dsll	v0, 1			/* get rid of sign bit */
	dsrl32	t0, v0, 20+1		/* then get right-shifted exp */
	beq	t0, t1, maxexp		/* more checks if max exponent */
	dsll	v0, 11			/* v0 = left shift fraction */
retFalse:
	j	ra
	li	v0,0

/* We have the maximum exponent.  But a zero fraction is infinity instead
 * of a NaN.  Note that v0 contains the (left shifted) fraction.
 * If ANY bit is set, then we have a NaN.
 */

maxexp:

/*
 * If v0 is non-zero, then we have a Nan.  Return true.  If v0 is zero,
 * then it's not a NaN, so we return false;
 */
	j	ra
	sltu	v0, zero, v0		/* v0 = return value */

END(isnand)

#endif
