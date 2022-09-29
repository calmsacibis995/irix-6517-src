#ident	"$Id: i_cacheinit.s,v 1.1 1994/07/21 01:25:10 davidl Exp $"
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
/*		i n i t _ l o a d _i c a c h e
 *
 * init_load_icache- Initialize the primary instruction cache from 
 * the power-up state
 */
LEAF(init_load_icache)

	move	t4, ra
	/* Get the icache size */
	jal	icache_size
	nop
	move	a0, v0
	/* Get the icache linesize */
	jal	icache_ln_size
	nop

	# to initialize cache, we have to load every line up to
	# cache size.
	# if the scache exists, it has already been 'loaded' in
	# this way.
	# use the 'fill' cache op to fill from scache/memory.
	#
	# a0: cache size, v0: line size
	li	t1,K0_RAMBASE
	addu	a0,t1			# a0 = K0base + cache size
1:
	.set	noreorder
	cache	CACH_PI|C_FILL,0(t1)
	.set	reorder
	addu	t1,v0			# step the address by line size
	bltu	t1,a0,1b

	j	t4
	END(init_load_icache)


/*		i c a c h e _ s i z e ()
 *
 * int icache_size()
 *	return the size of icache by calculating the icache size
 *	from the IC field of the config register.
 */

LEAF(icache_size)

	.set noreorder
	mfc0	t0,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t0,CONFIG_IC		# mask out the IC field
	srl	t0,CONFIG_IC_SHFT
	li	v0,0x1000		# I-cache size=2^(12+IC) bytes
	sll	v0,t0
#ifdef SABLE
	li	v0,0x60			# use 96 byte cache in sable debug
#endif SABLE
	j	ra
END(icache_size)



/*		i c a c h e _ l n _ s i z e ()
 *
 * int icache_ln_size()
 *	return the line size of icache by reading the IB field of 
 *	the config register.
 */

LEAF(icache_ln_size)

	.set noreorder
	mfc0	t0,C0_CONFIG		# get the config reg from c0
	nop
	.set reorder

	and	t0,CONFIG_IB		# mask out the IB field
	beqz	t0,1f
	li	v0,0x20			# icache line size is 32 byte
	j	ra
1:
	li	v0,0x10			# icache line size is 16 byte
	j	ra
END(icache_ln_size)



/*		i c a c h e _ i n i t ()
 *
 * icache_init - initializes the primary icache to a valid state. Has the
 * size effect of initializing the secondary cache.
 */
LEAF(icache_init)
	.set noreorder
	move	s3, ra			# save return address

	mfc0	s2,C0_SR		# save C0_SR
	nop
	mtc0	zero,C0_CAUSE		# clear all pending intr and
	nop				# exception code

	jal	icache_size
	nop
	jal	icache_ln_size
	move	a0,v0			# save icache size

	/* Zero the tags */
	jal	invalidate_icache
	move	a1,v0			# icache line size

	/* 
	 * Go init the secondary cache, if one exists
	 */
	jal	noSCache
	nop
	bne	v0, zero, 1f
	nop

	/* init the dcache and secondary cache */
	jal	dcache_init
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

	/*
	 * load the taglo and taghi register with zero
	 */

3:
	move	t0,s2
	and	t0,~SR_IE		# disable all interrupt during
	mtc0	t0,C0_SR		#  cache initialization.
	nop

	jal	icache_size
	nop
	jal	icache_ln_size
	move	a0,v0			# save icache size
	move	a1, v0
	/*
	 * Init (Fill) the icache data array with zero from memory
	 */
	li	t0,K0BASE
	add	a3, a0, t0
1:
	cache	CACH_PI | C_FILL,0(t0) 
	nop
	addu	t0,a1			# increment by line size
	bltu	t0,a3,1b
	nop

	/*
	 * Init the tag of the icache again to set state to Inv
	 */
	jal	invalidate_icache
	nop

	move	v0,zero
	mtc0	s2,C0_SR		# restore C0_SR
	nop

	j	s3
	nop
	.set	reorder
END(icache_init)
