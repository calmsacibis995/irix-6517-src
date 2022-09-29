/*
 * r4kcache.s -- R4000-specific Cache manipulation primitives
 */

#ident "$Revision: 1.27 $"

#include	"ml.h"
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <asm.h>

#ifdef	EVEREST
#  include <sys/EVEREST/evmp.h>
#endif

BSS(_icache_size, 4)		# bytes of icache
BSS(_dcache_size, 4)		# bytes of dcache
BSS(cachewrback, 4)		# writeback secondary cache?

BSS(_r4600sc_sidcache_size, 4)		# for r4600sc hinv 
BSS(_r5000_sidcache_size, 4)		# bytes of secondary cache calculate
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
#define CACHSZ_REG			0x11	# 4600SC cache size location in eerom
#define R4600_SCACHE_LINESIZE		(8*4)
#define R4600_SCACHE_LINEMASK		((8*4)-1)
#define XKPHYS_UNCACHED_BASE		0x9000
#define XKPHYS_UNCACHED_SHIFT		0x20

BSS(_r4600sc_cache_on, 4)		# 0 if not present or off, 1 if on
BSS(_two_set_pcaches, 4)	# 0 if one-set; set offset if two-set

#endif /* R4600 */

#if IP20 || IP22
#define K0_CACHEFLUSHBASE K0_RAMBASE
#else
#define K0_CACHEFLUSHBASE K0BASE
#endif

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
 *
 * XXX Assumes 32 word line size for secondary cache.
 */

LEAF(__dcache_inval)
#if _MIPS_SIM == _ABI64
	dsrl	t0,a0,56		# make sure it's a cached address
	andi	t0,0xf8
	beq	t0,0x98,1f
	beq	t0,0xa8,1f
#endif
	PTR_SRL	t0,a0,29		# make sure it's a cached address
	andi	t0,0x7
	beq	t0,0x4,1f
	j	ra
1:
#if defined(IP20)

	j	rmi_cacheflush
#else
	blez	a1,3f			# if length <= 0, we are done
#ifdef R4000PC
	lw	ta3,_sidcache_size
	beqz	ta3,99f			# no scache--do primary only
#endif
#ifdef IP22
	j	rmi_cacheflush		# XXX - broken on IP24
#else
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid

	# check and align starting and ending addresses.
	and	t2,t0,SCACHE_LINEMASK
	beq	t2,zero,1f		# starting addr is aligned

	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback invalidate' the secondary line indicated
	# by 'addr'. This also does writeback/invalidates in the primary
	# I and D caches as necessary.
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t0)	# writeback + inval
	.set	reorder

	# align addr to the next line
	PTR_ADDU t0,SCACHE_LINESIZE
	bgeu	t0,t1,3f		# Out if we are done
1:
	# check if ending address is cache-line aligned.
	and	t2,t1,SCACHE_LINEMASK
	beq	t2,zero,1f		# ending address is full cache line
	PTR_SUBU t1,t2			# align to start of line
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t1)	# writeback + inval
	.set	reorder
	bgeu	t0,t1,3f

1:	# top of loop
	.set	noreorder
	cache	CACH_SD|C_HINV,0(t0)	# invalidate secondary and primary
	.set	reorder

	PTR_ADDU t0,SCACHE_LINESIZE
	bltu	t0,t1,1b

3:
	j	ra
#endif	/* IP22 */
#ifdef R4000PC
99:	
	lw	ta3,_dcache_size
	beqz	ta3, 3f			# no dcache --> done

#ifdef R4600
	lw	t3,_two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > dcache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

#ifdef R4600
	beqz	t3,17f
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,R4600_DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_DCACHE_LINEMASK
	beq	t2,zero,1f		# starting addr is aligned

	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
	cache	CACH_PD|C_HWBINV,0(t0) # writeback + inval

	bgeu	t0,t1,3f		# Out if we are done
	# align addr to the next line
	PTR_ADDU t0,R4600_DCACHE_LINESIZE	# BDSLOT
1:
	# check if ending address is cache-line aligned.
	and	t2,t1,R4600_DCACHE_LINEMASK
	beq	t2,zero,1f		# ending address is full cache line
	PTR_SUBU t1,t2			# BDSLOT align to start of line
	cache	CACH_PD|C_HWBINV,R4600_DCACHE_LINESIZE(t1) # writeback + inval
	bgeu	t0,t1,3f
	nop				# BDSLOT

1:	# top of loop
#ifdef TWO_SET_HINV_WAR
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary
#else
	cache	CACH_PD|C_HINV,0(t0)	# invalidate primary
#endif

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_DCACHE_LINESIZE
	.set	reorder
3:
	j	ra

17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,DCACHE_LINEMASK
	beq	t2,zero,1f		# starting addr is aligned

	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
	cache	CACH_PD|C_HWBINV,0(t0) # writeback + inval

	bgeu	t0,t1,3f		# Out if we are done
	# align addr to the next line
	PTR_ADDU t0,DCACHE_LINESIZE	# BDSLOT
1:
	# check if ending address is cache-line aligned.
	and	t2,t1,DCACHE_LINEMASK
	beq	t2,zero,1f		# ending address is full cache line
	PTR_SUBU t1,t2			# BDSLOT align to start of line
	cache	CACH_PD|C_HWBINV,DCACHE_LINESIZE(t1) # writeback + inval
	bgeu	t0,t1,3f
	nop				# BDSLOT

