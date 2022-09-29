/*
 * IP22 specific assembly routines; cpuid always 0, also make semaphore
 * macros a no-op.
 */
#ident "$Revision: 1.16 $"

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

	j ra
END(dummy_func)

LEAF(dcache_wb)
XLEAF(dcache_wbinval)
XLEAF(dki_dcache_wb)
XLEAF(dki_dcache_wbinval)
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_WBACK|CACH_IO_COHERENCY
	j	cache_operation
	END(dcache_wb)

LEAF(dki_dcache_inval)
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_IO_COHERENCY
	j	cache_operation
	END(dki_dcache_inval)

/* dummy routines that return 0 */
LEAF(dummyret0_func)

XLEAF(vme_adapter)
XLEAF(is_vme_space)
XLEAF(getcpuid)
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

/* return time stamp */
LEAF(_get_timestamp)
	j	get_r4k_counter
	END(_get_timestamp)	


/*
 *
 * writemcreg (reg, val)
 * 
 * Basically this does *(volatile uint *)(PHYS_TO_K1(reg)) = val;
 *	a0 - physical register address
 *	a1 - value to write
 *
 * This was a workaround for a bug in the first rev MC chip, but IP22
 * has only rev B (or greater) MCs, so just do the actual operation.
 */

LEAF(writemcreg)
/*
 * Reserve a cacheline.
 */
	or	a0,SEXT_K1BASE	# a0 = PHYS_TO_K1(a0)
	sw	a1,(a0)
	j	ra
END(writemcreg)


/*
 * Write the VDMA MEMADR, MODE, SIZE, STRIDE registers
 *
 * write4vdma (buf, mode, size, stride);
 */

LEAF(write4vdma)
	/* XXX Only works because bit 15 not set in DMA_MEMADR */
        LI     v0, (K1BASE | DMA_MEMADR)& (~0xffff)

        sw      a0,DMA_MEMADR & 0xffff(v0)
        sw      a1,DMA_MODE   & 0xffff(v0)
        sw      a2,DMA_SIZE   & 0xffff(v0)
        sw      a3,DMA_STRIDE & 0xffff(v0)

        j       ra
END(write4vdma)
