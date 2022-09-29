#ident	"$Id: badsim.s,v 1.1 1994/07/20 23:50:36 davidl Exp $"
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


#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/mc.h"
#include "sys/sbd.h"

#include "monitor.h"


#ifdef IP24

#define	BANKS	0x2		/* define the number of indy banks */
#endif
#if !defined(IP24) & defined(IP22)

#define	BANKS	0x2		/* define the number of indy banks */
#endif


/*		b a d s i m m ( )
 *
 * badsimm()- displays the failing simm coordinate and the suspected dmux
 * coordinate. This routine should work for fullhouse with some minor
 * changes.
 *
 * INPUTS:
 *	a0 - failing address
 *	a1 - XOR (failing bits)
 *
 * Register Used:
 *	a0 - Failing address, PRINT usage, scratch
 *	a1 - XOR data, PRINT usage, scratch
 *	ra - Clobber on subroutine call
 *	t6 - Saved RA
 *	t2 - temporary bank counter
 *	t1 - Temporary register
 *	t0 - Pointer to message tables 
 *	s6 - failing address
 *	s4 - XOR data
 *
 */
LEAF(badsimm)

	move	t6, ra

	/* save failing bits */
	move	s4, a1
	move	s6, a0

	/* Is_Fullhouse() routine should go here */
	/* for now include the guiness code here */


	/* Start at bank 0 */
	li	t2, 0
loop:
	
	li	t1, 2
	sltu	a1, t2, t1		# check if bank 2 or 3 
	beq	a1, zero, 1f		# and branch

	li	t1, K1BASE | MEMCFG0	# addr of mem config register 0
	lw	t0, (t1)		# for banks 0 and 1
	b	2f
1:
	li	t1, K1BASE | MEMCFG1	# Read mem config reg 1 for banks
	lw	t0, (t1)		# 2 and 3
2:
	move	t1, t2			# determine if bank 0/2 or 1/3
	not	t1
	li	a1, 1
	and	t1, a1			# t1= 0 for banks 0/2
	beq	t1, zero, 1f		# if zero don't shift, bank 1/3
	srl	t0, t0, 16		# else shift bank 0/2 info down
					# shift 16 places if bank 0/2
					# don't shift if bank 1/3 data is already
					# where we want it.

1:
	/* Now decide which bank is failing based on the base address bits*/
	li	t1, MEMCFG_ADDR_MASK
	and	t1, t0, t1		# mask off base address bits
	sll	t1, 22			# and shift to bits 29-22 of physical addr
	li	a0, MEMCFG_DRAM_MASK	# 
	and	a0, t0, a0		# determine the last addr on bank
	addi	a0, 0x100

	/* a0 equals size in megabytes */
	sll	a0, 14
	addu	t1, a0			# add base to size to get last address

	li	a0, 0x1fffffff		# change address to physical addr since
					# reg. data is physical
	and	a0, s6
	sltu	t1, a0, t1		# now check to see if failing addr is
	beq	t1, zero, 1f		# within this range

	/* within the range, Get the address of the table into t0 */
	la	t0, bnkptr		# pointer to simm pointer table
	sll	a0, t2, 2		# pick up table offset
	addu	t0, a0			# t0 = address of simm table
	b	2f

1:	/* Not withing the range go to next bank */
	addi	t2, 1
	li	a1, BANKS
	sltu	a1, t2, a1
	bne	a1, zero, loop	
	b	ext
2:
	/* Found the range display generic message */
	la	a0, chkmsg
	jal	_puts

	/* Get the table pointer for bank 0 or bank 1 */
	lw	t0, (t0)

	move	a0, s6			# restore failing address
	/* Mask word select bits. To determine failing word
	 * with the 128 bit memory array (4 words)
	 */
	li	t1, 0xc	
	and	a0, t1


	/* Add the index to the table pointer */
	addu	t0, a0
	lw	a0, (t0)		# display failing simm

	jal	_puts

	/* Print out the suspected failing dmux chip. Since the dmux chips
	 * are split such that one chips passes bits 0-15 of a simm and the
	 * other bits 16-31 of the simm, it is only necessary to know which
	 * half of the word is failing to detemine the chip. 
	 */ 
	andi	t0, s4, 0xffff
	beq	t0, zero, 1f
	
	/* must be low 16 bits */
	la	a0, dmuxmsg0
	jal	_puts

1:
	/* Check high 16 bits (31-16) */
	lui	t0, 0xffff
	and	t0, s4
	beq	t0, zero, ext
	la	a0, dmuxmsg1
	jal	_puts
		
	
ext:
	PRINT("\r\n")
	j	t6
END(badsimm)


	.align 2

chkmsg:	.asciiz "Check simm "
sim0msg: .asciiz "S1"
	.align 2
sim1msg: .asciiz "S2"
	.align 2
sim2msg: .asciiz "S3"
	.align 2
sim3msg: .asciiz "S4"
sim4msg: .asciiz "S5"
	.align 2
sim5msg: .asciiz "S6"
	.align 2
sim6msg: .asciiz "S7"
	.align 2
sim7msg: .asciiz "S8"
dmuxmsg0: .asciiz ", DMUX chip U66"
dmuxmsg1: .asciiz ", DMUX chip U50"


bnkptr: .word bnk0ptrs
	.word bnk1ptrs
	.word bnk0ptrs
	.word bnk1ptrs

bnk0ptrs:
	.word	sim0msg
	.word	sim1msg
	.word	sim2msg
	.word	sim3msg
bnk1ptrs:
	.word	sim4msg
	.word	sim5msg
	.word	sim6msg
	.word	sim7msg
	
