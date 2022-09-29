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
#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/fpclass.c,v 1.1 1992/10/30 09:58:18 gb Exp $"
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

/*	IEEE recommended functions */

#ifdef __STDC__
	#pragma weak fpclass = _fpclass
#endif
#include "synonyms.h"
#include <values.h>
#include "fpparts.h"

#define P754_NOFAULT 1		/* avoid generating extra code */
#include <ieeefp.h>

/* FPCLASS(X)
 * fpclass(x) returns the floating point class x belongs to 
 */

fpclass_t	fpclass(x)
double	x;
{	
	register int	sign, exp;

	exp = EXPONENT(x);
	sign = SIGNBIT(x);
	if (exp == 0) { /* de-normal or zero */
		if (HIFRACTION(x) || LOFRACTION(x)) /* de-normal */
			return(sign ? FP_NDENORM : FP_PDENORM);
		else
			return(sign ? FP_NZERO : FP_PZERO);
	}
	if (exp == MAXEXP) { /* infinity or NaN */
		if ((HIFRACTION(x) == 0) && (LOFRACTION(x) == 0)) /* infinity*/
			return(sign ? FP_NINF : FP_PINF);
		else
			if (QNANBIT(x))
			/* hi-bit of mantissa set - quiet nan */
				return(FP_QNAN);
			else	return(FP_SNAN);
	}
	/* if we reach here we have non-zero normalized number */
	return(sign ? FP_NNORM : FP_PNORM);
}
