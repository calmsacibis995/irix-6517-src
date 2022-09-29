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
 * Module: mul_sd.c
 * $Revision: 1.5 $
 * $Date: 1996/05/18 00:36:57 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/mul_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for mul.s and mul.d instructions
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
_mul_sd(int32_t precision, int64_t rs, int64_t rt)
{
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;
uint32_t rtsign;
int32_t	rtexp;
uint64_t rtfrac;
int32_t	shift_count;
uint32_t prdsign;
int32_t	prdexp;
uint64_t prdfrac;
uint64_t guard_bits;
int64_t	result;

#ifdef _FPTRACE

	fp_trace2_sd( precision, rs, rt, MUL_SD );
#endif

	/* break rs and rt into sign, exponent, and fraction bits */

	if ( breakout_and_test2_sd(precision, &rs, &rssign, &rsexp, &rsfrac,
				&rt, &rtsign, &rtexp, &rtfrac)
	   )
		return;

	/* take care of infinities */

	if ( (rsexp == EXP_INF[precision]) && (rtexp == EXP_INF[precision]) )
	{
		result = ((((int64_t)rssign) << SIGN_SHIFT[precision]) ^ rt);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	if ( rsexp == EXP_INF[precision] )
	{
		/* rs is infinite; rt is finite */

		if ( (rtexp == 0) && (rtfrac == 0ull) )
		{
			/* rt is zero */

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
			result = ((((int64_t)rtsign) << SIGN_SHIFT[precision]) ^ rs);

			fpstore_sd( precision, result );

			store_fpcsr();

			return;
		}
	}

	if ( rtexp == EXP_INF[precision] )
	{
		/* rt is infinite; rs is finite */

		if ( (rsexp == 0) && (rsfrac == 0ull) )
		{
			/* rs is zero */

			SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);
	
			/* post signal if invalid trap is enabled */
	
			if ( GET_LOCAL_CSR() & INVALID_ENABLE )
			{
				post_sig(SIGFPE);

				store_fpcsr();

				return;
			}

			/* store a quiet NaN */

			fpstore_sd( precision,  QUIETNAN[precision] );

			store_fpcsr();

			return;
		}
		else
		{
			result = ((((int64_t)rssign) << SIGN_SHIFT[precision]) ^ rt);

			fpstore_sd( precision, result );

			store_fpcsr();

			return;
		}
	}

	/* take care of zeroes */

	if ( ((rsexp == 0) && (rsfrac == 0ull)) || ((rtexp == 0) && (rtfrac == 0ull)) )
	{
		/* rs or rt is a (signed) zero */

		result = (((int64_t)(rssign ^ rtsign)) << SIGN_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	/* at this point, both rs and rt are non-zero, finite numbers */

	/* normalize rs and rt so that the hidden bit of each fraction is set */

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

	if ( precision == SINGLE )
	{
		rsfrac <<= (DEXP_SHIFT - SEXP_SHIFT);
		rtfrac <<= (DEXP_SHIFT - SEXP_SHIFT);
	}

	prdsign = (rssign ^ rtsign);
	prdexp = rsexp + rtexp;
	prdfrac = mul64(rsfrac, rtfrac, &guard_bits);
	shift_count = renorm_d( &prdfrac );

	/* the shift count is either 11 or 12, since the product of
	 * rsfrac and rtfrac contains either 105 or 106 bits */

	prdfrac |= (guard_bits >> (64 - shift_count));
	guard_bits <<= shift_count;

	if ( shift_count == 11 )
	{
		/* adjust exponent of product, since the product of the
		 * fractions is >= 2.
		 */

		prdexp++;
	}

	round[precision](prdsign, prdexp, prdfrac, guard_bits);

	return;
}

