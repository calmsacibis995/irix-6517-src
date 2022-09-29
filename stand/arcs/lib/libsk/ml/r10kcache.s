/*
 * File: r10kcache.s
 * Purpose: r10000 (T5) cache manipulation primitives.
 */

#ident	"$Revision: 1.30 $"

#include <asm.h>
#include <regdef.h>
#include <R10kcache.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <ml.h>

/*
 * Define:	K0_CACHEFLUSHBASE
 * Purpose:	The base address used to start flushing the 
 *		cache.
 */	 
#if IP28 || IP30
#define	K0_CACHEFLUSHBASE	K0_RAMBASE
#else
#define	K0_CACHEFLUSHBASE	K0BASE
#endif	/* IP28 || IP30 */

#ifdef IP28
/* use prom to init cache as it is done before memory init (T5 seems to
 * turn fills into misses sometimes on pacecar).
 */
#define CACHE_INIT_BASE		PHYS_TO_K0(0x1fc00000)
#else
#define CACHE_INIT_BASE		K0_CACHEFLUSHBASE
#endif



/*
 * Function:	sCacheSize
 * Purpose:	determine the size of the secondary cache
 * Returns:	secondary cache size in bytes in v0
 */
LEAF(sCacheSize)
XLEAF(size_2nd_cache)
        .set	noreorder
	mfc0	v1,C0_CONFIG
	and	v1,CONFIG_SS
	dsrl	v1,CONFIG_SS_SHFT
	dadd	v1,CONFIG_SCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
        END(sCacheSize)



/*
 * Function:	iCacheSize
 * Purpose:	determine the size of the primary instruction cache
 * Returns:	primary cache size in bytes in v0
 */
LEAF(iCacheSize)
	.set	noreorder
	mfc0	v1,C0_CONFIG
	and	v1,CONFIG_IC
	dsrl	v1,CONFIG_IC_SHFT
	dadd	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
        END(iCacheSize)



/*
 * Function:	dCacheSize
 * Purpose:	determine the size of the primary data cache
 * Returns:	primary data cache size in bytes in v0
 */
LEAF(dCacheSize)
        .set	noreorder
	mfc0	v1,C0_CONFIG
	and	v1,CONFIG_DC
	dsrl	v1,CONFIG_DC_SHFT	
	dadd	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
        END(dCacheSize)



/*
 * Function:	config_cache
 * Purpose:	determine cache sizes and set global variables
 */
CONFIG_LOCALSZ = 1			# save ra only
CONFIG_FRAMESZ =(((NARGSAVE + CONFIG_LOCALSZ) * SZREG) + ALSZ) & ALMASK
CONFIG_RAOFF = CONFIG_FRAMESZ - (1 * SZREG)
NESTED(config_cache, CONFIG_FRAMESZ, zero)
	PTR_SUBU	sp,CONFIG_FRAMESZ
	REG_S	ra,CONFIG_RAOFF(sp)

	/* secondary cache size */
	jal	sCacheSize
	sw	v0,_sidcache_size
	li	a0,CACHE_SLINE_SIZE
	sw	a0,_scache_linesize
	li	a0,CACHE_SLINE_SIZE-1
	sw	a0,_scache_linemask
	LONG_S	a0,dmabuf_linemask

	/* primary instruction cache */
	jal	iCacheSize
	sw	v0,_icache_size
	li	a0,CACHE_ILINE_SIZE
	sw	a0,_icache_linesize
	li	a0,CACHE_ILINE_SIZE-1
	sw	a0,_icache_linemask

	/* primary data cache */
	jal	dCacheSize
	sw	v0,_dcache_size
	li	a0,CACHE_DLINE_SIZE
	sw	a0,_dcache_linesize
	li	a0,CACHE_DLINE_SIZE-1
	sw	a0,_dcache_linemask

	REG_L	ra,CONFIG_RAOFF(sp)
	PTR_ADDU	sp,CONFIG_FRAMESZ

	j	ra
	END(config_cache)

#if !defined (SN0)	
/*
 * Function:	FlushAllCaches
 * Purpose:	writeback and invalidate all caches
 */
LEAF(flush_cache)
#if !EVEREST
/* FlushAllCaches is defined in EVEREST/SN0.c.  flush_cache gets called
 * ultimately
 */
