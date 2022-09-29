
/*
 * cpu/tlb.s
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.3 $"


#include <sys/cpu.h>
#include <sys/sbd.h>
#include <asm.h>
#include <regdef.h>
#include "ip19.h"
#include "ml.h"


/*
 * ta_spl() -- reestablish desired sa lib status register
 * clear any pending write bus error interrupts
 * returns current sr
 */
LEAF(ta_spl)
	lw	zero, EV_CIPL124	# clear bus error interrupt
	j	ra
	END(ta_spl)


/*
 * clr_tlb_regs(void): Zero-out all tlb regs.  The tlb read/write 
 * instructions use the C0_ PGMASK, TLBHI, TLBLO_0, and TLBLO_1 as
 * destinations/sources.  Generally called from flushall_tlb().
 */
LEAF(clr_tlb_regs)
	.set	noreorder
	mtc0	zero, C0_TLBHI
	NOP_1_3
	mtc0	zero, C0_TLBLO_0
	NOP_1_3
	mtc0	zero, C0_TLBLO_1
	NOP_1_3
	mtc0	zero, C0_PGMASK
	NOP_1_3
	mtc0	zero, C0_INX
	NOP_1_3
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(clr_tlb_regs)


/*
 * settlbslot(indx, tlbpid, vaddr, pte0, pte1) -- set up specified TLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- tlbpid -- context number to use (0..TLBHI_NPID-1)
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pte0 -- contents of EntryLo0
 * 5th argument (sp+16) -- pte1 (contents of EntryLo1)
 */
LEAF(settlbslot)
	.set	noreorder
	mfc0	t0,C0_TLBHI		# save current TLBPID
	mfc0	v0,C0_SR		# save SR and disable interrupts
	mtc0	zero,C0_SR
	.set	reorder

	lw	t1,16(sp)		# 5th arg - 2nd pte
	and     a3,TLBLO_19HWBITS	# chop software bits from pte.
	and     t1,TLBLO_19HWBITS	# chop software bits from 2nd pte.
        and     a2,TLBHI_VPNMASK	# chop offset bits
	or      a1,a2			# formatted tlbhi entry
	.set    noreorder
	mtc0    a1,C0_TLBHI             # set VPN and TLBPID

	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	TLBLO_FIX_250MHz(C0_TLBLO_1)
        mtc0    a3,C0_TLBLO             # set PPN and access bits
        mtc0    t1,C0_TLBLO_1           # set PPN and access bits for 2nd pte.

	mtc0	a0,C0_INX		# set INDEX to desired entry
	NOP_1_1				# 1 clock delay tween mtc0 and tlbw
	c0	C0_WRITEI		# drop it in
	NOP_0_1				# 1 clock delay tween mtc0 and tlbw
	mtc0	t0,C0_TLBHI		# restore TLBPID
	mtc0	v0,C0_SR		# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(settlbslot)


/*
 * write_indexed_lo(indx, struct tlbptepairs *) -- setup ITLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- ptr to struct containing data to drop into tlblo_0 and tlblo_1
 */
LEAF(write_indexed_lo)
	.set	noreorder
	lw	t1, 0(a1)
	lw	t2, 4(a1)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	TLBLO_FIX_250MHz(C0_TLBLO_1)
	mtc0	t1, C0_TLBLO_0		# set PPN and access bits of even page
	mtc0	t2, C0_TLBLO_1		# and of odd page
	mtc0	a0, C0_INX		# set INDEX to wired entry
	NOP_1_1				# let index get through pipe
	c0	C0_WRITEI		# drop it in
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(write_indexed_lo)


/*
 * write_indexed_hi(indx, data) -- setup ITLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- data -- pre-formatted data to drop into tlbhi
 */
LEAF(write_indexed_hi)
	.set	noreorder
	mtc0	a1,C0_TLBHI		# set PPN and access bits
	mtc0	a0,C0_INX		# set INDEX to wired entry
	NOP_1_1				# get index through pipe
	c0	C0_WRITEI		# drop it in
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(write_indexed_hi)


/*
 * read_indexed_hi(indx) -- return contents of 'indx' slot ITLB entry
 * a0 -- indx -- tlb entry index
 */
LEAF(read_indexed_hi)
	.set	noreorder
	mtc0	a0,C0_INX		# set INDEX to wired entry
	NOP_1_4
	c0	C0_READI		# read into hi/lo pair
	NOP_1_4
1:
	mfc0	v0,C0_TLBHI		# return tlbhi
	nop
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(read_indexed_hi)


/*
 * read_indexed_lo(indx, struct tlbptepairs *) -- return contents of 
 *      TLBLO_0 and TLBLO_1 regs for entry
 * a0 -- indx -- tlb entry index
 * a1 -- ptr to struct (in caller's space) into which tlblo_0 and 
 * tlblo_1 are written
 */
LEAF(read_indexed_lo)
	.set	noreorder
	mtc0	a0,C0_INX		# set INDEX to wired entry
	NOP_1_1
	c0	C0_READI		# read into hi/lo pair
	NOP_1_3
	mfc0	t1, C0_TLBLO_0
	NOP_1_1				# 1 clock delay tween mfc0 and store
	sw	t1, 0(a1)
	mfc0	t1, C0_TLBLO_1
	NOP_1_1				# 1 clock delay tween mfc0 and store
	sw	t1, 4(a1)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(read_indexed_lo)


/*
 * matchtlb(rpid, vpage): probe for a single tlb entry in the ITLB.
 *
 *	Probe for RPID/VPAGE in TLB.  If it doesn't exist, all done.
 *	Return index of matching entry or -1 if no match.
 */
LEAF(matchtlb)
	.set	noreorder
	mfc0	t1,C0_TLBHI		# save TLBHI (for TLBPID)
	sll	t0,a1,TLBHI_VPNSHIFT	# construct TLBHI
	or	t0,a0			# add the pid (no shift for R4K)
	mfc0	v0,C0_SR		# disable interrupts.
	mtc0	zero,C0_SR
	mtc0	t0,C0_TLBHI
	NOP_1_3				# let tlbhi get through pipe
	c0	C0_PROBE		# probe for address
	NOP_0_2				# let probe write index reg
	mfc0	t0,C0_INX
	mtc0	t1,C0_TLBHI		# restore old TLBHI
	mtc0	v0,C0_SR		# restore sr and return
	j	ra
	move	v0,t0			# BDSLOT
	.set	reorder
	END(matchtlb)

LEAF(get_badvaddr)
        .set    noreorder
        mfc0    v0,C0_BADVADDR
        nop
        j       ra
        nop
        .set    reorder
        END(get_badvaddr)

