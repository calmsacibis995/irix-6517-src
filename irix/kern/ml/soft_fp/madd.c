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
 * Module: madd.c
 * $Revision: 1.7 $
 * $Date: 1996/05/18 00:36:48 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/madd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	subroutine shared by routines madd_sd, msub_sd, nmadd_sd,
 *		and nmsub_sd
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.7 $"

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#endif

void
madd( precision, rssign, rsexp, rsfrac, rtsign, rtexp, rtfrac,
	  rrsign, rrexp, rrfrac )
int32_t precision;
uint32_t rssign;
int32_t rsexp;
uint64_t rsfrac;
uint32_t rtsign;
int32_t rtexp;
uint64_t rtfrac;
uint32_t rrsign;
int32_t rrexp;
uint64_t rrfrac;
{
int64_t result;
int32_t	shift_count;
uint32_t prdsign;
int32_t prdexp;
uint64_t prdfrac[2];

#ifdef FUSED_MADD
uint32_t sumsign;
int32_t sumexp;
uint64_t guard_bits;
uint32_t carry;
uint32_t carry_in, carry_out;
uint64_t sfrac[3], tfrac[3];
#endif

	/* take care of infinities */

	if ( (rsexp == EXP_INF[precision]) && (rtexp == EXP_INF[precision]) && (rrexp == EXP_INF[precision]) )
	{
		if ( (rssign ^ rtsign) == rrsign )
		{
			result = (((int64_t)rrsign) << SIGN_SHIFT[precision]);
			result |= (((int64_t)rrexp) << EXP_SHIFT[precision]);

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

	if ( (rsexp == EXP_INF[precision]) && (rtexp == EXP_INF[precision]) )
	{
		/* rs and rt are infinite; rr is finite */

		result = (((int64_t)(rssign ^ rtsign)) << SIGN_SHIFT[precision]);
		result |= (((int64_t)rsexp) << EXP_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

	/* interchange rs and rt if rs is finite and rt is infinite */

	if ( (rsexp != EXP_INF[precision]) && (rtexp == EXP_INF[precision]) )
	{
		rssign ^= rtsign;
		rtsign ^= rssign;
		rssign ^= rtsign;

		rsexp ^= rtexp;
		rtexp ^= rsexp;
		rsexp ^= rtexp;

		rsfrac ^= rtfrac;
		rtfrac ^= rsfrac;
		rsfrac ^= rtfrac;
	}

	if ( (rsexp == EXP_INF[precision]) && (rrexp == EXP_INF[precision]) )
	{
		/* rs and rr are infinite; rt is finite */

		if ( ((rtexp == 0) && (rtfrac == 0ll)) ||
		     ((rssign ^ rtsign) != rrsign)
		   )

		{
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
			result = (((int64_t)rrsign) << SIGN_SHIFT[precision]);
			result |= (((int64_t)rrexp) << EXP_SHIFT[precision]);

			fpstore_sd( precision, result );

			store_fpcsr();

			return;
		}
	}

	if ( rsexp == EXP_INF[precision] )
	{
		
		if ( (rtexp == 0) && (rtfrac == 0ll) )
		{
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

		result = (((int64_t)(rssign ^ rtsign)) << SIGN_SHIFT[precision]);
		result |= (((int64_t)rsexp) << EXP_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}

#ifdef FUSED_MADD

	if ( rrexp == EXP_INF[precision] )
	{
		result = (((int64_t)rrsign) << SIGN_SHIFT[precision]);
		result |= (((int64_t)rrexp) << EXP_SHIFT[precision]);

		fpstore_sd( precision, result );

		store_fpcsr();

		return;
	}
#endif

	if ( ((rsexp == 0) && (rsfrac == 0ull)) ||
	     ((rtexp == 0) && (rtfrac == 0ull))
	   )
	{
		if ( (rrexp == 0) && (rrfrac == 0ull) )
		{
			if ( GET_FP_CSR_RM() == CSR_RM_RMI )
			{
				/* rounding mode is round to minus infinity */
	
				result = (((int64_t)((rssign ^ rtsign) | rrsign)) << SIGN_SHIFT[precision]);
			}
			else
			{
				result = (((int64_t)((rssign ^ rtsign) & rrsign)) << SIGN_SHIFT[precision]);
			}

			fpstore_sd( precision, result );

			store_fpcsr();

			return;
		}
		else
		{
			result = (((int64_t)rrsign) << SIGN_SHIFT[precision]);
			result |= (((int64_t)rrexp) << EXP_SHIFT[precision]);
			result |= rrfrac;

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

	prdsign = (rssign ^ rtsign);
	prdexp = rsexp + rtexp;
	prdfrac[0] = mul64(rsfrac, rtfrac, &prdfrac[1]);
	shift_count = renorm_d( &prdfrac[0] );

	/* the shift count is either 11 or 12, since the product of
	 * rsfrac and rtfrac contains either 105 or 106 bits */

	prdfrac[0] |= (prdfrac[1] >> (64 - shift_count));
	prdfrac[1] <<= shift_count;

	if ( shift_count == 11 )
	{
		/* adjust exponent of product, since the product of the
		 * fractions is >= 2.
		 */

		prdexp++;
	}

#ifndef FUSED_MADD

	result = maddrnd[precision](prdsign, prdexp, prdfrac[0], prdfrac[1]);

	/* break result into sign, exponent, and fraction bits */

	breakout_sd( precision, &result, &rssign, &rsexp, &rsfrac );

	add(precision, rssign, rsexp, rsfrac, rrsign, rrexp, rrfrac);

	return;

#else	/* FUSED_MADD */

	if ( (rrexp == 0) && (rrfrac == 0ull) )
	{
		/* rr = +/-0	*/

		round[precision](prdsign, prdexp, prdfrac[0], prdfrac[1]);
	
		return;
	}

	/* normalize rr so that bit 52 of the fraction is set */

	if ( rrexp == 0 )
	{
		shift_count = renorm[precision]( &rrfrac );
		rrexp = EXP_MIN[precision] - shift_count;
	}
	else
	{
		rrexp -= EXP_BIAS[precision];	/* subtract exponent bias */
		rrfrac |= IMP_1BIT[precision];	/* set implied one bit */
	}

	if ( precision == SINGLE )
	{
		rrfrac <<= (DEXP_SHIFT - SEXP_SHIFT);
	}

	if ( (prdsign != rrsign) && (prdexp == rrexp) &&
	     (prdfrac[0] == rrfrac) && (prdfrac[1] == 0ull)
	   )
	{
		/* prd = -rr */

		if ( GET_FP_CSR_RM() == CSR_RM_RMI )
		{
			/* rounding mode is round to minus infinity */

			result = ( ((int64_t)(prdsign | rrsign)) << DSIGN_SHIFT );
		}
		else
		{
			result = ( ((int64_t)(prdsign & rrsign)) << DSIGN_SHIFT );
		}

		fpstore_sd( precision, result );

		store_fpcsr();
		
		return;
	}

	/* in extreme cases, the smaller operand only affects the sticky
	 * bits, so we can simply readjust the exponent of the smaller term
	 */

	if ( rrexp < prdexp - 108 )
		rrexp = prdexp - 108;
	
	if ( prdexp < rrexp - 55 )
		prdexp = rrexp - 55;

	/* copy the larger of |prd| and |rr| to sfrac	*/

	if ( (prdexp > rrexp) ||
	     ((prdexp == rrexp) && (prdfrac[0] >= rrfrac))
	   )
	{
		sfrac[0] = prdfrac[0];
		sfrac[1] = prdfrac[1];
		sfrac[2] = 0ull;

		sumsign = prdsign;
		sumexp = prdexp;

		tfrac[0] = rrfrac;
		tfrac[1] = 0ull;
		tfrac[2] = 0ull;

		shift_count = prdexp - rrexp;
	}
	else
	{
		sfrac[0] = rrfrac;
		sfrac[1] = 0ull;
		sfrac[2] = 0ull;

		sumsign = rrsign;
		sumexp = rrexp;

		tfrac[0] = prdfrac[0];
		tfrac[1] = prdfrac[1];
		tfrac[2] = 0ull;

		shift_count = rrexp - prdexp;
	}

	/*
	 * line up the binary points of sfrac and tfrac by shifting
	 * the smaller to the right
	 */

	if ( shift_count >= 64 )
	{
		tfrac[2] = tfrac[1];
		tfrac[1] = tfrac[0];
		tfrac[0] = 0ull;

		shift_count -= 64;
	}

	if ( shift_count > 0 )
	{
		tfrac[2] >>= shift_count;
		tfrac[2] |= (tfrac[1] << (64 - shift_count));
		tfrac[1] >>= shift_count;
		tfrac[1] |= (tfrac[0] << (64 - shift_count));
		tfrac[0] >>= shift_count;
	}

	if ( prdsign != rrsign )
	{
		/* form two's complement of tfrac */

		carry = 0ull;

		if ( (tfrac[1] == 0ull) && (tfrac[2] == 0ull) )
			carry = 1;

		tfrac[0] = ~tfrac[0] + carry;

		carry = 0;

		if ( tfrac[2] == 0ull )
			carry = 1;

		tfrac[1] = ~tfrac[1] + carry;

		tfrac[2] = ~tfrac[2] + 1ull;
	}

	/* just add up sfrac and tfrac */

	carry_out = 0;

	if ( sfrac[2] > ~tfrac[2] )
		carry_out = 1;

	tfrac[2] += sfrac[2];

	
	carry_in = carry_out;

	carry_out = 0;

	if ( (tfrac[1] == ULONGLONG_MAX) && (carry_in != 0ull) )
		carry_out = 1;

	tfrac[1] += carry_in;

	if ( sfrac[1] > ~tfrac[1] )
		carry_out += 1;

	tfrac[1] += sfrac[1];

	
	carry_in = carry_out;

	tfrac[0] += carry_in;
	tfrac[0] += sfrac[0];

	/* normalize tfrac so that the most significant bit
	 * is in bit 52 of tfrac[0]
	 */

	if ( tfrac[0] & DHIGH11_MASK )
	{
		tfrac[2] >>= 11;
		tfrac[2] |= (tfrac[1] << 53);

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
		tfrac[1] |= (tfrac[2] >> (64 - shift_count));
	
		tfrac[2] <<= shift_count;

		sumexp -= shift_count;
	}

	/* now set up guard and sticky bits prior to rounding */

	guard_bits = tfrac[1];

	if ( tfrac[2] != 0 )
		guard_bits |= STICKY_BIT;
	
	round[precision](sumsign, sumexp, tfrac[0], guard_bits);

	return;

#endif	/* FUSED_MADD */
}

