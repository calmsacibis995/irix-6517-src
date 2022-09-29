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
 *		F i l l _ I c a c h e ( )
 *
 * Fill_Icache - Fill the Primary Icache from the specifed location
 *
 * Registers Used:
 *	a0 - address to fill from
 *	a1 - number of cache lines to fill
 *	a2 - icache line size
 *	t0 - Icache linesize
 *	t1 - mask
 *	t2 - temp register
 *	
 */
LEAF(Fill_Icache)
	.set	noreorder

	move	t0, a2 
	li	t1, 0x1fffffff
	and	a0, t1
	or	a0, 0x80000000

	
1:
	cache	Fill_I,0(a0) 
	addu	a0,t0			# increment by line size
	
	addi	a1, -1
	bne	a1, zero, 1b
	nop

	j	ra
	nop

	.set	reorder
END(Fill_Icache)
