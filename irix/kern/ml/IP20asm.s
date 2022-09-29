/*
 * IP20 specific assembly routines; cpuid always 0, also make semaphore
 * macros a no-op.
 */
#ident "$Revision: 1.13 $"

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
XLEAF(dma_mapfree)
XLEAF(apsfail)
XLEAF(disallowboot)
XLEAF(rmi_fixecc)

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
XLEAF(dma_mapaddr)
#ifdef DEBUG
XLEAF(getcyclecounter)
#endif /* DEBUG */

/* Semaphore call stubs */
XLEAF(appsema)
XLEAF(apvsema)
XLEAF(apcvsema)
	move	v0,zero
	j ra
END(dummy_func)

/* dummy routines that return 1 */
LEAF(dummyret1_func)
XLEAF(apcpsema)	/* can always get on non-MP machines */
	li	v0, 1
	j ra
END(dummy_func)

/* return time stamp */
LEAF(_get_timestamp)
	.set	noreorder
	mfc0	v0,C0_COUNT
	.set	reorder
	j	ra
END(get_timestamp)	


/*
 *
 * writemcreg (reg, val)
 * 
 * Basically this does *(volatile uint *)(PHYS_TO_K1(reg)) = val;
 *	a0 - physical register address
 *	a1 - value to write
 *
 * This is a workaround for a bug in the first rev MC chip.  
 *
 * Because of (what I'd call a bug in) ld, we have to
 * resort to a kludge in order to ensure that this code 
 * will never share its cache-line.
 */

 

LEAF(writemcreg)
/*
 * Reserve a cacheline.
 */
	.set	noreorder

	j	4f

	nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop

4:
        la      v0,1f
        or      v0,SEXT_K1BASE
        j       v0              # run uncached
1:	or	a0,SEXT_K1BASE # a0 = PHYS_TO_K1(a0)
#ifdef MIPSEB
	lw	v0,0xbfc00004	# dummy read from prom to flush mux fifo
#else
	lw	v0,0xbfc00000	# dummy read from prom to flush mux fifo
#endif
	sw	a1,(a0)		# write val in a1 to MC register *a0
	
	j	ra
/*
 * Reserve a cacheline.
 */
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop;
	nop; nop; nop; nop

	.set	reorder
END(writemcreg)

/*
 * Write the VDMA MEMADR, MODE, SIZE, STRIDE registers
 *
 * write4vdma (buf, mode, size, stride);
 */

LEAF(write4vdma)

        .set    noreorder

        j       4f		# These nops are just for show.

        nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop
4:
        la      v0,1f
        or      v0,SEXT_K1BASE       # run uncached
        j       v0
1:

#ifdef MIPSEB
        lw      v1,0xbfc00004   # dummy read from prom to flush mux fifo
#else
        lw      v1,0xbfc00000   # dummy read from prom to flush mux fifo
#endif


	/* XXX Only works because bit 15 not set in DMA_MEMADR */

        lui     v0, (K1BASE | DMA_MEMADR)>>16

        sw      a0,DMA_MEMADR & 0xffff(v0)
        sw      a1,DMA_MODE   & 0xffff(v0)
        sw      a2,DMA_SIZE   & 0xffff(v0)
        sw      a3,DMA_STRIDE & 0xffff(v0)

/*
        sw      ??,DMA_	GIOADRS & 0xffff(v0)
*/
        j       ra
/*
 * Reserve a cacheline.
 */
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop

        .set    reorder
END(write4vdma)

