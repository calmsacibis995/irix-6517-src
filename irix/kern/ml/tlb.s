/* Copyright(C) 1986, MIPS Computer Systems */

#ident	"$Revision: 3.86 $"

#include "ml/ml.h"

/*
 * NOTE: These routines are coded conservatively in regards to nop's
 * and m[tf]c0 operations. Note: the routines are coded noreorder
 * to avoid the reorganizer moving C0 instructions around
 * that it doesn't realize are order dependent.
 */
#ifdef IP19
/* special versions of tlbdropin and unmaptlb used ONLY by ECC exception
 * code.  Must be careful to not modify setting of ERL or DE bits.
 * In fact, for these routines there is no need to modify the SR at all
 * since ecc_handler will not allow interrupts.
 */
	
/*
 * ecc_unmaptlb(rpid, vpage): unmap a single tlb entry in the ITLB.
 *
 *	Probe for RPID/VPAGE in TLB.  If it doesn't exist, all done.
 *	If it does, mark the entries as invalid.
 *	Interrupts must be disabled because interrupts can generate
 *	a tlbmiss which will destroy the C0 registers.
 */
LEAF(ecc_unmaptlb)
	.set	noreorder
	/* following instructions load appropriate tlbpid after interrupts
	 * have been blocked.
	 */
	beq	zero,a0,1f
	nop				# BDSLOT
#if MP
	CPUID_L	t1,VPDA_CPUID(zero)
	PTR_ADD	a0,t1
#endif
	lbu	a0,(a0)			# a0 == tlbpid
1:		
	DMFC0(t1,C0_TLBHI)		# save TLBHI (for TLBPID)
	.set	reorder
	PTR_SLL	a1,TLBHI_VPNSHIFT	# shift vpn
	# An R4000 vpn is 20 bits, as for the R3000, but since the
	# tlb maps 2 virtual pages, only the upper 19 are stored
	# in the tlb.
	and	a1,TLBHI_VPNMASK	# isolate vpn
	or	t2,a1,a0		# tlbhi value to look for
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# put args into tlbhi
	NOP_1_3				# let tlbhi get through pipe
	c0	C0_PROBE		# probe for address
	NOP_0_4				# let probe write index reg
	mfc0	t3,C0_INX		# see what happened
	move	v0,zero			# LDSLOT
	bltz	t3,1f			# probe failed
	nop				# BDSLOT
	.set	reorder
	LI	t2,K0BASE&TLBHI_VPNMASK
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# invalidate entry
	DMTC0(zero,C0_TLBLO)		# cosmetic
	DMTC0(zero,C0_TLBLO_1)
	move	v0,t3			# let tlblo get through pipe
					# and return INX in v0 on "hit"
	c0	C0_WRITEI
	NOP_0_1				# let write get args before mod
1:	DMTC0(t1,C0_TLBHI)		# restore old TLBHI
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(ecc_unmaptlb)

/*
 * ecc_tlbdropin(tlbpid, vaddr, pte) -- random tlb drop-in
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 * a2 -- pte -- contents of pte
 *
 * v0 -- (return value) previous contents of TLBLO entry which we replaced
 *	 or zero if probe did not "hit".  This allows us to check for
 *	 utlbmiss having dropped in a "shared sentinel" and then we can
 *	 switch to special utlbmiss handler for overlapping segments.
 * Probes first to ensure that no other tlb entry exists with this pid
 * and vpn.
 */
LEAF(ecc_tlbdropin)
	.set	noreorder
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsrl    a2,a2,32                # 3rd arg - 1st pte
#endif
	DMFC0(t1,C0_TLBHI)		# save current pid
	.set	reorder
	beq	zero,a0,1f
	bne	a0,1,2f
	and	a0,t1,TLBHI_PIDMASK
	PTR_SRL	a0,a0,TLBHI_PIDSHIFT	# line up pid bits
	b	1f
2:
#if MP
	CPUID_L	t2,VPDA_CPUID(zero)
	PTR_ADD	a0,t2
#endif
	lbu	a0,(a0)
1:
	andi	t2,a1,NBPP		# Save low bit of vpn
#if !(R10000 && (_MIPS_SIM == _ABI64))
#if MCCHIP || IP32 || IPMHSIM
	and	a2,TLBLO_HWBITS		# Chop SW bits off pte
#else
	srl	a2,TLBLO_HWBITSHIFT	# Chop SW bits off pte
#endif	/* MCCHIP || IP32 || IPMHSIM */
#endif /* !(R10000 && (_MIPS_SIM == _ABI64)) */
	and	a1,TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	a0,a1			# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(a0,C0_TLBHI)		# vpn and pid of new entry
	NOP_1_3				# let tlbhi get through pipe
	c0	C0_PROBE		# probe for address
	NOP_0_4				# let probe fill index reg
	mfc0	t3,C0_INX
	move	v0,zero			# LDSLOT
	bltz	t3,ecc_nomatch		# Not found
	nop				# BDSLOT

	# read the existing mapping into TLBLO_0 and TLBLO_1.
	c0	C0_READI
	NOP_0_2				# part of pipe delay for READI


	# Test which tlb_lo register the pte should go into.
	# Since tlb entries map an even/odd pair of virtual
	# pages, if the low-order bit of the vpn is clear, it
	# is an even page, otherwise odd. The low bit was saved
	# in reg t2 above.
	beq	t2,zero,1f		# The even page.
	DMTC0(a0,C0_TLBHI)		# BDSLOT: for safety rewrite tlbhi
	nop				# required after mtc0 before mfc0
	DMFC0(v0,C0_TLBLO_1)		# return value (to check for sentinel)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(a2,C0_TLBLO_1)		# The odd page.
	b	2f
	nop
