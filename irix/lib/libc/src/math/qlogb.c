/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* ====================================================================
 * ====================================================================
 *
 * Module: qlogb.c
 * $Revision: 1.1 $
 * $Date: 1997/07/22 17:23:37 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/qlogb.c,v $
 *
 * Revision history:
 *  21-Jul-97 - Original Version
 *
 * Description:	source code for logbl function
 *
 * ====================================================================
 * ====================================================================
 */


extern	long double	qlogb(long double);
extern	long double	logbl(long double);

#pragma weak qlogb = _logbl
#pragma weak logbl = _logbl

#include "synonyms.h"
#include <values.h>
#include <math.h>
#include <errno.h>
#include <inttypes.h>
#include "quad.h"

static const	ldu	zero =
{
{0x00000000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	ldu	m_one =
{
{0xbff00000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	ldu	inf =
{
{0x7ff00000,	0x00000000,
 0x00000000,	0x00000000},
};


/* ====================================================================
 *
 * FunctionName		logbl
 *
 * Description		computes unbiased exponent of arg
 *
 * ====================================================================
 */

long double
logbl( arg )
long double	arg;
{
ldquad	x;
long long ix, xpt;
double	y;
ldquad 	result;

	x.ld = arg;

	if ( x.q.hi != x.q.hi )
	{
		/* x is a NaN; return a quiet NaN */

		setoserror(EDOM);

		return ( arg );
	}

	/* extract exponent of x for some quick screening */

	DBL2LL(x.q.hi, ix);
	xpt = (ix >> DMANTWIDTH);
	xpt &= 0x7ff;

	if ( xpt == 0x7ff )
	{
		/* arg = +/-infinity */

		setoserror(EDOM);

		return ( inf.ld );
	}

	if ( x.ld == 0.0L )
	{
		setoserror(EDOM);

		result.ld = m_one.ld/zero.ld;

		return ( result.ld );
	}

	if ( xpt == 0 )
		return ( -1022.0L );

	/* determine true IEEE exponent, independent of our representation */

	if ( x.q.hi < 0.0 )
	{
		x.q.hi = -x.q.hi;
		x.q.lo = -x.q.lo;
	}

	DBL2LL(x.q.hi, ix);
	ix >>= DMANTWIDTH;
	ix <<= DMANTWIDTH;
	LL2DBL(ix, y);


	if ( (x.q.hi == y) && (x.q.lo < 0.0) )
		xpt--;

	result.ld = xpt - 0x3ff;

	return ( result.ld );
}

