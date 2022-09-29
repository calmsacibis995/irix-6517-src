/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.22 $"

#include "ml/ml.h"
#include <sys/mapped_kernel.h>

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

#if _TLB_LOOP_LIMIT
	.data
EXPORT(tlb_loop_cntr)
	.word 	1
	.text
#endif /* _TLB_LOOP_LIMIT */

/* R10000_SPECULATION_WAR: $sp is not valid yet.  Whole file must be
 * noreorder as we only protect stores we know are ok, so we will get
 * warnings when things change.
 */
VECTOR(kmiss, M_EXCEPT)
	/*
	 * Inline implementation of a fast kernel tlbmiss.
	 * We can use k0 and AT (since we've already saved it).
	 * We can't use k1 until we KNOW its a valid kernel fault.
	 * ASSUMPTION: these ptes are always valid, therefore if we
	 * get a fault it's because NO entry exists in the tlb - not
	 * just an invalid one.
	 */
EXL_EXPORT(locore_exl_19)	
	.set 	noreorder
	.set	noat
	/*
	 * First look if utlbmiss and we're a 'big' proc
	 */
#undef SEGDEBUG
#ifdef SEGDEBUG
	.data
EXPORT(seg_fault)
	PTR_WORD	1
EXPORT(seg_tlbhi)
	PTR_WORD	1
EXPORT(seg_segaddr)
	PTR_WORD	1
EXPORT(seg_basetable)
	PTR_WORD	1

#if TFP
EXPORT(seg_tlblo)
	PTESZ_WORD	1
#endif
#if R4000 || R10000
EXPORT(seg_tlblo0)
	PTESZ_WORD	1
EXPORT(seg_tlblo1)
	PTESZ_WORD	1
#endif	/* R4000 */
	.text
#endif
#if R4000 || R10000
	.set	noat
	# check if in USER KPTESEG
	DMFC0(k0, C0_BADVADDR)		# get faulting virtual address
	LI	AT, KPTE_SHDUBASE	# LDSLOT
	# The R4000 uses the utlb miss vector for all address
	# spaces, whether in kernel or user mode. Thus, if the
	# address is not in KPTE_UBASE or above, this must be a
	# tlb invalid fault.

	# Note: A tlb invalid fault due to a missing half of a tlb pair
	#       will end up here and take the following "longway" branch
	#	since the BADVADDR will be in the normal K2 range.  We
	#	COULD check for this case and handle it here rather than
	#	exiting up to the generic tlbmiss handler.

	sltu	AT, k0, AT
	bne	AT, zero, jmp_to_longway
	nop				# BDSLOT
	LI	AT, KPTEBASE
	PTR_SUBU k0, AT
	LI	AT, KPTE_USIZE
	bltz	k0, kmiss_overlay
	nop				# BDSLOT
	sltu	AT, k0, AT
	beq	AT, zero, kmissnxt
	nop				# was  NOP_0_1

#ifdef SEGDEBUG
	LA	AT, seg_fault
	PTR_S	k0, 0(AT)
#endif	/* SEGDEBUG */
#ifndef FAST_LOCORE_TFAULT
	/* NOTE: This condition can not occur on 64-bit kernels (actually
	 * 16K pagesize kernels) since the u-area/kstack is always mapped
	 * in the second half of the pda page tlb entry.
	 * Since we intend to run all 64-bit kernels in FAST_LOCORE_TFAULT
	 * mode, we remove it from those kernels.
	 */
	# The thought here is that if we faulted in kernel mode on an
	# access to user space, it is possible that we faulted on the
	# page table that was in the upage or kernel stack slots. If
	# we drop a page table mapping into the random area of the tlb,
	# then on return to user mode replace the page table mapping
	# in the upage or kernel stack slot, then we have created a
	# duplicate entry in the tlb, of which the R4000 is very intolorant

	MFC0(AT,C0_SR)
	nop
	and	AT,SR_KSU_MSK
	beq	AT,zero,jmp_to_longway	# If in kernel mode, goto longway
#endif /* !FAST_LOCORE_TFAULT */
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#ifndef PROBE_WAR
#ifdef TRITON
	/* On R5000 if the instruction before a PROBE stalls, the tlb
	 * (incorrectly) gets read for the PROBE throughout the stall
	 * and will end up using the address created for the stalling
	 * instruction. -wsm11/8/96.
	 */
	nop
#else
        /* Do probe early so we can just load C0_INX later.  Doesn't cost
         * anything since we're doing this in a LDSLOT anyway.
         */
#endif	/* TRITON */
        c0      C0_PROBE                # LDSLOT - probe for address
#endif	/* !PROBE_WAR */
	beq	AT,zero,jmp_to_longway	# No uthread - goto longway
	nop				# BDSLOT
	lh	k1, UTAS_SEGFLAGS(AT)
	# k0 has pde offset - convert to vpn
					# LDSLOT
	beq	k1, zero, tfault_longway	# do nothing if no segment table

#ifdef _MIPS3_ADDRSPACE
	andi	k1, PSEG_TRILEVEL
	beq	k1, zero, pseg_segmiss	# go handle non-trilevel
	nop				# BDSLOT
	sreg	t0, VPDA_T0SAVE(zero)	# another reg to use
	# tri-level (PSEG_TRILEVEL) segment table code starts here.
#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#endif
	PTR_SRL	k1,k0,CPSSHIFT+BASETABSHIFT+PDESHFT	# Offset into base table
	PTR_L	t0,UTAS_SEGTBL(AT)	# pointer to base table.
	PTR_SLL	k1,PCOM_TSHIFT		# LDSLOT scale for pointer deref.
	PTR_ADDU t0,k1			# ptr into basetab for desired segment

#ifdef SEGDEBUG
	LA	AT,seg_basetable
	PTR_S	t0,0(AT)
#endif
	PTR_L	t0,0(t0)		# ptr to segment table.
	beql	t0,zero,kmiss_seg2_common	# No segment, go check other map (if present)
	lreg	t0, VPDA_T0SAVE(zero)	# BDSLOT (cancel if no br)

	PTR_SRL	k1,k0,CPSSHIFT+PDESHFT	# BD find faulting vaddr's segment nbr
	li	AT,SEGTABMASK
	and	k1,AT			# trim to fit within a segment
	PTR_SLL	k1,PTR_SCALESHIFT	# scale for pointer deref.
	PTR_ADDU k0,t0,k1		# pointer into segment table
	b	dosegmiss
	lreg	t0, VPDA_T0SAVE(zero)	# restore t0

pseg_segmiss:
	/* At this point we have a 32-bit process.  Need to make sure
	 * that the BADVADDR is OK for 32-bit process, otherwise we may
 	 * index off the end of the (single) segment table.
	 */
	PTR_SRL	k1,k0,CPSSHIFT+BASETABSHIFT+PDESHFT	# Offset into base table
	bne	k1, zero, jmp_to_longway	# bad badvaddr problem
#endif	/* _MIPS3_ADDRSPACE */

#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# BDSLOT vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)# BDSLOT load current uthread
#endif
	# compute segment offset - segment table is made of uints
	PTR_SRL	k1, k0, CPSSHIFT+PDESHFT	# LDSLOT (SEGDEBUG) or BDSLOT
	PTR_L	k0, UTAS_SEGTBL(AT)
	PTR_SLL	k1, PTR_SCALESHIFT	# LDSLOT turn into pointer index
	PTR_ADDU k0, k1

dosegmiss:
#ifdef SEGDEBUG
	LA	k1, seg_segaddr
	PTR_S	k0, 0(k1)
