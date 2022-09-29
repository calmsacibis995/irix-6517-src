#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

/* note that this routine must be kept in sync with the corresponding compiler
 * routine, c_q_sub in directory common/util
 */

static const du		twop914 =
{0x79100000,	0x00000000};

static const du		inf =
{0x7ff00000,	0x00000000};

double	fabs(double);
#pragma intrinsic (fabs)

long double
__q_sub( xhi, xlo, yhi, ylo)
double xhi, xlo, yhi, ylo;
{
long long ixhi, iyhi;
long long xptxhi, xptyhi;
long long iz, iw, iqulp;
double	u, uu;
double	w, ww;
double	qulp;
ldquad	z;
double	tmp1, tmp2, lo, rem;

	/* adapted from T. J. Dekker's add2 subroutine */

#include "msg.h"

	/* extract exponents of y and x for some quick screening */

	DBL2LL(yhi, iyhi);
	xptyhi = (iyhi >> DMANTWIDTH);
	xptyhi &= 0x7ff;

	DBL2LL(xhi, ixhi);
	xptxhi = (ixhi >> DMANTWIDTH);
	xptxhi &= 0x7ff;

#ifdef QUAD_DEBUG
	printf("q_sub: xhi = %08x%08x\n", *(int32_t *)&xhi, *((int32_t *)&xhi + 1));
	printf("q_sub: xlo = %08x%08x\n", *(int32_t *)&xlo, *((int32_t *)&xlo + 1));
	printf("q_sub: yhi = %08x%08x\n", *(int32_t *)&yhi, *((int32_t *)&yhi + 1));
	printf("q_sub: ylo = %08x%08x\n", *(int32_t *)&ylo, *((int32_t *)&ylo + 1));
#endif

	yhi = -yhi;
	ylo = -ylo;

	if ( xptxhi < xptyhi )
	{
		tmp1 = xhi;
		xhi = yhi;
		yhi = tmp1;
		xptxhi = xptyhi;
	}

	if ( fabs(xlo) < fabs(ylo) )
	{
		tmp2 = xlo;
		xlo = ylo;
		ylo = tmp2;
	}

	if ( xptxhi < 0x7fd )
	{
		z.q.hi = xhi + yhi;
		z.q.lo = xhi - z.q.hi + yhi;
		u = xlo + ylo;
		uu = xlo - u + ylo;

		lo = z.q.lo + u;

		w =  z.q.hi + lo;
		ww = z.q.hi - w + lo;

		rem = z.q.lo - lo + u;

		ww += rem + uu;
		z.q.hi = w + ww;
		DBL2LL( z.q.hi, iz );
		z.q.lo = w - z.q.hi + ww;

		/* if necessary, round z.q.lo so that the sum of z.q.hi and z.q.lo has at most
		   107 significant bits
		*/

		/* first, compute a quarter ulp of z */

		iw = (iz >> DMANTWIDTH);
		iqulp = (iw & 0x7ff);
		iqulp -= 54;
		iqulp <<= DMANTWIDTH;

		if ( iqulp > 0 )
		{
			LL2DBL( iqulp, qulp );
			iw <<= DMANTWIDTH;

			/* Note that the size of an ulp changes at a
			 * power of two.
			 */

			if ( iw == iz )
				goto fix;

			if ( fabs(z.q.lo) >= qulp )
			{
				qulp = 0.0;
			}
			else if ( z.q.lo < 0.0 )
				qulp = -qulp;

			z.q.lo += qulp;
			z.q.lo -= qulp;
		}


#ifdef QUAD_DEBUG
	printf("q_sub: z.q.hi = %08x%08x\n", *(int32_t *)&z.q.hi, *((int32_t *)&z.q.hi + 1));
	printf("q_sub: z.q.lo = %08x%08x\n", *(int32_t *)&z.q.lo, *((int32_t *)&z.q.lo + 1));
#endif

		return ( z.ld );
	}
	else if ( xptxhi == 0x7ff )
	{
		z.q.hi = xhi + yhi;
		z.q.lo = 0.0;

#ifdef QUAD_DEBUG
	printf("q_sub: z.q.hi = %08x%08x\n", *(int32_t *)&z.q.hi, *((int32_t *)&z.q.hi + 1));
	printf("q_sub: z.q.lo = %08x%08x\n", *(int32_t *)&z.q.lo, *((int32_t *)&z.q.lo + 1));
#endif

		return ( z.ld );
	}
	else
	{
		if ( fabs(yhi) < twop914.d )
		{
			z.q.hi = xhi;
			z.q.lo = xlo;

#ifdef QUAD_DEBUG
	printf("q_sub: z.q.hi = %08x%08x\n", *(int32_t *)&z.q.hi, *((int32_t *)&z.q.hi + 1));
	printf("q_sub: z.q.lo = %08x%08x\n", *(int32_t *)&z.q.lo, *((int32_t *)&z.q.lo + 1));
#endif

			return ( z.ld );
		}

		/*	avoid overflow in intermediate computations by 
			computing 4.0*(.25*x + .25*y)
		*/

		xhi *= 0.25;
		xlo *= 0.25;
		yhi *= 0.25;
		ylo *= 0.25;

		z.q.hi = xhi + yhi;
		z.q.lo = xhi - z.q.hi + yhi;
		u = xlo + ylo;
		uu = xlo - u + ylo;

		lo = z.q.lo + u;

		w =  z.q.hi + lo;
		ww = z.q.hi - w + lo;

		rem = z.q.lo - lo + u;

		ww += rem + uu;
		z.q.hi = w + ww;
		DBL2LL( z.q.hi, iz );
		z.q.lo = w - z.q.hi + ww;

		/* if necessary, round z.q.lo so that the sum of z.q.hi and z.q.lo has at most
		   107 significant bits
		*/

		/* first, compute a quarter ulp of z */

		iw = (iz >> DMANTWIDTH);
		iqulp = (iw & 0x7ff);
		iqulp -= 54;
		iqulp <<= DMANTWIDTH;

		if ( iqulp > 0 )
		{
			LL2DBL( iqulp, qulp );
			iw <<= DMANTWIDTH;

			/* Note that the size of an ulp changes at a
			 * power of two.
			 */

			if ( iw == iz )
				goto fix2;

			if ( fabs(z.q.lo) >= qulp )
			{
				qulp = 0.0;
			}
			else if ( z.q.lo < 0.0 )
				qulp = -qulp;

			z.q.lo += qulp;
			z.q.lo -= qulp;
		}

		z.q.hi *= 4.0;

		if ( fabs(z.q.hi) == inf.d )
		{
			z.q.lo = 0.0;
			return ( z.ld );
		}

		z.q.lo *= 4.0;

		return ( z.ld );

	}

fix:
	if ( ((z.q.hi > 0.0) && (z.q.lo < 0.0)) || ((z.q.hi < 0.0) && (z.q.lo > 0.0)) )
		qulp *= 0.5;

	if ( fabs(z.q.lo) >= qulp )
	{
		qulp = 0.0;
	}
	else if ( z.q.lo < 0.0 )
		qulp = -qulp;

	z.q.lo += qulp;
	z.q.lo -= qulp;

	return ( z.ld );

fix2:
	if ( ((z.q.hi > 0.0) && (z.q.lo < 0.0)) || ((z.q.hi < 0.0) && (z.q.lo > 0.0)) )
		qulp *= 0.5;

	if ( fabs(z.q.lo) >= qulp )
	{
		qulp = 0.0;
	}
	else if ( z.q.lo < 0.0 )
		qulp = -qulp;

	z.q.lo += qulp;
	z.q.lo -= qulp;

	z.q.hi *= 4.0;

	if ( fabs(z.q.hi) == inf.d )
	{
		z.q.lo = 0.0;
		return ( z.ld );
	}

	z.q.lo *= 4.0;

	return ( z.ld );
}

