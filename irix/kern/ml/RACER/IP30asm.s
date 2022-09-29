/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.56 $"

/*
 * IP30 specific assembly routines
 */

#include "ml/ml.h"
#include <sys/RACER/gda.h>
#include <sys/RACER/racermp.h>
#include <sys/dump.h>

/*	dummy routines whose return value is unimportant (or no return value).
	Some return reasonable values on other machines, but should never
	be called, or the return value should never be used on other machines.
*/
LEAF(dummy_func)
XLEAF(check_delay_iflush)
XLEAF(dma_mapinit)
XLEAF(apsfail)
XLEAF(disallowboot)
XLEAF(vme_init)
XLEAF(vme_ivec_init)
XLEAF(allowintrs)
XLEAF(bump_leds)
XLEAF(wd93_init)
XLEAF(stopkgclock)
XLEAF(ackkgclock)
#ifndef HEART_COHERENCY_WAR
XLEAF(heart_dcache_wb_inval)
XLEAF(heart_dcache_inval)
XLEAF(heart_write_dcache_wb_inval)
XLEAF(heart_write_dcache_inval)
#endif /* !HEART_COHERENCY_WAR */
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
#ifndef HEART_COHERENCY_WAR
XLEAF(heart_need_flush)
#endif /* !HEART_COHERENCY_WAR */
	move	v0,zero
	j	ra
	END(dummyret0_func)

LEAF(_get_timestamp)
	_GET_TIMESTAMP(v0)
	j	ra
	END(_get_timestamp)

LEAF(getcpuid)	
	ld	v0,PHYS_TO_COMPATK1(HEART_PRID)
	j	ra
	END(getcpuid)

#define NMI_ERREPC	0
#define NMI_EPC		8
#define NMI_SP		16
#define NMI_RA		24
#define NMI_SAVE_REGS	4
#define NMI_SAVESZ_SHFT	5

LEAF(nmi_dump)
        .set noreorder
	.set noat
	LI	k0,SR_KADDR|SR_DEFAULT	# make sure page size bits
	MTC0(k0,C0_SR)			# are on

	LI	k0,PHYS_TO_COMPATK1(HEART_PRID)	# get processor ID
	ld	k1,0(k0)
	LI	k0,GDA_ADDR		# Load address of global data area
	lw	k0,G_MASTEROFF(k0)	# Get the master processor's SPNUM
	bne	k0,k1,1f		# Skip forward if we're not master
	nop				# BDSLOT

	# Save Registers for Master cpu at offset 0 in nmi_saveregs.
	LA	k0,nmi_saveregs
	DMFC0(k1,C0_ERROR_EPC)
	sd	k1,NMI_ERREPC(k0)
	DMFC0(k1,C0_EPC)
	sd	k1,NMI_EPC(k0)
	sd	sp,NMI_SP(k0)
	sd	ra,NMI_RA(k0)

	LA	sp,dumpstack
	PTR_ADD sp,DUMP_STACK_SIZE - 16	# Set our stack pointer.
	LA	gp,_gp
	j       cont_nmi_dump
	nop				# BDSLOT
	/*NOTREACHED*/

1:
	# k1 now has an id number for slave cpu. Master cpu is set to 0.
	PTR_SLL k1,NMI_SAVESZ_SHFT	# Get right offset into k1
	LA	k0,nmi_saveregs		# Starting address of save array
	PTR_ADDU k0,k1			# k0 now has proper starting address

	DMFC0(k1,C0_ERROR_EPC)
	sd	k1,NMI_ERREPC(k0)
	DMFC0(k1,C0_EPC)
	sd	k1,NMI_EPC(k0)
	sd	sp,NMI_SP(k0)
	sd	ra,NMI_RA(k0)

2:	b       2b			# Loop forever.
	nop				# BDSLOT
	.set	at
	.set	reorder
	END(nmi_dump)

        .data
EXPORT(nmi_saveregs)
        .dword  0: (MAXCPU * NMI_SAVE_REGS)

	.text

