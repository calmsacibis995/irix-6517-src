/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* Copyright(C) 1986, MIPS Computer Systems */

#ident	"$Revision: 1.126 $"

#include "ml/ml.h"
#include "sys/traplog.h"
/**************************************************************************
 *
 * WARNING  WARNING WARNING WARNING WARNING
 *
 * This module re-organized for TFP.  Common code is at end so that the
 * utlbmiss handler can be aligned on a page boundary as the first item
 * in this module.  It has an "align 12" but this only works correctly in
 * the 32-bit (COFF) "ld" if utlbmiss.o is the first object loaded.
 *
 * On TFP, tfpcacheops.o will be loaded next as it requires 128-byte
 * alignment.  For this to work, this module must do an "align 12" to bump
 * us to the next page (leaving us on a page boundary).
 **************************************************************************/
	.set	noat
	.set	noreorder

#ifdef ULI_TSTAMP1

#define ULI_GET_TS(r1, r2, num, which)		\
	.set	reorder				;\
	CPUID_L	r1, VPDA_CPUID(zero)		;\
	subu	r1, TSTAMP_CPU			;\
	bne	r1, zero, 1f			;\
	LA	r2, EV_RTC			;\
	lw	r2, 4(r2)			;\
	LA	r1, which			;\
	sw	r2, num*ULITS_SIZE(r1)		;\
1:

#endif

/*
 * workaround for potential R4000 MP bug.
 * When a processor receives an intervention just as it
 * generates a tlbmiss when executing sequentially through
 * a page boundary, then the tlbmiss can get "garbage"
 * in the BADVADDR. Check that the virtual address we read
 * really has NO tlb -- if it does, then the VA is bogus
 * and we just return.
 */

#if _MIPS_SIM != _ABI64
#define CONTEXT		C0_CTXT
#else
#define CONTEXT		C0_EXTCTXT
#endif

#if MP_R4000_BADVA_WAR
#define	MP_R4000_BADVA_BUGFIX(reg,label)	\
	c0	C0_PROBE;			\
	NOP_0_4;				\
	mfc0	reg,C0_INX;			\
	NOP_0_2;				\
	bltz	reg,label;			\
	nop;					\
	eret
#else
#define	MP_R4000_BADVA_BUGFIX(reg,label)
#endif

/* Another R4000/R4400 bug
	The following code sequence exposes an R4000 bug - if a
	C0_READI happens to be lurking in the primary Icache, and the
	cache alignments are as follows:
	0x80000028		b	5f
	0x8000002c		c0	C0_WRITER
	0x80000030	5:	nop

	Suppose that tlbdropin had been executed, and the C0_READI
	was in the cache at:
	0x8005e030		c0	C0_READI

	Note that the readi is the first word of the next line in
	the primary cache. The R4K starts the READI down the pipe
	before it figures out that it is a cache miss. At this point,
	something bad happens which results in the R4K hanging solid.
	My theory is that the R4k makes a mistake, which results in
	a TLB shutdown.

	The branch appears to be a necessary condition - if that is
	replaced by a nop, and all the other instruction aligmnents
	are maintained, the hang goes away.

	Unfortunately, MTI cannot run the test case on the simulator
	to fully understand the bug, so we are left with putting in
	a workaround that appears to fix the problem. That workaround
	is to just restore the old code that was in prior to the branch
	optimization.

	Steve Cobb, 3/24/97
*/

#if	NBPP == 4096
#define CTXT_TO_PTE_ALIGN(reg)	PTR_SRA	reg, 1
#define CTXT_TO_PTE_CLEAN(reg)
#endif

#if	NBPP == 16384
#define CTXT_TO_PTE_ALIGN(reg)	PTR_SRA	reg, KPTE_TLBPRESHIFT+PGSHFTFCTR+3+PTESZ_SHFT
#define CTXT_TO_PTE_CLEAN(reg)	PTR_SLL	reg, 3+PTESZ_SHFT
#endif

#if (R4000 || R10000) && _MIPS_SIM == _ABI64
#define	CTXT_REGION_ADD(scratch,target)				\
					li	scratch,3 ;	\
					dsll	scratch,62 ;	\
					or	target, scratch
#else
#define	CTXT_REGION_ADD(scratch,target)
#endif



#if R4000 || R10000
/*
 * This is the standard prologue for all utlbmiss handlers.  It will
 * be copied into locore and executed prior to executing the various
 * utlbmiss handlers.
 * The standard utlbmiss handler will be copied immediately following 
 * this prolog and VPDA_UTLBMISSHNDLR will be setup to "jump" to this
 * location in locore when using the standard utlbmiss handler.  This
 * avoids requiring an additional secondary cache line in machines
 * which have secondary caches.
 */
NESTED(utlbmiss_prolog_up, 0, k1)
	DMFC0(k0,CONTEXT)
	nop
	CTXT_TO_PTE_ALIGN(k0)		# Convert for 4 byte ptes
EXPORT(eutlbmiss_prolog_up)
	END(utlbmiss_prolog_up)

#if MP || IP28 || TRAPLOG_DEBUG
NESTED(utlbmiss_prolog_mp, 0, k1)

#if SABLE && R4000
EXPORT(utlbmiss_prolog_up)
/* Work around a bug in SABLE which doesn't generate current version
 * of C0_CTXT when the MSB is set (i.e. for K2 addresses).
 */
	mfc0	k0,C0_CTXT
	srl	k1,k0,24
	sll	k1,k1,24		# turn off bit 23
	sll	k0,k0,9
	srl	k0,k0,9
	add	k0,k1			# this is correct C0_CTXT

	/* include 'real" utlbmiss handler prolog */

	PTR_L	k1,VPDA_UTLBMISSHNDLR   # load utlbmiss handler address
	j	k1
	CTXT_TO_PTE_ALIGN(k0)		# BDSLOT Convert for 4 byte ptes
EXPORT(eutlbmiss_prolog_mp)
EXPORT(eutlbmiss_prolog_up)

#else /* !SABLE */

	/* NOTE: The following "taken branch" supplies sufficient delay
	 * after the DMFC0 in the BDSLOT and before execution of the
	 * target instruction which utilizes the result of the DMFC
	 * (taken branches stall for 2 extra cycles).
	 */

	/* DEBUG kernels always use the "slow" utlbmiss prolog so that
	 * we can vector utlbmisses to symmon when appropriate.  Otherwise
	 * we end up with "Bad Address" complaints from symmon when
	 * we try to display kernel data structures with K2 pointers.
	 * This is accomplished via the utlbmiss_reset() and utlbmiss_resume()
	 * code which will always patch the prolog when these conditions
	 * are true.
	 */
	/* PTR_L k1,VPDA_UTLBMISSHNDLR   # load utlbmiss handler address*/

	b	2f			# branch to standard utlbmiss
					# (patched for special handlers)
	DMFC0(k0,CONTEXT)		# BDSLOT

	/* This code handles special utlbmiss handler vectoring.
	 * Only invoked when first instruction of prolog has been patched
	 * to load "k1" with the utlbmisshndlr address.
	 */
	j	k1
2:	CTXT_TO_PTE_ALIGN(k0)		# BDSLOT Convert for 4 byte ptes

EXPORT(eutlbmiss_prolog_mp)

#endif	/* !SABLE */
	END(utlbmiss_prolog_mp)
#endif /* MP */

	/* NOTE: We align to a cacheline boundary so this handler does
	 * not cross a cacheline boundary and (possibly) take a second
	 * cache miss.
	 */
	.align	7
