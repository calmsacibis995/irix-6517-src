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

#ident "$Revision: 1.1 $"



#include "asm.h"
#include "regdef.h"
#include "cpu.h"

	.globl scache_linesize
/*
 *		I n d x S t o r T a g _ S D ( )
 *
 * Write Secondary Data tag at location a1
 *
 * INPUTS
 *	a1 = Value to be written to taglo
 *	a0 = Cache line to write
 *
 */
LEAF(IndxStorTag_SD)
	.set	noreorder

	mtc0	a1,C0_TAGLO		/* Set tag low register */
	nop
1:
	cache	Index_Store_Tag_SD,0(a0) /* Write the tag location */
	j	ra			/* BDSLOT */
	nop

	.set	reorder
END(IndxStorTag_SD)

/*
 *		I n d x L o a d T a g _ S D ( )
 *
 * IndxLoadTag_SD - READ Secondary DATA TAG AT LOCATION a0
 *
 * INPUTS
 *	v0 = Tag data returned
 *	a0 = Cache line to read
 *
 */
LEAF(IndxLoadTag_SD)
	.set	noreorder

	cache	Index_Load_Tag_SD,0(a0) /* Read the tag location */
	nop
	mfc0	v0,C0_TAGLO		/* Read tag low register */
	j	ra
	nop
	.set	reorder
END(IndxLoadTag_SD)
