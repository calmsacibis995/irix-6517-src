#ident	"$Id: ainatest.s,v 1.1 1994/07/21 00:57:40 davidl Exp $"
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
 #	Aina test on memory		      				      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #      Tests memory or cache for addressing faults.			      #
 #                                                                            #
 #      Register Used:                                                        #
 #              a0 - input of first address                                   #
 #              a1 - input of last address                                    #
 #              a3 - temp register                                            #
 #              s1 - saved first address                                      #
 #              s2 - saved last address                                       #
 #              s7 - saved return address                                     #
 #                                                                            #
 #	User Information:						      #
 #                                                                            #
 #              The input address determine whether the test is performed     #
 #      upon memory (Kseg1) or cache (Kseg0). In case of failure return,      #
 #      a0 = failing address, a1 = expected pattern, a2 = actual pattern.     #
 #									      #
 ##############################################################################


LEAF(ainatest)

	move	s7, ra			#save the return address

        move    s1,a0                   # save first address
        move    s2,a1                   # save last address + 4

	/*
	 * Start of aina test
	 */

	/*----------------------------------------------------------------+
	| Set all locations to A's.                                       |
	+----------------------------------------------------------------*/
	move	a0, s1			# first address

	li	a1, 0xaaaaaaaa		# write pattern
1:
	sw	a1, (a0)		# write to location.
	addiu	a0,a0,4
	bltu	a0, s2, 1b		# check for last location.

	/*----------------------------------------------------------------+
	| Address-In-Address;  Ascending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/

	move	a0, s1			# get starting address.
1:
	sw	a0, (a0)		# write address-in-address

	addiu	a0,a0,4			# increment to next address
	bltu	a0, s2, 1b		# reach the end ?

	move	a0, s1			# get starting address.
2:
	move	a1,a0			# load expected pattern
	lw	a2, (a0)		# ascending check address-in-address
	bne	a1, a2, aina_err

	addiu	a0,a0,4			# increment to next address
	bltu	a0,s2,2b		# reach the end ?

	/*----------------------------------------------------------------+
	| ~Address-In-Address;  Ascending Stores; Descending Check        |
	+----------------------------------------------------------------*/

	move	a0,s1			# get starting address.
1:
	not	a1, a0
	sw	a1, (a0)		# write ~address-in-address

	addiu	a0,a0,4			# increment to next address
	bltu	a0, s2, 1b		# reach the end ?

	addiu	a0,s2,-4		# get last address
2:
	not	a1, a0
	lw	a2, (a0)		# descending check ~address-in-address
	bne	a1,a2,aina_err

	addiu	a0,a0,-4			# decrement the address.
	bgeu	a0, s1, 2b		# reach the beginning ?

	/*----------------------------------------------------------------+
	| Address-In-Address; Descending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/

	addiu	a0,s2,-4		# get last address
1:
	sw	a0,(a0) 		# write address-in-address

	addiu	a0,a0,-4  		# decrement the address.
	bgeu	a0, s1, 1b		# reach the beginning ?

	move	a0, s1			# get starting address.
2:
	move	a1,a0			# get the expected value
	lw	a2, (a0)		# ascending check address-in-address
	bne	a1, a2, aina_err

	addiu	a0,a0,4			# increment to next address
	bltu	a0,s2, 2b		# reach the end ?

	/*----------------------------------------------------------------+
	|~Address-In-Address; Descending Stores; Descending Check         |
	+----------------------------------------------------------------*/

	addiu	a0,s2,-4		# get last address
1:
	not	a1, a0
	sw	a1, (a0)		# write ~address-in-address

	addiu	a0,a0,-4		# decrement the address.
	bgeu	a0, s1, 1b		# reach the beginning ?

	addiu	a0,s2,-4		# get last address
2:
	not	a1, a0
	lw	a2, (a0)		# ascending check address-in-address
	bne	a1, a2, aina_err

	addiu	a0,a0,-4		# decrement the address.
	bgeu	a0, s1, 2b		# reach the beginning ?

	move	v0, zero
	j	s7			# return to calling routine

aina_err:
	li	v0, 1			# return failure
	j	s7

END(ainatest)

