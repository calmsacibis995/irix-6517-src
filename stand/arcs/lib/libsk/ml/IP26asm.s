#ident	"lib/libsk/ml/IP26asm.s:  $Revision: 1.32 $"

/*
 * IP26asm.s - IP26 specific assembly language functions
 */

#include "ml.h"
#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

/*
 * wbflush() -- spin until write buffer empty
 */
LEAF(wbflush)
XLEAF(flushbus)
	.set	noat
	dli	AT,PHYS_TO_K1(CPUCTRL0)
	lw	AT,0(AT)
	.set	at
	j	ra
	END(wbflush)

/* long splerr(void)
 *	Set SR to ignore interrupts and errors.
 */
LEAF(splerr)
	.set	noreorder
	DMFC0	(a0,C0_SR)
	dli	v0,~(SR_IMASK)			# all interrupts off
	and	a0,a0,v0
	ori	a0,SR_IE|SR_IBIT7		# enable exceptions
	# FALLSTHROUGH to spl to set SR
/*
 * old_sr = spl(new_sr) -- set the interrupt level (really the sr)
 *	returns previous sr
 */
XLEAF(spl)
	.set	noreorder
	DMFC0	(v0,C0_SR)
	DMTC0	(a0,C0_SR)
	j	ra
	nop
	.set	reorder
	END(splerr)


/*
 *
 * writemcreg (reg, val)
 *
 * Basically this does *(volatile uint *)(PHYS_TO_K1(reg)) = val;
 *      a0 - physical register address
 *      a1 - value to write
 *
 *  This was a workaround for a bug in the first rev MC chip, which is not
 * in fullhouse, but we make still need a routine by this name.
 */

LEAF(writemcreg)
        or      a0,K1BASE       # a0 = PHYS_TO_K1(a0)
        sw      a1,(a0)         # write val in a1 to MC register *a0
        j       ra
END(writemcreg)

/*  Invalidate, write back, or write back and invalidate the GCache (and
 * the IU's D cache) using the built-in TCC cache ops.  Address must be
 * in physical, K0, or K1.
 *
 * void __dcache_inval(void *address, int length)
 * void __dcache_wb(void *address, int length)
 * void __dcache_wb_inval(void *address, int length)
 *
 */
LEAF(__dcache_inval)
	dli	t2,K1BASE|TCC_CACHE_OP|TCC_INVALIDATE
	j	1f

XLEAF(__dcache_wb)
	dli	t2,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB
	j	1f

XLEAF(__dcache_wb_inval)
	dli	t2,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE
	/*FALLSTHROUGH*/

1:
	andi	t0,a0,TCC_LINESIZE-1		# get rounding amount
	add	a1,a1,t0			# add rounding amount to length
	li	t0,TCC_PHYSADDR			# significant address bits
	and	a0,a0,t0			# mask off un-need bits
	or	a0,a0,t2			# calc cache-op addr

	.align	4				# loop is one quad
1:
	.set	noreorder
	lw	zero,0(a0)			# cache op on line 'a0'
	addi	a1,a1,-TCC_LINESIZE		# decr length
	bgtz	a1,1b				# on to next cache line
	daddiu	a0,a0,TCC_LINESIZE		# BDSLOT incr cache op addr
	.set	reorder

	j	ra
	END(__dcache_inval)

/*  Cache ops that operate on the entire GCache, and DCache.  They are index
 * based to get all sets in the GCache and use the TCC built-in cache flush
 * operations.  Address must be in physical, K0, or K1.
 *
 * void __dcache_inval_all(void);
 * void __dcache_wb_all(void);
 * void __dcache_wb_inval_all(void);
 */
LEAF(__dcache_inval_all)
	dli	t3,K1BASE|TCC_CACHE_OP|TCC_INVALIDATE|TCC_INDEX_OP
	j	1f
XLEAF(__dcache_wb_all)
	dli	t3,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INDEX_OP
	j	1f
XLEAF(__dcache_wb_inval_all)
	dli	t3,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE|TCC_INDEX_OP
	/*FALLSTHROUGH*/

1:
	#  Get GCache size and calculate the number of indexes shifted into
	# the index field of the tcc cache flushing index space:
	#	(MB >> (TCC_CACHE_INDEX_SHIFT-TCC_CACHE_SET_SHIFT)
	dla	t0,_sidcache_size		# GCache size
	lw	t0,0(t0)
	srl	t0,TCC_CACHE_INDEX_SHIFT-TCC_CACHE_SET_SHIFT	# max index + 1
	addi	t0,-(1<<TCC_CACHE_INDEX_SHIFT)			# max index
	
	.align	4				# loop is two quads
