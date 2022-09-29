/*
 *	$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/libpdiag/r4k/RCS/d_indxwbinv.s,v 1.1 1994/07/21 01:24:37 davidl Exp $
 */
/*
 *  ---------------------------------------------------
 *  | Copyright (c) 1986 MIPS Computer Systems, Inc.  |
 *  | All Rights Reserved.                            |
 *  ---------------------------------------------------
 */
#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/sbd.h"

/*
 *		I n d x W B I n v _ D ( )
 *
 * IndxWBInv_D- Writes back the data and invalidates (n) primary cache
 * data locations, if the cache block contains the specified address.
 *
 * Registers Used:
 *	a0 - address to invalidate
 *	a1 - number of cache lines to invalidate
 *	a2 - cache linesize
 *	
 */
LEAF(IndxWBInv_D)
	.set	noreorder

1:
	cache	CACH_PD | C_IWBINV, 0x0(a0)
	addiu	a1, -1
	bne	a1, zero, 1b
	addu	a0, a2

	j	ra
	nop

	.set	reorder
END(IndxWBInv_D)
