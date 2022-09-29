/***********************************************************************\
*       File:           tlb.s                                           *
*                                                                       *
*       Subroutines for dropping in TLB entries and using a kernel	*
*	virtual mapping for jumping between two copies of the PROM	*
*	code (one in the PROM and one in RAM).				*
*                                                                       *
\***********************************************************************/

#ident "$Revision: 1.14 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>

#include <sys/SN/addrs.h>
#include "ip27prom.h"
#include "libasm.h"

	.set	noreorder

#define PAGE_SIZE_SHFT		20	/* 1 MB, must be even */
#define PAGE_SIZE		(1 << PAGE_SIZE_SHFT)

#define ASID_PROM		1
#define ASID_RAM_UALIAS		2
#define ASID_RAM_CAC		3
#define ASID_RAM_CAC_NODE	4

#define GOTO_PROM						\
	dla	t1, 1f;						\
	or	t1, COMPAT_K1BASE;				\
	jr	t1;						\
	 nop;							\
1:

#define RET_MAPPED(r)						\
	and	r, 0x1fffffff;					\
	or	r, 0xc000000000000000;				\
	jr	r;						\
	 nop

/*
 * void tlb_wire(indx, asid, vaddr, pte0, pte1)
 *
 * 1.  Jumps to the PROM to avoid running mapped while mucking with TLBHI.
 * 2.  Drops in a TLB entry
 * 3.  Switches the current ASID to the new entry.
 *
 * a0 -- indx   -- tlb entry index
 * a1 -- asid   -- context number to use (0..TLBHI_NPID-1)
 * a2 -- vaddr  -- virtual address (could have offset bits)
 * a3 -- pte0   -- contents of first pte
 * a4 -- pte1   -- contents of second pte
 */

LEAF(tlb_wire)
	GOTO_PROM

	and	a2, TLBHI_VPNMASK	# chop offset bits
	or	a1, a2			# formatted tlbhi entry
	DMTC0(a1, C0_TLBHI)		# set VPN and ASID
	dmtc0	a3, C0_TLBLO		# set PPN and access bits
	dmtc0	a4, C0_TLBLO_1		# set PPN and access bits for 2nd pte.
	mtc0	a0, C0_INX		# set INDEX to wired entry
	c0	C0_WRITEI		# drop it in

	j	ra
	 nop
	END(tlb_wire)

/*
 * ulong tlb_get_enhi(int index)
 *
 *   Reads a specified TLB index's TLBHI
 */

LEAF(tlb_get_enhi)
	GOTO_PROM

	DMFC0(v1, C0_TLBHI)
	MTC0(a0, C0_INX)
	tlbr
	DMFC0(v0, C0_TLBHI)
	DMTC0(v1, C0_TLBHI)

	j	ra
	 nop
	END(tlb_get_enhi)

/*
 * ulong tlb_get_enlo(int index, int entry)
 *
 *   Reads a specified TLB index's TLBLO0 or TLBLO1
 */

LEAF(tlb_get_enlo)
	GOTO_PROM

	DMFC0(v1, C0_TLBHI)
	MTC0(a0, C0_INX)
	tlbr

	bnez	a1, 1f
	 nop

	MFC0(v0, C0_TLBLO_0)
	DMTC0(v1, C0_TLBHI)
	j	ra
	 nop

1:
	MFC0(v0, C0_TLBLO_1)
	DMTC0(v1, C0_TLBHI)
	j	ra
	 nop

	END(tlb_get_enlo)

/*
 * void tlb_flush(void)
 *
 *   Invalidates all TLB entries
 */

LEAF(tlb_flush)
	DMTC0(zero, C0_TLBLO_0)		# clear all bits in EntryLo 0
	DMTC0(zero, C0_TLBLO_1)		# clear all bits in EntryLo 1

	li	v0, NTLBENTRIES - 1	# v0 is set counter
	dli	a0, 0xc000000000000000	# Kernel Global region
	DMTC0(a0, C0_TLBHI)

1:
	DMTC0(v0, C0_INX)		# specify which TLB index
	tlbwi
	addi	v0, -1
	bgez	v0, 1b			# are we done with all indeces?
	 nop

	j	ra
	 nop
END(tlb_flush)

/*
 * tlb_prom
 *
 *   Sets up ASID 1 to map kernel virtual 0xc00000001fc00000 to uncached
 *   PROM in HSPEC (0x900000001fc00000), and makes 1 the current ASID.
 *   The D (dirty) bit is left off so illegal attempts to write the PROM
 *   through the TLB will be caught with a TLB exception.
 *
 *   The PROM is linked at the virtual address 0xc00000001fc00000, but
 *   actually resides at 0xffffffffbfc00000.  Even so, JAL will work
 *   before going virtual because it uses only the lower 28 bits of an
 *   address and jumps within the same 256 MB segment.  This is why we
 *   have the virtual address end with 0x1fc00000.
 *
 *   Note: Assumes the TLB has already been flushed (initializeCPU).
 */

