#ident	"$Id: nv_addrudbg.s,v 1.1 1994/07/21 01:14:51 davidl Exp $"
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
#include "monitor.h"


	.extern	_puts
	.extern puthex
	.extern putbcd
	.extern addr_nvram

/*
 * NVRAM Address Uniqueness Test
 */

/*========================================================================+
| Routine : nvram_addru_pat  - Address Uniqueness                         |
|    									  |
| Syntax: int nvram_addru_pat(int start_byte_index, int end_byte_index)   |
|    									  |
| Description: This is a traditional, rule-of-thumb, "address-in-address" |
|   memory  test.  It also  puts  the complement  of the  address  in the |
|   address, and makes passes in both ascending  and decending addressing |
|   order. There are both full memory store then check passes, as well as |
|   read-after-write passes (with complementing).                         |
|    									  |
| Return Status: 0 - PASS, 1 - FAIL					  | 
|                                                                         |
| Register Usage:                                                         |
|       t2 - work address            t3 - write pattern                   |
|       t4 - read pattern            t5 - start address                   |
|       t6 - last address            t7 - saved return address            |
+========================================================================*/

	.text
	.align	2			# make sure we're on word boundary.

LEAF(nvram_addrudbg)
	move	t7, ra			# save return address.

1:
	PRINT("\nStart address:0x________")
	PRINT("\b\b\b\b\b\b\b\b")

	jal	gethex			/* get address */
	move	s0, v0			/* save address in s0 */
	bne	v1,zero, 1b		/* 0 = good, !0 = bad input */
	move	t5, s0

2:
	PRINT("\nEnd address:0x________")
	PRINT("\b\b\b\b\b\b\b\b")

	jal	gethex			/* get address */
	move	s0, v0			/* save address in s0 */
	bne	v1,zero, 2b		/* 0 = good, !0 = bad input */
	move	t6, s0


	/*----------------------------------------------------------------+
	| Set all locations to A's.                                       |
	+----------------------------------------------------------------*/
addru_pass1:
	move	t2, t5			# get starting address.
	li	t3, 0x000000aa		# write pattern (lsb only)
1:
	sb	t3, (t2)		# write to 4-consecutive location.
	sb	t3, 4(t2)
	sb	t3, 8(t2)
	sb	t3, 12(t2)

	addiu	t2, 16			# check for last location.
	bltu	t2, t6, 1b

	/*----------------------------------------------------------------+
	| Write ~Address--In-Address; Ascending Stores; Read-After-Write; |
	| Flip Read-After-Write; Flip Write; Decending Check.             |
	+----------------------------------------------------------------*/
addru_pass2:
	move	t2, t5			# get starting address.
1:
	not	t3, t2			# write complement of addr-to-addr
	.set	noreorder
	sb	t3, (t2)
	nop
	nop
	lbu	t4, (t2)		# read-after-write.
	.set	reorder
	andi	t3, 0x00FF		# lsb only
	bne	t3, t4, addru_err	# verify read data

	.set	noreorder
	sb	t2, (t2) 		# write addr-to-addr
	li	t8, 1			# Error code
	nop
	lbu	t4, (t2)		# read-after-write
	.set	reorder
	andi	t3, t2, 0x00FF		# lsb only
	bne	t3, t4, addru_err	# verify read data

	not	t3, t2			# flip pattern
	sb	t3, (t2)		#   and write.

	addiu	t2, 4			# check for last location.
	bltu	t2, t6, 1b

	subu	t2, t6, 4		# get last address
	li	t8, 2			# Error code
2:
	lbu	t4, (t2)		# read data back.
	not	t3, t2			# expected value.
	andi	t3, 0x00FF		# lsb only
	bne	t3, t4, addru_err

	subu	t2, 4
	bgeu	t2, t5, 2b

	/*----------------------------------------------------------------+
	| Set all locations to 5's.                                       |
	+----------------------------------------------------------------*/
addru_pass3:
	move	t2, t5			# get starting address.
	li	t3, 0x55555555		# write pattern
1:
	sb	t3, (t2)		# write to 4-consecutive location.
	sb	t3, 4 (t2)
	sb	t3, 8 (t2)
	sb	t3, 12 (t2)

	addiu	t2, 16			# check for last location.
	bltu	t2, t6, 1b

	/*----------------------------------------------------------------+
	| Write ~Address--In-Address; Descending Stores; Read-After-Write;|
	| Flip Read-After-Write; Flip Write; Ascending Check.             |
	+----------------------------------------------------------------*/
addru_pass4:
	subu	t2, t6, 4		# get last address.
1:
	li	t8, 3			# Error code
	sb	t2, (t2)		# write of addr-to-addr
	andi	t3, t2, 0x00FF		# expected value: lsb only
	lbu	t4, (t2)		# read-after-write.
	bne	t3, t4, addru_err	# verify read data

	not	t3, t3			# flip pattern.
	.set	noreorder
	sb	t3, (t2)
	li	t8, 4			# Error code
	nop
	lbu	t4, (t2)		# read-after-write
	.set	reorder
	andi	t3, 0x00FF		# lsb only
	bne	t3, t4, addru_err	# verify read data

	sb	t2, (t2)		# and write addr-to-addr

	subu	t2, 4			# check for last location.
	bgeu	t2, t5, 1b

	move	t2, t5			# get start address
	li	t8, 5			# Error code