/* Return size of secondary cache (really max cache size for start-up)
 *	getcachesz(int id) -- for MP all processors have same $ size
 */
LEAF(getcachesz)
        .set	noreorder
	mfc0	v1,C0_CONFIG
	and	v1,CONFIG_SS
	dsrl	v1,CONFIG_SS_SHFT
	dadd	v1,CONFIG_SCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
	END(getcachesz)

#if SABLE
/* speedup_counter
 * Intended to be called from idle() on SABLE in order to cause the next
 * clock tick more quickly.
 */
LEAF(speedup_counter)
	.set	noreorder
	MFC0(t0,C0_COMPARE)
	addiu	t0,-16			# interrupt in ~20 cycles
	MTC0(t0,C0_COUNT)
	j	ra
	nop
	.set    reorder
	END(speedup_counter)
#endif /* SABLE */

#ifdef	MP
/* all slave processors come here - assumed on boot stack */
NESTED(bootstrap, 0, zero)
	.set	noreorder

	/* do some one-time only initialization */
	li	t0,SR_KADDR		# run kernel in 64-bit mode
	MTC0(t0,C0_SR)

	LA	gp,_gp			# for C code

	/* a0 - pointer to mpconf entry */
	lw	v0,MP_SCACHESZ(a0)	# pick up log2(secondary cache size)
	li	s0,1
	dsllv	s0,v0			# secondary cache size

	MTC0(zero,C0_WATCHLO)		# no watch point exceptions
	MTC0(zero,C0_WATCHHI)
	li	a3,TLBPGMASK_MASK
	MTC0(a3,C0_PGMASK)		# set up pgmask register
	li	a3,NWIREDENTRIES
	MTC0(a3,C0_TLBWIRED)		# number of wired tlb entries
	# pairs of 4-byte ptes
	LI	v0,KPTEBASE<<(KPTE_TLBPRESHIFT+PGSHFTFCTR)
	DMTC0(v0,C0_EXTCTXT)
	LI	v0,FRAMEMASK_MASK
	DMTC0(v0,C0_FMMASK)
	DMTC0(zero,C0_TLBHI)

	.set	reorder

	/* set up pda as wired entry wirepda(pdaindr[id].pda) */
	jal	getcpuid		# grab cpuid from hardware 
	li	v1,PDAINDRSZ
	mul	v0,v1
	LA	v1,pdaindr
	PTR_ADDU	v0,v1
	PTR_L	a0,PDAOFF(v0)		# fetch addr of pda
					# (set up in initmasterpda)
	# shift and mask pfn
	dsrl	a0,TLBLO_PFNSHIFT	# align for proper pde
	and	a0,TLBLO_PFNMASK	# mask PFN suitably

	or	a4,a0,PG_M|PG_G|PG_VR|PG_COHRNT_EXLWR
	li	a3,PG_G
	move	s1,a4			# save for later

#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
	/*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
	 */
	dsll32	a3,0 			# 4th arg - 1st pte
	dsll32	a4,0			# 5th arg - 2nd pte
#endif

	li	a0,PDATLBINDEX
	move	a1,zero			# ASID
	LI	a2,PDAPAGE
	li	a5,TLBPGMASK_MASK
	jal	tlbwired

	# In PTE_64BIT Mode, pte sizes are 64 bits!!. 
	# So, use PTE_S instead of INT_S, which scales properly.
	# PTE_S is defined in ml/ml.h
	PTE_S	s1,VPDA_PDALO(zero)	# for later

	li	v0,PDA_CURIDLSTK
	sb	v0,VPDA_KSTACK(zero)	# running on boot/idle stack
	PTR_L	sp,VPDA_LBOOTSTACK(zero)

	# store secondary cache size in pda
	sw	s0,VPDA_PSCACHESIZE(zero)
	j	cboot
	END(bootstrap)
#endif	/* MP */

/*
 * find the first set bit from the left in a 64-bit value without branching
 * and/or memory look up
 */
	.align	7
