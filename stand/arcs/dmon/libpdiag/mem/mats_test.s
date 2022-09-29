#ident	"$Id: mats_test.s,v 1.1 1994/07/21 00:58:22 davidl Exp $"
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


 ##############################################################################
 #									      #
 #	Memory or cache data MATS+ test.				      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #	Tests memory or cache data for addressing and stuck-at faults.	      #
 #									      #
 #	Register Used:							      #
 #		a0 - input of first address				      #
 #		a1 - input of last address				      #
 #		a3 - temp register					      #
 #		t0 - pattern of 0x55555555				      #
 #		t1 - pattern of 0xaaaaaaaa				      #
 #		s1 - saved first address				      #
 #		s2 - saved last address					      #
 #		s7 - saved return address				      #
 #									      #
 #	User Information:						      #
 #									      #
 #		The input address determine whether the test is performed     #
 #	upon memory (Kseg1) or cache (Kseg0). In case of failure return,      #
 #	a0 = failing address, a1 = expected pattern, a2 = actual pattern.     #
 #									      #
 ##############################################################################


LEAF(mats_test)

	move	s7,ra			# save return address

	move	s1,a0			# save first address
	move	s2,a1			# save last address + 4

	/*
	 * Start of mats+ algorithm.  Performs mats+ test for addresses from
	 * firstaddr up to but not including lastaddr + 1.
	 *
	 */

	li	t0,0x55555555		# alternating one's pattern
	li	t1,0xaaaaaaaa		# another alternating one's pattern

	/*
	 * Fill from firstaddr + 4 to lastaddr with 5's.
	 */
	addiu	a0,s1,4			# first address + 4
1:
	sw	t0,(a0)			# store 5's
	addiu	a0,a0,4			# next address
	bltu	a0,s2,1b		# reach last address yet?

	/*
	 * Fill firstaddr with a's.
	 */
	move	a0,s1			# first address
	sw	t1,(a0)			# store a's
	addiu	a0,a0,4			# next address

	/*
	 * From firstaddr + 4 to lastaddr read and verify 5's, then write a's.
	 */
	move	a1,t0			# load the expected value for compare
1:
	lw	a2,(a0)			# load from memory
	bne	a2,a1,2f		# is it 5's?

	sw	t1,(a0)			# write a's
	addiu	a0,a0,4			# next address
	bltu	a0,s2,1b		# reach last address yet?

	/*
	 * From firstaddr to lastaddr - 4 read and verify a's, then write 5's.
	 */
	move	a0,s1			# reload saved start address
	subu	a3,s2,4			# lastaddr
	move	a1,t1			# load the expected value for compare
1:
	lw	a2,(a0)			# load from memory
	bne	a2,a1,2f		# is it a's?

	sw	t0,(a0)			# store 5's
	addiu	a0,a0,4			# next address
	bltu	a0,a3,1b		# reach last address-4 yet?

	/*
	 * Verify lastaddr is a's (don't need to write).
	 */
	lw	a2,(a0)			# load from memory
	bne	a2,t1,2f		# is it a's?

	/*
	 * Verify firstaddr is 5's.
	 */
	move	a0,s1			# get first address
	lw	a2,(a0)			# load from cache
	bne	a2,t0,2f		# is it 5's

	move	v0,zero			# indicate success
	j	s7			# and return
2:
	li	v0,1
	j	s7			# return failure

END(mats_test)
