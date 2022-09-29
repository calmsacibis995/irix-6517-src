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
 * Description: quad precision version of nextafter
 */
#ident "$Id: qnextafter.c,v 1.10 1997/10/11 19:59:12 vegas Exp $"

#ifdef __STDC__
	#pragma weak nextafterl = _nextafterl
	#pragma weak qnextafter = _nextafterl
	#pragma weak _qnextafter = _nextafterl
#endif

#include "synonyms.h"
#include <values.h>
#include <errno.h>
#include "fpparts.h"
#include <inttypes.h>
#include "quad.h"

extern	const	double	__infinity;


double	fabs(double);
#pragma intrinsic (fabs)

static	long double	fabsl(long double);
static	int	true_exponent(long double);
static	int	supports_denorms( double );
static	double	twofloor(long double);
static	double	dulp(long double);
static	double	qulp(long double);
static	long double	sub(long double, long double);

static const	ldu	twopm916 =
{
{0x06b00000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	ldu	twopm968 =
{
{0x03700000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	ldu	twopm969 =
{
{0x03600000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	ldu	maxldbl =
{
{0x7fefffff,	0xffffffff,
 0x7c8fffff,	0xffffffff},
};

static const du		mindbl =
{0x00100000,	0x00000000};

static const du		twopm52 =
{0x3cb00000,	0x00000000};

static const du		twopm106 =
{0x39500000,	0x00000000};

static const du		mindenorm =
{0x00000000,	0x00000001};

/* ====================================================================
 *
 * FunctionName: nextafterl
 *
 * Description: nextafterl(u,v) returns the next representable neighbor
 *		of u in the direction of v
 *		Special cases:
 *		1) if either u or v is a NaN then the result is one of the
 *		NaNs, errno = EDOM
 *		2) if u is +-inf, u is returned and errno = EDOM
 *		3) if u == v the results is u without any exceptions being signaled
 *		4) overflow  and inexact are signaled when u is finite,
 *			but nextafterl(u,v) is not, errno = ERANGE
 *		5) underflow and inexact are signaled when nextafterl(u,v)
 *			lies between +-(2**-916), errno = ERANGE
 *
 *		Note that the value returned by nextafterl(u, v) depends on
 *		whether the processor supports denormals when fabsl(u) < 2**-916
 *		Users can distinguish this case by testing errno.
 *
 *
 * ====================================================================
 */

long double
nextafterl(long double u, long double v)
{
ldquad x, y;
ldquad result;
double	d;
double	z, zz;
int	xpt1, xpt2;
int	bp;
unsigned long long m, n;

x.ld = u;
y.ld = v;

if (EXPONENT(x.q.hi) == MAXEXP) /* u a Nan or inf */
{

	setoserror(EDOM);

	return u; 
}

if ( (EXPONENT(y.q.hi) == MAXEXP) && (HIFRACTION(y.q.hi) || 
	LOFRACTION(y.q.hi))
   )
{

	setoserror(EDOM);

	return v;  /* v a NaN */
}

if (u == v)
	return u;

if ( fabsl(u) > twopm916.ld )
{
	if (((v > u) && !SIGNBIT(x.q.hi)) || ((v < u) && SIGNBIT(x.q.hi))) 
	{
		/* v > u, u positive or v < u, u negative */

		/* special case the boundary points */

		if ( fabsl(x.ld) == maxldbl.ld )
		{

			d = (SIGNBIT(x.q.hi) ? -__infinity : __infinity);

			setoserror(ERANGE);

			return ( (long double)d );
		}
	}

	result.ld = sub(u, v);

	return ( result.ld );
}
else if ( supports_denorms( MINDOUBLE ) )
{
	setoserror(ERANGE);

	if ( u == 0.0 )
	{
		result.q.lo = 0.0;

		if ( v > 0.0 )
			result.q.hi = mindenorm.d;
		else
			result.q.hi = -mindenorm.d;

		return ( result.ld );
	}

	if ( fabsl(u) > twopm968.ld )
	{
		result.ld = sub(u, v);

		return ( result.ld );
	}
	else
	{
		result.ld = x.ld;

		if ( ((v > u) && !SIGNBIT(x.q.hi)) || ((v < u) && SIGNBIT(x.q.hi)) ) 
		{
			/* v > u, u positive or v < u, u negative */

			result.ld += (!SIGNBIT(x.q.hi)) ? mindenorm.d : -mindenorm.d;

			return ( result.ld );
		}
		else
		{
			result.ld += (!SIGNBIT(x.q.hi)) ? -mindenorm.d : mindenorm.d;

			return ( result.ld );
		}
	}
}
else
{ /* no denorms */

	setoserror(ERANGE);

	if ( u == 0.0 )
	{
		result.q.lo = 0.0;

		if ( v > 0.0 )
			result.q.hi = mindbl.d;
		else
			result.q.hi = -mindbl.d;

		return ( result.ld );
	}

	if ( ((v > u) && !SIGNBIT(x.q.hi)) || ((v < u) && SIGNBIT(x.q.hi)) ) 
	{
		if ( fabsl(u) < twopm969.ld )
		{	/* |u| < 2**-969 == 0x036, so just add 1 dulp
			 * to x.q.hi
			 */

			if ( LOFRACTION(x.q.hi) != 0xffffffff )
			{
				LOFRACTION(x.q.hi) += 1;
			}
			else
			{
				LOFRACTION(x.q.hi) = 0;

				if ( HIFRACTION(x.q.hi) != 0xfffff )
				{
					HIFRACTION(x.q.hi) += 1;
				}
				else
				{
					HIFRACTION(x.q.hi) = 0;

					EXPONENT(x.q.hi) += 1;
				}
			}

			return ( x.ld );
		}
		else if ( x.q.lo == 0.0 )
		{
			x.q.lo = (x.q.hi > 0.0) ? mindbl.d : -mindbl.d;

		}
		else if ( x.q.lo == 0.5*dulp(x.ld) )
		{
			if ( fabs(x.q.lo) == mindbl.d )
				x.q.lo *= 2.0;
			else
			{
				x.q.hi += 2.0*x.q.lo;
				EXPONENT(x.q.lo) -= 1;
				FRACTION(x.q.lo) = 0xffffffffffffull;
			}
		}
		else if ( x.q.lo == -0.5*dulp(x.ld) )
		{
			if ( fabs(x.q.lo) == mindbl.d )
				x.q.lo = 0.0;
			else
			{
				EXPONENT(x.q.lo) -= 1;
				FRACTION(x.q.lo) = 0xffffffffffffull;
			}
		}
		else
		{
			xpt1 = true_exponent(x.ld);
			xpt2 = EXPONENT(x.q.lo);

			bp = xpt1 - xpt2 - 54;

			if ( SIGNBIT(x.q.hi) == SIGNBIT(x.q.lo) )
			{
				/* add 1 to mantissa of x.q.lo in bit position "bp" */

				if ( bp <= 51 )
				{
					n = (1 << bp);

					m = FRACTION(x.q.lo) + n;

					if ( m >> 52 )
					{
						EXPONENT(x.q.lo) += 1;
					}

					FRACTION(x.q.lo) = (m & 0xfffffffffffffull);

				}
				else
				{
					x.q.lo *= 2.0;
				}

			}
			else
			{
				/* subtract 1 from mantissa of x.q.lo in bit position "bp" */
				if ( bp <= 51 )
				{
					n = (1 << bp);

					if ( (FRACTION(x.q.lo) >> bp) == 0 )
					{
						EXPONENT(x.q.lo) -= 1;
						FRACTION(x) |= (0xffffffffffffull << bp);
					}
					else
						FRACTION(x.q.lo) -= n;

				}
				else
				{
					x.q.lo = 0.0;
				}

			}
		}

		/* now normalize result */
	
		z = x.q.hi + x.q.lo;
		zz = x.q.hi - z + x.q.lo;

		x.q.hi = z;
		x.q.lo = zz;

		return ( x.ld );
	}
	else
	{
		/* return the next long double in the direction of 0.0 */

		if ( fabs(x.q.hi) == mindenorm.d )
			return ( 0.0L );
		
		if ( fabsl(u) < twopm969.ld )
		{	/* |u| < 2**-969 == 0x036, so just subtract 1 dulp
			 * from x.q.hi
			 */

			if ( LOFRACTION(x.q.hi) != 0 )
			{
				LOFRACTION(x.q.hi) -= 1;
			}
			else
			{
				LOFRACTION(x.q.hi) = 0xffffffff;

				if ( HIFRACTION(x.q.hi) != 0 )
				{
					HIFRACTION(x.q.hi) -= 1;
				}
				else
				{
					HIFRACTION(x.q.hi) = 0xfffff;

					EXPONENT(x.q.hi) -= 1;
				}
			}

			return ( x.ld );
		}
		else if ( x.q.lo == 0.0 )
		{
			x.q.lo = (x.q.hi > 0.0) ? -mindbl.d : mindbl.d;

		}
		else if ( x.q.lo == 0.5*dulp(x.ld) )
		{
			if ( fabs(x.q.lo) == mindbl.d )
				x.q.lo = 0.0;
			else
			{
				EXPONENT(x.q.lo) -= 1;
				FRACTION(x.q.lo) = 0xffffffffffffull;
			}
		}
		else if ( x.q.lo == -0.5*dulp(x.ld) )
		{
			if ( fabs(x.q.lo) == mindbl.d )
				x.q.lo *= 2.0;
			else
			{
				x.q.hi += 2.0*x.q.lo;
				EXPONENT(x.q.lo) -= 1;
				FRACTION(x.q.lo) = 0xffffffffffffull;
			}
		}
		else
		{
			xpt1 = true_exponent(x.ld);
			xpt2 = EXPONENT(x.q.lo);

			bp = xpt1 - xpt2 - 54;

			if ( SIGNBIT(x.q.hi) == SIGNBIT(x.q.lo) )
			{

				/* subtract 1 from mantissa of x.q.lo in bit position "bp" */
				if ( bp <= 51 )
				{
					n = (1 << bp);

					if ( (FRACTION(x.q.lo) >> bp) == 0 )
					{
						EXPONENT(x.q.lo) -= 1;
						FRACTION(x) |= (0xffffffffffffull << bp);
					}
					else
						FRACTION(x.q.lo) -= n;

				}
				else
				{
					x.q.lo = 0.0;
				}
			}
			else
			{
				/* add 1 to mantissa of x.q.lo in bit position "bp" */

				if ( bp <= 51 )
				{
					n = (1 << bp);

					m = FRACTION(x.q.lo) + n;

					if ( m >> 52 )
					{
						EXPONENT(x.q.lo) += 1;
					}

					FRACTION(x.q.lo) = (m & 0xfffffffffffffull);

				}
				else
				{
					x.q.lo *= 2.0;
				}

			}
		}

		/* now normalize result */
	
		z = x.q.hi + x.q.lo;
		zz = x.q.hi - z + x.q.lo;

		x.q.hi = z;
		x.q.lo = zz;

		return ( x.ld );
	}
}
}

static
int
supports_denorms( double x )
{
	return ( x*0.5 != 0.0 );
}

static
double
twofloor( u )
long double	u;
{
ldquad	x;
_dval	v, w;

	/* Return the largest power of two <= u.
	 * Assume u >= 2**-1022 == 0x001
	 */

	x.ld = u;

	v.d = x.q.hi;
	w.d = x.q.lo;

	/* zero out the mantissa of v */

	v.fparts.hi = 0;
	v.fparts.lo = 0;

	/* if u is a power of two, return u */

	if ( (v.d == x.q.hi) && (x.q.lo == 0.0) )
		return ( v.d );

	/* If x.q.hi is a power of two and x.q.lo has the opposite sign
	 * then result is 0.5*v, otherwise v.
	 */

	if ( (v.d == x.q.hi) && (v.fparts.sign != w.fparts.sign) )
		v.fparts.exp -= 1;

	return ( v.d );
}

static
int
true_exponent( u )
long double	u;
{
double	d;

	d = twofloor(u);

	return ( EXPONENT(d) );
}

static
double
dulp( u )
long double	u;
{
	/* Return a (double) ulp of u.
	 * Assume u  >= 2**-1022 == 0x001
	 */

	return ( twopm52.d*twofloor(u) );
}

static
double
qulp( u )
long double	u;
{
	/* Return a (long double) ulp of u.
	 * Assume u  > 2**-968 == 0x037
	 */

	return ( twopm106.d*twofloor(u) );
}

static
long double
sub(long double u, long double v)
{
ldquad x;
double	d;
double	z, zz;

	x.ld = u;

	if (((v > u) && !SIGNBIT(x.q.hi)) || ((v < u) && SIGNBIT(x.q.hi))) 
	{
		/* v > u, u positive or v < u, u negative */

		if ( x.q.lo == 0.0 )
		{
			x.q.lo = qulp(u);

			return ( x.ld );
		}

		/* Add qulp(u), however you have to be careful
		 * in doing it because of the weird representation
		 * we use.
		 */

		d = dulp(u);
		z = d*0.5;

		if ( x.q.lo == z )
		{
			/* x.q.lo is .5*dulp(u), so we can't
			 * add qulp(u) directly
			 */

			x.q.hi += d;
			x.q.lo = -z + qulp(u);

			return ( x.ld );
		}

		x.q.lo += qulp(u);

		/* now normalize */

		z = x.q.hi + x.q.lo;
		zz = x.q.hi - z + x.q.lo;

		x.q.hi = z;
		x.q.lo = zz;

		return ( x.ld );
	}


	/* v < u, u pos or v > u, u neg */

	/* Subtract qulp(u), however you have to be careful
	   in doing it because of the weird representation
	   we use.
	 */

	/* have to handle powers of two separately */

	if ( (x.q.hi == twofloor(x.ld)) && (x.q.lo == 0.0) )
	{
		x.q.lo = -0.5*qulp(x.ld);

		return ( x.ld );
	}

	if ( x.q.lo == 0.0 )
	{
		x.q.lo = -qulp(u);

		return ( x.ld );
	}

	d = dulp(u);
	z = d*0.5;

	if ( x.q.lo == -z )
	{
		/* x.q.lo is -.5*dulp(u), so we can't
		 * subtract qulp(u) directly
		 */

		x.q.hi -= d;
		x.q.lo = z - qulp(u);

		return ( x.ld );
	}

	x.q.lo -= qulp(u);

	/* now normalize */

	z = x.q.hi + x.q.lo;
	zz = x.q.hi - z + x.q.lo;

	x.q.hi = z;
	x.q.lo = zz;

	return ( x.ld );
}

static
long double
fabsl( arg )
long double arg;
{
ldquad  x;
ldquad  result;

	/* assume arg is finite */

        x.ld = arg;

        if ( x.q.hi > 0.0 )
        {
                return ( arg );
        }

        if ( x.q.hi == 0.0 )
        {
                return ( 0.0L );
        }

        result.q.hi = -x.q.hi;
        result.q.lo = ((x.q.lo == 0.0) ? 0.0 : -x.q.lo);

        return ( result.ld );
}