NESTED(utlbmiss, 0, k1)			# Copied down to 0x80000000
	TRAPLOG_UTLBMISS(1)
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)
	
XNESTED(utlbmiss_not_large_page)	
	PTE_L	k1,0(k0)
	PTE_L	k0,NBPDE(k0)		# LDSLOT load the second mapping
#if defined(MP_R4000_BADVA_WAR)
	c0	C0_PROBE		# we would "stall" here anyway
#endif
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_0
	TLBLO_FIX_HWBITS(k0)
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k1,C0_INX
	mtc0	k0,C0_TLBLO_1
	bgez	k1,1f			# don't write tlb if probe hit.
	nop
	c0	C0_WRITER
	NOP_0_3
1:
#else
#ifndef R10000
	mtc0	k0,C0_TLBLO_1
	nop
	c0	C0_WRITER
	NOP_0_3
#else
	/* R10000 does not have hazards wrt C0_WRITER and ERET */
	mtc0	k0,C0_TLBLO_1
	c0	C0_WRITER
#endif /* R10000 */
#endif
	eret
EXPORT(eutlbmiss)
	END(utlbmiss)

#ifdef	PTE_64BIT
NESTED(lpg_utlbmiss, 0, k1)			# Copied down to 0x80000000
	TRAPLOG_UTLBMISS(1)
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)
	PTE_L	k1,0(k0)
	dsrl	k1, k1, PAGE_MASK_INDEX_SHIFT	# Get the page size shift mask
	andi	k1, k1, PAGE_MASK_INDEX_BITMASK
#ifdef MP
	beq	k1, zero, utlbmiss_not_large_page	# if pgsz  == 0 branch
	nop
#else
	beq	k1, zero, 1f			# if pgsz  == 0 branch
	nop
#endif	

	#	code to drop in  a large page TLB

	sreg	AT,VPDA_ATSAVE(zero)	# Get a temporary register

	/*
	 * The following code computes the page mask from the page mask index
	 * of the pte as follows.
	 * (((1 << pte_pgmask_index)-1) << PGMASK_SHFT)
	 * Thus for a 64K page the page mask index will be 4 and the page mask
	 * will be 0x1e000.
	 */
	
	li	AT, 1
	sllv	AT, AT, k1		# 1 << page size shift
	addi	AT, AT,-1		# pagemask is in AT
	sll	AT, PGMASK_SHFT	# Shift by 13 bits
	mtc0	AT, C0_PGMASK
#ifdef	DEBUG
	sreg	AT, VPDA_K1SAVE(zero)    # Get a temporary register
#endif

	/*
	 * Align the kpte address to the even pte based on the page size.
	 * The code does (k0 & ~((2*(page_size/NBPP)*sizeof(pte)) - 1));
	 * The page mask is still in register k1.
	 */
	srl	k1, AT, PGMASK_SHFT + PGSHFTFCTR
	addi	k1, k1, 1
	or	AT, zero, k1	# Copy k1 in AT
	sll	AT, AT, 1 + PTE_SIZE_SHIFT # AT = 2*num_base_pages*sizeof(pte)
	addi	AT, AT, -1
	nor	AT, zero, AT		# Complement AT
	and	k0, k0, AT

	/*
	 * Get back the offset from the even pte to the odd pte. After the next
	 * instruction k1 will contain sizeof(pte)*num_base_pages
	 */

	sll	k1, k1, PTE_SIZE_SHIFT # k1 = num_base_pages * sizeof(pte)

	PTE_L	AT, 0(k0)
	TLBLO_FIX_HWBITS(AT)		# Clear sw bits
	mtc0    AT,C0_TLBLO_0		# Put in entry lo0

	/*
	 * Compute the odd pte address. Add
	 * (page_size/NBPP)*sizeof(pte) to k0 to get it.
	 */

	dadd	k0, k0, k1
	PTE_L	AT, 0(k0)

#if defined(MP_R4000_BADVA_WAR)
	c0	C0_PROBE		# we would "stall" here anyway
#endif
	TLBLO_FIX_HWBITS(AT)		# Clear sw bits
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k0,C0_INX
	mtc0	AT,C0_TLBLO_1

	bgez	k0,2f			# don't write tlb if probe hit.
	nop
	c0	C0_WRITER
	NOP_0_3
2:
#else
	mtc0	AT,C0_TLBLO_1
	c0	C0_WRITER		# Drop it into the TLB.
	nop
#endif

	li	k1, TLBPGMASK_MASK
	mtc0	k1, C0_PGMASK		# restore the original page mask
	lreg    AT, VPDA_ATSAVE(zero)	# Restore the temporary register
	eret

#ifndef MP	
1:
	j	utlbmiss_not_large_page
	nop
#endif
EXPORT(elpg_utlbmiss)
	END(lpg_utlbmiss)
#endif /* PTE_64BIT */


/*
 * Utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 *
 * This handler is overlaid onto utlbmiss at process switch
 * time only for those processes which have overlay segments.
 *
 * NOTE: We align to a cacheline boundary in order to minimize the number
 * of cachelines required to contain this handler.
 */
	.align	7
NESTED(utlbmiss2, 0, k1)
	TRAPLOG_UTLBMISS(6)
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)

XNESTED(utlbmiss2_not_large_page)	
	PTE_L	k1,0(k0)
	PTE_L	k0,NBPDE(k0)		# LDSLOT load the second mapping
#if defined(MP_R4000_BADVA_WAR)
	c0	C0_PROBE		# we would "stall" here anyway
#endif
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_0
	TLBLO_FIX_HWBITS(k0)

	/* Now we check if either entry of the pair is a
	 * shared sentinel.
	 */
#if R10000
	/* R10000 does not have hazards wrt C0_WRITER and ERET */
	mtc0	k0,C0_TLBLO_1
	xori	k1,TLBLO_SHRD_SENTINEL	# BDSLOT
#if MP	
	beq	k1,zero,utlbmiss_sharedseg1a
	/* R10000 does NOT need any special hazard cycles before the eret */
	
	xori	k0,TLBLO_SHRD_SENTINEL
	beq	k0,zero,utlbmiss_sharedseg2a
#else /* UP */
	beq	k1,zero,1f
	/* R10000 does NOT need any special hazard cycles before the eret */
	
	xori	k0,TLBLO_SHRD_SENTINEL
	beq	k0,zero,2f
#endif /* UP */	
	DMFC0(k1,CONTEXT)		# BDSLOT
	
	c0	C0_WRITER
	ERET_PATCH(utlbmiss_eret_0)	# patchable eret

#ifndef MP
	/* On UP system we may end up copying utlbmiss2 to locore exception
	 * region, so we must have "j" instructions to non-moved code.
	 * NOTE: In practice, the utlbmiss2 code is generally "too large"
	 * to be moved.
	 */
1:	j	utlbmiss_sharedseg1
	DMFC0(k1,CONTEXT)
	
2:	j	utlbmiss_sharedseg2
	LI	k0,KPTE_USIZE		# BDSLOT
#endif /* !MP */
#else /* !R10000 */
	xori	k1,TLBLO_SHRD_SENTINEL
	beq	k1,zero,1f
	mtc0	k0,C0_TLBLO_1		# BDSLOT
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k1,C0_INX
	xori	k0,TLBLO_SHRD_SENTINEL
	beq	k0,zero,2f
	li	k0,KPTE_USIZE		# BDSLOT

	bgez	k1,5f			# exit - no dropin if HIT in tlb
	nop
	c0	C0_WRITER
	NOP_0_3
5:	eret

