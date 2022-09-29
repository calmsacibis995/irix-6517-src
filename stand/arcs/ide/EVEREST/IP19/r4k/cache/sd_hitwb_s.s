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
 *		H i t W B _ S D( )
 *
 *
 * HitWB_SD -  If the secondary cache block contains the specified address
 * and the cache state is dirty, write back the data and set the state to
 * clean exclusive or shared. If the primary data is more current than write
 * it back to memory, but the cache state remains unchanged.
 *
 * Registers Used:
 *	a0 - address to  check
 *	a1 - number of cache lines to check
 *	a2 - scache linesize
 *	
 */
LEAF(HitWB_SD)
	.set	noreorder

1:
	cache	Hit_Writeback_SD, 0x0(a0)
	addiu	a1, -1
	bne	a1, zero, 1b
	addu	a0, a2				# a0= address of next line

	j	ra
	nop

	.set	reorder
END(HitWB_SD)