1:	nop
	DMFC0(v0,C0_TLBLO_0)		# return value (to check for sentinel)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(a2,C0_TLBLO_0)
	nop

	# Probe might have changed the page mask depending on which
	# tlb entry it matched. Pagemask might contain a page size  > NBPP
 	# as we support large pages now. Since this routine always drops in a 
	# base page (NBPP) tlb entry we have to set the page mask register to 
	# the correct value.
	# This can happen during a pte downgrade of a large page to the base 
	# page. The downgrade algorithm first invalidates the ptes of the large
	# page, does a tlbclean and then clears the page size field of the pte.
	# If an sproc utlbmisses in the pte after the tlb clean has been done
	# it can load a large page invalid tlb entry. So it will get a vfault
	# and when vfault calls a tlbdropin() it has to overwrite this tlb
	# entry. Since tlbdropin() does a C0_READI, it will contain page mask
	# value of the large page. This has to be reset to the 16k value before
	# dropping in the tlb entry.

2:	li	t2, TLBPGMASK_MASK
	mtc0	t2, C0_PGMASK		# change page size to 16k page size.
	NOP_0_4

	c0	C0_WRITEI		# re-use slot
	NOP_0_1				# let write get args before mod
	TLBLO_UNFIX_HWBITS(v0)		# BDSLOT
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	j	ra
	nop				# BDSLOT
ecc_nomatch:
	# The global bit in the tlb is the bitwise and of the global
	# bits of tlblo_0 and tlblo_1.
	# If the global bit is set in the new pte, set it
	# in the unprovided mapping.
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	beq	t2,zero,1f		# The even page.
	andi	t3,a2,TLBLO_G		# BDSLOT: Set/clear global bit.
	DMTC0(t3,C0_TLBLO_0)		# Possibly set global bit.
	DMTC0(a2,C0_TLBLO_1)
	b	2f
	nop				# BDSLOT
1:	DMTC0(t3,C0_TLBLO_1)		# Possibly set global bit.
	DMTC0(a2,C0_TLBLO_0)
	NOP_1_1				# let tlbAlo get through pipe
2:	c0	C0_WRITER		# Random slot.
	NOP_1_1				# let write get args before mod
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	j	ra
	move	v0,zero			# BDSLOT
	.set	reorder
	
	END(ecc_tlbdropin)
	
#endif /* IP19 */

/*
 * unmaptlb(tlbpidp, vpage): unmap a single tlb entry in the ITLB.
 *
 *	a0 == tlbpidp == pointer to array of pid numbers for all cpus
 *	
 *	Probe for RPID/VPAGE in TLB.  If it doesn't exist, all done.
 *	If it does, mark the entries as invalid.
 *	Interrupts must be disabled because interrupts can generate
 *	a tlbmiss which will destroy the C0 registers.
 */
#if defined(JUMP_WAR) || defined(DEBUG)
LEAF(_unmaptlb)
#else
LEAF(unmaptlb)
#endif
	.set	noreorder
	mfc0	t0,C0_SR		# save SR
	NOP_0_4
	NOINTS(t2,C0_SR)		# no interrupts
	NOP_0_4

	/* following instructions load appropriate tlbpid after interrupts
	 * have been blocked.
	 */
	beq	zero,a0,1f
	nop				# BDSLOT
#if MP
	CPUID_L	t1,VPDA_CPUID(zero)
	PTR_ADD	a0,t1
#endif
	lbu	a0,(a0)			# a0 == tlbpid
1:		
	DMFC0(t1,C0_TLBHI)		# save TLBHI (for TLBPID)
	.set	reorder
	PTR_SLL	a1,TLBHI_VPNSHIFT	# shift vpn
	# An R4000 vpn is 20 bits, as for the R3000, but since the
	# tlb maps 2 virtual pages, only the upper 19 are stored
	# in the tlb.
	and	a1,TLBHI_VPNMASK	# isolate vpn
	or	t2,a1,a0		# tlbhi value to look for
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# put args into tlbhi
	NOP_1_3				# let tlbhi get through pipe
#if defined(PROBE_WAR)
	# The workaround requires that the probe instruction
	# never get into the i-cache. So we execute the instruction
	# uncached, and also surround the instruction with nops
	# that are not executed, which ensures that a cache miss
	# to a nearby instruction will not bring the probe into
	# the i-cache.
	LA	ta1,1f
	or	ta1,SEXT_K1BASE
	LA	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_0_4				# let probe write index reg
	mfc0	t3,C0_INX		# see what happened
	move	v0,zero			# LDSLOT
	bltz	t3,1f			# probe failed
	nop				# BDSLOT
#if defined(EVEREST) && defined(DEBUG)
	addi	t3,-2
	bgez	t3,9f
	nop
	LA	a0,8f
	jal	panic
	nop
	.data
8:	.asciiz "unmaptlb: PROBE returned 0 or 1\12\15"
	.text
9:	addi	t3,2
#endif

	.set	reorder
	LI	t2,K0BASE&TLBHI_VPNMASK
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# invalidate entry
	DMTC0(zero,C0_TLBLO)		# cosmetic
	DMTC0(zero,C0_TLBLO_1)
	move	v0,t3			# let tlblo get through pipe
					# and return INX in v0 on "hit"
	c0	C0_WRITEI
	NOP_0_1				# let write get args before mod
1:	DMTC0(t1,C0_TLBHI)		# restore old TLBHI
	mtc0	t0,C0_SR		# restore sr and return
	j	ra
	nop				# BDSLOT
	.set	reorder
