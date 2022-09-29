/*
 * IP26 specific assembly routines; cpuid always 0, also make semaphore
 * macros a no-op.
 */
#ident "$Revision: 1.46 $"

#include "ml/ml.h"

/*	dummy routines whose return value is unimportant (or no return value).
	Some return reasonable values on other machines, but should never
	be called, or the return value should never be used on other machines.
*/
LEAF(dummy_func)
XLEAF(delay_calibrate)
XLEAF(check_delay_tlbflush)
XLEAF(check_delay_iflush)
XLEAF(da_flush_tlb)
XLEAF(dma_mapinit)
XLEAF(apsfail)
XLEAF(disallowboot)
XLEAF(vme_init)
XLEAF(vme_ivec_init)
XLEAF(debug_stop_all_cpus)
	j ra
END(dummy_func)

/* Data cache operation aliases.
 */
LEAF(dcache_wb)
XLEAF(dcache_wbinval)
XLEAF(dki_dcache_wb)
XLEAF(dki_dcache_wbinval)
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_WBACK|CACH_IO_COHERENCY
	j	cache_operation
	END(dcache_wb)

LEAF(dki_dcache_inval)
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_IO_COHERENCY
	j	cache_operation
	END(dki_dcache_inval)

/* dummy routines that return 0 */
LEAF(dummyret0_func)

XLEAF(vme_adapter)
XLEAF(is_vme_space)
XLEAF(getcpuid)
#ifdef DEBUG
XLEAF(getcyclecounter)
#endif /* DEBUG */

/* Semaphore call stubs */
XLEAF(appsema)
XLEAF(apvsema)
XLEAF(apcvsema)
XLEAF(cache_preempt_limit)
	move	v0,zero
	j ra
END(dummyret0_func)

/* dummy routines that return 1 */
LEAF(dummyret1_func)
XLEAF(apcpsema)	/* can always get on non-MP machines */
	li	v0, 1
	j ra
END(dummyret1_func)

/* writemcreg(caddr_t reg, int val)
 * 
 * Basically this does *(volatile uint *)(PHYS_TO_K1(reg)) = val;
 *	a0 - physical register address
 *	a1 - value to write
 *
 * This was a workaround for a bug in the first rev MC chip, but IP26
 * has only rev C (or greater) MCs, so just do the actual operation.
 */
LEAF(writemcreg)
	or	a0,K1BASE	# a0 = PHYS_TO_K1(a0)
	sw	a1,(a0)
	j	ra
END(writemcreg)

/* Write the VDMA MEMADR, MODE, SIZE, STRIDE registers
 *
 * write4vdma (buf, mode, size, stride);
 */
LEAF(write4vdma)
	/* XXX Only works because bit 15 not set in DMA_MEMADR */
        LI     v0, (K1BASE | DMA_MEMADR) & (~0xffff)

        sw      a0,DMA_MEMADR & 0xffff(v0)
        sw      a1,DMA_MODE   & 0xffff(v0)
        sw      a2,DMA_SIZE   & 0xffff(v0)
        sw      a3,DMA_STRIDE & 0xffff(v0)

        j       ra
END(write4vdma)

/*  Index based writeback or writeback and invalidate routines to flush the
 * GCache (and the IU's DCache) using the built-in TCC cache ops.  Address
 * must be in physical, K0, or K1 (cannot handle a mapped address since
 * cacheops take a physical address).
 *
 * We must operate on all 4 sets to make sure we get the address in question.
 * The DCache (unlike the ICache) is a propper subset of the GCache, which
 * ensures propper flushing.
 *
 * NOTE: This code assumes a 2MB, 4 way set associative cache.  TCC can
 *	 support a 4MB (max) cache in theory, but in practice the extra
 *	 index bit is unused.
 *
 *	 We do cannot support only invalidate on index based ops with a
 *	 set associative cache as we will invalidate others data.
 *
 * void __cache_wb_inval(void *, int length);
 * void __cache_wb(void *, int length);
 *
 */
LEAF(__cache_wb)
	dli	t3,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INDEX_OP
	j	1f
XLEAF(__cache_wb_inval)
	dli	t3,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE|TCC_INDEX_OP

	/* Invalidate prefetch buffers on invalidation cache ops */
	dli	t2,K1BASE|TCC_PREFETCH		# prefetch control register
	ld	t1,0(t2)
	ori	t1,PRE_INV			# set invalidate bit
	sd	t1,0(t2)			# do invalildation op
	/*FALLSTHROUGH*/

