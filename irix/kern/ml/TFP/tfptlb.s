
/* Copyright(C) 1986, MIPS Computer Systems */

#ident	"$Revision: 1.32 $"

#include "ml/ml.h"


/*
 * NOTE: These routines are coded conservatively in regards to nop's
 * and m[tf]c0 operations. Note: the routines are coded noreorder
 * to avoid the reorganizer moving C0 instructions around
 * that it doesn't realize are order dependent.
 */


/*
 * invaltlb(i,j): Invalidate the ith ITLB entry in set j.
 * Called whenever a specific TLB entry needs to be invalidated.
 * Used by clock based routine to slowly flush all tlbs, and
 * also used in DEBUG to unmap uarea & kstack.
 */
LEAF(invaltlb_in_set)
	.set	noreorder
	DMFC0(v0,C0_SR)			# save SR and disable interrupts
	NOINTS(t1,C0_SR)
	DMFC0(t0,C0_TLBHI)		# save current TLBHI
	DMTC0(zero,C0_TLBLO)

	/* TFP HW requires that the virtual addresses in each set at a
	 * given index be unique.  So we create an invalid entry which
	 * has the set number included as a field in the VA.
	 * Also, place the bogus addresses in a portion of KV0 space
	 * which isn't used to avoid duplicate entries.
	 */
	dsll	t1,a1,8+PNUMSHFT	# set number shifted above index
	dsll	a0,PNUMSHFT		# convert index into virtual address
	dadd	a0,t1
	LI	t1, (KV0BASE>>48)	# Kernel private address space
	dsll	t1, 48
	dadd	a0, t1
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a0,C0_TLBHI)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set

	TLB_WRITER
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	DMTC0(v0,C0_SR)			# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(invaltlb_in_set)
/*
 * tlbwired(indx, tlbpid, vaddr, pte) -- setup wired ITLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- tlbpid -- pointer to array of context numbers to use (0..TLBHI_NPID-1)
 *                 indexed by cpuid.  EXCEPT if 0, tlbpid = 0.
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pte -- contents of pte
 */
#if DEBUG
LEAF(_tlbwired)
#else
LEAF(tlbwired)
#endif
	.set	noreorder
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsrl    a3,a3,32                # 4th arg - 1st pte
#endif
	DMFC0(v0,C0_SR)			# save SR and disable interrupts
	NOINTS(t1,C0_SR)
	DMFC0(t0,C0_TLBHI)		# save current TLBPID
	DMTC0(a2,C0_BADVADDR)
	.set	reorder
	beq	zero,a1,1f
#if MP
	CPUID_L	t1,VPDA_CPUID(zero)
	PTR_ADD	a1,t1
#endif
	lbu	a1,(a1)
1:
	TLBLO_FIX_HWBITS(a3)		# shift pfn field to correct position
	sll	a1,TLBHI_PIDSHIFT	# line up pid bits
	dsrl	a2,PNUMSHFT		# chop offset bits
	dsll	a2,PNUMSHFT
	or	a1,a2			# formatted tlbhi entry
	.set	noreorder
	DMTC0(a1,C0_TLBHI)		# set VPN and TLBPID
	DMTC0(a3,C0_TLBLO)		# set PPN and access bits
	DMTC0(zero,C0_TLBSET)		# wired entries are in Set 0
	TLB_WRITER			# drop it in
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	DMTC0(v0,C0_SR)			# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
#if DEBUG
	END(_tlbwired)
#else
	END(tlbwired)
#endif

/*
 * tlbdropin(tlbpid, vaddr, pte) -- random tlb drop-in
 * a0 -- tlbpid -- pointer to array of context numbers to use (0..TLBHI_NPID-1)
 *                 indexed by cpuid.  EXCEPT if 0, tlbpid = 0;
 *                 if 1, use current tlbpid.
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 * a2 -- pte -- contents of pte
 *
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
	DMFC0(t0,C0_SR)			# save SR and disable interrupts
	NOINTS(t2,C0_SR)
	DMFC0(t1,C0_TLBHI)		# save current pid
	.set	reorder
	beq	a0,zero,1f
	bne	a0,1,2f
	and	a0,t1,TLBHI_PIDMASK
	dsrl	a0,a0,TLBHI_PIDSHIFT	# line up pid bits
	b	1f
2:
#if MP
	CPUID_L	t2,VPDA_CPUID(zero)
	PTR_ADD	a0,t2
#endif
	lbu	a0,(a0)
	.set	noreorder
1:
	DMFC0(t2,C0_TLBSET)		# need to save current set
	DMTC0(a1,C0_BADVADDR)		# load vaddr into BADVADDR also
	.set	reorder
	TLBLO_FIX_HWBITS(a2)		# shift pfn to correct position
	sll	a0,TLBHI_PIDSHIFT	# align pid bits for entryhi
	dsrl	a1,PNUMSHFT		# chop offset bits from vaddr
	dsll	a1,PNUMSHFT
	or	a0,a1			# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(a0,C0_TLBHI)		# vpn and pid of new entry
	DMTC0(a2,C0_TLBLO)		# pte for new entry
	TLB_PROBE			# probe for stale value
	DMFC0(t3,C0_TLBSET)
	bgez	t3,3f			# sign bit clear => "hit", reuse it
	nop				# BDSLOT
	/* We "missed" in the TLB.  Write TLB using a random set.
	 * For now we make our job easy and only use sets 1,2 so we
	 * avoid the problem of checking for set 0 wired entries.
	 */
	bne	t2,zero,1f
	nop
	li	t2,1			# if set 0, bump to set 1
