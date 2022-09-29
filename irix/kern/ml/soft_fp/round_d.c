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
 * Module: round_d.c
 * $Revision: 1.8 $
 * $Date: 1996/07/18 21:31:02 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/round_d.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	contains subroutines round_l, round_d, and maddrnd_d
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.8 $"

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#include <ksys/exception.h>
#endif

/*
	round to nearest


sign	round	guard	sticky	   add to round
	 bit	 bit	 bit	      bit
  0	  0	  0	  0	  	0
  0	  0	  0	  1	  	0
  0	  0	  1	  0	  	0
  0	  0	  1	  1	  	1
  0	  1	  0	  0	  	0
  0	  1	  0	  1	  	0
  0	  1	  1	  0	  	1
  0	  1	  1	  1	  	1
  1	  0	  0	  0	  	0
  1	  0	  0	  1	  	0
  1	  0	  1	  0	  	0
  1	  0	  1	  1	  	1
  1	  1	  0	  0	  	0
  1	  1	  0	  1	  	0
  1	  1	  1	  0	  	1
  1	  1	  1	  1	  	1


	round to zero


sign	round	guard	sticky	   add to round
	 bit	 bit	 bit	      bit
  0	  0	  0	  0	  	0
  0	  0	  0	  1	  	0
  0	  0	  1	  0	  	0
  0	  0	  1	  1	  	0
  0	  1	  0	  0	  	0
  0	  1	  0	  1	  	0
  0	  1	  1	  0	  	0
  0	  1	  1	  1	  	0
  1	  0	  0	  0	  	0
  1	  0	  0	  1	  	0
  1	  0	  1	  0	  	0
  1	  0	  1	  1	  	0
  1	  1	  0	  0	  	0
  1	  1	  0	  1	  	0
  1	  1	  1	  0	  	0
  1	  1	  1	  1	  	0


	round to +infinity


sign	round	guard	sticky	   add to round
	 bit	 bit	 bit	      bit
  0	  0	  0	  0	  	0
  0	  0	  0	  1	  	1
  0	  0	  1	  0	  	1
  0	  0	  1	  1	  	1
  0	  1	  0	  0	  	0
  0	  1	  0	  1	  	1
  0	  1	  1	  0	  	1
  0	  1	  1	  1	  	1
  1	  0	  0	  0	  	0
  1	  0	  0	  1	  	0
  1	  0	  1	  0	  	0
  1	  0	  1	  1	  	0
  1	  1	  0	  0	  	0
  1	  1	  0	  1	  	0
  1	  1	  1	  0	  	0
  1	  1	  1	  1	  	0


	round to -infinity


sign	round	guard	sticky	   add to round
	 bit	 bit	 bit	      bit
  0	  0	  0	  0	  	0
  0	  0	  0	  1	  	0
  0	  0	  1	  0	  	0
  0	  0	  1	  1	  	0
  0	  1	  0	  0	  	0
  0	  1	  0	  1	  	0
  0	  1	  1	  0	  	0
  0	  1	  1	  1	  	0
  1	  0	  0	  0	  	0
  1	  0	  0	  1	  	1
  1	  0	  1	  0	  	1
  1	  0	  1	  1	  	1
  1	  1	  0	  0	  	0
  1	  1	  0	  1	  	1
  1	  1	  1	  0	  	1
  1	  1	  1	  1	  	1

*/

/* overflow table is indexed by rounding mode and sign bit.
 */

static int64_t	overflow_result[4][2] =
{
DINFINITY,	MDINFINITY,
MAXDBL,		MMAXDBL,
DINFINITY,	MMAXDBL,
MAXDBL,		MDINFINITY,
};

/* ====================================================================
 *
 * FunctionName		round_l
 *
 * Description		rounds a 64 bit integer value
 *
 * ====================================================================
 */