1:
#if DEBUG		/* ensure we do not have a K2 address */
	LI	t1,K2BASE
	and	t2,t1,a0
	bne	t1,t2,1f			# branch if not K2
	move	a1,a0				# address
	LA	a0,msg				# string
	jal	panic
	.data
msg:	.asciiz	"IP26 low-level cache flushing got K2 address: 0x%x"
	.text
1:	
#endif
	#  Calculate the number of indices to invalidate (a1).
	#
	addi	a1,TCC_LINESIZE-1		# round up to next cache line
	srl	a1,TCC_CACHE_INDEX_SHIFT	# drop insignificant bits

	#  Mask to pull index out of address.
	#
	li	t1,TCC_CACHE_INDEX

	#  Make sure number of indices is not bigger than the maximum index.
	#
	li	t0,TCC_CACHE_INDEX>>TCC_CACHE_INDEX_SHIFT
	blt	a1,t0,1f
	move	a1,t0

1:	and	t0,a0,t1			# isolate current index
	or	t0,t0,t3			# make cache op addr
	addi	a1,-1				# decrement line count

	lw	zero,0(t0)			# set 0
	lw	zero,1<<TCC_CACHE_SET_SHIFT(t0)	# set 1
	lw	zero,2<<TCC_CACHE_SET_SHIFT(t0)	# set 2
	lw	zero,3<<TCC_CACHE_SET_SHIFT(t0)	# set 3

	addi	a0,TCC_LINESIZE			# next index (use word add)

	bgez	a1,1b				# loop if more indexes

	j	ra
	END(__cache_wb)

/*  Hit based invalidate, write back, or write back and invalidate the
 * GCache (and the IU's DCache) using the built-in TCC cache ops.  Address
 * must be in physical, K0, or K1 (cannot handle a mapped address since
 * cacheops take a physical address).
 *
 * void __dcache_inval(void *address, int length)
 * void __dcache_wb(void *address, int length)
 * void __dcache_wb_inval(void *address, int length)
 *
 */
LEAF(__dcache_inval)
	dli	t2,K1BASE|TCC_CACHE_OP|TCC_INVALIDATE
	andi	t3,a0,(TCC_LINESIZE-1)		# non-aligned address
	andi	a5,a1,(TCC_LINESIZE-1)		# non-blocked length
	or	a6,t3,a5
	beqz	a6,2f				# do full block invalidation

	li	a7,TCC_PHYSADDR			# significant address bits
	dli	a6,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE

	beqz	t3,1f				# WB first block?
	and	t1,a0,a7			# get phys address
	or	t1,a6				# construct address
	lw	zero,0(t1)			# wb inval line
	li	t1,TCC_LINESIZE
	sub	t3,t1,t3			# size of flushed block
	subu	a1,t3				# update length
	addu	a0,t3				# update starting point
	andi	a5,a1,(TCC_LINESIZE-1)		# new non-blocked length

1:	beqz	a5,3f				# WB last block?
	addu	t1,a0,a1			# ending address
	subu	a1,a5				# update length
	and	t1,a7				# get phys address
	or	t1,a6				# construct address
	lw	zero,0(t1)			# wb inval line

3:	bnez	a1,2f				# continue if length != 0

	b	7f				# go inval prefetch + return

XLEAF(__dcache_wb)
	dli	t2,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB
	j	2f

XLEAF(__dcache_wb_inval)
	dli	t2,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE

2:	andi	t0,a0,TCC_LINESIZE-1		# get rounding amount
	add	a1,a1,t0			# add rounding amount to length
	li	t0,TCC_PHYSADDR			# significant address bits
	and	a0,a0,t0			# mask off un-need bits
	or	a0,a0,t2			# calc cache-op addr

	.align	4				# loop is one quad
	.set	noreorder
1:	lw	zero,0(a0)			# cache op on line 'a0'
	addi	a1,a1,-TCC_LINESIZE		# decr length
	bgtz	a1,1b				# on to next cache line
	daddiu	a0,a0,TCC_LINESIZE		# BDSLOT incr cache op addr
	.set	reorder

	/* Invalidate prefetch buffers on invalidation cache ops */
7:	dli	t3,K1BASE|TCC_PREFETCH		# prefetch control register
	ld	t1,0(t3)			# get current state
	ori	t1,PRE_INV			# set invalidate bit
	sd	t1,0(t3)			# do invalildation op

	j	ra
	END(__dcache_inval)