#endif
	# At this point we need:
	#    k0 => page table pointer within segtbl
	#    k1 = offset into segtbl of page table pointer
	#
	# load page table addr

	PTR_L	k0, 0(k0)		# load page table pointer from seg tbl.
	beql	k0, zero, kmiss_noprimary
	nop				# BDSLOT (cancel if no br)

kmiss_ptbl_dropin:

	.set	noat
/* At this point we are committed to dropping in the segment table
 * mapping.
 * At this point we have:
 *    k0 = page table pointer
 * ALSO: if !PROBE_WAR, then a probe should have already been executed.
 */

#ifdef PROBE_WAR
	# We can't execute probes from the instruction cache
	# on 2.2 (possibly 3.0?) chips
	LA	k1,1f
	LI	AT, K1BASE
	or	k1,AT
	LA	AT,2f
	j	k1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	AT			# PROBE in delay (uncached)
	c0	C0_PROBE		# probe for address
	NOP_0_4				# not executed, req. for inst. alignment
2:
	NOP_0_4
#endif
        mfc0    k1, C0_INX
#ifdef R4000
	LI	AT, K2BASE		# LDSLOT
	PTR_SUBU AT, k0, AT		# BDSLOT, AT = page table addr - K2BASE
        bgez    k1, 1f                  # branch if "hit" in tlb
	nop
#else
        bgez    k1, 1f                  # branch if "hit" in tlb
	nop
#endif /* R4000 */

        # "miss" in tlb, so zero both tlblo entries.
        # Will fill one of them in later.

        DMTC0(zero, C0_TLBLO_0)        # zero in case "miss"
        b       5f
        DMTC0(zero, C0_TLBLO_1)        # BDSLOT - zero in case "miss"

        # "hit" in tlb (i.e. other half of mapping already present)
        # Need to save TLBHI (actually PID) in case we hit on global
        # with some other PID

1:
        DMFC0(k1, C0_TLBHI)
        c0      C0_READI                # read TLBLO_0 and TLBLO_1
        NOP_1_3
        DMTC0(k1, C0_TLBHI)            # restore TLBHI (PID)
5:

	# dump page table pointer into random tlb area
	# make into a tlblo
#ifdef R4000
	# AT is a K2seg address - it is the k2 address of the
	# page table needed to complete the faulted mapping.
	# Compute the offset into kptbl to determine the mapping
	# for the address.
	PTR_L	k1, kptbl
	PTR_SRL	AT, PNUMSHFT
	PTR_SLL	AT, PDESHFT
	DMFC0(k0, C0_BADVADDR)		# reload fault addr for even/odd test
	PTR_ADDU AT, k1			# AT is a ptr into kptbl for
					# the page table mapping.
	PTE_L	AT, 0(AT)		# AT has the page mapping
#else /* R4000 */
        LI      AT, K0BASE
        PTR_SUBU AT, k0, AT             # AT = page table addr - K0BASE
        PTR_SRL AT, 6
        or      AT,PG_G|PG_VR|PG_COHRNT_EXLWR
        DMFC0(k0, C0_BADVADDR)          # reload fault addr for even/odd test
#endif /* R4000 */

	# For the R4000, we have to figure out if the page table
	# mapping we are adding to the tlb is an even or odd page.
	# This is done by testing bit 'NBPP' of C0_BADVADDR. Currently,
	# this value is in k0 (somewhat modified).
	# We only dropin in a single mapping, but we preserve the other
	# member of the pte pair if we "hit" in the tlb probe.

	andi	k1, k0, NBPP
	TLBLO_FIX_HWBITS(AT)
	mfc0	k0, C0_INX		# finally test result of probe
	beq	k1, zero, epg		# even page
	li	k1,~TLBLO_G		# BDSLOT
	and	AT, k1
#ifdef SEGDEBUG
	LA	k1, seg_tlblo1
	PTE_S	AT, 0(k1)
#endif
	# odd page
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	b	2f
	DMTC0(AT, C0_TLBLO_1)		# BDSLOT
epg:
	# do even page.

	and	AT, k1
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(AT, C0_TLBLO_0)
2:
#ifdef SEGDEBUG
        DMFC0(k1, C0_TLBLO_0)
        LA      AT, seg_tlblo0
        PTE_S	k1, 0(AT)
        DMFC0(k1, C0_TLBLO_1)
        LA      AT, seg_tlblo1
        PTE_S	k1, 0(AT)
#endif
	bgez	k0, tfault_indexed	# if probe "hit", go do indexed dropin
	.set	noat
	lreg	AT, VPDA_ATSAVE(zero)	# BDSLOT -- restore AT
#if _TLB_LOOP_LIMIT
	# execute a variable number of instructions, to avoid a TLB entry
	# exchange loop
	LA	AT, tlb_loop_cntr
	lw	k0,(AT)
	addiu	k0,1
	andi	k0,0x7
	sw	k0,(AT)
	beqz	k0,10f
	srl	k0,1			# BDSLOT
	beqz	k0,10f
	srl	k0,1			# BDSLOT
	beqz	k0,10f
	nop				# BDSLOT
	nop
10:
#endif /* _TLB_LOOP_LIMIT */

#ifdef FAST_LOCORE_TFAULT
	/* At this point, we didn't "hit" in the TLB so we want
	 * to dropin a new entry.  Normally this means we're in
	 * "segment table mode" with all the wired entries
	 * "in-use".  In fast tfault mode, we use this code
	 * to perform dropins into the "wired" tlbs if the
	 * u_nexttlb pointer is within the wired range.  If it isn't
	 * then we're really in segment table mode and should perform
	 * a random 2nd level dropin.
	 */

#ifdef ULI
	/* Running in ULI mode, the u-area is not mapped, so we immediately
	 * perform a random dropin.
	 */
	PTR_L	k0, VPDA_CURULI(zero)
	bne	k0, zero, tfault_random
	nop				# BDSLOT
#endif
	PTR_L	AT, VPDA_CURUTHREAD(zero)
	PTR_L	AT, UT_EXCEPTION(AT)
	lb	k0, U_NEXTTLB(AT)
	addi	k0,-NWIREDENTRIES+TLBWIREDBASE
	bgez	k0,tfault_random	# make sure index OK
	addi	k0,NWIREDENTRIES	# BDSLOT - get correct TLB index

	mtc0	k0,C0_INX
	addi	k1,k0,-TLBWIREDBASE+1
	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 will serialize execution
	sb	k1,U_NEXTTLB(AT)	# update u_nexttlb
	AUTO_CACHE_BARRIERS_ENABLE

	lreg	AT, VPDA_ATSAVE(zero)	# Restore AT
	b	tfault_indexed
	nop				# BDSLOT

tfault_random:
#endif /* FAST_LOCORE_TFAULT */
	lreg	AT, VPDA_ATSAVE(zero)	# Restore AT

	c0	C0_WRITER
	# nop's required between a tlb WRITE and a tlb-mapped lw
	NOP_0_4
	ERET_PATCH(locore_eret_0)	# patchable eret

	/* Here's where we drop in the second level tlb if we
	 * actually had a "hit" in the tlb.  This occurs only
	 * if we've previously done a dropin for the other half of
	 * the tlb-entry pair.
	 * Note that normally we would only "hit" in the random
	 * area of the TLB, but if the new FAST_TFAULT handler
	 * is being used we may hit in the "wired" tlb. In the
	 * latter case we actually want to update the table of
	 * entries in the u-page which contains copies of the
	 * "wired" tlbs.
	 * NOTE: k0 == index in tlb of "hit" entry
	 */
	
tfault_indexed:	

	c0	C0_WRITEI