/* This routine rounds a 64 bit integer value, using the IEEE 754
 * rounding rules.
 * Guard and sticky bits are bits 63 and 62-0, respectively, of guard_bits.
 * Sticky bits may or may not be consolidated to a single bit (bit 62)
 * prior to input to this routine.
 * (see restrictions below, however)
 */

void
round_l(sign, rs, guard_bits)
uint32_t sign;
int64_t	rs;
uint64_t guard_bits;
{
int32_t	rm, rb, gb, sb;
int32_t	carry;

	/* consolidate sticky bits into a single bit */

	if ( (guard_bits & STICKY_MASK) != 0 )
	{
		guard_bits &= ~STICKY_MASK;
		guard_bits |= STICKY_BIT;
	}

	rm = GET_FP_CSR_RM();		/* rounding mode */
	rb = rs & 1ll;			/* round bit */
	gb = (guard_bits >> 63);	/* guard bit */
	sb = (guard_bits >> 62) & 1ll;	/* sticky bit */

	carry = round_bit[rm][sign][rb][gb][sb];

	rs += carry;

	/* in this implementation, values which will not fit into a
	 * long long without overflowing have already been screened.
	 */

	/* negate result if necessary */

	if ( sign )
		rs = -rs;

	if ( guard_bits )
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

		/* post signal if inexact trap is enabled */

		if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
		{
			post_sig(SIGFPE);
		}
	}

	FPSTORE_D( rs );

	store_fpcsr();

	return;
}

/* ====================================================================
 *
 * FunctionName		round_d
 *
 * Description		rounds a double precision value
 *
 * ====================================================================
 */

/* This routine rounds a double precision value, using the IEEE 754
 * rounding rules; rsfrac is the (nonzero) fractional value, normalized so
 * that bit 52 is set, and the unbiased exponent (rsexp) adjusted accordingly.
 * Guard and sticky bits are bits 63 and 62-0, respectively, of guard_bits.
 * Sticky bits may or may not be consolidated to a single bit (bit 62)
 * prior to input to this routine.
 * Note that in the case of a non-fused MADD instruction, several cause bits
 * may be set for the same instruction.  In this case, whenever one or more
 * of the exceptions invalid, overflow, or underflow is trapped, no result is
 * delivered to the trap handler.  This is done so that the trap handler can
 * be sure to have all the operands available.
 */

