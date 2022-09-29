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
#ident "$Id: logb.s,v 1.9 1996/06/27 20:36:13 danc Exp $"

.weakext logb _logb

#include "synonyms.h"
#include <regdef.h>
#include <asm.h>
#include <errno.h>

/*
 * logb returns the unbiased exponent of its floating-point argument as
 * a double-precision floating point value
 *
 * logb(NaN) = NaN             sets errno = EDOM
 * logb(infinity) = +infinity  sets errno = EDOM
 * logb(0) = -infinity         raises divide-by-zero   sets errno = EDOM
 */

#if 0

double logb(x)
double x;
{
	register int iexp = EXPONENT(x);

	if (iexp == MAXEXP) { /* infinity  or NaN */
		SIGNBIT(x) = 0;
		errno = EDOM;
		return x;
	}
	if (iexp == 0)  {  /* de-normal  or 0*/
		if ((HIFRACTION(x) == 0) && (LOFRACTION(x) == 0)) { /*zero*/
			double zero = 0.0;
			errno = EDOM;
			return(-1.0/zero); /* return -inf - raise div by 
					    * zero exception
					    */
		}
		else  /* de-normal */
			return(-1022.0);
	}
	return((double)(iexp - 1023)); /* subtract bias */
}
#endif

	.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

LEAF(logb)

	mfc1	t1, $f13		# move arg to t0/t1
	mfc1	t0, $f12
	sll	t1, 1			# shift off sign bit
	srl	t2, t1, 20+1		# shift off mantissa
	beq	t2, 0, 1f		# branch if exponent is zero
	beq	t2, 0x7ff, 3f		# branch if exponent is 0x7ff
	subu	t2, 0x3ff		# subtract exponent bias
	mtc1	t2, $f0			# move unbiased exponent to $f0
	cvt.d.w	$f0			# convert to a double
	j	ra

1:	bne	t1, 0, 2f		# branch if arg != +/- 0.0
	bne	t0, 0, 2f

	/* arg = +/- zero */

	SETUP_GPX_L(t2, l10)

	li	t0, -1
	mtc1	t0, $f0
	cvt.d.w	$f0			# $f0 = -1.0
	mtc1	$0, $f2
	cvt.d.w	$f2			# $f2 = 0.0

	/* set result to -Inf; raise divide by zero exception */

	div.d	$f0, $f2		# result = -infinity
	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	j	ra

2:	/* arg = denorm */

	li	t0, -1022
	mtc1	t0, $f0
	cvt.d.w	$f0			# result = -1022.0
	j	ra

3:	/* arg = NaN or Infinity */

	SETUP_GPX_L(t2, l20)

	sll	t1, 11			# shift off exponent bits
	bne	t1, 0, 4f		# branch if arg != +/-Inf
	bne	t0, 0, 4f
	abs.d	$f0, $f12		# result = +Inf
	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	j	ra

4:	/* arg = NaN */

	mov.d	$f0, $f12		# result = arg
	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	j	ra

END(logb)

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

LEAF(logb)

	dmfc1	t1, $f12		# move arg to t1
	dsll	t1, 1			# shift off sign bit
	dsrl32	t2, t1, 20+1		# shift off mantissa
	beq	t2, 0, 1f		# branch if exponent is zero
	beq	t2, 0x7ff, 3f		# branch if exponent is 0x7ff
	subu	t2, 0x3ff		# subtract exponent bias
	dmtc1	t2, $f0			# move unbiased exponent to $f0
	cvt.d.l	$f0			# convert to a double
	j	ra

1:	bne	t1, 0, 2f		# branch if arg != +/- 0.0

	/* arg = +/- zero */

	USE_ALT_CP(t2)
	SETUP_GPX64_L(t2, t0, l10)

	li	t1, -1
	dmtc1	t1, $f0
	cvt.d.l	$f0			# $f0 = -1.0
	dmtc1	$0, $f2
	cvt.d.l	$f2			# $f2 = 0.0

	/* set result to -Inf; raise divide by zero exception */

	div.d	$f0, $f2		# result = -infinity
	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	j	ra

2:	/* arg = denorm */

	li	t1, -1022
	dmtc1	t1, $f0
	cvt.d.l	$f0			# $f0 = -1022.0
	j	ra

3:	/* arg = NaN or Infinity */

	SETUP_GPX64_L(t2, t0, l20)

	dsll	t1, 11			# shift off exponent bits
	bne	t1, 0, 4f		# branch if arg != +/-Inf
	abs.d	$f0, $f12		# result = +Inf
	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	j	ra

4:	/* arg = NaN */

	mov.d	$f0, $f12		# result = arg
	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	j	ra

END(logb)

#endif

