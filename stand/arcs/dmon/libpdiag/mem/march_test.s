#ident	"$Id: march_test.s,v 1.1 1994/07/21 00:58:20 davidl Exp $"
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
 #	March test on memory or cache.					      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #      Tests memory or cache data for addressing and stuck-at faults.        #
 #                                                                            #
 #      Register Used:                                                        #
 #              s1 - saved first address                                      #
 #              s2 - saved last address                                       #
 #              s7 - saved return address                                     #
 #              a0 - address pointer                                          #
 #              a1 - expected pattern                                         #
 #              a2 - data read from memory                                    #
 #		t0 - pattern of 0x55555555				      #
 #		t1 - pattern of 0xaaaaaaaa				      #
 #                                                                            #
 #	User Information:						      #
 #									      #
 #              The input address determine whether the test is performed     #
 #      upon memory (Kseg1) or cache (Kseg0). In case of failure return,      #
 #      a0 = failing address, a1 = expected pattern, a2 = actual pattern.     #
 #                                                                            #
 ##############################################################################


LEAF(march_test)

	move	s7, ra			#save the return address

        move    s1,a0                   # save first address
        move    s2,a1                   # save last address + 4

	/*
	 * Start of march test
	 */

	li	t0,0x55555555		# alternating one's pattern
	li	t1,0xaaaaaaaa		# another alternating one's pattern

	/*
	 * Fill from firstaddr to lastaddr with pattern in t0. Asscend
	 * thru memory by word and verify pattern. Reset to pattern in t1
	 * and verify immediately.
	 */
march:
	move	a0,s1			# first address
1:
	sw	t0,(a0)			# store t0 pattern
	addiu	a0,a0,4			# next address
	bltu	a0,s2,1b		# reach last address yet?

	/*
	 * From firstaddr to lastaddr read and verify previously set pattern,
	 * then reset to new pattern. Read the word immediately to verify 
	 * new pattern is written.
	 */
	move	a1,t0			# load the expected value for compare
	move	a3,t1			# new pattern to write
	move	a0,s1			# first address
1:
	lw	a2,(a0)			# load from memory
	bne	a2,a1,error		# is it the pattern expected

	sw	a3,(a0)			# write new pattern

	/* may need to flush the write buffer here */
	lw	a2,(a0)			# read back immediately
	bne	a2,a3,2f		# is it the pattern just wrote

	addiu	a0,a0,4			# next address
	bltu	a0,s2,1b		# reach last address yet?

	/*
	 * Descend thru memory by word. Check for previously set value, set
	 * new value and verify immediate.
	 */
	move	a1,t1			# load the expected value for compare
	move	a3,t0			# new pattern to write
1:
	addiu	a0,a0,-4		# descend to next address
	lw	a2,(a0)			# load from memory
	bne	a2,a1,error		# is it the expected pattern?

	sw	a3,(a0)			# store new pattern

	/* may need to flush the write buffer */
	lw	a2,(a0)			# read back immediately
	bne	a2,a3,2f		# is the pattern just wrote
	bgtu	a0,s1,1b		# reach first address yet?

	beq	a3,0xaaaaaaaa,done	# is it done yet?
	move	a3,t0			# swap the pattern
	move	t0,t1
	move	t1,a3
	b	march			# do it the second time
done:
	move	v0,zero			# indicate success
	j	s7			# and return
2:
	move	a1,a3			# move the expected value to a1
					# for error report
error:
	li	v0,1			# fail return
	j	s7

END(march_test)
