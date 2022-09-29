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
 * Module: qfpclass
 *
 * Description: quad precision version of fpclass
 *
 * ====================================================================
 * ====================================================================
 */

#ident	"$Id: qfpclass.c,v 1.4 1997/01/24 08:02:01 jwag Exp $"

#ifdef __STDC__
	#pragma weak fpclassl = _fpclassl
	#pragma weak qfpclass = _fpclassl
	#pragma weak _qfpclass = _fpclassl
#endif

#include "synonyms.h"
#include <values.h>
#include "fpparts.h"
#include <ieeefp.h>
#include <math.h>
#include "math_extern.h"
#include <inttypes.h>
#include "quad.h"

extern	fpclass_t	fpclass(double);

static const	du	twopm916 =
{0x06b00000,	0x00000000};

/* ====================================================================
 *
 * FunctionName: qfpclass
 *
 * Description: qfpclass(u) returns the floating point class u belongs to 
 *
 * ====================================================================
 */

fpclass_t
fpclassl(long double	u)
{	
ldquad	x;
int	sign, exp;

	x.ld = u;

	exp = EXPONENT(x.q.hi);

	sign = SIGNBIT(x.q.hi);

	SIGNBIT(x.q.hi) = 0;

	if (exp == 0) { /* de-normal or zero */

		if (HIFRACTION(x.q.hi) || LOFRACTION(x.q.hi)) /* de-normal */
			return(sign ? FP_NDENORM : FP_PDENORM);
		else
			return(sign ? FP_NZERO : FP_PZERO);
	}
	if (exp == MAXEXP) { /* infinity or NaN */

		if ((HIFRACTION(x.q.hi) == 0) && (LOFRACTION(x.q.hi) == 0)) /* infinity*/
			return(sign ? FP_NINF : FP_PINF);
		else
			if (QNANBIT(x.q.hi))
			/* hi-bit of mantissa set - quiet nan */
				return(FP_QNAN);
			else	return(FP_SNAN);
	}

	/* need to check that there are a full 107 bits in the arg */

	if ( exp < 0x06b ) /* de-normal */
		return(sign ? FP_NDENORM : FP_PDENORM);

	if ( (x.q.hi == twopm916.d) && (SIGNBIT(x.q.lo) != sign) ) {

		/* x < 2**-916; low part of x may be a denormal */

		return(sign ? FP_NDENORM : FP_PDENORM);
	}
	
	/* if we reach here we have non-zero normalized number */

	return(sign ? FP_NNORM : FP_PNORM);
}
