/*
 * cache.s -- Cache manipulation primitives
 */

#ident "$Revision: 1.4 $"

#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <asm.h>
#include "ip19.h"

BSS(_icache_size, 4)		# bytes of icache
BSS(_dcache_size, 4)		# bytes of dcache
BSS(cachewrback, 4)		# writeback secondary cache?

#define WD_ALIGN_MASK	0x3
#define MIN_CACH_POW2   12

#if 0		/* defined in libsk */
/*
 * return the size of the primary data cache
 */
LEAF(get_dcachesize)
	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	nop
	.set reorder
	and	t0,CONFIG_DC
	srl	t0,CONFIG_DC_SHFT
	addi	t0,MIN_CACH_POW2
	li	v0,1
	sll	v0,t0
	j	ra
	END(get_dcachesize)
#endif

/*
 * return the size of the primary instruction cache
 */
LEAF(get_icachesize)
	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	nop
	.set reorder
	and	t0,CONFIG_IC
	srl	t0,CONFIG_IC_SHFT
	addi	t0,MIN_CACH_POW2
	li	v0,1
	sll	v0,t0
	j	ra
	END(get_icachesize)

/*
 * get_scachesize()
 * return size of second cache
*/
LEAF(get_scachesize)
        li     t0, EV_CACHE_SZ_LOC
        ld      t0, 0(t0)       # Read the CPU board EAROM
        li      v0, 1
        beq     t0, v0, scsz_ret
        move    v0, zero        # A cache size of 1 means no scache
        li      v0, 1
        sll     v0, v0, t0      # Shift 1 left by a0 (cache size = 2^a0)
scsz_ret:
	j	ra
	END(get_scachesize)

/*
 * Return the line size of the primary dcache
 */
LEAF(get_dcacheline)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	nop
	nop
	.set reorder
	and	v0,CONFIG_DB
	beq	v0,zero,1f
	li	v0,32
	j	ra
1:
	li	v0,16
	j	ra
	END(get_dcacheline)

/*
 * Return the line size of the primary icache
 */
LEAF(get_icacheline)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	nop
	nop
	.set reorder
	and	v0,CONFIG_IB
	beq	v0,zero,1f
	li	v0,32
	j	ra
1:
	li	v0,16
	j	ra
	END(get_icacheline)

/*
 * Return the line size of the secondary cache
 */
#define	MIN_BLKSIZE_SHFT	4
LEAF(get_scacheline)
	.set	noreorder
	mfc0	v1,C0_CONFIG
	nop
	nop
	.set reorder
	and	v0,v1,CONFIG_SC		# Test for scache present
	beq	v0,zero,1f		# 0 -> cache present, 1 -> no scache
	move	v0,zero			# no scache
	j	ra
1:
	# second cache is present. figure out the block size
	and	v1,v1,CONFIG_SB		# pull out the bits
	srl	v1,CONFIG_SB_SHFT
	li	v0,1
	addu	v1,MIN_BLKSIZE_SHFT	# turn 0,1,2,3 into 16,32,64,128
	sll	v0,v1
	j	ra
	END(get_scacheline)


/* sd_hwb(vaddr): Secondary Data cache Hit Writeback.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(sd_hwb)
	li	t0, WD_ALIGN_MASK	# cacheops must be wd aligned
	not	t0
	and	a0, a0, t0
	.set	noreorder
	cache	CACH_SD|C_HWB, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	nop
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(sd_hwb)


/* pd_hwbinv(caddr): Primary Data cache Hit Writeback Invalidate.
 * a0: K0-seg virtual address */
LEAF(pd_hwbinv)
	li	t0, WD_ALIGN_MASK	# cacheops must be wd aligned
	not	t0
	and	a0, a0, t0
	.set	noreorder
	cache	CACH_PD|C_HWBINV, 0(a0)
	nop
	nop
	nop
	.set	reorder
	j       ra
	END(pd_hwbinv)