1:	# top of loop
	cache	CACH_PD|C_HINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,DCACHE_LINESIZE
	.set	reorder
3:
	j	ra

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R4600
	beqz	t3,17f
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV,0(t0)	# Invalidate cache line
	xor	ta0,t0,t3
	cache	CACH_PD|C_IWBINV,0(ta0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
17:
#endif /* R4600 */
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
#endif /* R4000PC */
#endif	/* IP20 */
	END(__dcache_inval)

/*
 * __dcache_wb(addr,len)
 * write back data from the primary data and secondary cache.
 *
 * XXX Assumes 32 word line size for secondary cache.
 */

LEAF(__dcache_wb)
#if _MIPS_SIM == _ABI64
	dsrl	t0,a0,56		# make sure it's a cached address
	andi	t0,0xf8
	beq	t0,0x98,1f
	beq	t0,0xa8,1f
#endif
	PTR_SRL	t0,a0,29		# make sure it's a cached address
	andi	t0,0x7
	beq	t0,0x4,1f
	j	ra
1:
#if defined(IP20)

	j	rmi_cacheflush
#else
	blez	a1,3f			# if length <= 0, we are done
#ifdef R4000PC
	lw	ta3,_sidcache_size
	beqz	ta3,99f			# no scache--do primary only
#endif
#ifdef IP22
	j	rmi_cacheflush		# XXX - broken on IP24
#else
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid

	# align the starting address.
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2

1:	# top of loop
	.set	noreorder
	cache	CACH_SD|C_HWB,0(t0)	# writeback secondary
	.set	reorder

	PTR_ADDU t0,SCACHE_LINESIZE
	bltu	t0,t1,1b

3:
	j	ra
#endif	/* IP22 */
#ifdef R4000PC
99:	
	lw	ta3,_dcache_size
	beqz	ta3, 3f			# no dcache --> done

#ifdef R4600
	lw	t3,_two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > dcache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

#ifdef R4600
	beqz	t3,17f
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,R4600_DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWB,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_DCACHE_LINESIZE
	.set	reorder
3:
	j	ra
17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWB,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,DCACHE_LINESIZE
	.set	reorder
3:
	j	ra

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R4600
	beqz	t3,17f
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV,0(t0)	# Invalidate cache line
	xor	ta0,t0,t3
	cache	CACH_PD|C_IWBINV,0(ta0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
17:
#endif /* R4600 */
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
#endif /* R4000PC */
#endif	/* IP20 */
	END(__dcache_wb)

/*
 * __dcache_wb_inval(addr, len)
 * writeback-invalidate data cache for range of physical addr to addr+len-1
 *
 * XXX Assumes 32 word line size for secondary cache.
 */

LEAF(__dcache_wb_inval)
XLEAF(pdcache_wb_inval)
#if _MIPS_SIM == _ABI64
	dsrl	t0,a0,56		# make sure it's a cached address
	andi	t0,0xf8
	beq	t0,0x98,1f
	beq	t0,0xa8,1f
#endif
	PTR_SRL	t0,a0,29		# make sure it's a cached address
	andi	t0,0x7
	beq	t0,0x4,1f
	j	ra
1:
#if defined(IP20)

	j	rmi_cacheflush
#else
	blez	a1,3f			# if length <= 0, we are done
#ifdef R4000PC
	lw	ta3,_sidcache_size
	beqz	ta3,99f			# no scache--do primary only
#endif
#ifdef IP22
	j	rmi_cacheflush		# XXX - broken on IP24
#else
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid

	# check and align starting address
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2

1:	# top of loop
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t0)	# wb-inval secondary line
	.set	reorder

	PTR_ADDU t0,SCACHE_LINESIZE
	bltu	t0,t1,1b

3:
	j	ra
#endif	/* IP22 */
#ifdef R4000PC
99:	
	lw	ta3,_dcache_size
	beqz	ta3, 3f			# no dcache --> done

#ifdef R4600
	lw	t3,_two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > dcache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

#ifdef R4600
	beqz	t3,17f
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,R4600_DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_DCACHE_LINESIZE
	.set	reorder
3:
	j	ra
17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,DCACHE_LINESIZE
	.set	reorder
3:
	j	ra

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R4600
	beqz	t3,17f
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV,0(t0)	# Invalidate cache line
	xor	ta0,t0,t3
	cache	CACH_PD|C_IWBINV,0(ta0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
17:
#endif /* R4600 */
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
#endif /* R4000PC */
#endif	/* IP20 */
	END(__dcache_wb_inval)

/*
 * __icache_inval(addr, len)
 * invalidate instruction cache for range of physical addr to addr+len-1
 *
 * We use the HIT_INVALIDATE cache op. Therefore, this requires
 * that 'addr'..'len' be the actual addresses which we want to
 * invalidate in the caches. (i.e. won't work for user virtual
 * addresses that the kernel converts to K0 before calling.)
 * addr/len are not necessarily cache line aligned.
 *
 * Invalidate lines in both the primary and secondary cache.
 *
 * XXX Assumes 32 word line size for secondary cache.
 */

LEAF(__icache_inval)
XLEAF(picache_inval)
#if _MIPS_SIM == _ABI64
	dsrl	t0,a0,56		# make sure it's a cached address
	andi	t0,0xf8
	beq	t0,0x98,1f
	beq	t0,0xa8,1f
#endif
	PTR_SRL	t0,a0,29		# make sure it's a cached address
	andi	t0,0x7
	beq	t0,0x4,1f
	j	ra
1:
	blez	a1,3f			# if length <= 0, we are done
#ifdef R4000PC
	lw	t2,_sidcache_size
	bnez	t2, 99f
#endif
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1

	# align the starting address.
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2

1:
	.set	noreorder
	cache	CACH_SD|C_HINV,0(t0)
	.set	reorder

	PTR_ADDU t0,SCACHE_LINESIZE
	bltu	t0,t1,1b

3:
	j	ra

#ifdef R4000PC
99:	
	lw	ta3,_icache_size
	beqz	ta3, 3f			# no icache --> done

#ifdef R4600
	lw	t3,_two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > icache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

#ifdef R4600
	beqz	t3,17f
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,R4600_ICACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_ICACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PI|C_HINV,0(t0)	# invalidate primary
	xor	ta0,t0,t3
	cache	CACH_PI|C_HINV,0(ta0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_ICACHE_LINESIZE
	.set	reorder
3:
	j	ra
17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,ICACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,ICACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PI|C_HINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,ICACHE_LINESIZE
	.set	reorder
3:
	j	ra

	/* clean icache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R4600
	beqz	t3,17f
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_ICACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PI|C_IINV,0(t0)	# Invalidate cache line
	xor	ta0,t0,t3
	cache	CACH_PI|C_IINV,0(ta0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_ICACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
17:
#endif /* R4600 */
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, ICACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PI|C_IINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, ICACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
#endif /* R4000PC */
	END(__icache_inval)
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
#if _MIPS_SIM == _ABI64
	dsrl	t0,a0,56		# make sure it's a cached address
	andi	t0,0xf8
	beq	t0,0x98,1f
	beq	t0,0xa8,1f
#endif
	PTR_SRL	t0,a0,29		# make sure it's a cached address
	andi	t0,0x7
	beq	t0,0x4,1f
	j	ra
1:
#if defined(IP20)

	j	rmi_cacheflush
#else
#if !defined(IP22)
	blez	a1, 42f			# if we don't have anything return
#endif

#ifdef R4000PC
	lw	ta0, _sidcache_size
	/* do we have an scache? */
	bnez	ta0,101f			# yes --> just use scache operations 

	lw	ta3, _dcache_size
	lw	v0, _icache_size
#ifdef R4600
	lw	ta2,_two_set_pcaches
	beqz	ta2,17f
	/* do dcache */
	beqz	ta3, 31f		# no dcache --> check icache

	/* clean dcache using indexed invalidate */
	move	t0, a0			# set up current address
 
	PTR_ADDU t1, t0, a1		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + count - R4600_DCACHE_LINESIZE)

	and t3, t0, R4600_DCACHE_LINEMASK	# align start to dcache line
	PTR_SUBU t0, t3			# pull off extra

	.set	noreorder
2:	cache	CACH_PD|C_IWBINV,0(t0)	# Invalidate cache line
	xor	ta1,t0,ta2
	cache	CACH_PD|C_IWBINV,0(ta1)	# Invalidate cache line
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
4:	cache	CACH_PI|C_IINV,0(t0) # Invalidate cache line
	xor	ta1,t0,ta2
	cache	CACH_PI|C_IINV,0(ta1) # Invalidate cache line
	bltu	t0, t1, 4b
	PTR_ADDU t0, R4600_ICACHE_LINESIZE	# BDSLOT
	.set	reorder

41:
	j	ra

17:
#endif /* R4600 */
	/* do dcache */
	beqz	ta3, 31f		# no dcache --> check icache

	/* clean dcache using indexed invalidate */
	move	t0, a0			# set up current address
 
	PTR_ADDU t1, t0, a1		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + count - DCACHE_LINESIZE)

	and t3, t0, DCACHE_LINEMASK	# align start to dcache line
	PTR_SUBU t0, t3			# pull off extra

	.set	noreorder
2:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
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
4:	cache	CACH_PI|C_IINV, 0(t0) # Invalidate cache line
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
	lw	ta0,_sidcache_size
	lw	t1,_dcache_size

	srl	t1,t1,1			# primary cache size/2
	move	t0,a0			# starting address
	bltu	t1,a1,1f		# cache is smaller than count
	move	t1,a1
1:
	# t1 has min(_dcache_size/2, len)
	PTR_ADDU t1,t0			# ending addr + 1

	# align the starting address to a 2nd cache line
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2

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
	PTR_ADDU t0,DCACHE_LINESIZE*4
#else
	PTR_ADDU t0,DCACHE_LINESIZE*8
#endif
	bltu	t0,t1,1b

	# now do 2nd cache
	move	t0,a0			# starting addr.
	bltu	ta0,a1,1f		# 2nd cache is smaller than count
	move	ta0,a1
1:
	PTR_ADDU ta0,t0			# ending addr + 1
	PTR_SUBU t0,t2			# line-align
1:
	.set	noreorder
	.set	noat
	cache	CACH_SD|C_IWBINV,0(t0)	# writeback + inval 2nd cache lines
	.set	reorder
	.set	at
	PTR_ADDU t0,SCACHE_LINESIZE
	bltu	t0,ta0,1b

42:
	j	ra			# restores cached mode.
#endif	/* IP22 */
#endif	/* IP20 */
	END(__cache_wb_inval)
/*
 * flush_cache()
 * flush entire I & D cache
 */

FLUSHFRM=	FRAMESZ((4+1)*SZREG)		# 4 arg saves, and ra
NESTED(flush_cache, FLUSHFRM, zero)
#ifndef EVEREST					# ev_flush_caches on EVEREST
XLEAF(FlushAllCaches)
#endif
	PTR_SUBU sp,FLUSHFRM
	REG_S	ra,FLUSHFRM-SZREG(sp)
	LI	a0,K0_RAMBASE
	lw	a1,_sidcache_size
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
	REG_L	ra,FLUSHFRM-SZREG(sp)
	PTR_ADDU sp,FLUSHFRM
	j	ra
	END(flush_cache)

/*
 * Config_cache() -- determine sizes of i and d caches
 * Sizes stored in globals picache_size, pdcache_size, icache_size
 * and dcache_size.
 * For IP17, 2nd cache size stored in global sidcache_size.
 * Determines size of secondary cache - assumes 2nd cache is
 * data or unified I+D.
 * Can be extended to look for 2nd instruction
 * cache by reading the config register. By definition, if 2nd
 * cache is present and 'split', then both secondary caches are
 * the same size.
 */

#define	MIN_CACH_POW2	12

CONFIGFRM=	FRAMESZ((4+1+1)*SZREG)	# 4 arg saves, ra, and a saved register
NESTED(config_cache, CONFIGFRM, zero)
	PTR_SUBU sp,CONFIGFRM
	REG_S	ra,CONFIGFRM-(1*SZREG)(sp)
	REG_S	s0,CONFIGFRM-(2*SZREG)(sp)	# save s0 on stack
	.set    noreorder
	mfc0	s0,C0_SR		# save SR
	NOP_1_1				# get thru pipe before we zero it
	and	t0,s0,~SR_IE
	mtc0	t0,C0_SR		# disable interrupts
	NOP_1_1				# mfc0 cannot follow a mtc0

	# Size primary instruction cache.
	mfc0	t0,C0_CONFIG
	.set reorder
	and	t1,t0,CONFIG_IC
	srl	t1,CONFIG_IC_SHFT
	addi	t1,MIN_CACH_POW2
	li	t2,1
	sll	t2,t1
	sw	t2,_icache_size
	li	t2,ICACHE_LINESIZE
	sw	t2,_icache_linesize
	li	t2,ICACHE_LINEMASK
	sw	t2,_icache_linemask

	# Size primary data cache.
	and	t1,t0,CONFIG_DC
	srl	t1,CONFIG_DC_SHFT
	addi	t1,MIN_CACH_POW2
	li	t2,1
	sll	t2,t1
	sw	t2,_dcache_size
	li	t2,DCACHE_LINESIZE
	sw	t2,_dcache_linesize
	li	t2,DCACHE_LINEMASK
	sw	t2,_dcache_linemask

	li	t2,SCACHE_LINESIZE
	sw	t2,_scache_linesize
	li	t2,SCACHE_LINEMASK
	sw	t2,_scache_linemask
	LONG_S	t2,dmabuf_linemask	# __scunsigned_t

#ifdef	EVEREST
	/*
	 * Determine CPU #.
	 */
	jal	get_mpconf
	lw	t2,MP_SCACHESZ(v0)	#  Bang --> cache size (sorta)
	li	v0,1
	sll	v0,t2			# 2^^<MP_SCACHESZ> == # bytes
	.set	noreorder
#else
	LA	v0,1f
	or	v0,K1BASE
	.set	noreorder
	j	v0			# run uncached
	nop

1:	jal	size_2nd_cache
	nop
#endif
	# v0 has the size of the secondary (data or unified) cache.
	sw	v0,_sidcache_size

	bne	v0,zero,1f
	nop
	li	t2,DCACHE_LINEMASK
	sw	t2,_scache_linemask
	LONG_S	t2,dmabuf_linemask
	li	t2,DCACHE_LINESIZE
	sw	t2,_scache_linesize

#ifdef R4600
	sw	zero,_two_set_pcaches
	mfc0	t2,C0_PRID
	NOP_0_4
	andi	t2,0xFF00
	sub	t2,0x2000
	beqz	t2,99f			# br if r4600
	nop
	sub	t2,(0x2100 - 0x2000)
	beqz	t2,99f			# br if r4700
	nop
	sub	t2,(0x2300-0x2100)
	beqz    t2,99f                  # br if r5000
        nop
        sub     t2,(0x2800-0x2300)
	bnez	t2,1f			# br if not r5271
	nop
					# size 46sc 
99:	li	a0,CACHSZ_REG           # 17th word 2nd cache size in 4k pages
	jal	eerom_read
	nop
	sll	v0,12                   # convert size to bytes
	sw	v0,_r4600sc_sidcache_size
	beq	v0,zero,2f		# end size 46sc 
	nop

	li	t2,R4600_SCACHE_LINESIZE	#redo scache, dmabuf for 46SC
	sw	t2,_scache_linesize
	li	t2,R4600_SCACHE_LINEMASK
	sw	t2,_scache_linemask
	sw	t2,dmabuf_linemask


2:
	lw	t2,_dcache_size
	nop
	srl	t2,t2,1
	sw	t2,_two_set_pcaches
	li	t2,R4600_ICACHE_LINESIZE
	sw	t2,_icache_linesize
	li	t2,R4600_ICACHE_LINEMASK
	sw	t2,_icache_linemask
	li	t2,R4600_DCACHE_LINESIZE
	sw	t2,_dcache_linesize
	li	t2,R4600_DCACHE_LINEMASK
	sw	t2,_dcache_linemask
#endif
1:	LA	t0,1f

#if IP22
	/*
	 * make sure we are using the same KSEG space as the calling routine.
	 * look at sysinit() in IP22prom/IP22.c
	 */
	REG_L	ra,CONFIGFRM-(1*SZREG)(sp)
	li	t1,0x1fffffff
	and	t0,t1
	not	t1
	and	t1,ra
	or	t0,t1
#endif	/* IP22 */

	j	t0			# back to cached mode
	nop

1:	mtc0	s0,C0_SR		# restore SR
	.set	reorder
	REG_L	s0,CONFIGFRM-(2*SZREG)(sp)	# restore old s0
	REG_L	ra,CONFIGFRM-(1*SZREG)(sp)
	PTR_ADDU sp,CONFIGFRM
	j	ra
	END(config_cache)

#ifndef	EVEREST
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
	move	v0,zero			# return zero as scache size
	j	ra			# No second cache
1:
	li	v0,MINCACHE		# If MINCACHE == MAXCACHE, just
	li	t0,MAXCACHE		# return that
	bne	t0,v0,1f
	j	ra
1:
	# Load valid tags at each cache boundary up to MAXCACHE.
	# v0 has MINCACHE value.
	LI	t1,K0_RAMBASE
1:
	PTR_ADDU t2,t1,v0
	lw	zero,0(t2)
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
	LI	t0,K0_RAMBASE	# LDSLOT
	li	v0,MINCACHE		# LDSLOT
	cache	CACH_PI|C_IST,0(t0)
	cache	CACH_PD|C_IST,0(t0)
	cache	CACH_SD|C_IST,0(t0)
	.set	reorder
	.set	at
	# Now loop until that tag is found
	# v0 - MINCACHE
1:
	LI	t0,K0_RAMBASE	# Reload K0BASE for each iteration.
	PTR_ADDU t0,v0
	.set	noreorder
	.set	noat
	cache	CACH_SD|C_ILT,0(t0)
	NOP_0_3
	mfc0	t1,C0_TAGLO
	.set	reorder
	.set	at
	beq	t1,zero,2f		# Found the marker
	sll	v0,1			# Next power of 2
	blt	v0,+MAXCACHE,1b		# Iterate until MAXCACHE
2:
#ifndef IP32
	j	ra
#else /* !IP32 */
	sw	v0,_r5000_sidcache_size

/* for IP32, bit define by CONFIG_TR_SS tell the size
 *	00 == 512K
 *	01 == 1M
 *	10 == 2M
 *	11 == 4M
 */
	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	nop
	.set	reorder
	and	t0,CONFIG_TR_SS
	srl	t0,CONFIG_TR_SS_SHFT
	li	v0,MINCACHE
	sll	v0,t0
	j	ra
#endif /* !IP32 */
	END(size_2nd_cache)
#endif

#if defined(IP20) || defined(IP22)

/*
 * rmi_cacheflush(addr,len)
 */
LEAF(rmi_cacheflush)
	move	t0,a0			# starting addr.
	move	ta1,a1
	PTR_ADDU ta1,t0			# ending addr + 1

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
	li	t2,PHYS_RAMBASE		# when in low memory map, we must
	and	t3,t1,t2		# flush from normal memory space
	bnez	t3,2f
	nop
	or	t1,t2
2:
#endif
	lw	zero,(t1)		# cause writeback	
	cache	CACH_SD|C_HINV,0(t1)	# invalidate cache line
	.set	reorder
	PTR_ADDU t0,SCACHE_LINESIZE
	bltu	t0,ta1,1b
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
	bltu	t0,ta1,1b
			
	move	t0,a0
	# align the starting address.
	and	t2,t0,DCACHE_LINEMASK
	subu	t0,t2

1:
	.set	noreorder
	cache	CACH_PD|C_IWBINV,0(t0)
	.set	reorder

	addu	t0,DCACHE_LINESIZE
	bltu	t0,ta1,1b
	
	j	ra
#endif
	END(rmi_cacheflush)
#endif	/* IP20 */

/*
 * Invalidate the cache in question.
 * Contents of the cache is invalidated - no writebacks occur.
 * These do not assume memory is working, or stack pointer valid.
 *
 * These invalidate without causing writebacks. They
 * are useful for cleaning up the random power-on state.
 *
 * a0: cache size
 * a1: cache line size
 */
LEAF(invalidate_icache)
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	LI	t0,K0_RAMBASE
	PTR_ADDU a0,t0		# a0: cache size + K0base for loop termination
	NOP_0_1
	.set	reorder
1:
	.set	noreorder
	cache	CACH_PI|C_IST,0(t0)
	.set	reorder
	PTR_ADDU t0,a1
	bltu	t0,a0,1b		# a0 is termination count

	j	ra
	END(invalidate_icache)

LEAF(invalidate_dcache)
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	LI	t0,K0_RAMBASE
	PTR_ADDU a0,t0		# a0: cache size + K0base for loop termination
	NOP_0_1
	.set	reorder
1:
	.set	noreorder
	cache	CACH_PD|C_IST,0(t0)
	.set	reorder
	PTR_ADDU t0,a1
	bltu	t0,a0,1b		# a0 is termination count

	j	ra
	END(invalidate_dcache)

LEAF(invalidate_scache)
	# 2nd cache not guaranteed to exist - just return if not present
	.set	noreorder
	MFC0(t0,C0_CONFIG)
	nop
	nop
	.set	reorder
	and	t0,CONFIG_SC		# 0 -> 2nd cache present
	beq	t0,zero,1f
	j	ra			# No second cache
1:
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	LI	t0,K0_RAMBASE
	PTR_ADDU a0,t0		# a0: cache size + K0base for loop termination
	NOP_0_1
	.set	reorder
1:
	.set	noreorder
	cache	CACH_SD|C_IST,0(t0)
	.set	reorder
	PTR_ADDU t0,a1
	bltu	t0,a0,1b		# a0 is termination count

	j	ra
	END(invalidate_scache)

/*
 * Clobber all of the caches.
 * Assumes the variables _icache_size, _dcache_size, _sidcache_size
 * are valid.
 */
INVALFRAME=	FRAMESZ((4+1)*SZREG)	# 4 arg saves, ra
NESTED(invalidate_caches, INVALFRAME, zero)
	PTR_SUBU sp,INVALFRAME
	REG_S	ra,INVALFRAME-SZREG(sp)

	lw	a0, _icache_size
	lw	a1, _icache_linesize
	jal	invalidate_icache

	lw	a0, _dcache_size
	lw	a1, _dcache_linesize
	jal	invalidate_dcache

	lw	a0, _sidcache_size
	lw	a1, _scache_linesize
	jal	invalidate_scache

	REG_L	ra,INVALFRAME-SZREG(sp)
	PTR_ADDU sp,INVALFRAME
	j	ra
	END(invalidate_caches)
	
#if IP32
/*
 * Clobber all of the caches.
 * Assumes the variables _icache_size, _dcache_size, _sidcache_size
 * are valid.
 */
ENBSCFRAME=	FRAMESZ((4+1)*SZREG)	# 4 arg saves, ra
NESTED(r5000_enable_scache, ENBSCFRAME, zero)
	.set	noreorder
	PTR_SUBU sp,ENBSCFRAME
	REG_S	ra,ENBSCFRAME-SZREG(sp)
	mfc0	t0,C0_CONFIG
	nop
	nop
	or	t0,CONFIG_TR_SE
	mtc0	t0,C0_CONFIG
	nop
	nop
	.set	reorder
	REG_L	ra,INVALFRAME-SZREG(sp)
	PTR_ADDU sp,INVALFRAME
	j	ra
	END(r5000_enable_scache)
#endif /* IP32 */

/* _read_tag(WhichCache, address, &destination)
 * WhichCache == CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * address may be in KUSER or K0SEG space.
 * destination is a pointer to two uints.
 * a0: WhichCache
 * a1: address
 * a2: destination ptr
 * returns value of C0_ECC
 */
LEAF(_read_tag)
	and	a1, ~0x3
	li	t0, CACH_PD
	beq	a0, t0, rprimd	# primary d-cache tag
	bltu	a0, t0, rprimi	# fetch primary i-cache tag
#ifdef R4000PC
	lw	t1,_sidcache_size	
	bnez	t1,1f
	move	v0,zero
	j	ra
1:
#endif
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a1)	# fetch secondary (S|D) cache tag

