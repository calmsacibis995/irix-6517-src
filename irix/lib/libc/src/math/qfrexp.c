/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1994       MIPS Computer Systems, Inc.      |
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
/*
 * Description: quad precision version of frexp
 */
#ident "$Id: qfrexp.c,v 1.7 1997/06/06 20:51:20 vegas Exp $"

#ifdef __STDC__
	#pragma weak frexpl = _frexpl
	#pragma weak qfrexp = _frexpl
	#pragma weak _qfrexp = _frexpl
#endif

#include "synonyms.h"
#include <inttypes.h>
#include <float.h>
#include <values.h>
#include <math.h>
#include <fp_class.h>
#include "fpparts.h"
#include "math_extern.h"
#include <errno.h>
#include "quad.h"

static	double	scaleup(double, int *);

static	const char shift_tab[] =
{0, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1};

/* ====================================================================
 *
 * FunctionName: qfrexp
 *
 * Description: returns long double f, such that f = 0.0 or 
 *		0.5 <= |f| < 1.0, and n such that arg = f*2**n.
 *		n is returned indirectly through eptr.
 *
 * ====================================================================
 */

long double
frexpl(long double value, int *eptr)
{
ldquad x;
int class, n;
int exp;

	class = _fp_class_q(value);

        if (class < FP_POS_NORM) {

                /* this handles NaNs and INFs */

                setoserror(EDOM);

                *eptr = DBL_MAX_EXP+1;

                if (class == FP_NEG_INF)
                        return(-.5L);
                else if ((class == FP_SNAN) || (class == FP_QNAN)) {
                        *eptr = 0;
                        return(value);
                }

                else
                        return(.5L);
        }

	*eptr = 0;

	if ( (class == FP_NEG_ZERO) || (class == FP_POS_ZERO) ) /* nothing to do for zero */
		return (value);


        x.ld = value;

	n = 0;

        exp = EXPONENT(x.q.hi);

	/* first, get rid of all denormal doubles by scaling */

	if ( exp == 0 ) {
		x.q.hi = scaleup(x.q.hi, &n);
		x.q.lo = 0.0;
	}
	else if ( ((exp = EXPONENT(x.q.lo)) == 0) &&
		  ((HIFRACTION(x.q.lo) != 0) || (LOFRACTION(x.q.lo) != 0))
		) {

               	x.q.lo = scaleup(x.q.lo, &n);
               	x.q.hi = _ldexp(x.q.hi, -n);
	}

	/* now just use qldexp to scale the number to something between 0.5 and 1.0 */

	exp = EXPONENT(x.q.hi);

	n += exp - 0x3fe;

	x.ld = _qldexp(x.ld, 0x3fe - exp);

	/* must do a little fixup if |x.ld| < 0.5L	*/

	if ( (x.ld >= 0.5L) || (x.ld <= -0.5L) ) {

		*eptr = n;

		return ( x.ld );
	}

	x.q.hi *= 2.0;
	x.q.lo *= 2.0;

	n--;

	*eptr = n;

	return ( x.ld );
	
}

/* ====================================================================
 *
 * FunctionName: scaleup
 *
 * Description: static routine to scale up a non-zero denormal to have
 *		exponent 0x001;
 *		return -logb(scale factor) in *p_n.
 *
 * ====================================================================
 */

/* scale up a non-zero denormal to have exponent 0x001;
 * return -logb(scale factor) in *p_n.
 */

static double
scaleup(double	x, int	*p_n)
{
_dval value;         /* the value manipulated */
int	exp;
int	n;
int	sign;
int shift_count;

	value.d = x;

        exp = EXPONENT(value);

	if (exp != 0 ) {
		*p_n = 0;

		return ( x );
	}

        sign = SIGNBIT(value);

	SIGNBIT(value) = 0;

	n = 0;

	while ( value.fwords.hi == 0 ) {

		value.fwords.hi = (value.fwords.lo >> 12);
		value.fwords.lo <<= 20;

		n -= 20;
	}

	while ( (value.fwords.hi & 0xff000) == 0 ) {

		value.fwords.hi <<= 8;
		value.fwords.hi |= (value.fwords.lo >> 24);
		value.fwords.lo <<= 8;
		n -= 8;
	}

	if ( (value.fwords.hi & 0xf0000) == 0 ) {

		value.fwords.hi <<= 4;
		value.fwords.hi |= (value.fwords.lo >> 28);
		value.fwords.lo <<= 4;
		n -= 4;
	}

	shift_count = shift_tab[(value.fwords.hi >> 16) & 0xf];
		
		value.fwords.hi <<= shift_count;
		value.fwords.hi |= (value.fwords.lo >> (32 - shift_count));
		value.fwords.lo <<= shift_count;
		n -= shift_count;

	/* hidden bit serves as exponent */

	SIGNBIT(value) = sign;	/* restore sign */

	*p_n = n;

	return ( value.d );
}
