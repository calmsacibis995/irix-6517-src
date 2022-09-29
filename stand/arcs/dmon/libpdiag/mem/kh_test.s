#ident	"$Id: kh_test.s,v 1.1 1994/07/21 00:58:12 davidl Exp $"
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
 #	Memory or cache data Knaizuk Hartmann test.			      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #	Tests memory or cache data for addressing and stuck-at faults.	      #
 #									      #
 #	Register Used:							      #
 #		s1 - saved first address				      #
 #		s2 - saved last address					      #
 #		s7 - saved return address				      #
 #		a0 - address pointer					      #
 #		a1 - expected pattern					      #
 #		a2 - data read from memory				      #
 #									      #
 #	User Information:						      #
 #									      #
 #		The input address determine whether the test is performed     #
 #	upon memory (Kseg1) or cache (Kseg0). In case of failure return,      #
 #	a0 = failing address, a1 = expected pattern, a2 = actual pattern.     #
 #									      #
 ##############################################################################


LEAF(KH_test)

	move	s7,ra			# save return address

	move	s1,a0			# save first address
	move	s2,a1			# save last address + 4

	/*
	 * Start Knaizuk Hartmann test
	 */

	/*
	 * Set partitions 1 and 2 to 0's
	 */
	move	a1,zero			# set expected pattern
	addiu	a0,s1,4			# get first partition 1 address
1:
	sw	a1,(a0)			# set partition 1 to zero
	addiu	a0,a0,4			# increment to partition 2 address
	bgeu	a0,s2,2f		# reach the end yet?
	sw	a1,(a0)			# set partition 2 to zero
	addiu	a0,a0,8			# increment to partition 1 address
	bltu	a0,s2,1b		# reach the end yet?

	/*
	 * Set partition 0 to 1's
	 */
2:
	li	a1,-1			# set expected pattern
	move	a0,s1			# get first partition 0 address
3:
	sw	a1,(a0)			# set partition 0 to one's
	addiu	a0,a0,12		# next address
	bltu	a0,s2,3b		# reach the end yet?

	/*
	 * Verify partition 1 is still 0's
	 */
	move	a1,zero			# set expected pattern
	addiu	a0,s1,4			# get first partition 1 address
4:
	lw	a2,(a0)			# read data
	bne	a2,a1,error		# does it match?
	addiu	a0,a0,12		# bump to next location
	bltu	a0,s2,4b		# reach the end yet?

	/*
	 * Set partition 1 to 1's
	 */
	li	a1,-1			# set expected pattern
	addiu	a0,s1,4			# get first partition 1 address
5:
	sw	a1,(a0)			# set partition 1 to one's
	addiu	a0,a0,12		# next address
	bltu	a0,s2,5b		# reach the end yet?

	/*
	 * Verify partition 2 is still 0's
	 */
	move	a1,zero			# set expected pattern
	addiu	a0,s1,8			# get first partition 2 address
6:
	lw	a2,(a0)			# read data
	bne	a2,a1,error		# does it match?
	addiu	a0,a0,12		# bump to next location
	bltu	a0,s2,6b		# reach the end yet?

	/*
	 * Verify partition 0 is still 1's
	 */
	li	a1,-1			# set expected pattern
	move	a0,s1			# get first partition 0 address
7:
	lw	a2,(a0)			# read data
	bne	a2,a1,error		# does it match?
	addiu	a0,a0,12		# bump to next location
	bltu	a0,s2,7b		# reach the end yet?

	/*
	 * Verify partition 1 is still 1's
	 */
	li	a1,-1			# set expected pattern
	addiu	a0,s1,4			# get first partition 1 address
8:
	lw	a2,(a0)			# read data
	bne	a2,a1,error		# does it match?
	addiu	a0,a0,12		# bump to next location
	bltu	a0,s2,8b		# reach the end yet?

	/*
	 * Set partition 0 to 0's
	 */
	move	a1,zero			# set expected pattern
	move	a0,s1			# get first partition 0 address
9:
	sw	a1,(a0)			# set partition 0 to zero's
	addiu	a0,a0,12		# next address
	bltu	a0,s2,9b		# reach the end yet?

	/*
	 * Verify partition 0 is still 0's
	 */
	move	a1,zero			# set expected pattern
	move	a0,s1			# get first partition 0 address
10:
	lw	a2,(a0)			# read data
	bne	a2,a1,error		# does it match?
	addiu	a0,a0,12		# bump to next location
	bltu	a0,s2,10b		# reach the end yet?

	/*
	 * Set partition 2 to 1's
	 */
	li	a1,-1			# set expected pattern
	addiu	a0,s1,8			# get first partition 2 address
11:
	sw	a1,(a0)			# set partition 2 to one's
	addiu	a0,a0,12		# next address
	bltu	a0,s2,11b		# reach the end yet?

	/*
	 * Verify partition 2 is still 1's
	 */
	li	a1,-1			# set expected pattern
	addiu	a0,s1,8			# get first partition 2 address
12:
	lw	a2,(a0)			# read data
	bne	a2,a1,error		# does it match?
	addiu	a0,a0,12		# bump to next location
	bltu	a0,s2,12b		# reach the end yet?

	move	v0,zero			# indicate success
	j	s7			# and return
error:
	li	v0,1
	j	s7			# return failure

END(KH_test)
