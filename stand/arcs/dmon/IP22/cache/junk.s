/*
 * r4kcache.s -- R4000-specific Cache manipulation primitives
 */

#ident "$Revision: 1.1 $"

#include <regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <asm.h>

BSS(_icache_size, 4)		# bytes of icache
BSS(_dcache_size, 4)		# bytes of dcache
BSS(cachewrback, 4)		# writeback secondary cache?

BSS(_sidcache_size, 4)			# bytes of secondary cache
BSS(_scache_linesize, 4)		# secondary cache line size
BSS(_scache_linemask, 4)		# secondary cache line mask

BSS(_icache_linesize, 4)		# primary I cache line size
BSS(_icache_linemask, 4)		# primary I cache line mask

BSS(_dcache_linesize, 4)		# primary D cache line size
BSS(_dcache_linemask, 4)		# primary D cache line mask

#define	NBPP			4096

#define	SCACHE_LINESIZE		(32*4)
#define SCACHE_LINEMASK		((32*4)-1)

#ifdef BLOCKSIZE_8WORDS
#define	DCACHE_LINESIZE		(8*4)
#define	ICACHE_LINESIZE		(8*4)
#define DCACHE_LINEMASK		((8*4)-1)
#define ICACHE_LINEMASK		((8*4)-1)
#else
#define	DCACHE_LINESIZE		(4*4)
#define	ICACHE_LINESIZE		(4*4)
#define DCACHE_LINEMASK		((4*4)-1)
#define ICACHE_LINEMASK		((4*4)-1)
#endif	/* BLOCKSIZE_8WORDS */

#ifdef R4600
#define	R4600_DCACHE_LINESIZE		(8*4)
#define	R4600_ICACHE_LINESIZE		(8*4)
#define R4600_DCACHE_LINEMASK		((8*4)-1)
#define R4600_ICACHE_LINEMASK		((8*4)-1)

BSS(_two_set_pcaches, 4)	# 0 if one-set; set offset if two-set

#endif /* R4600 */

#if IP20 || IP22
#define K0_CACHEFLUSHBASE K0_RAMBASE
#else
#define K0_CACHEFLUSHBASE K0BASE
#endif




/*
 * __cache_wb_inval(addr, len)
 * 
 * Uses the INDEX_INVALIDATE and INDEX_WRITEBACK_INVALIDATE
 * cacheops. Should be called with K0 addresses, to avoid
 * the tlb translation (and tlb miss fault) overhead.
 *
 * Since we are shooting down 'live' lines in the 2nd cache, these
 * lines have to be written back to memory before invalidating.
 * Since we are invalidating lines in the secondary cache which
 * will correspond to primary data cache lines, the primary data
 * cache has to be invalidated also (a line cannot be valid in
 * the primary but invalid in secondary.)
 * We have to invalidate the primary instruction cache for the
 * same reason.
 *
 * Since we are using index (writeback) invalidate cache ops on
 * the primary caches, and since the address we are given generally
 * has nothing to do with the virtual address at which the data may
 * have been used (otherwise we would use the 'hit' operations), we
 * perform the cache op at both indices in the primary caches. (Assumes
 * 4K page size and 8K caches.) This is necessary since the address
 * is basically meaningless.
 *
 * XXX Assumes 32 word line size for secondary cache.
 * XXX Assumes primary data and primary inst. cache are the same size.
 * XXX Assumes primary data and primary inst. line size are the same.
 * XXX Assumes 8k primary cache size and 4k page size.
 */
LEAF(__cache_wb_inval)
XLEAF(clear_cache)
	/*
	 * This routine is not preemptable, to avoid creating a non-subset
	 * cache situation, since the algorithm is to do the primary caches,
	 * then the secondary.
	 * Also, load any memory references into a register
	 * before flushing the primary caches, again to avoid a non-subset
	 * situation.
	 * Also, run uncached, for the same reason.
	 */
	/* NOTE: The hardware mavins assure me that it is OK to
	 * violate the subset rule for the i-cache. For this reason,
	 * run this routine cached, since uncached instruction accesses
	 * are about 48 times slower than cached.
	 */
	srl	t0,a0,29		# make sure it's a cached address
	beq	t0,0x4,1f
	j	ra