void
round_d(rssign, rsexp, rsfrac, guard_bits)
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;
uint64_t guard_bits;
{
int32_t	rm, rb, gb, sb;
int32_t	sb1, sb2;
int32_t	underflow;
int32_t	i;
uint32_t rtsign;
int32_t	rtexp;
uint64_t rtfrac;
int32_t	shift_count;
uint32_t carry;
int32_t	tininess;
int32_t	inexact;
uint64_t guard_bits2;
int64_t	result;
int64_t	biased_result;

	/* consolidate sticky bits into a single bit */

	if ( (guard_bits & STICKY_MASK) != 0 )
	{
		guard_bits &= ~STICKY_MASK;
		guard_bits |= STICKY_BIT;
	}

	rm = GET_FP_CSR_RM();		/* rounding mode */

	rtsign = rssign;
	rtexp = rsexp;
	rtfrac = rsfrac;
	guard_bits2 = guard_bits;
	tininess = FALSE;
	inexact = FALSE;

	if ( rsexp < (DEXP_MIN - 53) )
	{
		guard_bits = STICKY_BIT; /* rs is non-zero */
		rsfrac = 0;
		rsexp = DEXP_MIN - 1;
	}
	else if ( rsexp < DEXP_MIN )
	{
		shift_count = DEXP_MIN - rsexp;
		guard_bits >>= shift_count;
		guard_bits |= (rsfrac << (64 - shift_count));
		rsfrac >>= shift_count;
		rsexp = DEXP_MIN - 1;
	}

	/* Treatment of denormals is based on mips3/tfp hardware when denormal
	 * results are to be flushed; the easiest way to do this is with a table.
	 */

	if ( (GET_SOFTFP_FLAGS() & FLUSH_OUTPUT_DENORMS) && (rsexp < DEXP_MIN) )
	{
		underflow = FALSE;

		if ( rsfrac < DMAXDENORMAL )
		{
			/* fetch result from table; the result is always
			 * +/-0 or +/-minnormal depending on the rounding
			 * mode.
			 */

			i = underflow_tab[rm][rssign][0][0][0];

			if ( i == 0 )
				rsfrac = 0ull;
			else
				rsfrac = DMINNORMAL;

			underflow = TRUE;
		}
		else
		{
			/* result comes from a table here, too.  However, whether
			 * the result underflows depends both on the size of the
			 * denormal as well as the rounding mode.
			 */

			gb = (guard_bits >> 63);	/* guard bit */
			sb1 = (guard_bits >> 62) & 1;	/* sticky bit 1 */
			sb2 = 0;			/* sticky bit 2 */

			if ( (guard_bits << 2) != 0ull )
				sb2 = 1;

			i = underflow_tab[rm][rssign][gb][sb1][sb2];

			if ( i == 0 )
				rsfrac = 0ull;
			else
				rsfrac = DMINNORMAL;

			if ( (gb == 0) || ((gb == 1) && (sb1 == 0) && (sb2 == 0)) )
				underflow = TRUE;
			else if ( rsfrac == 0ull )
				underflow = TRUE;
		}

		result = (rsfrac | (((int64_t)rssign) << DSIGN_SHIFT));

		if ( underflow == TRUE )
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() | UNDERFLOW_EXC);
	
			/* post signal if underflow trap is enabled */
	
			if ( GET_LOCAL_CSR() & UNDERFLOW_ENABLE )
			{
				post_sig(SIGFPE);

				/* deliver biased result to trap handler,
				 * per IEEE 754 spec.
				 */
	
				rtexp += DEXP_OU_ADJ;
				rm = GET_FP_CSR_RM();	/* rounding mode */
				rb = rtfrac & 1;		/* round bit */
				gb = (guard_bits2 >> 63);	/* guard bit */
				sb = (guard_bits2 >> 62) & 1;	/* sticky bit */
			
				carry = round_bit[rm][rssign][rb][gb][sb];
			
				rtfrac += carry;
			
				/* adjust fraction and exponent if necessary */
			
				if ( rtfrac & (DIMP_1BIT << 1) )
				{
					rtfrac >>= 1;
					rtexp += 1;
				}
	
				rtfrac &= ~DIMP_1BIT;	/* clear implied 1 bit */
				rtexp += DEXP_BIAS;
				biased_result = (rtfrac | (((int64_t)rtexp) << DEXP_SHIFT));
				biased_result |= (((int64_t)rtsign) << DSIGN_SHIFT);
	
				/* if necessary, set inexact exception bit */

				if ( guard_bits2 )
					SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

				if ( GET_SOFTFP_FLAGS() & STORE_ON_ERROR )
				{
					FPSTORE_D( biased_result );

					PCB_STORE( result );
				}
	
				/* check for inexactness */
	
				if ( guard_bits )
					SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);
	
				store_fpcsr();
	
				return;
			}
		}

		SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

		/* post signal if inexact trap is enabled */

		if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
		{
			post_sig(SIGFPE);
		}

		if ( (GET_SOFTFP_FLAGS() & STORE_ON_ERROR) ||
		     ((GET_SOFTFP_FLAGS() & FP_ERROR) == 0) )
		{
			FPSTORE_D( result );
		}

		store_fpcsr();

		return;
	}

	if ( guard_bits )
		inexact = TRUE;

	/* consolidate sticky bits into a single bit */

	if ( (guard_bits & STICKY_MASK) != 0ull )
	{
		guard_bits &= ~STICKY_MASK;
		guard_bits |= STICKY_BIT;
	}

	rb = rsfrac & 1;		/* round bit */
	gb = (guard_bits >> 63);	/* guard bit */
	sb = (guard_bits >> 62) & 1;	/* sticky bit */

	carry = round_bit[rm][rssign][rb][gb][sb];

	rsfrac += carry;

	/* adjust fraction and exponent if necessary */

	if ( (rsexp == (DEXP_MIN - 1)) && (rsfrac & DIMP_1BIT) )
	{
		rsexp++;
	}
	else if ( (rsexp >= DEXP_MIN) && (rsfrac & (DIMP_1BIT << 1)) )
	{
		rsfrac >>= 1;
		rsexp++;
	}

	if ( rsexp < DEXP_MIN )
		tininess = TRUE;

	if ( rsexp > DEXP_MAX )
	{
		/* overflow has occurred */

		result = overflow_result[rm][rssign];

		SET_LOCAL_CSR(GET_LOCAL_CSR() | OVERFLOW_EXC);

		/* post signal if overflow trap is enabled */

		if ( GET_LOCAL_CSR() & OVERFLOW_ENABLE )
		{
			post_sig(SIGFPE);

			/* deliver biased result to trap handler, per IEEE 754
			 * spec.
			 */

			rsexp -= DEXP_OU_ADJ;
			rsfrac &= ~DIMP_1BIT;	/* clear implied 1 bit */
			rsexp += DEXP_BIAS;
			biased_result = (rsfrac | (((int64_t)rsexp) << DEXP_SHIFT));
			biased_result |= (((int64_t)rssign) << DSIGN_SHIFT);

			if ( GET_SOFTFP_FLAGS() & STORE_ON_ERROR )
			{
				FPSTORE_D( biased_result );

				PCB_STORE( result );
			}

			/* check for inexactness */

			if ( guard_bits )
				SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

			store_fpcsr();

			return;
		}

		SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

		/* post signal if inexact trap is enabled */

		if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
		{
			post_sig(SIGFPE);
		}

		if ( (GET_SOFTFP_FLAGS() & STORE_ON_ERROR) ||
		     ((GET_SOFTFP_FLAGS() & FP_ERROR) == 0) )
		{
			FPSTORE_D( result );
		}

		store_fpcsr();

		return;
	}

	if ( rsexp >= DEXP_MIN )
	{
		rsexp += DEXP_BIAS;

		rsfrac &= ~DIMP_1BIT;	/* clear implied 1 bit */

		result = (rsfrac | (((int64_t)rsexp) << DEXP_SHIFT));

		result |= (((int64_t)rssign) << DSIGN_SHIFT);
	}
	else
	{
		result = rsfrac;

		result |= (((int64_t)rssign) << DSIGN_SHIFT);
	}

	/* check for underflow */

	if ( GET_LOCAL_CSR() & UNDERFLOW_ENABLE )
	{
		if ( tininess == TRUE )
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() | UNDERFLOW_EXC);

			post_sig(SIGFPE);

			rtexp += DEXP_OU_ADJ;
			rm = GET_FP_CSR_RM();		/* rounding mode */
			rb = rtfrac & 1;		/* round bit */
			gb = (guard_bits2 >> 63);	/* guard bit */
			sb = (guard_bits2 >> 62) & 1;	/* sticky bit */
		
			carry = round_bit[rm][rssign][rb][gb][sb];
		
			rtfrac += carry;
		
			/* adjust fraction and exponent if necessary */
		
			if ( rtfrac & (DIMP_1BIT << 1) )
			{
				rtfrac >>= 1;
				rtexp += 1;
			}

			rtfrac &= ~DIMP_1BIT;	/* clear implied 1 bit */
			rtexp += DEXP_BIAS;
			biased_result = (rtfrac | (((int64_t)rtexp) << DEXP_SHIFT));
			biased_result |= (((int64_t)rtsign) << DSIGN_SHIFT);

			if ( guard_bits2 )
				SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

			if ( GET_SOFTFP_FLAGS() & STORE_ON_ERROR )
			{
				FPSTORE_D( biased_result );

				PCB_STORE( result );
			}

			store_fpcsr();

			return;
		}
	}
	else	/* underflow trap not enabled */
	{
		if ( (tininess == TRUE) && (inexact == TRUE) )
			SET_LOCAL_CSR(GET_LOCAL_CSR() | UNDERFLOW_EXC);
	}

	if ( inexact == TRUE )
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

		if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
		{
			post_sig(SIGFPE);
		}

	}

	if ( (GET_SOFTFP_FLAGS() & STORE_ON_ERROR) ||
	     ((GET_SOFTFP_FLAGS() & FP_ERROR) == 0) )
	{
		FPSTORE_D( result );
	}

	store_fpcsr();

	return;
}