/* Write back/invalidate one line from the cache.  This can be used by drivers
 * (enet uses it now) to have a lower overhead cacheflush when getting around
 * problems with IP26 ECC baseboard.  Assumes this cache line will not have
 * been prefetched (and hence need to invalidate prefetch buffers).
 */
LEAF(__dcache_line_wb_inval)
	dli	t2,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE

	li	t0,TCC_PHYSADDR			# significant address bits
	and	a0,a0,t0			# mask off un-need bits
	or	a0,a0,t2			# calc cache-op addr
	lw	zero,0(a0)			# cache op on line 'a0'
	j	ra
	END(__dcache_line_wb_inval)

/*  Various routines to get TFP C0 registers.
 */
LEAF(get_config)
	.set	noreorder
	dmfc0	v0, C0_CONFIG
	j	ra
	nada					# BDSLOT
	.set	reorder
	END(get_config)

LEAF(set_config)
	.set	noreorder
	dmtc0	a0, C0_CONFIG
	j	ra
	nada					# BDSLOT
	.set	reorder
	END(set_config)

LEAF(_get_timestamp)
XLEAF(get_r4k_counter)				/* for compat */
	.set	noreorder
	dmfc0	v0, C0_COUNT
	j	ra
	nada					# BDSLOT
	.set	reorder
	END(_get_timestamp)

LEAF(get_trapbase)
	.set	noreorder
	dmfc0	v0,C0_TRAPBASE
	j	ra
	nada					# BDSLOT
	.set	reorder
	END(get_trapbase)

LEAF(set_trapbase)
	.set	noreorder
	dmtc0	a0,C0_TRAPBASE
	j	ra
	nada
	.set	reorder
	END(set_trapbase)

/*  Clear TFP count interrupt by clearing CAUSE_IP11 (don't zero count
 * as this is now fatal as we do not enable the counter interrupt).
 */
LEAF(_tfpcount_intr)
	.set	noreorder
	dmfc0	t0,C0_CAUSE
	ssnop
	dli	t1,~CAUSE_IP11
	and	t0,t1
	dmtc0	t0,C0_CAUSE
	ssnop
	ssnop
	j	ra
	nada
	.set	reorder
	END(_tfpcount_intr)

/* Count the number of TCC Fifo Cycles vs the X number of SysAd
 * Cycles - the larger the number of sysad cycles, the less
 * percentage error hit the caller gets
 */
LEAF(CountTccVsSysAdCycles)
	.set	noreorder
	LI	t0,PHYS_TO_K1(TCC_COUNT)	# TCC counter == SysAD freq
	sd	zero,0(t0)
	move	t2,zero				# initialize counter
	dmtc0	zero,C0_COUNT			# zero TFP counter
	ssnop					# C0 hazzard
	ssnop					# C0 hazzard #2
1:	dmfc0	v0,C0_COUNT			# get current count before ld
	ssnop					# C0 hazzard
	ld	t2,0(t0)			# get sysAD count
	blt	t2,a0,1b			# enough cycles?
	nada					# BDSLOT
	j	ra
	nada					# BDSLOT
	.set	reorder
	END(CountTccVsSysAdCycles)

/*  Specialized teton pagecopy routine that is SWP'ed to take advantage
 * of the hardware support for prefetching to memory hide latency.
 *
 *  _pagecopy(void *src, void *dst, int len)
 *
 *  Assumes src and dst are both cache line aligned and len is a multiple
 * of (n*2*TCC_LINESIZE)+2*TCC_LINESIZE, ie an even number of cache lines
 * greater than or equal to 4.
 *
 *  The code does not copy the last two lines to avoid prefetching past the
 * end of the page which can cause a MC bus error when done on the last
 * page of memory or the last page before the hole.
 *
 *  Be very careful as the code here is carefully quad aligned.  Losing
 * propper alignment will reduce performance!
 *
 *  Caller saves the dwong patch state.
 */
#define PREF(offset,reg)	pref	0,offset(reg)
LEAF(_pagecopy)
	.align	4
	.set	noreorder
	beqz	a2,2f			# skip zero length copies
	sltu	t1,a0,a1		# BDSLOT: if src < dst
	bnez	t1,_pagecopy_backwards	# then do backwards copy
	nada
	PREF(TCC_LINESIZE,a0)		# request second src line
	nada
	nada
	addi	a2,-(2*TCC_LINESIZE)	# do last 2 lines seperately
	PREF(TCC_LINESIZE,a1)		# second dst line
	li	t0,2*TCC_LINESIZE	# size of trailer
	nada
	nada