getout:	
	nop
	nop
	mfc0	t0, C0_TAGLO
	# DO NOT READ C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	nop
	nop
	mfc0	v0, C0_ECC
	nop
	nop
	sw	t0, 0(a2)	# taglo is 1st uint in tag struct
	.set	reorder
	j	ra

rprimi:	.set	noreorder
	cache	CACH_PI|C_ILT, 0(a1)	# fetch primary instruction cache tag
	.set	reorder
	b	getout

rprimd:	.set	noreorder
	cache	CACH_PD|C_ILT, 0(a1)	# fetch primary data cache tag
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

LEAF(_write_tag)
	and	a1, ~0x3
	lw	t0, 0(a2)	# a2 pts to taglo: 1st uint in tag struct

	.set	noreorder
	mtc0	t0, C0_TAGLO	# just set taglo
	NOP_0_2
	.set	reorder
	li	t0, CACH_PD
	bltu	a0, t0, wprimi	# fetch primary i-cache tag
	beq	a0, t0, wprimd	# primary d-cache tag
#ifdef R4000PC
	lw	t1,_sidcache_size	
	beqz	t1,setout
#endif
	.set	noreorder
	cache	CACH_SD|C_IST, 0(a1)	# set secondary (I|D) cache tag
	.set	reorder

setout:	j	ra

