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

#ifdef _CALL_MATHERR
#include <stdio.h>
#include <math.h>
#include <errno.h>
#endif

#include "libm.h"
#include "complex.h"

extern	dcomplex	__dcis(double);

#pragma weak __dcis = __libm_dcis

static const du	zero =
{0x00000000,	0x00000000};

static const du	half =
{0x3fe00000,	0x00000000};

static const du	one =
{0x3ff00000,	0x00000000};

static const du	Twop19xpi =
{0x413921fb,	0x54442d18};

static const du	rpiby2 =
{0x3fe45f30,	0x6dc9c883};

static const du	piby2hi =
{0x3ff921fb,	0x54400000};

static const du	piby2lo =
{0x3dd0b461,	0x1a600000};

static const du	piby2tiny =
{0x3ba3198a,	0x2e037073};

static const du	ph =
{0x3ff921fb,	0x50000000};

static const du	pl =
{0x3e5110b4,	0x60000000};

static const du	pt =
{0x3c91a626,	0x30000000};

static const du	pe =
{0x3ae8a2e0,	0x30000000};

static const du	pe2 =
{0x394c1cd1,	0x29024e09};

static const du	Ph =
{0x3ff921fb,	0x54000000};

static const du	Pl =
{0x3e110b46,	0x10000000};

static const du	Pt =
{0x3c5a6263,	0x3145c06e};

/* coefficients for polynomial approximation of sin on +/- pi/4 */

static const du	S[] =
{
{0x3ff00000,	0x00000000},
{0xbfc55555,	0x55555548},
{0x3f811111,	0x1110f7d0},
{0xbf2a01a0,	0x19bfdf03},
{0x3ec71de3,	0x567d4896},
{0xbe5ae5e5,	0xa9291691},
{0x3de5d8fd,	0x1fcf0ec1},
};

/* coefficients for polynomial approximation of cos on +/- pi/4 */

static const du	C[] =
{
{0x3ff00000,	0x00000000},
{0xbfdfffff,	0xffffff96},
{0x3fa55555,	0x5554f0ab},
{0xbf56c16c,	0x1640aaca},
{0x3efa019f,	0x81cb6a1d},
{0xbe927df4,	0x609cb202},
{0x3e21b8b9,	0x947ab5c8},
};

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

/* tables used to do argument reduction for args between +/- 16 radians;
   the sum of the high and low values of the kth entry is (k - 10)*pi/2
*/

static const du	tblh[] =
{
{0xc02f6a7a,	0x2955385e},
{0xc02c463a,	0xbeccb2bb},
{0xc02921fb,	0x54442d18},
{0xc025fdbb,	0xe9bba775},
{0xc022d97c,	0x7f3321d2},
{0xc01f6a7a,	0x2955385e},
{0xc01921fb,	0x54442d18},
{0xc012d97c,	0x7f3321d2},
{0xc00921fb,	0x54442d18},
{0xbff921fb,	0x54442d18},
{0x00000000,	0x00000000},
{0x3ff921fb,	0x54442d18},
{0x400921fb,	0x54442d18},
{0x4012d97c,	0x7f3321d2},
{0x401921fb,	0x54442d18},
{0x401f6a7a,	0x2955385e},
{0x4022d97c,	0x7f3321d2},
{0x4025fdbb,	0xe9bba775},
{0x402921fb,	0x54442d18},
{0x402c463a,	0xbeccb2bb},
{0x402f6a7a,	0x2955385e},
};

static const du	tbll[] =
{
{0xbcc60faf,	0xbfd97309},
{0xbcc3daea,	0xf976e788},
{0xbcc1a626,	0x33145c07},
{0xbcbee2c2,	0xd963a10c},
{0xbcba7939,	0x4c9e8a0a},
{0xbcb60faf,	0xbfd97309},
{0xbcb1a626,	0x33145c07},
{0xbcaa7939,	0x4c9e8a0a},
{0xbca1a626,	0x33145c07},
{0xbc91a626,	0x33145c07},
{0x00000000,	0x00000000},
{0x3c91a626,	0x33145c07},
{0x3ca1a626,	0x33145c07},
{0x3caa7939,	0x4c9e8a0a},
{0x3cb1a626,	0x33145c07},
{0x3cb60faf,	0xbfd97309},
{0x3cba7939,	0x4c9e8a0a},
{0x3cbee2c2,	0xd963a10c},
{0x3cc1a626,	0x33145c07},
{0x3cc3daea,	0xf976e788},
{0x3cc60faf,	0xbfd97309},
};
#endif


