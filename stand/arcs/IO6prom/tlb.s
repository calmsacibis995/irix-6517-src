/***********************************************************************\
*       File:           tlb.s                                           *
*                                                                       *
*       Subroutines for dropping in TLB entries and using a kernel	*
*	virtual mapping for jumping between two copies of the PROM	*
*	code (one in the PROM and one in RAM).				*
*                                                                       *
\***********************************************************************/

#ident "$Revision: 1.18 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>

	.set	noreorder

#define PAGE_SIZE_SHFT		20		/* 1 MB */
#define PAGE_SIZE		(1 << PAGE_SIZE_SHFT)

#define BIT_RANGE(hi, lo) \
        ((0xffffffffffffffff >> (63 - (hi))) & (0xffffffffffffffff << (lo)))

/* In T5, Vaddr = 44 bits */
#define	VA_MASK			0xfffffffffff
/* uses three entries. 59, 58, 57 */
#define	IO6_TLB_INDEX		59


/*
 * void tlb_wire(indx, asid, vaddr, pte0, pte1, pgmask)
 *
 * This is a simplified version of tlbwired from irix/kern/ml/tlb.s.
 *
 * a0 -- indx   -- tlb entry index
 * a1 -- asid   -- context number to use (0..TLBHI_NPID-1)
 * a2 -- vaddr  -- virtual address (could have offset bits)
 * a3 -- pte0   -- contents of first pte
 * a4 -- pte1   -- contents of second pte
 * a5 -- pgmask -- page mask (size)
 */

LEAF(tlb_wire)
	and	a2, TLBHI_VPNMASK	# chop offset bits
	or	a1, a2			# formatted tlbhi entry
	DMTC0(a1, C0_TLBHI)		# set VPN and ASID
	DMTC0(a3, C0_TLBLO)		# set PPN and access bits
	DMTC0(a4, C0_TLBLO_1)		# set PPN and access bits for 2nd pte.
	mtc0	a0, C0_INX		# set INDEX to wired entry
	NOP_1_1
	c0	C0_WRITEI		# drop it in
	NOP_0_1
	j	ra
	nop				# BDSLOT
	END(tlb_wire)

/*
 * ulong tlb_get_enhi(int index)
 *
 *   Reads a specified TLB index's TLBHI
 */

LEAF(tlb_get_enhi)
	MTC0(a0, C0_INX)
	tlbr
	DMFC0(v0, C0_TLBHI)
	j	ra
	 nop
	END(tlb_get_enhi)

/*
 * ulong tlb_get_enlo(int index, int entry)
 *
 *   Reads a specified TLB index's TLBLO0 or TLBLO1
 */

LEAF(tlb_get_enlo)
	MTC0(a0, C0_INX)
	tlbr
	bnez	a1, 1f
	 nop
	MFC0(v0, C0_TLBLO_0)
	j	ra
	 nop
1:
	MFC0(v0, C0_TLBLO_1)
	j	ra
	 nop
	END(tlb_get_enlo)

/*
 * tlb_setup(nasid_t nasid)
 *
 *   Switches the T5 instruction source from physical to virtual mode
 *   so we can run the same image on any node without relocation.
 *
 *   drops in three entries in the tlb for 1MB page sizes each at
 *   indices 59, 58 & 57. sets asid = 0. turns on global bit
 *   maps 0xc000000011e00000 to 0xa800000001e00000 because the kernel
 *   maps itself in upto 0xc000000002000000 and there must be a 
 *   different virtual mapping for the io6prom. The last 7 bits
 *   must be the same for intial jals to work
 *
 *   uses registers s0, s1, s2, s3. also assumes tlb_wire doesn't clobber
 *   a few a* registers
 *
 */

LEAF(tlb_setup)
	move	s0, ra
	move	s3, a0

	dla	t3, start
	dsrl	t3, 56			# Just look at the top byte
	li	t0, 0xc0
	bne	t0, t3, no_need		# If we're not mapped, just return
	nop

	dli	t0, BIT_RANGE(PAGE_SIZE_SHFT, 0);
	mtc0	t0, C0_PGMASK		# Set page size
	mtc0	zero, C0_FMMASK		# Do not mask any bits on TLB writes.

	# Set up TLB entry 0 with ASID 0 to map 0xc000000011e00000 to
	#   uncached PROM at 0xa800000001e00000

	dli	a0, IO6_TLB_INDEX	# Pick a safe TLB index
	move	a1, zero		# ASID 0
	dli	a2, 0xc000000000000000	# virtual address

	dli	a4, TLBLO_D | TLBLO_V | TLBLO_G | TLBLO_EXLWR

	dla     t1, start		# prom link address
	dli     t2, VA_MASK 		# per node address space mask
	and     t1, t2			# t1 = Offset into node memory
	or	a2, t1			# Actual linked virtual address

	move	s1, a2			# needed after tlb_wire