1:
#if defined(IP20) || defined(EVEREST)

	j	rmi_cacheflush
#else
#if !defined(IP22)
	blez	a1, 42f			# if we don't have anything return
#endif

#ifdef R4000PC
	li	t4, 0x100000
	/* do we have an scache? */
	bnez	t4,101f			# yes --> just use scache operations 

	lw	t7, _dcache_size
	lw	v0, _icache_size
#ifdef R4600
	lw	t6,_two_set_pcaches
	beqz	t6,17f
	/* do dcache */
	beqz	t7, 31f		# no dcache --> check icache

	/* clean dcache using indexed invalidate */
	move	t0, a0			# set up current address
 
	PTR_ADDU t1, t0, a1		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + count - R4600_DCACHE_LINESIZE)

	and t3, t0, R4600_DCACHE_LINEMASK	# align start to dcache line
	PTR_SUBU t0, t3			# pull off extra

	.set	noreorder
2:	cache	CACH_PD|C_IWBINV, (t0)	# Invalidate cache line
	xor	t5,t0,t6
	cache	CACH_PD|C_IWBINV, (t5)	# Invalidate cache line
	bltu	t0, t1, 2b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT
	.set	reorder

	/* now do icache */
31:	beqz	v0, 41f

	/* clean icache using indexed invalidate */
	move	t0, a0			# set up current address

	PTR_ADDU t1, t0, a1		# set up target address
	PTR_SUBU t1, R4600_ICACHE_LINESIZE	# (base + size - R4600_ICACHE_LINESIZE)

	and t3, t0, R4600_ICACHE_LINEMASK # align start to icache line
	PTR_SUBU t0, t3			# pull off extra

	and	t3, t1, R4600_ICACHE_LINEMASK	# align ending to icache line
	PTR_SUBU t1, t3

	.set	noreorder
4:	cache	CACH_PI|C_IINV, (t0) # Invalidate cache line
	xor	t5,t0,t6
	cache	CACH_PI|C_IINV, (t5) # Invalidate cache line
	bltu	t0, t1, 4b
	PTR_ADDU t0, R4600_ICACHE_LINESIZE	# BDSLOT
	.set	reorder

41:
	j	ra

17:
#endif /* R4600 */
	/* do dcache */
	beqz	t7, 31f		# no dcache --> check icache

	/* clean dcache using indexed invalidate */
	move	t0, a0			# set up current address
 
	PTR_ADDU t1, t0, a1		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + count - DCACHE_LINESIZE)

	and t3, t0, DCACHE_LINEMASK	# align start to dcache line
	PTR_SUBU t0, t3			# pull off extra

	.set	noreorder
2:	cache	CACH_PD|C_IWBINV, (t0)	# Invalidate cache line
	bltu	t0, t1, 2b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT
	.set	reorder

	/* now do icache */
31:	beqz	v0, 41f

	/* clean icache using indexed invalidate */
	move	t0, a0			# set up current address

	PTR_ADDU t1, t0, a1		# set up target address
	PTR_SUBU t1, ICACHE_LINESIZE	# (base + size - ICACHE_LINESIZE)

	and t3, t0, ICACHE_LINEMASK # align start to icache line
	PTR_SUBU t0, t3			# pull off extra

	and	t3, t1, ICACHE_LINEMASK	# align ending to icache line
	PTR_SUBU t1, t3

	.set	noreorder
4:	cache	CACH_PI|C_IINV, (t0) # Invalidate cache line
	bltu	t0, t1, 4b
	PTR_ADDU t0, ICACHE_LINESIZE	# BDSLOT
	.set	reorder

41:
	j	ra

101:
#endif /* R4000PC */
#ifdef IP22
	j	rmi_cacheflush		# XXX - broken on IP24
#else
	# Load this before flushing the primary dcache.
	lw	t4,_sidcache_size
	lw	t1,_dcache_size

	srl	t1,t1,1			# primary cache size/2
	move	t0,a0			# starting address
	bltu	t1,a1,1f		# cache is smaller than count
	move	t1,a1
1:
	# t1 has min(_dcache_size/2, len)
	addu	t1,t0			# ending addr + 1

	# align the starting address to a 2nd cache line
	and	t2,t0,SCACHE_LINEMASK
	subu	t0,t2

