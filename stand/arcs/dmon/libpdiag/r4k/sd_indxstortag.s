/*
 *	$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/libpdiag/r4k/RCS/sd_indxstortag.s,v 1.1 1994/07/21 01:26:01 davidl Exp $
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
	cache	CACH_SD | C_IST,0(a0) /* Write the tag location */
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

	mtc0	zero, C0_TAGLO
	nop
	nop
	cache	CACH_SD | C_ILT,0(a0) /* Read the tag location */
	nop
	nop
	nop
	mfc0	v0,C0_TAGLO		/* Read tag low register */
	j	ra
	nop
	.set	reorder
END(IndxLoadTag_SD)