wprimi:	.set	noreorder
	cache	CACH_PI|C_IST, 0(a1)	# set primary instruction cache tag
	.set	reorder
	b	setout

wprimd:	.set	noreorder
	cache	CACH_PD|C_IST, 0(a1)	# set primary data cache tag
	.set	reorder
	b	setout

	END(_write_tag)

LEAF(get_cache_err)
	.set    noreorder
	mfc0    v0,C0_CACHE_ERR
	.set    reorder
	j       ra
	END(get_cache_err)

LEAF(get_error_epc)
	.set    noreorder
	mfc0    v0,C0_ERROR_EPC
	.set    reorder
	j       ra
	END(get_error_epc)

LEAF(get_ecc)
	.set    noreorder
	mfc0    v0,C0_ECC		# get the current ECC value 
	.set    reorder
	j       ra
	END(get_ecc)

LEAF(set_ecc)
	.set	noreorder
	mtc0	a0,C0_ECC		# set the ECC register
	.set	reorder
	j	ra
	END(set_ecc)

#ifdef R4600
/*
 * _r4600sc_disable_cache()
 *
 * disable the secondary cache on an R4600SC
 */
LEAF(_r4600sc_disable_scache)
	li	a0,0
	LA	v0,_r4600sc_cache_index
	.set noreorder
	jalr	t9,v0
	nop
	mfc0	t0,C0_SR
	nop
