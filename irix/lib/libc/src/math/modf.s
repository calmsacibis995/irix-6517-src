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

#ident "$Id: modf.s,v 1.8 1994/10/26 20:51:33 jwag Exp $"

.weakext modf _modf

#include "synonyms.h"
#include <regdef.h>
#include <sys/asm.h>

/*  double modf(double value, double *iptr)
 *
 *  returns the signed fractional part of value
 *  and stores the integer part indirectly through iptr.
 *
 *  If arg is +/-inf, sets *iptr to arg, and returns arg
 *
 *  If arg is a Nan, sets *iptr to qnan, and returns qnan
 *
 */

.rdata

negzero:	.dword	0x8000000000000000
__qnan:		.dword	0x7ff1000000000000

	.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

LEAF(modf)
	mfc1	t1, $f13		# copy MSW of arg to t1
	srl	t2, t1, 20		# shift off mantissa
	andi	t2, 0x7ff		# mask exponent bits
	bge	t2, 0x414, big		# branch if exponent(arg) >= 0x414
	blt	t2, 0x3ff, small	# branch if |arg| < 1.0

	sub	t2, 0x413
	neg	t2			# compute shift value
	srl	t1, t2			# shift off fractional bits
	sll	t1, t2			# shift back
	mtc1	$0, $f0			# $f0-$f1 = fractional part of arg
	mtc1	t1, $f1
	s.d	$f0, (a2)		# *iptr = $f0
	sub.d	$f0, $f12, $f0		# return arg - *iptr
	j	ra

big:
	/* exponent(arg) >= 0x414	*/

	bge	t2, 0x433, realbig	# branch if exponent(arg) >= 0x433
	mfc1	t0, $f12		# fetch LSW of arg
	sub	t2, 0x433
	neg	t2			# compute shift value
	srl	t0, t2			# shift off fractional bits
	sll	t0, t2			# shift back
	mtc1	t0, $f0			# $f0-$f1 = fractional part of arg
	mtc1	t1, $f1
	s.d	$f0, (a2)		# *iptr = $f0
	sub.d	$f0, $f12, $f0		# return arg - *iptr
	j	ra

realbig:
	/* exponent(arg) >= 0x433	*/

	SETUP_GPX_L(t0, l1)

	beq	t2, 0x7ff, infornan

	/* arg is an integer; *iptr = arg; result = 0.0 */

	s.d	$f12, (a2)		# *iptr = arg
	li.d	$f0, 0.0
	j	ra

infornan:

	c.un.d	$f12, $f12
	bc1t	nan			# branch if arg is a NaN

	/* input is +/-inf; *iptr = arg; result = arg */

	s.d	$f12, (a2)		# *iptr = arg
	mov.d	$f0, $f12
	j	ra

nan:
	/* input is NaN; *iptr = qnan; return qnan */

	l.d	$f0, __qnan
	s.d	$f0, (a2)		# *iptr = quiet NaN
	j	ra

small:
	/* |arg| < 1.0 */

	SETUP_GPX_L(t0, l2)

	li.d	$f0, 0.0 
	c.eq.d	$f0, $f12
	bc1t	zeroarg			# branch if arg == 0.0

	c.lt.d	$f12, $f0
	bc1t	neg			# branch if arg < 0.0

	/* 0.0 < arg < 1.0; *iptr = 0.0; return arg */

	s.d	$f0, (a2)		# *iptr = 0.0
	mov.d	$f0, $f12		# return arg as result
	j	ra

neg:
	l.d	$f0, negzero
	s.d	$f0, (a2)		# *iptr = -0.0
	mov.d	$f0, $f12		# return arg as result
	j	ra

zeroarg:
	/* arg == +/- 0.0; *iptr = arg; return 0.0 */

	s.d	$f12, (a2)		# *iptr = 0.0
	j	ra

END(modf)

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

LEAF(modf)
	dmfc1	t1, $f12		# copy arg to t1
	dsrl32	t2, t1, 20		# shift off mantissa
	andi	t2, 0x7ff		# mask exponent bits
	bge	t2, 0x433, big		# branch if exponent(arg) >= 0x433
	blt	t2, 0x3ff, small	# branch if |arg| < 1.0

	subu	t2, 0x433
	neg	t2			# compute shift value
	dsrlv	t1, t2			# shift off fractional bits
	dsllv	t1, t2			# shift back
	dmtc1	t1, $f0
	s.d	$f0, (a1)		# *iptr = $f0
	sub.d	$f0, $f12, $f0		# return arg - *iptr
	j	ra

big:
	/* exponent(arg) >= 0x433	*/

	beq	t2, 0x7ff, infornan

	/* arg is an integer; *iptr = arg; result = 0.0 */

	s.d	$f12, (a1)		# *iptr = arg
	dmtc1	$0, $f0
	j	ra

infornan:

	c.un.d	$f12, $f12
	bc1t	nan			# branch if arg is a NaN

	/* input is +/-inf; *iptr = arg; result = arg */

	s.d	$f12, (a1)		# *iptr = arg
	mov.d	$f0, $f12
	j	ra

nan:
	/* input is NaN; *iptr = qnan; return qnan */

	USE_ALT_CP(t2)
	SETUP_GPX64_L(t2, t0, l1)

	l.d	$f0, __qnan
	s.d	$f0, (a1)		# *iptr = quiet NaN
	j	ra

small:
	/* |arg| < 1.0 */

	USE_ALT_CP(t2)
	SETUP_GPX64_L(t2, t0, l2)

	li.d	$f0, 0.0 
	c.eq.d	$f0, $f12
	bc1t	zeroarg			# branch if arg == 0.0

	c.lt.d	$f12, $f0
	bc1t	neg			# branch if arg < 0.0

	/* 0.0 < arg < 1.0; *iptr = 0.0; return arg */

	s.d	$f0, (a1)		# *iptr = 0.0
	mov.d	$f0, $f12		# return arg as result
	j	ra

neg:
	l.d	$f0, negzero
	s.d	$f0, (a1)		# *iptr = -0.0
	mov.d	$f0, $f12		# return arg as result
	j	ra

zeroarg:
	/* arg == +/- 0.0; *iptr = arg; return 0.0 */

	s.d	$f12, (a1)		# *iptr = 0.0
	j	ra


END(modf)

#endif