1:
	ldc1	$f4,0(a0)	;	sdc1	$f4, 0(a1)
	PREF(2*TCC_LINESIZE,a0)	;	ldc1	$f4, 8(a0)
	PREF(2*TCC_LINESIZE,a1)	;	sdc1	$f4, 8(a1)
	ldc1	$f4,16(a0)	;	sdc1	$f4, 16(a1)
	ldc1	$f4,24(a0)	;	sdc1	$f4, 24(a1)
	ldc1	$f4,32(a0)	;	sdc1	$f4, 32(a1)
	ldc1	$f4,40(a0)	;	sdc1	$f4, 40(a1)
	ldc1	$f4,48(a0)	;	sdc1	$f4, 48(a1)
	ldc1	$f4,56(a0)	;	sdc1	$f4, 56(a1)
	ldc1	$f4,64(a0)	;	sdc1	$f4, 64(a1)
	ldc1	$f4,72(a0)	;	sdc1	$f4, 72(a1)
	ldc1	$f4,80(a0)	;	sdc1	$f4, 80(a1)
	ldc1	$f4,88(a0)	;	sdc1	$f4, 88(a1)
	ldc1	$f4,96(a0)	;	sdc1	$f4, 96(a1)
	ldc1	$f4,104(a0)	;	sdc1	$f4, 104(a1)
	ldc1	$f4,112(a0)	;	sdc1	$f4, 112(a1)
	ldc1	$f4,120(a0)	;	sdc1	$f4, 120(a1)
	addi	a2,-TCC_LINESIZE;	daddiu	a0,TCC_LINESIZE

	daddiu	a1,TCC_LINESIZE		# BDSLOT: next dst cache line
	bgtz	a2,1b			# keep going?
	nada				# BDSLOT keep alignment
	nada				# keep alignment

1:	ldc1	$f4,0(a0)	;	sdc1	$f4, 0(a1)
	ldc1	$f4,8(a0)	;	sdc1	$f4, 8(a1)
	ldc1	$f4,16(a0)	;	sdc1	$f4, 16(a1)
	ldc1	$f4,24(a0)	;	sdc1	$f4, 24(a1)

	addi	t0,-32			# done with one chunk
	daddiu	a0,32			# next src chunk
	bgtz	t0,1b			# keep going?
	daddiu	a1,32			# BDSLOT: next dst chunk

2:	j	ra
	nada
	nada				# keep alignment
	nada

_pagecopy_backwards:
	daddu	a0,a2			# start with ending addresses
	daddu	a1,a2
	PREF(-2*TCC_LINESIZE,a0)	# request second src line
	nada
	nada
	addi	a2,-(2*TCC_LINESIZE)	# do last 2 lines seperately
	PREF(-2*TCC_LINESIZE,a1)	# second dst line
	li	t0,2*TCC_LINESIZE	# size of trailer
1:
	ldc1	$f4, -8(a0)	;	sdc1	$f4, -8(a1)
	PREF(-3*TCC_LINESIZE,a0);	ldc1	$f4,-16(a0)
	PREF(-3*TCC_LINESIZE,a1);	sdc1	$f4,-16(a1)
	ldc1	$f4,-24(a0)	;	sdc1	$f4,-24(a1)
	ldc1	$f4,-32(a0)	;	sdc1	$f4,-32(a1)
	ldc1	$f4,-40(a0)	;	sdc1	$f4,-40(a1)
	ldc1	$f4,-48(a0)	;	sdc1	$f4,-48(a1)
	ldc1	$f4,-56(a0)	;	sdc1	$f4,-56(a1)
	ldc1	$f4,-64(a0)	;	sdc1	$f4,-64(a1)
	ldc1	$f4,-72(a0)	;	sdc1	$f4,-72(a1)
	ldc1	$f4,-80(a0)	;	sdc1	$f4,-80(a1)
	ldc1	$f4,-88(a0)	;	sdc1	$f4,-88(a1)
	ldc1	$f4,-96(a0)	;	sdc1	$f4,-96(a1)
	ldc1	$f4,-104(a0)	;	sdc1	$f4,-104(a1)
	ldc1	$f4,-112(a0)	;	sdc1	$f4,-112(a1)
	ldc1	$f4,-120(a0)	;	sdc1	$f4,-120(a1)
	ldc1	$f4,-128(a0)	;	sdc1	$f4,-128(a1)
	addi	a2,-TCC_LINESIZE;	daddiu	a0,-TCC_LINESIZE

	daddiu	a1,-TCC_LINESIZE	# BDSLOT: next dst cache line
	bgtz	a2,1b			# keep going?
	nada				# BDSLOT keep alignment
	nada				# keep alignment

