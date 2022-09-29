/*
 * IP32 specific assembly routines; cpuid always 0, also make semaphore
 * macros a no-op.
 */
#ident "$Revision: 1.10 $"

#include "ml/ml.h"

/*	dummy routines whose return value is unimportant (or no return value).
	Some return reasonable values on other machines, but should never
	be called, or the return value should never be used on other machines.
*/
LEAF(dummy_func)
XLEAF(check_delay_tlbflush)
XLEAF(check_delay_iflush)
XLEAF(da_flush_tlb)
XLEAF(dma_mapinit)
XLEAF(apsfail)
XLEAF(disallowboot)
XLEAF(rmi_fixecc)
XLEAF(vme_init)
XLEAF(vme_ivec_init)
XLEAF(wd93_init)

	j ra
END(dummy_func)

/*
 * Performance: just do a write back on the R4600 & R5000!!!
 */
LEAF(dki_dcache_wb)
XLEAF(dcache_wb)
	LI	a2,CACH_DCACHE|CACH_WBACK|CACH_IO_COHERENCY
	j	cache_operation
	END(dki_dcache_wb)

#ifdef MH_R10000_SPECULATION_WAR
LEAF(dcache_wbinval)
	j	dki_dcache_wbinval
	END(dcache_wbinval)

#define dki_dcache_wbinval _dki_dcache_wbinval	
#define dki_dcache_inval _dki_dcache_inval	
#endif /* MH_R10000_SPECULATION_WAR */
	
LEAF(dki_dcache_wbinval)
#ifndef MH_R10000_SPECULATION_WAR
XLEAF(dcache_wbinval)
#endif /* MH_R10000_SPECULATION_WAR */
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_WBACK|CACH_IO_COHERENCY
	j	cache_operation
	END(dki_dcache_wbinval)

LEAF(dki_dcache_inval)
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_IO_COHERENCY
	j	cache_operation
	END(dki_dcache_inval)

/*
 * Fast fill using R5000 create dirty exclusive, called from bzero
 * if the destination address and length are 32B cache line aligned.
 *    (this routine is ~84% faster than bzero on a 180Mhz R5000)
 */
LEAF(__cdx_blkfill)
	.set noreorder
	beqz	a1,2f
	addu	v0,a0,a1
	addiu	v0,-32
1:
#if defined(_R5000_CDX_WAR) && !defined(_DISABLE_CDX)
	/* Prevents occasional corruption of the first cache line of the page.
	* See bug 451386 for details. -wsm12/18/96.
	*/
	cache   CACH_PD|C_HWB,0(a0)
#endif /* _R5000_CDX_WAR && _DISABLE_CDX */
#if !defined(_DISABLE_CDX)
	cache   CACH_PD|C_CDX,0(a0)
#endif /* _DISABLE_CDX */
	sd	a2,0(a0)
	sd	a2,8(a0)
	sd	a2,16(a0)
	sd	a2,24(a0)
	bnel	a0,v0,1b
	addiu	a0,32
2:
	.set reorder
	j	ra
	END(__cdx_blkfill)

/*
 * Fast copy using R5000 create dirty exclusive, called from bcopy
 * if the destination address and length are 32B cache line aligned.
 * 	performs copy at approx 533MB/second on 200Mhz R5000
 *	note: memory latency assumed to be zero above...
 */
LEAF(__cdx_blkcopy)
	.set noreorder
	beqz	a2,2f
	sltu	t1,a0,a1			# if src < dst
	bnez	t1,__cdx_blkcopy_backwards	# then do backwards block copy
	addu	v0,a1,a2
1:
#if defined(_R5000_CDX_WAR) && !defined(_DISABLE_CDX)
	/* Prevents occasional corruption of the first cache line of the page.
	* See bug 451386 for details. -wsm12/18/96.
	*/
	cache   CACH_PD|C_HWB,0(a1)
#endif /* _R5000_CDX_WAR && !_DISABLE_CDX */
	ld	t0,0(a0)