#if defined(JUMP_WAR) || defined(DEBUG)
	END(_unmaptlb)
#else
	END(unmaptlb)
#endif

#ifdef NOTDEF
/*
 * unmodtlb(rpid, vpage): Clear the dirty/writeable bit for the ITLB.
 */
LEAF(unmodtlb)
	.set	noreorder
	mfc0	t0,C0_SR		# save sr
	NOP_0_4
	NOINTS(t2,C0_SR)		# disable interrupts
	NOP_0_4
	DMFC0(t1,C0_TLBHI)		# save TLBHI
	.set	reorder
	PTR_SLL	a1,TLBHI_VPNSHIFT	# construct new TLBHI
	and	a1,TLBHI_VPNMASK	# isolate vpn
	or	t2,a0,a1
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# move to C0 for probe
	NOP_1_3				# let tlbi get through pipe
#if defined(PROBE_WAR)
	la	ta1,1f
	or	ta1,SEXT_K1BASE
	la	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_0_4				# let probe fill index reg
	mfc0	t3,C0_INX
	move	v0,zero			# LDSLOT
	bltz	t3,1f			# probe failed
	nop				# BDSLOT
#if defined(EVEREST) && defined(DEBUG)
	addi	t3,-2
	bgez	t3,9f
	nop
	la	a0,8f
	jal	panic
	nop
	.data
8:	.asciiz "unmodtlb: PROBE returned 0 or 1\12\15"
	.text
9:	addi	t3,2
#endif

	c0	C0_READI		# load entry in TLBLO/TLBHI
	NOP_0_3				# let readi fill tlblo reg
	DMFC0(t2,C0_TLBLO)
	DMFC0(a1,C0_TLBLO_1)
	NOP_1_0				# !R4000: wait for t2 thru pipe
	.set	reorder
	and	t2,~TLBLO_D		# reset modified bit
	and	a1,~TLBLO_D		# reset modified bit
	.set	noreorder
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(t2,C0_TLBLO)		# unmod entry
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(a1,C0_TLBLO_1)
	NOP_1_1				# let tlblo get through pipe
	c0	C0_WRITEI
	NOP_0_1				# let write get args before mod
1:	DMTC0(t1,C0_TLBHI)		# restore old TLBHI
	mtc0	t0,C0_SR		# restore sr
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(unmodtlb)
#endif	/* NOTDEF */

/*
 * invaltlb(i): Invalidate the ith ITLB entry.
 * called whenever a specific TLB entry needs to be invalidated.
 */
#if JUMP_WAR
LEAF(_invaltlb)
#else
LEAF(invaltlb)
#endif
	LI	t2,K0BASE&TLBHI_VPNMASK
	.set	noreorder
	mfc0	v0,C0_SR		# save SR and disable interrupts
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	DMFC0(t0,C0_TLBHI)		# save current TLBHI
	DMTC0(t2,C0_TLBHI)		# invalidate entry
	DMTC0(zero,C0_TLBLO)
	DMTC0(zero,C0_TLBLO_1)
	mtc0	a0,C0_INX
	NOP_1_1				# let index get through pipe
	c0	C0_WRITEI
	NOP_0_1				# let write get args before mod
	DMTC0(t0,C0_TLBHI)
	mtc0	v0,C0_SR
	j	ra
	nop				# BDSLOT
	.set	reorder
#if JUMP_WAR
	END(_invaltlb)
#else
	END(invaltlb)
#endif

/*
 * tlbwired(indx, tlbpid, vaddr, pte) -- setup wired ITLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- tlbpidp -- pointer to array of context numbers to use (0..TLBHI_NPID-1)
 *                  indexed by cpuid.  EXCEPT if 0, tlbpid = 0.
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pte -- contents of pte
 *
 * 5th argument - (sp+16) second pte
 * 6th argument - page mask (size)
 */
LEAF(tlbwired)
	.set	noreorder
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsrl    a3,a3,32                # 4th arg - 1st pte
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
        dsrl    a4,a4,32                # 5th arg - 1st pte
#endif
#endif
	mfc0	v0,C0_SR		# save SR and disable interrupts
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	DMFC0(t0,C0_TLBHI)		# save current TLBPID
	.set	reorder
	beq	zero,a1,1f
#if MP
	CPUID_L	t1,VPDA_CPUID(zero)
	PTR_ADD	a1,t1
#endif
	lbu	a1,(a1)
1:
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	move	t1,a4			# 5th arg - 2nd pte
	move	t2,a5			# 6th arg - page size
#else
	lw	t1,16(sp)		# 5th arg - 2nd pte
	lw	t2,20(sp)		# 6th arg - page size
#endif
#if !(R10000 && (_MIPS_SIM == _ABI64))
#if MCCHIP || IP32 || IPMHSIM
	and	a3,TLBLO_HWBITS		# chop software bits from pte.
	and	t1,TLBLO_HWBITS		# chop software bits from 2nd pte.
#else
	srl	a3,TLBLO_HWBITSHIFT	# chop software bits from pte.
	srl	t1,TLBLO_HWBITSHIFT	# chop software bits from 2nd pte.