#if !defined(FUSED_MADD) && defined(MIPS4_ISA)

/* ====================================================================
 *
 * FunctionName		maddrnd_d
 *
 * Description		rounds double precision product from madd
 *			instruction
 *
 * ====================================================================
 */

/* This routine rounds the double precision product from a madd instruction,
 * using the IEEE 754 rounding rules; rsfrac is the (nonzero) fractional 
 * value, normalized so that bit 52 is set, and the unbiased exponent (rsexp)
 * adjusted accordingly.  Guard and sticky bits are bits 63 and 62-0, 
 * respectively, of guard_bits.  Sticky bits may or may not be consolidated 
 * to a single bit (bit 62) prior to input to this routine.  Note that
 * this routine returns the rounded value instead of storing it.
 * Read notes on MADD instruction in header for routine round_d.
 */

int64_t
maddrnd_d(rssign, rsexp, rsfrac, guard_bits)
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;
uint64_t guard_bits;
{
int32_t	rm, rb, gb, sb;
int32_t	sb1, sb2;
int32_t	underflow;
int32_t	i;
int32_t	shift_count;
uint32_t carry;
int32_t	tininess;
int32_t	inexact;
int64_t	result;

	/* consolidate sticky bits into a single bit */

	if ( (guard_bits & STICKY_MASK) != 0 )
	{
		guard_bits &= ~STICKY_MASK;
		guard_bits |= STICKY_BIT;
	}

	rm = GET_FP_CSR_RM();		/* rounding mode */

	tininess = FALSE;
	inexact = FALSE;

	if ( rsexp < (DEXP_MIN - 53) )
	{
		guard_bits = STICKY_BIT; /* rs is non-zero */
		rsfrac = 0;
		rsexp = DEXP_MIN - 1;
	}
	else if ( rsexp < DEXP_MIN )
	{
		shift_count = DEXP_MIN - rsexp;
		guard_bits >>= shift_count;
		guard_bits |= (rsfrac << (64 - shift_count));
		rsfrac >>= shift_count;
		rsexp = DEXP_MIN - 1;
	}

	/* Treatment of denormals is based on mips3/tfp hardware when denormal
	 * results are to be flushed; the easiest way to do this is with a table.
	 */

	if ( (GET_SOFTFP_FLAGS() & FLUSH_OUTPUT_DENORMS) && (rsexp < DEXP_MIN) )
	{
		underflow = FALSE;

		if ( rsfrac < DMAXDENORMAL )
		{
			/* fetch result from table; the result is always
			 * +/-0 or +/-minnormal depending on the rounding
			 * mode.
			 */

			i = underflow_tab[rm][rssign][0][0][0];

			if ( i == 0 )
				rsfrac = 0ull;
			else
				rsfrac = DMINNORMAL;

			underflow = TRUE;
		}
		else
		{
			/* result comes from a table here, too.  However, whether
			 * the result underflows depends both on the size of the
			 * denormal as well as the rounding mode.
			 */

			gb = (guard_bits >> 63);	/* guard bit */
			sb1 = (guard_bits >> 62) & 1;	/* sticky bit 1 */
			sb2 = 0;			/* sticky bit 2 */

			if ( (guard_bits << 2) != 0ull )
				sb2 = 1;

			i = underflow_tab[rm][rssign][gb][sb1][sb2];

			if ( i == 0 )
				rsfrac = 0ull;
			else
				rsfrac = DMINNORMAL;

			if ( (gb == 0) || ((gb == 1) && (sb1 == 0) && (sb2 == 0)) )
				underflow = TRUE;
			else if ( rsfrac == 0ull )
				underflow = TRUE;
		}

		SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

		/* post signal if inexact trap is enabled */

		if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
		{
			post_sig(SIGFPE);
		}

		if ( underflow == TRUE )
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() | UNDERFLOW_EXC);
	
			/* post signal if underflow trap is enabled */
	
			if ( GET_LOCAL_CSR() & UNDERFLOW_ENABLE )
			{
				post_sig(SIGFPE);

				SET_SOFTFP_FLAGS(GET_SOFTFP_FLAGS() | FP_ERROR);
			}
		}

		result = (rsfrac | (((int64_t)rssign) << DSIGN_SHIFT));

		return ( result );
	}

	if ( guard_bits )
		inexact = TRUE;

	/* consolidate sticky bits into a single bit */

	if ( (guard_bits & STICKY_MASK) != 0ull )
	{
		guard_bits &= ~STICKY_MASK;
		guard_bits |= STICKY_BIT;
	}

	rb = rsfrac & 1;		/* round bit */
	gb = (guard_bits >> 63);	/* guard bit */
	sb = (guard_bits >> 62) & 1;	/* sticky bit */

	carry = round_bit[rm][rssign][rb][gb][sb];

	rsfrac += carry;

	/* adjust fraction and exponent if necessary */

	if ( (rsexp == (DEXP_MIN - 1)) && (rsfrac & DIMP_1BIT) )
	{
		rsexp++;
	}
	else if ( (rsexp >= DEXP_MIN) && (rsfrac & (DIMP_1BIT << 1)) )
	{
		rsfrac >>= 1;
		rsexp++;
	}

	if ( rsexp < DEXP_MIN )
		tininess = TRUE;

	if ( rsexp > DEXP_MAX )
	{
		/* overflow has occurred */

		result = overflow_result[rm][rssign];

		SET_LOCAL_CSR(GET_LOCAL_CSR() | OVERFLOW_EXC);

		/* post signal if overflow trap is enabled */

		if ( GET_LOCAL_CSR() & OVERFLOW_ENABLE )
		{
			post_sig(SIGFPE);

			SET_SOFTFP_FLAGS(GET_SOFTFP_FLAGS() | FP_ERROR);

			/* check for inexactness */

			if ( guard_bits )
				SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

			return ( result );
		}

		SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

		/* post signal if inexact trap is enabled */

		if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
		{
			post_sig(SIGFPE);
		}

		return ( result );
	}

	/* check for underflow */

	if ( GET_LOCAL_CSR() & UNDERFLOW_ENABLE )
	{
		if ( tininess == TRUE )
		{
			SET_LOCAL_CSR(GET_LOCAL_CSR() | UNDERFLOW_EXC);

			post_sig(SIGFPE);

			SET_SOFTFP_FLAGS(GET_SOFTFP_FLAGS() | FP_ERROR);
		}
	}
	else	/* underflow trap not enabled */
	{
		if ( (tininess == TRUE) && (inexact == TRUE) )
			SET_LOCAL_CSR(GET_LOCAL_CSR() | UNDERFLOW_EXC);
	}

	if ( inexact == TRUE )
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

		if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
		{
			post_sig(SIGFPE);
		}

	}

	if ( rsexp >= DEXP_MIN )
	{
		rsexp += DEXP_BIAS;

		rsfrac &= ~DIMP_1BIT;	/* clear implied 1 bit */

		result = (rsfrac | (((int64_t)rsexp) << DEXP_SHIFT));

		result |= (((int64_t)rssign) << DSIGN_SHIFT);
	}
	else
	{
		result = rsfrac;

		result |= (((int64_t)rssign) << DSIGN_SHIFT);
	}

	return ( result );
}
#endif /* !FUSED_MADD && MIPS4_ISA */