#ifdef FAST_LOCORE_TFAULT
	/* If index is outside the "wired" tlbs, then we don't have to
	 * update the u-page wired table.
	 */
	addi	k0, -NWIREDENTRIES
	bgez	k0, 1f
	addi	k0, NWIREDENTRIES-TLBWIREDBASE	# BDSLOT

	/* At this point, k0 == index into u-page wired table
	 * and we had a "hit" in the wired area.
	 * NOTE" The "bgez" above has several extra cycles of
	 * delay, so it's safe to just jump to the ERET.
	 */
#if	PTE_64BIT
	PTR_SLL	k0,4			# tlb entry pair is 16 bytes
#else
	PTR_SLL	k0,3			# tlb entry pair is 8 bytes
#endif
	DMFC0(k1,C0_TLBLO_0)
	TLBLO_UNFIX_HWBITS(k1)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
	PTR_L	AT, UT_EXCEPTION(AT)
	PTR_ADDU AT, k0
	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 will serialize execution
	PTE_S	k1, U_UBPTBL(AT)	# save TLBLO in exception struct
	AUTO_CACHE_BARRIERS_ENABLE

	DMFC0(k1,C0_TLBLO_1)
	TLBLO_UNFIX_HWBITS(k1)
	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 will serialize execution
	PTE_S	k1, U_UBPTBL+NBPDE(AT)	# save TLBLO_1 in exception struct
	AUTO_CACHE_BARRIERS_ENABLE

	DMFC0(k1,C0_TLBHI)
	ori	k1,TLBHI_PIDMASK
	xori	k1,TLBHI_PIDMASK	# need to clear PID
	PTR_L	AT, VPDA_CURUTHREAD(zero)
	PTR_L	AT, UT_EXCEPTION(AT)
#if	PTE_64BIT
	/*
	 * k0 was adjusted for pair of tlb entries above.
	 * In the case of 32 bit PTE's, that means 8 bytes which
	 * matches the pointer size for tlbhi_tbl so we can use
	 * the same index. In the case of 64 bit PTEs, we need
	 * to readjust k0 to index into tlbhi_tbl.
	 */
	PTR_SRL	k0,1
#endif
	PTR_ADDU AT, k0
	AUTO_CACHE_BARRIERS_DISABLE	# DMFC0 will serialize execution
	PTR_S	k1, U_TLBHI_TBL(AT)	# save TLBHI in exception struct
	AUTO_CACHE_BARRIERS_ENABLE

	lreg	AT, VPDA_ATSAVE(zero)	# Restore AT
1:		
#else
	# nop's required between a tlb WRITE and a tlb-mapped lw
	NOP_0_4
#endif /* !FAST_LOCORE_TFAULT */		
	ERET_PATCH(locore_eret_1)	# patchable eret

kmiss_overlay:
	/* BADVADDR is between KPTE_SHDUBASE and KPTEBASE so it must
	 * be an address in the "overlay" region generated by utlbmiss after
	 * we've loaded the SHRD_SENTINEL.  Note that this process has
	 * both a shared address space and a private address space AND
	 * we have hit a pte in an overlapped segment.
	 */
	# normalize k0 for KPTE_USIZE overlapped shaddr space shift that
	# is expressed in the difference between KPTEBASE and KPTE_UBASE

	PTR_ADDU k0, AT			# SHRD space only
#ifdef SEGDEBUG
	LA	AT, seg_fault
	PTR_S	k0, 0(AT)
#endif	/* SEGDEBUG */

#ifndef FAST_LOCORE_TFAULT
	/* NOTE: This condition can not occur on 64-bit kernels (actually
	 * 16K pagesize kernels) since the u-area/kstack is always mapped
	 * in the second half of the pda page tlb entry.
	 * Since we intend to run all 64-bit kernels in FAST_LOCORE_TFAULT
	 * mode, we remove it from those kernels.
	 */
	# The thought here is that if we faulted in kernel mode on an
	# access to user space, it is possible that we faulted on the
	# page table that was in the upage or kernel stack slots. If
	# we drop a page table mapping into the random area of the tlb,
	# then on return to user mode replace the page table mapping
	# in the upage or kernel stack slot, then we have created a
	# duplicate entry in the tlb, of which the R4000 is very intolorant

	MFC0(AT,C0_SR)
	nop
	and	AT,SR_KSU_MSK
	beq	AT,zero,jmp_to_longway	# If in kernel mode, goto longway
#endif /* !FAST_LOCORE_TFAULT */	

#ifndef PROBE_WAR
#ifdef TRITON
	/* On R5000 if the instruction before a PROBE stalls, the tlb
	 * (incorrectly) gets read for the PROBE throughout the stall
	 * and will end up using the address created for the stalling
	 * instruction. -wsm11/8/96.
	 */
	nop
#else
        /* Do probe early so we can just load C0_INX later.  Doesn't cost
         * anything since we're doing this in a LDSLOT anyway.
         */
#endif	/* TRITON */
        c0      C0_PROBE                # LDSLOT - probe for address
#endif	/* !PROBE_WAR */
	/* Common code for kmiss_overlay (an outright miss on a shared sentinel)
	 * and kmiss_noprimary (which didn't find entry in primary map and
	 * must check the secondary map).  Making code common costs us a couple
	 * of instructions in the shared sentinel case since we must determine
	 * if the secondary map exists (it must exist in the shared sentinel
	 * case).  But code should be easier to maintain.
	 *	k0 = (2nd level) fault address - KPTEBASE
	 */
kmiss_seg2_common:
	PTR_L	AT, VPDA_CURUTHREAD(zero)
	lh	k1, UTAS_SEGFLAGS(AT)
	# k0 has pde offset - convert to vpn
					# LDSLOT
	beq	k1, zero, tfault_longway	# do nothing if no segment table

#ifdef _MIPS3_ADDRSPACE
	andi	k1, PSEG_TRILEVEL
	beq	k1, zero, pseg_segmiss2	# go handle non-trilevel
	nop				# BDSLOT
	sreg	t0, VPDA_T0SAVE(zero)	# another reg to use
	# tri-level (PSEG_TRILEVEL) segment table code starts here.
#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#endif
	PTR_SRL	k1,k0,CPSSHIFT+BASETABSHIFT+PDESHFT	# Offset into base table
	PTR_L	t0,UTAS_SHRDSEGTBL(AT)	# pointer to base table.
	PTR_SLL	k1,PCOM_TSHIFT		# LDSLOT scale for pointer deref.
	beql	t0, zero, tfault_longway	# bail if no secondary segments
	lreg	t0, VPDA_T0SAVE(zero)	# BDSLOT (cancel if no br)
	
	PTR_ADDU t0,k1			# ptr into basetab for desired segment

#ifdef SEGDEBUG
	LA	AT,seg_basetable
	PTR_S	t0,0(AT)
#endif

	PTR_L	t0,0(t0)		# ptr to segment table.
	beql	t0,zero,tfault_longway	# No segment, bail out to tfault.
	lreg	t0, VPDA_T0SAVE(zero)	# BDSLOT (cancel if no br)

	PTR_SRL	k1,k0,CPSSHIFT+PDESHFT	# BD find faulting vaddr's segment nbr
	li	AT,SEGTABMASK
	and	k1,AT			# trim to fit within a segment
	PTR_SLL	k1,PTR_SCALESHIFT	# scale for pointer deref.
	PTR_ADDU k0, t0,k1		# pointer into segment table
	b	dosegmiss2
	lreg	t0, VPDA_T0SAVE(zero)	# restore t0

pseg_segmiss2:
	/* At this point we have a 32-bit process.  Need to make sure
	 * that the BADVADDR is OK for 32-bit process, otherwise we may
 	 * index off the end of the (single) segment table.
	 * NOTE: Originally this check was only needed under MP_R4000_BADVA_WAR
	 * but now that we run 32-bit programs as -mips3/-mips4 we must
	 * check that the user didn't access a bogus 32-bit address.
	 */
	PTR_SRL	k1,k0,CPSSHIFT+BASETABSHIFT+PDESHFT	# Offset into base table
	bne	k1, zero, jmp_to_longway	# bad badvaddr problem