XLEAF(FlushAllCaches)
XLEAF(_FlushAllCaches)
#endif
	dli	a0,K0_CACHEFLUSHBASE
	lw	a1,_sidcache_size
	srl	a1,1				# x/2 -> two way assoc.

	j	__cache_wb_inval	
	END(flush_cache)


#define	TO_REGION_SHFT		40
#define	TO_COMPAT_REGION_SHFT	29

	
/*
 * Function:	clear_cache
 * Purpose:	writeback and invalidate cache lines INDEXED
 *		by the given addresses.
 * Parameters: 	a0 - starting address, must be cached and unmapped
 *		a1 - number of bytes
 */


LEAF(__cache_wb_inval)
XLEAF(clear_cache)
	dsrl	t0,a0,TO_REGION_SHFT		# get cache attribute

	li	t1,K0BASE>>TO_REGION_SHFT	# cached exclusive on write
	beq	t0,t1,1f
	li	t1,K0BASE_NONCOH>>TO_REGION_SHFT	# cached noncoherent
	beq	t0,t1,1f
	li	t1,K0BASE_EXL>>TO_REGION_SHFT	# cached exclusive
	beq	t0,t1,1f

	/* check for compatibility cached space */
	dsra	t0,a0,TO_COMPAT_REGION_SHFT
	li	t1,0xfffffffc
	beq	t0,t1,1f

	j	ra			# do nothing if uncached address
1:
	PTR_ADDU	a1,a0		# last address

	/*
	 * flush the secondary cache with index writeback invalidate,
	 * both ways. a0 has start address, a1, end address. align
	 * addresses on secondary cache line
	 */
	and	a0,CACHE_SLINE_MASK
	PTR_ADDU	a1,CACHE_SLINE_SIZE-1	# round up to next line
	and	a1,CACHE_SLINE_MASK

	.set	noreorder

	/* R10000 only checks ptag for primary caches, so also hit them
	 * explicitly as this is an indexed operation.
	 */
	lw	t2,_dcache_size		# cache size (isize == dsize)
	move	t0,a0			# base addr
	move	t1,a1			# base + length
	srl	t2,1			# 2 way assoc
	PTR_ADDU ta0,t0,t2		# addr + cache size
	bltu	t1,ta0,2f		# len > cachesize?
	nop				# BDSLOT
	move	t1,ta0			# cap primary loop @ cachesize
2:
	PTR_ADDU	t0,CACHE_ILINE_SIZE
	cache	CACH_PD|C_IWBINV,-CACHE_DLINE_SIZE(t0)		# dcache 1 way 0
	cache	CACH_PD|C_IWBINV,-CACHE_DLINE_SIZE+1(t0)	# dcache 1 way 1
	cache	CACH_PD|C_IWBINV,-(2*CACHE_DLINE_SIZE)(t0)	# dcache 2 way 0
	cache	CACH_PD|C_IWBINV,-(2*CACHE_DLINE_SIZE)+1(t0)	# dcache 2 way 1
	cache	CACH_PI|C_IINV,-CACHE_ILINE_SIZE(t0)		# icache way 0
	bltu	t0,t1,2b
	cache	CACH_PI|C_IINV,-CACHE_ILINE_SIZE+1(t0)		# BDSLOT way 1
#if CACHE_ILINE_SIZE != (2*CACHE_DLINE_SIZE)
#error R10000 CACHE_ILINE_SIZE != (2*CACHE_DLINE_SIZE)
#endif

1:
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	cache	CACH_SD|C_IWBINV,-CACHE_SLINE_SIZE(a0)		# way 0
	bltu	a0,a1,1b
	cache	CACH_SD|C_IWBINV,-CACHE_SLINE_SIZE+1(a0)	# BDSLOT: way 1

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(a0)
	.set	reorder
	
	j	ra
	END(__cache_wb_inval)


/*
 * Function:	__dcache_inval
 * Purpose:	invalidate cache lines SPECIFIED by the given addresses,
 *		overlapped cache lines at both ends will be wrote back
 *		before invalidated
 * Parameters: 	a0 - starting address, must be cached and unmapped
 *		a1 - number of bytes
 */
