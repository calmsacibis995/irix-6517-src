/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * SN0 specific assembly routines
 */

#ident	"$Revision: 1.24 $"

#include <ml/ml.h>
#include <sys/loaddrs.h>
#include <sys/dump.h>
#include <sys/mapped_kernel.h>
#include <sys/sbd.h>
#include <sys/SN/klkernvars.h>

/* stubs that just return */
LEAF(dummy_func)
XLEAF(dcache_wb)                /* SN0 has coherent I/O - NOP */
XLEAF(dcache_wbinval)
XLEAF(dki_dcache_wb)
XLEAF(dki_dcache_wbinval)
XLEAF(dki_dcache_inval)
        j ra
END(dummy_func)


/*
 * all slave processors come here - assumed on boot stack
 */
BOOTFRM=	FRAMESZ((NARGSAVE+2)*SZREG)	# arg save + 5th/6th args to tlbwired
NESTED(bootstrap, BOOTFRM, zero)
	/* do some one-time only initialization */
	PTR_SUBU sp,BOOTFRM
	.set	noreorder

	/*
	 * Leave SR_BEV on so we can take exceptions before we hook the
	 * handlers.
	 */
	li	t0,SR_KADDR | SR_BEV
	MTC0(t0,C0_SR)

#ifdef MAPPED_KERNEL
	# Address of node 0 KLDIR structure's kern vars pointer
	LI	t0, KLDIR_OFFSET + (KLI_KERN_VARS * KLDIR_ENT_SIZE) + KLDIR_OFF_POINTER + K0BASE

	# Get our NASID, shift it, and OR it into the address.
	GET_NASID_ASM(t1)
	dsll	t1, NASID_SHFT
	or	t0, t1

	# t1 will be our pointer to the kern_vars structure.
	PTR_L	t1, 0(t0)

	# Load the read-write data and read-only data NASIDs from low memory
	NASID_L	a0, KV_RO_NASID_OFFSET(t1)
	NASID_L	a1, KV_RW_NASID_OFFSET(t1)

	jal	mapped_kernel_setup_tlb
	nop

	LA	t0, 1f			# Load the link address for label 1f
	jr	t0			# Jump to our link (mapped) address
	nop
1:
#endif /* MAPPED_KERNEL */

	/*
	 * Some of the calls below (size_2nd_cache on TFP for example) end up
	 * calling C code, so we need to init gp here.
	 */
	LA	gp,_gp

	.set	noreorder
	mtc0	zero,C0_WATCHLO		# No watch point exceptions.
	nop
	mtc0	zero,C0_WATCHHI
	li	a3,TLBPGMASK_MASK
	mtc0	a3,C0_PGMASK		# Set up pgmask register
	li	a3,NWIREDENTRIES
	mtc0	a3,C0_TLBWIRED		# Number of wired tlb entries

	# pairs of 4-byte ptes
	LI	v0,KPTEBASE<<(KPTE_TLBPRESHIFT+PGSHFTFCTR)
	DMTC0(v0,C0_EXTCTXT)
	LI	v0,FRAMEMASK_MASK
	DMTC0(v0,C0_FMMASK)
	DMTC0(zero,C0_TLBHI)

	.set	reorder

	j	cboot

	END(bootstrap)


/*
 * mapped_kernel_setup_tlb(text_nasid, data_nasid)
 *
 */
LEAF(mapped_kernel_setup_tlb)
#ifdef MAPPED_KERNEL
#ifdef R10000
	.set noreorder

	# Get basics ready for TLb entries
	dli	t0, 0xc000000000000000	# Virtual address base.
	LA	t1, _ftext		# Text link address

	# set up TLB page mask
	li	ta2,MAPPED_KERN_TLBMASK	# 16M pages for text and data
	mtc0    ta2, C0_PGMASK

