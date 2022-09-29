#ident	"$Id: dkseg01_func.s,v 1.1 1994/07/21 01:25:00 davidl Exp $"
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
#include "cache_err.h"


/*
 * int
 * Dkseg01_func()
 *
 *	Verify the cache work properly in the kseg0 and kseg1 area.
 *
 * inputs:
 *	none
 *
 */

#define K0BASE_ADDR	0x88000000
#define K1BASE_ADDR	0xa8000000
LEAF(Dkseg01_func)

	move	s0,ra			# save the return address


	jal	inv_dcache # init the dcache and scache if present

	jal	dcache_size		# get cache size
	move	s1,v0			# save cache size

	/*
	 * Fill cache from address K0BASE with 0xaaaaaaaa.
	 * This should change the cache state to dirty exclusive
	 */
	la	a0,K0BASE_ADDR
	srl	a1,s1,2			# convert cache size to word count
	li	a2,0xaaaaaaaa		# pattern to cache

	jal	WriteDcacheW		# write the cache

	/*
	 * Write memory from K1BASE to K1BASE+cache_size
	 * behind cache with 0x55555555
	 */
	la	a0,K1BASE_ADDR		# kseg1
	srl	a1,s1,2			# convert cache size to word count
	li	a2,0x55555555		# pattern to memory

	jal	WriteMemW		# write to memory

	/*
	 * Check if the kseg1 addresses affects the kseg0 addresses. All
	 * cache read should be a hit and no refill from memory should occur.
	 */
	la	a0,K0BASE_ADDR
	srl	a1,s1,2			# convert cache size to word count
	li	a2,0xaaaaaaaa		# pattern to check
	jal	CheckDcacheW		# check if cache is affected
	li	a3,K0MOD_ERR		# in case of error
	bne	v0,zero,error

	/*
	 * write memory from K1BASE+cache_size to K1BASE+2*cache_size
	 * behind cache with 0x33333333. This should alias the primary
	 * cache, if a secondary cache exists use the alias for the secondary
	 * cache.
	 */
	la	a0,K1BASE_ADDR
#ifdef SABLE
	addiu	a0,a0,0x2000
#else SABLE
	jal	noSCache		# check for a secondary cache
	bne	v0, zero, 1f		# Branch if no secondary cache
	jal	size_2nd_cache		# else get the secondary cache size
	move	s2, v0			# save 2nd cache size
	addu	a0, a0, v0		# This will alias dcache/scache
	b	2f
1:
	addu	a0,a0,s1		# get K1BASE+cache_size
2:
#endif SABLE
	srl	a1,s1,2			# convert cache size to word count
	li	a2,0x33333333
	jal	WriteMemW		# fill memory

	/*
	 * Fill cache from address K0BASE+cache_size with 0xcccccccc. The
	 * cache lines should be written back to memory first.
	 */
	la	a0, K0BASE_ADDR
#ifdef SABLE
	addiu	a0,a0,0x2000
#else
	jal	noSCache
	bne	v0, zero, 1f
	addu	a0, a0, s2		# Use size of secondary cache so that
	b	2f			# we force a writeback there also
1:
	addu	a0,a0,s1		# get K0BASE+cache_size
2:
#endif SABLE
	srl	a1,s1,2			# get word count
	li	a2,0xcccccccc
	move	s7, a0
	jal	WriteDcacheW		# fill the cache

	/*
	 * Check if the cache lines are written back to memory
	 */
	la	a0,K1BASE_ADDR
	srl	a1,s1,2			# convert cache size to word count
	li	a2,0xaaaaaaaa
	jal	CheckMemW		# check if write back occured
	li	a3,WBACK_ERR		# in case of error
	bne	v0,zero,error

	/*
	 * Check if kseg1 addresses affect by kseg2 addresses when cache
	 * hit occurs
	 */
	la	a0,K1BASE_ADDR
#ifdef SABLE
	addiu	a0,a0,0x2000
#else
	jal	noSCache
	bne	v0, zero, 1f
	addu	a0, a0, s2		# Use size of secondary cache so that
	b	2f			# we force a writeback there also
1:
	addu	a0,a0,s1		# get K0BASE+cache_size
2:
#endif SABLE
	srl	a1,s1,2			# convert cache size to word count
	li	a2,0x33333333
	jal	CheckMemW		# check memory is affected by hit
	li	a3,K1MOD_ERR
	bne	v0,zero,error

	/*
	 * Check if cache is loaded from memory when cache miss in a read
	 */
	la	a0,K0BASE_ADDR
	srl	a1,s1,2			# convert cache size to word count
	li	a2,0xaaaaaaaa		# pattern to check
	jal	CheckDcacheW		# check if cache is loaded from mem
	li	a3,FILL_ERR
	bne	v0,zero,error

	move	v0,zero			# success return
	j	s0

error:
	j	cache_err
END(Dkseg01_func)

/*
 * inv_dcache()
 *	invalidate the data cache.
 *
 * register usage:
 */
LEAF(inv_dcache)

	.set noreorder
	move	s7,ra			# save return addr

	jal	dcache_size		# get cache size
	nop
	la	a0,K0BASE
	addu	a1,a0,v0		# get the last addr

	jal	dcache_ln_size		# get d cache line size
	nop
	move	a2,v0

	mtc0	zero,C0_TAGLO		# load TAGLO with 0's
	nop
1:
	cache	CACH_PD | C_IST,(a0)	# store the tag to cache
	addu	a0,a0,a2		# increment by line size
	bltu	a0,a1,1b
	nop
	j	s7
	nop
	.set reorder
END(inv_dcache)
