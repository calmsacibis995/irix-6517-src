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
 * Module: sqrt_sd.c
 * $Revision: 1.4 $
 * $Date: 1996/05/18 00:37:23 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/sqrt_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for sqrt.d instruction
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
_sqrt_sd(int32_t precision, int64_t rs)
{
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;
int32_t	shift_count;
int32_t	i;
uint64_t ans, rem;
int64_t	trialsub;
uint64_t guard_bits;

#ifdef _FPTRACE

	fp_trace1_sd( precision, rs, SQRT_SD );
#endif

	/* break rs and rt into sign, exponent, and fraction bits */

	if ( breakout_and_test_sd(precision, &rs, &rssign, &rsexp, &rsfrac) )
		return;

	/* take care of infinities */

	if ( rsexp == EXP_INF[precision] )
	{
		if ( rssign == 0 )
		{
			fpstore_sd( precision, rs );

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

	/* take care of zeroes */

	if ( (rsexp == 0) && (rsfrac == 0ull) )
	{
		/* rs is a (signed) zero */

		fpstore_sd( precision, rs );

		store_fpcsr();

		return;
	}

	/* take care of negative numbers */

	if ( rssign == 1 )
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

	/* normalize rs so that the hidden bit of the fraction is set */

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

	rsfrac <<= (rsexp & 1);
	rsexp >>= 1;

	ans = 1ll;
	rem = 0ll;

	rem = (rsfrac >> EXP_SHIFT[precision]);
	rsfrac <<= (64 - EXP_SHIFT[precision]);

	for ( i=0; i<=EXP_SHIFT[precision]+1; i++ )
	{
		trialsub = rem - ans;

		ans <<= 1;
		ans &= 0xfffffffffffffffcull;
		ans |= 1ll;

		if ( trialsub < 0ll )
		{
			rem <<= 2;
		}
		else
		{
			ans |= 4ll;
			rem = (trialsub << 2);
		}

		rem |= ((rsfrac >> 62) & 0x3);
		rsfrac <<= 2;
	}

	ans >>= 2;

	guard_bits = (ans << 63);
	ans >>= 1;

	if ( rem != 0ull )
	{
		guard_bits |= STICKY_BIT;
	}

	if ( precision == SINGLE )
	{
		ans <<= (DEXP_SHIFT - SEXP_SHIFT);
		ans |= (guard_bits >> (64 - (DEXP_SHIFT - SEXP_SHIFT)));
		guard_bits <<= (DEXP_SHIFT - SEXP_SHIFT);
	}

	round[precision](rssign, rsexp, ans, guard_bits);

	return;
}

