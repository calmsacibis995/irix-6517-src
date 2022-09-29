/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/reg.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#if EVEREST
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#endif

extern int pdcache_size;
extern int picache_size;

void invaltlb_in_set(int, int);
void __dcache_inval(void *, int);
void tfp_flush_icache(void *, unsigned int);


#if DEBUG
/*
 * On TFP, we only allow the PDA and kernel stack to be wired.
 * Note that the index parameter is meaningless and that vaddr actually
 * determines position in the tlb.
 * NOTE: Implicit assumption is that each of these addresses hash to a
 *	 unique entry in TLB set 0 (i.e. they must be unique in the
 *	 7 least significant bits of the virtual page number).
 */
void tlbwired(int index, unsigned char *tlbpidp, caddr_t vaddr, pte_t pte)
{
	extern void _tlbwired(int, unsigned char *, caddr_t, pte_t);

	if ((vaddr != (caddr_t)PDAPAGE) &&
	    (vaddr != (caddr_t)(KSTACKPAGE)))
	  cmn_err(CE_PANIC, "tlbwired: invalid vaddr: 0x%x\n", vaddr);

	_tlbwired(index, tlbpidp, vaddr, pte);
}
#endif	/* DEBUG */

/*
 * On TFP, we assume that invaltlb is only called by routines like the clock
 * routine which are invalidating by index in order to slowly flush the tlb.
 * Consequently, we do NOT allow flushing of the PDA page, uarea, or kernel
 * stack by this routine.
 */
void invaltlb(int index)
{
	int set;
	
	/* If this index "hits" one of the wired entries, then we only
	 * need to invalidate sets 1 & 2.
	 */
	
	if ((index < 0) || (index>NTLBENTRIES)) {
		/* This check added because an invalid value (like -1)
		 * could be passed in and cause us to wipe out one of our
		 * wired entries since index will not match TLBINDEX(index)
		 * but gets truncated into a value which matches in the HW.
		 */
#if DEBUG
		cmn_err(CE_WARN|CE_CPUID,"invaltlb on invald index 0x%x\n", index);
#endif
		return;
	}
	if ((index == TLBINDEX(PDAPAGE,0)) ||
	    (index == TLBINDEX(KSTACKPAGE,0)))
		set = 1;
	else
		set = 0;

	for (; set < NTLBSETS; set++)
		invaltlb_in_set(index, set);
}

/*
 * flush_tlb(index)
 *
 * On non-TFP platforms, the parameter specifies the start index of the TLB 
 * operation.  On those platforms we never flush the PDA, typically starting
 * the flush after the UPAGE/KSTACK wired entries or at the start
 * of the random entries.  During initialization is the only time we actually
 * flush the UPAGE/KSTACK entries.
 *
 * On TFP systems, a compatible set of operations is defined by allowing a
 * flag value of 1 to indicate that the UPAGE/KSTACK tlb should be flushed.
 * All other flag values result in the flushing of only random entries (i.e.
 * the PDA/UPAGE/KSTACK are not flushed).
 */

void flush_tlb(int flag)
{
	int index;

	for (index=0; index < NTLBENTRIES; index++)
		invaltlb(index);

	if (flag == TLBFLUSH_NONPDA) {
		invaltlb_in_set(TLBINDEX(KSTACKPAGE,0), 0);
	}
}

/* unmap_tlb(pid, vpage)
 *
 */

#if DEBUG
void _unmaptlb(unsigned char *pid, __psunsigned_t vpage)
#else
void unmaptlb(unsigned char *pid, __psunsigned_t vpage)
#endif
{
	int	index, s;
	long	vaddr, xormask;
	
	/* must block preemption while we use tlbpid - just use splhi since
	 * operation should be very quick.
	 */
	s = splhi();
	vaddr = ctob(vpage);
	if (pid && (IS_KUSEG(vaddr) || ((vaddr & KV1BASE) == KV1BASE)))
		xormask = pid[cpuid()] & 0x7f;
	else
		xormask = 0;
	index = (vpage ^ xormask) & (NTLBENTRIES-1);

	/* we could implement a PROBE routine and only flush the set(s)
	 * which hit.  But for now we just flush all tlbs at this index.
	 */

	invaltlb(index);
	splx(s);
}

#if !IP26
/*
 * void clean_icache(void *addr, unsigned int len, unsigned int pfn,
 *		     unsigned int flags)
 *
 */
/* ARGSUSED */
void clean_icache(void *addr, unsigned int len, unsigned int pfn,
		  unsigned int flags)
{
	__psint_t real_addr;

	len = min(len, picache_size);

	/* Round addr down to the nearest cacheline strat */
	real_addr = ((__psint_t)addr) & ~(__psunsigned_t)ICACHE_LINEMASK;

	/* Adjust len for starting at a lower address */
	len += ((__psint_t)addr - real_addr);

#ifndef SABLE
	ASSERT(picache_size == ICACHE_SIZE);
#endif

	/*
	 * Convert len to cache lines, rounding up,
	 * convert real_addr into an icache index,
	 * and call tfp_flushicache
	 */
	if (len)
		tfp_flush_icache((void *)(real_addr & (ICACHE_SIZE - 1)),
			 (len + (ICACHE_LINESIZE - 1)) / ICACHE_LINESIZE);
}


	/* Try to flush data at "addr" from the secondary 
	   cache; as the secondary cache is 4-way set associative,
	   accessing data at the addr. 4 times incrementing the 
	   address with the size of a set should work if the replacement 
	   algorithm were LRU or some other mechanism which is deterministic; 
	   unfortunately, the replacement algorithm is random, thus
	   making it impossible to guarantee that the data has been flushed 
           out; there are other ways using SET_ALLOW to enable sets 
	   selectively one at a time to flush data, but these methods
	   have not been proven to work without causing cache coherency 
	   problems. So we try to increase the probability of evicting
	   data by trying 12 times.

	   "addr" is a K0SEG addr.
	*/
void evict_gcdata(void *addr)
{
	extern pfn_t physmem;
	extern int boot_sidcache_size;
	__psint_t ta, sa = (__psint_t)addr;
	int incr, exc_physmem = 0;
	int i, j, num_tries = 3;
	int num_sets = SCACHE_SET_ASSOC; /* no. of sets in secondary cache */

comp_incr:
	incr = (1 - exc_physmem) * (boot_sidcache_size / num_sets);
	for (i = num_tries, ta = sa; i--;)
		for (j = num_sets; j--; ta += incr) 
			if (ta < PHYS_TO_K0(ctob(physmem))) {
				*(volatile int *)ta;
			}
			else {
				exc_physmem = 2;
				goto comp_incr;
			}
				
}

/*
 * void clean_dcache(void *addr, unsigned int len, unsigned int pfn,
 *		     unsigned int flags)
 *
 */
/* ARGSUSED */
void clean_dcache(void *addr, unsigned int len, unsigned int pfn,
		  unsigned int flags)
{
	__dcache_inval((void *)NULL, pdcache_size);
	/* Take off the bottom bits and nail those lines. */
}


/*
 * void flush_cache()
 *
 *	Flush both the i and dcache.
 * XXX - On EVEREST TFP, we probably only need to flush the icache
 *
 */
void flush_cache(void)
{
	/* Just flush out the icache here. */
	tfp_flush_icache(0, ICACHE_SIZE / ICACHE_LINESIZE);

}	
#endif	/* !IP26 */