#endif	/* _MIPS3_ADDRSPACE */

#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# BDSLOT vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#endif
	# compute segment offset - segment table is made of uints
	PTR_SRL	k1, k0, CPSSHIFT+PDESHFT	# LDSLOT (SEGDEBUG) or BDSLOT
	PTR_L	k0, UTAS_SHRDSEGTBL(AT)
	PTR_SLL	k1, PTR_SCALESHIFT	# LDSLOT turn into pointer index
	beq	k0, zero, tfault_longway	# bail if no secondary table
	PTR_ADDU k0, k1			# BDSLOT

dosegmiss2:
#ifdef SEGDEBUG
	LA	k1, seg_segaddr
	PTR_S	k0, 0(k1)
#endif
	# At this point we need:
	#    k0 => page table pointer within segtbl
	#    k1 = offset into segtbl of page table pointer
	#    AT = VPDA_CURPROC
	#
	# load page table addr

	PTR_L	k0, 0(k0)		# load page table pointer from seg tbl.
	nop				# LDSLOT
	bne	k0, zero, kmiss_ptbl_dropin
	nop				# BDSLOT (cancel if no br)
	b	tfault_longway		# no page table pointer
	nop				# BDSLOT

	/* BADVADDR was in primary segment table between KPTEBASE and
	 * KPTEBASE+KPTE_USIZE.  But when we loaded the pointer from UTAS_SEGTBL
	 * the pointer was zero.  So we need to check if there is a
	 * secondary pointer in P_SHRDSEGTBL which would mean we have a
	 * process with both private mappings and shared mappings.  In this
	 * case we check for a segment pointer in the shared map since
	 * the private map does not have a segment.
	 */
kmiss_noprimary:
	DMFC0(k0,C0_BADVADDR)		# Need to reload fault address
	LI	k1, KPTEBASE		# and get KPTEBASE offset into k0
	b	kmiss_seg2_common
	PTR_SUBU	k0, k1		# BDSLOT
	
tfault_longway:		
#ifdef FAST_LOCORE_TFAULT
	/* Normally we would go to tfault, which would setup the second level
	 * map.  Then return to user and dropin an invalid first level map
	 * which takes us back through tlb invalid path.  Instead we take
	 * a shortcut - if we can't find a valid second level tlb, just
	 * dropin an invalid first level tlb without incurring the overhead
	 * of a full tfault.  This avoids tfault and instead causes vfault
	 * to handle everything.
	 */

	DMFC0(k0, C0_BADVADDR)
	LI	k1, KPTEBASE
	PTR_SUBU k0, k1
	bgez	k0, 1f
	LI	k1, KPTE_USIZE

	PTR_ADDU k0, k1			# SHRD space tfault

1:		
	DMFC0(k1, C0_TLBHI)
	PTR_SLL	k0, PNUMSHFT-PDESHFT
#ifdef TFP			/* R4K/R10K C0_BADVADDR is read-only */
	DMTC0(k0, C0_BADVADDR)
#endif
	andi	k1, TLBHI_PIDMASK
	or	k1, k0
	DMTC0(k1, C0_TLBHI)

	/* May need to probe if PROBE_WAR defined */

	lreg	AT, VPDA_ATSAVE(zero)
	nop
	nop
	c0	C0_PROBE		# Probe for first level address
	nop
	nop
	
	mfc0	k1, C0_INX
	nop
	bgez	k1, 2f
	andi	k0, NBPP		# BDSLOT - save even/odd bit

	/* MISS */
	
	DMTC0(zero, C0_TLBLO_0)
	DMTC0(zero, C0_TLBLO_1)
	nop
	c0	C0_WRITER
	NOP_0_4
	eret
	
	/* HIT */
2:
	/* Following code sequence executes exactly ONE of the "dmtc0"
	 * instructions to make an invalid first level tlb entry for
	 * the first level vaddr.  We then "eret" and take the
	 * vfault path and resolve the address (we know that the address
	 * has yet to be allocated).
	 */
	beql	k0, zero, 3f
	DMTC0(zero, C0_TLBLO_0)		# BDSLOT - invalid even entry

	DMTC0(zero, C0_TLBLO_1)		#  make invalid odd entry 

3:	
	nop
	c0	C0_WRITEI
	NOP_0_4
	eret
#else /* FAST_LOCORE_TFAULT */
	
	j	longway
	nop				# BDSLOT
#endif /* FAST_LOCORE_TFAULT */
	
#elif TFP
	# check if in USER KPTESEG
	MFC0_BADVADDR(k0)		# get faulting virtual address
	/* At this point, possible hi order 2-bit values for BADVADDR
	 *	00 = UV  (user virtual)
	 * 	01 = KV0 (kernel private) - 2nd level user tlb
	 *	10 = KP  (kernel physical) - impossible
	 *	11 = KV1 (kernel global) - K2 space
	 *
	 * NOTE: Normal K2 misses (within sysseg) are handled by
	 *	 ktlbmiss handler accessing kptbl.  We must have
	 *	 an invalid tlb (this shouldn't occur).
	 */
	bltz	k0, jmp_to_longway	# KV1 - fault on kernel K2 address
	dsll	k1, k0, 1
	bgez	k1, jmp_to_longway	# UV - user tlb invalid
	LI	k1, KPTEBASE		# BDSLOT -- macro instruction ok!
	/*
	 * At this point, we've faulted on a KV0 address.
	 * Currently the only thing in this map is the user second level
	 * tlbs.
	 */
	PTR_SUBU k0, k1
	bltz	k0, kmiss_overlay
	LI	k1, KPTE_USIZE		# BDSLOT
	PTR_L	AT, VPDA_CURUTHREAD(zero)
	beq	AT,zero,jmp_to_longway	# No uthread - goto longway
	nop

	/* The following check will only allow 40 bit KU address range in
	 * order to be consistent with R4000.  This is important to keep
	 * us from indexing out of the "base table" on TRILEVEL processes.
	 */
	sltu	k1, k0, k1		# check for valid 2nd level KU addr
	beq	k1, zero, tfault_longway

#ifdef SEGDEBUG
	LA	k1, seg_fault		# BDSLOT (if SEGDEBUG)
	PTR_S	k0, 0(k1)
#endif	/* SEGDEBUG */

	lh	k1, UTAS_SEGFLAGS(AT)	# BDSLOT
	 				# LDSLOT
	beq	k1, zero, tfault_longway # do nothing if no segment table

#ifdef _MIPS3_ADDRSPACE
	andi	k1, PSEG_TRILEVEL	# BDSLOT what type of seg table.
	beq	k1, zero, pseg_segmiss	# leave if not tri-level map

	# tri-level (PSEG_TRILEVEL) segment table code starts here.
#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#endif
	PTR_SRL	k1,k0,CPSSHIFT+BASETABSHIFT+PDESHFT	# Offset into base table

	/* Now load from base table since index is valid. */

	PTR_L	AT,UTAS_SEGTBL(AT)	# pointer to base table.
	PTR_SLL	k1,PCOM_TSHIFT		# LDSLOT scale for pointer deref.
	PTR_ADDU AT,k1			# ptr into basetab for desired segment

#ifdef SEGDEBUG
	LA	k1,seg_basetable
	PTR_S	AT,0(k1)