#endif /* MCCHIP || IP32 || IPMHSIM */
#endif /* !(R10000 && (_MIPS_SIM == _ABI64)) */
	and	a2,TLBHI_VPNMASK	# chop offset bits
	or	a1,a2			# formatted tlbhi entry
	.set	noreorder
	mfc0	t3,C0_PGMASK		# save current page mask
	mtc0	t2,C0_PGMASK
	DMTC0(a1,C0_TLBHI)		# set VPN and TLBPID
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(a3,C0_TLBLO)		# set PPN and access bits
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(t1,C0_TLBLO_1)		# set PPN and access bits for 2nd pte.
	mtc0	a0,C0_INX		# set INDEX to wired entry
	NOP_1_1				# let index get through pipe
	c0	C0_WRITEI		# drop it in
	NOP_0_1				# let write get args before mod
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	mtc0	t3,C0_PGMASK		# change page size back
	mtc0	v0,C0_SR		# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(tlbwired)

/*
 * tlbdropin(tlbpid, vaddr, pte) -- random tlb drop-in
 * a0 -- tlbpidp -- pointer to array of context numbers to use (0..TLBHI_NPID-1)
 *                  indexed by cpuid.  EXCEPT if 0, tlbpid = 0;
 *                  if 1, use current tlbpid.
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 * a2 -- pte -- contents of pte
 *
 * v0 -- (return value) previous contents of TLBLO entry which we replaced
 *	 or zero if probe did not "hit".  This allows us to check for
 *	 utlbmiss having dropped in a "shared sentinel" and then we can
 *	 switch to special utlbmiss handler for overlapping segments.
 * Probes first to ensure that no other tlb entry exists with this pid
 * and vpn.
 */
#if DEBUG
LEAF(_tlbdropin)
#else
LEAF(tlbdropin)
#endif
	.set	noreorder
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsrl    a2,a2,32                # 3rd arg - 1st pte
#endif
	mfc0	t0,C0_SR		# save SR and disable interrupts
	NOP_0_4
	NOINTS(t2,C0_SR)
	NOP_0_4
	DMFC0(t1,C0_TLBHI)		# save current pid
	.set	reorder
	beq	zero,a0,1f
	bne	a0,1,2f
	and	a0,t1,TLBHI_PIDMASK
	PTR_SRL	a0,a0,TLBHI_PIDSHIFT	# line up pid bits
	b	1f
2:
#if MP
	CPUID_L	t2,VPDA_CPUID(zero)
	PTR_ADD	a0,t2
#endif
	lbu	a0,(a0)
1:
	andi	t2,a1,NBPP		# Save low bit of vpn
#if !(R10000 && (_MIPS_SIM == _ABI64))
#if MCCHIP || IP32 || IPMHSIM
	and	a2,TLBLO_HWBITS		# Chop SW bits off pte
#else
	srl	a2,TLBLO_HWBITSHIFT	# Chop SW bits off pte
#endif	/* MCCHIP || IP32 || IPMHSIM */
#endif /* !(R10000 && (_MIPS_SIM == _ABI64)) */
	and	a1,TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	a0,a1			# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(a0,C0_TLBHI)		# vpn and pid of new entry
	NOP_1_3				# let tlbhi get through pipe
#if defined(PROBE_WAR)
	LA	ta1,1f
	or	ta1,SEXT_K1BASE
	LA	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_0_4				# let probe fill index reg
	mfc0	t3,C0_INX
	move	v0,zero			# LDSLOT
	bltz	t3,nomatch		# Not found
	nop				# BDSLOT

#if defined(EVEREST) && defined(DEBUG)
	addi	t3,-2
	bgez	t3,8f
	nop
	addi	a2,t3,2
	move	a3,a1
	PANIC("tlbdropin: PROBE returned 0x%x on vaddr 0x%x\12\15")
8:	addi	t3,2
	/* Do a SECOND probe and verify the result */
#if defined(PROBE_WAR)
	la	ta1,1f
	or	ta1,SEXT_K1BASE
	la	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_0_4
	mfc0	ta1,C0_INX
	NOP_0_4
	beq	ta1,t3,2f
	nop
	move	a2,t3
	move	a3,ta1
	PANIC("tlbdropin: PROBE did not VERIFY, first 0x%x second 0x%x\12\15")
	/* SECOND probe verified */
2:
#endif	/* EVEREST & DEBUG */
	# read the existing mapping into TLBLO_0 and TLBLO_1.
	c0	C0_READI
	NOP_0_2				# part of pipe delay for READI


	# Test which tlb_lo register the pte should go into.
	# Since tlb entries map an even/odd pair of virtual
	# pages, if the low-order bit of the vpn is clear, it
	# is an even page, otherwise odd. The low bit was saved
	# in reg t2 above.
	beq	t2,zero,1f		# The even page.
	DMTC0(a0,C0_TLBHI)		# BDSLOT: for safety rewrite tlbhi
	nop				# required after mtc0 before mfc0
	DMFC0(v0,C0_TLBLO_1)		# return value (to check for sentinel)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(a2,C0_TLBLO_1)		# The odd page.
	b	2f
	nop
1:	nop
	DMFC0(v0,C0_TLBLO_0)		# return value (to check for sentinel)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(a2,C0_TLBLO_0)
	nop

	# Probe might have changed the page mask depending on which
	# tlb entry it matched. Pagemask might contain a page size  > NBPP
 	# as we support large pages now. Since this routine always drops in a 
	# base page (NBPP) tlb entry we have to set the page mask register to 
	# the correct value.
	# This can happen during a pte downgrade of a large page to the base 
	# page. The downgrade algorithm first invalidates the ptes of the large
	# page, does a tlbclean and then clears the page size field of the pte.
	# If an sproc utlbmisses in the pte after the tlb clean has been done
	# it can load a large page invalid tlb entry. So it will get a vfault
	# and when vfault calls a tlbdropin() it has to overwrite this tlb
	# entry. Since tlbdropin() does a C0_READI, it will contain page mask
	# value of the large page. This has to be reset to the 16k value before
	# dropping in the tlb entry.