#if _MIPS_SIM == _ABI64
	li	t1,SR_KX
	mtc0	t1,C0_SR
#else
	mtc0	zero,C0_SR
	li	t1,SR_KX
#endif
	nop
	LA	t2,1f
	lui	t3,0x2000
	and 	ta0,t3,t2		# check for addr in K0
	bne	ta0,zero,1f		# K1 space, dont fill PI cache
	nop
	cache	CACH_PI|C_FILL, 0(t2)
	nop
	cache	CACH_PI|C_FILL, 32(t2)
	j	1f
	nop
1:	
	.align 6
	mtc0	t1,C0_SR
	nop
	nop
	nop
	nop
	sh	zero, 0(v0)
#if _MIPS_SIM != _ABI64
	mtc0	zero,C0_SR
#endif
	nop
	nop
	nop
	nop	
	mtc0	t0,C0_SR
	nop
	.set reorder
	j	ra
	END(_r4600sc_disable_scache)

/*
 * _r4600sc_enable_cache()
 *
 * enable and invalidate the secondary cache on an R4600SC
 */
LEAF(_r4600sc_enable_scache)
	li	a0,0
	LA	v0,_r4600sc_cache_index
	.set noreorder
	jalr	t9,v0
	nop
	mfc0	t0,C0_SR
	nop
