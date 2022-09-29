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
 * Module: renorm_d.c
 * $Revision: 1.2 $
 * $Date: 1995/11/01 23:46:26 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/renorm_d.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	utility routine which normalizes a double mantissa
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

static	int8_t	shift_count[16] =
{
0, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
};

	/* shift denormal fraction so that bit 52 is set, returning
	 * the shift count; assumes at least one bit among bits 52-0
	 * is non-zero
	 */

int32_t
renorm_d( p_val )
uint64_t *p_val;
{
int32_t	count;
int32_t	index;

	if ( ((*p_val) & DLOW53_MASK) == 0ull )
	{
		/* error; just return 0 */

		return ( 0 );
	}

	if ( (*p_val) & DIMP_1BIT )
		return ( 0 );

	count = 0;

	while ( ((*p_val) & 0xfff0000000000ull) == 0ull )
	{
		*p_val <<= 12;
		count += 12;
	}

	while ( ((*p_val) & 0xf000000000000ull) == 0ull )
	{
		*p_val <<= 4;
		count += 4;
	}

	index = ((*p_val >> 48) & 0xf);

	*p_val <<= shift_count[index];
	count += shift_count[index];

	return ( count );
}