#else
	xori	k0,TLBLO_SHRD_SENTINEL
	beq	k0,zero,2f
	li	k0,KPTE_USIZE		# BDSLOT
	c0	C0_WRITER
	NOP_0_3

	ERET_PATCH(utlbmiss_eret_0)	# patchable eret
#endif

1:	j	utlbmiss_sharedseg1
	DMFC0(k1,CONTEXT)
2:	j	utlbmiss_sharedseg2
	DMFC0(k1,CONTEXT)
#endif /* !R10000 */
EXPORT(eutlbmiss2)
	END(utlbmiss2)


#ifdef R10000
#ifdef MP	
LEAF(utlbmiss_sharedseg1a)
	DMFC0(k1,CONTEXT)
	END(utlbmiss_sharedseg1a)
#endif	
LEAF(utlbmiss_sharedseg1)
	bne	k0,zero,onlyseg1
	
	/* BOTH are shared */
	LI	k0,KPTE_USIZE		# BDSLOT

	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	PTE_L	k0,0(k1)
	PTE_L	k1,NBPDE(k1)
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_HWBITS(k1)
	mtc0	k0,C0_TLBLO_0
	mtc0	k1,C0_TLBLO_1

	/* R10000 does not have hazards wrt C0_WRITER and ERET */
	c0	C0_WRITER		# BDSLOT - cancel if no br (sentinel)
	eret

	/* Only seg1 is shared, seg2 already dropped in */
	
onlyseg1:			
	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	PTE_L	k0,0(k1)
	TLBLO_FIX_HWBITS(k0)
	mtc0	k0,C0_TLBLO_0
	c0	C0_WRITER		# BDSLOT - cancel if no br (sentinel)
	ERET_PATCH(utlbmiss_eret_1)	# patchable eret
	END(utlbmiss_sharedseg1)

#else /* !R10000 */

LEAF(utlbmiss_sharedseg1)
	# We are going to do first mappings from the shared pmap.
	# Second mapping has been placed in TLBLO_1 but needs to
	# be checked for shared sentinel.
	# at this point, k1 has C0_CTXT (unshifted), k0 is not useful

	LI	k0,KPTE_USIZE

	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	PTE_L	k0,0(k1)
	mfc0	k1,C0_TLBLO_1		# LDSLOT
	TLBLO_FIX_HWBITS(k0)
	mtc0	k0,C0_TLBLO_0

	xori	k1,TLBLO_SHRD_SENTINEL
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k0,C0_INX		# test result of probe
	beq	k1,zero,utlbmiss_sharedseg2a
	DMFC0(k1,CONTEXT)		# BDSLOT

	bgez	k0,5f			# exit - no dropin if HIT in tlb
	nop
	c0	C0_WRITER
	NOP_0_3
5:
#else
	beq	k1,zero,utlbmiss_sharedseg2a
	DMFC0(k1,CONTEXT)		# BDSLOT
	c0	C0_WRITER
	NOP_0_3
#endif

	ERET_PATCH(utlbmiss_eret_1)	# patchable eret
	END(utlbmiss_sharedseg1)
#endif /* !R10000 */		


LEAF(utlbmiss_sharedseg2a)
	LI	k0,KPTE_USIZE		# BDSLOT
	END(utlbmiss_sharedseg2a)

LEAF(utlbmiss_sharedseg2)
	# We are going to do only the odd mapping from the shared pmap.
	# at this point, k1 == C0_CTXT, k0 == KPTE_USIZE
	# C0_TLBLO has the correct low mapping

	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	PTE_L	k1,NBPDE(k1)
#if R10000
	/* R10000 does not have hazards wrt C0_WRITER and ERET */
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_1
	c0	C0_WRITER
#else /* !R10000 */
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k0,C0_INX		# LDSLOT - test result of probe
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_1

	bgez	k0,5f
	nop
	c0	C0_WRITER
	NOP_0_3
5:
#else
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_1
	nop
	c0	C0_WRITER
	NOP_0_3
#endif
#endif /* !R10000 */		
	ERET_PATCH(utlbmiss_eret_2)	# patchable eret
	END(utlbmiss_sharedseg2)

#ifdef	PTE_64BIT
/*
 * Large page utlbmiss handler for processes with overlay segments.
 * This is the same as as utlbmiss2 except for the check for large page
 * ptes in the beginning. Note that sentinel checks are not done for ptes
 * which are large pages. This is because large page ptes will not have
 * sentinels in either the even or the odd pte entries. 
 * If a pte is a sentinel
 * pte the corresponding shared pte is faulted in as a base page pte even though
 * it may be a large page pte. This is because it is hard to maintain
 * consistency between large pages on the shared pte and the private ptes
 * as they belong to different processes and different regions. A shared process
 * might create a large page on a shared region covered by sentinel ptes in
 * another sproc. The sentinel pte handling code thus is the
 * same as for a base page and the code jumps to utlbmiss_sharedseg1
 * and utlbmiss_sharedseg2 to handle them.
 */
NESTED(lpg_overlay_utlbmiss, 0, k1)
	TRAPLOG_UTLBMISS(6)
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)

	PTE_L	k1,0(k0)
	dsrl	k1, k1, PAGE_MASK_INDEX_SHIFT	# Get the page size shift mask
	andi	k1, k1, PAGE_MASK_INDEX_BITMASK
#if MP
	beq	k1, zero, utlbmiss2_not_large_page	# if pgsz  <= 0 branch
	nop
#else
	/* need to jump indirect since utlbmiss MIGHT be copied to locore */
	beq	k1, zero, not_large_page	# if pgsz  <= 0 branch
	nop
#endif	

	#	code to drop in  a large page TLB

	sreg	AT,VPDA_ATSAVE(zero)	# Get a temporary register

	/*
	 * The following code computes the page mask from the page mask index
	 * of the pte as follows.
	 * (((1 << pte_pgmask_index)-1) << PGMASK_SHFT)
	 * Thus for a 64K page the page mask index will be 4 and the page mask
	 * will be 0x1e000.
	 */
	
	li	AT, 1
	sllv	AT, AT, k1		# 1 << page size shift
	addi	AT, AT,-1		# pagemask is in AT
	sll	AT, PGMASK_SHFT		# Shift by 13 bits
	mtc0	AT, C0_PGMASK
#ifdef	DEBUG
	sreg	AT, VPDA_K1SAVE(zero)    # Get a temporary register
#endif

	/*
	 * Align the kpte address to the even pte based on the page size.
	 * The code does (k0 & ~((2*(page_size/NBPP)*sizeof(pte)) - 1));
	 * The page mask is still in register k1.
	 */
	srl	k1, AT, PGMASK_SHFT + PGSHFTFCTR
	addi	k1, k1, 1
	or	AT, zero, k1		# Copy k1 in AT
	sll	AT, AT, 1 + PTE_SIZE_SHIFT # AT = 2*num_base_pages*sizeof(pte)
	addi	AT, AT, -1
	nor	AT, zero, AT		# Complement AT
	and	k0, k0, AT

	/*
	 * Get back the offset from the even pte to the odd pte. After the next
	 * instruction k1 will contain sizeof(pte)*num_base_pages
	 */

	sll	k1, k1, PTE_SIZE_SHIFT # k1 = num_base_pages * sizeof(pte)

	PTE_L	AT, 0(k0)
	TLBLO_FIX_HWBITS(AT)		# Clear sw bits
	mtc0    AT,C0_TLBLO_0		# Put in entry lo0

	/*
	 * Compute the odd pte address. Add
	 * (page_size/NBPP)*sizeof(pte) to k0 to get it.
	 */

	dadd	k0, k0, k1
	PTE_L	AT, 0(k0)

