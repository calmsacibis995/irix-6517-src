#include <sys/types.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/pda.h>
#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <ksys/cacheops.h>

#ifndef NULL
#define NULL	0
#endif

#ifdef DEBUG
int mapflushcoll;	/* K2 address collision counter */
int mapflushdepth;	/* K2 address collision maximum depth */
#endif

#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif

is_in_pfdat(pgno_t pfn)
{
	pgno_t min_pfn = btoct(kpbase);
	extern int seg0_maxmem, seg1_maxmem;

	pfn = pfn_to_eccpfn(pfn);
	return((pfn >= min_pfn &&
		pfn < (btoct(SEG0_BASE) + seg0_maxmem)) ||
	       (pfn >= btoct(SEG1_BASE) &&
	        pfn < (btoct(SEG1_BASE) + seg1_maxmem)));
}

static void
mapflush(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
    void *kvaddr;
    uint lastpgi = 0;
    int s;
    int current_color;
    static int *pginuse;

    if (!private.p_cacheop_pages) {
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
	    /* force to an even page of a dedicate pair of pages */
	    private.p_cacheop_pages = kvalloc(3,0,0);
	    if (((__psunsigned_t) private.p_cacheop_pages) & NBPP)
		private.p_cacheop_pages += NBPP;
	} else
#endif /* MH_R10000_SPECULATION_WAR */
	private.p_cacheop_pages = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
	pginuse = (int *)kmem_zalloc(sizeof(int) * (cachecolormask+1), 0);
	ASSERT(private.p_cacheop_pages != NULL && pginuse != NULL);
    }
#ifdef _VCE_AVOIDANCE
    if (vce_avoidance && is_in_pfdat(pfn))
	current_color = pfd_to_vcolor(pfntopfdat(pfn));
    else
	current_color = -1;
    if (current_color == -1)
	current_color = colorof(addr);
#else
    current_color = colorof(addr);
#endif

    kvaddr = (void *)((__psunsigned_t)private.p_cacheop_pages +
		  (NBPP * current_color) + poff(addr));

    s = spl7();
    if (pginuse[current_color]) {
#ifdef DEBUG
	mapflushcoll++;
	if (pginuse[current_color] > mapflushdepth)
	    mapflushdepth = pginuse[current_color];
#endif
	/* save last mapping */
	lastpgi = kvtokptbl(kvaddr)->pgi;
	ASSERT(lastpgi && lastpgi != mkpde(PG_G, 0));
	unmaptlb(0, btoct(kvaddr));
    }
    if (is_in_pfdat(pfn)) {
	    if (pfntopfdat(pfn)->pf_flags & P_ECCSTALE)
		    pfn = eccpfn_to_noeccpfn(pfn);
	    else
		    pfn = pfn_to_eccpfn(pfn);
    }
    pg_setpgi(kvtokptbl(kvaddr),
	      mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));
    pginuse[current_color]++;

    if (flags & CACH_ICACHE) {
	__icache_inval(kvaddr, len);
    } else {
	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK))
		__dcache_wb_inval(kvaddr, len);
	else if (flags & CACH_INVAL)
		__dcache_inval(kvaddr, len);
	else if (flags & CACH_WBACK)
		__dcache_wb(kvaddr, len);
    }

    unmaptlb(0, btoct(kvaddr));
    if (lastpgi) {
	/* restore last mapping */
	ASSERT(lastpgi != mkpde(PG_G, 0));
	pg_setpgi(kvtokptbl(kvaddr), lastpgi);
	tlbdropin(0, kvaddr, kvtokptbl(kvaddr)->pte);
    } else {
	pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0));
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000())
		unmaptlb(0, btoct(kvaddr));
#endif /* MH_R10000_SPECULATION_WAR */
    }
    pginuse[current_color]--;
    splx(s);
}