1:	ldc1	$f4, -8(a0)	;	sdc1	$f4, -8(a1)
	ldc1	$f4,-16(a0)	;	sdc1	$f4,-16(a1)
	ldc1	$f4,-24(a0)	;	sdc1	$f4,-24(a1)
	ldc1	$f4,-32(a0)	;	sdc1	$f4,-32(a1)

	addi	t0,-32			# done with one chunk
	daddiu	a0,-32			# next src chunk
	bgtz	t0,1b			# keep going?
	daddiu	a1,-32			# BDSLOT: next dst chunk

	j	ra
	nada
	.set	reorder
	END(_pagecopy)

/*  Specialized teton pagezero routine that is SWP'ed to take advantage
 * of the hardware support for prefetching to memory hide latency.
 *
 *	_pagezero(void *dst, int len)
 *
 *  The dst address must be page cache aligned, and the length must be
 * a (n*4*TCC_LINESIZE)+4*TCC_LINESIZE.  Since this is a bit odd it's
 * probably best to assume len some multiple of 8*TCC_LINESIZE (2KB).
 *
 *  The loop is prefetches four lines ahead.  The last four cache lines are
 * done seperately to avoid fetching past the end of the page (and they
 * have already been prefetched.
 *
 *  The main loop takes 9 cycles once the cache line has been fetched.  This
 * is one additional cycle per cache line for lines that are in the GCache,
 * but overall we should save more cycles when prefetching helps.
 *
 *  Caller saves the dwong patch state.
 */
LEAF(_pagezero)
	.set	noreorder
	beqz	a1,2f			# make sure length is non-zero
	addi	a1,-(4*TCC_LINESIZE)	# do last 4 lines seperately
	PREF(0,a0)			# first line
	PREF(TCC_LINESIZE,a0)		# second line
	dmtc1	zero,$f4		# get those zero bits ready
	PREF(2*TCC_LINESIZE,a0)		# third line
	li	t0,4*TCC_LINESIZE	# size of secondary copy
	PREF(3*TCC_LINESIZE,a0)		# fourth line

1:	PREF(4*TCC_LINESIZE,a0)	;	nada				# [0]
	nada			;	addi    zero,zero,0		# [0]
	sdc1	$f4,0(a0)	;	sdc1	$f4,8(a0)		# [1]
	sdc1	$f4,16(a0)	;	sdc1	$f4,24(a0)		# [2]
	sdc1	$f4,32(a0)	;	sdc1	$f4,40(a0)		# [3]
	sdc1	$f4,48(a0)	;	sdc1	$f4,56(a0)		# [4]
	sdc1	$f4,64(a0)	;	sdc1	$f4,72(a0)		# [5]
	sdc1	$f4,80(a0)	;	sdc1	$f4,88(a0)		# [6]
	sdc1	$f4,96(a0)	;	sdc1	$f4,104(a0)		# [7]
	addi	zero,zero,0	;	addi	a1,-TCC_LINESIZE	# [7]
	sdc1	$f4,112(a0)	;	sdc1	$f4,120(a0)		# [8]
	bgtz	a1,1b			# loop more?			  [8]
	daddiu	a0,TCC_LINESIZE		# BDSLOT: bump address		  [8]

1:	sdc1	$f4,0(a0)	;	sdc1	$f4,8(a0)		# [0]
	sdc1	$f4,16(a0)	;	sdc1	$f4,24(a0)		# [1]
	sdc1	$f4,32(a0)	;	sdc1	$f4,40(a0)		# [2]
	addi	t0,-TCC_LINESIZE;	addi	zero,zero,0		# [2]
	sdc1	$f4,48(a0)	;	sdc1	$f4,56(a0)		# [3]
	sdc1	$f4,64(a0)	;	sdc1	$f4,72(a0)		# [4]
	sdc1	$f4,80(a0)	;	sdc1	$f4,88(a0)		# [5]
	sdc1	$f4,96(a0)	;	sdc1	$f4,104(a0)		# [6]
	sdc1	$f4,112(a0)	;	sdc1	$f4,120(a0)		# [7]
	bgtz	t0,1b			# loop more?			  [7]
	daddiu	a0,TCC_LINESIZE		# BDSLOT: next address		  [7]