/* ====================================================================
 *
 * FunctionName    __libm_dcis
 *
 * Description    computes cos(arg) + i*sin(arg)
 *
 * Note:  Routine __libm_dcis() computes cos(arg) + i*sin(arg) and should
 *	  be kept in sync with sin() and cos() so that it returns 
 *	  the same result as one gets by calling cos() and sin() 
 *	  separately.
 *
 * ====================================================================
 */

dcomplex
__libm_dcis(double x)
{
#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)
int	ix, xpt, m, l;
#else
long long ix, xpt, m, l;
#endif

double  xsq;
double  cospoly, sinpoly;
int  n;
dcomplex  result;
double  absx;
double  y, z, dn;
double  dn1, dn2;
double  zz;
double	s, ss;
double	t, w, ww;
#ifdef _CALL_MATHERR
struct exception	exstruct;
#endif

	/* extract exponent of x and 1 bit of mantissa for some quick screening */

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

	DBLHI2INT(x, ix);	/* copy MSW of x to ix	*/
#else
	DBL2LL(x, ix);		/* copy x to ix	*/
#endif
	xpt = (ix >> (DMANTWIDTH-1));
	xpt &= 0xfff;

	if ( xpt < 0x7fd )
	{
		/*   |x| < 0.75   */

		if ( xpt >= 0x7c2 )
		{
      			/*   |x| >= 2^(-30)   */

      			xsq = x*x;

			result.dreal = (((((C[6].d*xsq + C[5].d)*xsq +
					C[4].d)*xsq + C[3].d)*xsq +
					C[2].d)*xsq + C[1].d)*xsq + C[0].d;

			result.dimag = (((((S[6].d*xsq + S[5].d)*xsq +
					S[4].d)*xsq + S[3].d)*xsq +
					S[2].d)*xsq + S[1].d)*(xsq*x) + x;

			return ( result );
		}
		else
		{
			result.dreal = 1.0;
			result.dimag = x;

			return ( result );
		}
	}

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

	if (xpt < 0x806)
	{
		/*   |x| < 16.0   */

		/*  do a table based argument reduction to +/- pi/4  */

		dn = x*rpiby2.d;

		n = ROUND(dn);

		/*  compute x - n*pi/2  */

		x = x - tblh[n+10].d;
		x = x - tbll[n+10].d;

		goto L;

	} else
#endif
	if ( xpt < 0x827 )
	{
		/*  |x| < 1.5*2^20  */

cont:
		dn = x*rpiby2.d;
		n = ROUND(dn);
		dn = n;

		x = x - dn*piby2hi.d;
		x = x - dn*piby2lo.d;
		x = x - dn*piby2tiny.d;	/* x = x - n*pi/2 */

L:
		xsq = x*x;

		cospoly = (((((C[6].d*xsq + C[5].d)*xsq +
				C[4].d)*xsq + C[3].d)*xsq +
				C[2].d)*xsq + C[1].d)*xsq + C[0].d;

		sinpoly = (((((S[6].d*xsq + S[5].d)*xsq +
				S[4].d)*xsq + S[3].d)*xsq +
				S[2].d)*xsq + S[1].d)*(xsq*x) + x;

		if ( n&1 )
		{
			if ( n&2 )
			{
				/*
				 *  n%4 = 3
				 *  result is sin(x) - i*cos(x)
				 */

				result.dreal = sinpoly;
				result.dimag = -cospoly;
			}
			else
			{
				/*
				 *  n%4 = 1
				 *  result is -sin(x) + i*cos(x)
				 */

				result.dreal = -sinpoly;
				result.dimag = cospoly;
			}

			return (result);
		}

		if ( n&2 )
		{
			/*
			 *  n%4 = 2
			 *  result is -cos(x) - i*sin(x)
			 */

			result.dreal = -cospoly;
			result.dimag = -sinpoly;
		}
		else
		{
			/*
			 *  n%4 = 0
			 *  result is cos(x) + i*sin(x)
			 */

			result.dreal = cospoly;
			result.dimag = sinpoly;
		}

		return(result);

	}
	else if ( xpt < 0x836 )
	{
		/*  |x| < 2^28  */

		if ( fabs(x) < Twop19xpi.d )
			goto cont;

		dn = x*rpiby2.d;
		n = ROUND(dn);
		dn = n;

		x = x - dn*ph.d;
		x = x - dn*pl.d;
		x = x - dn*pt.d;
		x = x - dn*pe.d;
		x = x - dn*pe2.d;

		goto L;
	}
	else if ( xpt < 0x862 )
	{
		/*  |x| < 2^50  */

		absx = fabs(x);

		dn = z = absx*rpiby2.d;

		/* round dn to the nearest integer */

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

		DBLHI2INT(dn, l);
		m = (l >> DMANTWIDTH);
		m &= 0x7ff;

		/* shift off fractional bits of dn */

		DBLLO2INT(dn, l);

		l >>= (0x433 - m);
		n = l;
		l <<= (0x433 - m);
		INT2DBLLO(l, dn);
#else
		DBL2LL(dn, l);
		m = (l >> DMANTWIDTH);
		m &= 0x7ff;

		/* shift off fractional bits of dn */

		l >>= (0x433 - m);
		n = l;
		l <<= (0x433 - m);
		LL2DBL(l, dn);
#endif
		/* adjust dn and n if the fractional part of dn 
		   was >= 0.5
		*/

		n &= 3;

		if ( (z - dn) >= half.d )
		{
			dn += one.d;
			n += 1;
		}

	/* compute x - dn*Ph - dn*Pl - dn*Pt by dividing dn into
	   two parts and using the Kahan summation formula
	*/

		/* split dn into 2 parts */

		dn1 = dn;

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

		DBLLO2INT(dn1, m);
		m >>= 28;
		m <<= 28;
		INT2DBLLO(m, dn1);
#else
		DBL2LL(dn1, m);
		m >>= 28;
		m <<= 28;
		LL2DBL(m, dn1);
#endif
		dn2 = dn - dn1;

		z = absx - dn1*Ph.d;	/* this operation is exact */

		t = dn2*Ph.d;
		s = z - t;
		ss = z - s - t;		/* correction term */

		t = ss - dn1*Pl.d;
		w = s + t;
		ww = s - w + t;		/* correction term */

		t = ww - dn2*Pl.d;
		s = w + t;
		ss = w - s + t;		/* correction term */

		t = ss - dn1*Pt.d;
		w = s + t;
		ww = s - w + t;		/* correction term */

		t = ww - dn2*Pt.d;
		z = w + t;		/* z = reduced arg */

		if ( x < 0.0 )
		{	/* adjust for sign of arg */

			z = -z;
			n = -n;
		}

		x = z;

		goto L;
	}


	if (x != x)
	{
		/* x is a NaN; return a pair of quiet NaNs */

#ifdef _IP_NAN_SETS_ERRNO
		/*
		 * Commented out the following line because I don't
		 * think anyone cares about this feature from the
		 * context of libtoolroot.so.  And because I couldn't
		 * figure out how to get errnoaddr working correctly
		 * within libtoolroot.so :-)
		 *	bean 970917
		 */
/*		*__errnoaddr = EDOM;	*/
#endif
		result.dreal = __libm_qnan_d;
		result.dimag = __libm_qnan_d;
		return (result);
	}

	/* just give up and return 0.0 */

	result.dreal = 0.0;
	result.dimag = 0.0;
	return (result);
}

