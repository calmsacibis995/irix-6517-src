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
 * Module: comp_sd.c
 * $Revision: 1.4 $
 * $Date: 1996/05/18 00:35:53 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/comp_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for single and double precision compare instructions
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
_comp_sd( precision, rs, rt, cond, cc )
int32_t precision;
int64_t rs, rt;
int32_t cond;
int32_t	cc;
{
uint32_t rssign;
uint64_t rsfrac;
int32_t rsexp;
uint32_t rtsign;
uint64_t rtfrac;
int32_t rtexp;

#ifdef _FPTRACE

	fp_trace2_sd( precision, rs, rt, COMP_SD );
#endif

	/* break rs and rt into sign, exponent, and fraction bits */

	rsexp = (rs >> EXP_SHIFT[precision]);
	rsexp &= EXP_MASK[precision];

	rssign = (rs >> SIGN_SHIFT[precision]);
	rssign &= 1;

	rsfrac = (rs & FRAC_MASK[precision]);

	rtexp = (rt >> EXP_SHIFT[precision]);
	rtexp &= EXP_MASK[precision];

	rtsign = (rt >> SIGN_SHIFT[precision]);
	rtsign &= 1;

	rtfrac = (rt & FRAC_MASK[precision]);

	/* take care of hole in fp csr */

	if ( cc != 0ll )
		cc++;

	/* take care of NaNs first */

	if ( ((rsexp == EXP_NAN[precision]) && (rsfrac != 0ull)) ||
	     ((rtexp == EXP_NAN[precision]) && (rtfrac != 0ull))
	   )
	{
		/* rs or rt is a NaN */

		if ( cond & COND_UN_MASK )
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() | (CSR_CBITSET << cc));
		}
		else
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() & ~(CSR_CBITSET << cc));
		}

		/* set the invalid exception if either operand is a
		 * signaling NaN or if the high bit of the predicate
		 * is set
		 */

		if ( ((rsexp == EXP_NAN[precision]) && (rsfrac & SNANBIT_MASK[precision])) ||
		     ((rtexp == EXP_NAN[precision]) && (rtfrac & SNANBIT_MASK[precision])) ||
		     (cond & COND_IN_MASK)
		   )
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);
	
			/* post signal if invalid trap is enabled */
	
			if ( GET_LOCAL_CSR() & INVALID_ENABLE )
			{
				post_sig(SIGFPE);
			}
		}
	
		store_fpcsr();

		return;
	}

	/* Set up to do the comparison by changing negative
	 * floating point values to two's complement notation
	 * so that integer comparisons can be used.
	 */

	if ( precision == SINGLE )
	{
		rs <<= 32;
		rt <<= 32;
	}

	if ( rssign )
	{
		rs &= ~DSIGNBIT;
		rs = -rs;
	}

	if ( rtsign )
	{
		rt &= ~DSIGNBIT;
		rt = -rt;
	}

	/* Now compare the two operands */

	if ( rs < rt )
	{
		if ( cond & COND_LT_MASK )
			SET_LOCAL_CSR(GET_LOCAL_CSR() | (CSR_CBITSET << cc));
		else
			SET_LOCAL_CSR(GET_LOCAL_CSR() & ~(CSR_CBITSET << cc));
	}
	else if ( rs == rt )
	{
		if ( cond & COND_EQ_MASK )
			SET_LOCAL_CSR(GET_LOCAL_CSR() | (CSR_CBITSET << cc));
		else
			SET_LOCAL_CSR(GET_LOCAL_CSR() & ~(CSR_CBITSET << cc));
	}
	else
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() & ~(CSR_CBITSET << cc));
	}

	store_fpcsr();

	return;
}

