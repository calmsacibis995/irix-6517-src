#ident  "ide/IP30/mem/misc_s.s  $Revision: 1.2 $"

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or	    | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.						    | */
/* ------------------------------------------------------------------ */

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
LEAF(WriteLoop16)
	.set	noat
	.set	noreorder
	
	lw	t3, 16(sp)
 	sltu	AT,a0,a1
	beq	AT,zero, 2f
	nop
	sw	a2,0(a0)
1:
	sw	a3,16(a0)
	sw	t3,32(a0)
	sw	a2,48(a0)
	sw	a3,64(a0)
	sw	t3,80(a0)
	sw	a2,96(a0)
	sw	a3,112(a0)
	sw	t3,128(a0)
	sw	a2,144(a0)
	sw	a3,160(a0)
	sw	t3,176(a0)
	sw	a2,192(a0)
	sw	a3,208(a0)
	sw	t3,224(a0)

	addiu	a0, 240
	sltu	AT,a0,a1
	bne	AT,zero, 1b
	sw	a2,0(a0)
2:
	move	v0, a0
	j	ra
	nop

	.set	reorder
	.set	at
END(WriteLoop16)

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
	sw	a2,14(a0)
	sw	a3,-14(a1)
	sw	a2,18(a0)
	sw	a3,-18(a1)
	sw	a2,22(a0)
	addiu	a0, 56
	sltu	AT,a0,a1
	bne	AT,zero, 1b
	sw	a3,-24(a1)
2:
	move	v0, a0
	j	ra
	nop

	.set	reorder
	.set	at
END(butfly)
