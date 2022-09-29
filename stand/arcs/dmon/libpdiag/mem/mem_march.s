#ident	"$Id: mem_march.s,v 1.1 1994/07/21 00:58:45 davidl Exp $"
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
#include "mem_err.h"


 ##############################################################################
 #									      #
 #	March test on memory.						      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #      Tests memory data for addressing and stuck-at faults.         	      #
 #                                                                            #
 #      Register Used:                                                        #
 #              a0 - input of first address                                   #
 #              a1 - input of last address                                    #
 #              a3 - error code                                               #
 #              s0 - saved return address                                     #
 #              s1 - saved first address                                      #
 #                                                                            #
 #	User Information:						      #
 #									      #
 #              The _FIRSTADDR and _LASTADDR must be in the Kseg1 range for   #
 #      memory testing. The routine mats_test does not check address.         #
 #									      #
 ##############################################################################


LEAF(mem_march)

	move	s0, ra			#save the return address

	li	a0,_FIRSTADDR		# first address 
	jal	lw_nvram
	move	s1,v0 			# first address

	li	a0,_LASTADDR		# last address
	jal	lw_nvram
	move	a1,v0			# last address + 4
	move	a0,s1			# first address

	/*
	 * Start of march test
	 */

	jal	march_test
	bne	v0,zero,error
	j	s0			# and return
error:
	li	v0,1			# failure return
	li	a3,MEM_ERR		# error code
	j	mem_err			# jmp the error handling routine

END(mem_march)