/*
 * On the IP32, the secondary cache is a unified i/d cache. The
 * r4000 caches must maintain the property that the primary caches
 * are a subset of the secondary cache - any line that is present in
 * the primary must also be present in the secondary. A line can be
 * dirty in the primary and clean in the secondary, of coarse, but
 * the subset property must be maintained. Therefore, if we invalidate
 * lines in the secondary, we must also invalidate lines in both the
 * primary i and d caches. So when we use the 'index invalidate' or
 * 'index writeback invalidate' cache operations, those operations
 * must be targetted at both the primary caches.
 *
 * When we use the 'hit' operations, the operation can be targetted
 * selectively at the primary i-cache or the d-cache.
 */

void
clean_icache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	char *kvaddr;
	int	in_pfdat;
	int	ecc_stale;

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;

	ASSERT(flags & CACH_OPMASK);
	ASSERT(!IS_KSEG1(addr));	/* catch PHYS_TO_K0 on high addrs */

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/*
	 * Remap the page to flush if
	 * 	the page is mapped and vce_avoidance is enabled, or
	 *	the page is in high memory
	 */

	in_pfdat = pfn && is_in_pfdat(pfn);
	ecc_stale = (in_pfdat && ((pfntopfdat(pfn)->pf_flags & P_ECCSTALE)));

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR) || ecc_stale) {
		ASSERT(pfn);
		if (
		    ecc_stale ||
#ifdef _VCE_AVOIDANCE
		    (vce_avoidance && in_pfdat) ||
#endif
		    (pfn >= SMALLMEM_K0_PFNMAX)) {

		    mapflush(addr, len, pfn, flags & ~CACH_DCACHE);
		    return;
		} else
		    kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	} else
		kvaddr = addr;

	__icache_inval(kvaddr, len);
}

void
clean_dcache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	char *kvaddr;
	int	in_pfdat;
	int	ecc_stale;
	extern void __sysad_wbflush(void);

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;

	ASSERT(flags & CACH_OPMASK);
	ASSERT(!IS_KSEG1(addr));	/* catch PHYS_TO_K0 on high addrs */

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		if (flags & CACH_IO_COHERENCY)
			__sysad_wbflush();
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/*
	 * Remap the page to flush if
	 * 	the page is mapped and vce_avoidance is enabled, or
	 *	the page is in high memory
	 */

	in_pfdat = pfn && is_in_pfdat(pfn);
	ecc_stale = (in_pfdat && ((pfntopfdat(pfn)->pf_flags & P_ECCSTALE)));

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR) || ecc_stale) {
		ASSERT(pfn);
		if (
		    ecc_stale ||
#ifdef _VCE_AVOIDANCE
		    (vce_avoidance && in_pfdat) ||
#endif
		    (pfn >= SMALLMEM_K0_PFNMAX)) {

		    mapflush(addr, len, pfn, flags & ~CACH_ICACHE);
		    if (flags & CACH_IO_COHERENCY)
			__sysad_wbflush();
		    return;
		} else
		    kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	} else
		kvaddr = addr;

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK)) {
		__dcache_wb_inval(kvaddr, len);
	} else if (flags & CACH_INVAL) {
		__dcache_inval(kvaddr, len);
	} else if (flags & CACH_WBACK) {
		__dcache_wb(kvaddr, len);
	}

	if (flags & CACH_IO_COHERENCY)
		__sysad_wbflush();
}


/*
 *  Invalidate cache for I/O with minimal overhead
 */

void
__vm_dcache_inval(void *addr,int bcnt)
{
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
		invalidate_range_references(addr,bcnt,
					    CACH_DCACHE|CACH_SCACHE|
					    CACH_INVAL|CACH_IO_COHERENCY,
					    INV_REF_FAST);
	} else 
#endif /* MH_R10000_SPECULATION_WAR */
		__dcache_inval( addr, bcnt );
}

/* 
 *  Writeback-invalidate cache for I/O with minimal overhead
 */

void
__vm_dcache_wb_inval(void *addr,int bcnt)
{
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
		invalidate_range_references(addr,bcnt,
					    CACH_DCACHE|CACH_SCACHE|CACH_WBACK|
					    CACH_INVAL|CACH_IO_COHERENCY,
					    INV_REF_FAST);
	} else 
#endif /* MH_R10000_SPECULATION_WAR */
		__dcache_wb_inval( addr, bcnt );
}
