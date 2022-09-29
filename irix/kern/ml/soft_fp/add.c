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
 * Module: add.c
 * $Revision: 1.5 $
 * $Date: 1996/05/18 00:35:48 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/add.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	subroutine shared by add_sd and sub_sd routines
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

/* common code shared by add_sd and sub_sd; NaNs have already been screened */

void
add(precision, rssign, rsexp, rsfrac, rtsign, rtexp, rtfrac)
int32_t precision;
uint32_t rssign;
int32_t rsexp;
uint64_t rsfrac;
uint32_t rtsign;
int32_t rtexp;
uint64_t rtfrac;
{
int32_t	shift_count;
uint32_t sumsign;
int32_t sumexp;
uint64_t sfrac[2], tfrac[2];
uint64_t guard_bits;
uint32_t carry;
int64_t result;

	/* We need to compute 55 bits of the sum:  sumfrac + guard-bit +
	 * sticky-bit, so that we can round the sum properly.
	 * NaNs have already been screened.
	 */

	/* take care of infinities */

	if ( (rsexp == EXP_INF[precision]) && (rtexp == EXP_INF[precision]) )
	{
		/* rs and rt are both +/- infinity */

		if ( rssign == rtsign )
		{
			result = (((int64_t)rssign) << SIGN_SHIFT[precision]);
			result |= (((int64_t)rsexp) << EXP_SHIFT[precision]);

			fpstore_sd( precision, result );

			store_fpcsr();

			return;
		}

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
		/* rs is infinite; rt is finite */

		result = (((int64_t)rssign) << SIGN_SHIFT[precision]);
		result |= (((int64_t)rsexp) << EXP_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	if ( rtexp == EXP_INF[precision] )
	{
		/* rt is infinite; rs is finite */

		result = (((int64_t)rtsign) << SIGN_SHIFT[precision]);
		result |= (((int64_t)rtexp) << EXP_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	/* take care of zeroes */

	if ( ((rsexp == 0) && (rsfrac == 0ull)) && ((rtexp == 0) && (rtfrac == 0ull)) )
	{
		/* both rs and rt are (signed) zeroes */

		if ( GET_FP_CSR_RM() == CSR_RM_RMI )
		{
			/* rounding mode is round to minus infinity */

			result = (((int64_t)(rssign | rtsign)) << SIGN_SHIFT[precision]);
		}
		else
			result = (((int64_t)(rssign & rtsign)) << SIGN_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	if ( (rsexp == 0) && (rsfrac == 0ull) )
	{
		/* rs is zero and rt is finite, non-zero.  Return rt. */

		result = (((int64_t)rtsign) << SIGN_SHIFT[precision]);
		result |= (((int64_t)rtexp) << EXP_SHIFT[precision]);
		result |= rtfrac;

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	if ( (rtexp == 0) && (rtfrac == 0ull) )
	{
		/* rt is zero and rs is finite, non-zero.  Return rs. */

		result = (((int64_t)rssign) << SIGN_SHIFT[precision]);
		result |= (((int64_t)rsexp) << EXP_SHIFT[precision]);
		result |= rsfrac;

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	/* check if rs == -rt */

	if ( rssign != rtsign )
	{
		if ( (rsexp == rtexp) && (rsfrac == rtfrac) )
		{
			/* rs = -rt */

			if ( GET_FP_CSR_RM() == CSR_RM_RMI )
			{
				/* rounding mode is round to minus infinity */
	
				result = ( ((int64_t)(rssign | rtsign)) << SIGN_SHIFT[precision] );
			}
			else
			{
				result = ( ((int64_t)(rssign & rtsign)) << SIGN_SHIFT[precision] );
			}

			fpstore_sd( precision, result );

			store_fpcsr();
			
			return;
		}
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

	if ( precision == SINGLE )
	{
		rsfrac <<= (DEXP_SHIFT - SEXP_SHIFT);
		rtfrac <<= (DEXP_SHIFT - SEXP_SHIFT);
	}

	/* in extreme cases, the smaller operand only affects the sticky
	 * bits, so we can simply readjust the exponent of the smaller term
	 */

	if ( rtexp < rsexp - 55 )
		rtexp = rsexp - 55;
	
	if ( rsexp < rtexp - 55 )
		rsexp = rtexp - 55;

	/* copy the larger of |rs| and |rt| to sfrac and the
	 * smaller to tfrac
	 */

	if ( (rsexp > rtexp) ||
	     ((rsexp == rtexp) && (rsfrac >= rtfrac))
	   )
	{
		sfrac[0] = rsfrac;
		tfrac[0] = rtfrac;
		shift_count = rsexp - rtexp;

		sumsign = rssign;
		sumexp = rsexp;
	}
	else
	{
		sfrac[0] = rtfrac;
		tfrac[0] = rsfrac;
		shift_count = rtexp - rsexp;

		sumsign = rtsign;
		sumexp = rtexp;
	}

	sfrac[1] = tfrac[1] = 0ull;

	/* shift the smaller to line up the binary points */

	if ( shift_count != 0 )
	{
		tfrac[1] = (tfrac[0] << (64 - shift_count));
		tfrac[0] >>= shift_count;
	}

	if ( rssign != rtsign )
	{
		/* form two's complement of tfrac */

		carry = 0;

		if ( tfrac[1] == 0ull )
			carry = 1;

		tfrac[0] = ~tfrac[0] + carry;

		tfrac[1] = ~tfrac[1] + 1;
	}

	/* just add up sfrac and tfrac; note that sfrac[1] == 0 */

	tfrac[0] += sfrac[0];

	/* sum of rsfrac and rtfrac is now in tfrac	*/

	/* normalize tfrac so that the most significant bit
	 * is in bit 52 of tfrac[0]
	 */

	if ( tfrac[0] == 0ull )
	{
		tfrac[0] = tfrac[1];
		tfrac[1] = 0ull;

		sumexp -= 64;
	}

	if ( tfrac[0] & DHIGH11_MASK )
	{
		tfrac[1] >>= 11;
		tfrac[1] |= (tfrac[0] << 53);

		tfrac[0] >>= 11;

		sumexp += 11;
	}

	shift_count = renorm_d( &tfrac[0] );

	if ( shift_count != 0 )
	{
		tfrac[0] |= (tfrac[1] >> (64 - shift_count));

		tfrac[1] <<= shift_count;

		sumexp -= shift_count;
	}

	guard_bits = tfrac[1];

	round[precision](sumsign, sumexp, tfrac[0], guard_bits);
}

