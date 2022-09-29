/**************************************************************************
 *									  *
 *		 Copyright (C) 1997-1998, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * This file has all the beast specific cacheops.
 */
#include "ml/ml.h"
	.set	noreorder
/*
 * set_trapbase(__psunsigned_t)
 */
LEAF(get_trapbase)
	
	j	ra
	 DMFC0(v0, C0_TRAPBASE)

	END	(get_trapbase)

/*
 * set_trapbase(__psunsigned_t)
 */
LEAF(set_trapbase)

	j	ra
  	 DMTC0(a0, C0_TRAPBASE)

	END	(set_trapbase)

	.set	reorder


#if defined (SABLE)
LEAF(print_trap)
	.word	0x40ffffff
	jr	ra
	END(print_trap)
		
#endif

LEAF(disable_ints)
	.set	noreorder
	DMFC0(a1, C0_SR)
	or	a1, a0
	xor	a1, a0
	DMTC0(a1, C0_SR)
	.set	reorder
	jr	ra
	END(disable_ints)			
