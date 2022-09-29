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
 * Module: sub_sd.c
 * $Revision: 1.2 $
 * $Date: 1995/11/01 23:47:22 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/sub_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for sub.s and sub.d instructions
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.2 $"

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#endif

void
_sub_sd(int32_t precision, int64_t rs, int64_t rt)
{
uint32_t rssign;
int32_t rsexp;
uint64_t rsfrac;
uint32_t rtsign;
int32_t rtexp;
uint64_t rtfrac;

#ifdef _FPTRACE

	fp_trace2_sd( precision, rs, rt, SUB_SD );
#endif

	/* break rs and rt into sign, exponent, and fraction bits */

	if ( breakout_and_test2_sd(precision, &rs, &rssign, &rsexp, &rsfrac,
				&rt, &rtsign, &rtexp, &rtfrac)
	   )
		return;

	add(precision, rssign, rsexp, rsfrac, rtsign ^ 1, rtexp, rtfrac);

	return;
}