#endif

	PTR_L	AT,0(AT)		# ptr to segment table.
	PTR_SRL	k1,k0,CPSSHIFT+PDESHFT	# LDSLOT find faulting vaddr's segment nbr
	beq	AT,zero,kmiss_seg2_common # No segment, bail out to tfault.

	andi	k1,SEGTABMASK		# BDSLOT trim to fit within a segment
	PTR_SLL	k1,PTR_SCALESHIFT	# scale for pointer deref.
	b	dosegmiss
	PTR_ADDU AT,k1			# BDSLOT pointer into segment table

pseg_segmiss:
#endif	/* _MIPS3_ADDRSPACE */
	
#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# BDSLOT vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#endif

	# compute segment offset - segment table is made of uints
	PTR_SRL	k1, k0, CPSSHIFT+PDESHFT	# LDSLOT (SEGDEBUG) or BDSLOT
	PTR_L	AT, UTAS_SEGTBL(AT)
	PTR_SLL	k1, PTR_SCALESHIFT	# LDSLOT turn into pointer index
	PTR_ADDU AT, k1

	/* This code handles processes with a single segment table.
	 * Since TFP does NOT give an Address Error Exception when UX == 0
	 * and the user accesses and address >= 2^^31, we need to check
	 * that the address "fits" within one segment.
	 * We check that the "base table index" is zero, which is the
	 * only allowed value when you don't have a TRI_LEVEL map.
	 */
	PTR_SRL	k1,BASETABSHIFT+PTR_SCALESHIFT
	bne	k1,zero,tfault_longway	# go longway if address out of range

dosegmiss:
#ifdef SEGDEBUG
	LA	k1, seg_segaddr
	PTR_S	AT, 0(k1)
#endif
	# load page table addr

	PTR_L	k1, 0(AT)
	lreg	AT, VPDA_ATSAVE(zero)	# LDSLOT restore AT
	beq	k1, zero, kmiss_noprimary

kmiss_ptbl_dropin:

	.set	at
/* At this point we are committed to dropping in the segment table
 * mapping. 
 *	k1 == page table pointer loaded from segment table
 *	k0    scratch
 *	AT    has already been restored, now unusable
 */
	# dump page table pointer into random tlb area
	# make into a tlblo.  No "G" bit on TFP - it's a private
	# mapping because it's a KV0 address.
	# AT is a K0seg address - it is the k0 address of the
	# page table needed to complete the faulted mapping.

	PTR_SLL	k1, 2			# take off top 2 bits
	PTR_SRL	k1, MIN_PNUMSHFT+2	# take off bottom 12 bits
	PTR_SLL	k1, TLBLO_PFNSHIFT-TLBLO_HWBITSHIFT	# position it for tlblo
	ori	k1, PG_VR|PG_M|PG_COHRNT_EXLWR
	TLBLO_FIX_HWBITS(k1)		# shift pfn to correct position
#ifdef SEGDEBUG
	LA	k0, seg_tlblo
	PTE_S	k1, 0(k0)
#endif
	DMTC0(k1, C0_TLBLO)		# pte for new entry
	TLB_WRITER			# create new entry in TLB
	eret

kmiss_overlay:
	/* BADVADDR is between KPTE_SHDUBASE and KPTEBASE so it must
	 * be an address in the "overlay" region generated by utlbmiss after
	 * we've loaded the SHRD_SENTINEL.  Note that this process has
	 * both a shared address space and a private address space AND
	 * we have hit a pte in an overlapped segment.
	 */
	# normalize k0 for KPTE_USIZE overlapped shaddr space shift that
	# is expressed in the difference between KPTEBASE and KPTE_UBASE

	PTR_ADDU k0, k1			# SHRD space only
#ifdef SEGDEBUG
	LA	k1, seg_fault
	PTR_S	k0, 0(k1)
#endif	/* SEGDEBUG */


	/* BADVADDR was in primary segment table between KPTEBASE and
	 * KPTEBASE+KPTE_USIZE.  But when we loaded the pointer from UTAS_SEGTBL
	 * the pointer was zero.  So we need to check if there is a
	 * secondary pointer in P_SHRDSEGTBL which would mean we have a
	 * process with both private mappings and shared mappings.  In this
	 * case we check for a segment pointer in the shared map since
	 * the private map does not have a segment.
	 *	k0 =  fault address less KPTEBASE
	 */
kmiss_noprimary:

	/* Common code for kmiss_overlay (an outright miss on a shared sentinel)
	 * and kmiss_noprimary (which didn't find entry in primary map and
	 * must check the secondary map).  Making code common costs us a couple
	 * of instructions in the shared sentinel case since we must determine
	 * if the secondary map exists (it must exist in the shared sentinel
	 * case).  But code should be easier to maintain.
	 *	k0 = (2nd level) fault address - KPTEBASE
	 *	AT   Random
	 *	k1   Random
	 */
kmiss_seg2_common:
	.set	noat
	PTR_L	AT, VPDA_CURUTHREAD(zero)
	lh	k1, UTAS_SEGFLAGS(AT)
	beq	k1, zero, tfault_longway	# leave if no segment table (kmiss_overlay)
#ifdef _MIPS3_ADDRSPACE
	andi	k1, PSEG_TRILEVEL	# BDSLOT what type of seg table.
	beq	k1, zero, pseg_segmiss2	# leave if not tri-level map

	# tri-level (PSEG_TRILEVEL) segment table code starts here.
#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#endif
	PTR_SRL	k1,k0,CPSSHIFT+BASETABSHIFT+PDESHFT	# Offset into base table.
	PTR_L	AT,UTAS_SHRDSEGTBL(AT)		# pointer to base table.
	PTR_SLL	k1,PCOM_TSHIFT		# LDSLOT scale for pointer deref.
	beq	AT, zero, tfault_longway # bail if no secondary segment mapped
	PTR_ADDU AT,k1			# ptr into basetab for desired segment

#ifdef SEGDEBUG
	LA	k1,seg_basetable
	PTR_S	AT,0(k1)
#endif

	PTR_L	AT,0(AT)		# ptr to segment table.
	PTR_SRL	k1,k0,CPSSHIFT+PDESHFT	# LDSLOT find faulting vaddr's segment nbr
	beq	AT,zero,tfault_longway	# No segment, bail out to tfault.

	andi	k1,SEGTABMASK		# BDSLOT trim to fit within a segment
	PTR_SLL	k1,PTR_SCALESHIFT	# scale for pointer deref.
	b	dosegmiss2
	PTR_ADDU AT,k1			# BDSLOT pointer into segment table

pseg_segmiss2:
#endif	/* _MIPS3_ADDRSPACE */

#ifdef SEGDEBUG
	DMFC0(k1, C0_TLBHI)		# BDSLOT vpn and pid of new entry
	LA	AT, seg_tlbhi
	PTR_S	k1, 0(AT)
	PTR_L	AT, VPDA_CURUTHREAD(zero)
#endif
	# compute segment offset - segment table is made of uints
	PTR_SRL	k1, k0, CPSSHIFT+PDESHFT	# LDSLOT (SEGDEBUG) or BDSLOT
	PTR_L	AT, UTAS_SHRDSEGTBL(AT)
	PTR_SLL	k1, PTR_SCALESHIFT	# LDSLOT turn into pointer index
	beq	AT, zero, tfault_longway	# bail if no secondary segment mapped
	PTR_ADDU AT, k1

dosegmiss2:
#ifdef SEGDEBUG
	LA	k1, seg_segaddr
	PTR_S	AT, 0(k1)
#endif
	# load page table addr

	PTR_L	k1, 0(AT)
	bne	k1, zero, kmiss_ptbl_dropin
	lreg	AT, VPDA_ATSAVE(zero)	# BDSLOT restore AT
	b	tfault_longway			# no secondary segment either - tfault
	nop
	.set	at


	/* Normally we would go to tfault, which would setup the second level
	 * map.  Then return to user and dropin an invalid first level map
	 * which takes us back through tlb invalid path.  Instead we take
	 * a shortcut - if we can't find a valid second level tlb, just
	 * dropin an invalid first level tlb without incurring the overhead
	 * of a full tfault.  This avoids tfault and instead causes vfault
	 * to handle everything.
	 */
