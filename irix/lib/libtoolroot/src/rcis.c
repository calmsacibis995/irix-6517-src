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

extern	complex	__rcis(float);

#pragma weak __rcis = __libm_rcis

/* coefficients for polynomial approximation of sin on +/- pi/4 */

static const du  S[] =
{
{0x3ff00000,  0x00000000},
{0xbfc55554,  0x5268a030},
{0x3f811073,  0xafd14db9},
{0xbf29943e,  0x0fc79aa9},
};

/* coefficients for polynomial approximation of cos on +/- pi/4 */

static const du  C[] =
{
{0x3ff00000,  0x00000000},
{0xbfdffffb,  0x2a77e083},
{0x3fa553e7,  0xf02ac8aa},
{0xbf5644d6,  0x2993c4ad},
};

static const du  rpiby2 =
{0x3fe45f30,  0x6dc9c883};

static const du  piby2hi =
{0x3ff921fb,  0x50000000};

static const du  piby2lo =
{0x3e5110b4,  0x611a6263};


/* ====================================================================
 *
 * FunctionName    __libm_rcis
 *
 * Description    computes cos(arg) + i*sin(arg)
 *
 * Note:  Routine __libm_rcis() computes cosf(arg) + i*sinf(arg) and should
 *	  be kept in sync with sinf() and cosf() so that it returns 
 *	  the same result as one gets by calling cosf() and sinf() 
 *	  separately.
 *
 * ====================================================================
 */

complex
__libm_rcis(float x)
{
int  n;
int  ix, xpt;
double  dx, xsq;
double  dn;
double  sinpoly, cospoly;
complex  result;
#ifdef _CALL_MATHERR
struct exception	exstruct;
#endif

	FLT2INT(x, ix);	/* copy arg to an integer */

	xpt = (ix >> (MANTWIDTH-1));
	xpt &= 0x1ff;

	/* xpt is exponent(x) + 1 bit of mantissa */
	
	if ( xpt < 0xfd )
	{
		/* |x| < .75 */

		if ( xpt >= 0xe6 )
		{
			/* |x| >= 2^(-12) */

			dx = x;

			xsq = dx*dx;

			result.real = ((C[3].d*xsq + C[2].d)*xsq + C[1].d)*xsq + C[0].d;
			result.imag = ((S[3].d*xsq + S[2].d)*xsq + S[1].d)*(xsq*dx) + dx;

			return ( result );
		}
		else
		{
			result.real = 1.0f;
			result.imag = x;

			return ( result );
		}

	}

	if (xpt < 0x136)
	{
		/*  |x| < 2^28  */

		dx = x;
		dn = dx*rpiby2.d;

		n = ROUND(dn);
		dn = n;

		dx = dx - dn*piby2hi.d;
		dx = dx - dn*piby2lo.d;  /* dx = x - n*piby2 */

		xsq = dx*dx;

		sinpoly = ((S[3].d*xsq + S[2].d)*xsq + S[1].d)*(xsq*dx) + dx;
		cospoly = ((C[3].d*xsq + C[2].d)*xsq + C[1].d)*xsq + C[0].d;

		if ( n&1 )
		{
			if ( n&2 )
			{
				/*
				 *  n%4 = 3
				 *  result is sin(x) - i*cos(x)
				 */

				result.real = sinpoly;
				result.imag = -cospoly;
			}
			else
			{
				/*
				 *  n%4 = 1
				 *  result is -sin(x) + i*cos(x)
				 */

				result.real = -sinpoly;
				result.imag = cospoly;
			}

			return (result);
		}

		if ( n&2 )
		{
			/*
			 *  n%4 = 2
			 *  result is -cos(x) - i*sin(x)
			 */

			result.real = -cospoly;
			result.imag = -sinpoly;
		}
		else
		{
			/*
			 *  n%4 = 0
			 *  result is cos(x) + i*sin(x)
			 */

			result.real = cospoly;
			result.imag = sinpoly;
		}

		return ( result );
	}

	if ( (x != x) || (fabsf(x) == __libm_inf_f) )
	{
		/* x is a NaN or +/-inf; return a pair of quiet NaNs */

#ifdef _CALL_MATHERR

                exstruct.type = DOMAIN;
                exstruct.name = "cosf";
                exstruct.arg1 = x;
                exstruct.retval = __libm_qnan_f;

                if ( matherr( &exstruct ) == 0 )
                {
                        fprintf(stderr, "domain error in cosf\n");
                        SETERRNO(EDOM);
                }

		result.real = exstruct.retval;

                exstruct.type = DOMAIN;
                exstruct.name = "sinf";
                exstruct.arg1 = x;
                exstruct.retval = __libm_qnan_f;

                if ( matherr( &exstruct ) == 0 )
                {
                        fprintf(stderr, "domain error in sinf\n");
                        SETERRNO(EDOM);
                }

		result.imag = exstruct.retval;

                return ( result );
#else
		NAN_SETERRNO(EDOM);
		
		result.real = result.imag = __libm_qnan_f;

		return ( result );
#endif
	}

	/* just give up and return 0.0, setting errno = ERANGE */

#ifdef _CALL_MATHERR

		exstruct.type = TLOSS;
		exstruct.name = "cosf";
		exstruct.arg1 = x;
		exstruct.retval = 0.0f;

		if ( matherr( &exstruct ) == 0 )
		{
			fprintf(stderr, "range error in cosf (total loss \
of significance)\n");
			SETERRNO(ERANGE);
		}

		result.real = exstruct.retval;

		exstruct.type = TLOSS;
		exstruct.name = "sinf";
		exstruct.arg1 = x;
		exstruct.retval = 0.0f;

		if ( matherr( &exstruct ) == 0 )
		{
			fprintf(stderr, "range error in sinf (total loss \
of significance)\n");
			SETERRNO(ERANGE);
		}

		result.imag = exstruct.retval;

		return ( result );

#else
		SETERRNO(ERANGE);

		result.real = result.imag = 0.0f;

		return ( result );
#endif

}