LEAF(__dcache_inval)
	dsrl	t0,a0,TO_REGION_SHFT		# get cache attribute

	li	t1,K0BASE>>TO_REGION_SHFT	# cached exclusive on write
	beq	t0,t1,1f
	li	t1,K0BASE_NONCOH>>TO_REGION_SHFT	# cached noncoherent
	beq	t0,t1,1f
	li	t1,K0BASE_EXL>>TO_REGION_SHFT	# cached exclusive
	beq	t0,t1,1f

	/* check for compatibility cached space */
	dsra	t0,a0,TO_COMPAT_REGION_SHFT
	li	t1,0xfffffffc
	beq	t0,t1,1f

	j	ra			# do nothing if uncached address
1:
	PTR_ADDU	a1,a0

	and	t0,a0,CACHE_SLINE_MASK
	beq	t0,a0,1f
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t0)

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(t0)
	.set	reorder
	PTR_ADDU	a0,t0,CACHE_SLINE_SIZE
	bgeu	a0,a1,2f

1:
	and	t0,a1,CACHE_SLINE_MASK
	beq	t0,a1,1f
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t0)

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(t0)
	.set	reorder
	move	a1,t0

1:
	bgeu	a0,a1,2f

	.set	noreorder
1:
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	bltu	a0,a1,1b
	cache	CACH_SD|C_HINV,-CACHE_SLINE_SIZE(a0)

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(a0)
	.set	reorder

2:
	j	ra
	END(__dcache_inval)


/*
 * Function:	__dcache_wb_inval
 * Purpose:	writeback and invalidate cache lines SPECIFIED by the 
 *		given addresses
 * Parameters: 	a0 - starting address, must be cached and unmapped
 *		a1 - number of bytes
 */
LEAF(__dcache_wb_inval)
XLEAF(pdcache_wb_inval)
XLEAF(__dcache_wb)
	dsrl	t0,a0,TO_REGION_SHFT		# get cache attribute

	li	t1,K0BASE>>TO_REGION_SHFT	# cached exclusive on write
	beq	t0,t1,1f
	li	t1,K0BASE_NONCOH>>TO_REGION_SHFT	# cached noncoherent
	beq	t0,t1,1f
	li	t1,K0BASE_EXL>>TO_REGION_SHFT	# cached exclusive
	beq	t0,t1,1f

	/* check for compatibility cached space */
	dsra	t0,a0,TO_COMPAT_REGION_SHFT
	li	t1,0xfffffffc
	beq	t0,t1,1f

	j	ra			# do nothing if uncached address
1:
	PTR_ADDU	a1,a0
	and	a0,CACHE_SLINE_MASK
	PTR_ADDU	a1,CACHE_SLINE_SIZE-1
	and	a1,CACHE_SLINE_MASK

	bgeu	a0,a1,2f

	.set	noreorder
1:
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	bltu	a0,a1,1b
	cache	CACH_SD|C_HWBINV,-CACHE_SLINE_SIZE(a0)	# BDSLOT

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(a0)
	.set	reorder

2:

	j	ra
	END(__dcache_wb_inval)


	
#else	/* !SN0 */

/*
 * Function:	flush_cache
 * Purpose:	writeback and invalidate all caches
 */
LEAF(flush_cache)
	dli	a0,K0_CACHEFLUSHBASE
	lw	a1,_sidcache_size
	srl	a1,1				# x/2 -> two way assoc.
	PTR_ADDU a1, a0				# last address

	.set	noreorder
1:		
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	cache	CACH_SD|C_IWBINV,-CACHE_SLINE_SIZE(a0)		# way 0
	bltu	a0,a1,1b
	cache	CACH_SD|C_IWBINV,-CACHE_SLINE_SIZE+1(a0)	# way 1

	.set	reorder

	j	ra
	
	END(flush_cache)


/*
 * Function:	clear_cache
 * Purpose:	writeback and invalidate cache lines INDEXED
 *		by the given addresses.
 * Parameters: 	a0 - starting address, virtual or physical
 *		a1 - number of bytes
 * Note:	Uses Index writeback if number of bytes > size of secondary
 *		On SN0, the standalone stuff runs mapped code.
 */

