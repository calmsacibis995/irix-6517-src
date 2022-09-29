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
 * Module: cvtsd_w.c
 * $Revision: 1.3 $
 * $Date: 1995/11/02 23:47:20 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/cvtsd_w.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for cvt.s.w and cvt.d.w instructions
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.3 $"

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#endif

	/* convert a 32-bit integer to a single or double precision value */

	/* rs is assumed to be a valid 32-bit value, not sign extended */

void
_cvtsd_w( precision, rs )
int32_t precision;
int64_t	rs;
{
int32_t	shift_count;
uint32_t resultsign;
int32_t	resultexp;
uint64_t guard_bits;

#ifdef _FPTRACE

	fp_trace1_w( precision, rs, CVTSD_W );
#endif

	if ( rs == 0ll )
	{
		fpstore_sd( precision, 0ll );

		store_fpcsr();

		return;
	}

	resultsign = 0;

	/* sign extend rs */

	rs <<= 32;
	rs >>= 32;

	if ( rs < 0ll )
	{
		rs = -rs;
		resultsign = 1;
	}

	shift_count = renorm_d( (uint64_t *)&rs );
	resultexp = 52 - shift_count;
	guard_bits = 0ull;

	round[precision](resultsign, resultexp, rs, guard_bits);

	return;
}