#if _MIPS_SIM == _ABI64
	li	t1,SR_KX
	mtc0	t1,C0_SR
#else
	mtc0	zero,C0_SR
	li	t1,SR_KX
#endif
	nop
	LA	t2,1f
	lui	t3,0x2000
	and 	ta0,t3,t2		# check for addr in K0
	bne	ta0,zero,1f		# K1 space, dont fill PI cache
	nop
	cache	CACH_PI|C_FILL, 0(t2)
	nop
	cache	CACH_PI|C_FILL, 32(t2)
	j	1f
	nop
1:	
	.align 6
	mtc0	t1,C0_SR
	nop
	nop
	nop
	nop
	sb	zero, 0(v0)
#if _MIPS_SIM != _ABI64
	mtc0	zero,C0_SR
#endif
	nop
	nop
	nop
	nop	
	mtc0	t0,C0_SR
	nop
	.set reorder
	j	ra
	END(_r4600sc_enable_scache)


/*
 * _r4600sc_cache_index(paddr)
 *
 * calculate the address to write to flush the line containing
 * the physical address "paddr".
 * note that since bits 4:0 are ignored, the index is contained
 * in bits 18:5
 *
 * The CCC only looks at bits 18:5 when determining which line to
 * invalidate, so we don't have to extract the index bits from
 * the address here -- the hardware is nice to us.
 */