#define SABLE_DISK_WAR

LEAF(__cache_wb_inval)
XLEAF(clear_cache)
	and	t0, a0, CALGO_MASK
	dsrl	t0, CALGO_SHFT
	li	t1, CONFIG_UNCACHED
	bne	t0, t1, 1f
	j	ra			# uncached address, ignore.
	
1:
	lw	t0, _sidcache_size
	sltu	t1, a1, t0
	beq	t1, zero, 10f		# attempt to flush > secondary
					# cache size, use index WB.

	PTR_ADDU	a1,a0		# last address

	and	a0,CACHE_SLINE_MASK
	PTR_ADDU	a1,CACHE_SLINE_SIZE-1	# round up to next line
	and	a1,CACHE_SLINE_MASK

	.set	noreorder
1:
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	bltu	a0,a1,1b
	cache	CACH_SD|C_HWBINV,-CACHE_SLINE_SIZE(a0)
	.set	reorder

	j	ra

	
10:
	dli	a0,K0_CACHEFLUSHBASE
	lw	a1,_sidcache_size
	srl	a1,1				# x/2 -> two way assoc.
	PTR_ADDU	a1, a0
	.set	noreorder
1:		
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	cache	CACH_SD|C_IWBINV,-CACHE_SLINE_SIZE(a0)		# way 0
	bltu	a0,a1,1b
	cache	CACH_SD|C_IWBINV,-CACHE_SLINE_SIZE+1(a0)	# way 1

	.set	reorder

	j	ra

	END(__cache_wb_inval)

/*
 * Function:	__dcache_inval
 * Purpose:	invalidate cache lines SPECIFIED by the given addresses,
 *		overlapped cache lines at both ends will be wrote back
 *		before invalidated
 * Parameters: 	a0 - starting address, virtual or physical
 *		a1 - number of bytes
 */
LEAF(__dcache_inval)
	and	t0, a0, CALGO_MASK
	dsrl	t0, CALGO_SHFT
	li	t1, CONFIG_UNCACHED
	bne	t0, t1, 1f
	j	ra			# uncached address, ignore.
1:
	PTR_ADDU	a1,a0

	and	t0,a0,CACHE_SLINE_MASK
	beq	t0,a0,1f
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t0)
	.set	reorder
	PTR_ADDU	a0,t0,CACHE_SLINE_SIZE
	bgeu	a0,a1,2f

1:
	and	t0,a1,CACHE_SLINE_MASK
	beq	t0,a1,1f
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t0)
	.set	reorder
	move	a1,t0

1:
	bgeu	a0,a1,2f

	.set	noreorder
1:
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	bltu	a0,a1,1b
	cache	CACH_SD|C_HINV,-CACHE_SLINE_SIZE(a0)
	.set	reorder

2:
	j	ra
	END(__dcache_inval)


/*
 * Function:	__dcache_wb_inval
 * Purpose:	writeback and invalidate cache lines SPECIFIED by the 
 *		given addresses
 * Parameters: 	a0 - starting address, virtual or physical
 *		a1 - number of bytes
 */
LEAF(__dcache_wb_inval)
XLEAF(pdcache_wb_inval)
XLEAF(__dcache_wb)
	and	t0, a0, CALGO_MASK
	dsrl	t0, CALGO_SHFT
	li	t1, CONFIG_UNCACHED
	bne	t0, t1, 1f
	j	ra			# uncached address, ignore.
1:
	PTR_ADDU	a1,a0
	and	a0,CACHE_SLINE_MASK
	PTR_ADDU	a1,CACHE_SLINE_SIZE-1
	and	a1,CACHE_SLINE_MASK

	bgeu	a0,a1,2f

	.set	noreorder
1:
	PTR_ADDU	a0,CACHE_SLINE_SIZE
	bltu	a0,a1,1b
	cache	CACH_SD|C_HWBINV,-CACHE_SLINE_SIZE(a0)
	.set	reorder

2:
	j	ra
	END(__dcache_wb_inval)
		
#endif /* !SN0 */


/*
 * Function:	invalidate_icache
 * Purpose:	invalidate the icache tag array by modifying the tags, use for
 *		power-up initialization
 * Parameters: 	a0 - icache size
 */