/* read_tag(WhichCache, address, &deststruct)
 * WhichCache == PRIMARYD (1), PRIMARYI (2), SECONDARY (3)
 * destruct is type struct tag_regs.
 */
LEAF(read_tag)
	li	v0, 0		# success by default
	li	t0, K0BASE
	addu	a1, t0		# a1: addr of tag to fetch in cached space
	li	t0, PRIMARYI
	bltu	a0, t0, rprimd	# fetch primary data cache tag?
	beq	a0, t0, rprimi	# fetch primary instruction cache tag?
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a1)	# nope, fetch secondary cache tag
	.set	reorder

getout:	.set	noreorder
	nop
	nop
	mfc0	t0, C0_TAGLO
	# DO NOT READ C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	nop
	nop
	sw	t0, 0(a2)	# taglo is 1st uint in tag struct
	# sw	t1, 4(a2)	# taghi is 2nd
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

	END(read_tag)


/*
 * write_tag(WhichCache, address, &srcstruct)
 * WhichCache == PRIMARYD (1), PRIMARYI (2), SECONDARY (3)
 */

LEAF(write_tag)
	lw	t0, 0(a2)	# a2 pts to taglo: 1st uint in tag struct
	# lw	t1, 4(a2)	# taghi is 2nd

	.set	noreorder
	mtc0	t0,C0_TAGLO
	# DO NOT WRITE C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	li	v0, 0		# success by default
	li	t0, K0BASE
	addu	a1, t0		# a1: addr of tag to set in cached space
	.set	reorder		# this is at least 3 clocks

	li	t0, PRIMARYI
	bltu	a0, t0, wprimd	# set primary data cache tag?
	beq	a0, t0, wprimi	# set primary instruction cache tag?
	.set	noreorder
	nop
	nop
	cache	CACH_SD|C_IST, 0(a1)	# no, set secondary cache tag
	nop
	nop
	.set	reorder

setout: j	ra

wprimi:	.set	noreorder
	cache	CACH_PI|C_IST, 0(a1)	# set primary instruction cache tag
	.set	reorder
	b	setout

wprimd:	.set	noreorder
	cache	CACH_PD|C_IST, 0(a1)	# set primary data cache tag
	.set	reorder
	b	setout

	END(write_tag)

/*
 * fill_ipline(addr)
 * Fill Primary Instruction Cache line via k0seg
 * addr should always be in k0seg 
 */
LEAF(fill_ipline)
	.set	noreorder
	cache	CACH_PI|C_FILL, 0(a0)	# fill the line - easy, wasn't it?
	NOP_0_1
	.set	reorder
	j	ra
	END(fill_ipline)

#ifdef HARD_PD
/* ml_hammer_pdcache(iterations, uintptr):
 * a0: number of iterations to execute.
 * a1: ptr to 5 ints: 0==badaddr, 1==expectedval, 2==badval, endaddr 
 * 
 */
LEAF(ml_hammer_pdcache)
	move	s0, ra		# mustn't use s0!

	lui	t0, 0xa000
	slt	t1, t0, s0
	beq	t1,zero, pdc
	.set	noreorder
	nop
	.set	reorder
	jal	runcached		# must execute in cache for speed

pdc:	# write a pd-cache of values
	li	ta3, 0x80400000		# start at 4mb cached
	li	t0, 0x00400000		# addr for patterns 
	li	t1, 0x2000		# one dcache size (8kb)
	# pattern algorithm: ((5 * x) + addr + 1); x begins at 1
	li	t3, 1			# x
pdcwlp:
	move	ta0, t3			# x * 1
	sll	t3, 2			# x = x * 4
	addu	t3, ta0			# + x == *5
	addu	t3, t0			# current address
	addu	t3, 1			# +1
	move	ta1, t3

	and	t2, a0, 0x1		# inverse?
	bgt	t2, zero, wnorm
	not	ta1
wnorm:
	sw	ta1, 0(ta3)
	addu	t0, 4
	addu	ta3, 4
	subu	t1, 4
	bgt	t1,zero,pdcwlp


	# verify them all
	li	ta3, 0x80400000		# start at 4mb cached
	li	t0, 0x00400000
	li	t1, 0x2000
	li	t3, 1			# x
