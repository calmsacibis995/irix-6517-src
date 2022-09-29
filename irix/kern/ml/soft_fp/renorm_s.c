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
 * Module: renorm_s.c
 * $Revision: 1.2 $
 * $Date: 1995/11/01 23:46:33 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/renorm_s.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	utility routine which normalizes a single mantissa
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
0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
};

	/* shift denormal fraction so that bit 23 is set, returning
	 * the shift count; assumes at least one bit among bits 23-0
	 * is non-zero
	 */

int32_t
renorm_s( p_val )
uint64_t *p_val;
{
int32_t	count;
int32_t	index;

	if ( ((*p_val) & DLOW24_MASK) == 0ull )
	{
		/* error; just return 0 */

		return ( 0 );
	}

	if ( (*p_val) & SIMP_1BIT )
		return ( 0 );

	count = 0;

	while ( ((*p_val) & 0xff0000ull) == 0ull )
	{
		*p_val <<= 8;
		count += 8;
	}

	if ( ((*p_val) & 0xf00000ll) == 0 )
	{
		*p_val <<= 4;
		count += 4;
	}

	index = ((*p_val >> 20) & 0xf);

	*p_val <<= shift_count[index];
	count += shift_count[index];

	return ( count );
}