LEAF(invalidate_icache)
	dli	v0,CACHE_INIT_BASE
	dsrl	a0,1				# 2 ways
	PTR_ADDU	v1,v0,a0		# last address

	.set	noreorder
	mtc0	zero,C0_TAGLO		
	mtc0	zero,C0_TAGHI

1:	
	PTR_ADDU	v0,CACHE_ILINE_SIZE
	cache	CACH_PI|C_IST,-CACHE_ILINE_SIZE(v0)	# initialize tag array
	bltu	v0,v1,1b
	cache	CACH_PI|C_IST,-CACHE_ILINE_SIZE+1(v0)	# BDSLOT (other way)

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(v0)
	.set	reorder

	j	ra
	END(invalidate_icache)



/*
 * Function:	invalidate_dcache
 * Purpose:	invalidate the dcache tag array by modifying the tags, use for
 *		power-up initialization
 * Parameters: 	a0 - dcache size
 */
LEAF(invalidate_dcache)
	dli	v0,CACHE_INIT_BASE
	dsrl	a0,1				# 2 ways
	PTR_ADDU	v1,v0,a0		# last address

	.set	noreorder
	mtc0	zero,C0_TAGLO		
	li	a0,CTP_STATEMOD_N<<(CTP_STATEMOD_SHFT-32)
	mtc0	a0,C0_TAGHI

1:	
	PTR_ADDU	v0,CACHE_DLINE_SIZE
	cache	CACH_PD|C_IST,-CACHE_DLINE_SIZE(v0)	# initialize tag array
	bltu	v0,v1,1b
	cache	CACH_PD|C_IST,-CACHE_DLINE_SIZE+1(v0)	# BDSLOT (other way)

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(v0)
	.set	reorder

	j	ra
	END(invalidate_dcache)



/*
 * Function:	invalidate_scache
 * Purpose:	invalidate the scache tag array by modifying the tags, use for
 *		power-up initialization
 * Parameters: 	a0 - scache size
 */
LEAF(invalidate_scache)
	dli	v0,CACHE_INIT_BASE
	dsrl	a0,1				# 2 ways
	PTR_ADDU	v1,v0,a0		# last address

	.set	noreorder
	mtc0	zero,C0_TAGLO		
	mtc0	zero,C0_TAGHI

1:	
	PTR_ADDU	v0,CACHE_SLINE_SIZE
	cache	CACH_SD|C_IST,-CACHE_SLINE_SIZE(v0)	# initialize tag array
	bltu	v0,v1,1b
	cache	CACH_SD|C_IST,-CACHE_SLINE_SIZE+1(v0)	# BDSLOT (other way)

	/* ensure previous cache instructions have graduated */
	cache	CACH_BARRIER,-CACHE_SLINE_SIZE(v0)
	.set	reorder

	j	ra
	END(invalidate_scache)



/*
 * Function:	invalidate_caches
 * Purpose:	invalidate all the caches' tag array by modifying the tags,
 *		use for power-up initialization
 */
INVAL_LOCALSZ = 1			# save ra only
INVAL_FRAMESZ =(((NARGSAVE + INVAL_LOCALSZ) * SZREG) + ALSZ) & ALMASK
INVAL_RAOFF = INVAL_FRAMESZ - (1 * SZREG)
NESTED(invalidate_caches, INVAL_FRAMESZ, zero)
	PTR_SUBU	sp,INVAL_FRAMESZ
	REG_S	ra,INVAL_RAOFF(sp)

	jal	iCacheSize
	move	a0,v0
	jal	invalidate_icache

	jal	dCacheSize
	move	a0,v0
	jal	invalidate_dcache

	jal	sCacheSize
	move	a0,v0
	jal	invalidate_scache

	REG_L	ra,INVAL_RAOFF(sp)
	PTR_ADDU	sp,INVAL_FRAMESZ

	j	ra
	END(invalidate_caches)



/*
 * Function:	_read_tag
 * Purpose:	read the tag accociates with the specified cacheline
 * Parameters: 	a0 - which cache: CACH_PI, CACH_PD, CACH_SD, CACH_SI
 *		a1 - cacheline index, must be cached and unmapped
 *		a2 - array of 2 uints
 * Returns:	taglo/taghi will be in element 0/1 of the array
 *		value of C0_ECC in v0		
 */