pdcrlp:
	move	ta0, t3			# x * 1
	sll	t3, 2			# x = x * 4
	addu	t3, ta0			# + x == *5
	addu	t3, t0			# current address
	addu	t3, 1			# +1
	move	ta1, t3

	and	t2, a0, 0x1		# inverse?
	bgt	t2, zero, rnorm
	not	ta1
rnorm:
	lw	t2, 0(ta3)
	bne	t2, ta1, pdcerr

	addu	t0, 4
	addu	ta3, 4
	subu	t1, 4
	bgt	t1,zero,pdcrlp

	addu	ta2, 1
	subu	a0, 1
	bgt	a0, zero, pdc

	move	v1, zero
	b	pdcout

pdcerr:
	li	v1, 1
	sw	ta3, 0(a1)		# bad addr
	sw	ta1, 4(a1)		# expected val
	sw	t2, 8(a1)		# actual value
pdcout:
	sw	ta3, 12(a1)		# ending address
	sw	ta2, 16(a1)		# total # of iterations
 	jal	uncached
	.set	noreorder
	nop
	nop
	.set	reorder

	move	v0, v1		# runcached/uncached uses v0
	move	ra, s0
	j       ra
	.set	noreorder
	nop
	.set	reorder
	END(ml_hammer_pdcache)

LEAF(ml_h_pdc_end)
	END(ml_h_pdc_end)
#endif 



/* ml_hammer_scache(iterations, uintptr): 
 * a0: number of iterations to execute.
 * a1: ptr to 3 ints: 0==badaddr, 1==expectedval, 2==badval
 * 
 */
LEAF(ml_hammer_scache)
	move	s0, ra		# mustn't use s0!

	lui	t0, 0xa000
	slt	t1, t0, s0
	beq	t1,zero, sdc
	.set	noreorder
	nop
	.set	reorder
	jal	runcached		# must execute in cache for speed

sdc:	# write a pd-cache of values xxxxxstart here
	li	ta3, 0x80400000		# start at 4mb cached
	li	t0, 0x00400000		# addr for pattern
	li	t1, 0x100000		# one scache size (1mb)
	# pattern algorithm: ((5 * x) + addr + 1); x begins at 1
	li	t3, 1			# x
sdcwlp:
	move	ta0, t3			# x * 1
	sll	t3, 2			# x = x * 4
	addu	t3, ta0			# + x == *5
	addu	t3, t0			# current address
	addu	t3, 1			# +1
	move	ta1, t3

	and	t2, a0, 0x1		# inverse?
	bgt	t2, zero, swnorm
	not	ta1
swnorm:
	sw	ta1, 0(ta3)
	addu	t0, 4
	addu	ta3, 4
	subu	t1, 4
	bgt	t1,zero,sdcwlp


	# verify them all
	li	ta3, 0x80400000
	li	t0, 0x00400000
	li	t1, 0x100000
	li	t3, 1			# x
sdcrlp:
	move	ta0, t3			# x * 1
	sll	t3, 2			# x = x * 4
	addu	t3, ta0			# + x == *5
	addu	t3, t0			# current address
	addu	t3, 1			# +1
	move	ta1, t3

	and	t2, a0, 0x1		# inverse?
	bgt	t2, zero, srnorm
	not	ta1
srnorm:
	lw	t2, 0(ta3)
	bne	t2, ta1, sdcerr

	addu	t0, 4
	addu	ta3, 4
	subu	t1, 4
	bgt	t1,zero,sdcrlp

	addu	ta2, 1
	subu	a0, 1
	bgt	a0, zero, sdc

	move	v1, zero
	b	sdcout

sdcerr:
	li	v1, 1
	sw	ta3, 0(a1)		# bad addr
	sw	ta1, 4(a1)		# expected val
	sw	t2, 8(a1)		# actual value
