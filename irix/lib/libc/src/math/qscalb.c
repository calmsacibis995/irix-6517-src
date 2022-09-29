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

/* ====================================================================
 * ====================================================================
 *
 * Module: qscalb
 * Description: quad precision version of scalb
 *
 * ====================================================================
 * ====================================================================
 */

#ident	"$Id: qscalb.c,v 1.5 1997/06/11 23:03:40 vegas Exp $"

#ifdef __STDC__
	#pragma weak scalbl = _scalbl
	#pragma weak qscalb = _scalbl
	#pragma weak _qscalb = _scalbl
#endif

#include "synonyms.h"
#include <values.h>
#include <math.h>
#include <errno.h>
#include "fpparts.h"
#include <limits.h>
#include "math_extern.h"
#include <inttypes.h>
#include "quad.h"

/* ====================================================================
 *
 * FunctionName: qscalb(x, n)
 *
 * Description:  return x*2**N without computing 2**N - this is the
 *		 C library qldexp() routine except that signaling NANs
 *		 generate invalid op exception - errno = EDOM
 *
 * ====================================================================
 */

long double
scalbl(long double arg, long double n)
{
ldquad	x, dn;
ldquad	result;

	x.ld = arg;
	dn.ld = n;

	if ((EXPONENT(x.q.hi) == MAXEXP) && (HIFRACTION(x.q.hi) || LOFRACTION(x.q.hi)) ) {
		setoserror(EDOM);
		result.q.hi = x.q.hi*dn.q.hi; /* a signaling NaN raises exception */
		result.q.lo = 0.0;
		return (result.ld);
	}

	if ((EXPONENT(dn.q.hi) == MAXEXP) && (HIFRACTION(dn.q.hi) || LOFRACTION(dn.q.hi)) ) {
		setoserror(EDOM);
		result.q.hi = x.q.hi*dn.q.hi; /* a signaling NaN raises exception */
		result.q.lo = 0.0;
		return (result.ld);
	}

	if (x.q.hi == 0.0)
		return x.ld;

	if ((n >= (long double)INT_MAX) || (n <= (long double)INT_MIN)) {
	/* over or underflow */

		setoserror(ERANGE);

		if ( n > 0.0L )
			result.q.hi = ((x.q.hi > 0.0) ? HUGE_VAL : -HUGE_VAL);
		else
			result.q.hi = ((x.q.hi > 0.0) ? 0.0 : -0.0);

		result.q.lo = 0.0;

		return ( result.ld );
	}

	return( _qldexp(x.ld, (int)n) );
}
