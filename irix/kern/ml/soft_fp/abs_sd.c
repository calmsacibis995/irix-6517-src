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
 * Module: abs_sd.c
 * $Revision: 1.4 $
 * $Date: 1995/11/01 23:43:49 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/abs_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for abs.s and abs.d instructions
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
_abs_sd(int32_t precision, int64_t rs)
{
int32_t	rsexp;
uint64_t rsfrac;

#ifdef _FPTRACE

	fp_trace1_sd( precision, rs, ABS_SD );
#endif

	/* break rs into exponent and fraction bits */

	rsexp = (rs >> EXP_SHIFT[precision]);
	rsexp &= EXP_MASK[precision];

	rsfrac = (rs & FRAC_MASK[precision]);

	/* take care of NaNs first */

	if ( screen_nan_sd(precision, rs, rsexp, rsfrac) )
		return;

	rs &= ~SIGNBIT[precision];

	fpstore_sd( precision, rs );

	store_fpcsr();

	return;
}