#if defined(MP_R4000_BADVA_WAR)
	c0	C0_PROBE		# we would "stall" here anyway
#endif
	TLBLO_FIX_HWBITS(AT)		# Clear sw bits
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k0, C0_INX
	mtc0	AT, C0_TLBLO_1
	bgez	k0, 1f			# don't do tlb write if probe hit
	nop
	c0	C0_WRITER
	NOP_0_3
1:
#else
	mtc0	AT,C0_TLBLO_1
	c0	C0_WRITER		# Drop it into the TLB.
	nop
#endif

	li	k1, TLBPGMASK_MASK
	mtc0	k1, C0_PGMASK		# restore the original page mask
	lreg    AT, VPDA_ATSAVE(zero)	# Restore the temporary register
	eret

	/*
	 * End of large page utlbmiss code.
	 */

#ifndef MP	
not_large_page:
	j	utlbmiss2_not_large_page
	nop
#endif

EXPORT(elpg_overlay_utlbmiss)
	END(lpg_overlay_utlbmiss)
#endif /* PTE_64BIT */
	
/*
 *	Counting utlbmiss handler.
 */
NESTED(utlbmiss1, 0, k1)
	MFC0(k1,C0_SR)
	NOP_1_0
	andi	k1,SR_KSU_USR
	beq	k1,zero,1f
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss
	sw	k1,VPDA_UTLBMISSES(zero)
1:
	lw	k1,VPDA_KTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss
	sw	k1,VPDA_KTLBMISSES(zero)
EXPORT(eutlbmiss1)
	END(utlbmiss1)

#ifdef	PTE_64BIT
/*
 * Counting utlbmiss handler for processes with large page ptes.
 */
NESTED(lpg_counting_utlbmiss, 0, k1)
	DMFC0(k1,C0_SR)
	NOP_1_0
	andi	k1,SR_KSU_USR
	beq	k1,zero,1f
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	lpg_utlbmiss
	sw	k1,VPDA_UTLBMISSES(zero)
1:
	lw	k1,VPDA_KTLBMISSES(zero)
	addi	k1,1
	j	lpg_utlbmiss
	sw	k1,VPDA_KTLBMISSES(zero)
EXPORT(elpg_counting_utlbmiss)
	END(lpg_counting_utlbmiss)
#endif /* PTE_64BIT */

/*
 * Counting utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 *
 * This handler is overlaid onto utlbmiss at process switch
 * time only for those processes which have overlay segments.
 */
NESTED(utlbmiss3, 0, k1)
	DMFC0(k1,C0_SR)
	NOP_1_0
	andi	k1,SR_KSU_USR
	beq	k1,zero,1f
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss2
	sw	k1,VPDA_UTLBMISSES(zero)
1:
	lw	k1,VPDA_KTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss2
	sw	k1,VPDA_KTLBMISSES(zero)
EXPORT(eutlbmiss3)
	END(utlbmiss3)

#ifdef	PTE_64BIT
/*
 * Counting utlbmiss handler for processes with large page ptes.
 * and overlay segments.
 */
NESTED(lpg_overlay_counting_utlbmiss, 0, k1)
	DMFC0(k1,C0_SR)
	NOP_1_0
	andi	k1,SR_KSU_USR
	beq	k1,zero,1f
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	lpg_overlay_utlbmiss
	sw	k1,VPDA_UTLBMISSES(zero)
1:
	lw	k1,VPDA_KTLBMISSES(zero)
	addi	k1,1
	j	lpg_overlay_utlbmiss
	sw	k1,VPDA_KTLBMISSES(zero)
EXPORT(elpg_overlay_counting_utlbmiss)
	END(lpg_overlay_counting_utlbmiss)
#endif /* PTE_64BIT */

#if R4000 && (IP19 || IP22)
/*
 * Copies of the above utlbmiss handlers with a workaround for the
 * 250MHz R4400 modules (clear entrylo regs before updating to avoid
 * corrupted values being loaded).
 *
 * These handlers are copied into utlbmiss_swtch[] and to UT_VEC
 * only if the system contains a 250MHz module, otherwise the standard
 * R4400 handlers are used (also see TLBLO_FIX_250MHz() macro in ml.h).
 */
NESTED(utlbmiss_250mhz, 0, k1)			# Copied down to 0x80000000
	TRAPLOG_UTLBMISS(1)
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)
	PTE_L	k1,0(k0)
	PTE_L	k0,NBPDE(k0)		# LDSLOT load the second mapping
#if defined(MP_R4000_BADVA_WAR)
	c0	C0_PROBE		# we would "stall" here anyway
#endif
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	mtc0	k1,C0_TLBLO_0
	TLBLO_FIX_HWBITS(k0)
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k1,C0_INX
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	k0,C0_TLBLO_1

	bgez	k1,1f			# don't write tlb if probe hit.
	nop
	c0	C0_WRITER		# BDSLOT - cancel if branch (probe hit)
	NOP_0_3
1:	eret
#else
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	k0,C0_TLBLO_1
	nop
	c0	C0_WRITER
	NOP_0_3
	eret
#endif
EXPORT(eutlbmiss_250mhz)
	END(utlbmiss_250mhz)

/*
 * Utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 *
 * This handler is overlaid onto utlbmiss at process switch
 * time only for those processes which have overlay segments.
 */
NESTED(utlbmiss2_250mhz, 0, k1)
	TRAPLOG_UTLBMISS(6)
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)
	PTE_L	k1,0(k0)
	PTE_L	k0,NBPDE(k0)		# LDSLOT load the second mapping
#if defined(MP_R4000_BADVA_WAR)
	c0	C0_PROBE		# we would "stall" here anyway
#endif
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	mtc0	k1,C0_TLBLO_0
	TLBLO_FIX_HWBITS(k0)

	/* Now we check if either entry of the pair is a
	 * shared sentinel.
	 */
	xori	k1,TLBLO_SHRD_SENTINEL
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	beq	k1,zero,1f
	mtc0	k0,C0_TLBLO_1		# BDSLOT
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k1,C0_INX
	xori	k0,TLBLO_SHRD_SENTINEL
	beq	k0,zero,2f
	li	k0,KPTE_USIZE		# BDSLOT

	bgez	k1,5f			# exit - no dropin if HIT in tlb
	nop
	c0	C0_WRITER		# BDSLOT - cancel if no br (probe hit)
	NOP_0_3
5:	eret
#else
	xori	k0,TLBLO_SHRD_SENTINEL
	beq	k0,zero,2f
	LI	k0,KPTE_USIZE		# BDSLOT
	c0	C0_WRITER
	NOP_0_3
	ERET_PATCH(utlbmiss_eret_0_250mhz)	# patchable eret
#endif

1:	j	utlbmiss_sharedseg1_250mhz
	DMFC0(k1,CONTEXT)
2:	j	utlbmiss_sharedseg2_250mhz
	DMFC0(k1,CONTEXT)
EXPORT(eutlbmiss2_250mhz)
	END(utlbmiss2_250mhz)

LEAF(utlbmiss_sharedseg1_250mhz)
	# We are going to do first mappings from the shared pmap.
	# Second mapping has been placed in TLBLO_1 but needs to
	# be checked for shared sentinel.
	# at this point, k1 has C0_CTXT (unshifted), k0 is not useful

	LI	k0,KPTE_USIZE

	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	PTE_L	k0,0(k1)
	mfc0	k1,C0_TLBLO_1		# LDSLOT
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	mtc0	k0,C0_TLBLO_0

	xori	k1,TLBLO_SHRD_SENTINEL
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k0,C0_INX		# test result of probe
	beq	k1,zero,utlbmiss_sharedseg2a_250mhz
	DMFC0(k1,CONTEXT)		# BDSLOT

	bgez	k0,5f			# exit - no dropin if HIT in tlb
	nop
	c0	C0_WRITER
	NOP_0_3