1:	DMTC0(t2,C0_TLBSET)
	/* We perform the TLB write here for both cases (i.e. hit or miss)
	 */
3:
	TLB_WRITER			# use random slot
	/* Return to the caller after restoring PID
	 */
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	DMTC0(t0,C0_SR)			# restore SR
	j	ra
	move	v0,zero			# BDSLOT
	.set	reorder
#if DEBUG
	END(_tlbdropin)
#else
	END(tlbdropin)
#endif

/*
 * set_tlbpid -- set current tlbpid and change
 * There are no wired tlb entries on TFP which are user specific
 * so there are no tlb pids to change.
 * For now, we set the IASID (Icache ASID) to the same value as
 * the ASID in TLBHI.
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 */
LEAF(set_tlbpid)
	.set	noreorder
	DMFC0(t0,C0_SR)
	NOINTS(t1,C0_SR)		# interrupts off
	sll	t2,a0,TLBHI_PIDSHIFT	# line up pid bits
	DMTC0(t2,C0_TLBHI)		# setup ASID
	DMTC0(t0,C0_SR)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(set_tlbpid)
/*
 * set_icachepid -- set current tlbpid 
 * a0 -- icachepid -- tlbcontext number to use (0..TLBHI_NPID-1)
 */
LEAF(set_icachepid)
	.set	noreorder
	dsll	t2,a0,40
	DMTC0(t2,C0_ICACHE)		# setup IASID
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(set_icachepid)
/*
 * set_kptbl -- set current kernel global base (i.e. kptbl)
 */
LEAF(set_kptbl)
	.set	noreorder
	DMTC0(a0,C0_KPTBL)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(set_kptbl)

/*
 * tlbfind(tlbpid, vaddr, &low) -- find a tlb entry corresponding to vaddr
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 * a2 -- ptr to hold tlblo value
 *
 * Returns -1 if not in tlb else TLBSET register as well as tlblo
 */
LEAF(tlbfind)
	.set	noreorder
	DMFC0(t0,C0_SR)			# save SR and disable interrupts
	NOINTS(t2,C0_SR)
	DMFC0(t1,C0_TLBHI)		# save current pid
	.set 	reorder
	dsll	a0,TLBHI_PIDSHIFT	# align pid bits for entryhi
	dsrl	a1,PNUMSHFT		# chop offset bits from vaddr
	dsll	a1,PNUMSHFT
	or	t2,a0,a1		# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# vpn and pid of new entry
	DMFC0(t3,C0_TLBSET)		# save SET for restore after probe
	TLB_PROBE			# probe for address
	DMFC0(v0,C0_TLBSET)
	bltz	v0,1f			# not found
	nop				# BDSLOT

	/* found it - return ENTRYLo */
	TLB_READ			# read slot
	DMFC0(a0,C0_TLBLO)
	dsrl	a0,TLBLO_HWBITSHIFT	# shift back to pte format
			/* Is this okay? */
#ifdef PTE_64BIT
	sd	a0, 0(a2)
#else
	sw      a0, 0(a2)
#endif
	b       2f
	nop
1:
	li      v0,-1
2:
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	DMTC0(t0,C0_SR)			# restore SR
	DMTC0(t3,C0_TLBSET)		# restore SET value
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
	and	v0,TLBHI_PIDMASK
	j	ra
	srl	v0,TLBHI_PIDSHIFT	# BDSLOT - line up pid bits
	.set 	reorder
	END(tlbgetpid)

/*
 * icachegetpid - return current icachepid
 */
LEAF(icachegetpid)
	.set    noreorder
	DMFC0(v0,C0_ICACHE)		# get current icachepid
	j	ra
	dsrl	v0,40			# BDSLOT - shift ICACHEPID
	.set 	reorder
	END(icachegetpid)

/*
 * getrand - return random number
 * This routine is here since on other machines we return the contents
 * of the C0_RANDOM register.
 */
LEAF(getrand)
	.set    noreorder
	DMFC0(v0,C0_COUNT)		# get current counts register
	.set 	reorder
	j	ra
	END(getrand)

/*
 if TFP
 * tlbgetent(ind, set, *hi, *lo)
 * Get the contents of tlb registers for given index and set
 *
 */
LEAF(tlbgetent)
	.set	noreorder
	DMFC0(t1,C0_TLBHI)		# save current pid
	dsll	a0,PNUMSHFT		# convert index into virtual address
	DMTC0(zero,C0_TLBHI)		# clear pid
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set
	TLB_READ			# read entry to entry hi/lo
	DMFC0(v0,C0_TLBHI)		# copy into v0
	sd	v0, 0(a2)		# save in memory
	DMFC0(v0,C0_TLBLO)		# copy into v0
	sd	v0, 0(a3)		# save in memory
	DMTC0(t1,C0_TLBHI)		# restore current pid
	.set 	reorder
	j	ra

	END(tlbgetent)

	.data
lmsg:	.asciiz	"tfptlb.s"