tfault_longway:
	lreg	AT,VPDA_ATSAVE(zero)	/* restore AT */
	PTR_SLL	k1, k0, PNUMSHFT-PDESHFT
	dmfc0	k0, C0_TLBHI
	NOP_SSNOP
	dmtc0	k1, C0_BADVADDR
	andi	k0, TLBHI_PIDMASK	/* need to preserve the pid */
	or	k1, k0
	dmtc0	k1, C0_TLBHI
	NOP_SSNOP
	DMTC0(zero, C0_TLBLO)		/* invalid tlb entry */
	NOP_SSNOP
	NOP_SSNOP
	dmfc0	k1, C0_TLBSET
	bne	k1, zero, 1f
	li	k1, 1
	/* Don't let write occur to set 0, bump to set 1.
	 * This preserves the wired entries in set 0.
	 */

	dmtc0	k1, C0_TLBSET
	NOP_SSNOP
	NOP_SSNOP
1:
	TLB_WRITER
	eret

#elif BEAST

#if defined (PSEUDO_BEAST)
	.data
errmsg:	.asciiz "UNHANDLED KMISS EXCEPTION"
	.text
	LA	a0, errmsg
	jal	printf
	 nop
	
	eret
	
#else /* PSEUDO_BEAST */

#error <BOMB! need to take care of beast kmiss here. >
	
#endif /* PSEUDO_BEAST */	

			
#else				/* !TFP && !R4000 && !R10000 && !BEAST */
<<bomb -- add cpu type here >>
#endif				/* !TFP && !R4000 && !R10000 && !BEAST */

#if !TFP
EXPORT(kmissnxt)
#if R4000 || R10000
	# R4000 fast kernel tlb miss handler:
	# The R4000 uses the utlbmiss handler for K2SEG references
	# if the EXL bit is low (and in kernel mode). This is
	# handled much like a double user level tlb miss. Since the
	# faulting address will be a K2SEG address, the BadVaddr
	# register will look like:
	# KPTEBASE + (K2BASE >> 9) + offset within K2SEG.
	# Check for a faulting address within this range.
	#
	# NOTE that if the R4000 ever takes a tlb miss when EXL is high,
	# we are basically out of luck, since EPC is not saved in that
	# case. Therefore, we would not have a valid return address.
	#
	# ASSUMES that kptbl is page-aligned.
	#
	DMFC0(k0, C0_BADVADDR)		# get faulting virtual address
	.set	noat
	LI	AT, KPTE_KBASE 
	PTR_SUBU k0, AT
	bltz	k0, jmp_to_longway
	nop				# BDSLOT
	lw	AT, syssegsz		# number of pages is SYSSEG
	addiu	AT, (MAPPED_KERN_SIZE >> BPCSHIFT)
	PTR_SRL	k0, PDESHFT		# LDSLOT convert to vpn
	sltu	AT, k0, AT
	beq	AT, zero, syssegmiss	# is not in K2PTEBASE..SYSSEGSIZE
	PTR_SLL	k0, PDESHFT		# BDSLOT convert vpn into pde index

	# it's an address in SYSSEG. Drop in a mapping for the
	# page table, and for the page.
	# Since the R4000 doesn't overwrite EPC on a 2nd level miss,
	# we can use k1.
	#

#if defined(EVEREST) || defined(SN0)
	/*
	 * update private.p_kvfault
	 * NOTE - must use same convention as bitmap routines.
	 * That means interpreting things as 32 bit quantities.
	 * For R4K and R10K must light both even/odd entries..
	 * *(private.p_kvfault + (vpn >> 5) << 2) |= 3 << (vpn & 0x1e);
	 * account for colormaps - must light both even and odd bits
	 * vpn -= sptsz
	 * *(p_clrkvflt[vpn&COLOR  ] + (vpn >> (5 + COLORSHFT)) << 2) |= 1 << ((vpn >> COLORSHFT) & 0x1f)
	 * *(p_clrkvflt[vpn&COLOR+1] + (vpn >> (5 + COLORSHFT)) << 2) |= 1 << ((vpn >> COLORSHFT) & 0x1f)
	 * k0 = vpn
	 */
#if _MIPS_SIM == _ABI64
	PTR_L	k1, VPDA_KVFAULT	# load base of kernel fault history
	PTR_SRL	k0, PDESHFT		# convert pte offset into vpn
	beq	k1, zero, 3f		# kvfault is not set(MP with 1 cpu)
	nop
	lw	k1, sptsz
#else
	PTR_L	k1, VPDA_KVFAULT	# load base of kernel fault history
	DMFC0(k0, C0_BADVADDR)		# get faulting virtual address
	beq	k1, zero, 3f		# kvfault is not set(MP with 1 cpu)
	LI	AT, K2BASE
	# convert BADVADDR to original faulting addr.
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
	PTR_SUBU k0, AT
	lw	k1, sptsz
	PTR_SRL	k0, PNUMSHFT		# convert to vpn
#endif
	/* need to bias the base address by size of mapped kernel */
	PTR_ADDIU k0,-(MAPPED_KERN_SIZE >> BPCSHIFT)
	sltu	AT, k0, k1
	beq	AT, zero, 2f
	nop
	PTR_L	k1, VPDA_KVFAULT	# load base of kernel fault history
	PTR_SRL	AT, k0, 5		# convert vpn to uint offset in history
	PTR_SLL AT, 2			# convert to byte offset
	PTR_ADDU k1, AT
	li	AT, 3
	andi	k0, 0x1e		# vpn & 0x1e, LDSLOT
	sll	AT, k0
	lw	k0, 0(k1)
	or	k0, AT			# set bit for even and odd page vpns
	b	222f			# now dropin first level tlb entries
	sw	k0, 0(k1)
2:
	subu	k0, k0, k1		# vpn - sptsz
	lw	k1, colormap_size
	sltu	AT, k0, k1
	bne	AT, zero, 22f
	nop
	move	a2, k0
	move	a3, k1
	PANIC("kmissnxt: vpn offset 0x%x > colormap_size 0x%x");
22:
	LI	k1, VPDA_CLRKVFLT	# load colorfault array
	andi	AT, k0, CACHECOLORMASK-1	# get even cache color
	PTR_SLL	AT, PTR_SCALESHIFT
	PTR_ADDU k1, AT			# index to appropriate colormap
	PTR_L	k1, 0(k1)		# load colormap
	PTR_SRL	AT, k0, 5+CACHECOLORSHIFT
	PTR_SLL AT, 2			# convert to byte offset
	PTR_ADDU k1, AT			# index into colormap
	li	AT, 1
	srl	k0, CACHECOLORSHIFT
	andi	k0, 0x1f			# word mask
	sll	AT, k0
	lw	k0, 0(k1)
	or	k0, AT
	sw	k0, 0(k1)

	DMFC0(k0, C0_BADVADDR)		# get faulting virtual address
	lw	k1, sptsz
#if _MIPS_SIM == _ABI64
	LI	AT, KPTE_KBASE
	PTR_SUBU k0, AT
	PTR_SRL	k0, PDESHFT		# convert pde index into vpn
#else
	LI	AT, K2BASE		# MFC0 SLOT
	# convert BADVADDR to original faulting addr.
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
	PTR_SUBU k0, AT
	PTR_SRL	k0, PNUMSHFT		# convert to vpn
