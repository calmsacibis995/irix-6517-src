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

#ident	"$Revision: 1.20 $"

#include "ml/ml.h"

/*
 * Cache flushing, cache sizing routines.
 *
 * The globals icache_size, dcache_size no longer refer to
 * the size of the primary i/d caches, but are the size
 * of the largest cache that must be written back/invalidated:
 * e.g. clean_dcache(K0BASE, dcache_size) for IP17 needs to refer
 * to the size of the 2nd cache. The same call for IP6 needs refer
 * to the size of the primary dcache. This allows code in other
 * directories to just use icache_size, dcache_size.
 *
 * XXX - On TFP, can dcache_size be zero?
 *
 * Sizes of specific caches are computed and stored in variables such
 * as picache_size, sdcache_size etc. These sizes are used in the
 * add_to_inventory() calls
 */

/*
 * Config_cache() -- determine sizes of i and d caches
 * Sizes stored in globals picache_size, pdcache_size, icache_size
 * and dcache_size.
 * 2nd cache size stored in global boot_sidcache_size.
 * Determines size of secondary cache - assumes 2nd cache is
 * data or unified I+D.
 * Can be extended to look for 2nd instruction
 * cache by reading the config register. By definition, if 2nd
 * cache is present and 'split', then both secondary caches are
 * the same size.
 */

#define	MIN_CACH_POW2	12
#define	SCACHE_LINEMASK	(512-1)

CFGLOCALSZ=	2			# Save ra, s0
CFGFRM=		FRAMESZ((NARGSAVE+CFGLOCALSZ)*SZREG)
CFGRAOFF=	CFGFRM-(1*SZREG)
CFGSROFF=	CFGFRM-(2*SZREG)

NESTED(config_cache, CFGFRM, zero)
	PTR_SUBU sp,CFGFRM
	REG_S	ra,CFGRAOFF(sp)
	.set    noreorder
	DMFC0(t0,C0_SR)			# save SR
	REG_S	t0,CFGSROFF(sp)	# save s0 on stack
	NOINTS(t0,C0_SR)		# disable interrupts

	# Size primary instruction cache.
	DMFC0(t0,C0_CONFIG)
	and	t1,t0,CONFIG_IC
	srl	t1,CONFIG_IC_SHFT
	/* TFP encodes chip revision number in IC field for some revs of
	 * chip.
	 */
	DMFC0(t2,C0_PRID)
	andi	t2, t2, 0xff
	bne	t2, zero, 1f		# if revision non-zero, no encoding
	nop

	/* We've hit encoded case (PRID == 0)
	 */
	beq	t1, zero, 1f		# let encode of zero
	li	t1, 0x04		#     be a 32K icache
	li	t1, 0x03		# all others be 16K icache

	.set reorder
1:	addi	t1,MIN_CACH_POW2-1	# IC size has lower MIN than DC
	li	t2,1
	sll	t2,t1
	sw	t2,picache_size

	# Size primary data cache.
	and	t1,t0,CONFIG_DC
	srl	t1,CONFIG_DC_SHFT
	addi	t1,MIN_CACH_POW2
	li	t2,1
	sll	t2,t1
	sw	t2,pdcache_size

	li	t2,SCACHE_LINEMASK
	sw	t2,scache_linemask

	jal	size_2nd_cache

	# v0 has the size of the secondary (data or unified) cache.

	sw	v0,boot_sidcache_size
	sw	v0,icache_size
	sw	v0,dcache_size

	.set	noreorder
1:	REG_L	t0,CFGSROFF(sp)	# restore old s0
	DMTC0(t0,C0_SR)		# restore SR
	.set	reorder
	REG_L	ra,CFGRAOFF(sp)
	PTR_ADDU sp,CFGFRM
	j	ra
	END(config_cache)


/* Return size of current cpu's secondary cache.  For EVEREST systems the
 * cache has already been sized by the prom, so we simply retrieve the
 * value stored by the prom.
 */
#ifdef EVEREST
NESTED(size_2nd_cache, CFGFRM, zero)
	PTR_SUBU sp,CFGFRM
	REG_S	ra,CFGRAOFF(sp)

	jal	getcpuid		# get current cpu id
	move	a0,v0
	jal	getcachesz		# then retrieve secondary cache size

	REG_L	ra,CFGRAOFF(sp)
	PTR_ADDU sp,CFGFRM
	j	ra
	END(size_2nd_cache)
#endif

#define ICACHE_LINE_CODE				\
	addi	a1, -1;			/* 0 */		\
	addu	zero, zero, zero;	/* 4 */		\
	addu	zero, zero, zero;	/* 8 */		\
	addu	zero, zero, zero;	/* 12 */	\
	beqz	a1, get_out;		/* 16 */	\
	addu	zero, zero, zero;	/* 20 */	\
	addu	zero, zero, zero;	/* 24 */	\
	addu	zero, zero, zero	/* 28 */

/* Four icache lines per gcache line (128 bytes) */
#define FOUR_LINES_CODE		\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE	\

/* 32 icache lines per 1k */
#define THIRTYTWO_LINES_CODE			\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE	\

/* 
 * void tfp_flush_icache(void *index, unsigned int lines)
 *	a0 = index (index into icache rounded down to a cache line offset)
 *	a1 = lines (the number of cache lines to flush)
 *	NOTE: a1 is used by the code in the macros above.
 */
LEAF(tfp_flush_icache)
#ifdef SABLE
	li	t1, ICACHE_SIZE
#else
	LA	t1, picache_size
	lw	t1, 0(t1)			# Get the cache size

#endif

	LA	t0, code_start
	addi	t2, t1, -1			# t2 = icachesize - 1
	PTR_ADD	t0, t2				# t0 = code_start+icache_size-1
	not	t2				# t2 = ~(icachesize - 1)
	and	t0, t2				# t0 = address within the block
						# of icache "nops" that has
						# icache index 0.
	PTR_ADD	a0, t0				# Address to jump to to
						# flush the appropriate
						# icache lines
	# if a0 >= (code_start + icachesize), 
	# a0 = a0 - icachesize

	LA	t0, code_start	
	PTR_SUB	t2, a0, t0			# t2 = a0 - code_start
	PTR_SUB	t2, t1				# t2 = a0-code_start-icachesize
	
	bltz	t2, 1f				# a0 < code_start + icachesize?

	PTR_SUB	a0, t1				# a0 = a0 - icachesize
	
1:
	j	a0

	/*
	 * This is 16k of code that basically does nothing.
	 * It's aligned on a gcache line boundary.	
	 * The code keeps going around until a1 == 0.
	 * For details, see ICACHE_LINE_CODE above.
	 */

	.set noreorder
	nop

	.align 7
code_start:
#if (ICACHE_SIZE == 16384)
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 2k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 4k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 6k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 8k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 10k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 12k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 14k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 16k */
	b	code_start
	nop

	.set reorder
	
#else
	Code needs to be rewritten to support larger icache sizes.
#endif /* (ICACHE_SIZE == 16384) */

get_out:
	j	ra
	END(tfp_flush_icache)

#if !IP26	/* tcc has it's own cache ops */
/*
 * __dcache_inval(addr, len)
 * No need to do this since the cache is write-through.
 */
LEAF(__dcache_inval)
	j	ra
	END(__dcache_inval)
#endif	/* !IP26 */

	.data
lmsg:	.asciiz	"cache.s"