2:
	lbu	t4, (t2)		# read data back.
	andi	t3, t2, 0x00FF		# expected value: lsb only
	bne	t3, t4, addru_err

	addiu	t2, 4
	bltu	t2, t6, 2b

	/*----------------------------------------------------------------+
	| Set all locations to A's.                                       |
	+----------------------------------------------------------------*/
addru_pass5:
	move	t2, t5			# get starting address.
	li	t3, 0xaaaaaaaa		# write pattern
1:
	sb	t3, (t2)		# write to 4-consecutive location.
	sb	t3, 4(t2)
	sb	t3, 8(t2)
	sb	t3, 12(t2)

	addiu	t2, 16			# check for last location.
	bltu	t2, t6, 1b

	/*----------------------------------------------------------------+
	| Write ~Address--In-Address; Descending Stores; Read-After-Write;|
	| Flip Read-After-Write; Flip Write; Decending Check.             |
	+----------------------------------------------------------------*/
addru_pass6:
	subu	t2, t6, 4		# get last address.
1:
	li	t8, 6			# Error code
	not	t3, t2			# write complement of addr-to-addr
	.set	noreorder
	sb	t3, (t2)
	nop
	nop
	lbu	t4, (t2)		# read-after-write.
	.set	reorder
	andi	t3, 0x00FF		# lsb only
	bne	t3, t4, addru_err	# verify read data

	.set	noreorder
	sb	t2, (t2)		# write addr-to-addr
	li	t8, 7			# Error code
	nop
	lbu	t4, (t2)		# read-after-write
	.set	reorder
	andi	t3, t2, 0x00FF		# expected value: lsb only
	bne	t3, t4, addru_err	# verify read data

	not	t3, t2			# write ~addr-to-addr
	sb	t3, (t2)		# 

	subu	t2, 4			# check for last location.
	bgeu	t2, t5, 1b

	subu	t2, t6, 4		# get last address
2:
	li	t8, 8			# Error code
	lbu	t4, (t2)		# read data back.
	not	t3, t2			# expected value: ~addr
	andi	t3, 0x00FF		# expected value: lsb only
	bne	t3, t4, addru_err

	subu	t2, 4
	bgeu	t2, t5, 2b

	/*----------------------------------------------------------------+
	| Set all locations to 5's.                                       |
	+----------------------------------------------------------------*/
addru_pass7:
	move	t2, t5			# get starting address.
	li	t3, 0x55555555		# write pattern
1:
	sb	t3, (t2)		# write to 4-consecutive location.
	sb	t3, 4(t2)
	sb	t3, 8(t2)
	sb	t3, 12(t2)

	addiu	t2, 16			# check for last location.
	bltu	t2, t6, 1b

	/*----------------------------------------------------------------+
	| Write ~Address--In-Address; Ascending Stores; Read-After-Write; |
	| Flip Read-After-Write; Flip Write; Acending Check.              |
	+----------------------------------------------------------------*/
addru_pass8:
	move	t2, t5			# get starting address.
1:
	li	t8, 9			# Error code
	sb	t2, (t2)		# write of addr-to-addr
	andi	t3, t2, 0x00FF		# expected value: lsb only
	lbu	t4, (t2)		# read-after-write.
	bne	t3, t4, addru_err	# verify read data

	not	t3, t3			# flip pattern.
	.set	noreorder
	sb	t3, (t2)
	li	t8, 0xa			# Error code
	nop
	lbu	t4, (t2)		# read-after-write
	.set	reorder
	andi	t3, 0x00FF		# lsb only
	bne	t3, t4, addru_err	# verify read data

	sb	t2, (t2)		# and write addr-to-addr

	addiu	t2, 4			# check for last location.
	bltu	t2, t6, 1b

	move	t2, t5			# get starting address.
	li	t8, 0xb			# Error code
2:
	lbu	t4, (t2)		# read data back.
	andi	t3, t2, 0x00FF		# expected value: lsb only
	bne	t3, t4, addru_err

	addiu	t2, 4			# check for last location.
	bltu	t2, t6, 2b

addru_done:
	move	v0, zero
	j	t7			# return to calling routine

addru_err:
	/*
	 * t2 - address
	 * t3 - expected data (LSB - least significant byte)
	 * t4 - actual data (LSB)
	 */
	la	a0, nvram_code
	jal	_puts
	move	a0, t8
	jal	puthex			# print error code

	la	a0, nverr_addr
	jal	_puts
	move	a0, t2
	jal	puthex			# print address

	la	a0, nverr_exp
	jal	_puts
	move	a0, t3
	li	a1, 1
	jal	putbcd			# print expected data

	la	a0, nverr_act
	jal	_puts
	move	a0, t4
	li	a1, 1
	jal	putbcd			# print actual data

	la	a0, nverr_xor
	jal	_puts
	xor	a0, t3, t4
	li	a1, 1
	jal	putbcd			# print xor
	
	or	v0, zero, 1		# set flag to fail
	j	addru_pass1
END(nvram_addrudbg)

	.data

nvram_code:	.asciiz "\r\n  Error code = "

nverr_addr:	.asciiz "\n\r-  Address 0x"
nverr_exp:	.asciiz " Expected 0x"
nverr_act:	.asciiz " Actual 0x"
nverr_xor:	.asciiz " Xor 0x"