LEAF(ffintr64)
	.set	noreorder

	move	v0,zero		# result will be stored here

	li	v1,32		# shift right this amount to get the left half
	dsrl32	a1,a0,0		# get the leftmost 32 bits
	movn	v0,v1,a1	# at least one bit is set
	movn	a0,a1,a1	# use the leftmost 32 bits if != 0

	li	v1,16		# shift right this amount to get the left half
	dsrl	a1,a0,16	# get the leftmost 16 bits
	movn	a1,v1,a1	# at least one bit is set
	dadd	v0,v0,a1	# update intermediate result
	dsrlv	a0,a0,a1	# choose which half to use

	li	v1,8		# shift right this amount to get the left half
	dsrl	a1,a0,8		# get the leftmost 8 bits
	movn	a1,v1,a1	# at least one bit is set
	dadd	v0,v0,a1	# update intermediate result
	dsrlv	a0,a0,a1	# choose which half to use

	li	v1,4		# shift right this amount to get the left half
	dsrl	a1,a0,4		# get the leftmost 4 bits
	movn	a1,v1,a1	# at least one bit is set
	dadd	v0,v0,a1	# update intermediate result
	dsrlv	a0,a0,a1	# choose which half to use

	li	v1,2		# shift right this amount to get the left half
	dsrl	a1,a0,2		# get the leftmost 2 bits
	movn	a1,v1,a1	# at least one bit is set
	dadd	v0,v0,a1	# update intermediate result
	dsrlv	a0,a0,a1	# choose which half to use

	dsrl	a0,a0,1		# get the leftmost bit
	j	ra
	dadd	v0,v0,a0	# update to get final result

	.set	reorder
	END(ffintr64)

#define LOOP_COUNT ((1024/2)-1)
#define SUB_LOOP        10
#define LOOP_FACTOR              \
	li	t1,SUB_LOOP     ;\
11:	bgt	t1,zero,11b     ;\
	subu	t1,1

LEAF(_ticksper1024inst)
	sync
	li	a0,LOOP_COUNT
	LI	t0,PHYS_TO_COMPATK1(HEART_COUNT)
.align	6
	ld	v0,0(t0)
	.set	noreorder
3:
	LOOP_FACTOR
	bgt	a0,zero,3b
	subu	a0,1

	ld	v1,0(t0)
	.set	reorder
	dsubu	v0,v1,v0

	j	ra
	END(_ticksper1024inst)

/* void us_delay(uint usecs)
 *	- delay loop with a loop factoring, and also helps sometimes with
 *	  messy compilers.
 *
 * NOTE: set-up + loop must be on one i$ line (16 instructions)
 */
	.align	6
LEAF(us_delay)
	.set noreorder
	lw	a1,VPDA_DECINSPERLOOP(zero)	# [1]
	dsll	a0,32				# [2] zero out upper 32 bits
	dsrl	a0,32				# [3]
	/* dmul	a0,1000 macro as output optimized -- usec to nsec */
	dsll	t0,a0,5				# [4]
	dsubu	t0,t0,a0			# [5]
	dsll	t0,t0,2				# [6]
	daddu	t0,t0,a0			# [7]
	dsll	a0,t0,3				# [8]

#ifdef US_DELAY_DEBUG
	mfc0	t0,C0_COUNT			# [9]
#endif
1:
	LOOP_FACTOR				# [10/11/12]
	bgt	a0,zero,1b			# [13] us_delay loop
	LONG_SUBU a0,a1				# [14] BDSLOT

#ifdef US_DELAY_DEBUG
	mfc0	t1,C0_COUNT			# [15]
	sw	t0,us_before			# now any alignment is ok
	sw	t1,us_after
#endif
	.set reorder
	j	ra
	END(us_delay)

/* Routines to call heart_intr() with the appropriate interrupt mask.
 * Done in assembly to avoid some stack/calling overhead.
 */
LEAF(heart_intr_low)
	dli	a1,HEART_INT_LEVEL0
	j	heart_intr
XLEAF(heart_intr_med)
	dli	a1,HEART_INT_LEVEL1
	j	heart_intr