#if !defined(_DISABLE_CDX)
	cache   CACH_PD|C_CDX,0(a1)
#endif /* _DISABLE_CDX */
	ld	t1,8(a0)
	sd	t0,0(a1)
	ld	t0,16(a0)
	sd	t1,8(a1)
	ld	t1,24(a0)
	sd	t0,16(a1)
	sd	t1,24(a1)
	addiu	a1,32
	bnel	a1,v0,1b
	addiu	a0,32

2:
	j	ra
	nop
	
__cdx_blkcopy_backwards:
	move	v0,a1				# save a1 for ref.
	PTR_ADDU	a0,a2			# start with ending addresses
	PTR_ADDU	a1,a2

1:	
#if defined(_R5000_CDX_WAR) && !defined(_DISABLE_CDX)
	/* Prevents occasional corruption of the first cache line of the page.
	* See bug 451386 for details. -wsm12/18/96.
	*/
	cache   CACH_PD|C_HWB,-32(a1)
#endif /* _R5000_CDX_WAR && !_DISABLE_CDX */
	ld	t0,-8(a0)	# load 4th 8B
#if !defined(_DISABLE_CDX)
	cache   CACH_PD|C_CDX,-32(a1)
#endif /* _DISABLE_CDX */
	ld	t1,-16(a0)	#load 3rd 8B
	sd	t0,-8(a1)	#store 4th 8B
	ld	t0,-24(a0)	#load 2nd 8B 
	sd	t1,-16(a1)	#store 3rd 8B
	ld	t1,-32(a0)	#load 1st 8B
	sd	t0,-24(a1)	#store 2nd 8B
	sd	t1,-32(a1)	#store 1st 8B
	addi	a1,-32
	bnel	a1,v0,1b
	addi	a0,-32
	
2:
	j	ra
	nop
	.set	reorder
	END(__cdx_blkcopy)

/*
 * The CRIME ASIC sysad write buffer is not hdwr I/O coherent!!!
 *
 * Read a cpu interface register to flush the sysad write buffer
 * for things like IO coherency (we read the CRIME ID register).
 */
LEAF(__sysad_wbflush)
	sync
	CLI	v0,PHYS_TO_COMPATK1(CRM_ID)
	ld	v0,0(v0)
	j	ra
	END(__sysad_wbflush)

/* dummy routines that return 0 */
LEAF(dummyret0_func)

XLEAF(vme_adapter)
XLEAF(is_vme_space)
XLEAF(getcpuid)
XLEAF(disarm_threeway_trigger)
XLEAF(threeway_trigger_armed)
XLEAF(is_fullhouse)
#ifdef DEBUG
XLEAF(getcyclecounter)
#endif /* DEBUG */

/* Semaphore call stubs */
XLEAF(appsema)
XLEAF(apvsema)
XLEAF(apcvsema)
	move	v0,zero
	j ra
END(dummyret0_func)

/* dummy routines that return 1 */
LEAF(dummyret1_func)
XLEAF(apcpsema)	/* can always get on non-MP machines */
	li	v0, 1
	j ra
END(dummyret1_func)

#if 0
EXPORT(prom_dexec)
	li	v0,PROM_DEXEC
	j	v0

/*	load the nvram table from the prom.
	prom_nvram_tab(addr, size)
	This means we don't embed the info of the current layout
	anywhere but in the prom.
*/
EXPORT(prom_nvram_tab)
	li	v0,PROM_NVRAM_TAB
	j	v0
#endif

/* return time stamp */
LEAF(_get_timestamp)
	j	get_r4k_counter
END(_get_timestamp)	

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
LEAF(read_reg64_sign_extend)
XLEAF(read_reg64)
	ld	a0,0(a0)
	dsra32	v0,a0,0
	dsll32	a0,0
	.set noreorder
	j	ra
	dsra32	v1,a0,0
	.set reorder
	END(read_reg64_sign_extend)