LEAF(_r4600sc_cache_index)
	li	ta3,0x1				# create R4600_CCC_BASE
	dsll	ta3,31				# ...
	or	v0,a0,ta3			# physical address to write

	lui	ta3,XKPHYS_UNCACHED_BASE
	dsll32	ta3,XKPHYS_UNCACHED_SHIFT-0x20	# create 64 bit base address
	or	v0,ta3				# xkphys address to write
	j	t9
	END(_r4600sc_cache_index)

/*
 * _r4600sc_cache_on_test()
 *
 * determine if the r4600sc cache is on and set cache state global variable
 *
 * Assumes code and stack are in cached K0 space, the stack is used for a
 * scratch memory while testing the cache.
 *
 */
SCONTSTFRM=	FRAMESZ(8*SZREG)		# 8 words per cache line
LEAF(_r4600sc_cache_on_test)
	.set	noreorder
	PTR_SUBU sp,SCONTSTFRM		# use stack for cache testing address
	sw	zero,_r4600sc_cache_on
	lw	ta3,_r4600sc_sidcache_size
	nop
	beqz	ta3,99f			# no scache--state = off?
	nop
#if _MIPS_SIM != _ABI64
	lui	t3,0x2000		# address bits mask for K1->K0 
	xor 	t2,t3,sp			
	and	t2,t2,sp		# make sure it pts to K0 space
	srl	t2,6
	sll	t2,6			# align ptr to cache line boundry
#else
	li	ta0,0x1fffffff		# to phys mask
	and	t2,ta0,sp
	or	t2,K0BASE		# cached
	dsrl	t2,6
	dsll	t2,6			# align ptr to cache line boundry
	LI	t3,0x1000000000000000	# uncached bit (except compat)
#endif
	
	mfc0	t0,C0_SR
	nop
#if _MIPS_SIM == _ABI64
	li	ta0,SR_KX
	mtc0	ta0,C0_SR
#else
	mtc0	zero,C0_SR
