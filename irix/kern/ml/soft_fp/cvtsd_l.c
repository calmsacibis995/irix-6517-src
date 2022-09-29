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
 * Module: cvtsd_l.c
 * $Revision: 1.4 $
 * $Date: 1996/08/30 00:17:23 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/cvtsd_l.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for cvt.s.l and cvt.d.l instructions
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

/* converts a long long to a double using rounding mode */

static	int8_t	shift_count[16] =
{
0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
};

void
_cvtsd_l( precision, rs )
int32_t precision;
int64_t	rs;
{
uint32_t resultsign;
int32_t	resultexp;
int32_t	count;
int32_t	index;
uint64_t guard_bits;

#ifdef _FPTRACE

	fp_trace1_l( precision, rs, CVTSD_L );
#endif

	/* take care of zero first */

	if ( rs == 0ll )
	{
		fpstore_sd( precision, 0ll );

		store_fpcsr();

		return;
	}

	/* take care of max neg value */

	if ( rs == LONGLONG_MIN )
	{
		/* store -2**63	*/

		fpstore_sd( precision, MTWOP63[precision] );

		store_fpcsr();

		return;
	}

	resultsign = 0;

	if ( rs < 0ll )
	{
		rs = -rs;
		resultsign = 1;
	}

	if ( rs & DHIGH11_MASK )
	{
		/* rs contains more than 53 significant bits, so we have
		 * to do some shifting and rounding
		 */

		if ( rs & DHIGH4_MASK )
		{
			index = ((rs >> 60) & 0xf);
			count = (8 + shift_count[index]);
		}
		else if ( rs & DHIGH8_MASK )
		{
			index = ((rs >> 56) & 0xf);
			count = (4 + shift_count[index]);
		}
		else
		{
			index = ((rs >> 52) & 0xf);
			count = shift_count[index];
		}

		guard_bits = (rs << (64 - count));
		rs >>= count;

		resultexp = 52 + count;
	}
	else
	{
		count = renorm_d( (uint64_t *)&rs );
		resultexp = 52 - count;
		guard_bits = 0ull;
	}
		
	round[precision](resultsign, resultexp, rs, guard_bits);

	return;
}