1:	# top of loop
	.set	noreorder
	.set	noat
	# loop unrolled by four - 1 secondary cache line

	# invalidate 4 primary inst. lines
	cache	CACH_PI|C_IINV,0(t0)
	cache	CACH_PI|C_IINV,(ICACHE_LINESIZE)(t0)
	cache	CACH_PI|C_IINV,(ICACHE_LINESIZE*2)(t0)
	cache	CACH_PI|C_IINV,(ICACHE_LINESIZE*3)(t0)

#ifndef BLOCKSIZE_8WORDS
	cache	CACH_PI|C_IINV,(ICACHE_LINESIZE*4)(t0)
	cache	CACH_PI|C_IINV,(ICACHE_LINESIZE*5)(t0)
	cache	CACH_PI|C_IINV,(ICACHE_LINESIZE*6)(t0)
	cache	CACH_PI|C_IINV,(ICACHE_LINESIZE*7)(t0)
#endif

	# Do the other index in the primary I cache
	cache	CACH_PI|C_IINV,NBPP(t0)
	cache	CACH_PI|C_IINV,(NBPP+ICACHE_LINESIZE)(t0)
	cache	CACH_PI|C_IINV,(NBPP+ICACHE_LINESIZE*2)(t0)
	cache	CACH_PI|C_IINV,(NBPP+ICACHE_LINESIZE*3)(t0)

#ifndef BLOCKSIZE_8WORDS
	cache	CACH_PI|C_IINV,(NBPP+ICACHE_LINESIZE*4)(t0)
	cache	CACH_PI|C_IINV,(NBPP+ICACHE_LINESIZE*5)(t0)
	cache	CACH_PI|C_IINV,(NBPP+ICACHE_LINESIZE*6)(t0)
	cache	CACH_PI|C_IINV,(NBPP+ICACHE_LINESIZE*7)(t0)
#endif

	# writeback + inval 4 primary data
	cache	CACH_PD|C_IWBINV,0(t0)
	cache	CACH_PD|C_IWBINV,(DCACHE_LINESIZE)(t0)
	cache	CACH_PD|C_IWBINV,(DCACHE_LINESIZE*2)(t0)
	cache	CACH_PD|C_IWBINV,(DCACHE_LINESIZE*3)(t0)

#ifndef BLOCKSIZE_8WORDS
	cache	CACH_PD|C_IWBINV,(DCACHE_LINESIZE*4)(t0)
	cache	CACH_PD|C_IWBINV,(DCACHE_LINESIZE*5)(t0)
	cache	CACH_PD|C_IWBINV,(DCACHE_LINESIZE*6)(t0)
	cache	CACH_PD|C_IWBINV,(DCACHE_LINESIZE*7)(t0)
#endif

	# do the other index
	cache	CACH_PD|C_IWBINV,NBPP(t0)
	cache	CACH_PD|C_IWBINV,(NBPP+DCACHE_LINESIZE)(t0)
	cache	CACH_PD|C_IWBINV,(NBPP+DCACHE_LINESIZE*2)(t0)
	cache	CACH_PD|C_IWBINV,(NBPP+DCACHE_LINESIZE*3)(t0)

#ifndef BLOCKSIZE_8WORDS
	cache	CACH_PD|C_IWBINV,(NBPP+DCACHE_LINESIZE*4)(t0)
	cache	CACH_PD|C_IWBINV,(NBPP+DCACHE_LINESIZE*5)(t0)
	cache	CACH_PD|C_IWBINV,(NBPP+DCACHE_LINESIZE*6)(t0)
	cache	CACH_PD|C_IWBINV,(NBPP+DCACHE_LINESIZE*7)(t0)
#endif
	.set	reorder
	.set 	at
#ifdef BLOCKSIZE_8WORDS
	addu	t0,DCACHE_LINESIZE*4
#else
	addu	t0,DCACHE_LINESIZE*8
#endif
	bltu	t0,t1,1b

	# now do 2nd cache
	move	t0,a0			# starting addr.
	bltu	t4,a1,1f		# 2nd cache is smaller than count
	move	t4,a1
1:
	addu	t4,t0			# ending addr + 1
	subu	t0,t2			# line-align