#endif
	nop
	LA	t1,1f
	and 	ta0,t3,t1		# check for addr in K0
	bne	ta0,zero,1f		# K1 space, dont fill PI cache
	nop
	cache	CACH_PI|C_FILL, 0(t1)
	nop
	cache	CACH_PI|C_FILL, 32(t1)
	j	1f
	nop
	nop
1:	
	.align 6
#if _MIPS_SIM != _ABI64
	or	ta0,t2,K1BASE		# ptr to same addr in K1 space
#else
	and	ta0,t2,0x1fffffff	# to phys
	or	ta0,K1BASE		# ptr to same addr in K1 space
#endif
	lw	ta1,0(t2)		# read from K0 space to fill caches
	cache	CACH_PD|C_HINV,0(t2)	# invalidate primary
	xori	ta2,ta1,0xffff		# complement lower halfword of data
	sw	ta2,0(ta0)		# write it back to memory thru K1 space
	nop
	lw	ta1,0(t2)		# read addr again
	nop
	beq	ta2,ta1,2f		# if eql got it from memory, cache off
	nop
	li	ta1,0x01
	sw	ta1,_r4600sc_cache_on	# else the cache is on	
2:
	nop
	mtc0    t0,C0_SR                # restore status register
	nop

99:

	PTR_ADDU sp,SCONTSTFRM		# restore stack ptr
	.set reorder
	j	ra
	END(_r4600sc_cache_on_test)

#endif

#if IP19
LEAF(ip19_cache_init)
/*
 * Routine:	ip19_cache_init
 * Purpose:	Initialize the cache tags to a known "legal" state.
 * Parameters:	None
 * Returns:	Nothing
 * Notes:	This routine accomplishes its task in
 *		the following steps, and uses the CONFIG register and the
 *		MPCONF structure to determine the cache sizes.
 *
 *		1) Index write-back, invalidate on all secondary cache lines.
 *		   This is required if symmon is running cached to be sure
 *		   we don't lose any data.  This in itself *COULD* cause a 
 *		   problem if the caches are not inited properly, but it 
 *		   appears to usually work - so if an older prom is installed,
 *		   this makes things more likely to work.
 *		2) Set all Primary data/instruction tags to contain a valid
 *		   address but set the line invalid.
 *		3) Set all secondary cache tags to contain a valid address,
 *		   such that the primary cache tags are a subset of the
 *		   secondary tags even though all are invalid.
 *
 */
	/* Get secondary cache size */

	LI	t2,EV_CACHE_SZ_LOC
	ld	t2,0(t2)
	and	t2,0xff
	LI	v0,1
	sll	v0,t2			/* Size in bytes */

	.set	noreorder
	
	LI	a0,K0BASE
	PTR_ADDU a1,a0,v0		/* address of byte after last line */

	LA	t1,1f			/* Set up to run uncached */
	LI	t2,TO_PHYS_MASK
	and	t1,t2
	LI	t2,K1BASE
	or	t1,t2
	j	t1			/* Bang - uncached */

	.align	7

	/* Flush cache */

1:	
	.set	noreorder
	cache	CACH_SD+C_IWBINV,0(a0)
	bltu	a0,a1,1b
	PTR_ADDU a0,SCACHE_LINESIZE	/* BDSLOT */

	/* Write primary tags */

	mfc0	t0,C0_CONFIG
	nop
	and	t0,CONFIG_IC
	srl	t0,CONFIG_IC_SHFT
	add	t0,12
	li	a1,1
	sll	a1,t0			/* IC size in a1 */
	
	move	a0,zero			/* Start Address in a0 */
	mtc0	zero,C0_TAGHI
1:
	
	PTR_SRL	a2,a0,12
	PTR_SLL	a2,8
	mtc0	a2,C0_TAGLO
	or	a2,a0,K0BASE
	cache	CACH_PI+C_IST,0(a2)
	PTR_ADD	a0,ICACHE_LINESIZE	/* Next cache line */
	bltu	a0,a1,1b
	nop

	mfc0	t0,C0_CONFIG
	nop
	and	t0,CONFIG_DC
	srl	t0,CONFIG_DC_SHFT
	add	t0,12
	li	a1,1
	sll	a1,t0			/* IC size in a1 */
	
	move	a0,zero			/* Start Address in a0 */
	mtc0	zero,C0_TAGHI
1:
	
	PTR_SRL	a2,a0,12
	PTR_SLL	a2,8
	mtc0	a2,C0_TAGLO
	or	a2,a0,K0BASE
	cache	CACH_PD+C_IST,0(a2)
	PTR_ADD	a0,DCACHE_LINESIZE	/* Next cache line */
	bltu	a0,a1,1b
	nop

	/* Write secondary tags */

	move	a0,zero
	move	a1,v0
1:
	PTR_SRL a2,a0,17
	PTR_SLL	a2,13
	PTR_SRL	a3,a0,12
	and	a3,7
	PTR_SLL	a3,7
	or	a3,a2
	mtc0	a3,C0_TAGLO
	or	a3,a0,K0BASE
	cache	CACH_SD+C_IST,0(a3)
	PTR_ADDI a0,SCACHE_LINESIZE
	bltu	a0,a1,1b
	nop

	.set	reorder
	j	ra			/* Back to cached space */

	.align	7
	
	END(ip19_cache_init)
#endif	/* IP19 */
	