2:	j	ra
	nada
	.set	reorder
	END(_pagezero)

/* Save stuff in C0_WORK1 register
 */
LEAF(set_work1)
	.set	noreorder
	DMTC0(a0,C0_WORK1)
	.set	reorder
	j	ra
	END(set_work1);

/*  Calculate speed ratio between TFP side and sysAD side of teton,
 * which we use to set cache refil characteristics.  We handle
 * ratios (TFP/Sysad) from 0/1 to 2/1.  The value to use for fast
 * mode is returned to the caller for later use.
 */
LEAF(tune_tcc_gcache)
	.set	noreorder
	LI	t0,PHYS_TO_K1(TCC_COUNT)	# TCC counter == SysAD freq
	sd	zero,0(t0)
	move	t2,zero				# initialize counter
	dmtc0	zero,C0_COUNT			# zero TFP counter
	ssnop					# C0 hazzard
	li	t3,2000				# count 2000 SysAd cycles
1:	dmfc0	a0,C0_COUNT			# get current count before ld
	ld	t2,0(t0)			# get sysAD count
	blt	t2,t3,1b			# enough cycles?
	nada
	.set	reorder

	srl	a0,1				# divide down to 1000's
	sub	a0,1000				# get ratios >= 1.0
	blt	a0,zero,1f			# use default if ratio < 1.0
	li	t0,10				# divide down to 100's
	div	a0,t0
	mflo	a0
	andi	a0,0xfe				# even indicies only

	li	t0,100				# check range of index
	ble	a0,t0,1f
	move	a0,zero				# use default if > 2.0
	
1:	LA	t0,tcc_fiforatios
	daddu	t0,a0				# index short array
	lh	a0,0(t0)
	sll	a0,GCACHE_RR_FULL_SHIFT		# shift into position

	LI	t0,PHYS_TO_K1(TCC_GCACHE)
	ld	v0,0(t0)
	dli	t2,~(GCACHE_RR_FULL|GCACHE_WB_RESTART|GCACHE_REL_DELAY)
	and	v0,t2
	or	v0,a0				# set our new bits

	j	ra				# return new setting

	END(tune_tcc_gcache)

#define MEMACC_XOR		(CPU_MEMACC_SLOW&0x3fff)
#define CPU_MEMACC_OFFSET	CPU_MEMACC-CPUCTRL0

/* Enable uncachedable writes via slow memory, returning the old state.
 *
 * Critical section on one cache line to prevent writebacks during
 * the mode switch.
 *
 * The Read from K1BASE below is to force a read from main memory before
 * entering slow mode.  This will check for any bad (uncached) writes
 * done previously, which have gone undetected until here.  (The bus error
 * can't be raised for an uncached write until the next read cycle.)
 *
 * GCache indicies used:
 *	TCC, MC, PAL PIO	0
 *	critical cache line	odd
 *	write flush cache line	2
 */
LEAF(ip26_enable_ucmem)
	LI	a0,K1BASE			# K1
	LI	a6,K0BASE			# K0
	lw	zero,0(a0)			# Force mem read to flush errs.
	li	a2,CPU_MEMACC_SLOW		# slow memory setting
	or	a1,a0,TCC_BASE			# PHYS_TO_K1(TCC_BASE)
	or	a5,a0,HPC3_SYS_ID		# PHYS_TO_K1(HPC3_SYS_ID)
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a7,a0,TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)
	ld	a3,tcc_gcache_slow		# the slow value to write
	li	t3,PRE_INV			# prefetch off + invalidated

	beqz	a3,2f				# not initialized yet

	lw	t1,0(a5)			# get system id
	andi	t1,BOARD_REV_MASK		# isolate board rev
	sltiu	a5,t1,IP26_ECCSYSID		# 1=IP22, 0=IP26

	.set	noreorder
	dmfc0	t0,C0_SR			# disable interrupts
	ssnop
	ssnop
	ori	t1,t0,SR_IE
	xori	t1,SR_IE
	.align	8
	/*  The critical section fits on one cache line.  However we want
	 * the GCache index of this cache line to be odd so it is not
	 * affected by the PIO's or the flush cache line.
	 */
	b 1f; nada; nada; nada; nada; nada; nada; nada
	nada; nada; nada; nada; nada; nada; nada; nada
	nada; nada; nada; nada; nada; nada; nada; nada
	nada; nada; nada; nada; nada; nada; nada; nada