#if 0
LEAF(read_reg64)
	ld	a0,0(a0)
	dsrl32	v0,a0,0
	dsll32	a0,0
	.set noreorder
	j	ra
	dsrl32	v1,a0,0
	.set reorder
	END(read_reg64)
#endif

LEAF(write_reg64)
	dsll32	a1,0
	dsrl32	a1,0
	dsll32	a0,0
	or	a0,a1
	.set noreorder
	j	ra
	sd	a0,0(a2)
	.set reorder
	END(write_reg64)

#endif /* _MIPS_SIM == _MIPS_SIM_ABI32 */
	
#ifdef RESET_DUMPSYS
LEAF(flash_jump_vector)
	.set noat
	la	AT,reset_dumpsys
	ori	AT,K1BASE>>16
	.set noreorder
	jalr	AT
	nop
EXPORT(flash_jump_vector_end)
	nop
	.set reorder
	.set at
	END(flash_jump_vector)
#endif /* RESET_DUMPSYS */

/*
 * used by ECC correction routines to clean memory errors.
 * CRIME will return corrected data when a doubleword with
 * a byte containing a single bit ECC error is read.  Read
 * the doubleword and then write the corrected data back.
 */
LEAF(scrub_memory)
	.set noreorder
	ld ta0,0(a0)
	j ra
	sd ta0,0(a0)
	.set reorder
	END(scrub_memory)

/*
 * tlbgetlo(tlbpid, vaddr) -- find a tlb entry corresponding to vaddr
 * 	same as tlbfind() but doesn't modify SR
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 *
 * Returns -1 if not in tlb else TLBLO 0 or 1 register
 */
LEAF(tlbgetlo)
	.set	noreorder
	mfc0	t1,C0_TLBHI		# save current pid
	.set 	reorder
	sll	a0,TLBHI_PIDSHIFT	# align pid bits for entryhi
	andi	v1,a1,NBPP		# Save low bit of vpn
	and	a1,TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	t2,a0,a1		# vpn/pid ready for entryhi
	.set	noreorder
	mtc0	t2,C0_TLBHI		# vpn and pid of new entry
	NOP_1_3				# let tlbhi get through pipe
#if defined(PROBE_WAR)
	la	ta1,1f
	or	ta1,K1BASE
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
	NOP_1_4				# let probe write index reg
	mfc0	t3,C0_INX	
	NOP_1_1				# wait for t3 to be filled
	bltz	t3,1f			# not found
	li	v0,-1			# BDSLOT

	/* found it - return ENTRYLo */
	c0	C0_READI		# read slot
	NOP_1_3				# let readi fill tlblo reg
	beqz	v1,lo0
	nop
	mfc0	v0,C0_TLBLO_1
	b	1f
	nop
lo0:
	mfc0	v0,C0_TLBLO_0

1:
	mtc0	t1,C0_TLBHI		# restore TLBPID
	j	ra
	nop				# BDSLOT
	.set 	reorder
	END(tlbgetlo)


#ifdef R10000
/* Return size of secondary cache (really max cache size for start-up) */
LEAF(getcachesz)
        .set	noreorder
	mfc0	v1,C0_CONFIG
#ifdef R4000
	and	v1,CONFIG_R10K_SS
	dsrl	v1,CONFIG_R10K_SS_SHFT
#else /* R4000 */
	and	v1,CONFIG_SS
	dsrl	v1,CONFIG_SS_SHFT
#endif /* R4000 */
	dadd	v1,CONFIG_SCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
END(getcachesz)

/* return the content of the R10000 C0 config register */
LEAF(get_r10k_config)
        .set    noreorder
        mfc0    v0,C0_CONFIG
        .set    reorder
        j       ra
END(get_r10k_config)

#ifdef MH_R10000_SPECULATION_WAR
LEAF(set_r10k_config)
        .set    noreorder
        mtc0    a0,C0_CONFIG
        .set    reorder
        j       ra