XLEAF(heart_intr_hi)
	dli	a1,HEART_INT_LEVEL2
	j	heart_intr
XLEAF(heart_intr_err)
	dli	a1,HEART_INT_LEVEL4
	j	heart_intr
	END(heart_intr_low)

#if ECC_DEBUG || (HWG_PERF_CHECK && !DEBUG)
LEAF(chill_caches)
	.set	noreorder
	li	a0,0x80000000
	li	a1,0x100000
1:
	cache	CACH_SD|C_IWBINV, 1(a0)
	cache	CACH_SD|C_IWBINV, 0(a0)
	add	a0,a0,32
	bgt	a1,zero,1b
	sub	a1,a1,32

	cache	CACH_BARRIER, 0(a0)
	.set	reorder

	j	ra
	
	END(chill_caches)
#endif

#if ECC_DEBUG
LEAF(heart_ecc_test_read)
	.set	noreorder
	move	v0,a1
	cache	CACH_BARRIER, 0(a0)
	ld	v0,0(a0)
	cache	CACH_BARRIER, 0(a0)
	.set	reorder
	j	ra
	END(heart_ecc_test_read)
#endif

#if HEART_COHERENCY_WAR || HEART_INVALIDATE_WAR

/*  Specialized IP30 pagecopy routine that is unrolled a bit to avoid
 * d$ speculation workaround overhead execept on the last bit.
 * 
 *  _pagecopy(void *src, void *dst, int len)
 *
 *  Assumes src and dst are both cache line aligned and len is a multiple
 * of (n*2*CACHE_SLINE_SIZE)+2*CACHE_SLINE_SIZE, ie an even number of cache
 * lines greater than or equal to 4.
 *
 *  The code does not copy the last two lines to avoid the d$ speculation
 * on stores problem.  The T5 has a 16 deep Address queue which has to fill
 * 4 times for us to do a speculative store past the end of our buffer so
 * we are safe.  The trailer loop has an explicit cache barrier when the
 * HEART_COHERENCY*_WAR is enabled.
 *
 *  This is derived from the pacecar function of the same name.  We do not
 * use prefetch as I think we bog down the address queue enough so it
 * doesn't really become effective.  On some strides it helps.
 *
 *  Copy registers: a4, a5, a6, a7
 */
LEAF(_pagecopy)
	.set	noreorder

	beqz	a2,2f			# skip zero length copies
	sltu	t1,a0,a1		# BDSLOT: if src < dst
	bnez	t1,_pagecopy_backwards	# then do backwards copy
	li	t0,2*CACHE_SLINE_SIZE	# BDSLOT: size of trailer
	addi	a2,-(2*CACHE_SLINE_SIZE)# do last 2 lines seperately

1:	ld	a4,  0(a0)	;	ld	a5, 32(a0)	# line 1 + 2
	addi    a2,-CACHE_SLINE_SIZE				# add
	ld	a6, 80(a0)	;	ld	a7,112(a0)	# line 3 + 4
	sd	a4,  0(a1)	;	sd	a5, 32(a1)	# bank 1
	sd	a6, 80(a1)	;	sd	a7,112(a1)	# bank 2
	ld	a4,  8(a0)	;	ld	a5, 40(a0)	# bank 1
	ld	a6, 16(a0)	;	ld	a7, 24(a0)	# bank 2
	sd	a4,  8(a1)	;	sd	a5, 40(a1)
	sd	a6, 16(a1)	;	sd	a7, 24(a1)
	ld	a4, 64(a0)	;	ld	a5, 72(a0)	# bank 1
	ld	a6, 88(a0)	;	ld	a7,120(a0)	# bank 2
	sd	a4, 64(a1)	;	sd	a5, 72(a1)
	sd	a6, 88(a1)	;	sd	a7,120(a1)
	ld	a4, 96(a0)	;	ld	a5,104(a0)	# bank 1
	ld	a6, 48(a0)	;	ld	a7, 56(a0)	# bank 2
	sd	a4, 96(a1)	;	sd	a5,104(a1)
	sd	a6, 48(a1)	;	sd	a7, 56(a1)

	daddiu	a0,CACHE_SLINE_SIZE

	bgtz	a2,1b			# keep going?
	daddiu	a1,CACHE_SLINE_SIZE	# BDSLOT: next dst cache line

