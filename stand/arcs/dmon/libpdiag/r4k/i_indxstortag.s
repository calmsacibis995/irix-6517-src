#ident	"$Id: i_indxstortag.s,v 1.1 1994/07/21 01:25:18 davidl Exp $"
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
 * IndxLoadTagI(u_int vaddr)
 *
 *	Load the TagLo register with the tag of the primary instruction
 *	cache indexed by vaddr.
 *
 * register usage:
 *	a0 = The virtual address indexing the cache line.
 *	v0 = The return value of the tag.
 */
LEAF(IndxLoadTagI)

	.set noreorder
	cache	CACH_PI | C_ILT , 0(a0)
	mfc0	v0, C0_TAGLO
	nop
	j	ra
	nop
	.set reorder
END(IndxLoadTagI)



/*
 * IndxStorTagI(u_int vaddr, u_int tagvalue)
 *
 *      Store the tagvalue to the primary instruction cache tag indexed by
 *	vaddr.
 *
 * register usage:
 *      a0 = The virtual address indexing the cache line.
 *      a1 = The tag value store to the cache tag
 */

LEAF(IndxStorTagI)

	.set	noreorder
	mtc0	a1,C0_TAGLO
	nop
	cache	CACH_PI | C_IST,0(a0)
	j	ra
	nop
	.set	noreorder
END(IndxStorTagI)


/*
 * IndxInvldI(u_int vaddr)
 *
 *	Set the indexed primary instruction cache line to invalid.
 *
 * register usage:
 *	a0 = The virtual address indexing the cache line.
 */
LEAF(IndxInvldI)

	.set noreorder
	cache	CACH_PI | C_IINV, 0(a0)
	j	ra
	nop
	.set reorder
END(IndxInvldI)
