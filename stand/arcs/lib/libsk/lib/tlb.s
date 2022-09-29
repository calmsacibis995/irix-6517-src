/*
 * tlb.s -- tlb code
 */

#include "ml.h"
#include <sys/sbd.h>
#include <regdef.h>
#include <asm.h>

#define SC_TLBHI_VPNMASK	0xfff80000
	.text
#if !TFP

/*
 * get_tlblo(index) -- return entry low contents of tlb entry "index"
 */
LEAF(get_tlblo0)
XLEAF(get_tlblo)
	.set	noreorder
	MFC0(t0,C0_SR)			# save sr
	nop
	ori	t1,t0,SR_IE
	xori	t1,SR_IE
	MTC0(t1,C0_SR)			# disable interrupts
	nop
	DMFC0(t1,C0_TLBHI)		# save current pid
	MTC0(a0,C0_INX)			# drop it in C0 register
	nop
	c0	C0_READI		# read entry to entry hi/lo
	nop
	nop
	nop
	DMFC0(v0,C0_TLBLO)		# to return value
	DMTC0(t1,C0_TLBHI)		# restore current pid
	MTC0(t0,C0_SR)			# restore sr
	j	ra
	nop
	.set	reorder
	END(get_tlblo0)

#if R4000 || R10000
/*
 * get_tlblo1(index) -- return entry low1 contents of tlb entry "index"
 */
LEAF(get_tlblo1)
	.set	noreorder
	MFC0(t0,C0_SR)			# save sr
	nop

	ori	t1,t0,SR_IE
	xori	t1,SR_IE
	MTC0(t1,C0_SR)			# disable interrupts
	nop
	DMFC0(t1,C0_TLBHI)		# save current pid
	MTC0(a0,C0_INX)			# drop it in C0 register
	nop
	c0	C0_READI		# read entry to entry hi/lo
	nop
	nop
	nop
	DMFC0(v0,C0_TLBLO_1)		# to return value
	DMTC0(t1,C0_TLBHI)		# restore current pid
	MTC0(t0,C0_SR)			# restore sr
	j	ra
	nop
	.set	reorder
	END(get_tlblo1)


/*
 * get_pgmaski(index) -- return page mask associated with index
 */
LEAF(get_pgmaski)
	.set	noreorder
	MFC0(t0,C0_SR)			# save sr
	nop
	ori	t1,t0,SR_IE
	xori	t1,SR_IE
	MTC0(t1,C0_SR)			# disable interrupts
	nop
	DMFC0(t1,C0_TLBHI)		# save current pid
	MTC0(a0,C0_INX)			# drop it in C0 register
	nop
	c0	C0_READI		# read entry to entry hi/lo
	nop
	nop
	nop
	MFC0(v0,C0_PGMASK)
	nop
	DMTC0(t1,C0_TLBHI)		# restore current pid
	MTC0(t0,C0_SR)			# restore sr
	j	ra
	nop
	.set	reorder
	END(get_pgmaski)


/* read TLB page mask register */
LEAF(get_pgmask)
	.set	noreorder
	MFC0(v0,C0_PGMASK)
	nop
	j	ra
	nop
	.set	reorder
	END(get_pgmask)

/* write TLB page mask register */
LEAF(set_pgmask)
	.set	noreorder
	MTC0(a0,C0_PGMASK)
	j	ra
	nop
	.set	reorder
	END(set_pgmask)
#endif /* R4000 || R10000 */

/*
 * get_tlbhi(index) -- return entry high contents of tlb entry "index"
 */
LEAF(get_tlbhi)
	.set	noreorder
	MFC0(t0,C0_SR)			# save sr
	nop
	ori	t1,t0,SR_IE
	xori	t1,SR_IE
	MTC0(t1,C0_SR)			# disable interrupts
	nop
	DMFC0(t1,C0_TLBHI)		# save current pid
	MTC0(a0,C0_INX)			# drop it in C0 register
	nop
	c0	C0_READI		# read entry to entry hi/lo
	nop
	nop
	nop
	DMFC0(v0,C0_TLBHI)		# to return value
	DMTC0(t1,C0_TLBHI)		# restore current pid
	MTC0(t0,C0_SR)			# restore sr
	j	ra
	nop
	.set	reorder
	END(get_tlbhi)

/*
 * get_tlbpid() -- return current tlb pid
 */