1:	addi	t0,-32			# done with one chunk
	ld	a4, 0(a0)	;	ld	a5, 8(a0)
	ld	a6,16(a0)	;	ld	a7,24(a0)
	sd	a4, 0(a1)	;	sd	a5, 8(a1)
	sd	a6,16(a1)	;	sd	a7,24(a1)

	daddiu	a0,32			# next src chunk
	daddiu	a1,32			# next dst chunk
	cache	CACH_BARRIER,-32(a0)	# quench store speculation
	bgtz	t0,1b			# keep going?
	nop				# BDSLOT

2:	j	ra
	nop				# BDSLOT

_pagecopy_backwards:
	daddu	a0,a2			# start with ending addresses
	daddu	a1,a2
	li	t0,2*CACHE_SLINE_SIZE	# size of trailer
	addi	a2,-(2*CACHE_SLINE_SIZE)# do last 2 lines seperately

1:	ld	a4,  -8(a0)	;	ld	a5, -40(a0)	# line 1 + 2
	addi    a2,-CACHE_SLINE_SIZE;
	ld	a6, -88(a0)	;	ld	a7,-120(a0)	# line 3 + 4
	sd	a4,  -8(a1)	;	sd	a5, -40(a1)	# bank 2
	sd	a6, -88(a1)	;	sd	a7,-120(a1)	# bank 1
	ld	a4, -16(a0)	;	ld	a5, -48(a0)
	ld	a6, -24(a0)	;	ld	a7, -32(a0)
	sd	a4, -16(a1)	;	sd	a5, -48(a1)	# bank 2
	sd	a6, -24(a1)	;	sd	a7, -32(a1)	# bank 1
	ld	a4, -72(a0)	;	ld	a5, -80(a0)
	ld	a6, -96(a0)	;	ld	a7,-128(a0)
	sd	a4, -72(a1)	;	sd	a5, -80(a1)	# bank 2
	sd	a6, -96(a1)	;	sd	a7,-128(a1)	# bank 1
	ld	a4,-104(a0)	;	ld	a5,-112(a0)
	ld	a6, -56(a0)	;	ld	a7, -64(a0)
	sd	a4,-104(a1)	;	sd	a5,-112(a1)	# bank 2
	sd	a6, -56(a1)	;	sd	a7, -64(a1)	# bank 1

	daddiu	a0,-CACHE_SLINE_SIZE

	bgtz	a2,1b			# keep going?
	daddiu	a1,-CACHE_SLINE_SIZE	# BDSLOT: next dst cache line

1:	ld	a4, -8(a0)	;	ld	a5,-16(a0)
	ld	a6,-24(a0)	;	ld	a7,-32(a0)
	addi	t0,-32			# done with one chunk
	sd	a4, -8(a1)	;	sd	a5,-16(a1)
	sd	a6,-24(a1)	;	sd	a7,-32(a1)

	daddiu	a0,-32			# next src chunk
	daddiu	a1,-32			# next dst chunk
	cache	CACH_BARRIER,32-8(a0)	# quench speculation
	bgtz	t0,1b			# keep going?
	nop				# BDSLOT

	j	ra
	nop				# BDSLOT
	.set	reorder
	END(_pagecopy)

/*  Specialized IP30 pagezero routine that is unrolled a bit, and avoids
 * speculation problems vs the pre rev D heart bugs (and 2.6 T5 bugs).
 *
 *	_pagezero(void *dst, int len)
 *
 *  The dst address must be page cache aligned, and the length must be
 * of more than 4 cache lines long (1 then 3 trailers).
 *
 *  This is derived from the pacecar function of the same name, but does
 * not use prefetch as with non blocking caches, we don't have enough
 * time to really hide the latency.
 */
