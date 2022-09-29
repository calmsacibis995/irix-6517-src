/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
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
#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/nextafter.c,v 1.5 1997/06/18 00:31:37 vegas Exp $"
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

/* IEEE recommended functions */

#ifdef __STDC__
	#pragma weak nextafter = _nextafter
#endif
#include "synonyms.h"
#include <values.h>
#include <errno.h>
#include "fpparts.h"

static	int	supports_denorms( double );


/* NEXTAFTER(X,Y)
 * nextafter(x,y) returns the next representable neighbor of x
 * in the direction of y
 * Special cases:
 * 1) if either x or y is a NaN then the result is one of the NaNs, errno
 * 	=EDOM
 * 2) if x is +-inf, x is returned and errno = EDOM
 * 3) if x == y the results is x without any exceptions being signalled
 * 4) overflow  and inexact are signalled when x is finite,
 *	but nextafter(x,y) is not, errno = ERANGE
 * 5) underflow and inexact are signalled when nextafter(x,y)
 * 	lies between +-(2**-1022), errno = ERANGE
 */
 double nextafter(x,y)
 double x,y;
 {
	if (EXPONENT(x) == MAXEXP) { /* Nan or inf */
		setoserror(EDOM);
		return x; 
	}

	if ((EXPONENT(y) == MAXEXP) && (HIFRACTION(y) || 
		LOFRACTION(y))) {
		setoserror(EDOM);
		return y;  /* y a NaN */
	}

	if (x == y)
		return x;

	if ( (x == 0.0) && (y > 0.0) ) {
		setoserror(ERANGE);

		if ( supports_denorms( MINDOUBLE ) ) {
			HIWORD(x) = 0x0;
			LOFRACTION(x) = 0x1;
			return ( x );

		} else {
			return ( MINDOUBLE );
		}
	}
	else if ( (x == 0.0) && (y < 0.0) ) {
		setoserror(ERANGE);

		if ( supports_denorms( MINDOUBLE ) ) {
			HIWORD(x) = 0x0;
			SIGNBIT(x) = 0x1;
			LOFRACTION(x) = 0x1;
			return ( x );

		} else {
			return ( -MINDOUBLE );
		}
	}
	else if ( (x == MINDOUBLE) && (y < x) ) {
		setoserror(ERANGE);

		if ( supports_denorms( MINDOUBLE ) ) {
			HIWORD(x) = 0x0;
			HIFRACTION(x) = 0xfffff;
			LOFRACTION(x) = 0xffffffff;
			return ( x );

		} else {
			return ( 0.0 );
		}
	}
	else if ( (x == -MINDOUBLE) && (y > x) ) {
		setoserror(ERANGE);

		if ( supports_denorms( MINDOUBLE ) ) {
			HIWORD(x) = 0x0;
			SIGNBIT(x) = 0x1;
			HIFRACTION(x) = 0xfffff;
			LOFRACTION(x) = 0xffffffff;
			return ( x );

		} else {
			return ( -0.0 );
		}
	}
	else if (((y > x) && !SIGNBIT(x)) || ((y < x) && SIGNBIT(x))) {
		/* y>x, x positive or y<x, x negative */

		if (LOFRACTION(x) != (unsigned)0xffffffff)
			LOFRACTION(x) += 0x1;
		else {
			LOFRACTION(x) = 0x0;
			if (((unsigned)(HIWORD(x) & 0x7fffffff)) < (unsigned)0x7ff00000)
				HIWORD(x) += 0x1;
		}
	}
	else { /* y<x, x pos or y>x, x neg */

		if (LOFRACTION(x) != 0x0)
			LOFRACTION(x) -= 0x1;
		else {
			LOWORD(x) = (unsigned)0xffffffff;
			if ((HIWORD(x) & 0x7fffffff) != 0x0)
				HIWORD(x) -=0x1;
		}
	}

	if (EXPONENT(x) == MAXEXP) { /* signal overflow and inexact */
		x = (SIGNBIT(x) ? -MAXDOUBLE : MAXDOUBLE);
		setoserror(ERANGE);
		return( x*2.0 );
	}

        if (EXPONENT(x) == 0) {
        /* result is a de-normal or zero */
                setoserror(ERANGE);
                return( x );
        }

	return x;
}

static
int
supports_denorms( double x )
{
	return ( x*0.5 != 0.0 );
}