LEAF(_read_tag)
	li	t0,CACH_PD
	beq	a0,t0,rprimd		# primary d-cache tag
	blt	a0,t0,rprimi		# primary i-cache tag
	.set	noreorder
	cache	CACH_SD|C_ILT,0(a1)	# fetch secondary cache tag
	.set	reorder

getout:	
	.set	noreorder
	mfc0	t0,C0_TAGLO
	mfc0	t1,C0_TAGHI
	mfc0	v0,C0_ECC
	.set	reorder
	sw	t0,0(a2)	# taglo is 1st uint in tag struct
	sw	t1,4(a2)	# taghi is 2nd uint in tag struct
	j	ra

rprimi:
	.set	noreorder
	cache	CACH_PI|C_ILT,0(a1)	# fetch primary instruction cache tag
	.set	reorder
	b	getout

rprimd:
	.set	noreorder
	cache	CACH_PD|C_ILT,0(a1)	# fetch primary data cache tag
	.set	reorder
	b	getout

	END(_read_tag)


/* _write_tag(WhichCache, address, &source)
 * WhichCache == CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * address may be in KUSER or K0SEG space.
 * source is a pointer to two uints.
 * a0: WhichCache
 * a1: address
 * a2: source ptr
 */

/*
 * Function:	_write_tag
 * Purpose:	write the tag accociates with the specified cacheline
 * Parameters: 	a0 - which cache: CACH_PI, CACH_PD, CACH_SD, CACH_SI
 *		a1 - cacheline index, must be cached and unmapped
 *		a3 - array of 2 uints, taglo/taghi is element 0/1 of the array
 */
LEAF(_write_tag)
	lw	t0,0(a2)		# taglo
	lw	t1,4(a2)		# taghi

	.set	noreorder
	mtc0	t0,C0_TAGLO
	mtc0	t1,C0_TAGHI

	li	t0,CACH_PD
	blt	a0,t0,wprimi
	nop
	beq	a0,t0,wprimd
	nop

	j	ra
	cache	CACH_SD|C_IST,0(a1)	# store secondary cache tag

wprimi:
	j	ra
	cache	CACH_PI|C_IST,0(a1)	# set primary instruction cache tag

wprimd:
	j	ra
	cache	CACH_PD|C_IST,0(a1)	# set primary data cache tag
	.set	reorder
	END(_write_tag)



/*
 * Function:	get_cache_err
 * Purpose:	read the coprocessor 0 cache error register
 * Returns: 	cache error register value in v0
 */
LEAF(get_cache_err)
	.set    noreorder
	j       ra
	mfc0    v0,C0_CACHE_ERR
	.set    reorder
	END(get_cache_err)



/*
 * Function:	get_error_epc
 * Purpose:	read the coprocessor 0 cache error EPC register
 * Returns: 	cache error EPC register value in v0
 */
LEAF(get_error_epc)
	.set    noreorder
	j       ra
	dmfc0    v0,C0_ERROR_EPC
	.set    reorder
	END(get_error_epc)



/*
 * Function:	get_ecc
 * Purpose:	read the coprocessor 0 cache ECC register
 * Returns: 	cache ECC register value in v0
 */
LEAF(get_ecc)
	.set    noreorder
	j       ra
	mfc0    v0,C0_ECC		# get the current ECC value 
	.set    reorder
	END(get_ecc)



/*
 * Function:	set_ecc
 * Purpose:	write the coprocessor 0 cache ECC register
 * Parameters: 	a0 - new value for the cache ECC register
 */
LEAF(set_ecc)
	.set	noreorder
	j	ra
	mtc0	a0,C0_ECC		# set the ECC register
	.set	reorder
	END(set_ecc)



/*
 * Function:	_read_cache_data
 * Purpose:	read the data accociates with the specified cacheline
 * Parameters: 	a0 - which cache: CACH_PI, CACH_PD, CACH_SD, CACH_SI
 *		a1 - cacheline index, must be a cached address
 *		a2 - array of 2 __uint_32's
 * Returns:	taglo/taghi will be in element 0/1 of the array, value
 *		of C0_ECC in v0		
 */
