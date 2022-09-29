/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/modff.c	1.7"
/*LINTLIBRARY*/
/*
 * modff(value, iptr) returns the signed fractional part of value
 * and stores the integer part indirectly through iptr.
 *
 */

#ifdef __STDC__
	#pragma weak modff = _modff
#endif
#include <sys/types.h>
#include "synonyms.h"
#include <values.h>
#if _IEEE /* machines with IEEE floating point only */
#include <signal.h>
#include <unistd.h>
#ifdef __sgi
#include "fpparts.h"
#define IsFNANorINF(X)  (((_fval *)&(X))->fparts.exp == 0xff)
#define IsFNAN(X)	(((_fval *)&(X))->fparts.fract != 0)
#define QNAN_VALUE	0x7F810000
#else
typedef union {
	float f;
	unsigned long word;
} _fval;
#define EXPMASK	0x7f800000
#define FRACTMASK 0x7fffff
#define FWORD(X)	(((_fval *)&(X))->word)
#endif

#endif

float
#ifdef __STDC__
modff(float value, register float *iptr)
#else
modff(value, iptr)
float value;
register float *iptr;
#endif
{
	register float absvalue;

#if _IEEE
#ifndef __sgi
/* dont raise exceptions on NaNs to be consistent with the 
   double-precision version
*/
	/* raise exception on NaN - 3B only */
	if (((FWORD(value) & EXPMASK) == EXPMASK) &&
		(FWORD(value) & FRACTMASK))
		(void)kill(getpid(), SIGFPE);
#endif
#endif
	if (IsFNANorINF(value))  {
		/* for NaN return a quiet NaN
		   as both the integral and fractional parts */
		if (IsFNAN(value)) {
			_fval __qnan;

			/* Takes one LUI to load QNAN constant */
			__qnan.fword = QNAN_VALUE;
			*iptr = __qnan.f;
			return (__qnan.f);
		}
		else {
		/* for INF return the value
		   as both the integral and fractional parts */
			*iptr = value;
			return(value); 
		}
	}
	else 
	if ((absvalue = (value >= (float)0.0) ? value : -value) >= FMAXPOWTWO) 
		*iptr = value; /* it must be an integer */
	else {
		*iptr = absvalue + FMAXPOWTWO; /* shift fraction off right */
		*iptr -= FMAXPOWTWO; /* shift back without fraction */
		while (*iptr > absvalue) /* above arithmetic might round */
			*iptr -= (float)1.0; /* test again just to be sure */
		if (value < (float)0.0)
			*iptr = -*iptr;
	}
	return (value - *iptr); /* signed fractional part */
}
