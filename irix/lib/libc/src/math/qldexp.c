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
 * Description: quad precision version of ldexp
 */
#ident "$Id: qldexp.c,v 1.6 1997/10/10 17:31:24 vegas Exp $"

#ifdef __STDC__
	#pragma weak ldexpl = _ldexpl
	#pragma weak qldexp = _ldexpl
	#pragma weak _qldexp = _ldexpl
#endif

#include "synonyms.h"
#include <values.h>
#include "fpparts.h"
#include "math_extern.h"
#include <math.h>

/* ====================================================================
 *
 * FunctionName: qldexp
 *
 * Description: scales a long double by a power of 2
 *
 * ====================================================================
 */

long double
ldexpl(long double value, int exp)
{
_ldblval	x, result;
double	z, zz;

	x.ldbl = value;

	result.qparts.hi.d = _ldexp(x.qparts.hi.d, exp);

	if ( (result.qparts.hi.fparts.exp == 0) || (result.qparts.hi.fparts.exp == 0x7ff) )
	{
		result.qparts.lo.d = 0.0;

		return ( result.ldbl );
	}

	result.qparts.lo.d = _ldexp(x.qparts.lo.d, exp);

	/* now must normalize the result */

	z = result.qparts.hi.d + result.qparts.lo.d;
	zz = result.qparts.hi.d - z + result.qparts.lo.d;

	result.qparts.hi.d = z;
	result.qparts.lo.d = zz;

	return ( result.ldbl );
}