2:	li	t2, TLBPGMASK_MASK
	mtc0	t2, C0_PGMASK		# change page size to 16k page size.
	NOP_0_4

	c0	C0_WRITEI		# re-use slot
	NOP_0_1				# let write get args before mod
	TLBLO_UNFIX_HWBITS(v0)		# BDSLOT
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	mtc0	t0,C0_SR		# Restore SR
	j	ra
	nop				# BDSLOT
nomatch:
#if defined(EVEREST) && defined(DEBUG)
	/* Do a SECOND probe and verify the result */
#if defined(PROBE_WAR)
	la	ta1,1f
	or	ta1,SEXT_K1BASE
	la	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_0_4
	mfc0	ta1,C0_INX
	NOP_0_4
	bltz	ta1,2f			# OK if didn't find it agani
	nop
	move	a2,t3
	move	a3,ta1
	PANIC("tlbdropin: PROBE NOmatch did not VERIFY, first 0x%x second 0x%x\12\15")
	/* SECOND probe verified */
2:
#endif	/* EVEREST & DEBUG */
	# The global bit in the tlb is the bitwise and of the global
	# bits of tlblo_0 and tlblo_1.
	# If the global bit is set in the new pte, set it
	# in the unprovided mapping.
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	beq	t2,zero,1f		# The even page.
	andi	t3,a2,TLBLO_G		# BDSLOT: Set/clear global bit.
	DMTC0(t3,C0_TLBLO_0)		# Possibly set global bit.
	DMTC0(a2,C0_TLBLO_1)
	b	2f
	nop				# BDSLOT
1:	DMTC0(t3,C0_TLBLO_1)		# Possibly set global bit.
	DMTC0(a2,C0_TLBLO_0)
	NOP_1_1				# let tlbAlo get through pipe
2:	c0	C0_WRITER		# Random slot.
	NOP_1_1				# let write get args before mod
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	mtc0	t0,C0_SR		# restore SR
	j	ra
	move	v0,zero			# BDSLOT
	.set	reorder
#if DEBUG
	END(_tlbdropin)
#else
	END(tlbdropin)
#endif

/*
 * tlbdrop2in(tlbpid, vaddr, pte, pte2) -- random tlb drop-in
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 * a2 -- pte -- contents of pte
 * a3 -- pte2 -- contents of pte
 *
 * like tlbdropin, but called with 2 pte's. Simpler, since it
 * doesn't have to figure out even/odd.
 *
 * Probes first to ensure that no other tlb entry exists with this pid
 * and vpn.
 */
#if DEBUG
LEAF(_tlbdrop2in)
#else
LEAF(tlbdrop2in)
#endif
	.set	noreorder
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsrl    a2,a2,32                # 3rd arg - 1st pte
        dsrl    a3,a3,32                # 4th arg - 2nd pte
#endif
	mfc0	t0,C0_SR		# save SR and disable interrupts
	NOP_0_4
	NOINTS(t2,C0_SR)
	NOP_0_4
	DMFC0(t1,C0_TLBHI)		# save current pid
	.set	reorder
#if !(R10000 && (_MIPS_SIM == _ABI64))
#if MCCHIP || IP32 || IPMHSIM
	and	a2,TLBLO_HWBITS		# Chop SW bits off pte
	and	a3,TLBLO_HWBITS		# Chop SW bits off pte
#else
	srl	a2,TLBLO_HWBITSHIFT	# Chop SW bits off pte
	srl	a3,TLBLO_HWBITSHIFT	# Chop SW bits off pte
#endif /* MCCHIP || IP32 || IPMHSIM */
#endif /* !(R10000 && (_MIPS_SIM == _ABI64)) */
	and	a1,TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	a0,a1			# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(a0,C0_TLBHI)		# vpn and pid of new entry
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(a2,C0_TLBLO_0)		# pte for new entry
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(a3,C0_TLBLO_1)		# pte for new entry
	NOP_0_3				# let tlblo get through pipe
#if defined(PROBE_WAR)
	LA	ta1,1f
	or	ta1,SEXT_K1BASE
	LA	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_0_4				# let probe fill index reg
	mfc0	t3,C0_INX	
	move	v0,zero			# LDSLOT
	bltz	t3,1f			# not found
	nop				# BDSLOT
#if defined(EVEREST) && defined(DEBUG)
	addi	t3,-2
	bgez	t3,9f
	nop
	LA	a0,8f
	jal	panic
	nop
	.data
8:	.asciiz "tlbdropin2: PROBE returned 0 or 1\12\15"
	.text
9:	addi	t3,2
#endif
	c0	C0_WRITEI		# re-use slot
	b	2f
	nop				# BDSLOT
1:	c0	C0_WRITER		# use random slot
	NOP_0_1				# let write get args before mod
2:	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	mtc0	t0,C0_SR		# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
#if DEBUG
	END(_tlbdrop2in)
#else
	END(tlbdrop2in)
#endif

#if	defined(PTE_64BIT)
/*
 * tlbdropin_lpgs(tlbpid, vaddr, pte, pte, pgsize) -- tlb drop-in
 * of a pair of ptes for very large pages.
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- badvaddr -- virtual address to map once it is rounded
 *			to the appropriate large page boundary.
 * a2 -- even slot pte 
 * a3 -- odd slot pte
 * a4 -- pagesize-- used to get value to write into pagemask register
 *
 * Probes first to ensure that no other tlb entry exists with this pid
 * and vpn.
 */
