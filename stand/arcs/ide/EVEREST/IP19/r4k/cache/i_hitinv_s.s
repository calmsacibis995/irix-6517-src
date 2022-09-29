/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.2 $"



#include "asm.h"
#include "regdef.h"
#include "cpu.h"
/* #include "mach_ops.h"*/
/* #include "cache_ops.h"*/

/*
 * 		H i t I n v _ I ( )
 *
 * HitInv_I -  If the cache block contains the specified address, mark the
 * the cache block invalid.
 *
 * Registers Used:
 *	a0 - address to invalidate
 *	a1 - number of cache lines to invalidate
 *	a2 - cache line size
 *	t1 - icache linesize
 *	
 */
LEAF(HitInv_I)
	.set	noreorder

1:
	cache	Hit_Invalidate_I, 0x0(a0)
	addiu	a1, -1
	bne	a1, zero, 1b
	addu	a0, a2				# a0= address of next line

	j	ra
	nop

	.set	reorder
END(HitInv_I)