LEAF(_pagezero)
	.set	noreorder
	beqz	a1,2f			# make sure length is non-zero
	li	t0,3*CACHE_SLINE_SIZE	# BDSLOT: size of secondary copy
	addi	a1,-(3*CACHE_SLINE_SIZE)# do last 4 lines seperately

1:	sd	zero,  0(a0)	;	sd	zero, 32(a0)	# line 1 + 2
	addi	a1,-CACHE_SLINE_SIZE
	sd	zero, 80(a0)	;	sd	zero,112(a0)	# line 3 + 4
	sd	zero,  8(a0)	;	sd	zero, 40(a0)	# bank 1
	sd	zero, 16(a0)	;	sd	zero, 24(a0)	# bank 2
	sd	zero, 64(a0)	;	sd	zero, 72(a0)	# bank 1
	sd	zero, 88(a0)	;	sd	zero,120(a0)	# bank 2
	sd	zero, 96(a0)	;	sd	zero,104(a0)	# bank 1
	sd	zero, 48(a0)	;	sd	zero, 56(a0)	# bank 2
	bgtz	a1,1b			# loop more?
	daddiu	a0,CACHE_SLINE_SIZE	# BDSLOT: bump address

1:	sd	zero,  0(a0)	;	sd	zero, 32(a0)	# line 1 + 2
	addi	t0,-CACHE_SLINE_SIZE
	sd	zero, 80(a0)	;	sd	zero,112(a0)	# line 3 + 4
	sd	zero,  8(a0)	;	sd	zero, 40(a0)	# bank 1
	sd	zero, 16(a0)	;	sd	zero, 24(a0)	# bank 2
	sd	zero, 64(a0)	;	sd	zero, 72(a0)	# bank 1
	sd	zero, 88(a0)	;	sd	zero,120(a0)	# bank 2
	sd	zero, 96(a0)	;	sd	zero,104(a0)	# bank 1
	sd	zero, 48(a0)	;	sd	zero, 56(a0)	# bank 2

	cache	CACH_BARRIER,0(a0)	# prevent speculation

	bgtz	t0,1b			# loop more?
	daddiu	a0,CACHE_SLINE_SIZE	# BDSLOT: bump address

2:	j	ra
	nop				# BDSLOT
	.set	reorder
	END(_pagezero)
#endif /* HEART_COHERENCY_WAR || HEART_INVALIDATE_WAR */

/*
 * Routine to set count and compare system coprocessor registers,
 * used for the scheduling clock.
 *
 * We mess around with the count register, so it can't be used for
 * other timing purposes.
 *
 * Align at 7 (128B) to ensure to minimize secondary cache misses.
 * We try very hard to get the common case on one 64B icache line,
 * so the latency does not get too moldy waiting for another i$ miss.
 */
	.align	7
LEAF(resetcounter)
XLEAF(resetcounter_val)	
        .set    noreorder
	mfc0	t1,C0_SR		# [0]
	li	a1,SR_KADDR		# [1]
        li      v0,1000                 # [2] "too close for comfort" / default
        mtc0    a1,C0_SR                # [3] disable ints while we muck around
        mfc0    t2,C0_COMPARE		# [4]
        mfc0    t0,C0_COUNT		# [5] get count 2nd, so latency closer
	mtc0    zero,C0_COUNT           # [6] start COUNT back at zero ASAP
        sltu    ta1,t0,t2		# [7] if COUNT < COMPARE
	bne	ta1,zero,3f		# [8] handle via trimcompare
	subu	ta0,t0,t2		# [9] BDSLOT: ta0 = COUNT - COMPARE
	sltu	ta1,a0,ta0              # [A] if (COUNT-COMPARE)
	bne	ta1,zero,1f             # [B]  exceeds interval, then way past!
	subu	a0,ta0                  # [C] decr latency from next interval
6:	sltu	ta1,a0,v0               # [D] if next COMPARE not soon
	movz	v0,a0,ta1		# [E]  then use a0 as the delta
