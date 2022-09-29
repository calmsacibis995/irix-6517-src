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
 * Module: neg_sd.c
 * $Revision: 1.3 $
 * $Date: 1995/11/01 23:46:10 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/neg_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for neg.s and neg.d instructions
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

/* negate rs */

void
_neg_sd(int32_t precision, int64_t rs)
{
uint32_t rssign;
int32_t	rsexp;
uint64_t rsfrac;

#ifdef _FPTRACE

	fp_trace1_sd( precision, rs, NEG_SD );
#endif

	/* break rs into sign, exponent, and fraction bits */

	rsexp = (rs >> EXP_SHIFT[precision]);
	rsexp &= EXP_MASK[precision];

	rssign = (rs >> SIGN_SHIFT[precision]);
	rssign &= 1;

	rsfrac = (rs & FRAC_MASK[precision]);

	/* take care of NaNs first */

	if ( screen_nan_sd(precision, rs, rsexp, rsfrac) )
		return;

	rs ^= SIGNBIT[precision];

	fpstore_sd( precision, rs );

	store_fpcsr();

	return;
}

