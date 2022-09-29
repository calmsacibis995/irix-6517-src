/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Id: ilogb.c,v 1.2 1997/06/25 21:34:10 vegas Exp $"

#pragma weak ilogb = _ilogb

#include <sgidefs.h>
#include "synonyms.h"
#include <limits.h>

#define _IEEE 1
#include "fpparts.h"
#undef  _IEEE

/*
 * The 'ilogb()' function is equivalent to: (int)logb(x);
 *
 * logb returns the unbiased exponent of its floating-point argument as
 *      a double-precision floating point value
 *
 * ilogb returns the exponent part of its floating-point argument as an
 *       integer value.
 *
 * ilogb(NaN)                   returns INT_MIN
 * ilogb(0)                     returns INT_MIN
 * ilogb(infinity)              returns INT_MAX
 * ilogb(-infinity)             returns INT_MAX
 */

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

__int32_t
ilogb(x)
double x;
{
	__int32_t iexp;		/* exponents of number */
	_dval value;		/* the value manipulated */

	value = * ((_dval *) &x);
	iexp = EXPONENT(value);

	if (iexp == MAXEXP) { /* infinity  or NaN */
		if (HIFRACTION(value) || LOFRACTION(value))
			return((__int32_t)INT_MIN);	/* NaN */
		else
			return((__int32_t)INT_MAX);	/* infinity */
	}
	if (iexp == 0) {
		if ((HIFRACTION(value) == 0) && (LOFRACTION(value) == 0))
			return((__int32_t)INT_MIN);	/* true zero */
		else			
			return((__int32_t)INT_MAX);	/* -infinity */
	}
	return((__int32_t)(iexp - 1023)); /* subtract bias */
}
#else

__int32_t
ilogb(x)
double x;
{
	__int32_t iexp;		/* exponents of number */
	_dval value;		/* the value manipulated */
	__uint64_t frac;	/* fraction of number */

	value = * ((_dval *) &x);
	iexp = EXPONENT(value);
	frac= (* ((__uint64_t *) &value)) << 12 >> 12;

	if (iexp == MAXEXP) { /* infinity  or NaN */
		if (frac)
			return((__int32_t)INT_MIN);	/* NaN */
		else
			return((__int32_t)INT_MAX);	/* infinity */
	}
	if (iexp == 0) {
		if (!frac)
			return((__int32_t)INT_MIN);	/* true zero */
		else
			return((__int32_t)INT_MAX);	/* -infinity */
	}
	return((__int32_t)(iexp - 1023)); /* subtract bias */
}
#endif
