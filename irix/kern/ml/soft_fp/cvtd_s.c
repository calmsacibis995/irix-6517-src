/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* ====================================================================
 * ====================================================================
 *
 * Module: cvtd_s.c
 * $Revision: 1.5 $
 * $Date: 1996/08/14 23:16:51 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/cvtd_s.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for cvt.d.s instruction
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.5 $"

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#endif

void
_cvtd_s( rs )
int64_t	rs;
{
uint32_t rssign;
int32_t rsexp;
uint64_t rsfrac;
int32_t	shift_count;
int64_t result;

#ifdef _FPTRACE

	fp_trace1_sd( SINGLE, rs, CVTD_S );
#endif

	/* break rs into sign, exponent, and fraction bits */

	breakout_sd(SINGLE, &rs, &rssign, &rsexp, &rsfrac);

	/* take care of NaNs first */

	if ( (rsexp == SEXP_NAN) && (rsfrac & SSNANBIT_MASK) )
	{
		/* rs is a signaling NaN */

		SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);

		/* post signal if invalid trap is enabled */

		if ( GET_LOCAL_CSR() & INVALID_ENABLE )
		{
			post_sig(SIGFPE);

			store_fpcsr();

			return;
		}

		/* store a quiet NaN */

		FPSTORE_D( DQUIETNAN );

		store_fpcsr();

		return;
	}

	if ( (rsexp == SEXP_NAN) && (rsfrac != 0ull) )
	{
		/* rs is a quiet NaN */

		result = (rsfrac << (DFRAC_BITS - SFRAC_BITS));

		result |= (((int64_t)DEXP_NAN) << DEXP_SHIFT);

		result |= (((int64_t)rssign) << DSIGN_SHIFT);

		FPSTORE_D( result );
		store_fpcsr();

		return;
	}

	/* take care of infinities */

	if ( rsexp == SEXP_INF )
	{
		result = (((int64_t)rssign) << DSIGN_SHIFT);
		result |= (((int64_t)DEXP_INF) << DEXP_SHIFT);

		FPSTORE_D( result );

		store_fpcsr();

		return;
	}

	/* take care of zeroes */

	if ( (rsexp == 0) && (rsfrac == 0ull) )
	{
		/* rs is a (signed) zero */

		result = (((int64_t)rssign) << DSIGN_SHIFT);

		FPSTORE_D( result );

		store_fpcsr();

		return;
	}

	/* at this point, rs is finite, non-zero */

	if ( rsexp == 0 )
	{
		shift_count = renorm_s( &rsfrac );
		rsexp = SEXP_MIN - shift_count;
		rsfrac &= ~SIMP_1BIT;	/* clear implied one bit */
	}
	else
	{
		rsexp -= SEXP_BIAS;	/* subtract exponent bias */
	}

	result = (rsfrac << (DFRAC_BITS - SFRAC_BITS));
	result |= (((int64_t)rssign) << DSIGN_SHIFT);
	result |= (((int64_t)(rsexp + DEXP_BIAS)) << DEXP_SHIFT);

	/* Note that underflow, overflow, and inexact exceptions are
	 * not possible here.
	 */

	FPSTORE_D( result );

	store_fpcsr();

	return;
}

