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
 * Module: madd_sd.c
 * $Revision: 1.3 $
 * $Date: 1996/05/18 00:36:51 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/madd_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for madd.s and madd.d instructions
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

void
_madd_sd( precision, rs, rt, rr )
int32_t	precision;
int64_t	rs, rt, rr;
{
int32_t	rsexp;
uint32_t rssign;
uint64_t rsfrac;
int32_t	rtexp;
uint32_t rtsign;
uint64_t rtfrac;
int32_t	rrexp;
uint32_t rrsign;
uint64_t rrfrac;

#ifdef _FPTRACE

	fp_trace3_sd( precision, rs, rt, rr, MADD_SD );
#endif

#ifndef FUSED_MADD

	SET_SOFTFP_FLAGS(GET_SOFTFP_FLAGS() & ~STORE_ON_ERROR);
#endif

	/* break rs, rt, and rr into sign, exponent, and fraction bits */

	if ( breakout_and_test3_sd(precision, &rs, &rssign, &rsexp, &rsfrac,
				&rt, &rtsign, &rtexp, &rtfrac,
				&rr, &rrsign, &rrexp, &rrfrac)
	   )
		return;

	madd(precision, rssign, rsexp, rsfrac, rtsign, rtexp, rtfrac,
		 rrsign, rrexp, rrfrac);

	return;
}

