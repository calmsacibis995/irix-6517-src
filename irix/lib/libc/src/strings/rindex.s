/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: rindex.s,v 1.6 1997/04/28 19:32:17 danc Exp $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

.weakext rindex, _rindex

#include "sys/regdef.h"
#include "sys/asm.h"

#if _MIPS_ISA != _MIPS_ISA_MIPS4
LEAF(_rindex)
	move	v0,zero
	andi	a1,a1,255	# convert int to unsigned char
1:	lb	a3,0(a0)
	PTR_ADDU	a0,1
	bne	a3,a1,2f
	PTR_SUBU	v0,a0,1
2:	bne	a3,zero,1b
	j	ra
.end	_rindex
#else
	/* Unrolled TFP (mips 4) specific rindex routine for use in mips4 libc.
	 * Written by Iain McClatchie (iain@mti).
	 */
	.set	noreorder
	.align	4
	nop
	nop
	nop
LEAF(_rindex)
	li	v0,1
	andi	a1,a1,255	# convert int to unsigned char
1:	lb	a3,0(a0)	# [0]
	PTR_ADDU	a0,1	# [0]
	xor	t0,a3,a1	# [1]
	beq	a3,zero,ret	# [1]
	movz	v0,a0,t0	# [2]
	lb	a3,0(a0)	# [2]
	PTR_ADDU	a0,1	# [2]
	xor	t0,a3,a1	# [3]
	beq	a3,zero,ret	# [3]
	movz	v0,a0,t0	# [4]
	lb	a3,0(a0)	# [4]
	PTR_ADDU	a0,1	# [4]
	xor	t0,a3,a1	# [5]
	beq	a3,zero,ret	# [5]
	movz	v0,a0,t0	# [6]
	lb	a3,0(a0)	# [6]
	PTR_ADDU	a0,1	# [6]
	xor	t0,a3,a1	# [7]
	bne	a3,zero,1b	# [7]
	movz	v0,a0,t0	# [8]

ret:	j	ra
	PTR_SUBU v0,1
	.set	reorder
	.end	_rindex
#endif
