/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: index.s,v 1.6 1997/05/07 20:30:28 danc Exp $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

.weakext index, _index

#include "sys/asm.h"
#include "sys/regdef.h"

#if _MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2
LEAF(_index)
	andi	a1,a1,255	# convert int to unsigned char
1:	lb	a2,0(a0)
	PTR_ADDU	a0,1
	beq	a2,a1,2f
	bne	a2,zero,1b
	move	v0,zero
	j	ra

2:	PTR_SUBU	v0,a0,1
	j	ra
.end	_index
#else
	/* Unrolled TFP tuned index routine for use in mips4 libc.
	 * Written by Iain McClatchie (iain@mti).
	 */
	.set	noreorder
	.align	4
	nop
LEAF(_index)
	andi	a1,a1,255	# convert int to unsigned char
	lb	t0,0(a0)	# [0]
	beq	t0,a1,ret	# [1]
	PTR_ADDU a0,1		# [1]
1:	beq	t0,zero,fail	# [2]
	nop			# [2]
	lb	t0,0(a0)	# [2]
	beq	t0,a1,ret	# [3]
	PTR_ADDU a0,1		# [3]
	beq	t0,zero,fail	# [4]
	nop			# [4]
	lb	t0,0(a0)	# [4]
	beq	t0,a1,ret	# [5]
	PTR_ADDU a0,1		# [5]
	beq	t0,zero,fail	# [6]
	nop			# [6]
	lb	t0,0(a0)	# [6]
	beq	t0,a1,ret	# [7]
	PTR_ADDU a0,1		# [7]
	beq	t0,zero,fail	# [8]
	nop			# [8]
	lb	t0,0(a0)	# [8]
	bne	t0,a1,1b	# [9]
	PTR_ADDU a0,1		# [9]

ret:	j	ra
	PTR_SUBU v0,a0,1

fail:	j	ra
	move	v0,zero		

	.end	_index
#endif
