#ident	"$Id: oldsimx_test.s,v 1.1 1994/07/20 23:50:49 davidl Exp $"
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
/*#include "machine/cpu.h"*/

#include "pdiag.h"
#include "monitor.h"
#include "mem_test.h"


/* Increment to get to next location within a simm */
#define SAMEBANK	0x10


	.text
	.align	2			# make sure we're on word boundary.

/*		s i m 0 ( )
 *
 * sim0()- Test sim0 in bank 0. Can be used to test sim4 in bank 1 by 
 * changing the starting address to bank 1.
 *
 * Inputs:
 *	a2- first address to test. Controlled by fa command
 *	a3- last address to test. Controlled by la command
 *
 */
LEAF(sim0)
	li	t0, 0xfffffff0		# Make sure on simm 0 boundary
	and	a2, t0
	and	a3, t0
	j	simx_test
	nop
	/* Should never get here, saves a level of stack saving */
END(sim0)


/*		s i m 1 ( )
 *
 * sim1()- Test sim1 in bank 0. Can be used to test sim5 in bank 1 by 
 * changing the starting address to bank 1.
 *
 * Inputs:
 *	a2- first address to test. Controlled by fa command
 *	a3- last address to test. Controlled by la command
 *
 */
LEAF(sim1)
	li	t0, 0xfffffff0		# Make sure on simm 1 boundary
	and	a2, t0
	ori	a2, 0x4
	and	a3, t0
	ori	a3, 0x4
	j	simx_test
	nop
	/* Should never get here, saves a level of stack saving */
END(sim1)


/*		s i m 2 ( )
 *
 * sim2()- Test sim2 in bank 0. Can be used to test sim6 in bank 1 by 
 * changing the starting address to bank 1.
 *
 * Inputs:
 *	a2- first address to test. Controlled by fa command
 *	a3- last address to test. Controlled by la command
 *
 */
LEAF(sim2)
	li	t0, 0xfffffff0		# Make sure on simm 2 boundary
	and	a2, t0
	ori	a2, 0x8
	and	a3, t0
	ori	a3, 0x8
	j	simx_test
	nop
END(sim2)


/*		s i m 3 ( )
 *
 * sim3()- Test sim3 in bank 0. Can be used to test sim7 in bank 1 by 
 * changing the starting address to bank 1.
 *
 * Inputs:
 *	a2- first address to test. Controlled by fa command
 *	a3- last address to test. Controlled by la command
 *
 */
LEAF(sim3)
	li	t0, 0xfffffff0		# Make sure on simm 3 boundary
	and	a2, t0
	ori	a2, 0xc
	and	a3, t0
	ori	a3, 0xc
	j	simx_test
	nop
END(sim0)



/*========================================================================+
| Routine  : int simx_test(first_addr, last_addr, loopcnt)	  	  |
|                                                                         |
| Function :  Address In Address Test					  | 
|                                                                         |
| Description: This is a traditional, rule-of-thumb, "address-in-address" |
|    memory test. It makes one pass writing the address, followed by one  |
|    read pass.  Both writes and reads are in ascending order. Note the   |
|    next location to remain on the same simm maybe architecture dependent|
|    In other words this test checks one simm at a time.                  |
|                                                                         |
| Register Usage:                                                         |
|       - t2	work address            - t4	write pattern             |
|       - t5    read pattern                                              |
|       - t8	start address           - t9	last address              |
|       - s3	loop count (0-forever)  - s4	saved C0_SR               |
+========================================================================*/
LEAF(simx_test)
	move	s5, ra			# saved return address.
	li	a0, 0x4
	li	a1, 0x1
	jal	badsimm

	li	a0, 0xa8000000
	li	a1, 0x11000
	jal	badsimm

	li	a0, 0xa8000008
	li	a1, 0x8000
	jal	badsimm


	li	a0, 0xa800000c
	li	a1, 0x80000000
	jal	badsimm

	j	s5
	PUTHEX(s1)
	move	s1, a2			# save first_addr in s1
	move	s2, a3			# save last_addr in s2
	#move	s3, a2			# save loopcnt: loop forever if 0
	li	s3, 1

aina_loop:
	move	t8, s1			# initialize start and last addresses
	move	t9, s2

	/*----------------------------------------------------------------+
	| Set all locations to A's.                                       |
	+----------------------------------------------------------------*/
aina_pass1:

	PUTHEX(s1)
	PRINT("  ")
	PUTHEX(s2)

	PRINT("\n  Set all locations to 0xAAAAAAAA ")

	move	t2, t8			# get starting address.
	li	t4, 0xaaaaaaaa		# write pattern
