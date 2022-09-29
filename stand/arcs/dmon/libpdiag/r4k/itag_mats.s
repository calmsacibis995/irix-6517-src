#ident	"$Id: itag_mats.s,v 1.1 1994/07/21 01:25:31 davidl Exp $"
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

#include "pdiag.h"
#include "cache_err.h"


 ##############################################################################
 #									      #
 #	Instruction cache tag MATS+ test.				      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #	Tests instruction cache tag for stuck-at faults.		      #
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

LEAF(Itag_mats)
	.set noreorder
	move	s0, ra			#save the return address

	jal	icache_size
	nop
	move	a0,v0			# save dcache sizes

	jal	icache_ln_size
	nop
	move	a1,v0			# save line size

	li	t7,K0BASE		# first address
	addu	t6,t7,a0		# last address + 4
	move	t5,a1			# line size

	mfc0	s1,C0_SR		# save current C0_SR
	nop
	and	a0,s1,~SR_IE		# disable all interrupt
	mtc0	a0,C0_SR
	nop
	mtc0	zero,C0_CAUSE		# clear cause register
	nop

	/*
	 * Start of mats+ algorithm.  Performs mats+ test from
	 * tag zero up to the last tag
	 */

	/*
	 * bit 1-5 of TAGLO register are zeros
	 */
	li	t0,0x55555500		# alternating one's pattern
	li	t1,0xaaaaaa00		# another alternating one's pattern
	move	a0,t7			# address for tag 0

	/*
	 * Fill from tag 1 to last tag with 5's.
	 */
	addu	a0,a0,t5		# address for tag 1
	mtc0	t0,C0_TAGLO		# load TAGLO with 5's
	nop
1:
	cache	CACH_PI | C_IST,(a0)	# store 5's to tag
	nop
	addu	a0,a0,t5		# next tag
	blt	a0,t6,1b		# reach last tag yet?
	nop

	/*
	 * Fill tag 0 with a's.
	 */
	move	a0,t7			# first address
	mtc0	t1,C0_TAGLO		# load TAGLO with a's
	nop
	cache	CACH_PI | C_IST,(a0)	# store a's
	nop
	addu	a0,a0,t5		# next tag

	/*
	 * From tag 1 to last tag read and verify 5's, then write a's.
	 */
	move	a1,t0			# load the expected value for compare
1:
	cache	CACH_PI | C_ILT,(a0)	# load TAGLO from cache tag
	nop
	mfc0	a2,C0_TAGLO		# move TAGLO to a2
	nop
	and	a2,a2,0xffffff00	# ignore the parity bit
	bne	a2,a1,2f		# is it 5's?
	nop

	mtc0	t1,C0_TAGLO		# load TAGLO with a's
	nop
	cache	CACH_PI | C_IST,(a0)	# store a's to tag
	nop
	addu	a0,a0,t5		# next tag
	blt	a0,t6,1b		# reach last tag yet?
	nop

	/*
	 * From tag 0 to (last-tag - 1) read and verify a's, then write 5's.
	 */
	move	a0,t7			# reload saved start address
	subu	t4,t6,t5		# last-tag 
	move	a1,t1			# load the expected value for compare
1:
	cache	CACH_PI | C_ILT,(a0)	# load TAGLO for cache tag
	nop
	mfc0	a2,C0_TAGLO		# load a2 from TAGLO
	nop
	and	a2,a2,0xffffff00	# ignore the parity bit
	bne	a2,a1,2f		# is it a's?
	nop

	mtc0	t0,C0_TAGLO		# load TAGLO with 5's
	nop
	cache	CACH_PI | C_IST,(a0)	# store 5's to tag
	nop
	addu	a0,a0,t5		# next tag
	blt	a0,t4,1b		# reach last address yet?
	nop

	/*
	 * Verify last tag is a's (don't need to write).
	 */
	cache	CACH_PI | C_ILT,(a0)	# load TAGLO from tag
	nop
	mfc0	a2,C0_TAGLO		# load a2 from TAGLO
	nop
	and	a2,a2,0xffffff00	# ignore the parity bit
	bne	a2,t1,2f		# is it a's?
	nop

	/*
	 * Verify tag 0 is 5's.
	 */
	move	a0,t7			# get first address
	cache	CACH_PI | C_ILT,(a0)	# load TAGLO from tag
	nop
	mfc0	a2,C0_TAGLO		# move to a2
	nop
	and	a2,a2,0xffffff00	# ignore the parity bit
	bne	a2,t0,2f		# is it 5's
	nop

	mtc0	s1,C0_SR		# restore C0_SR
	nop
	move	v0,zero			# indicate success
	j	s0			# and return
2:
	mtc0	s1,C0_SR		# resetor C0_SR
	nop
	li	a3,ITAG_ERR		# error code
	j	cache_err		# jmp the error handling routine
	.set reorder

END(Idata_mats)