#if SN0
	# Set up the physical addresses by shifting NASIDs into place
	dli	t2, NODE_ADDRSPACE_MASK # Get offset bits from link address
	and	t1, t2			# t1 = Offset into node memory
	dsll	a0, NASID_SHFT		# Shift text nasid into place
	dsll	a1, NASID_SHFT		# Same for data nasid
	or	a0, t1			# Physical load address of kernel text
	or	a1, t1			# Physical load address of kernel data
	# a0 = kernel text physical load address	
	# a1 = kernel data physical load address
	
#else
	Bomb!  Not implemented for non-NUMA machines yet.
#endif
	dsrl	a0, TLBLO_PFNSHIFT	# Put physical address into place
	dsrl	a1, TLBLO_PFNSHIFT	# Put physical address into place

	dli	t3, TLBLO_PFNMASK	# Mask off any extraneous bits
	and	a0, t3
	and	a1, t3

	dli	t3, TLBLO_V | TLBLO_G | TLBLO_EXLWR
					# Basis for tlblo entries
	or	a0, t3			# OR pfn bits into tlblo0 values
	or	a1, t3
	or	a1, TLBLO_D		# Allow writes to the data page

	and	t0, TLBHI_VPNMASK	# chop offset bits off of VPN
					# (using ASID 0)
	DMTC0(t0, C0_TLBHI)		# set VPN and ASID
	mtc0	a0, C0_TLBLO		# set up first PTE
	mtc0	a1, C0_TLBLO_1		# set up second PTE
	li	t0, KMAP_INX
	mtc0	t0, C0_INX		# set INDEX to wired entry
	tlbwi				# drop it in
	nop
	jr	ra
	nop
	
	.set reorder
#else
	Bomb!  Need to implement mapping code for other processors.
#endif /* R10000 */
#endif /* MAPPED_KERNEL */

	j	ra
	END(mapped_kernel_setup_tlb)

#if defined (FORCE_ERRORS)
	
LEAF(forcebad_icache_parity)
	.set noreorder
	PTR_SUBU	sp, 32
	REG_S		ra, 24(sp)
	REG_S		a0, 16(sp)
	LA	a1, 2f
	and	a1, TO_PHYS_MASK
	or	a1, K1BASE
	jr	a1
	 nop
2:
	li	v0, CACH_PI + C_ILD
	sw	v0, COP_OP(a0)
	jal	cacheOP
	 nop
	REG_L	a0, 16(sp)
	lw	v0, COP_ECC(a0)
	xori	v0, 1
	sw	v0, COP_ECC(a0)
	li	v0, CACH_PI + C_ISD
	sw	v0, COP_OP(a0)

	jal	cacheOP
	 nop
	REG_L	ra, 24(sp)
	PTR_ADDU sp, 32
	j	ra
	 nop

	.set reorder
	END(forcebad_icache_parity)
	
#endif /* FORCE_ERRORS */	


/*
 * The prom has already saved the registers in low memory...
 */
LEAF(nmi_dump)
	.set noreorder

	LI	k0,SR_KADDR|SR_DEFAULT	# make sure page size bits
	MTC0(k0, C0_SR)			# are on

	ld	t1, NPDA(zero)		# Get the current nodepda
					# pointer

					# Get the address of the 
					# dump count within the pda
	LI	a0, NPDA_DUMP_COUNT_OFFSET
	PTR_ADD	a0, t1
	jal	atomicAddInt
	 li	a1, 1			# (BD)

1:	bne	v0, 1, 1b
	 nop				# (BD)

	ld	t1, NPDA(zero)		# Get the current nodepda
					# pointer

					# Get the dump stack offset
					# within the nodepda
	PTR_L	sp, NPDA_DUMP_STACK_OFFSET(t1)
	LI	t0, DUMP_STACK_SIZE - 16
	PTR_ADD	sp, t0			# Set our stack pointer.
	LA	gp, _gp

	j	cont_nmi_dump
	 nop				# (BD)

	END(nmi_dump)

	.data
dump_count:	.word	0