1:	dmtc0	t1,C0_SR			# critical begin
	ssnop; ssnop; ssnop; ssnop		# drain the pipe

	sd	t3,TCC_PREFETCH-TCC_BASE(a1)	# invalidate prefetch buffers
	lw	t1,CPU_MEMACC_OFFSET(a0)	# get MC memory config
	andi	v0,t1,0x3fff			# important bits
	xori	v0,MEMACC_XOR			# 0=slow, !0=normal

	sd	a3,TCC_GCACHE-TCC_BASE(a1)	# go to slow mode on TCC
	lw	zero,0(a0)			# flushbus
	sd	zero,2*TCC_LINESIZE(a6)		# bring cache line in
	sw	a2,CPU_MEMACC_OFFSET(a0)	# go to slow mode on MC
	lw	zero,0(a0)			# flushbus
	bnez	a5,1f				# skip ECC on IP22
	nada					# BDSLOT
	li	a2,ECC_CTRL_DISABLE		# disable ECC chk (uc writes ok)
	sd	a2,0(a4)			# Enter slow mode.
	ssnop					# break super scalar dispatch
	lw	zero,0(a0)			# flushbus
	lw	zero,2*TCC_LINESIZE(a7)		# flush cache line
						# end of critical

1:	dmtc0	t0,C0_SR			# restore C0_SR
	ssnop
	ssnop
	.set	reorder

2:	j	ra
	END(ip26_enable_ucmem)

/* Disable uncacheable writes via faster memory, returning the old state.
 *
 * Critical section on one cache line to prevent writebacks during
 * the mode switch.
 */
LEAF(ip26_disable_ucmem)
	LI	a0,K1BASE			# K1
	li	a2,CPU_MEMACC_NORMAL		# normal memory setting
	or	a1,a0,TCC_BASE			# PHYS_TO_K1(TCC_BASE)
	or	a5,a0,HPC3_SYS_ID		# PHYS_TO_K1(HPC3_SYSID)
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)
	ld	a3,tcc_gcache_normal		# the normal value to write
	li	t3,PRE_DEFAULT|PRE_INV		# re-enable prefetch + inval

	beqz	a3,2f				# not initialized yet

	lw	t1,0(a5)			# get system id
	andi	t1,BOARD_REV_MASK		# isolate board rev
	sltiu	a5,t1,IP26_ECCSYSID		# 1=IP22, 0=IP26

	.set	noreorder
	dmfc0	t0,C0_SR			# disable interrupts
	ssnop
	ssnop
	ori	t1,t0,SR_IE
	xori	t1,SR_IE

	.align	8
	/*  The critical section fits on one cache line.  However we want
	 * the GCache index of this cache line to be odd so it is not
	 * affected by the PIO's or the flush cache line.
	 */
	b 1f; nada; nada; nada; nada; nada; nada; nada
	nada; nada; nada; nada; nada; nada; nada; nada
	nada; nada; nada; nada; nada; nada; nada; nada
	nada; nada; nada; nada; nada; nada; nada; nada

1:	dmtc0	t1,C0_SR			# critical begin
	ssnop; ssnop; ssnop; ssnop		# drain the pipe

	sd	t3,TCC_PREFETCH-TCC_BASE(a1)	# invalidate prefetch buffers
	lw	t1,CPU_MEMACC_OFFSET(a0)	# get MC memory config
	andi	v0,t1,0x3fff			# important bits
	xori	v0,MEMACC_XOR			# 0=slow, !0=normal

	bnez	a5,1f				# skip ECC on IP22
	nada					# BDSLOT
	sd	zero,0(a4)			# ECC_CTRL_ENABLE==0 (Fast)
	lw	zero,0(a0)			# flushbus
1:	sw	a2,CPU_MEMACC_OFFSET(a0)	# go to normal mode on MC
	lw	zero,0(a0)			# flushbus
	sd	a3,TCC_GCACHE-TCC_BASE(a1)	# go to normal mode on TCC
	lw	zero,0(a0)			# flushbus - end of critical

	dmtc0	t0,C0_SR			# restore C0_SR
	ssnop
	.set	reorder

