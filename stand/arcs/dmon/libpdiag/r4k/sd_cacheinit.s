#ident	"$Id: sd_cacheinit.s,v 1.1 1994/07/21 01:25:55 davidl Exp $"
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
/* Initialize the secondary cache from the power-up state */
LEAF(init_load_scache)
	move	s1, ra
	/* Determine the secondary size */
	jal	size_2nd_cache
	nop
	move	a0, v0
	li	a0, 0x00100000
	/* Get the line size */
	jal	scache_ln_size
	nop

	# 0x80 PUTHEX(v0)
	# to initialize cache, we have to load every line up to
	# cache size
	#
	# a0: cache size, v0: line size
	li	t1,K0_RAMBASE
	addu	a0,t1			# a0 = K0base + cache size
1:
	lw	zero,0(t1)
	addu	t1,v0			# step the address by line size
	sltu	AT, t1, a0
	bne	AT, zero, 1b
	#bltu	t1,a0,1b

	j	s1
	END(init_load_scache)
/*
 * size_2nd_cache()
 * return size of current data cache
 * called while running in uncached space.
 *
 * sizing secondary cache: (assumes running uncached, 2nd cache is
 * a data or unified cache)
 * 1: Load up the cache in powers of 2, from MINCACHE to MAXCACHE. Then
 * each of these lines guarenteed to be valid (may cycle around the cache.)
 * 2. do an index store tag zero to line 0 - this tag is guaranteed to be
 * invalid for any address we use.
 * 3. Starting at MINCACHE, do an index load tag. Continue until find the
 * zero tag.
 */
LEAF(size_2nd_cache)
	move	v0,zero

	# First check if there is a cache there at all

	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	nop
	.set	reorder
	and	t0,CONFIG_SC		# 0 -> 2nd cache present
	beq	t0,zero,1f
	j	ra			# No second cache
1:
	li	v0,MINCACHE		# If MINCACHE == MAXCACHE, just
	li	t0,MAXCACHE		# return that
	bne	t0,v0,1f
	j	ra
1:
	# Load valid tags at each cache boundary up to MAXCACHE.
	# v0 has MINCACHE value.
1:
	lw	zero,K0_RAMBASE(v0)
	sll	v0,1
	ble	v0,+MAXCACHE,1b

	# Invalidate cache tag at 0th index in all caches.
	# Invalidating the primaries insures that we do not
	# create a non-subset of the secondary cache in
	# either of the primary caches.
	.set	noreorder
	.set	noat
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	li	t0,K0_RAMBASE	# LDSLOT
	li	v0,MINCACHE		# LDSLOT
	cache	CACH_PI|C_IST,0(t0)
	cache	CACH_PD|C_IST,0(t0)
	cache	CACH_SD|C_IST,0(t0)
	.set	reorder
	.set	at
	# Now loop until that tag is found
	# v0 - MINCACHE
1:
	li	t0,K0_RAMBASE	# Reload K0BASE for each iteration.
	addu	t0,v0
	.set	noreorder
	.set	noat
	cache	CACH_SD|C_ILT,0(t0)
	nop
	nop
	nop
	mfc0	t1,C0_TAGLO
	.set	reorder
	.set	at
	beq	t1,zero,2f		# Found the marker
	sll	v0,1			# Next power of 2
	blt	v0,+MAXCACHE,1b		# Iterate until MAXCACHE
2:
	j	ra
END(size_2nd_cache)

/*
 * scache_ln_size() - Returns the secondary cache linesize
 */
LEAF(scache_ln_size)
	.set	noreorder
	mfc0	t0, C0_CONFIG
	nop
	and	t1, t0, CONFIG_SB
	srl	t1, t1, CONFIG_SB_SHFT
	li	v0, 16
	nop
	sll	v0, t1
	j	ra
	nop
	.set	reorder
END(scache_ln_size)
LEAF(invalidate_scache)
	# 2nd cache not guaranteed to exist - just return if not present
	bne	a0,zero,1f
	j	ra
1:
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	li	t0,K0_RAMBASE
	addu	a0,t0		# a0: cache size + K0base for loop termination
	nop
	.set	reorder
1:
	.set	noreorder
	cache	CACH_SD|C_IST,0(t0)
	.set	reorder
	addu	t0,a1
	bltu	t0,a0,1b		# a0 is termination count

	j	ra
	END(invalidate_scache)