LEAF(get_tlbpid)
	.set	noreorder
	DMFC0(v0,C0_TLBHI)		# to return value
	nop
	and	v0,TLBHI_PIDMASK
	srl	v0,TLBHI_PIDSHIFT
	j	ra
	nop
	.set	reorder
	END(get_tlbpid)

/*
 * set_tlbpid() -- set current tlb pid
 */
LEAF(set_tlbpid)
	.set	noreorder
	sll	a0,TLBHI_PIDSHIFT
	and	a0,TLBHI_PIDMASK
	DMTC0(a0,C0_TLBHI)		# set new pid
	j	ra
	nop
	.set	reorder
	END(set_tlbpid)

/*
 * probe_tlb(address, pid) -- return index for tlb probe of address:pid
 */
LEAF(probe_tlb)
	.set	noreorder
	MFC0(t0,C0_SR)			# save sr
	nop
	ori	t1,t0,SR_IE
	xori	t1,SR_IE
	MTC0(t1,C0_SR)			# disable interrupts
	nop
	DMFC0(t1,C0_TLBHI)		# save current pid
	and	a0,TLBHI_VPNMASK	# construct tlbhi for probe
	sll	a1,TLBHI_PIDSHIFT
	and	a1,TLBHI_PIDMASK
	or	a0,a1
	DMTC0(a0,C0_TLBHI)
	nop
	c0	C0_PROBE		# probe entry to entry hi/lo
	nop
	nop
	MFC0(v0,C0_INX)
	nop
	DMTC0(t1,C0_TLBHI)		# restore current pid
	MTC0(t0,C0_SR)			# restore sr
	j	ra
	nop
	.set	reorder
	END(probe_tlb)

/*
 * invaltlb(i): Invalidate the ith ITLB entry.
 * called whenever a specific TLB entry needs to be invalidated.
 */
LEAF(invaltlb)
	.set	noreorder
	LI	t2,K0BASE&TLBHI_VPNMASK
	MFC0(v0,C0_SR)			# save SR and disable interrupts
	nop
	ori	t1,v0,SR_IE
	xori	t1,SR_IE
	MTC0(t1,C0_SR)
	nop
	DMFC0(t0,C0_TLBHI)		# save current TLBHI
	DMTC0(t2,C0_TLBHI)		# invalidate entry
	DMTC0(zero,C0_TLBLO)
#if R4000 || R10000
	DMTC0(zero,C0_TLBLO_1)
#endif /* R4000 || R10000 */
	MTC0(a0,C0_INX)
	nop
	c0	C0_WRITEI
	nop
	DMTC0(t0,C0_TLBHI)
	MTC0(v0,C0_SR)
	j	ra
	nop
	.set	reorder
	END(invaltlb)

/*
 * tlbwired(indx, tlbpid, vaddr, pte, pte2) -- setup wired ITLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- tlbpid -- context number to use (0..TLBHI_NPID-1)
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pte -- contents of pte
 *
 * 5th argument (sp+16) -- pte2 -- contents of 2nd pte (R4000 only)
 */
LEAF(tlbwired)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)		# save current TLBPID
	MFC0(v0,C0_SR)			# save SR and disable interrupts
	nop
	ori	t1,v0,SR_IE
	xori	t1,SR_IE
	MTC0(t1,C0_SR)
	.set	reorder
#if R4000 || R10000
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	move	t1,ta0			# 5th arg - 2nd pte
#else
	lw	t1,16(sp)		# 5th arg - 2nd pte
#endif
#else
	sll	a1,TLBHI_PIDSHIFT	# line up pid bits
#endif
	and	a2,TLBHI_VPNMASK	# chop offset bits
	or	a1,a2			# formatted tlbhi entry
	.set	noreorder
	DMTC0(a1,C0_TLBHI)		# set VPN and TLBPID
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(a3,C0_TLBLO)		# set PPN and access bits
#if R4000 || R10000
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(t1,C0_TLBLO_1)		# set PPN and access bits for 2nd pte.
#endif
	MTC0(a0,C0_INX)			# set INDEX to wired entry
	nop
	c0	C0_WRITEI		# drop it in
	nop
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	MTC0(v0,C0_SR)			# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(tlbwired)

LEAF(tlbwired_sc)
	.set	noreorder
	mfc0	t0,C0_TLBHI		# save current TLBPID
	mfc0	v0,C0_SR		# save SR and disable interrupts
	mtc0	zero,C0_SR
	.set	reorder
#if R4000 || R10000
	lw	t1,16(sp)		# 5th arg - 2nd pte
