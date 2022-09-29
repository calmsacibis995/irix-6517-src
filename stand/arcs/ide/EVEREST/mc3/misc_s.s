/*
 * mc3/misc_s.s
 *
 *
 * Copyright 1991, 1992, Silicon Graphics, Inc.
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

#ident "$Id: misc_s.s,v 1.1 1994/01/28 22:13:40 frias Exp $"

#include "sys/asm.h"
#include "sys/regdef.h"


	.align	4
LEAF(WriteLoop14)
	.set	noat
	.set	noreorder
	
	lw	t3, 16(sp)
 	sltu	AT,a0,a1
	beq	AT,zero, 2f
	nop
	sw	a2,0(a0)
1:
	sw	a3,4(a0)
	sw	t3,8(a0)
	sw	a2,12(a0)
	sw	a3,16(a0)
	sw	t3,20(a0)
	sw	a2,24(a0)
	sw	a3,28(a0)
	sw	t3,32(a0)
	sw	a2,36(a0)
	sw	a3,40(a0)
	sw	t3,44(a0)
	sw	a2,48(a0)
	sw	a3,52(a0)
	sw	t3,56(a0)

	addiu	a0, 60
	sltu	AT,a0,a1
	bne	AT,zero, 1b
	sw	a2,0(a0)
2:
	move	v0, a0
	j	ra
	nop

	.set	reorder
	.set	at
END(WriteLoop14)

LEAF(butfly)
	.set	noreorder
	.set	noat
	
	lw	t3, 16(sp)
 	sltu	AT,a0,a1
	beq	AT,zero, 2f
	nop
1:
	sw	a2, 0(a0)
	sw	a3, 0(a1)
	sw	a2, 4(a0)
	sw	a3,-4(a1)
	sw	a2, 8(a0)
	sw	a3,-8(a1)
	sw	a2,12(a0)
	sw	a3,-12(a1)
	sw	a2,16(a0)
	sw	a3,-16(a1)
	sw	a2,20(a0)
	sw	a3,-20(a1)
	sw	a2,24(a0)
	sw	a3,-24(a1)
	addiu	a0, 28
	addiu	a1, -28
	sltu	AT,a0,a1
	bne	AT,zero, 1b
	nop
2:
	move	v0, a0
	j	ra
	nop

	.set	reorder
	.set	at
END(butfly)
