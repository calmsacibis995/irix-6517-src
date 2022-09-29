#ident	"$Id: dtag_parity.s,v 1.1 1994/07/21 01:25:08 davidl Exp $"
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
 #	Data cache tag parity test.					      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #	Tests data cache parity.					      #
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


LEAF(Dtag_parity)
	.set noreorder
	move	s0, ra			#save the return address

	jal	dcache_size
	nop
	move	a0,v0			# save dcache sizes

	jal	dcache_ln_size
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
	 * bit 1-5 of TAGLO register are zeros
	 */
	li	t0,0xffffffc0		# initial pattern

	/*
	 * Fill from tag 0 to last tag with pattern
	 */
1:
	move	a0,t7			# first addtess
	mtc0	t0,C0_TAGLO		# load TAGLO 
	nop
2:
	cache	CACH_PD | C_IST,(a0)	# store pattern to tag
	nop
	addu	a0,a0,t5		# next tag
	blt	a0,t6,2b		# reach last tag yet?
	nop

	/*
	 * Check if the parity bit is zero
	 */
	move	a1,zero			# expected parity value
	move 	a0,t7			# first address
3:
	cache	CACH_PD | C_ILT,(a0)	# load TAGLO from cache tag
	nop
	mfc0	a2,C0_TAGLO		# move TAGLO to a2
	nop
	and	a2,a2,0x00000001	# mask out the parity bit
	bne	a2,a1,error		# is it zero?
	nop
	addu	a0,a0,t5		# next tag
	blt	a0,t6,3b		# reach last tag yet?
	nop

	srl	t0,1			# shift the pattern right 1 bit
	beq	t0,0x0000001f,7f	# finish?
4:
	move	a0,t7
	mtc0	t0,C0_TAGLO		# load TAGLO 
	nop
5:
	cache	CACH_PD | C_IST,(a0)	# store pattern to tag
	nop
	addu	a0,a0,t5		# next tag
	blt	a0,t6,5b		# reach last tag yet?
	nop

	/*
	 * Check if the parity bit is one
	 */
	li	a1,0x00000001 		# expected parity value
	move 	a0,t7			# first address
6:
	cache	CACH_PD | C_ILT,(a0)	# load TAGLO from cache tag
	nop
	mfc0	a2,C0_TAGLO		# move TAGLO to a2
	nop
	and	a2,a2,0x00000001	# mask out the parity bit
	bne	a2,a1,error		# is it one?
	nop
	addu	a0,a0,t5		# next tag
	blt	a0,t6,6b		# reach last tag yet?
	nop

	srl	t0,1			# shift the pattern left 1 bit
	bne	t0,0x0000001f,1b	# finish?
	nop
7:
	mtc0	s1,C0_SR		# restore C0_SR
	nop
	move	v0,zero			# indicate success
	j	s0			# and return
error:
	mtc0	s1,C0_SR		# resetor C0_SR
	nop
	li	a3,DTAG_PAR_ERR		# error code
	j	cache_err		# jmp the error handling routine
	.set reorder

END(Dtag_parity)
