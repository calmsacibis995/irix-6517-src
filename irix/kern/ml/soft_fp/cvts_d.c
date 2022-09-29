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
 * Module: cvts_d.c
 * $Revision: 1.6 $
 * $Date: 1996/08/14 23:16:52 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/cvts_d.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for cvt.s.d instruction
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.6 $"

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#endif

void
_cvts_d( rs )
int64_t	rs;
{
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;
int32_t	shift_count;
uint64_t guard_bits;
int64_t result;

#ifdef _FPTRACE

	fp_trace1_sd( DOUBLE, rs, CVTS_D );
#endif

	/* break rs into sign, exponent, and fraction bits */

	breakout_sd(DOUBLE, &rs, &rssign, &rsexp, &rsfrac);

	/* take care of NaNs first */

	if ( (rsexp == DEXP_NAN) && (rsfrac & DSNANBIT_MASK) )
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

		FPSTORE_S( SQUIETNAN );

		store_fpcsr();

		return;
	}

	if ( (rsexp == DEXP_NAN) && (rsfrac != 0ull) )
	{
		/* rs is a quiet NaN */

		result = (rsfrac >>= (DFRAC_BITS - SFRAC_BITS));

		if ( result == 0ll )
		{
			result = SQUIETNAN;
		}
		else
		{
			result |= (((int64_t)SEXP_NAN) << SEXP_SHIFT);

			result |= (((int64_t)rssign) << SSIGN_SHIFT);
		}

		FPSTORE_S( result );
		store_fpcsr();

		return;
	}

	/* take care of infinities */

	if ( rsexp == DEXP_INF )
	{
		result = (((int64_t)rssign) << SSIGN_SHIFT);
		result |= (((int64_t)SEXP_INF) << SEXP_SHIFT);

		FPSTORE_S( result );

		store_fpcsr();

		return;
	}

	/* take care of zeroes */

	if ( (rsexp == 0) && (rsfrac == 0ull) )
	{
		/* rs is a (signed) zero */

		result = (((int64_t)rssign) << SSIGN_SHIFT);

		FPSTORE_S( result );

		store_fpcsr();

		return;
	}

	if ( rsexp == 0 )
	{
		shift_count = renorm_d( &rsfrac );
		rsexp = DEXP_MIN - shift_count;
	}
	else
	{
		rsexp -= DEXP_BIAS;	/* subtract exponent bias */
		rsfrac |= DIMP_1BIT;	/* set implied one bit */
	}

	guard_bits = 0;

	round_s(rssign, rsexp, rsfrac, guard_bits);

	return;
}