#endif
	/* need to bias the base address by size of mapped kernel */
	PTR_ADDIU k0, -(MAPPED_KERN_SIZE >> BPCSHIFT)
	PTR_SUBU k0, k0, k1		# vpn - sptsz
	LI	k1, VPDA_CLRKVFLT	# load colorfault array
	andi	AT, k0, CACHECOLORMASK-1	# get even cache color
	addi	AT, 1			# now have odd cache color
	PTR_SLL	AT, PTR_SCALESHIFT
	PTR_ADDU k1, AT			# index to appropriate colormap
	PTR_L	k1, 0(k1)		# load colormap
	PTR_SRL	AT, k0, 5+CACHECOLORSHIFT
	PTR_SLL AT, 2			# convert to byte offset
	PTR_ADDU k1, AT			# index into colormap
	li	AT, 1
	PTR_SRL	k0, CACHECOLORSHIFT
	andi	k0, 0x1f		# word mask
	sll	AT, k0
	lw	k0, 0(k1)		# update colormap
	or	k0, AT
	sw	k0, 0(k1)
222:
	#
	# Dropin the first level entries, but not the 2nd.  Want to be able
	# to log all faults on kernel addresses
	#
	# Compute virtual address that caused utlbmiss
	# Validity checking was done above, so don't need that here
	#
	DMFC0(AT, C0_BADVADDR)		# get faulting address
#if _MIPS_SIM == _ABI64
	LI	k0,KPTE_KBASE
	PTR_SUBU k0, AT, k0
	PTR_SLL	AT, k0, (PNUMSHFT-PDESHFT)	# vaddr of 1st level addr
	LI	k1, K2BASE
	PTR_ADDU AT, k1			# vaddr with region bits
	PTR_L	k1, kptbl
#else	/* 32-bit kernel */
	LI	k0, K2BASE
	PTR_SLL	AT, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
	PTR_SUBU k0, AT, k0
	PTR_L	k1, kptbl		# base of kernel page table
	PTR_SRL	k0, (PNUMSHFT-PDESHFT)	# convert to vpn byte offset
#endif	/* 32-bit kernel */
	#
	# k0 is byte offset of pte in page table
	# k1 points at base of kptbl
	# AT is virtual page address that caused utlbmiss
	#
	PTR_ADDU k0, k1			# have index into kptbl
	PTE_L	k1, 0(k0)		# grab pte
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(k1, C0_TLBLO_0)		# drop pte in tlb

	PTE_L	k0, NBPDE(k0)		# grab pte
	DMFC0(k1, C0_TLBHI)		# LDSLOT save current TLBPID

	# load this into TLBLO register then check if valid

	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO_1)		# drop pte in tlb

	# construct TLBHI register and write new tlb entry
	# since this is a double tlb miss, the faulting address looks
	# like KPTEBASE | K2SEG_addr >> 9. So to compute the faulting
	# address of the 1st level miss, C0_BADVADDR << 9
	# Since G bit on, pid doesn't matter, so we don't bother to mask

	DMTC0(AT, C0_TLBHI)		# drop pte in tlb
	NOP_0_4
	c0	C0_WRITER
	NOP_0_4

	# all done - let's go!
	# Don't need AT anymore
	.set	noat
	lreg	AT, VPDA_ATSAVE(zero)	# BDSLOT restore AT
	.set	at
	DMTC0(k1, C0_TLBHI)		# LDSLOT restore TLBPID
	NOP_0_4
	eret
3:
	.set	noat
	/* recompute k0 */
	DMFC0(k0, C0_BADVADDR)		# get faulting virtual address
	LI	AT, KPTE_KBASE
	PTR_SUBU k0, AT
#endif	/* EVEREST || SN0 */

	# make k1 a pointer to nearest page in kptbl.
	# k1 will be used for tlblo.
	# ASSUMES that kptbl is page-aligned.

	PTR_L	AT, kptbl		# base of kernel page table
	li	k1, POFFMASK
	not	k1
	and	k1, k0			# retain upper bits of k0 in k1
	PTR_ADDU k1, AT			# pointer to nearest page in kptbl
	# decide if this is an even page, or an odd page.
	# make k1 a pointer to the even page of the even/odd pair.
	and	AT, k0, NBPP
	beq	AT, zero, 1f
	.set	at
	nop				# BDSLOT
	PTR_SUBU k1, NBPP		# make k1 point to the even page.
1:
#if (_MIPS_SZPTR == 64)
	# Since we used doubleword ops to set up k1, we must
	# continue with doubleword ops on k1
	dsll	k1, 24			# take off top 24 bits
	dsrl32	k1, MIN_PNUMSHFT-8	# take off bottom 12 bits
	dsll	k1, TLBLO_PFNSHIFT	# position it for tlblo_0
#else
	sll	k1, 2			# take off top 2 bits
	srl	k1, MIN_PNUMSHFT+2	# take off bottom 12 bits
	sll	k1, TLBLO_PFNSHIFT	# position it for tlblo_0
#endif
#if IP32
	ori	k1, (PG_G|PG_VR|PG_M|PG_NONCOHRNT)
#elif MCCHIP || IPMHSIM
#if IP20_MC_VIRTUAL_DMA
	ori	k1, (PG_G|PG_VR|PG_M|PG_COHRNT_EXLWR)
#else
	ori	k1, (PG_G|PG_VR|PG_M|PG_NONCOHRNT)
#endif /* IP20_MC_VIRTUAL_DMA */
#elif IP19
	ori	k1, (PG_G|PG_VR|PG_M|PG_COHRNT_EXLWR)>>TLBLO_HWBITSHIFT
#elif IP25 || IP27 || IP30
	ori	k1, (PG_G|PG_VR|PG_M|PG_COHRNT_EXLWR)
#else
	<< Bomb!! Need TLBLO flags. >>
#endif
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(k1, C0_TLBLO_0)
	PTR_ADDU k1, (PGFCTR<<TLBLO_PFNSHIFT)	# mapping for the odd page
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(k1, C0_TLBLO_1)		# is NBPP beyond the even page.
#if DROPIN_FIRST_LEVEL
	PTR_L	k1, kptbl		# LDSLOT load base of kernel page table
	c0	C0_WRITER		# LDSLOT write the mapping.

	# k1 points at base of kptbl. k0 is a byte index.
	# Compute a byte offset into kptbl. grab pte at that index
	PTR_ADDU k0, k1			# have index into kptbl
	PTE_L	k1, 0(k0)		# grab pte
	nop				# LDSLOT
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(k1, C0_TLBLO_0)		# drop pte in tlb

	PTE_L	k0, NBPDE(k0)		# grab pte
	DMFC0(k1, C0_TLBHI)		# LDSLOT save current TLBPID

	# load this into TLBLO register then check if valid
	.set	noat
	DMFC0(AT, C0_BADVADDR)		# pick up faulting vaddress
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(k0, C0_TLBLO_1)		# drop pte in tlb

	# construct TLBHI register and write new tlb entry
	# since this is a double tlb miss, the faulting address looks
	# like KPTEBASE | K2SEG_addr >> 9. So to compute the faulting
	# address of the 1st level miss, C0_BADVADDR << 9
	# Since G bit on, pid doesn't matter, so we don't bother to mask
	PTR_SLL	AT, TLBCTXT_BASEBITS
	DMTC0(AT, C0_TLBHI)		# drop pte in tlb
	NOP_0_4
	# XXXXXXX
	#c0	C0_WRITER
	NOP_0_4

	# all done - let's go!
	# Don't need AT anymore
	lreg	AT, VPDA_ATSAVE(zero)	# BDSLOT restore AT
	.set	at
	DMTC0(k1, C0_TLBHI)		# LDSLOT restore TLBPID