1:	mtc0    v0,C0_COMPARE           # [F] set COMPARE to the next interval
2: 	j	ra
	mtc0    t1,C0_SR                # BDSLOT: reenable intrs
3:
	/* We should not call resetcounter before a tick, as our last
	 * interval is not up.  Try and handle mostly gracefully, especially
	 * since we already zeroed C0_COUNT.
	 */
	mtc0	t0,C0_COUNT		# we zeroed this, and lost time, but...
	subu	ta0,t2,t0		# ta0 = COUNT - COMPARE
	sltu	ta1,ta0,v0		# if delta < 1000 ticks then
	bne	ta1,zero,2b		# let counter_inter handle
	addu	ta1,a0,v0		# BDSLOT: add  saftey zone
	sltu	ta1,ta0,ta1		# if delta < (new delta + 1000)
	beq	ta1,zero,6b		# (not case): continue checks.
	nop				# BDSLOT
	b	2b			# return and let counter_intr handle
	nop				# BDSLOT
        .set    reorder
        END(resetcounter)

/* Backs the compare up closer to count to make the interrupt happen sooner
 * w/o having to worry about how long it takes to reset the count register.
 */
	.align	7
LEAF(trimcompare)
        .set    noreorder
        mfc0    t1,C0_SR
        li      t0,SR_KADDR
        mtc0    t0,C0_SR                # disable ints while we muck around
        mfc0    t0,C0_COUNT
        mfc0    t2,C0_COMPARE
        li      t3,1000                 # "too close for comfort" value
	/* Guard against any races as we do not wish to miss or clear any
	 * interrupts.
	 */
	sltu	ta1,t0,t2		# if COUNT > COMPARE
	beq	ta1,zero,9f		# exit (interrupt should be pending)
	subu	ta0,t0,t2		# BDSLOT: delta
	sltu	ta1,ta0,t3		# if delta < 1000 ticks
	bne	ta1,zero,9f		# exit (interrupt happening soon)
	sltu	ta1,a0,ta0		# if new delta > (compare - count)
	beq	ta1,zero,7f		# start interrupt now
	subu	a1,t2,a0		# BDSLOT: new compare value
	sltu	ta1,t0,a1		# if COUNT > NEW COMPARE
	beq	ta1,zero,7f		# start interrupt now
	subu	ta0,a1,t0		# BDSLOT: new delta
	sltu	ta1,ta0,t3		# if new delta < 1000 ticks
	beq	ta1,zero,8f		# start interrupt now
	nop				# BDSLOT
	/* set-up interrupt to happen "now", within a safe window */
7:	daddu	a1,t0,t3		# read count + 1000 is new compare
8:	mtc0	a1,C0_COMPARE		# new compare value
9: 	j	ra
	mtc0	t1,C0_SR                # BDSLOT: reenable intrs 
        .set    reorder
        END(trimcompare)

/* Initialize count compare interrupt w/o any checks or latency math.
 */
	.align	7
LEAF(initcounter)
        .set    noreorder
        mfc0    t1,C0_SR
        li      t0,SR_KADDR
        mtc0    t0,C0_SR                # disable ints while we muck around
	mtc0	zero,C0_COUNT		# start COUNT at zero
	mtc0	a0,C0_COMPARE		# set-up COMPARE for next interval
	j	ra
	mtc0	t1,C0_SR		# BDSLOT: reenable interrupts
	.set	reorder
	END(initcounter)

/*  On slow count/compare we need reset the latency to avoid blowing out
 * p_next_intr with a bogus latency.  We do loose a bit of time here.
 *
 * Should fit on a cacheline with initcounter.
 */
LEAF(resetlatency)
        .set    noreorder
	li	a0,1
        mfc0    t1,C0_SR
        li      t0,SR_KADDR
        mtc0    t0,C0_SR                # disable ints while we muck around
	mtc0	a0,C0_COUNT		# start COUNT at 1
	mtc0	zero,C0_COMPARE		# compare at 0
	j	ra
	mtc0	t1,C0_SR		# BDSLOT: reenable interrupts
	.set	reorder
	END(resetlatency)
