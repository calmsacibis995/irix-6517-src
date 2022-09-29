#ident	"$Id: dcache_aina.s,v 1.1 1994/07/21 01:24:43 davidl Exp $"
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
#include "sys/sbd.h"

#include "cache_err.h"



 ##############################################################################
 #									      #
 #	Data cache data aina test.					      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #	Tests data cache for addressing faults.				      #
 #									      #
 #	User Information:						      #
 #									      #
 #	The cache initialization routine should invalid the cache lines at    #
 #	the begin of the routine. Since it is write back cache and this	      #
 #	routine only loads and stores data within the range of K0BASE to      #
 #	(K0BASE + cachsize). There should be no line replacement in the cache #
 #	during this test.						      # 
 #									      #
 ##############################################################################


LEAF(Dcache_aina)

	move	s0, ra			#save the return address

	jal	dcache_init		# init and invalidate d cache

	jal	dcache_size

	li	a0,K0BASE		# first address
	addu	a1,a0,v0		# last address + 4

	/*
	 * Start of address-in-address test algorithm.
	 */
	jal	ainatest
	bne	v0,zero,error
	j	s0			# return
error:
	li	v0,1			# failure return
	li	a3,AINA_ERR		# error code
	j	cache_err		# jmp the error handling routine

END(Dcache_aina)
