#ident	"$Id: cache_init.s,v 1.1 1994/07/21 01:24:12 davidl Exp $"
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


#include "machine/regdef.h"
#include "machine/asm.h"
#include "/usr1/5.1/irix/kern/sys/sbd.h"
#include "machine/cpu.h"

#include "pdiag.h"
#include "monitor.h"

	.text
#define PHYS_RAMBASE            0x08000000
#define K0_RAMBASE              X_TO_K0(PHYS_RAMBASE)
#define MINCACHE        (128*1024)
#define MAXCACHE        (4*1024*1024)



LEAF(icache_init)
	.set noreorder
	move	s7, ra			# save return address

	mfc0	s6,C0_SR		# save C0_SR
	nop
	mtc0	zero,C0_CAUSE		# clear all pending intr and
	nop				# exception code

	jal	icache_size
	nop
	move	a1,v0			# save icache size

	jal	icache_ln_size
	nop
	move	a2,v0			# icache line size

	/*
	 * init the low memory to zero
	 */
	li	t0,0
1:
	sw	zero,K1BASE(t0)
	addiu	t0,4
	bltu	t0,a1,1b
	nop

	/*
	 * load the taglo and taghi register with zero
	 */
	li	t0, 0
	mtc0	t0,C0_TAGLO
	nop
	mtc0	t0,C0_TAGHI
	nop

	move	a0,s6
	and	a0,~SR_IE		# disable all interrupt during
	mtc0	a0,C0_SR		#  cache initialization.
	nop

	/*
	 * Init the tag of the icache from the taglo register
	 */
	li	t0, K0BASE		# beginning line addr
	addu	a3,a1,t0		# ending line addr
1:
	cache	Index_Store_Tag_I,0(t0) 
	nop
	addu	t0,a2			# increment by line size
	bltu	t0,a3,1b
	nop

	/*
	 * Init (Fill) the icache data array with zero from memory
	 */
	li	t0,K0BASE
1:
	cache	Fill_I,0(t0) 
	nop
	addu	t0,a2			# increment by line size
	bltu	t0,a3,1b
	nop

	/*
	 * Init the tag of the icache again to set state to Inv
	 */
	li	t0, K0BASE		# beginning line addr
	addu	a3,a1,t0		# ending line addr
1:
	cache	Index_Store_Tag_I,0(t0) 
	nop
	addu	t0,a2			# increment by line size
	bltu	t0,a3,1b
	nop

	move	v0,zero
	mtc0	s6,C0_SR		# restore C0_SR
	nop

	j	s7
	nop
	.set	reorder
END(icache_init)


LEAF(dcache_init)
	.set noreorder
	move	s7, ra			# save return address

	mfc0	s6,C0_SR		# save C0_SR
	nop
	mtc0	zero,C0_CAUSE		# clear all pending intr and
	nop				# exception code

	/*
	 * init the low memory to zero
	 */
	li	t0,0
1:
	sw	zero,K1BASE(t0)
	addiu	t0,4
	bltu	t0,a1,1b
	nop

	move	a0,s6
	and	a0,~SR_IE		# disable all interrupt during
	mtc0	a0,C0_SR		#  cache initialization.
	nop

	/*
	 * Init the tag of the dcache from the taglo register
	 */
	jal	dcache_size
	nop
	move	a0,v0			# save dcache size
	jal	invalidate_dcache
	nop

	/*
	 * Init the dcache data array from memory
	 */
	jal	init_load_dcache
	nop

	/*
	 * Init the tag of the dcache again so that the state is Inv
	 */
	jal	dcache_size
	nop
	move	a0,v0			# save dcache size
	jal	invalidate_dcache
	nop

	move	v0,zero
	mtc0	s6,C0_SR		# restore C0_SR
	nop

	j	s7
	nop
	.set reorder
END(dcache_init)


/*
 * int icache_size()
 *	return the size of icache by calculating the icache size
 *	from the IC field of the config register.
 */

LEAF(icache_size)

	.set noreorder
	mfc0	t7,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t7,CONFIG_IC		# mask out the IC field
	srl	t7,CONFIG_IC_SHIFT
	li	v0,0x1000		# I-cache size=2^(12+IC) bytes
	sll	v0,t7
#ifdef SABLE
	li	v0,0x60			# use 96 byte cache in sable debug
#endif SABLE
	j	ra
END(icache_size)


/*
 * int dcache_size()
 *	return the size of dcache by calculating the dcache size
 *	from the DC field of the config register.
 */

LEAF(dcache_size)

	.set noreorder
	mfc0	t7,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t7,CONFIG_DC		# mask out the DC field
	srl	t7,CONFIG_DC_SHIFT
	li	v0,0x1000		# I-cache size=2^(12+DC) bytes
	sll	v0,t7
#ifdef SABLE
	li	v0,0x60			# use 96 byte cache in sable debug
#endif SABLE
	j	ra
END(dcache_size)


/*
 * int icache_ln_size()
 *	return the line size of icache by reading the IB field of 
 *	the config register.
 */

LEAF(icache_ln_size)

	.set noreorder
	mfc0	t7,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t7,CONFIG_IB		# mask out the IB field
	beqz	t7,1f
	li	v0,0x20			# icache line size is 32 byte
	j	ra
1:
	li	v0,0x10			# icache line size is 16 byte
	j	ra
END(icache_ln_size)


/*
 * int dcache_ln_size()
 *	return the line size of dcache by reading the DB field of 
 *	the config register.
 */

LEAF(dcache_ln_size)

	.set noreorder
	mfc0	t7,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t7,CONFIG_DB		# mask out the DB field
	beqz	t7,1f
	li	v0,0x20			# dcache line size is 32 byte
	j	ra
1:
	li	v0,0x10			# dcache line size is 16 byte
	j	ra
END(dcache_ln_size)
/*
 * 		s c a c h e _ i n i t ()
 *
 * scache_init() - Initializes the secondary cache data, parity, and tags
 * to a known state.
 */
LEAF(scache_init)
	.set noreorder
	move	s5, ra			# save return address

	mfc0	s4,C0_SR		# save C0_SR
	nop
	mtc0	zero,C0_CAUSE		# clear all pending intr and
	nop				# exception code


	/* 
	 * Get the secondary cache size and linesize
	 */
	jal	size_2nd_cache
	nop
	move	a0, v0
	jal	scache_ln_size
	nop
	move	a1, v0

	/*
	 * init the low memory to zero
	 */
	li	t0,0
1:
	sw	zero,K1BASE(t0)
	addiu	t0,4
	bltu	t0,a0,1b
	nop

	move	t0,s4
	and	t0,~SR_IE		# disable all interrupt during
	mtc0	t0,C0_SR		#  cache initialization.
	nop

	/*
	 * Init the tag of the dcache from the taglo register
	 */
	jal	invalidate_scache
	nop

	/*
	 * Init the dcache data array from memory
	 */
	jal	init_load_scache
	nop

	/*
	 * Init the tag of the dcache again so that the state is Inv
	 */
	jal	invalidate_scache
	nop

	move	v0,zero
	mtc0	s4,C0_SR		# restore C0_SR
	nop

	j	s5
	nop
	.set reorder
END(scache_init)

