/**************************************************************************
 *									  *
 *		 Copyright (C) 1997-1998, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * This file has all the beast specific cacheops.
 */
#include "ml/ml.h"

/*
 * __dcache_inval(addr, len)
 * invalidate data cache for range of physical addr to addr+len-1
 *
 * We use the HIT_INVALIDATE cache op. Therefore, this requires
 * that 'addr'..'len' be the actual addresses which we want to
 * invalidate in the caches.
 * addr/len are not necessarily cache line aligned. Any lines
 * partially covered by addr/len are written out with the
 * HIT_WRITEBACK_INVALIDATE op.
 *
 * Invalidate lines in both the primary and secondary cache.

LEAF(__dcache_inval)
 	END(__dcache_inval)
 */
	

/* 
 * void flush_icache(void *index, unsigned int lines)
 *	a0 = index (index into icache rounded down to a cache line offset)
 *	a1 = lines (the number of cache lines to flush)
 *	NOTE: a1 is used by the code in the macros above.
 */	
LEAF(__flush_icache)
#if 0
	/*
	 * needs to be checked for correctness.
	 */
	set	.noreorder
	lw	a0, picache_size
	sll	a0, PICACH_SIZE_SHFT
	move	a1, zero
1:	
	lw	a2, PICACHE_SET - 1
	sll	t0,a2, TAGLO_SET_SHFT
2:		
	DMTC0(t0, C0_TAGLO)
	cache	CACH_PI | C_IINV, 0(a1)
	subu	a2, 1
	bge	zero, a2, 2b
	 sll	t0,a2, TAGLO_SET_SHFT

	addu	a1, CACHE_ILINE_SIZE
	ble	a1, a0, 1b
	 nop
	j	ra
	 nop
	set	.reorder
#else
	/*
	 * Die
	 */

	xor	a0, a0
	ld	a0, 0(a0)
#endif	
	END(__flush_icache)


	
/*
 * Config_cache() -- determine sizes of i and d caches
 * Sizes stored in globals picache_size, pdcache_size, icache_size
 * and dcache_size.
 * 2nd cache size stored in global boot_sidcache_size.
 */

#define MIN_PCACHE_SIZE_SHFT	12
#define MIN_SCACHE_SIZE_SHFT	19
		
LEAF(config_cache)
	.set	noreorder
	MFC0(t1, C0_SR)
	NOINTS(t0, C0_SR)

	# Size primary instruction cache.
	DMFC0(t0, C0_CONFIG)
	and	t2, t0, CONFIG_IC_MASK
	li	t3, CONFIG_IC_SHFT
	dsrlv	t2, t3
	addi	t2, MIN_PCACHE_SIZE_SHFT
	li	t3, 1
	dsllv	t3, t2
	sw	t3, picache_size

	# Size primary data cache.
	and	t2, t0, CONFIG_DC_MASK
	li	t3, CONFIG_DC_SHFT	
	dsrlv	t2, t3
	addi	t2, MIN_PCACHE_SIZE_SHFT
	li	t3, 1
	dsllv	t3, t2
	sw	t3, pdcache_size

	# Size secondary cache.
	and	t2, t0, CONFIG_ES_MASK
	li	t3, CONFIG_ES_SHFT	
	dsrlv	t2, t3
	addi	t2, MIN_SCACHE_SIZE_SHFT
	li	t3, 1
	dsllv	t3, t2

	MTC0(t1, C0_SR)
	j	ra
	 sw	t3, boot_sidcache_size
	.set	reorder	
	END(config_cache)
	
/*
 * __cache_wb_inval(addr, len)
 * 
 * Uses the INDEX_INVALIDATE and INDEX_WRITEBACK_INVALIDATE
 * cacheops. Should be called with K0 addresses, to avoid
 * the tlb translation (and tlb miss fault) overhead.
 *

LEAF(__cache_wb_inval)
	END(__cache_wb_inval)

 */
	
		
/*
 * __cache_hwb_inval_pfn(poffset, len, pfn)
 * Uses the HIT_WRITEBACK_INVALIDATE
 * cacheops. Should be called with xkphys addresses, to avoid
 * the tlb translation (and tlb miss fault) overhead.
 *
LEAF(__cache_hwb_inval_pfn)
	END(__cache_hwb_inval_pfn)

 */
	
LEAF(cacheOP)
/*
 * Routine:	cacheOP
 * Purpose:	Perform a cache operation
 * Parameters:	a0 - cacheop_t * - pointer to cache op structure.
 * Returns:	v0 - undefined, for Load data, taghi, taglo, and ECC
 *		     values filled in.
 */
	.set	noreorder
	ld	v0,COP_TAG(a0)		/* First setup CP0 */
	DMTC0(v0, C0_TAGLO)
	
	ld	a1,COP_ADDRESS(a0)		/* Address to use */
	lw	v1,COP_OP(a0)			/* Operation */
	dla	v0,cacheOP_table
		/*
		 * To ensure we access the cacheOP_table uncached during
		 * ecc errors, we pick the offset within the kernel to the
		 * cacheop table and use the significant bits from RA.
		 * Since ra could be pointing to compatibility space as well,
		 * the mask we use is COMPAT_PHYS_MASK. Since the kernel
		 * is typically within the lower 16MB, this works out fine.
		 */
	and	v0, TO_COMPAT_PHYS_MASK
	and	t0, ra, ~TO_COMPAT_PHYS_MASK
	or	v0, t0				/* Force uncached access */
	sll	v1,3				/* Index into table. */
	daddu	v0,v1
	jalr	a2,v0
	nop
	CACHE_BARRIER			# should be ok, but not perf path
	DMFC0(v0, C0_TAGLO)
	sd	v0,COP_TAG(a0)
	j	ra
	.set	reorder

        /*
         * There are 32 possible operations to perform, each is
	 * defined in the table below, and uses 2 instructions (8 bytes).
         */

cacheOP_table:
	.set	noreorder
	jr	a2
	cache	0, 0(a1)
	jr	a2
	cache	1, 0(a1)
	jr	a2
	cache	2, 0(a1)
	jr	a2
	cache	3, 0(a1)
	jr	a2
	cache	4, 0(a1)
	jr	a2
	cache	5, 0(a1)
	jr	a2
	cache	6, 0(a1)
	jr	a2
	cache	7, 0(a1)
	jr	a2
	cache	8, 0(a1)
	jr	a2
	cache	9, 0(a1)
	jr	a2
	cache	10, 0(a1)
	jr	a2
	cache	11, 0(a1)
	jr	a2
	cache	12, 0(a1)
	jr	a2
	cache	13, 0(a1)
	jr	a2
	cache	14, 0(a1)
	jr	a2
	cache	15, 0(a1)
	jr	a2
	cache	16, 0(a1)
	jr	a2
	cache	17, 0(a1)
	jr	a2
	cache	18, 0(a1)
	jr	a2
	cache	19, 0(a1)
	jr	a2
	cache	20, 0(a1)
	jr	a2
	cache	21, 0(a1)
	jr	a2
	cache	22, 0(a1)
	jr	a2
	cache	23, 0(a1)
	jr	a2
	cache	24, 0(a1)
	jr	a2
	cache	25, 0(a1)
	jr	a2
	cache	26, 0(a1)
	jr	a2
	cache	27, 0(a1)
	jr	a2
	cache	28, 0(a1)
	jr	a2
	cache	29, 0(a1)
	jr	a2
	cache	30, 0(a1)
	jr	a2
	cache	31, 0(a1)
	.set	reorder
	END(cacheOP)
	