5:
#else
	beq	k1,zero,utlbmiss_sharedseg2a_250mhz
	DMFC0(k1,CONTEXT)		# BDSLOT
	c0	C0_WRITER
	NOP_0_3
#endif
	ERET_PATCH(utlbmiss_eret_1_250mhz)	# patchable eret
	END(utlbmiss_sharedseg1_250mhz)


LEAF(utlbmiss_sharedseg2a_250mhz)
	LI	k0,KPTE_USIZE		# BDSLOT
	END(utlbmiss_sharedseg2a_250mhz)

LEAF(utlbmiss_sharedseg2_250mhz)
	# We are going to do only the odd mapping from the shared pmap.
	# at this point, k1 == C0_CTXT, k0 == KPTE_USIZE
	# C0_TLBLO has the correct low mapping

	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	PTE_L	k1,NBPDE(k1)
#if defined(MP_R4000_BADVA_WAR)
	mfc0	k0,C0_INX		# LDSLOT - test result of probe
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	k1,C0_TLBLO_1

	bgez	k0,5f			# don't do tlb write if probe hit
	nop
	c0	C0_WRITER
	NOP_0_3
5:
#else
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	k1,C0_TLBLO_1
	nop
	c0	C0_WRITER
	NOP_0_3
#endif
	ERET_PATCH(utlbmiss_eret_2_250mhz)	# patchable eret
	END(utlbmiss_sharedseg2_250mhz)

/*
 *	Counting utlbmiss handler.
 */
NESTED(utlbmiss1_250mhz, 0, k1)
	DMFC0(k1,C0_SR)
	NOP_1_0
	andi	k1,SR_KSU_USR
	beq	k1,zero,1f
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss_250mhz
	sw	k1,VPDA_UTLBMISSES(zero)
1:
	lw	k1,VPDA_KTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss_250mhz
	sw	k1,VPDA_KTLBMISSES(zero)
EXPORT(eutlbmiss1_250mhz)
	END(utlbmiss1_250mhz)

/*
 * Counting utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 *
 * This handler is overlaid onto utlbmiss at process switch
 * time only for those processes which have overlay segments.
 */
NESTED(utlbmiss3_250mhz, 0, k1)
	DMFC0(k1,C0_SR)
	NOP_1_0
	andi	k1,SR_KSU_USR
	beq	k1,zero,1f
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss2_250mhz
	sw	k1,VPDA_UTLBMISSES(zero)
1:
	lw	k1,VPDA_KTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss2_250mhz
	sw	k1,VPDA_KTLBMISSES(zero)
EXPORT(eutlbmiss3_250mhz)
	END(utlbmiss3_250mhz)

#endif	/* R4000 && (IP19 || IP22) */

#ifdef R4600
/*
 * The R4600 has separate versions of the main handlers, because
 * it does not require the nop's which are needed for the R4000.
 * Removing the nop's improves SPECint92 performance by about 1%
 * on the R4600.  There is not enough difference to justify duplicating
 * utlbmiss2.
 */

NESTED(utlbmiss_prolog_r4600, 0, k1)
	DMFC0(k0,CONTEXT)
	CTXT_TO_PTE_ALIGN(k0)		# Convert for 4 byte ptes
EXPORT(eutlbmiss_prolog_r4600)
	END(utlbmiss_prolog_r4600)

NESTED(utlbmiss_r4600, 0, k1)			# Copied down to 0x80000000
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)
	PTE_L	k1,0(k0)
	PTE_L	k0,NBPDE(k0)		# LDSLOT load the second mapping
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_0
	TLBLO_FIX_HWBITS(k0)
	mtc0	k0,C0_TLBLO_1
	nop
	c0	C0_WRITER
	nop
	ERET_PATCH(utlbmiss_eret_3)	# patchable eret
EXPORT(eutlbmiss_r4600)
	END(utlbmiss_r4600)

/*
 *	Counting utlbmiss handler.
 */
NESTED(utlbmiss_r4600_1, 0, k1)
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss_r4600
	sw	k1,VPDA_UTLBMISSES(zero)
EXPORT(eutlbmiss_r4600_1)
	END(utlbmiss_r4600_1)

#endif /* R4600 */

#ifdef _R5000_BADVADDR_WAR
NESTED(utlbmiss_r5000, 0, k1)			# Copied down to 0x80000000
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)
	lw	k1,0(k0)
	lw	k0,NBPDE(k0)		# LDSLOT load the second mapping
	c0	C0_PROBE		# we would "stall" here anyway
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_0
	mfc0	k1,C0_INX
	TLBLO_FIX_HWBITS(k0)
	bgez	k1,1f
	mtc0	k0,C0_TLBLO_1
	c0	C0_WRITER
	nop
	eret
1:	la	k0,E_VEC
	jr	k0
	nop
EXPORT(eutlbmiss_r5000)
	END(utlbmiss_r5000)

/*
 * Utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 *
 * This handler is overlaid onto utlbmiss at process switch
 * time only for those processes which have overlay segments.
 */
NESTED(utlbmiss2_r5000, 0, k1)
	CTXT_TO_PTE_CLEAN(k0)
	CTXT_REGION_ADD(k1,k0)
	lw	k1,0(k0)
	lw	k0,NBPDE(k0)		# LDSLOT load the second mapping
	c0	C0_PROBE		# we would "stall" here anyway
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_0
	TLBLO_FIX_HWBITS(k0)

	/* Now we check if either entry of the pair is a
	 * shared sentinel.
	 */
	xori	k1,TLBLO_SHRD_SENTINEL
	beq	k1,zero,1f
	mtc0	k0,C0_TLBLO_1		# BDSLOT
	mfc0	k1,C0_INX
	xori	k0,TLBLO_SHRD_SENTINEL
	beq	k0,zero,2f
	li	k0,KPTE_USIZE		# BDSLOT
	bltzl	k1,5f			# exit - no dropin if HIT in tlb
	c0	C0_WRITER		# BDSLOT - cancel if no br (probe hit)
	la	k0,E_VEC
	jr	k0
5:	nop
	eret

2:	j	utlbmiss_sharedseg2_r5000
	DMFC0(k1,CONTEXT)

1:	j	utlbmiss_sharedseg1_r5000
	DMFC0(k1,CONTEXT)
EXPORT(eutlbmiss2_r5000)
	END(utlbmiss2_r5000)

LEAF(utlbmiss_sharedseg1_r5000)
	# We are going to do first mappings from the shared pmap.
	# Second mapping has been placed in TLBLO_1 but needs to
	# be checked for shared sentinel.
	# at this point, k1 has C0_CTXT (unshifted), k0 is not useful

	li	k0,KPTE_USIZE

	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	lw	k0,0(k1)
	mfc0	k1,C0_TLBLO_1		# LDSLOT
	TLBLO_FIX_HWBITS(k0)
	mtc0	k0,C0_TLBLO_0

	xori	k1,TLBLO_SHRD_SENTINEL
	mfc0	k0,C0_INX		# test result of probe
	beq	k1,zero,utlbmiss_sharedseg2a_r5000
	DMFC0(k1,CONTEXT)		# BDSLOT
	bltzl	k0,5f			# exit - no dropin if HIT in tlb
	c0	C0_WRITER		# BDSLOT - cancelled if no branch (probe hit)
	la	k0,E_VEC
	jr	k0