LEAF(_read_cache_data)
	beq	a0,CACH_PI,2f		# want icache data
	beq	a0,CACH_PD,3f		# want dcache data

	.set	noreorder
	cache	CACH_SD|C_ILD,0(a1)	# read 2nd cache data

1:	
	nop
	mfc0	t0,C0_TAGLO
	mfc0	t1,C0_TAGHI
	mfc0	v0,C0_ECC
	AUTO_CACHE_BARRIERS_DISABLE	# serialized by mfc0s
	sw	t0,0(a2)		# taglo
	j	ra
	sw	t1,4(a2)		# BDSLOT: taghi
	AUTO_CACHE_BARRIERS_ENABLE

2:
	b	1b
	cache	CACH_PI|C_ILD,0(a1)	# read icache data

3:
	b	1b
	cache	CACH_PD|C_ILD,0(a1)	# read dcache data

	.set	reorder
	END(_read_cache_data)


/*
 * Function:	_write_cache_data
 * Purpose:	write the data accociates with the specified cacheline
 * Parameters: 	a0 - which cache: CACH_PI, CACH_PD, CACH_SD, CACH_SI
 *		a1 - cacheline index, must be a cached address
 *		a2 - array of 2 __uint32_t's, taglo/taghi is element
 *			0/1 of the array
 *		a3 - ecc
 */
LEAF(_write_cache_data)
	lw	t0,0(a2)		# taglo
	lw	t1,4(a2)		# taghi

	.set	noreorder
	mtc0	t0,C0_TAGLO
	mtc0	t1,C0_TAGHI
	mtc0	a3,C0_ECC

	beq	a0,CACH_PI,1f		# want icache data
	nop
	beq	a0,CACH_PD,2f		# want dcache data
	nop

	j	ra
	cache	CACH_SD|C_ISD,0(a1)	# write 2nd cache data

1:
	j	ra
	cache	CACH_PI|C_ISD,0(a1)	# write icache data

2:
	j	ra
	cache	CACH_PD|C_ISD,0(a1)	# write dcache data

	.set	reorder
	END(_write_cache_data)


/*
 * Function:	iLine
 * Purpose:	Fetch an instruction line from the icache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to il_t buffer
 * Returns:	a1-> filled in.
 */
LEAF(iLine)
	.set	noreorder

	ICACHE(C_ILT, 0(a0))		/* Store Tag */
	MFC0(v0, C0_TAGLO)
	MFC0(v1, C0_TAGHI)
	dsll	v1,32
	dsll	v0,32
	dsrl	v0,32
	or	v0,v1
	sd	v0,IL_TAG(a1)

	daddiu	t0,a1,IL_DATA		/* Data pointer   */
	daddiu	t1,a1,IL_PARITY		/* Parity pointer */

	daddiu	a2,a0,CACHE_ILINE_SIZE
1:		
	ICACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	MFC0(v1, C0_TAGHI)
	dsll	v1,32
	dsll	v0,32
	dsrl	v0,32
	or	v0,v1
	sd	v0,0(t0)
	
	MFC0(v0, C0_ECC)
	sb	v0,0(t1)
	daddiu	t0,8			/* 8-bytes stored */
	daddiu	a0,4
	blt	a0, a2, 1b
	daddiu	t1,1			/* 1-bit of parity */
	
	j	ra
	nop
	.set	reorder
	END(iLine)

/*
 * Function:	dLine
 * Purpose:	Fetch primary data cache line.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to dl_t
 * Returns:	a1--> filled in.
 */
LEAF(dLine)
	.set	noreorder
	
	DCACHE(C_ILT, 0(a0))		/* Read and store tag */
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	dsll	v1,32
	dsrl	v1,32
	or	v0,v1
	sd	v0,DL_TAG(a1)

	daddiu	a2,a0,CACHE_DLINE_SIZE
	daddiu	t0,a1,DL_DATA		/* Data pointer */
	daddiu	t1,a1,DL_ECC		/* ECC pointer */

