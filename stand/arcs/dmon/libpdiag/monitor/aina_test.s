#ident	"$Id: aina_test.s,v 1.1 1994/07/21 01:12:59 davidl Exp $"
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
/*#include "mem_test.h"*/

#if	defined (RB3125) || defined (R3030)
#define CLEAR_SR_ISC(save_reg) \
	.set	noreorder; \
	.set	noat; \
	mfc0	save_reg, C0_SR; \
	li	AT, ~SR_ISC; \
	and	AT, AT, save_reg; \
	mtc0	AT, C0_SR; \
	nop; \
	.set	at; \
	.set	reorder;

#define RESTORE_SR(save_reg) \
	.set	noreorder; \
	mtc0	save_reg, C0_SR; \
	nop; \
	.set	reorder;
#endif	(RB3125)


/*========================================================================+
| Routine  : int aina_test(first_addr, last_addr, loopcnt)	  	  |
|                                                                         |
| Function :  Address In Address Test					  | 
|                                                                         |
| Description: This is a traditional, rule-of-thumb, "address-in-address" |
|    memory test. It makes one pass writing the address, followed by one  |
|    read pass.  Both writes and reads are in ascending order.            |
|                                                                         |
| Register Usage:                                                         |
|       - t2	work address            - t4	write pattern             |
|       - t5    read pattern                                              |
|       - t8	start address           - t9	last address              |
|       - s3	loop count (0-forever)  - s4	saved C0_SR               |
+========================================================================*/
	.text
	.align	2			# make sure we're on word boundary.
#define	RetAdr	s7
LEAF(aina_test)
	move	RetAdr, ra		# saved return address.
	move	s1, a0			# save first_addr in s1
	move	s2, a1			# save last_addr in s2
	move	s3, a2			# save loopcnt: loop forever if 0

aina_loop:
	move	t8, s1			# initialize start and last addresses
	move	t9, s2

	/*----------------------------------------------------------------+
	| Set all locations to A's.                                       |
	+----------------------------------------------------------------*/
aina_pass1:

#if	defined (RB3125) || defined (R3030)
	CLEAR_SR_ISC(s4)
#endif	(RB3125)

	PRINT("\n  Set all locations to 0xAAAAAAAA ")

#if	defined (RB3125) || defined (R3030)
	RESTORE_SR(s4)
#endif	(RB3125)

	move	t2, t8			# get starting address.
	li	t4, 0xaaaaaaaa		# write pattern
1:
	sw	t4, (t2)		# write to 4-consecutive location.
	sw	t4, 4 (t2)
	sw	t4, 8 (t2)
	sw	t4, 12 (t2)

	addiu	t2, 16			# check for last location.
	bltu	t2, t9, 1b

	/*----------------------------------------------------------------+
	| Address-In-Address;  Ascending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/
aina_pass2:

#if	defined (RB3125) || defined (R3030)
	CLEAR_SR_ISC(s4)
#endif	(RB3125)

	PRINT("\n  Address-in-Address;  Ascending stores;  Ascending check")

#if	defined (RB3125) || defined (R3030)
	RESTORE_SR(s4)
#endif	(RB3125)

	move	t2, t8			# get starting address.
1:
	sw	t2, (t2)		# write address-in-address

	addiu	t2, 4			# increment to next address
	bltu	t2, t9, 1b		# reach the end ?

	move	t2, t8			# get starting address.
2:
	lw	t5, (t2)		# ascending check address-in-address
	move	t4, t2
	bne	t4, t5, aina_err

	addiu	t2, 4			# increment to next address
	bltu	t2, t9, 2b		# reach the end ?


	/*----------------------------------------------------------------+
	| ~Address-In-Address;  Ascending Stores; Descending Check        |
	+----------------------------------------------------------------*/
aina_pass3:

#if	defined (RB3125) || defined (R3030)
	CLEAR_SR_ISC(s4)
#endif	(RB3125)

	PRINT("\n  ~Address-in-Address; Ascending stores;  Descending check")

#if	defined (RB3125) || defined (R3030)
	RESTORE_SR(s4)
#endif	(RB3125)

	move	t2, t8			# get starting address.
1:
	not	t4, t2
	sw	t4, (t2)		# write ~address-in-address

	addiu	t2, 4			# increment to next address
	bltu	t2, t9, 1b		# reach the end ?

	subu	t2, t9, 4		# get last address
2:
	lw	t5, (t2)		# descending check ~address-in-address
	not	t4, t2
	bne	t4, t5, aina_err

	subu	t2, 4			# decrement the address.
	bgeu	t2, t8, 2b		# reach the beginning ?


	/*----------------------------------------------------------------+
	| Address-In-Address; Descending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/
aina_pass4:

#if	defined (RB3125) || defined (R3030)
	CLEAR_SR_ISC(s4)
#endif	(RB3125)

	PRINT("\n  Address-in-Address;  Descending stores; Ascending check")

#if	defined (RB3125) || defined (R3030)
	RESTORE_SR(s4)
#endif	(RB3125)

	subu	t2, t9, 4		# get last address
1:
	sw	t2, (t2)		# write address-in-address

	subu	t2, 4			# decrement the address.
	bgeu	t2, t8, 1b		# reach the beginning ?

	move	t2, t8			# get starting address.
2:
	lw	t5, (t2)		# ascending check address-in-address
	move	t4, t2
	bne	t4, t5, aina_err

	addiu	t2, 4			# increment to next address
	bltu	t2, t9, 2b		# reach the end ?

	/*----------------------------------------------------------------+
	|~Address-In-Address; Descending Stores; Descending Check         |
	+----------------------------------------------------------------*/
aina_pass5:

#if	defined (RB3125) || defined (R3030)
	CLEAR_SR_ISC(s4)
#endif	(RB3125)

	PRINT("\n  ~Address-in-Address; Descending stores; Descending check")

#if	defined (RB3125) || defined (R3030)
	RESTORE_SR(s4)
#endif	(RB3125)

	subu	t2, t9, 4		# get last address
1:
	not	t4, t2
	sw	t4, (t2)		# write ~address-in-address

	subu	t2, 4			# decrement the address.
	bgeu	t2, t8, 1b		# reach the beginning ?

	subu	t2, t9, 4		# get last address
2:
	lw	t5, (t2)		# ascending check address-in-address
	not	t4, t2
	bne	t4, t5, aina_err

	subu	t2, 4			# decrement the address.
	bgeu	t2, t8, 2b		# reach the beginning ?


#if	defined (RB3125) || defined (R3030)
	CLEAR_SR_ISC(s4)
#endif	(RB3125)

	PRINT("\nPASSED\n")

#if	defined (RB3125) || defined (R3030)
	RESTORE_SR(s4)
#endif	(RB3125)

	beq	s3, zero, aina_loop	# loop forever if loopcnt==0
	beq	s3, 1, 1f
	sub	s3, 1
	j	aina_loop
1:
	move	v0, zero
	j	RetAdr			# return to calling routine

aina_err:
	move	s0, t2			# failing address.
	move	s1, t4			# expected value.
	move	s2, t5			# actual value.

#if	defined (RB3125) || defined (R3030)
	CLEAR_SR_ISC(s4)
#endif	(RB3125)

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
	move	a0, s0
	xor	a1, s1, s2
	jal	badsimm
	/* RESTORE_SR(s4) */
	li	v0, 1
	j	RetAdr			# return to calling routine
END(aina_test)