5:	nop
	eret
	END(utlbmiss_sharedseg1_r5000)


LEAF(utlbmiss_sharedseg2a_r5000)
	li	k0,KPTE_USIZE		# BDSLOT
	END(utlbmiss_sharedseg2a_r5000)

LEAF(utlbmiss_sharedseg2_r5000)
	# We are going to do only the odd mapping from the shared pmap.
	# at this point, k1 == C0_CTXT, k0 == KPTE_USIZE
	# C0_TLBLO has the correct low mapping

	CTXT_TO_PTE_ALIGN(k1)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(k1)
	PTR_SUBU k1,k0
	CTXT_REGION_ADD(k0,k1)
	lw	k1,NBPDE(k1)
	mfc0	k0,C0_INX		# LDSLOT - test result of probe
	TLBLO_FIX_HWBITS(k1)
	mtc0	k1,C0_TLBLO_1
       /* Careful when making changes.  Branchs actually have a delay of 3
        * cycles - one explicitly (in the BDslot) and the other 2 are simply
        * processor stalls.
        */
	bltzl	k0,5f
	c0	C0_WRITER		# BDSLOT - cancel if no branch (probe hit)
	la	k0,E_VEC
	jr	k0
5:	nop
	eret
	END(utlbmiss_sharedseg2_r5000)

/*
 *	Counting utlbmiss handler.
 */
NESTED(utlbmiss1_r5000, 0, k1)
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss_r5000
	sw	k1,VPDA_UTLBMISSES(zero)
EXPORT(eutlbmiss1_r5000)
	END(utlbmiss1_r5000)

/*
 * Counting utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 *
 * This handler is overlaid onto utlbmiss at process switch
 * time only for those processes which have overlay segments.
 */
NESTED(utlbmiss3_r5000, 0, k1)
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss2_r5000
	sw	k1,VPDA_UTLBMISSES(zero)
EXPORT(eutlbmiss3_r5000)
	END(utlbmiss3_r5000)
#endif /* _R5000_BADVADDR_WAR */

#endif	/* R4000 || R10000 */

#if TFP

#if ((KPTEBASE & 0x0000ffff) != 0) || (((KPTEBASE>>16) & 0x0000ffff) != 0)
<<bomb - expect low 32-bits of KPTEBASE to be zero>>
<<NOTE: Above test on KPTEBASE was broken into two pieces because compiler
<<      does not do correct comparision using KPTEBASE & 0x00ffffffff.
<<	If low bits must be non-zero, then you need to use "dli k1,KPTEBASE"
<<	which will generate 6 instruction instead of the 3 used here.
#endif

/**************************************************************************
 * utlbmiss -- offset 0x000 within page (use .align 12)
 *************************************************************************/
/*
 * This is the standard prologue for all utlbmiss handlers.  It will
 * be copied into locore and executed prior to executing the various
 * utlbmiss handlers.
 * The standard utlbmiss handler will be copied immediately following 
 * this prolog and VPDA_UTLBMISSHNDLR will be setup to "jump" to this
 * location in locore when using the standard utlbmiss handler.  This
 * avoids requiring an additional secondary cache line in machines
 * which have secondary caches.
 */
	/* Align to page boundary so we can just place this address
	 * into the TrapBase.
	 */
	.align 12
EXPORT(trap_table)

NESTED(utlbmiss, 0, k1)
	TRAPLOG(1)
	/* WARNING: Don't allow the two "dmfc0" to execute in the same
	 * cycle.  Currently the "dsra" on the result of the first load
	 * forces the subsequent "dmfc0" (as well as the "dsra") into
	 * the next superscaler cycle.
	 * KPTEBASE is constant, but is loaded from C0_KPTEBASE since generating
	 * the constant takes 3 instructions, one of them a shift.  This
	 * is faster.
 	 */
	MFC0_BADVADDR_NOHAZ(k0)
	NOP_NADA			# alignment op (for C0_WRITER)
	dsra	k0,PNUMSHFT		# Convert to VPN - FORCE NEXT CYCLE
	dmfc0	k1,C0_KPTEBASE		# Load KPTEBASE
	dsll	k0,2			# Convert VPN for 4 byte pte
	dadd	k0,k1			# address is in KV0 space
	NOP_NADA			# alignment op (for C0_WRITER)
	PTE_L	k1,0(k0)		# load 4 byte pte
	NOP_NADA			# alignment op causes no penalty here
	TLBLO_FIX_HWBITS(k1)		# move pfn bits to correct position
	dmtc0	k1,C0_TLBLO
	/* The preceding "nada"s align the code (and execute faster) than
 	 * the "nops" the compiler will use.  The "align" is just in case
	 * the code at utlbmiss changes -- like the traplog being enabled.
	 * Note that the nada's were placed where they execute "for free"
	 * in the same superscaler cycle in which we would otherwise stall
	 * anyway.
	 */
	TLB_WRITE_AND_ERET
EXPORT(eutlbmiss)
	END(utlbmiss)

/**************************************************************************
 * ktlbmiss_private -- offset 0x400 within page (use .align 10)
 *************************************************************************/
/* ktlbmiss_private - used for TLB miss on Kernel Private Region (KV0)
 * This shouldn't happen (for now), so panic.
 */
	.align	10
NESTED(ktlbmiss_private, 0, k1)
	TRAPLOG(2)
	MFC0_BADVADDR(a1)
	.set	reorder
	PANIC("ktlbmiss_private: BADVADDR %x")
	.set	noreorder
	END(ktlbmiss_private)

/**************************************************************************
 * ktlbmiss -- offset 0x800 within page (use .align 10)
 *************************************************************************/
/* ktlbmiss - used for TLB miss on Kernel Global Region (KV1)
 * This implies that the region bits are both set.
 *
 * We used to have debug code in here that checked the badvaddr to
 * make sure it's smaller than v.v_syssegsz. However, there are a lot
 * of problems with this code (see comments below) and allowing such
 * code will break the alignment, so we've removed that code. If we
 * pick up a garbage pte, we'll (hopefully) end up taking a tlb invalid
 * exception and end up in vfault which will panic.
 */
	.align	10

NESTED(ktlbmiss, 0, k1)
	TRAPLOG(3)
	/* WARNING: Don't allow the two "dmfc0" to execute in the same
	 * cycle.  Currently the "dsll" on the result of the first load
	 * forces the subsequent "dmfc0" (as well as the "dsll") into
	 * the next superscaler cycle.
	 * kptbl is constant after it is setup, but is loaded from
	 * C0_KPTBL since generating for speed.
 	 */
	MFC0_BADVADDR_NOHAZ(k0)
	dsll	k0,2			# clear hi bits
	dmfc0	k1, C0_KPTBL		# base of kernel page table
	dsrl	k0,2+PNUMSHFT		# clear offset bits
	dsll	k0,PDESHFT		# BDSLOT - convert to PDE offset
	dadd	k1,k0
#if EVEREST
	PTR_L	k0, VPDA_KVFAULT	# load base of kernel fault history
	PTE_L	k1,(k1)			# load pde
	TLBLO_FIX_HWBITS(k1)		# move pfn bits to correct position
	bne	k0, zero, kvhist	# alignment op causes no penalty here
	dmtc0	k1,C0_TLBLO		# BDSLOT