LEAF(tlbdropin_lpgs)
	.set	noreorder
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsrl    a2,a2,32                # 3rd arg - 1st pte
        dsrl    a3,a3,32                # 4th arg - 2nd pte
#endif
	mfc0	v0,C0_SR		# save SR and disable interrupts
	NOP_0_4
	NOINTS(t0,C0_SR)
	NOP_0_4
	.set	reorder

#ifdef R4000
	srl	a2,TLBLO_HWBITSHIFT	# Chop SW bits off pte
	srl	a3,TLBLO_HWBITSHIFT	# Chop SW bits off pte
#endif
	or	a1,a0			# vpn/pid ready for probe

	.set	noreorder
	DMTC0(a1,C0_TLBHI)		# vpn and pid of badvaddr
	NOP_1_3				# let tlbhi get through pipe
	c0	C0_PROBE
	NOP_0_4				# let probe fill index reg
	mfc0	t8,C0_INX
	NOP_0_1

	# round vaddr to large page boundary
	addi	t3,a4,-1
	and	t3,a1,t3
	dsubu	a1,a1,t3		# subtract off large page offset
	nor	t0,zero,a4		# construct vpnmask for vaddr
	and	a1,a1,t0
	or	a0,a1			# or in tlbpid
	DMTC0(a0,C0_TLBHI)		# set entryhi for dropin
	NOP_1_3				# let tlbhi get through pipe
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(a2,C0_TLBLO_0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(a3,C0_TLBLO_1)
	NOP_0_1				# let write get args before mod

	# get the page mask from the page size
	addi	t3,a4,-1
	srl	t3,t3,0xc
	sll	t3,t3,0xd
	mtc0    t3, C0_PGMASK		# set the page mask 
	NOP_1_3

	bltz	t8,1f			# probe missed- do random drop
	nop
	c0	C0_WRITEI		# drop into where probe hit
	NOP_0_1
	b	2f
	nop
1:	TLB_WRITER			# random drop into tlb
	NOP_0_4
2:	li	t0, TLBPGMASK_MASK
	mtc0	t0, C0_PGMASK		# change page size back
	NOP_1_3
	mtc0	v0,C0_SR		# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(tlbdropin_lpgs)
#endif

#ifdef LATER
/*
 * tlbwire_lpgs(indx, tlbpid, vaddr, pte) -- setup wired ITLB entry
 * for a potentially large page.
 * a0 -- indx -- tlb entry index
 * a1 -- tlbpid -- context number to use (0..TLBHI_NPID-1)
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pagesize
 * a4 -- even pte
 * a5 -- odd pte
 *
 */
LEAF(tlbwire_lpgs)
	.set	noreorder
	mfc0	v0,C0_SR		# save SR and disable interrupts
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	.set	reorder

#ifdef R4000
	srl	a4,TLBLO_HWBITSHIFT	# chop software bits from pte.
	srl	a5,TLBLO_HWBITSHIFT	# chop software bits from pte.
#endif
	move	t0,a3
	nor	t0,zero,t0		# construct vpnmask for vaddr
	and	a2,a2,t0
	or	a1,a2			# formatted tlbhi entry
	# get the page mask from the page size
	addi	a3,-1
	srl	a3,0xc
	sll	a3,0xd

	.set	noreorder
	DMTC0(a1,C0_TLBHI)		# set VPN and TLBPID
	NOP_1_3				# let tlbhi get through pipe
	c0	C0_PROBE
	NOP_0_4				# let probe fill index reg
	mfc0	t8,C0_INX
	NOP_0_1
	bltz	t8,1f			# Not found
	nop
	LI	t8,K0BASE&TLBHI_VPNMASK
	DMTC0(t8,C0_TLBHI)		# invalidate entry
	NOP_1_3
	DMTC0(zero,C0_TLBLO_0)
	NOP_0_1
	DMTC0(zero,C0_TLBLO_1)
	NOP_0_1
	c0	C0_WRITEI		# drop it in
	NOP_0_1
	DMTC0(a1,C0_TLBHI)		# restore tlbhi
	NOP_1_3				# let tlbhi get through pipe
1:	mtc0    a3, C0_PGMASK		# set the page mask
	NOP_1_3
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(a4,C0_TLBLO_0)
	NOP_0_1
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(a5,C0_TLBLO_1)
	NOP_0_1
	mtc0	a0,C0_INX		# set INDEX to wired entry
	NOP_1_1				# let index get through pipe
	c0	C0_WRITEI		# drop it in
	NOP_0_1				# let write get args before mod
	li	t3, TLBPGMASK_MASK
	mtc0	t3, C0_PGMASK		# change page size back
	NOP_1_3
	mtc0	v0,C0_SR		# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(tlbwire_lpgs)
#endif /* LATER */

/*
 * flush entire non-wired tlb
 * flush_tlb(start) -- flush tlb beginning at entry start
 * a0 -- start -- first entry to flush
 */
LEAF(flush_tlb)
#ifdef DEBUG
	bltz	a0,2f
	li	v1,TLBRANDOMBASE
	ble	a0,v1,3f
2:
	move	t8,a0
	/* save off a0 into t8 since ASMASSFAIL trashes */
	ASMASSFAIL("start<0 or start>TLBRANDOMBASE (t8 contains a0 == start)");
3:
#endif /* DEBUG */
	LI	t0,K0BASE&TLBHI_VPNMASK	# set up to invalidate entries
#if R10000 && R4000
	lw	v1,ntlbentries
#else
	li	v1,NTLBENTRIES		# last entry plus one
#endif /* R10000 && R4000 */
	.set	noreorder
	mfc0	t2,C0_SR
	NOINTS(t3,C0_SR)		# interrupts off
	NOP_0_4
	DMFC0(t1,C0_TLBHI)		# save pid
	DMTC0(t0,C0_TLBHI)
	DMTC0(zero,C0_TLBLO)
	DMTC0(zero,C0_TLBLO_1)
	mtc0	a0,C0_INX		# set index
1:
	addu	a0,1			# LDSLOT: bump to next entry
	c0	C0_WRITEI		# invalidate
	bne	a0,v1,1b		# more to do
	mtc0	a0,C0_INX		# BDSLOT - set index

	DMTC0(t1,C0_TLBHI)
	j	ra
	mtc0	t2,C0_SR		# BDSLOT
	.set	reorder
	END(flush_tlb)

/*
 * set_tlbpid -- set current tlbpid and change all the pid fields
 * in the wired tlb entries to the new pid.
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 */
LEAF(set_tlbpid)
	.set	noreorder
	mfc0	t0,C0_SR
	NOP_0_4
	NOINTS(t1,C0_SR)		# interrupts off
	NOP_0_4
	.set	reorder
	PTR_SLL	a0,TLBHI_PIDSHIFT	# line up pid bits
	li	t1,NWIREDENTRIES-1
	li	t3,TLBWIREDBASE
1:
	.set	noreorder
	mtc0	t1,C0_INX
	NOP_1_1				# let index get through pipe
	c0	C0_READI		# read current tlb entry
	NOP_0_3				# let readi set tlbhi reg
	DMFC0(t2,C0_TLBHI)
	NOP_1_0				# wait while t2 loaded
	.set	reorder
	and	t2,~TLBHI_PIDMASK	# take down current pid bits
	or	t2,a0			# assert new pid
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# write back new value
	NOP_1_1				# let tlbhi get through pipe
	c0	C0_WRITEI
	bgt	t1,t3,1b
	sub	t1,1			# BDSLOT
	mtc0	t0,C0_SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(set_tlbpid)

#ifdef PIDDEBUG
/*
 * tlbpidmatch(tlbpid, indx)
 * find a tlb entry corresponding to tlbpid,indx pair
 * Ignore G(lobal) mappings
 */
LEAF(tlbpidmatch)
	.set	noreorder
	mfc0	t0,C0_SR
	NOP_0_4
	NOINTS(ta1,C0_SR)
	NOP_0_4
	DMFC0(t1,C0_TLBLO)		# 
	DMFC0(t2,C0_TLBHI)
	mfc0	t3,C0_INX	
	DMFC0(ta0,C0_TLBLO_1)		# 
	PTR_SLL	a0,TLBHI_PIDSHIFT	# 
	mtc0	a1, C0_INX
	move	v0, zero		# LDSLOT
	c0	C0_READI		# read slot
	NOP_1_3				# let readi write tlblo reg
	DMFC0(a1,C0_TLBLO)
	NOP_1_0				# wait for a1 to be filled
	andi	a1,TLBLO_V
	beq	a1,zero,2f
	andi    a2,a1,TLBLO_G           # BDSLOT
	bne     a2,zero,2f              # ignore G(lobal) mappings
	nop				# BDSLOT
	DMFC0(a1,C0_TLBHI)
	NOP_1_0				# wait for a1 to be filled
	andi	a1, TLBHI_PIDMASK
	beq	a0,a1,1f
	nop				# BDSLOT
	b	2f
	nop				# BDSLOT
1:
	li	v0,1
2:
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(t1,C0_TLBLO)		# 
	DMTC0(t2,C0_TLBHI)
	mtc0	t3,C0_INX	
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(ta0,C0_TLBLO_1)		# 
	mtc0	t0,C0_SR
	j	ra
	nop				# BDSLOT
	.set 	reorder
	END(tlbpidmatch)
#endif /* PIDDEBUG */
/*
 * tlbfind(tlbpid, vaddr &low, &hi) -- find a tlb entry corresponding to vaddr
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 * a2 -- ptr to hold tlblo value
 * a3 -- ptr to hold tlblo1 value
 *
 * Returns -1 if not in tlb else TLBIDX register as well as tlblo & tlblo1
 */
LEAF(tlbfind)
	.set	noreorder
	mfc0	t0,C0_SR		# save SR and disable interrupts
	NOP_0_4
	NOINTS(t2,C0_SR)
	NOP_0_4
	DMFC0(t1,C0_TLBHI)		# save current pid
	.set 	reorder
	PTR_SLL	a0,TLBHI_PIDSHIFT	# align pid bits for entryhi
	and	a1,TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	t2,a0,a1		# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# vpn and pid of new entry
	NOP_1_3				# let tlbhi get through pipe
#if defined(PROBE_WAR)
	LA	ta1,1f
	or	ta1,SEXT_K1BASE
	LA	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_1_4				# let probe write index reg
	mfc0	v0,C0_INX	
	NOP_1_1				# wait for v0 to be filled
	bltz	v0,1f			# not found
	nop

	/* found it - return ENTRYLo and ENTRYLo1 */
	c0	C0_READI		# read slot
	NOP_1_3				# let readi fill tlblo reg
	DMFC0(t3,C0_TLBLO)
	NOP_1_0				# wait for t3 to be filled
	TLBLO_UNFIX_HWBITS(t3)
#ifdef PTE_64BIT
	sd	t3, 0(a2)
#else
	sw	t3, 0(a2)
#endif
	DMFC0(t3,C0_TLBLO_1)
	NOP_1_0				# wait for t3 to be filled
	TLBLO_UNFIX_HWBITS(t3)
#ifdef PTE_64BIT
	sd	t3, 0(a3)
#else
	sw	t3, 0(a3)
#endif
	b	2f
	nop
1:
	li	v0,-1
2:
#if R4000 || R10000
        li	t2, TLBPGMASK_MASK
	mtc0	t2, C0_PGMASK		# change page size to 16k page size.
#endif
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	mtc0	t0,C0_SR		# restore SR
	j	ra
	nop				# BDSLOT
	.set 	reorder
	END(tlbfind)

/*
 * tlbgetpid - return current tlbpid
 */
LEAF(tlbgetpid)
	.set    noreorder
	DMFC0(v0,C0_TLBHI)		# get current tlbpid
	NOP_1_0				# wait for v0 to be filled
	.set 	reorder
	and	v0,TLBHI_PIDMASK
	srl	v0,TLBHI_PIDSHIFT	# line up pid bits
	j	ra
	END(tlbgetpid)


/*
 if R4000
 * tlbgetent(ind, *hi, *lo0, *lo1)
 else if R3000
 * tlbgetent(ind, *hi, *lo0)
 * Get the contents of tlb registers for given index
 *
 */
LEAF(tlbgetent)
	.set	noreorder
	mfc0	t3,C0_SR	# save SR
	NOP_0_4
	NOINTS(t2,C0_SR)	# no interrupts
	NOP_0_4

	DMFC0(v0,C0_TLBHI)	# Save old TLBHI
	NOP_1_1

	MTC0(a0,C0_INX)		# Load index and read
	NOP_1_1
	c0	C0_READI
	NOP_1_3

	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 will serialize execution

	DMFC0(t0,C0_TLBHI)	# Save TLBHI in first parameer
	NOP_1_1
	REG_S	t0,0(a1)

	DMFC0(t0,C0_TLBLO)	# TLBL00 as second parameter
	NOP_1_1
	INT_S	t0,0(a2)

	DMFC0(t0,C0_TLBLO_1)	# Save TLBLO1 if R4000 or TFP
	NOP_1_1
	INT_S	t0,0(a3)
	DMTC0(v0,C0_TLBHI)
	NOP_1_1

#if R4000 || R10000
	mtc0	t0, C0_PGMASK	# change page size to 16k page size.
	NOP_1_3
#endif
	mtc0	t3,C0_SR	# restore sr
	NOP_0_2
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder

	j	ra

	END(tlbgetent)

/*
 * tlbgetrand - return current tlbrandom register
 */
LEAF(tlbgetrand)
XLEAF(getrand)
	.set    noreorder
	mfc0	v0,C0_RAND		# get current tlbrandom register
	NOP_1_0				# wait for v0 to be filled
	.set 	reorder
	srl	v0,TLBRAND_RANDSHIFT
	j	ra
	END(tlbgetrand)

#if JUMP_WAR
/*
 * setwired(x): Set the number of wired entries as specified.
 */
LEAF(setwired)
	.set	noreorder
	mtc0	a0,C0_TLBWIRED
	NOP_0_4
	j	ra
	nop
	.set	reorder
	END(setwired)

LEAF(getwired)
	.set	noreorder
	mfc0	v0,C0_TLBWIRED
	NOP_0_4
	j	ra
	nop
	.set	reorder
	END(getwired)
#endif

#if R4000 || R10000
/*
 * tlb_probe(tlbpid, vaddr, int *)
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 * a2 -- &page_mask -- page mask of the tlb entry if found.
 * Returns -1 or the correct TLBLO register.
 */
LEAF(tlb_probe)
	.set	noreorder	
	mfc0	t3,C0_SR		# save SR
	NOP_0_4
	NOINTS(t2,C0_SR)		# no interrupts
	NOP_0_4

	DMFC0(t0,C0_TLBHI)		# save current pid
	PTR_SLL	a0,TLBHI_PIDSHIFT	# align pid bits for entryhi
	and	t1, a1, TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	t1, t1, a0		# vpn/pid ready for entryhi
	DMTC0(t1,C0_TLBHI)		# vpn and pid of new entry
	NOP_0_3
	c0	C0_PROBE		# probe for address
	NOP_0_4
	mfc0	t1,C0_INX

	bltz	t1,1f			# not found
	li	v0,-1			# BDSLOT
	
	c0	C0_READI		# read the tlb at set index
	NOP_0_3
	mfc0	t1, C0_PGMASK		# read the page mask
	NOP_0_3
	AUTO_CACHE_BARRIERS_DISABLE	# mfc0 will serialize execution
	INT_S	t1, 0(a2)		# write at pointer passed in.
	AUTO_CACHE_BARRIERS_ENABLE
	INT_SRL	t1, 1			# now matches upper page offset bits.
	or	t1, MIN_POFFMASK	# page_mask + all offset bits set.
	PTR_ADDU t1, 1			# odd/even selector bit set.
	and	t1, a1			# check if vaddr is odd or even
	DMFC0(v0,C0_TLBLO_1)		# Assume odd and load tlblo_1 
	bne	t1, zero, 1f
	nop				# BDSLOT
	NOP_0_1				# LDSLOT, if we didn't take the branch.
	DMFC0(v0,C0_TLBLO)		# if even read tlblo
1:
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	li	t0, TLBPGMASK_MASK
	mtc0	t0, C0_PGMASK		# change page size to 16k page size.
	NOP_1_3
	mtc0	t3,C0_SR		# restore sr
	NOP_0_2
	j	ra
	nop				# BDSLOT
	.set	reorder	
	END(tlb_probe)
#endif /* R4000 || R10000 */

	.data
lmsg:	.asciiz	"tlb.s"

