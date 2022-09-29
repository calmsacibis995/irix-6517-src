#ident	"$Id: d_cacheinit.s,v 1.1 1994/07/21 01:24:25 davidl Exp $"
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
#include "monitor.h"

#define PHYS_RAMBASE            0x08000000
#define K0_RAMBASE              X_TO_K0(PHYS_RAMBASE)


/*
 *		i n i t _ l o a d _ d c a c h e ()
 *
 * init_load_dcache()- initializes the data cache data and parity array
 * from memory. This routine assumes memory has been initialized
 */
LEAF(init_load_dcache)
	move	t4, ra

	/* Get the dcache size */
	jal	dcache_size
	nop
	move	a0, v0
	/* Get the line size */
	jal	dcache_ln_size
	nop

	# to initialize cache, we have to load every line up to
	# cache size.
	# if the scache exists, it has already been 'loaded' in
	# this way.
	#
	# a0: cache size, v0: line size
	li	t1,K0_RAMBASE
	addu	a0,t1			# a0 = K0base + cache size
1:
	lw	zero,0(t1)
	addu	t1,v0			# step the address by line size
	bltu	t1,a0,1b

	j	t4
END(init_load_dcache)



/*		d c a c h e _ s i z e ( )
 *
 * int dcache_size()
 *	return the size of dcache by calculating the dcache size
 *	from the DC field of the config register.
 */
LEAF(dcache_size)

	.set noreorder
	mfc0	t0,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t0,CONFIG_DC		# mask out the DC field
	srl	t0,CONFIG_DC_SHFT
	li	v0,0x1000		# I-cache size=2^(12+DC) bytes
	sll	v0,t0
#ifdef SABLE
	li	v0,0x60			# use 96 byte cache in sable debug
#endif SABLE
	j	ra
END(dcache_size)



/*		d c a c h e _ l n _ s i z e ( )
 *
 * int dcache_ln_size()
 *	return the line size of dcache by reading the DB field of 
 *	the config register.
 */

LEAF(dcache_ln_size)

	.set noreorder
	mfc0	t0,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t0,CONFIG_DB		# mask out the DB field
	beqz	t0,1f
	li	v0,0x20			# dcache line size is 32 byte
	j	ra
1:
	li	v0,0x10			# dcache line size is 16 byte
	j	ra
END(dcache_ln_size)


/*		i n v a l i d a t e _ d c a c h e ( )
 *
 * invalidate_dcache- zeros the primary data tags, thus invalidating them.
 */
LEAF(invalidate_dcache)
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	li	t0,K0_RAMBASE
	addu	a0,t0		# a0: cache size + K0base for loop termination
	nop	
	.set	reorder
1:
	.set	noreorder
	cache	CACH_PD|C_IST,0(t0)
	.set	reorder
	addu	t0,a1
	bltu	t0,a0,1b		# a0 is termination count

	j	ra
END(invalidate_dcache)


/*		d c a c h e _ i n i t ()
 *
 * dcache_init- initializes the data cache to a valid state. Has the side
 * effect of initializing the secondary cache.
 */
LEAF(dcache_init)
	.set noreorder
	move	s7, ra			# save return address

	mfc0	s6,C0_SR		# save C0_SR
	nop
	mtc0	zero,C0_CAUSE		# clear all pending intr and
	nop				# exception code

	/*
	 * Init the tag of the dcache from the taglo register
	 */
	jal	dcache_size
	nop
	move	a0,v0			# save dcache size
	jal	dcache_ln_size
	nop
	move	a1, v0
	jal	invalidate_dcache
	nop

	/* Check for a secondary cache */
	jal	noSCache
	nop
	bne	v0, zero, 1f
	nop

	/* Init secondary first, note scache_init zero memory */
	jal	scache_init
	nop
	b	3f
	/*
	 * init the low memory to zero
	 */
1:
	li	t0,0
2:
	sw	zero,K1BASE(t0)
	addiu	t0,4
	bltu	t0,a1,2b
	nop

	move	a0,s6
	and	a0,~SR_IE		# disable all interrupt during
	mtc0	a0,C0_SR		#  cache initialization.
	nop


	/*
	 * Init the dcache data array from memory
	 */
	jal	init_load_dcache
	nop

3:
	/*
	 * Init the tag of the dcache again so that the state is Inv
	 */
	jal	dcache_size
	nop
	jal	dcache_ln_size
	move	a0,v0			# save dcache size
	jal	invalidate_dcache
	move	a1, v0

	move	v0,zero
	mtc0	s6,C0_SR		# restore C0_SR
	nop

	j	s7
	nop
	.set reorder
END(dcache_init)

/* end */
