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

/*
 *		I n d x W B I n v _ D ( )
 *
 * IndxWBInv_D- Writes back the data and invalidates (n) primary cache
 * data locations, if the cache block contains the specified address.
 *
 * Registers Used:
 *	a0 - address to invalidate
 *	a1 - number of cache lines to invalidate
 *	a2 - Cache line size
 *	t1 - Data cache linesize
 *	
 */
LEAF(IndxWBInv_D)
	.set	noreorder

1:
	cache	Index_Writeback_Inv_D, 0x0(a0)
	addiu	a1, -1
	bne	a1, zero, 1b
	addu	a0, a2

	j	ra
	nop

	.set	reorder
END(IndxWBInv_D)