#else	/* !DROPIN_FIRST_LEVEL */
	# Just return after dropping in second level mapping.  First
	# level mapping will be dropped in by subsequent utlbmiss.
	# Don't need AT anymore
	.set	noat
#if _TLB_LOOP_LIMIT
	# execute a variable number of instructions, to avoid a TLB entry
	# exchange loop
	LA      AT, tlb_loop_cntr
	lw      k0,(AT)
	addiu   k0,1
	andi    k0,0x7
	sw      k0,(AT)
	beqz    k0,10f
	srl     k0,1                    # BDSLOT
	beqz    k0,10f
	srl     k0,1                    # BDSLOT
	beqz    k0,10f
	nop                             # BDSLOT
	nop
10:
#endif /* _TLB_LOOP_LIMIT */
	lreg	AT, VPDA_ATSAVE(zero)	# BDSLOT restore AT
	.set	at
	c0	C0_WRITER		# write the mapping
#endif	/* !DROPIN_FIRST_LEVEL */
	NOP_0_4
	ERET_PATCH(locore_eret_2)	# patchable eret

	# miss not in range of SYSSEG
syssegmiss:

#endif	/* R4000 || R10000 */

	/*
	 * R4000 vme processing.
	 * For R4000, recall that the fast refill vector is used
	 * for K2seg misses. When we get here, we are processing
	 * a 2nd level tlb fault.
	 * NOTE: cannot use k1 for R3000
	 *
	 * set k0 = btoct(vaddr - K2BASE)
	 * check k0 > 0 && k0 < SYSSEGSZ
	 */

	DMFC0(k0, C0_BADVADDR)		# get faulting virtual address
	.set	noat
	LI	AT, K2BASE		# LDSLOT
#if R4000 || R10000
#if _MIPS_SIM == _ABI64
	LI	k1, KPTE_REGIONMASK
	and	k1, k0
	PTR_SLL	k1, KPTE_REGIONSHIFT
	LI	AT, KPTE_VPNMASK
	and	k0, AT
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
	or	k0, k1			# put in region bits
	LI	AT, K2BASE
#else
	# turn k0 to faulting page no.
	# shift+1 to account for shift in utlbmiss
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
#endif
#endif	/* R4000 || R10000 */
	bgez	k0, jmp_to_longway	# KUSEG
	PTR_SUBU k0, AT			# BDSLOT subtract: k0 is K2BASE relative
	bltz	k0, jmp_to_longway	# < K2BASE
	nop				# BDSLOT
	lw	AT, syssegsz
	addiu	AT, (MAPPED_KERN_SIZE >> BPCSHIFT)
	PTR_SRL	k0, BPCSHIFT		# convert to vpn
	PTR_SUBU AT, AT, k0		# SYSSEGSZ - vpn

#if !EVEREST
	/* no io maps for these machines */
	blez	AT, jmp_to_longway	# outside of SYSSEGSZ
	nop				# BDSLOT

	/* FALL THROUGH to kvmiss_hit */
	.set	at
	.set	reorder
#else /* EVEREST */
	bgtz	AT, kvmiss_hit		# within SYSSEGSZ
	nop				# BDSLOT
	.set	at

#include "sys/iotlb.h"

	.data
	.globl	iotlb			# io space tlb array

	.text
	.set	noat
	.set	noreorder
	.globl	iosegmiss
EXPORT(iosegmiss)
	DMFC0(k0, C0_BADVADDR)
	NOP_1_1
#if R4000 || R10000
#if _MIPS_SIM == _ABI64
	# Region bits still intact in k1 at this point.
	LI	AT, KPTE_VPNMASK
	and	k0, AT
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
	or	k0, k1			# put in region bits
#else
	# turn k0 to faulting page no.
	# shift+1 to account for shift in utlbmiss
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
#endif
#endif	/* R4000 || R10000 */

	PTR_SLL	k0, IOMAP_SHIFTL	# get rid of leading bits
	# get rid of io vpage offset bits
	PTR_SRL	k0, IOMAP_SHIFTR+KPTE_TLBPRESHIFT
	# 8 byte per dual entry
	PTR_SLL	k0, PDESHFT+KPTE_TLBPRESHIFT
	LA	AT, iotlb
	PTR_ADDU k0, k0, AT

	PTE_L	AT, 0(k0)		# iomap carries TLBLO pfn
	nop
	beq	AT,zero,jmp_to_longway
	nop				# BDSLOT
#if defined(R4000) && !defined(IP19)
	TLBLO_FIX_HWBITS(AT)
#endif /* r4000 && !IP19 */
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(AT, C0_TLBLO)

#if R4000 || R10000
	PTE_L	AT, NBPDE(k0)
	nop
	beq	AT,zero,jmp_to_longway
#if IP19 || IP25 || IP27
	nop
#else
	# XXX - Do we need this for IP27?
	TLBLO_FIX_HWBITS(AT)
#endif /* IP19 */
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(AT, C0_TLBLO_1)

	li	k0, IOMAP_TLBPAGEMASK	# page size is larger for io
	mtc0	k0, C0_PGMASK
	NOP_1_1
#endif	/* R4000 || R10000 */

	DMFC0(k0, C0_BADVADDR)
#if R4000 || R10000
#if _MIPS_SIM == _ABI64
	LI	k1, KPTE_REGIONMASK
	and	k1, k0
	PTR_SLL	k1, KPTE_REGIONSHIFT
	LI	AT, KPTE_VPNMASK
	and	k0, AT
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
	or	k0, k1			# put in region bits
#else
	NOP_1_1
	# turn k0 to faulting page no.
	# shift+1 to account for shift in utlbmiss
	PTR_SLL	k0, TLBCTXT_VPNNORMALIZE+KPTE_TLBPRESHIFT+PGSHFTFCTR
#endif
	DMFC0(k1, C0_TLBHI)		# save current TLBPID
#endif	/* R4000 || R10000 */

	PTR_SRL	k0, IOMAP_PFNSHIFT	# tlbhi is the pfn and
	PTR_SLL	k0, IOMAP_PFNSHIFT	# 0 for asid.
	DMTC0(k0, C0_TLBHI)

	NOP_1_4
	TLB_WRITER		# random drop in into tlb
	NOP_0_4
#if R4000 || R10000
	li	k0, TLBPGMASK_MASK
	mtc0	k0, C0_PGMASK		# change page size back
#endif /* R4000 || R10000 */
	DMTC0(k1, C0_TLBHI)		# restore TLBPID
	.set	noat
	lreg	AT, VPDA_ATSAVE(zero)	# mfc0 LDSLOT restore AT
	NOP_0_4
	eret
	.set	reorder
	.set	at	
#endif /* !EVEREST */

EXPORT(kvmiss_hit)
	/*
	 * now valid K2SEG addr - grab pde - can now use k1
	 */
	.set	noreorder

	/* should not be here..... */
	b	invalpte
	nop				# BDSLOT
invalpte:
	/*
	 * Bad access
	 */
#ifdef R10000_SPECULATION_WAR
	lui	a0,0x8000
	CACHE_BARRIER_AT(0,a0)
#endif
	sreg	a0,_assregs+EF_A0
	sreg	a1,_assregs+EF_A1
	sreg	a2,_assregs+EF_A2
	ASMASSFAIL("invalid kernel pte");
	/* NOTREACHED */
#endif /* !TFP */

	/* Hack - can't branch to an external label, so branch here
	 * to do the jump.
	 */
jmp_to_longway:
	j	longway
	nop				# BDSLOT

	.set	at
	.set	reorder		# R10000_SPECULATION_WAR all text noreorder!

/*
 * This string is used by ASMASSFAIL().
 */
	.data
lmsg:	.asciiz "kmiss.s"

	.text
EXL_EXPORT(elocore_exl_19)	
	END(kmiss)
