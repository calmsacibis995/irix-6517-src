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
 * Module: div_sd.c
 * $Revision: 1.4 $
 * $Date: 1996/05/18 00:36:27 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/div_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for div.s and div.d instructions
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.4 $"

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#endif

void
_div_sd(int32_t precision, int64_t rs, int64_t rt)
{
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;
uint32_t rtsign;
int32_t	rtexp;
uint64_t rtfrac;
int32_t	shift_count;
int32_t	i;
uint32_t qsign;
int32_t	qexp;
uint64_t qfrac;
uint64_t guard_bits;
int64_t	result;

#ifdef _FPTRACE

	fp_trace2_sd( precision, rs, rt, DIV_SD );
#endif

	/* break rs and rt into sign, exponent, and fraction bits */

	if ( breakout_and_test2_sd(precision, &rs, &rssign, &rsexp, &rsfrac,
				&rt, &rtsign, &rtexp, &rtfrac)
	   )
		return;

	/* take care of infinities */

	if ( (rsexp == EXP_INF[precision]) && (rtexp == EXP_INF[precision]) )
	{
		/* both rs and rt are +/- inf	*/

		SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);

		/* post signal if invalid trap is enabled */

		if ( GET_LOCAL_CSR() & INVALID_ENABLE )
		{
			post_sig(SIGFPE);

			store_fpcsr();

			return;
		}

		/* store a quiet NaN */

		fpstore_sd( precision, QUIETNAN[precision] );

		store_fpcsr();

		return;
	}

	if ( rsexp == EXP_INF[precision] )
	{
		/* rs is +/- inf, but rt is not	*/

		result = ((((int64_t)rtsign) << SIGN_SHIFT[precision]) ^ rs);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	if ( rtexp == EXP_INF[precision] )
	{
		/* rt is +/- inf, but rs is not	*/

		result = (((int64_t)(rssign ^ rtsign)) << SIGN_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	/* take care of zeroes */

	if ( (rtexp == 0) && (rtfrac == 0ull) )
	{
		/* rt is a (signed) zero */

		if ( (rsexp == 0) && (rsfrac == 0ull) )
		{
			/* rs is zero, set invalid bit in fp csr */

			SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);

			/* post signal if invalid trap is enabled */
	
			if ( GET_LOCAL_CSR() & INVALID_ENABLE )
			{
				post_sig(SIGFPE);
	
				store_fpcsr();

				return;
			}
	
			/* store a quiet NaN */
	
			fpstore_sd( precision, QUIETNAN[precision] );
	
			store_fpcsr();

			return;
		}
		else
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() | DIVIDE0_EXC);

			/* post signal if divide by zero trap is enabled */
	
			if ( GET_LOCAL_CSR() & DIVIDE0_ENABLE )
			{
				post_sig(SIGFPE);
	
				store_fpcsr();

				return;
			}
	
			/* return a properly signed infinity */
	
			result = (INFINITY[precision] | (((int64_t)(rssign ^ rtsign)) << SIGN_SHIFT[precision]));
			fpstore_sd( precision, result );

			store_fpcsr();

			return;
		}
	}

	if ( (rsexp == 0) && (rsfrac == 0ull) )
	{
		/* rs is a (signed) zero; rt is finite, non-zero */

		result = (((int64_t)(rssign ^ rtsign)) << SIGN_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	/* at this point, both rs and rt are non-zero, finite numbers */

	/* normalize rs and rt so that bit 52 of each fraction is set */

	if ( rsexp == 0 )
	{
		shift_count = renorm[precision]( &rsfrac );
		rsexp = EXP_MIN[precision] - shift_count;
	}
	else
	{
		rsexp -= EXP_BIAS[precision];	/* subtract exponent bias */
		rsfrac |= IMP_1BIT[precision];	/* set implied one bit */
	}

	if ( rtexp == 0 )
	{
		shift_count = renorm[precision]( &rtfrac );
		rtexp = EXP_MIN[precision] - shift_count;
	}
	else
	{
		rtexp -= EXP_BIAS[precision];	/* subtract exponent bias */
		rtfrac |= IMP_1BIT[precision];	/* set implied one bit */
	}

	qsign = (rssign ^ rtsign);

	qexp = rsexp - rtexp;

	if ( rsfrac < rtfrac )
	{
		/* we need to shift the dividend to ensure enough bits
		 * in quotient
		 */

		rsfrac <<= 1;
		qexp--;
	}

	/* the standard division algorithm using repeated subtraction */

	qfrac = 0ull;

	for ( i=0; i<=MANTWIDTH[precision]; i++ )
	{
		qfrac <<= 1;

		if ( rtfrac <= rsfrac )
		{
			qfrac |= 1ull;
			rsfrac -= rtfrac;
		}

		rsfrac <<= 1;
	}

	guard_bits = 0ull;

	if ( rtfrac <= rsfrac )
	{
		guard_bits = GUARD_BIT;
		rsfrac -= rtfrac;
	}

	if ( rsfrac != 0ull )
		guard_bits |= STICKY_BIT;

	if ( precision == SINGLE )
	{
		qfrac <<= (DEXP_SHIFT - SEXP_SHIFT);
		qfrac |= (guard_bits >> (64 - (DEXP_SHIFT - SEXP_SHIFT)));
		guard_bits <<= (DEXP_SHIFT - SEXP_SHIFT);
	}

	round[precision](qsign, qexp, qfrac, guard_bits);

	return;
}

