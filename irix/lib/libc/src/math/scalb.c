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
#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/scalb.c,v 1.5 1997/06/11 22:38:01 vegas Exp $"
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

/* SCALB(X,N)
 * return x * 2**N without computing 2**N - this is the standard
 * C library ldexp() routine except that signaling NANs generate
 * invalid op exception - errno = EDOM
 */

#ifdef __STDC__
	#pragma weak scalb = _scalb
#endif
#include "synonyms.h"
#include <values.h>
#include <math.h>
#include <errno.h>
#include "fpparts.h"
#include <limits.h>

#ifndef	_IEEE
#define	_IEEE	1
#endif

double scalb(double x, double n)
{
#if _IEEE
	if ((EXPONENT(x) == MAXEXP) && (HIFRACTION(x) || LOFRACTION(x)) ) {
		setoserror(EDOM);
		return (x*n); /* return quiet NaN - raise exception
			       * if either x or n is a signalling Nan
			       */
	}

	if ((EXPONENT(n) == MAXEXP) && (HIFRACTION(n) || LOFRACTION(n)) ) {
		setoserror(EDOM);
		return (x*n); /* return quiet NaN - raise exception
			       * if either x or n is a signalling Nan
			       */
	}
#endif

	if (x == 0.0)
		return x;

	if ((n >= (double)INT_MAX) || (n <= (double)INT_MIN)) {
	/* over or underflow */

		setoserror(ERANGE);

		if ( n < 0.0 )
			return 0.0;  /* underflow */
		else {
#ifdef NOTDEF
			if (_lib_version == c_issue_4)
				return(x > 0.0 ? HUGE : -HUGE);
			else
#endif
				return(x > 0.0 ? HUGE_VAL : -HUGE_VAL);
		}
	}

	return(ldexp(x, (int)n));
}