#ifdef SN0
	dla	t1, IO6PROM_BASE
	dli	v0, NODE_ADDRSPACE_MASK # per node address space mask
	and	t1, v0			# t1 = Offset into node memory
#endif /* SN0 */
	dsll	s3, NASID_SHFT		# Shift it into place for an address
	or	t1, s3			# Physical load address of prom
	dsrl	t1, TLBLO_PFNSHIFT	# Put virtual address into place
	dli	t3, TLBLO_PFNMASK
	and	t1, t3			# Mask off any extraneous bits
	or	a3, a4, t1		# or pfn bits into tlblo0 value

	dli	t3, PAGE_SIZE >> TLBLO_PFNSHIFT	# Add one to the pfn
	dadd	a4, a3, t3

	move	s2, a4			# needed after tlb_wire

	jal	tlb_wire
	nop

	/* Drop in the second set of entries */
	dsubu	a0, 1
	move	a1, zero
	move	a2, s1
	dli	t3, PAGE_SIZE << 1
	daddu	a2, t3
	move	s1, a2
	dli	t3, PAGE_SIZE >> TLBLO_PFNSHIFT	# Add one to the pfn
	daddu	a3, s2,	t3
	daddu	a4, a3, t3

	move	s2, a4			# needed after tlb_wire

	jal	tlb_wire
	nop

#ifdef SN0XXL
	/* Drop in the third set of entries */
	dsubu	a0, 1
	move	a1, zero
	move	a2, s1
	dli	t3, PAGE_SIZE << 1
	daddu	a2, t3
	move	s1, a2
	dli	t3, PAGE_SIZE >> TLBLO_PFNSHIFT	# Add one to the pfn
	daddu	a3, s2,	t3
	daddu	a4, a3, t3

	move	s2, a4			# needed after tlb_wire

	jal	tlb_wire
	nop
#endif

	/* Drop in the third (fourth if XXL) set of entries */
	dsubu	a0, 1
	move	a1, zero
	move	a2, s1
	dli	t3, PAGE_SIZE << 1
	daddu	a2, t3
	dli	t3, PAGE_SIZE >> TLBLO_PFNSHIFT	# Add one to the pfn
	daddu	a3, s2,	t3
	daddu	a4, a3, t3

	jal	tlb_wire
	nop

	li	v0, 0			# Set ASID
	DMTC0(v0, C0_TLBHI)		# Static enable latches are set now.

	# Adjust return address from cached unmapped space
	# (0xa800000001e00000) to TLB-mapped space (0xc000000011e00000)

	dli	t0, 0xfffffff
	and	s0, t0
	dli	t0, 0xc000000010000000
	or	s0, t0

no_need:

	j	s0
	nop
	END(tlb_setup)



/*
* void slave_print_mod_num(nasid_t nasid)
*
* This is launched on a remote cpu to print out its module number on its elsc.
* It calls tlb_setup, prints the module number and then jumps back to the
* slave loop. This is necessary because tlb_setup wants to jump back to a
* mapped address and the slave loop is unmapped.
*/   
LEAF(slave_print_mod_num)

	jal	tlb_setup
	nop
	
	move	a0,	zero	# not master
	jal	elsc_print_mod_num
	nop
	
	dli	s0, IP27PROM_SLAVELOOP
	
	jalr	s0
	nop	

	END(slave_print_mod_num)

/*
 * Function:            slave_elsc_module_set -> launched slave sets moduleid
 * Args:                master_naaid(a0) -> nasid of master where code sits
 *                      module(gp) -> moduleid to set
 * Returns:             Nothing
 */

LEAF(slave_elsc_module_set)

        jal     tlb_setup
        nop

        move    a0,     gp
        move    gp,     zero
        jal     slave_set_moduleid
        nop

        dli     s0,     IP27PROM_SLAVELOOP

        jalr    s0
        nop

	END(slave_elsc_module_set)

/*
 * Function:            slave_checkclk_local -> check if clock ok in module
 * Args:                master_nasid(gp)
 * Returns:             Nothing
 */

LEAF(slave_checkclk_local)

	move	s7, a0

        jal     tlb_setup
        nop

/*        move    a0,     gp */
	move	a0, s7

        move    gp,     zero
        jal     checkclk_local
        nop

        dli     s0,     IP27PROM_SLAVELOOP

        jalr    s0
        nop

        END(slave_checkclk_local)

#if IP27_CPU_EARLY_INIT_WAR
LEAF(ei_pwr_cycle_launch)

        jal     tlb_setup
        nop

        jal     ei_pwr_cycle_module
        nop

        dli     s0,     IP27PROM_SLAVELOOP

        jalr    s0
        nop

        END(ei_pwr_cycle_launch)
#endif