1:
	.set	noreorder
	.set	noat
	cache	CACH_SD|C_IWBINV,0(t0)	# writeback + inval 2nd cache lines
	.set	reorder
	.set	at
	addu	t0,SCACHE_LINESIZE
	bltu	t0,t4,1b

42:
	j	ra			# restores cached mode.
#endif	/* IP22 */
#endif	/* IP20 */
	END(__cache_wb_inval)
/*
 * flush_cache()
 * flush entire I & D cache
 */

#define ALSZ            15      /* align on 16 byte boundary */
#define ALMASK          ~0xf
#define SZREG 4
#define FRAMESZ(size) (((size)+ALSZ) & ALMASK)


FLUSHFRM=	FRAMESZ((4+1)*SZREG)		# 4 arg saves, and ra
NESTED(flush_cache2, FLUSHFRM, zero)
XLEAF(FlushAllCaches)
	subu	sp,FLUSHFRM
	sw	ra,FLUSHFRM-4(sp)
	li	a0,K0_RAMBASE
	li	a1, 0x100000
#ifdef R4000PC
	bnez	a1, 1f
	lw	a1, _dcache_size
#ifdef R4600
	lw	a2, _two_set_pcaches
	beqz	a2,1f
	move	a1,a2
#endif /* R4600 */	
1:
#endif /* R4000PC */
	jal	__cache_wb_inval
	lw	ra,FLUSHFRM-4(sp)
	addu	sp,FLUSHFRM
	j	ra
	END(flush_cache)


#if defined(IP20) || defined(EVEREST) || defined(IP22)

/*
 * rmi_cacheflush(addr,len)
 */
LEAF(rmi_cacheflush)
	move	t0,a0			# starting addr.
	move	t5,a1
	addu	t5,t0			# ending addr + 1

#ifdef R4000PC_XXXX
	lw	t2,_sidcache_size
	bnez	t2,101f
#endif
	# align the starting address to a 2nd cache line
	and	t0,~SCACHE_LINEMASK
1:
	.set	noreorder
	/*
	** The RMI workaround is for a bug in the RMI/MC chip on IP17/IP20.
	** The problem occurs when we try to do writes without a
	** preceding read.  Such an operation only occurs when we
	** do a cache instruction with a WRITEBACK from secondary
	** cache.
	**
	** The workaround is to:
	**	1) read from the location about to be flushed
	**	2) read from the "companion location", which is
	**	   a location that will hit at the same index in the

	**	   secondary cache and cause an implicit writeback, if
	**	   the old line is dirty.
	**	3) invalidate the companion line.
	**
	** Step one is needed in case the companion line is already
	** in the cache and is dirty....it will be flushed back to
	** memory.
	** Step 2 is needed to cause the desired line to be flushed.
	** Step 3 is needed in order to avoid upsetting assumptions
	** about cache lines that, say, a driver might make.
	**
	*/
	lw	zero,(t0)		# read from line to flush
#define COMPANION_BIT 0x00400000
	li	t1,COMPANION_BIT	# generate companion
	xor	t1,t0
#if defined(IP20) || defined(IP22)
	li	t2,K0_RAMBASE
	bgeu	t1,t2,2f
	nop
	or	t1,K0_RAMBASE
2:
#endif
	lw	zero,(t1)		# cause writeback	
	cache	CACH_SD|C_HINV,0(t1)	# invalidate cache line
	.set	reorder
	addu	t0,SCACHE_LINESIZE
	bltu	t0,t5,1b
	j	ra
#ifdef R4000PC_XXXX
101:
	# align the starting address.
	and	t2,t0,ICACHE_LINEMASK
	subu	t0,t2

1:
	.set	noreorder
	cache	CACH_PI|C_IINV,0(t0)
	.set	reorder

	addu	t0,ICACHE_LINESIZE
	bltu	t0,t5,1b
			
	move	t0,a0
	# align the starting address.
	and	t2,t0,DCACHE_LINEMASK
	subu	t0,t2

1:
	.set	noreorder
	cache	CACH_PD|C_IWBINV,0(t0)
	.set	reorder

	addu	t0,DCACHE_LINESIZE
	bltu	t0,t5,1b
	
	j	ra
#endif
	END(rmi_cacheflush)
#endif	/* IP20 */

