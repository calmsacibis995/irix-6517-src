#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

/* note that this routine must be kept in sync with the corresponding compiler
 * routine, c_q_div in directory common/util
 */

static const du		twopm968 =
{0x03700000,	0x00000000};

static const du		twopm54 =
{0x3c900000,	0x00000000};

static const du		twop52 =
{0x43300000,	0x00000000};

static const du		inf =
{0x7ff00000,	0x00000000};

extern	long double	_prodl(double, double);

double	fabs(double);
#pragma intrinsic (fabs)

#define	EXPBIAS	0x3ff

	/* computes the quotient of two long doubles */

long double
__q_div( xhi, xlo, yhi, ylo )
double xhi, xlo, yhi, ylo;
{
int32_t	n;
long long ixhi, iyhi;
long long xptxhi, xptyhi;
long long ix, iy, iz;
double	c, cc, w, ww;
double	quarterulp;
double	xfactor, yfactor;
double	z, zz;
ldquad	u;
ldquad	result;

	/* adapted from T. J. Dekker's div2 subroutine */

#include "msg.h"

#ifdef QUAD_DEBUG
	printf("q_div: xhi = %08x%08x\n", *(int32_t *)&xhi, *((int32_t *)&xhi + 1));
	printf("q_div: xlo = %08x%08x\n", *(int32_t *)&xlo, *((int32_t *)&xlo + 1));
	printf("q_div: yhi = %08x%08x\n", *(int32_t *)&yhi, *((int32_t *)&yhi + 1));
	printf("q_div: ylo = %08x%08x\n", *(int32_t *)&ylo, *((int32_t *)&ylo + 1));
#endif

	/* extract exponents of y and x for some quick screening */

	DBL2LL(yhi, iyhi);
	xptyhi = (iyhi >> DMANTWIDTH);
	xptyhi &= 0x7ff;

	DBL2LL(xhi, ixhi);
	xptxhi = (ixhi >> DMANTWIDTH);
	xptxhi &= 0x7ff;

	/* Avoid underflows and overflows in forming the products
	   x*const1.d, y*const1.d, x*y, and hx*hy by scaling if
	   necessary.  x and y are also be scaled if tx*ty is
	   a denormal.
	*/

	if ( (0x30d < xptxhi) && (xptxhi < 0x4f1) && 
	     (0x30d < xptyhi) && (xptyhi < 0x4f1) 
	   )
	{
		/* normal case */

		c = xhi/yhi;

		u.ld = _prodl(c, yhi);

		cc = ((xhi - u.q.hi - u.q.lo + xlo) - c*ylo)/yhi;

		z = c + cc;
		zz = (c - z) + cc;

#ifdef QUAD_DEBUG
	printf("q_div: z = %08x%08x\n", *(int32_t *)&z, *((int32_t *)&z + 1));
	printf("q_div: zz = %08x%08x\n", *(int32_t *)&zz, *((int32_t *)&zz + 1));
#endif

		/* if necessary, round zz so that the sum of z and zz has
		   at most 107 significant bits
		*/

		if ( fabs(z) >= twopm968.d )
		{
			/* determine true exponent of z + zz as a 107 bit number */

			DBL2LL(z, iz);
			iz >>= DMANTWIDTH;
			iz <<= DMANTWIDTH;
			LL2DBL(iz, w); /* w = dtwofloor(z) */

			if ( (z == w) && ((z > 0.0 && zz < 0.0) ||
			     (z < 0.0 && zz > 0.0))
			   )
				w *= 0.5;

			/* round zz if it's less than 1/4 ulp of w */

			quarterulp = twopm54.d*fabs(w);

			if ( fabs(zz) < quarterulp )
			{
				if ( zz >= 0.0 )
				{
					zz = (quarterulp + zz) - quarterulp;
				}
				else
				{
					zz = quarterulp + (zz - quarterulp);
				}

				w = z + zz;
				ww = (z - w) + zz;

				z = w;
				zz = ww;
			}
		}

		result.q.hi = z;
		result.q.lo = zz;

#ifdef QUAD_DEBUG
	printf("q_div: result.hi = %08x%08x\n", *(int32_t *)&result.hi, *((int32_t *)&result.hi + 1));
	printf("q_div: result.lo = %08x%08x\n", *(int32_t *)&result.lo, *((int32_t *)&result.lo + 1));
#endif

		return ( result.ld );
	}

	if ( (xptxhi < 0x7ff) && (xptyhi < 0x7ff) )
	{
		if ( (xhi == 0.0) || (yhi == 0.0) )
		{
			result.q.hi = xhi/yhi;
			result.q.lo = 0.0;

#ifdef QUAD_DEBUG
	printf("q_div: result.hi = %08x%08x\n", *(int32_t *)&result.hi, *((int32_t *)&result.hi + 1));
	printf("q_div: result.lo = %08x%08x\n", *(int32_t *)&result.lo, *((int32_t *)&result.lo + 1));
#endif

			return ( result.ld );
		}

		xfactor = 1.0;
		yfactor = 1.0;

		/* Scale x and y appropriately and use previous algorithm.
		   Then unscale result.
		*/

		if ( xptxhi <= 0x30d )
		{
			xhi *= twop52.d;	/* first, make sure x is normal */
			xlo *= twop52.d;

			/* now scale x so that its exponent is 0x30e */

			DBL2LL(xhi, ix);
			ix >>= DMANTWIDTH;
			n = (ix & 0x7ff);

			ix = (0x30e - n) + EXPBIAS;
			ix <<= DMANTWIDTH;
			LL2DBL(ix, c);
			xhi *= c;
			xlo *= c;
			ix = (n - 52 - 0x30e) + EXPBIAS;
			ix <<= DMANTWIDTH;
			LL2DBL(ix, xfactor);
		}

		if ( xptyhi <= 0x30d )
		{
			yhi *= twop52.d;	/* first, make sure y is normal */
			ylo *= twop52.d;

			/* now scale y so that its exponent is 0x30e */

			DBL2LL(yhi, iy);
			iy >>= DMANTWIDTH;
			n = (iy & 0x7ff);

			iy = (0x30e - n) + EXPBIAS;
			iy <<= DMANTWIDTH;
			LL2DBL(iy, c);
			yhi *= c;
			ylo *= c;
			iy = (0x30e - n + 52) + EXPBIAS;
			iy <<= DMANTWIDTH;
			LL2DBL(iy, yfactor);
		}

		if ( xptxhi >= 0x4f1 )
		{
			/* scale x so that its exponent is 0x4f1 */

			DBL2LL(xhi, ix);
			ix >>= DMANTWIDTH;
			n = (ix & 0x7ff);

			ix = (0x4f1 - n) + EXPBIAS;
			ix <<= DMANTWIDTH;
			LL2DBL(ix, c);
			xhi *= c;
			xlo *= c;
			ix = (n - 0x4f1) + EXPBIAS;
			ix <<= DMANTWIDTH;
			LL2DBL(ix, xfactor);
		}

		if ( xptyhi >= 0x4f1 )
		{
			/* scale y so that its exponent is 0x4f1 */

			DBL2LL(yhi, iy);
			iy >>= DMANTWIDTH;
			n = (iy & 0x7ff);

			iy = (0x4f1 - n) + EXPBIAS;
			iy <<= DMANTWIDTH;
			LL2DBL(iy, c);
			yhi *= c;
			ylo *= c;
			iy = (0x4f1 - n) + EXPBIAS;
			iy <<= DMANTWIDTH;
			LL2DBL(iy, yfactor);
		}

		c = xhi/yhi;

		u.ld = _prodl(c, yhi);

		cc = ((xhi - u.q.hi - u.q.lo + xlo) - c*ylo)/yhi;

		z = c + cc;
		zz = (c - z) + cc;

#ifdef QUAD_DEBUG
	printf("q_div: z = %08x%08x\n", *(int32_t *)&z, *((int32_t *)&z + 1));
	printf("q_div: zz = %08x%08x\n", *(int32_t *)&zz, *((int32_t *)&zz + 1));
#endif

		/* if necessary, round zz so that the sum of z and zz has
		   at most 107 significant bits
		*/

		if ( fabs(z) >= twopm968.d )
		{
			/* determine true exponent of z + zz as a 107 bit number */

			DBL2LL(z, iz);
			iz >>= DMANTWIDTH;
			iz <<= DMANTWIDTH;
			LL2DBL(iz, w); /* w = dtwofloor(z) */

			if ( (z == w) && ((z > 0.0 && zz < 0.0) ||
			     (z < 0.0 && zz > 0.0))
			   )
				w *= 0.5;

			/* round zz if it's less than 1/4 ulp of w */

			quarterulp = twopm54.d*fabs(w);

			if ( fabs(zz) < quarterulp )
			{
				if ( zz >= 0.0 )
				{
					zz = (quarterulp + zz) - quarterulp;
				}
				else
				{
					zz = quarterulp + (zz - quarterulp);
				}

				w = z + zz;
				ww = (z - w) + zz;

				z = w;
				zz = ww;
			}
		}

		if ( ((xfactor <= 1.0) && (1.0 <= yfactor)) ||
		     ((yfactor <= 1.0) && (1.0 <= xfactor))
		   )
		{
			z = z*(xfactor*yfactor);
		}
		else
		{
			z *= xfactor;
			z *= yfactor;
		}

		if ( (z == 0.0) || (fabs(z) == inf.d) )
		{
			result.q.hi = z;
			result.q.lo = 0.0;

#ifdef QUAD_DEBUG
	printf("q_div: result.hi = %08x%08x\n", *(int32_t *)&result.hi, *((int32_t *)&result.hi + 1));
	printf("q_div: result.lo = %08x%08x\n", *(int32_t *)&result.lo, *((int32_t *)&result.lo + 1));
#endif

			return ( result.ld );
		}

		if ( ((xfactor <= 1.0) && (1.0 <= yfactor)) ||
		     ((yfactor <= 1.0) && (1.0 <= xfactor))
		   )
		{
			zz = zz*(xfactor*yfactor);
		}
		else
		{
			zz *= xfactor;
			zz *= yfactor;
		}

		result.q.hi = z + zz;
		result.q.lo = (z - result.q.hi) + zz;

#ifdef QUAD_DEBUG
	printf("q_div: result.hi = %08x%08x\n", *(int32_t *)&result.hi, *((int32_t *)&result.hi + 1));
	printf("q_div: result.lo = %08x%08x\n", *(int32_t *)&result.lo, *((int32_t *)&result.lo + 1));
#endif

		return ( result.ld );
	}

	result.q.hi = xhi/yhi;
	result.q.lo = 0.0;

#ifdef QUAD_DEBUG
	printf("q_div: result.hi = %08x%08x\n", *(int32_t *)&result.hi, *((int32_t *)&result.hi + 1));
	printf("q_div: result.lo = %08x%08x\n", *(int32_t *)&result.lo, *((int32_t *)&result.lo + 1));
#endif

	return ( result.ld );
}