#else
	PTE_L	k0,0(k1)		# load pde
	nada				# alignment op causes no penalty here

	TLBLO_FIX_HWBITS(k0)		# move pfn bits to correct position
	nada				# alignment op causes no penalty here
	dmtc0	k0,C0_TLBLO
#endif

	/* The preceding "nada"s align the code (and execute faster) than
 	 * the "nops" the compiler will use.  The "align" is just in case
	 * the code at utlbmiss changes -- like the traplog being enabled.
	 * Note that the nada's were placed where they execute "for free"
	 * in the same superscaler cycle in which we would otherwise stall
	 * anyway.
	 *
	 * Do not touch this code unless you know exactly what you are doing.
	 * The mult/mflo below is needed to break superscalar dispatch. There
	 * is a bug in the TFP processor that forces us to align all tlb write
	 * instructions on quad word boundary and to fill the rest of the quad
	 * word with ssnop's to avoid executing bad instructions in the icache.
	 *
	 * We can't use ssnop's because two ssnop's are not enough (if the
	 * branch is incorrectly predicted, we end up with tlbw/eret/ssnop
	 * in one cycle and 2nd ssnop in second cycle and garbage in third
	 * cycle). The mult/mflo fixes this problem because the mflo will
	 * interlock in d-stage to wait for the multiply to complete.
	 */
	TLB_WRITE_AND_ERET

#if EVEREST
kvhist:
	MFC0_BADVADDR(k1)
	sreg	AT, VPDA_ATSAVE(zero)
	dsll	k1, 2			# get rid of region bits
	dsrl	k1, 2+PNUMSHFT		# get vpn

	dsrl	AT, k1, 3		# get byte offset from vpn
	daddu	k0, AT			# k0 == byte address in kvfault history
	li	AT, 1
	andi	k1, 7			# vpn & 7
	sll	AT, k1
	lbu	k1, 0(k0)
	or	k1, AT			# set bit for this page
	sb	k1, 0(k0)
	lreg	AT, VPDA_ATSAVE(zero)
	TLB_WRITE_AND_ERET
#endif	/* EVEREST */
EXPORT(ektlbmiss)
	END(ktlbmiss)

/**************************************************************************
 * exception_jump -- offset 0xc00 with page (use .align 10)
 *************************************************************************/
	.align 10
LEAF(__j_exceptnorm)
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_GENEX, uli_tstamps)
	.set	noreorder
#endif
	TRAPLOG(4)
#if EVEREST
	/* Make sure that the "shadow" CEL value in the PDA is currently
	 * in affect, if this exception is due to an interrupt.
	 */
	dmfc0	k0, C0_CAUSE
	andi	k0,CAUSE_EXCMASK
	bne	k0,zero,1f
	lbu	k1,VPDA_CELSHADOW(zero)	# BDSLOT
	dmfc0	k0,C0_CEL
	sd	k1,(k0)		# Store correct CEL value
	ld	k0,(k0)		# Wait for CAUSE register to be updated
	sb	k1,VPDA_CELHW(zero)
	/* Following check makes this one consistent with the subsequent
	 * check in intr().  Note that it seems that the SRB_SCHEDCLK interrupt
	 * will appear in the CAUSE register even when it is blocked, just
	 * doesn't generate an interrupt.
	 */
	dmfc0	k0,C0_SR
	/* Need to see if the SR_IE bit is still set.  The bit may have just
	 * been cleared in the C0_SR but has not yet taken affect.  This is
	 * critical with the addition of kernel preemption since on interrupt
	 * exit we perform a preemption check based upon isspl0() and we may
	 * miss an interrupt thread preemption.
	 */
	andi	k1,k0,SR_IE
	beq	k1,zero,2f
	nop
	
	LI	k1,SR_IMASK
	and	k0,k1
	LI	k1,CAUSE_BE|CAUSE_VCI|CAUSE_FPI
	or	k1,k0		# k1 == enabled interrupt mask
	dmfc0	k0, C0_CAUSE	# check to see if still have intr pending
	and	k0,k1
	bne	k0,zero,1f
	nop
2:	eret
1:
#endif
	PTR_L	k0, VPDA_EXCNORM
	j	k0
	nop			# BDSLOT
	END(__j_exceptnorm)

/**************************************************************************
 * This second page of exception vectors will be used by those CPUs which
 * are currently executing a user process which requires one of the
 * "special" utlbmiss handlers.
 *
 * Eventually we can just replicate the handlers here rather than incurring
 * the extra overhead to jump to the master page above.  But until these
 * handlers stablize, we'll just jump there.
 *************************************************************************/

/**************************************************************************
 * utlbmiss_prolog_mp -- offset 0x000 within another page (use .align 12)
 *************************************************************************/
	/* Align MP prolog to a different page so it's address can
	 * also be used directly in TrapBase.
	 */
	.align 12
LEAF(trap_table_vector)
	MFC0_BADVADDR(k0)
	PTR_L	k1,VPDA_UTLBMISSHNDLR   # load utlbmiss handler address
	j	k1
	dsra	k0,PNUMSHFT		# get VPN
	END(trap_table_vector)

/**************************************************************************
 * ktlbmiss_private -- offset 0x400 within page (use .align 10)
 *************************************************************************/

	.align	10

LEAF(ktlbmiss_private_jump)
	j	ktlbmiss_private
	nop
	END(ktlbmiss_private_jump)

/**************************************************************************
 * ktlbmiss -- offset 0x800 within page (use .align 10)
 *************************************************************************/

	.align	10

LEAF(ktlbmiss_jump)
	j	ktlbmiss
	nop
	END(ktlbmiss_jump)

/**************************************************************************
 * exception_jump -- offset 0xc00 with page (use .align 10)
 *************************************************************************/
	.align 10
LEAF(__j_exceptnormv)
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_GENEX, uli_tstamps)
	.set	noreorder
#endif
#if EVEREST
	/* Make sure that the "shadow" CEL value in the PDA is currently
	 * in affect, if this exception is due to an interrupt.
	 */
	dmfc0	k0, C0_CAUSE
	andi	k0,CAUSE_EXCMASK
	bne	k0,zero,1f
	lbu	k1,VPDA_CELSHADOW(zero)	# BDSLOT
	dmfc0	k0,C0_CEL
	sd	k1,(k0)		# Store correct CEL value
	ld	k0,(k0)		# Wait for CAUSE register to be updated
	sb	k1,VPDA_CELHW(zero)
	dmfc0	k0,C0_SR
	/* Need to see if the SR_IE bit is still set.  The bit may have just
	 * been cleared in the C0_SR but has not yet taken affect.  This is
	 * critical with the addition of kernel preemption since on interrupt
	 * exit we perform a preemption check based upon isspl0() and we may
	 * miss an interrupt thread preemption.
	 */
	andi	k1,k0,SR_IE
	beq	k1,zero,2f
	nop
	
	LI	k1,SR_IMASK
	and	k0,k1
	LI	k1,CAUSE_BE|CAUSE_VCI|CAUSE_FPI
	or	k1,k0		# k1 == enabled interrupt mask
	dmfc0	k0, C0_CAUSE
	and	k0,k1
	bne	k0,zero,1f
	nop
2:	eret
1:
#endif
	PTR_L	k0, VPDA_EXCNORM
	j	k0
	nop			# BDSLOT
	END(__j_exceptnormv)

/**************************************************************************
 * This page of exception vectors will be used by those CPUs which
 * use the overlay segment utlbmiss handler.  We directly jump here
 * avoiding the indirection of trap_table_vector.
 *************************************************************************/