1:
	or	ta0,t3,t0			# reset of cache op addr

	lw	zero,0(ta0)				# set 0
	lw	zero,1<<TCC_CACHE_SET_SHIFT(ta0)	# set 1
	lw	zero,2<<TCC_CACHE_SET_SHIFT(ta0)	# set 2
	lw	zero,3<<TCC_CACHE_SET_SHIFT(ta0)	# set 3

	addi	t0,-(1<<TCC_CACHE_INDEX_SHIFT)	# next index

	bgez	t0,1b				# loop in more indexes

	j	ra
	END(__dcache_inval_all)

LEAF(FlushAllCaches)
	j	flush_cache
	END(FlushAllCaches)


/* Count the number of TCC Fifo Cycles vs the X number of SysAd
 * Cycles - the larger the number of sysad cycles, the less
 * percentage error hit the caller gets */
LEAF(CountTccVsSysAdCycles)
	.set    noreorder
        LI      t0,PHYS_TO_K1(TCC_COUNT)        # TCC counter == SysAD freq
        sd      zero,0(t0)
        move    t2,zero                         # initialize counter
        dmtc0   zero,C0_COUNT                   # zero TFP counter
	ssnop					# C0 hazard
        ssnop                                   # C0 hazzard
1:      dmfc0   v0,C0_COUNT                     # get current count before ld
	ssnop					# C0 hazzard
        ld      t2,0(t0)                        # get sysAD count
        blt     t2,a0,1b                        # enough cycles?
        nada
        .set    reorder
	j	ra
	END(CountTccVsSysAdCycles)

#define MEMACC_XOR		(CPU_MEMACC_SLOW&0x3fff)
#define CPU_MEMACC_OFFSET	CPU_MEMACC-CPUCTRL0
#define CPU_CTRLD_OFFSET	(CTRLD-CPUCTRL0)

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
 * STANDALONE: return TCC_GCACHE value in v1
 *
 * GCache indicies used:
 *	TCC, MC, PAL PIO	0
 *	critical cache line	odd
 *	write flush cache line	2
 */
LEAF(ip26_enable_ucmem)
XLEAF(ip28_enable_ucmem)
	LI	a0,K1BASE			# K1
	LI	a6,K0BASE			# K0
	lw	zero,0(a0)			# Force mem read to flush errs.
	li	a2,CPU_MEMACC_SLOW		# slow memory setting
	or	a1,a0,TCC_BASE			# PHYS_TO_K1(TCC_BASE)
	or	a5,a0,HPC3_SYS_ID		# PHYS_TO_K1(HPC3_SYS_ID)
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a7,a0,TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)
	ld	v1,TCC_GCACHE-TCC_BASE(a1)	# save TCC_GCACHE for return
	and	a3,v1,~GCACHE_REL_DELAY		# figure slow mode
	ori	a3,GCACHE_RR_FULL|GCACHE_WB_RESTART
	li	t3,PRE_INV			# disable prefetch + invalidate

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

	j	ra
	END(ip26_enable_ucmem)

/*  Return to previous memory system state.  1 == fast, 0 == slow.
 *	- a0 -- previous mode
 *	- a1 -- previous TCC_GCACHE
 *
 *  Assumes standalone runs in slow mode, so doesn't actually switch
 * if comming from slow mode.
 *
 * Critical section on one cache line to prevent writebacks during
 * the mode switch.
 *
 * NOTE: interface is different from the kernel as it knows what the
 *	 fast mode setting is.  Also symmon must react to what the
 *	 kernel thinks it is, which is a bit different, so we need
 *	 to explicitly restore it.
 */
LEAF(ip26_return_gcache_ucmem)
XLEAF(ip28_return_ucmem)
	beqz	a0,2f				# return to slow -- no change

	move	a3,a1				# fast TCC_GCACHE value
	LI	a0,K1BASE			# K1
	li	a2,CPU_MEMACC_NORMAL		# normal memory setting
	or	a1,a0,TCC_BASE			# PHYS_TO_K1(TCC_BASE)
	or	a5,a0,HPC3_SYS_ID		# PHYS_TO_K1(HPC3_SYSID)
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)
	li	t3,PRE_DEFAULT|PRE_INV		# enable prefetch and inval

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
	END(ip26_return_gcache_ucmem)
