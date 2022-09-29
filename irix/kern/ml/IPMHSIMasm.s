/*
 * IP22 specific assembly routines; cpuid always 0, also make semaphore
 * macros a no-op.
 */
#ident "$Revision: 1.1 $"

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
XLEAF(disarm_threeway_trigger)
XLEAF(threeway_trigger_armed)
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
END(get_timestamp)	


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
	or	a0,K1BASE	# a0 = PHYS_TO_K1(a0)
	sw	a1,(a0)
	j	ra
END(writemcreg)