1:	
	DCACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	sw	v0,0(t0)
	MFC0(v0, C0_ECC)
	sb	v0,0(t1)
	daddiu	a0,4			/* Next word - vaddr */
	daddiu	t0,4			/* Next word - buffer */
	blt	a0,a2,1b
	daddiu	t1,1			/* DELAY: Next ECC field */

	j	ra
	nop
	
	.set	reorder
	END(dLine)
	
/*
 * Function:	sLine
 * Purpose:	Fetch a cache line from the secondary cache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - pointer to sl_t area.
 * Returns:	nothing
 */
LEAF(sLine)

	.set	noreorder

	SCACHE(C_ILT, 0(a0))		/* Pick up T5 TAG */
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	dsll	v1,32			/* Clear high order 32 bits */
	dsrl	v1,32
	or	v0,v1
	sd	v0, SL_TAG(a1)
	
	move	t0,ra			/* Pick up CC tag */
	jal	sCacheSize
	nop
	move	ra,t0

#ifdef EVEREST
	srl	v0,1			/* div by 2 for # ways */
	sub	v0,1			/* scachesize/2 - 1 */
	and	v0,a0,v0		/* V0 - normalizes virtual address */

	divu	v0,CACHE_SLINE_SIZE	/* Compute index */
	sll	v0,4			/* Index into duplicate tags */
	and	v1,a0,1			/* Look at way */
	sll	v1,3			/* add 8-bytes for way 1 */
	daddu	v1,v0			/* Offset into TAGS */
	daddu	v1,EV_BTRAM_BASE	/* address of TAG */
#if SABLE
	move	v0,zero
#else
	ld	v0,0(v1)		/* duplicate tag */
#endif
	sd	v0,SL_CCTAG(a1)		/* and store for the caller */
#endif	/* EVEREST */

	/* OK - lets get the data */

	li	v0,CACHE_SLINE_SIZE/16	/* # fetches */
	daddiu	t0,a1,SL_DATA
	daddiu	t1,a1,SL_ECC
2:
	SCACHE(C_ILD, 0(a0))
	MFC0(v1, C0_TAGLO)		/* Store 8-bytes of data */
	sw	v1,0(t0)
	MFC0(v1, C0_TAGHI)
	sw	v1,4(t0)
	MFC0(v1, C0_ECC)
	sh	v1,0(t1)		/* Store ECC for this 8 bytes */
	
	SCACHE(C_ILD, 8(a0))
	MFC0(v1, C0_TAGLO)		/* Store 8-bytes of data */
	sw	v1,8(t0)
	MFC0(v1, C0_TAGHI)
	sw	v1,12(t0)
	
	sub	v0,1
	daddu	a0,16			/* Increment address */
	daddu	t0,16			/* Increment data array. */
	bgt	v0,zero,2b
	daddiu	t1,2			/* DELAY: Increment ECC array */
1:
	/* All done ... */
	j	ra
	nop
	.set	reorder
	END(sLine)

LEAF(cache_K0)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	and	t0,v0,~CONFIG_K0
	or	t0,CONFIG_NONCOHRNT
	mtc0	t0,C0_CONFIG
	j	ra
	nop					# BDSLOT
	.set	reorder
	END(cache_K0)

LEAF(uncache_K0)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	and	t0,v0,~CONFIG_K0
	or	t0,CONFIG_UNCACHED
	mtc0	t0,C0_CONFIG
	j	ra
	nop					# BDSLOT
	.set	reorder
	END(uncache_K0)

LEAF(is_K0_cached)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	.set	reorder
	and	t0,v0,CONFIG_K0
	sub	v0,t0,CONFIG_UNCACHED
	j	ra
	END(is_K0_cached)

BSS(_icache_size, 4)			# bytes of icache
BSS(_dcache_size, 4)			# bytes of dcache
BSS(cachewrback, 4)			# writeback secondary cache?

BSS(_sidcache_size, 4)			# bytes of secondary cache
BSS(_scache_linesize, 4)		# secondary cache line size
BSS(_scache_linemask, 4)		# secondary cache line mask

BSS(_icache_linesize, 4)		# primary I cache line size
BSS(_icache_linemask, 4)		# primary I cache line mask

BSS(_dcache_linesize, 4)		# primary D cache line size
BSS(_dcache_linemask, 4)		# primary D cache line mask