#else
	sll	a1,TLBHI_PIDSHIFT	# line up pid bits
#endif
	and	a2,SC_TLBHI_VPNMASK	# chop offset bits
	or	a1,a2			# formatted tlbhi entry
	.set	noreorder
	mtc0	a1,C0_TLBHI		# set VPN and TLBPID
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	a3,C0_TLBLO		# set PPN and access bits
#if R4000 || R10000
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	t1,C0_TLBLO_1		# set PPN and access bits for 2nd pte.
#endif
	mtc0	a0,C0_INX		# set INDEX to wired entry
	nop
	c0	C0_WRITEI		# drop it in
	nop
	mtc0	t0,C0_TLBHI		# restore TLBPID
	mtc0	v0,C0_SR		# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(tlbwired_sc)

#if !R4000 && !R10000
/*
 * tlb_setup(ppn, vpn, index, ncache)
 * Make tlb entry for virtual page vpn mapped to physical page ppn
 * using tlb entry index.  
 */
LEAF(tlb_setup)
	.set	noreorder
	MFC0(v0,C0_SR)				# save SR and disable 
	MTC0(zero,C0_SR)			# interrupt
	nop
	DMFC0(t0,C0_TLBHI)			# save current TLBHI
	nop
	MTC0(a2,C0_INX)				# set index
	nop
	sll	a0,TLBLO_PFNSHIFT		# move pfn over
	or	a0,TLBLO_D|TLBLO_V|TLBLO_G	# or in D, V, and G bits
	beq	a3,zero,1f			# check if no-cache
	or	a0,TLBLO_N			# set nocache
1:
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(a0,C0_TLBLO)
	sll	a1,TLBHI_VPNSHIFT		# move vpn over
	DMTC0(a1,C0_TLBHI)
	nop
	c0	C0_WRITEI
	nop
	DMTC0(t0,C0_TLBHI)
	MTC0(v0,C0_SR)				# restore SR
	j	ra
	nop
	.set 	reorder
	END(tlb_setup)
#endif /* !R4000 && !R10000 */

#else  /* TFP */

/*
 * probe_tlb(address, pid) -- return index for tlb probe of address:pid
 * a0 -- address -- virtual address to map. Can contain offset bits
 * a1 -- pid -- tlbcontext number to use (0..TLBHI_NPID-1)
 *
 * Returns index. < 0 (NOT -1) on probe failure.
 */
LEAF(probe_tlb)
	.set	noreorder
	DMFC0(t0,C0_SR)			# save SR and disable interrupts
	DMFC0(t1,C0_TLBHI)		# save current pid
	LI	t2,SR_PAGESIZE
	MTC0_NO_HAZ(t2,C0_SR)
	NOP_MTC0_HAZ

	.set 	reorder
	dsll	a1,TLBHI_PIDSHIFT	# align pid bits for entryhi
	dsrl	a0,TLBHI_VPNSHIFT	# chop offset bits from vaddr
	dsll	a0,TLBHI_VPNSHIFT
	or	t3,a1,a0		# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(t3,C0_TLBHI)		# vpn and pid of new entry
	DMTC0(a0,C0_BADVADDR)		# need VA here too (low bits of VPN)
	DMFC0(ta0,C0_BADVADDR)		# save BADVADDR for restore after probe
	DMFC0(t2,C0_TLBSET)		# save SET for restore after probe
	TLB_PROBE			# probe for address
	DMFC0(v0,C0_TLBSET)		# return set number in v0
	DMTC0(ta0,C0_BADVADDR)		# restore BADVADDR
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	DMTC0(t2,C0_TLBSET)		# restore SET value
	DMTC0(t0,C0_SR)			# restore SR
	j	ra
	nop				# BDSLOT
	.set 	reorder
	END(probe_tlb)

/*
 * get_tlbpid() -- return current tlb pid
 */
LEAF(get_tlbpid)
	.set	noreorder
	DMFC0(v0,C0_TLBHI)		# to return value
	nop
	and	v0,TLBHI_PIDMASK
	srl	v0,TLBHI_PIDSHIFT
	j	ra
	nop
	.set	reorder
	END(get_tlbpid)

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
	sll	a0,TLBHI_PIDSHIFT	# line up pid bits
	and	a0,TLBHI_PIDMASK
	DMTC0(a0,C0_TLBHI)		# setup ASID
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(set_tlbpid)
#endif /* !TFP */
