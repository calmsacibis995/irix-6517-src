#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

/* note that this routine must be kept in sync with the corresponding compiler
 * routine, c_q_mul in directory common/util
 */

/* const1 is 1.0 + 2^(53 - 53/2), i.e. 1.0 + 2^27 */

static	const du	const1 =
{0x41a00000,	0x02000000};

static	const du	twop590 =
{0x64d00000,	0x00000000};

static	const du	twopm590 =
{0x1b100000,	0x00000000};

static const du		inf =
{0x7ff00000,	0x00000000};

#define	NO	0
#define	YES	1

double	fabs(double);
#pragma intrinsic (fabs)

long double
__q_mul( xhi, xlo, yhi, ylo)
double xhi, xlo, yhi, ylo;
{
long long ixhi, iyhi;
long long xptxhi, xptyhi;
long long xpthi, xptlo;
int32_t	scaleup, scaledown;
long long iz, iw, iqulp;
double	xfactor, yfactor;
double	hx, tx, hy, ty;
double	p, q;
double	w, ww;
double	qulp;
ldquad	z, u;
double	rem;
double	a, v, vv;

	/* Avoid underflows and overflows in forming the products
	   x*const1.d, y*const1.d, x*y, and hx*hy by scaling if
	   necessary.  x and y should also be scaled if tx*ty is
	   a denormal.
	*/

#include "msg.h"

#ifdef QUAD_DEBUG
	printf("q_mul: xhi = %08x%08x\n", *(int32_t *)&xhi, *((int32_t *)&xhi + 1));
	printf("q_mul: xlo = %08x%08x\n", *(int32_t *)&xlo, *((int32_t *)&xlo + 1));
	printf("q_mul: yhi = %08x%08x\n", *(int32_t *)&yhi, *((int32_t *)&yhi + 1));
	printf("q_mul: ylo = %08x%08x\n", *(int32_t *)&ylo, *((int32_t *)&ylo + 1));
#endif

	DBL2LL(yhi, iyhi);
	xptyhi = (iyhi >> DMANTWIDTH);
	xptyhi &= 0x7ff;

	DBL2LL(xhi, ixhi);
	xptxhi = (ixhi >> DMANTWIDTH);
	xptxhi &= 0x7ff;

	xpthi = xptxhi;
	xptlo = xptyhi;

	if ( xptxhi < xptyhi )
	{
		xptlo = xptxhi;
		xpthi = xptyhi;
	}

	if ( (0x21a < xptlo) && (xpthi < 0x5fd) )
	{
		/* normal case */

		/* Form the exact product of xhi and yhi using
		 * Dekker's algorithm.
		 */

		p = xhi*const1.d;
	
		hx = (xhi - p) + p;
		tx = xhi - hx;
	
		q = yhi*const1.d;
	
		hy = (yhi - q) + q;
		ty = yhi - hy;
	
		u.q.hi = xhi*yhi;
		u.q.lo = (((hx*hy - u.q.hi) + hx*ty) + hy*tx) + tx*ty;

		/* Add the remaining pieces using the Kahan summation formula. */

		a = xhi*ylo;

		ww = u.q.lo + a;
		rem = u.q.lo - ww + a;

		v = u.q.hi + ww;
		vv = u.q.hi - v + ww;

		a = xlo*yhi + rem;
		u.q.lo = vv + a;

		rem = vv - u.q.lo + a;

		w = v + u.q.lo;
		ww = v - w + u.q.lo;

		a = xlo*ylo + rem;

		vv = ww + a;

		z.q.hi = w + vv;
		DBL2LL(z.q.hi, iz);
		z.q.lo = w - z.q.hi + vv;

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
	printf("q_mul: z.q.hi = %08x%08x\n", *(int32_t *)&z.q.hi, *((int32_t *)&z.q.hi + 1));
	printf("q_mul: z.q.lo = %08x%08x\n", *(int32_t *)&z.q.lo, *((int32_t *)&z.q.lo + 1));
#endif

		return ( z.ld );
	}
	else if ( (xptxhi == 0x7ff) || (xhi == 0.0) || (yhi == 0.0) )
	{
		z.q.hi = xhi*yhi;
		z.q.lo = 0.0;

#ifdef QUAD_DEBUG
	printf("q_mul: z.q.hi = %08x%08x\n", *(int32_t *)&z.q.hi, *((int32_t *)&z.q.hi + 1));
	printf("q_mul: z.q.lo = %08x%08x\n", *(int32_t *)&z.q.lo, *((int32_t *)&z.q.lo + 1));
#endif

		return ( z.ld );
	}
	else
	{
		xfactor = 1.0;
		yfactor = 1.0;
		scaleup = scaledown = NO;

		if ( xptxhi <= 0x21a )
		{
			xhi *= twop590.d;
			xlo *= twop590.d;
			xfactor = twopm590.d;
			scaleup = YES;
		}

		if ( xptyhi <= 0x21a )
		{
			yhi *= twop590.d;
			ylo *= twop590.d;
			yfactor = twopm590.d;
			scaleup = YES;
		}

		if ( xptxhi >= 0x5fd )
		{
			xhi *= twopm590.d;
			xlo *= twopm590.d;
			xfactor = twop590.d;
			scaledown = YES;
		}

		if ( xptyhi >= 0x5fd )
		{
			yhi *= twopm590.d;
			ylo *= twopm590.d;
			yfactor = twop590.d;
			scaledown = YES;
		}

		if ( (scaleup == YES) && (scaledown == YES) )
		{
			xfactor = yfactor = 1.0;
		}


		/* Form the exact product of xhi and yhi using
		 * Dekker's algorithm.
		 */

		p = xhi*const1.d;
	
		hx = (xhi - p) + p;
		tx = xhi - hx;
	
		q = yhi*const1.d;
	
		hy = (yhi - q) + q;
		ty = yhi - hy;
	
		u.q.hi = xhi*yhi;
		u.q.lo = (((hx*hy - u.q.hi) + hx*ty) + hy*tx) + tx*ty;

		/* Add the remaining pieces using the Kahan summation formula. */

		a = xhi*ylo;

		ww = u.q.lo + a;
		rem = u.q.lo - ww + a;

		v = u.q.hi + ww;
		vv = u.q.hi - v + ww;

		a = xlo*yhi + rem;
		u.q.lo = vv + a;

		rem = vv - u.q.lo + a;

		w = v + u.q.lo;
		ww = v - w + u.q.lo;

		a = xlo*ylo + rem;

		vv = ww + a;

		z.q.hi = w + vv;
		z.q.lo = w - z.q.hi + vv;

		/* Rescale z.q.hi and z.q.lo before rounding */

		w = z.q.hi*xfactor;
		w *= yfactor;

		if ( (w == 0.0) || (fabs(w) == inf.d) )
		{
			z.q.hi = w;
			z.q.lo = 0.0;

#ifdef QUAD_DEBUG
	printf("q_mul: z.q.hi = %08x%08x\n", *(int32_t *)&z.q.hi, *((int32_t *)&z.q.hi + 1));
	printf("q_mul: z.q.lo = %08x%08x\n", *(int32_t *)&z.q.lo, *((int32_t *)&z.q.lo + 1));
#endif

			return ( z.ld );
		}

		ww = z.q.lo*xfactor;
		ww *= yfactor;

		z.q.hi = w + ww;
		DBL2LL(z.q.hi, iz);
		z.q.lo = w - z.q.hi + ww;

#ifdef QUAD_DEBUG
	printf("q_mul: z.q.hi = %08x%08x\n", *(int32_t *)&z.q.hi, *((int32_t *)&z.q.hi + 1));
	printf("q_mul: z.q.lo = %08x%08x\n", *(int32_t *)&z.q.lo, *((int32_t *)&z.q.lo + 1));
#endif

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

back:
			if ( fabs(z.q.lo) >= qulp )
			{
				qulp = 0.0;
			}
			else if ( z.q.lo < 0.0 )
				qulp = -qulp;

			z.q.lo += qulp;
			z.q.lo -= qulp;
		}

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

	goto back;
}

