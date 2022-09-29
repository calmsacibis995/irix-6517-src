/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: qmodf.c,v 1.4 1996/08/02 23:50:27 vegas Exp $"
#include <inttypes.h>
#include "quad.h"

/*  long double qmodf(long double value, long double *iptr)
 *
 *  returns the signed fractional part of value
 *  and stores the integer part indirectly through iptr.
 *
 *  If arg is +/-inf, sets *iptr to arg, and returns arg
 *
 *  If arg is a Nan, sets *iptr to qnan, and returns qnan
 *
 */

extern	long double	qmodf(long double, long double *);
extern	long double	modfl(long double, long double *);

#pragma weak qmodf = __qmodf
#pragma weak modfl = __qmodf

extern	double	_modf(double, double *);

extern	double	fabs(double);
#pragma intrinsic (fabs)

static const	ldu	qnan =
{
{0x7ff10000,	0x00000000,
 0x00000000,	0x00000000},
};

static const du		twop52 =
{0x43300000,	0x00000000};

static const	du	twop106 =
{0x46900000,	0x00000000};

static const du		infinity =
{0x7ff00000,	0x00000000};


/* ====================================================================
 *
 * FunctionName		qmodf
 *
 * Description		computes integral and fractional parts of arg
 *
 * ====================================================================
 */

long double
__qmodf( long double x, long double *p_xi )
{
ldquad	u;
ldquad	result;
double	y, yi;

	u.ld = x;

	if ( u.q.hi != u.q.hi )
	{
		result.ld = *p_xi = qnan.ld;

		return ( result.ld );
	}

	if ( fabs(u.q.hi) == infinity.d )
	{
		result.ld = *p_xi = x;

		return ( result.ld );
	}

	if ( fabs(u.q.hi) >= twop106.d )
	{
		/* arg is an integer */

		*p_xi = x;
		result.ld = 0.0L;

		return ( result.ld );
	}

	if ( fabs(u.q.hi) >= twop52.d )
	{
		/* u.q.hi is an integer */

		/* break low part of arg into integral and fractional parts */

		y = _modf(u.q.lo, &yi);

		if ( y == 0.0 )
		{
			*p_xi = x;

			return ( 0.0L );
		}

		if ( (u.q.hi > 0.0) && (y < 0.0) )
		{
			*p_xi = u.q.hi;
			*p_xi += yi;
			*p_xi -= 1.0L;

			return ( 1.0L + y );
		}

		if ( (u.q.hi < 0.0) && (y > 0.0) )
		{
			*p_xi = u.q.hi;
			*p_xi += yi;
			*p_xi += 1.0L;

			return ( y - 1.0L );
		}

		*p_xi = u.q.hi;
		*p_xi += yi;

		return ( y );
	}

	/* break high part of arg into integral and fractional parts */

	y = _modf(u.q.hi, &yi);

	result.ld = y;
	result.ld += u.q.lo;

	if ( (yi > 0.0) && (result.q.hi < 0.0) )
	{
		*p_xi = yi;
		*p_xi -= 1.0L;

		return ( 1.0L + result.ld );
	}

	if ( (yi < 0.0) && (result.q.hi > 0.0) )
	{
		*p_xi = yi;
		*p_xi += 1.0L;

		return ( result.ld - 1.0L );
	}

	*p_xi = yi;

	return ( result.ld );
}

