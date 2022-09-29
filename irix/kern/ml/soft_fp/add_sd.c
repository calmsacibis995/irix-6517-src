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
 * Module: add_sd.c
 * $Revision: 1.2 $
 * $Date: 1995/11/01 23:44:07 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/add_sd.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	emulation code for add.s and add.d instructions
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
_add_sd(int32_t precision, int64_t rs, int64_t rt)
{
uint32_t rssign;
uint64_t rsfrac;
int32_t rsexp;
uint32_t rtsign;
uint64_t rtfrac;
int32_t rtexp;

#ifdef _FPTRACE

	fp_trace2_sd( precision, rs, rt, ADD_SD );
#endif

	/* break rs and rt into sign, exponent, and fraction bits */

	if ( breakout_and_test2_sd(precision, &rs, &rssign, &rsexp, &rsfrac,
				&rt, &rtsign, &rtexp, &rtfrac)
	   )
		return;

	add(precision, rssign, rsexp, rsfrac, rtsign, rtexp, rtfrac);

	return;
}