/**************************************************************************
 * utlbmiss2 -- offset 0x000 within another page (use .align 12)
 *************************************************************************/
	/* Align utlbmiss2 to a different page so it's address can
	 * also be used directly in TrapBase.
	 */
	.align 12
EXPORT(trap_table2)

NESTED(utlbmiss2, 0, k1)
/*
 * Utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 * NOTE: This routine should be same as utlbmiss except
 *	 after loading PTE we check for shared sentinel
 *	 and goto utlbmiss_sharedseg if we find it.
 */
	/* WARNING: Don't allow the two "dmfc0" to execute in the same
	 * cycle.  Currently the "dsra" on the result of the first load
	 * forces the subsequent "dmfc0" (as well as the "dsra") into
	 * the next superscaler cycle.
	 * KPTEBASE is constant, but is loaded from C0_KPTEBASE since generating
	 * the constant takes 3 instructions, one of them a shift.  This
	 * is faster.
 	 */
	TRAPLOG(6)
	MFC0_BADVADDR_NOHAZ(k0)
	dsra	k0,PNUMSHFT		# Convert to VPN - FORCE NEXT CYCLE
	dmfc0	k1,C0_KPTEBASE		# Load KPTEBASE
	dsll	k0,2			# Convert VPN for 4 byte pte
	dadd	k0,k1			# address is in KV0 space
	PTE_L	k0,0(k0)		# load 4 byte pte
	li	k1,SHRD_SENTINEL	# LDSLOT
	nada				# alignment op causes no penalty here
	beq	k0,k1,utlbmiss_sharedseg

	TLBLO_FIX_HWBITS(k0)		# move pfn bits to correct position
	dmtc0	k0,C0_TLBLO
	TLB_WRITE_AND_ERET

	.align	4
utlbmiss_sharedseg :

	# We are going to do first mappings from the shared pmap.
	# This just means we need to adjust base adddress of KPTEBASE
	# before loading the pte (other than different base it's same
 	# routine as utlbmiss).

	MFC0_BADVADDR_NOHAZ(k0)
	dsra	k0,PNUMSHFT		# get VPN
	dsll	k0,2			# Convert VPN for 4 byte pte
	LI	k1,KPTEBASE-KPTE_USIZE	# ASSUMES low 16 bits are zero
	dadd	k0,k1			# address is in KV0 space
	PTE_L	k0,(KPTEBASE-KPTE_USIZE)&0xFFFF(k0)	# load 4 byte pte
	TLBLO_FIX_HWBITS(k0)		# move pfn bits to correct position
	dmtc0	k0,C0_TLBLO
	TLB_WRITE_AND_ERET
EXPORT(eutlbmiss2)
	END(utlbmiss2)

/**************************************************************************
 * ktlbmiss_private -- offset 0x400 within page (use .align 10)
 *************************************************************************/

	.align	10

LEAF(ktlbmiss_private_jump2)
	j	ktlbmiss_private
	nop
	END(ktlbmiss_private_jump2)

/**************************************************************************
 * ktlbmiss -- offset 0x800 within page (use .align 10)
 *************************************************************************/

	.align	10

LEAF(ktlbmiss_jump2)
	j	ktlbmiss
	nop
	END(ktlbmiss_jump2)

/**************************************************************************
 * exception_jump -- offset 0xc00 with page (use .align 10)
 *************************************************************************/
	.align 10
LEAF(__j_exceptnorm2)
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_GENEX, uli_tstamps)
	.set	noreorder
#endif
	TRAPLOG(7)
#if EVEREST
	/* Make sure that the "shadow" CEL value in the PDA is currently
	 * in affect, if this exception is due to an interrupt.
	 */
	dmfc0	k0, C0_CAUSE
	andi	k0,CAUSE_EXCMASK
	bne	k0,zero,1f
	lbu	k1,VPDA_CELSHADOW(zero)	# BDSLOT
	dmfc0	k0,C0_CEL
	sd	k1,(k0)		# Store correct CEL value
	ld	k0,(k0)		# Wait for CAUSE register to be updated
	sb	k1,VPDA_CELHW(zero)
	dmfc0	k0,C0_SR
	/* Need to see if the SR_IE bit is still set.  The bit may have just
	 * been cleared in the C0_SR but has not yet taken affect.  This is
	 * critical with the addition of kernel preemption since on interrupt
	 * exit we perform a preemption check based upon isspl0() and we may
	 * miss an interrupt thread preemption.
	 */
	andi	k1,k0,SR_IE
	beq	k1,zero,2f
	nop
	
	LI	k1,SR_IMASK
	and	k0,k1
	LI	k1,CAUSE_BE|CAUSE_VCI|CAUSE_FPI
	or	k1,k0		# k1 == enabled interrupt mask
	dmfc0	k0, C0_CAUSE
	and	k0,k1
	bne	k0,zero,1f
	nop
2:	eret
1:
#endif
	PTR_L	k0, VPDA_EXCNORM
	j	k0
	nop			# BDSLOT
	END(__j_exceptnorm2)

/*
 *	Counting utlbmiss handler
 *
 *	NOTE: For now, we'll increment the count and just jump to the
 *	"standard" utlbmiss.  Eventually we'll in-line it, but at
 *	least it should work for now.
 */
NESTED(utlbmiss1, 0, k1)
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss
	sw	k1,VPDA_UTLBMISSES(zero)
EXPORT(eutlbmiss1)
	END(utlbmiss1)


/*
 * Counting utlbmiss handler for processes with overlay segments.
 * If a page table entry has SHRD_SENTINEL value, go to
 * (shared) page tables mapped just _before_ KPTEBASE.
 *
 */
NESTED(utlbmiss3, 0, k1)
	lw	k1,VPDA_UTLBMISSES(zero)
	addi	k1,1
	j	utlbmiss2
	sw	k1,VPDA_UTLBMISSES(zero)
EXPORT(eutlbmiss3)
	END(utlbmiss3)
#endif	/* TFP */

#if R4000 || R10000
/*
 *
 * On MP systems, if any process in the system currently requires a
 * special utlbmiss handler, the utlbmiss_prolog_patch will be applied
 * to the first instructions of the utlbmiss prolog.  This changes a
 * branch instruction (which jumps to the standard utlbmiss handler)
 * into a load of the address of the desired utlbmiss handler from the
 * cpu's pda.
 */

NESTED(utlbmiss_prolog_patch, 0, k1)
	PTR_L	k1,VPDA_UTLBMISSHNDLR   # load utlbmiss handler address
	END(utlbmiss_prolog_patch)


/*
 * this gets used if a utlbmiss handler is too large ...
 */
NESTED(utlbmissj, 0, k1)
	.set	noat		# must be set so li doesn't use at
#if (EVEREST || SN0) && DEBUG
	NOP_0_4
	PTR_L	k1,VPDA_UTLBMISSHNDLR
	DMFC0(k0,CONTEXT)	# BDSLOT
	j	k1
	CTXT_TO_PTE_ALIGN(k0)	# BDSLOT Convert for 4 byte ptes
#else
	nop
	nop
	nop
	nop
	j	utlbmiss
	nop			# BDSLOT
	nop
	nop
#endif
	.set	at
EXPORT(eutlbmissj)
END(utlbmissj)
#endif	/* R4000 || R10000 */

#if TFP
/**************************************************************************
 * Exit this module aligned on a page boundary so that tfp_flush_icache
 * gets properly aligned IFF it's the next module loaded (old COFF "ld"
 * requires this).
 *************************************************************************/
	.align	12
#endif	/* TFP */
	.set	reorder
	.set	at