2:	j	ra
	END(ip26_disable_ucmem)

/* Return to previous memory system state.  1 == fast, 0 == slow.
 */
LEAF(ip26_return_ucmem)
	beqz	a0,1f
	b	ip26_disable_ucmem		# going to normal mode
1:	b	ip26_enable_ucmem		# going to slow mode
	END(ip26_return_ucmem)

.data
/* R = proc_freq/sysad_freq	(usually 75.0/50.0)
 *
 * Parity memory timing (DDx):
 *	Read Response Level = floor[(47R-28)/3R]
 *	Release Delay = max{floor[17-27.5R],floor[10-3R]}
 *	Writeback Restart Level = ceiling[15 - 19/R]
 *
 * ECC memory timing (DDxx):
 *	Read Response Level = floor[(31.5R - 15)/2R]
 *	Release Delay = max{floor(17-34.5R),floor(10-3R)}
 *	Writeback Restart Level = ceiling[15 - 19/R]
 */
tcc_fiforatios:
#if _MC_MEMORY_PARITY
	.half	0x0706 0x0606 0x0606 0x0606 0x0607	/* 1.0 - 1.08 */
	.half	0x0607 0x0607 0x0607 0x0607 0x0607	/* 1.1 - 1.18 */
	.half	0x0607 0x0608 0x0608 0x0608 0x0618	/* 1.2 - 1.28 */
	.half	0x0618 0x0618 0x0518 0x0528 0x0528	/* 1.3 - 1.38 */
	.half	0x0528 0x0529 0x0529 0x0529 0x0539	/* 1.4 - 1.48 */
	.half	0x0539 0x0539 0x0539 0x0539 0x0539	/* 1.5 - 1.58 */
	.half	0x0549 0x0549 0x0549 0x054a 0x044a	/* 1.6 - 1.68 */
	.half	0x044a 0x044a 0x045a 0x045a 0x045a	/* 1.7 - 1.78 */
	.half	0x045a 0x045a 0x045a 0x045a 0x045a	/* 1.8 - 1.88 */
	.half	0x045a 0x046a 0x046a 0x046a 0x046a	/* 1.9 - 1.98 */
	.half	0x046a					/* 2.0 */
#else
	.half	0x00ff 0x00ff 0x00ff 0x00ff 0x00ff	/* 1.0 - 1.08 */
	.half	0x00ff 0x00ff 0x00ff 0x00ff 0x0609	/* 1.1 - 1.18 */
	.half	0x0609 0x0609 0x0609 0x0609 0x0619	/* 1.2 - 1.28 */
	.half	0x0619 0x061a 0x051a 0x052a 0x052a	/* 1.3 - 1.38 */
	.half	0x052a 0x052a 0x052a 0x052a 0x053a	/* 1.4 - 1.48 */
	.half	0x053a 0x053a 0x053a 0x053a 0x053b	/* 1.5 - 1.58 */
	.half	0x054b 0x054b 0x054b 0x054b 0x044b	/* 1.6 - 1.68 */
	.half	0x044b 0x044b 0x045b 0x045b 0x045b	/* 1.7 - 1.78 */
	.half	0x045b 0x045b 0x045b 0x045b 0x045b	/* 1.8 - 1.88 */
	.half	0x045b 0x046b 0x046b 0x046b 0x046b	/* 1.9 - 1.98 */
	.half	0x046c					/* 2.0 */
#endif

	.text

#ifdef _IP26_SYNC_WAR
/* Since we cannot handle sync instructions well, we cheat the mongoose
 * __synchronize() built-in, to call ___synchronize() instead.  Since
 * TFP integer side is well ordered, we really do not have to do anything.
 * If we needed to flush the FP load/store side we may be in trouble, but
 * the kernel should only care about integer in the synchronization spots.
 *
 * A function call should ensure the compiler performs load/stores to aliased
 * or global items before the next function call, and breaks up problems with
 * the compiler re-ordering critical load/stores.
 *
 * Another option would have been to have a "nosync" option to the compiler
 * to order stores, but just drop the sync, or de-sync the kernel via post
 * processing (sync -> nada).
 */
	.align	4
LEAF(___synchronize)
	.set	noreorder
	j	ra
	nada				# BDSLOT
	.set	reorder
	END(___synchronize)
#endif