sdcout:
	sw	ta3, 12(a1)		# ending address
	sw	ta2, 16(a1)		# total # of iterations
 	jal	uncached
	.set	noreorder
	nop
	nop
	.set	reorder

	move	v0, v1		# runcached/uncached uses v0
	move	ra, s0
	j       ra
	.set	noreorder
	nop
	.set	reorder
	END(ml_hammer_scache)

LEAF(ml_h_sc_end)
	END(ml_h_sc_end)



/* a0: caddr, a1: tag_lo */
LEAF(_sd_ist)
	.set	noreorder
	mtc0	a1, C0_TAGLO
	nop
	nop
	nop
	cache	CACH_SD|C_IST, 0(a0)
	nop
	nop
	nop
	nop
	.set	reorder
	j	ra	
	END(_sd_ist)

/* a0: caddr, a1: tag_lo */
LEAF(_pd_ist)
	.set	noreorder
	mtc0	a1, C0_TAGLO
	nop
	nop
	nop
	cache	CACH_PD|C_IST, 0(a0)
	nop
	nop
	nop
	nop
	.set	reorder
	j	ra	
	END(_pd_ist)


/* ml_hammer_pdcache(iterations, uintptr):
 * a0: number of iterations to execute.
 * a1: ptr to 5 ints: 0==badaddr, 1==expectedval, 2==badval, endaddr 
 * 
 */
LEAF(ml_hammer_pdcache)
	move	s0, ra		# mustn't use s0!

	lui	t0, 0xa000
	slt	t1, t0, s0
	beq	t1,zero, pdc
	.set	noreorder
	nop
	.set	reorder
	jal	runcached		# must execute in cache for speed

	move	ta2, zero

pdrpt:	li	t2, 7
pdc:	# write a pd-cache of values
	li	t0, 0x80400000		# start at 4mb cached
	li	t1, 0x2000		# one dcache size (8kb)
	li	t3, 3
	addu	t0, t2
pdcwlp:
	sb	t3, 0(t0)
	addu	t0, 8
	subu	t1, 8
	bgt	t1,zero,pdcwlp

	# verify them all
	li	t0, 0x80400000		# start at 4mb cached
	li	t1, 0x2000
	addu	t0, t2
pdcrlp:
	lb	ta0, 0(t0)
	bne	ta0, t3, pdcerr

	addu	t0, 8
	subu	t1, 8
	bgt	t1,zero,pdcrlp		# all words in cache

	addu	ta2, 1
	subu	t2, 1
	bge	t2, zero, pdc		# test all 8 bytes in each dbl-word

	subu	a0, 1
	bgt	a0, zero, pdrpt		# for specified number of iterations

	move	v1, zero
	b	pdcout

pdcerr:
	li	v1, 1
	sw	t0, 0(a1)		# bad addr
	sw	t3, 4(a1)		# expected val
	sw	ta0, 8(a1)		# actual value
pdcout:
	sw	t0, 12(a1)		# ending address
	sw	ta2, 16(a1)		# total # of iterations
 	jal	uncached
	.set	noreorder
	nop
	nop
	.set	reorder

	move	v0, v1		# runcached/uncached uses v0
	move	ra, s0
	j       ra
	.set	noreorder
	nop
	.set	reorder
	END(ml_hammer_pdcache)

LEAF(ml_h_pdc_end)
	END(ml_h_pdc_end)

/* pi_fill(vaddr): Primary Instruction fill command.
 * a0: virtual address
 */
LEAF(pi_fill)
	.set	noreorder
	cache	CACH_PI|C_FILL, 0(a0)
	nop
	nop
	j       ra
	nop
	.set	reorder
	END(pi_fill)


/* pd_HWB(vaddr): Primary Data cache Hit Writeback.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(pd_HWB)
	.set	noreorder
	cache	CACH_PD|C_HWB, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(pd_HWB)


/* sd_HWB(vaddr): Secondary Data cache Hit Writeback.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(sd_HWB)
	.set	noreorder
	cache	CACH_SD|C_HWB, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(sd_HWB)