LEAF(tlb_prom)
	move	t0, ra

	GOTO_PROM

	# Set PGMASK and FMMASK for the the life of IP27prom

	dli	v0, BIT_RANGE(PAGE_SIZE_SHFT, 13);
	mtc0	v0, C0_PGMASK
	mtc0	zero, C0_FMMASK		# Do not mask any bits on TLB writes.

	dli	a0, 62			# TLB index
	dli	a1, ASID_PROM
	dli	a2, 0xc00000001fc00000	# virtual address
	dli	a3, TLBLO_V | TLBLO_UNCACHED | \
		(0 << TLBLO_UATTRSHIFT) | /* IP27 HSPEC */ \
		((0x1fc00000		) >> TLBLO_PFNSHIFT)
	dli	a4, TLBLO_V | TLBLO_UNCACHED | \
		(0 << TLBLO_UATTRSHIFT) | /* IP27 HSPEC */ \
		((0x1fc00000 + PAGE_SIZE) >> TLBLO_PFNSHIFT)

	jal	tlb_wire
	 nop

	RET_MAPPED(t0)

	END(tlb_prom)

/*
 * tlb_ram_ualias
 *
 *   Sets up ASID 2 to map kernel virtual 0xc00000001fc00000 to uncached
 *   RAM at IP27PROM_BASE in UALIAS space, and makes 2 the current ASID.
 */

LEAF(tlb_ram_ualias)
	move	t0, ra

	GOTO_PROM

	dli	a0, 61			# TLB index
	dli	a1, ASID_RAM_UALIAS
	dli	a2, 0xc00000001fc00000	# virtual address
	dli	a3, TLBLO_D | TLBLO_V | TLBLO_UNCACHED | \
		(0 << TLBLO_UATTRSHIFT) | /* IP27 HSPEC */ \
		(K0_TO_PHYS(IP27PROM_BASE	     ) >> TLBLO_PFNSHIFT)
	dli	a4, TLBLO_D | TLBLO_V | TLBLO_UNCACHED | \
		(0 << TLBLO_UATTRSHIFT) | /* IP27 HSPEC */ \
		(K0_TO_PHYS(IP27PROM_BASE + PAGE_SIZE) >> TLBLO_PFNSHIFT)

	jal	tlb_wire
	 nop

	RET_MAPPED(t0)

	END(tlb_ram_ualias)

/*
 * tlb_ram_cac
 *
 *   Sets up ASID 3 to map kernel virtual 0xc00000001fc00000 to cached
 *   RAM at IP27PROM_BASE, and makes 3 the current ASID.
 */

LEAF(tlb_ram_cac)
	move	t0, ra

	GOTO_PROM

	dli	a0, 60			# TLB index
	dli	a1, ASID_RAM_CAC
	dli	a2, 0xc00000001fc00000	# virtual address
	dli	a3, TLBLO_D | TLBLO_V | TLBLO_EXLWR | \
		(K0_TO_PHYS(IP27PROM_BASE	     ) >> TLBLO_PFNSHIFT)
	dli	a4, TLBLO_D | TLBLO_V | TLBLO_EXLWR | \
		(K0_TO_PHYS(IP27PROM_BASE + PAGE_SIZE) >> TLBLO_PFNSHIFT)

	jal	tlb_wire
	 nop

	RET_MAPPED(t0)

	END(tlb_ram_cac)

/*
 * tlb_ram_cac_node
 *
 *   Sets up ASID 4 to map kernel virtual 0xc00000001fc00000 to cached
 *   RAM at IP27PROM_BASE plus node offset nasid (in a0), and makes 4
 *   the current ASID.
 */

LEAF(tlb_ram_cac_node)
	move	t0, ra

	GOTO_PROM

	dsll	t1, a0, NASID_SHFT

	dli	a0, 59			# TLB index
	dli	a1, ASID_RAM_CAC_NODE
	dli	a2, 0xc00000001fc00000	# virtual address

	dli	t2, K0_TO_PHYS(IP27PROM_BASE)
	or	t2, t1
	dsrl	t2, TLBLO_PFNSHIFT

	dli	a3, TLBLO_D | TLBLO_V | TLBLO_EXLWR
	or	a3, t2

	dli	t2, K0_TO_PHYS(IP27PROM_BASE + PAGE_SIZE)
	or	t2, t1
	dsrl	t2, TLBLO_PFNSHIFT

	dli	a4, TLBLO_D | TLBLO_V | TLBLO_EXLWR
	or	a4, t2

	jal	tlb_wire		# Destroys t1
	 nop

	RET_MAPPED(t0)

	END(tlb_ram_cac_node)

/*
 * tlb_to_prom_addr
 *
 *   If running from PROM (ASID=1), returns a0 unchanged.
 *   Otherwise converts it to a COMPAT_K1 address.
 */

LEAF(tlb_to_prom_addr)
	DMFC0(v1, C0_TLBHI)
	li	t0, ASID_PROM
	andi	v1, TLBHI_PIDMASK
	bne	v1, t0, 1f
	 move	v0, a0
	or	v0, COMPAT_K1BASE
1:
	j	ra
	 nop
	END(tlb_to_prom_addr)
