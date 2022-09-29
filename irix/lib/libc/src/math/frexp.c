/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/frexp.c	1.9.1.4"
/*LINTLIBRARY*/
/*
 * frexp(value, eptr)
 * returns a double x such that x = 0 or 0.5 <= |x| < 1.0
 * and stores an integer n such that value = x * 2 ** n
 * indirectly through eptr.
 *
 */

#ifdef FREXP_DEBUG
#include <stdio.h>
#endif

#include <sys/types.h>
#include "synonyms.h"
#include "shlib.h"
#include <nan.h>
#include <signal.h>
#include <unistd.h>
#include <float.h>
#include <fp_class.h>
#include <errno.h>
#include <values.h>
#include "fpparts.h"

typedef  union {
	__uint32_t	i[2];
	__uint64_t	ll;
} _ullval;

double
frexp(x, eptr)
double x; /* don't declare register, because of KILLNan! */
int *eptr;
{
_dval value;			/* the value manipulated */ 
__int32_t exp;			/* exponent of number */ 
_ullval	frac;			/* fraction of number */
int class;

	class = _fp_class_d(x);

	if (class < FP_POS_NORM)
	{
		/* this handles NaNs and INFs */

		setoserror(EDOM);

		*eptr = DBL_MAX_EXP+1;

		if (class == FP_NEG_INF)
			return(-.5);
		else if ((class == FP_SNAN) || (class == FP_QNAN))
		{
                        *eptr = 0;
                        return(x);
                }
		else
			return(.5);
	}

	value = *((_dval *)&x);

	exp = EXPONENT(value);
	frac.i[0] = value.fparts.hi;
	frac.i[1] = value.fparts.lo;

#ifdef FREXP_DEBUG
	fprintf(stderr, "exp = %d\n", exp);
	fprintf(stderr, "frac.i[0] = %08x\n", frac.i[0]);
	fprintf(stderr, "frac.i[1] = %08x\n\n", frac.i[1]);
#endif

	if ( exp != 0 )
	{
		value.fparts.exp = 0x3fe;
		*eptr = exp - 0x3fe;

		return ( value.d );
	}
	else if ( (frac.i[0] == 0) && (frac.i[1] == 0) )
	{
		*eptr = 0;
		return ( 0.0 );
	}
	else while ( (frac.i[0] & (1 << 20)) == 0 )
	{
		frac.i[0] <<= 1;
		frac.i[0] |= (frac.i[1] >> 31);
		frac.i[1] <<= 1;
		exp--;
	}

#ifdef FREXP_DEBUG
	fprintf(stderr, "exp = %d\n", exp);
	fprintf(stderr, "frac.i[0] = %08x\n", frac.i[0]);
	fprintf(stderr, "frac.i[1] = %08x\n", frac.i[1]);
#endif
	value.fparts.hi = (frac.i[0] & 0x000fffff);
	value.fparts.lo = frac.i[1];
	value.fparts.exp = 0x3fe;

	*eptr = exp - 0x3fd;

	return ( value.d );
}

