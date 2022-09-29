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
 * Module: cvtl_sd.c
 * $Revision: 1.4 $
 * $Date: 1996/05/18 00:35:59 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/cvtl_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for cvt.l.s and cvt.l.d instructions
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
_cvtl_sd( precision, rs )
int32_t precision;
int64_t	rs;
{
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;
int32_t	shift_count;
uint64_t guard_bits;

#ifdef _FPTRACE

	fp_trace1_sd( precision, rs, CVTL_SD );
#endif

	/* break rs into sign, exponent, and fraction bits */

	breakout_sd( precision, &rs, &rssign, &rsexp, &rsfrac );

	/* special case -2**63 */

	if ( rs == MTWOP63[precision] )
	{
		/* rs == -2**63 */

		FPSTORE_D( LONGLONG_MIN );

		store_fpcsr();

		return;
	}

	/* NaNs, infinities, and values >= 2**63 are invalid */

	if ( rsexp >= EXP_BIAS[precision] + 63 )
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);

		/* post signal if invalid trap is enabled */

		if ( GET_LOCAL_CSR() & INVALID_ENABLE )
		{
			post_sig(SIGFPE);

			store_fpcsr();

			return;
		}

		/* return largest positive result */

		FPSTORE_D( LONGLONG_MAX );

		store_fpcsr();

		return;
	}

	/* take care of zeroes */

	if ( (rsexp == 0) && (rsfrac == 0ull) )
	{
		/* rs is a (signed) zero */

		FPSTORE_D( 0x0ll );

		store_fpcsr();

		return;
	}

	/* rs is finite, non-zero, < 2**63 */

	/* normalize rs so that bit 52 is set */

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

	if ( precision == SINGLE )
	{
		rsfrac <<= (DEXP_SHIFT - SEXP_SHIFT);
	}

	if ( rsexp < -1 )
	{
		rsfrac = 0;
		guard_bits = STICKY_BIT;
	}
	else if ( rsexp == -1 )
	{
		guard_bits = (rsfrac << 11);
		rsfrac = 0;
	}
	else if ( rsexp < 52 )
	{
		shift_count = 52 - rsexp;
		guard_bits = (rsfrac << (64 - shift_count));
		rsfrac >>= shift_count;
	}
	else if ( rsexp == 52 )
	{
		guard_bits = 0;
	}
	else
	{
		shift_count = rsexp - 52;
		rsfrac <<= shift_count;
		guard_bits = 0;
	}

	round_l(rssign, rsfrac, guard_bits);

	return;
}