1:
	sw	t4, (t2)		# write to 4-consecutive location.
	sw	t4, SAMEBANK(t2)
	sw	t4, SAMEBANK *2(t2)
	sw	t4, SAMEBANK *3(t2)

	addiu	t2, SAMEBANK *4		# check for last location.
	bltu	t2, t9, 1b

	/*----------------------------------------------------------------+
	| Address-In-Address;  Ascending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/
aina_pass2:

	PRINT("\n  Address-in-Address;  Ascending stores;  Ascending check")

	move	t2, t8			# get starting address.
1:
	sw	t2, (t2)		# write address-in-address

	addiu	t2, SAMEBANK		# increment to next address
	bltu	t2, t9, 1b		# reach the end ?

	move	t2, t8			# get starting address.
2:
	lw	t5, (t2)		# ascending check address-in-address
	move	t4, t2
	bne	t4, t5, aina_err

	addiu	t2, SAMEBANK		# increment to next address
	bltu	t2, t9, 2b		# reach the end ?


	/*----------------------------------------------------------------+
	| ~Address-In-Address;  Ascending Stores; Descending Check        |
	+----------------------------------------------------------------*/
aina_pass3:

	PRINT("\n  ~Address-in-Address; Ascending stores;  Descending check")

	move	t2, t8			# get starting address.
1:
	not	t4, t2
	sw	t4, (t2)		# write ~address-in-address

	addiu	t2, SAMEBANK		# increment to next address
	bltu	t2, t9, 1b		# reach the end ?

	subu	t2, t9, SAMEBANK	# get last address
2:
	lw	t5, (t2)		# descending check ~address-in-address
	not	t4, t2
	bne	t4, t5, aina_err

	subu	t2, SAMEBANK		# decrement the address.
	bgeu	t2, t8, 2b		# reach the beginning ?


	/*----------------------------------------------------------------+
	| Address-In-Address; Descending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/
aina_pass4:

	PRINT("\n  Address-in-Address;  Descending stores; Ascending check")

	subu	t2, t9, SAMEBANK	# get last address
1:
	sw	t2, (t2)		# write address-in-address

	subu	t2, SAMEBANK		# decrement the address.
	bgeu	t2, t8, 1b		# reach the beginning ?

	move	t2, t8			# get starting address.
2:
	lw	t5, (t2)		# ascending check address-in-address
	move	t4, t2
	bne	t4, t5, aina_err

	addiu	t2, SAMEBANK		# increment to next address
	bltu	t2, t9, 2b		# reach the end ?

	/*----------------------------------------------------------------+
	|~Address-In-Address; Descending Stores; Descending Check         |
	+----------------------------------------------------------------*/
aina_pass5:

	PRINT("\n  ~Address-in-Address; Descending stores; Descending check")

	subu	t2, t9, SAMEBANK	# get last address
1:
	not	t4, t2
	sw	t4, (t2)		# write ~address-in-address

	subu	t2, SAMEBANK		# decrement the address.
	bgeu	t2, t8, 1b		# reach the beginning ?

	subu	t2, t9, SAMEBANK	# get last address
2:
	lw	t5, (t2)		# ascending check address-in-address
	not	t4, t2
	bne	t4, t5, aina_err

	subu	t2, SAMEBANK		# decrement the address.
	bgeu	t2, t8, 2b		# reach the beginning ?


	PRINT("\nPASSED\n")

	beq	s3, zero, aina_loop	# loop forever if loopcnt==0
	beq	s3, 1, 1f
	sub	s3, 1
	j	aina_loop
1:
	move	v0, zero
	j	s5			# return to calling routine

aina_err:
	move	s0, t2			# failing address.
	move	s1, t4			# expected value.
	move	s2, t5			# actual value.

	PRINT("\nError: Addr ")
	PUTHEX(s0)
	PRINT(" Expected ")
	PUTHEX(s1)
	PRINT(" Act ")
	PUTHEX(s2)
	PRINT(" Xor ")
	xor	a0, s1, s2
	PUTHEX(a0)
	PRINT("\n")
	li	v0, 1
	j	s5			# return to calling routine
END(simx_test)


/*		b a d s i m m ( )
 *
 * badsimm()- displays the failing simm coordinate and the suspected dmux
 * coordinate.
 *
 * INPUTS:
 *	a0 - failing address
 *	a1 - XOR (failing bits)
 *
 */
LEAF(badsimm)

	move	t6, ra

	/* save failing bits */
	move	s4, a1
	move	s6, a0

	move	s0, t2			# failing address.
	move	s1, t4			# expected value.
	move	s2, t5			# actual value.

	PRINT("\nError: Addr ")
	PUTHEX(s0)
	PRINT(" Expected ")
	PUTHEX(s1)
	PRINT(" Act ")
	PUTHEX(s2)
	PRINT(" Xor ")
	xor	a0, s1, s2
	PUTHEX(a0)
	PRINT("\n")
	/* Is_Fullhouse() routine should go here */
	/* for now include the guiness code here */

	la	a0, chkmsg
	jal	_puts

	move	a0, s6
	/* Mask word select bits. */
	li	t0, 0xc	
	and	a0, t0

	/* Get the address of the table into t0 */
	la	t0, simmptrs

	/* Add the index to the table pointer */
	addu	t0, a0
	lw	a0, (t0)

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
	lui	t0, 0xffff
	and	t0, s4
	beq	t0, zero, 2f
	la	a0, dmuxmsg1
	jal	_puts
		
	
2:
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
	