END(set_r10k_config)
#endif /* MH_R10000_SPECULATION_WAR */
#endif /* R10000 */

#ifdef FAST_TEXTURE_LOAD
/*
 * Fast texture TLB loads.
 *
 * Currently this is not defined since it wasn't working
 * in bonsai and would take some time to port.
 */
EXPORT(handle_wade)
	.set	noreorder
	.set	noat

	b	longway		# Early branch until PerfTown hang fixed

	/* wire in upage/user kernel stack */
	SVSLOTS
#if R4000 || R10000
#if USIZE == 2
	# KERNELSTACK & UADDR in same page pair, with the former at the
	# even page address so load it into EntryHi (as VPN2)
	LI	k0, KERNELSTACK-NBPP
	PTE_L	k1, VPDA_UPGLO(zero)
	DMFC0(AT, C0_TLBHI)		# LDSLOT save virtual page number/pid
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	k1, C0_TLBLO_1		# LDSLOT set pfn and access bits
	DMTC0(k0, C0_TLBHI)		# set virtual page number
	li	k0, ((UKSTKTLBINDEX)<<TLBINX_INXSHIFT)	# base of wired entries
	and	k1, TLBLO_V		# NOP SLOT check valid bit
	beq	k1, zero, _baduk	# panic if not valid
	/* now do kernel stack */
	PTE_L	k1, VPDA_UKSTKLO(zero)
	mtc0	k0, C0_INX		# set index to wired entry
	TLBLO_FIX_HWBITS(k1)
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	k1, C0_TLBLO		# LDSLOT set pfn and access bits
	and	k1, TLBLO_V		# NOP SLOT check valid bit
	c0	C0_WRITEI		# write entry
	beq	k1, zero, _baduk	# panic if not valid
	DMTC0(AT, C0_TLBHI)		# BDSLOT restore pid
#endif	/* USIZE == 2 */
#endif	/* R4000 || R10000 */

	NOP_0_4
	DMFC0(AT,C0_BADVADDR)			# Get faulting virtual address
	NOP_0_4
	bgez	AT, longway			# Above the texture load address
	addi	k1,AT,0x1000
	bltz	k1,longway			# Below the texture load address
	sub	k0,k1,CRM_TEX_MAX_RESIDENT
	bgez	k0, longway			# Above maximum no. of textures
	lw	k0,u+U_TEX_TLB			# Point to Texture Table
	beqz	k0, longway
	nop
	sw	k1,-4(k0)			# Save the new texture id
	sll	k1,2				# Convert byte to ptr offset
	add	k0,k0,k1			# Address of texture list
	lw	k1,0(k0)
	beqz	k1, longway			# Empty slot
	# Check FIFO level
	la	AT, CRM_TEX_IBSTAT_ADDR
	lw	k0, 0(AT)
	srl	k0, CRM_TEX_FIFOSRL
	andi	k0, CRM_TEX_FIFOMASK
	bgtu	k0, CRM_TEX_FIFOMAX, longway	# Fifo level too high
	nop
	# Load texture TLB
	la	k0, CRM_TEX_TLB_FIRST_ADDR
1:
	ld	AT, CRM_TEX_TLB_DATA_OFFSET(k1)
	nop
	addiu	k1, CRM_TEX_TLB_STRUCT_SIZE
	sd	AT, 0(k0)
	addiu	k0, 8
	bne	k0, CRM_TEX_TLB_LAST_ADDR, 1b
	nop
	# Return from exception
	DMFC0(k0,C0_EPC)
	NOP_0_4
	lreg	AT, VPDA_ATSAVE(zero)	# -- restore AT (dmfc0 LDSLOT)
	PTR_ADDIU k0, 4			# Advance past write instruction
	DMTC0(k0,C0_EPC)
	NOP_0_4				# NOP to get EPC into cp0 before eret
	eret
END(handle_wade)
#endif	/* FAST_TEXTURE_LOAD */
