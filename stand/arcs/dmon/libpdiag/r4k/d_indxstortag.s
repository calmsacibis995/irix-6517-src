#ident	"$Id: d_indxstortag.s,v 1.1 1994/07/21 01:24:27 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"


/*
 * u_int
 * IndxLoadTagD(u_int vaddr)
 *
 *      Load the TagLo register with the tag of the primary data
 *      cache indexed by vaddr.
 *
 * register usage:
 *      a0 = The virtual address indexing the cache line.
 *      v0 = The return value of the tag.
 */
LEAF(IndxLoadTagD)

        .set noreorder
        cache   CACH_PD | C_ILT, 0(a0)
        mfc0    v0, C0_TAGLO
        nop
        j       ra
	nop
        .set reorder
END(IndxLoadTagD)
/*
 * IndxStorTagD(u_int vaddr, u_int tagvalue)
 *
 *	Store the tagvalue to the primary data cache tag indexed by vaddr.
 *
 * register usage:
 *	a0 = The virtual address indexing the cache line.
 *	a1 = The tag value store to the cache tag
 */
LEAF(IndxStorTagD)

	.set	noreorder
	mtc0	a1,C0_TAGLO
	nop
	cache	CACH_PD | C_IST,0(a0)
	j	ra
	nop
	.set	reorder
END(IndxStorTagD)
/*
 * IndxInvldD(u_int vaddr)
 *
 *	Zero the tag of the indexed primary data cache line.
 *
 * register usage:
 *	a0 = The virtual address indexing the cache line.
 */
LEAF(IndxInvldD)

	move	a1, zero
	jal	IndxStorTagD
	nop
	j	ra
	nop
END(IndxInvldD)
