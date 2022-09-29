/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1995 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/SN/klconfig.h>
#include <ksys/cacheops.h>
#include <sys/kthread.h>
#include <sys/rt.h>

/*
 * On the IP27, the secondary cache is a unified i/d cache. The
 * R10000 caches must maintain the property that the primary caches
 * are a subset of the secondary cache - any line that is present in
 * the primary must also be present in the secondary. A line can be
 * dirty in the primary and clean in the secondary, of coarse, but
 * the subset property must be maintained. Therefore, if we invalidate
 * lines in the secondary, we must also invalidate lines in both the
 * primary i and d caches. So when we use the 'index invalidate' or
 * 'index writeback invalidate' cache operations, those operations
 * must be targeted at both the primary caches.
 *
 * When we use the 'hit' operations, the operation can be targetted
 * selectively at the primary i-cache or the d-cache.
 */
void
clean_icache(void *addr, unsigned int len, pfn_t pfn,
	     unsigned int flags)
{
	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		if (len > pdcache_size) {
			if ((flags & CACH_REASONS) == CACH_ICACHE_COHERENCY) {
				if (!__pdcache_wbinval(addr,len)) {
					__picache_inval(addr,len);
					return;
				}
				/* should only get here on old R4400s */
			}
		}
		__cache_wb_inval(addr, len);
		return;
	}

	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));
	/* This routine performs cacheops using the xkphys address range
	 * and 64-bit addressing.  No need to remap.
	 */

	__cache_hwb_inval_pfn( poff(addr), len, pfn );
}

void
clean_dcache(void *addr, unsigned int len, pfn_t pfn,
	     unsigned int flags)
{
	char *kvaddr;

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;
	ASSERT(flags & CACH_OPMASK);
	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/* If original addr is KUSEG, then we must remap it since it may be
	 * a request from another CPU and hence not the current user map.
	 * If it is a K2 address, then use it without remapping, since the map
	 * is global to all CPUs and hence still setup. Also true for K0.
	 * NOTE: We currently preserve the virtual address page color.  This
	 * probably isn't necessary since the "HIT" cacheops know which
	 * primary cache line to operate on, but it doesn't hurt.
	 *
	 * NOTE: We also need to remap if CACH_NOADDR is set and addr was not
	 * a K0 address (i.e. only the pfn was passed in).
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR))
		kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) + poff(addr));
	else
		kvaddr = addr;

	if (flags & CACH_SN0_POQ_WAR) {
		extern void __hub_poq_dcache_wbinval(caddr_t, int);	
		if (IS_KSEG0(kvaddr))
			__hub_poq_dcache_wbinval(kvaddr, len);
		return;
	}

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK)) {
		__dcache_wb_inval(kvaddr, len);
	} else if (flags & CACH_INVAL) {
		__dcache_inval(kvaddr, len);
	} else if (flags & CACH_WBACK) {
		__dcache_wb(kvaddr, len);
	}
}

#define	PHYS_TO_K0_EXL(x)	((x) | K0BASE_EXL) /* physical to kseg0 - excl*/

/*
 * sync_dcaches_excl
 *
 * This routine not only forces other dirty primary lines to be written
 * back, but performs an exclusive access on each cache line.
 * So, this INVALIDATES the line in the other cpu's cache.
 * NOTE: This effectively performs a flush of this line from ALL of the
 * other dcaches & icaches on SN0 systems.
 */

/* ARGSUSED */
int
sync_dcaches_excl(void *addr, unsigned int len, pfn_t pfn,
	     unsigned int flags)
{
	char *kvaddr;
	void *old_rtpin;
	int s;
	int i;

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	/* Better not be able to migrate to another cpu.
	 *
	 * splhi()/splx() works just fine and is still used early in
	 * system initialization when there is no thread and we can't use
	 * mutex_lock().
	 * But cleaning the cache on a large system (32 processor EVEREST)
	 * which is busy can take up to 10 microseconds per cacheline !!
	 * (system bus is heavily loaded, say by Sproc stress test) so
	 * in this case we just pin the thread and use a mutex.
	 */
	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK)) {
		old_rtpin = rt_pin_thread();
	} else
		s = splhi();

	kvaddr = (char *)(PHYS_TO_K0_EXL(ctob(pfn)) + poff(addr));

	/*
	 * We're about to do a read from a PG_EXCL cache line in order to
	 * remove the cache line entirely from other cpus' caches.  Unfortunately,
	 * a read from a PG_EXCL won't really have this effect if the line is
	 * currently in our own cache.  If we guarantee that the line is not in
	 * our own cache, then the PG_EXCL will work the way we'd like.
	 * [TBD: We may end up doing an additional cache operation after we
	 * return from the sync_dcaches_* operation.  We should change some
	 * layering in the cache flushing routines to avoid this extra pass.]
	 */
	__dcache_wb_inval(kvaddr, len);

	/* Read one word from each cache line. This causes a writeback and
	 * invalidate of the line from all other caches in the system.
	 */
	for(i=0; i<len; i+=CACHE_SLINE_SIZE)
		*(volatile int *)(kvaddr+i);

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK))
		rt_unpin_thread(old_rtpin);
	else
		splx(s);

	return(1);	/* !0 => all other icaches invalidated */
}

/*
 * sync_dcaches
 *
 * Force potentially dirty primary data cache lines on other CPUs to
 * within the system's coherency boundary.  This avoids tricky
 * problems with stale icache that could occur if a process migrated
 * after faulting in a text page.
 *
 * NOTE: Upon completion of this routine, the line could still be dirty
 * in THIS cpu's cache, but not in any other cpu's cache.
 */

int
sync_dcaches(void *addr, unsigned int len, pfn_t pfn,
	     unsigned int flags)
{

	char *kvaddr;
	void *old_rtpin;
	int s, i;

	/* make sure we're word aligned */
	addr = (void *)((__psint_t)addr & ~3);
	
	/* If caller will end up operating on ICACHE too, then use
	 * exclusive access to invalidate all other caches in one pass
	 * rather than sending separate icache invalidate.
	 */
	
	if (flags & CACH_ICACHE)
		return( sync_dcaches_excl(addr,len,pfn,flags) );

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */

	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/* Better not be able to migrate to another cpu.
	 *
	 * splhi()/splx() works just fine and is still used early in
	 * system initialization when there is no thread and we can't use
	 * mutex_lock().
	 * But cleaning the cache on a large system (32 processor EVEREST)
	 * which is busy can take up to 10 microseconds per cacheline !!
	 * (system bus is heavily loaded, say by Sproc stress test) so
	 * in this case we just pin the thread and use a mutex.
	 */

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK)) {
		old_rtpin = rt_pin_thread();
	} else
		s = splhi();

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR))
		kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) + poff(addr));
	else
		kvaddr = addr;

	/* Read one word from each cache line. This causes a writeback and
	 * invalidate of the line from all other caches in the system.
	 */

	for(i=0; i<len; i+=CACHE_SLINE_SIZE)
		*(volatile int *)(kvaddr+i);

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK))
		rt_unpin_thread(old_rtpin);
	else
		splx(s);

	return(0);
}

uint
getcachesz(cpuid_t cpu) {
	klcpu_t *acpu;
	acpu = get_cpuinfo(cpu);

	/* Take the cachesize in megabytes and shift it into bytes. */
	return (uint)acpu->cpu_scachesz << 20;
}

