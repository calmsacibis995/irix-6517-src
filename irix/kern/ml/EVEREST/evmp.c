/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Misc support routines for multiprocessor operations.
 */

#ident "$Revision: 1.190 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/dmamap.h>
#include <sys/proc.h>	/* shaddr_t for sys/runq.h */
#include <sys/runq.h>
#include <sys/sbd.h>
#include <sys/sysinfo.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/evintr.h>
#include <sys/invent.h>
#include <sys/kopt.h>
#include <sys/ktrace.h>
#include <sys/traplog.h>
#include <ksys/cacheops.h>
#include <bsd/sys/tcpipstats.h>
#if CELL
#include <ksys/cell.h>
#include <sys/EVEREST/evintr.h>
#endif
#include <sys/rt.h>
#include <sys/EVEREST/everror.h>

#include "ev_private.h"
#if	IP25
extern	void	ecc_init(void);
#endif

/*
 * For communication between sched and the new processor:
 * the new processor raises and waits to go low
 */
static volatile int	cb_wait = 0;

/*
 * Number of CPUs running when the NMI hit.
 */
int		nmi_maxcpus = 0;

int	pokeboot = 1;
#define BOOTTIMEOUT	500000		/* 1/2 sec */

extern cpuid_t master_procid;
static void	cpu_io_setup(void);
static void	init_nmi_dump(void);

extern struct lf_listvec *str_init_alloc_lfvec(cpuid_t);
extern str_init_slave(struct lf_listvec *, cpuid_t, cnodeid_t);
extern struct strstat *str_init_alloc_strstat(cpuid_t);

extern void	init_timebase(void);
extern void	reset_leds(void);
extern void	nmi_dump(void);
extern void	allowintrs(void);

#ifdef TFP
#define NASTY_BBCC_WAR	1
/* This turns on code for detecting illegal combinations of BBCC rev 5 chips
 * and graphics adapters.  It should be removed when we're sure that those
 * suckers are gone form the field.  At worst, the code that disables CPus
 * should be replaced with a panic so the horrid SCSI chip reinitialization
 * code can be removed.  See check_bbcc_revs().
 */

extern int ip21_bbcc_gfx;
#endif

/*
 * mlreset uses this to determine if a CPU has really started.
 */
extern char slave_loop_ready[EV_MAX_CPUS];

#if R4000 || R10000
/*
 * On the IP19, the secondary cache is a unified i/d cache. The
 * r4000 caches must maintain the property that the primary caches
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
#if NO_64BIT_OPS
	char *kvaddr;
	int s, remap=0;
#endif

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
#if IP19
		if (len > pdcache_size) {
			if ((flags & CACH_REASONS) == CACH_ICACHE_COHERENCY) {
				if (!__pdcache_wbinval(addr,len)) {
					__picache_inval(addr,len);
					return;
				} else {
					/* only get here on old R4400s */
					extern void indirect_flush_primary_caches(void);
					indirect_flush_primary_caches();
					return;
				}
			}
		}
#endif
		__cache_wb_inval(addr, len);
		return;
	}

#if NO_64BIT_OPS
	if (!private.p_cacheop_pages) {
		private.p_cacheop_pages = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
		ASSERT(private.p_cacheop_pages != NULL);
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((int)addr+len-1)));

	/* If original addr is KUSEG, then we must remap it since it may be
	 * a request from another CPU and hence not the current user map.
	 * If it is a K2 address, then use it without remapping, since the map
	 * is global to all CPUs and hence still setup. Also true for K0.
	 *
	 * NOTE: We also need to remap if CACH_NOADDR is set and addr was not
	 * a K0 address (i.e. only the pfn was passed in).
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		remap = 1;
		kvaddr = private.p_cacheop_pages +
				  (NBPP * colorof(addr)) + poff(addr);
		s = splhi();
		pg_setpgi(kvtokptbl(kvaddr),
			  mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));
	} else
		kvaddr = addr;

	__icache_inval(kvaddr, len);

	if (remap) {
		unmaptlb(0, btoct(kvaddr));
		pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0)); /* safety net */
		splx(s);
	}
#else
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));
	/* This routine performs cacheops using the xkphys address range
	 * and 64-bit addressing.  No need to remap.
	 */

	__cache_hwb_inval_pfn( poff(addr), len, pfn );
#endif
}

/* We currently use RMI_WAR versions of the cache flushing routines to flush
 * the caches.  We will use real cache ops in 5.1, after they've been heavily
 * tested.
 */
/*****#define USE_RMI_WAR 1*****/
#if USE_RMI_WAR
#define	RMI_CACHEMASK	0xfffff		/* mask for 1 meg cache */
#define	RMI_PFN(x)	((x) & btoct(RMI_CACHEMASK))
#endif

void
clean_dcache(void *addr, unsigned int len, pfn_t pfn,
	     unsigned int flags)
{
#if USE_RMI_WAR
	/*
	 * WARNING: The "real" clean_dcache requires real cache operations.
	 * We're forced to use rmi_cacheflush at the lower levels.
	 */

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;

	ASSERT(flags & CACH_OPMASK);
	/* IP19 routines require K0 addresses with RMI_WAR.  In fact,
	 * RMI_WAR just needs an address which is correct modulo cache
	 * size since it works by loading the cache with data from
	 * memory addresses below 2 MB.  So we'll use this feature
	 * in order to handle large memory configurations.
	 */
	if (!IS_KSEG0(addr)) {
		ASSERT(pfn);
		addr = (void *)(PHYS_TO_K0(ctob(RMI_PFN(pfn))) | poff(addr));
	}

	ASSERT(IS_KSEG0(addr));
	if (flags & CACH_NOADDR) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(flags & CACH_VADDR);

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK)) {
		__dcache_wb_inval(addr, len);
		return;
	}
	if (flags & CACH_INVAL) {
		__dcache_inval(addr, len);
		return;
	}
	if (flags & CACH_WBACK) {
		__dcache_wb(addr, len);
		return;
	}
#else

	char *kvaddr;
#if !defined(IP25)
	int s, remap=0;
#endif

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

#if !(IP25)

	if (!private.p_cacheop_pages) {
		private.p_cacheop_pages = kvalloc(cachecolormask+1,
						  VM_VACOLOR, 0);
		ASSERT(private.p_cacheop_pages != NULL);
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

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		remap = 1;
		kvaddr = private.p_cacheop_pages +
				  (NBPP * colorof(addr)) + poff(addr);
		if (curthreadp)
			mutex_lock(&pdaindr[cpuid()].pda->p_cacheop_pageslock, PZERO);
		else
			s = splhi();

		pg_setpgi(kvtokptbl(kvaddr),
			  mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));
	} else
		kvaddr = addr;
#else /* IP25 */
	kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) + poff(addr));
#endif /* IP25 */

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK)) {
		__dcache_wb_inval(kvaddr, len);
	} else if (flags & CACH_INVAL) {
		__dcache_inval(kvaddr, len);
	} else if (flags & CACH_WBACK) {
		__dcache_wb(kvaddr, len);
	}
#if !IP25
	if (remap) {
		unmaptlb(0, btoct(kvaddr));
		pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0)); /* safety net */
		if (curthreadp)
			mutex_unlock(&pdaindr[cpuid()].pda->p_cacheop_pageslock);
		else
			splx(s);
	}
#endif
#endif /* USE_RMI_WAR */
}



#ifdef R10000
#define	PHYS_TO_K0_EXL(x)	((x) | K0BASE_EXL) /* physical to kseg0 - excl*/
#endif

/*
 * sync_dcaches_excl
 *
 * This routine not only forces other dirty primary lines to be written
 * back, but performs an exclusive access on each cache line.
 * So, this INVALIDATES the line in the other cpu's cache.
 * NOTE: This effectively performs a flush of this line from ALL of the
 * other dcaches & icaches on EVEREST systems.
 */

/* ARGSUSED */
int
sync_dcaches_excl(void *addr, unsigned int len, pfn_t pfn,
	     unsigned int flags)
{

	char *kvaddr;
	void *old_rtpin;
	int s;
#ifndef	R10000
	int i;
#endif
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
	if (curthreadp) {
		old_rtpin = rt_pin_thread();
	} else
		s = splhi();

#ifdef R10000
#if	IP25
	kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) + poff(addr));
	__dcache_wb_inval(kvaddr, len);
	__cache_exclusive(kvaddr, len);
#else
	/*
	 * If we use IP19 type cache flush that work on secondary
	 * cache, then we would want the following:
	 */
	kvaddr = (char *)(PHYS_TO_K0_EXL(ctob(pfn)) + poff(addr));
	__dcache_wb_inval(kvaddr, len);
	for (i = 0; i < len; i += CACHE_SLINE_SIZE) {
	    *(volatile int *)(kvaddr + i);
	}
#endif /* IP25 */

#else /* !R10000 */

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 * splhi() moved prior to potential allocation of p_cacheop_pages
	 * to make sure we don't migrate after the check.
	 */
	if (curthreadp)
		mutex_lock(&pdaindr[cpuid()].pda->p_cacheop_pageslock, PZERO);

	if (!private.p_cacheop_pages) {
		private.p_cacheop_pages = kvalloc(cachecolormask+1,
						  VM_VACOLOR, 0);
		ASSERT(private.p_cacheop_pages != NULL);
	}
	ASSERT(btoct(addr)==btoct((__psint_t)addr+len-1));

	kvaddr = private.p_cacheop_pages + (NBPP * colorof(addr)) + poff(addr);
	pg_setpgi(kvtokptbl(kvaddr),
		  mkpde(PG_VR|PG_G|PG_SV|PG_M|PG_COHRNT_EXCL, pfn));

	/*
	 * We're about to do a read from a PG_COHRNT_EXCL cache line in order to
	 * remove the cache line entirely from other cpus' caches.  Unfortunately,
	 * a read from a PG_COHRNT_EXCL won't really have this effect if the line is
	 * currently in our own cache.  If we guarantee that the line is not in
	 * our own cache, then the PG_COHRNT_EXCL will work the way we'd like.
	 * [TBD: We may end up doing an additional cache operation after we
	 * return from the sync_dcaches_* operation.  We should change some
	 * layering in the cache flushing routines to avoid this extra pass.]
	 */
	__dcache_wb_inval(kvaddr, len);

	/* Read one word from each cache line. This causes a writeback and
	 * invalidate of the line from all other caches in the system.
	 */
	for(i=0; i<len; i+=128)
		*(volatile int *)(kvaddr+i);

	unmaptlb(0, btoct(kvaddr));
	pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0)); /* safety net */

	if (curthreadp)
		mutex_unlock(&pdaindr[cpuid()].pda->p_cacheop_pageslock);
#endif
	if (curthreadp)
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
	int s, remap=0;
#if	!IP25
	int i;
#endif

	/* Better not be able to migrate to another cpu */
	ASSERT_NOMIGRATE;

	/* make sure we're word aligned */
	addr = (void *)((__psint_t)addr & ~3);
	
	/* If caller will end up operating on ICACHE too, then use
	 * exclusive access to invalidate all other caches in one pass
	 * rather than sending separate icache invalidate.
	 *
	 * NOTE the check that the area being flushed fits within one page.
	 * This is because sync_dcaches_excl can't currently handle a
	 * request which crosses a page boundary, even if it is a K0seg
	 * address - this should be fixed at some point. For page crossing
	 * case we just use the "slow" approach.
	 */
	
	if ((flags & CACH_ICACHE)&&(btoct(addr)==btoct((__psint_t)addr+len-1)))
		return( sync_dcaches_excl(addr,len,pfn,flags) );

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if (!private.p_cacheop_pages) {
		private.p_cacheop_pages = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
		ASSERT(private.p_cacheop_pages != NULL);
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/* If original addr is KUSEG, then we must remap it since we may not
	 * be able to satisfy a fault on a user address (for example, mprotect
	 * calls high level flushing code which has the region locked, and any
	 * attempt to tlbmiss results in a deadlock trying to obtain the
	 * same lock).  So map it to a K2 address, so we don't have any
	 * problems.  IF we remap, then we go to splhi, because we can't allow
	 * nested operation of the routine since it would attempt to
	 * re-use the same p_cacheop_pages.
	 * NOTE: Also remap if CACH_NOADDR is set, since we don't have a valid
	 * addr, but it is the correct color.
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		remap = 1;
		if (curthreadp)
			mutex_lock(&pdaindr[cpuid()].pda->p_cacheop_pageslock, PZERO);
		else
			s = splhi();

		kvaddr = private.p_cacheop_pages +
				  (NBPP * colorof(addr)) + poff(addr);
		pg_setpgi(kvtokptbl(kvaddr),
			  mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));
	} else
		kvaddr = addr;

#if defined(IP25)
	/*
	 * Be sure line is flushed from all other CPU's caches.
	 */
	__cache_exclusive(kvaddr, len);
#else
	/* Read one word from each cache line. This forces the any dirty
	 * line on another CPU to be written back.
	 */
	for(i=0; i<len; i+=128)
		*(volatile int *)(kvaddr+i);
#endif

	if (remap) {
		unmaptlb(0, btoct(kvaddr));
		pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0)); /* safety net */
		if (curthreadp)
			mutex_unlock(&pdaindr[cpuid()].pda->p_cacheop_pageslock);
		else
			splx(s);
	}
	return(0);
}
#endif /* R4000 || R10000 */

#if	IP25

lock_t	configLock;
extern	int	disable_ip25_check;
extern	int	disable_r10k_log;

static	void
logR10000Config(void)
/*
 * Function: 	logR10000Config
 * Purpose:	Display the current R10000 configuration
 * Parameters:	none
 * Returns:	nothing
 */
{
#define	REV_MAJOR(p)	(((p) & C0_MAJREVMASK) >> C0_MAJREVSHIFT)
#define	REV_MINOR(p)	\
    ((REV_MAJOR(p) == 1) ? (((((p)&C0_MINREVMASK) >> C0_MINREVSHIFT) == 1)? \
			    4 : (((p) & C0_MINREVMASK) >> C0_MINREVSHIFT))  \
                     : (((p) & C0_MINREVMASK)>> C0_MINREVSHIFT))
    extern int	get_cpu_irr(void);
    __uint32_t	get_config(void);
    __uint32_t	c, prid;
    char	ccr;			/* cc revision */
    int		s;
    char	config[20];

    s = io_splock(configLock);

    c = get_config();
    ccr = (char)((EV_GETMYCONFIG_REG(EV_IWTRIG)) >> 28) + 'A';
    prid = (__uint32_t)get_cpu_irr();		/* Use config register */
    sprintf(config, "(%d-%d-%d)", 
	    (c & R10000_SCD_MASK) >> R10000_SCD_SHFT, 
	    (c & R10000_SCCD_MASK) >> R10000_SCCD_SHFT,
	    ((c & R10000_PRM_MASK) >> R10000_PRM_SHFT) + 1);

    if (disable_r10k_log) {
    	cmn_err(CE_NOTE+CE_CPUID, "R10000 %d.%d %dMB SCC(%c) %s",
	    REV_MAJOR(prid), REV_MINOR(prid), 
	    1 << (((c & R10000_SCS_MASK) >> R10000_SCS_SHFT) - 1), 
	    ccr, config);
    }

    /*
     * Now check for valid customer configuration. We don't want bad
     * processors going out to the field. Allow override if we want to 
     * allow us to turn it on.
     */

    if (!disable_ip25_check) {
	if ((ccr < 'D') 
	    || (REV_MAJOR(prid) < 2) 
	    || ((REV_MAJOR(prid) == 2) && (REV_MINOR(prid) < 3))
	    || ((REV_MAJOR(prid) == 3) && (REV_MINOR(prid) < 1))) {
	    cmn_err(CE_PANIC+CE_CPUID,
		    "R10000 %d.%d SCC(%c) - "
		    "INVALID CUSTOMER CONFIGURATION\n",
		    REV_MAJOR(prid), REV_MINOR(prid), ccr);
	}
    }
	    
    /*
     * Do not allow multiple write on CPUs with CC's rev <= D.
     */
    switch(ccr) {
    case 'A':
	if (((c & R10000_PRM_MASK) >> R10000_PRM_SHFT) > 0) {
	    cmn_err(CE_PANIC, 
		  "This kernel requires Revision B or greater - running '%c'",
		    ccr);
	}
    case 'B':
    case 'C':
	EV_SET_REG(EV_ECCSB, EV_GET_REG(EV_ECCSB) | EV_ECCSB_SWM);
	break;
    case 'D':
	EV_SET_REG(EV_ECCSB, 
		   EV_GET_REG(EV_ECCSB) | EV_ECCSB_SWM | EV_ECCSB_DIAL);
	break;
    case 'E':
	EV_SET_REG(EV_ECCSB, EV_GET_REG(EV_ECCSB) | EV_ECCSB_SWM);
	EV_SETMYCONFIG_REG(EV_PGBRDEN, 0);
	break;
    default:
	break;
    }
    io_spunlock(configLock, s);
}

#endif


#if TFP
/*
 * sync_dcaches
 *
 * Force potentially dirty primary data cache lines on other CPUs to
 * within the system's coherency boundary.  This avoids tricky
 * problems with stale icache that could occur if a process migrated
 * after faulting in a text page.
 *
 * NOTE: Upon completion of this routine, the line could still be dirty
 * in THIS cpus's cache, but not in any other cpu's cache.
 *
 * On TFP the primary dcaches write-through to the g-caches, so
 * we don't need to take any action.  We need to return "0" so that
 * the caller knows that the icache has NOT been synced with the
 * data dcache (do way to do this on TFP without a cpu to cpu interrupt).
 */

/* ARGSUSED */
int
sync_dcaches(void *addr, unsigned int len, pfn_t pfn,
	     unsigned int flags)
{
	return(0);
}
#endif	/* TFP */



#ifdef TFP

/*
 * early_init_wg -- this function writes 64 words of data to the write gatherer,
 *                  making it flush both of its a&b buffers and insuring that
 *                  all the words in the DB chips have correct parity.
 *
 * This function should be called for each the processor in the system,
 * by that processor (i.e. it doesn't do any good to run it 4 times on
 * cpu 0, it needs to be run on each individual cpu).
 */

#define SETWG(addr)  EV_SET_LOCAL(EV_WGDST, addr)

#define FLUSH                                              \
	  *(int *)0x8000000018300020 = 0;                  \
                                                           \
	  for(i=0; i < 3000; i++)  /* spin a little to make sure that the writes go out */\
	    ;                                              \
                                                           \
	  dummy = *(int *)0x9000000000000000;  /* do two uncached reads */\
	  dummy = *(int *)0x9000000000000008;              \
                                                           \
	  for(i=0; i < 3000; i++)  /* spin some more */    \
	    ;


void
early_init_wg(void)
{
  int i;
  volatile int dummy;
  char *ptr = (char *)WGTESTMEM_ADDR;

  ptr += cpuid()*256;
  
  EV_SET_LOCAL(EV_WGDST, ptr);
  
  /*
   * cause a flush to happen to clear any junk that might
   * happen to be in the write-gatherer after a reset.
   *
   * This could potentially cause a DB chip parity error
   * and a data sent error on channel zero.  If you get
   * those, you're on your own.  You could ignore them since
   * they're only for junk data that we don't care about,
   * but then again, I'm not sure.
   */
  FLUSH;

  /* write 64 words to flush out the write gather (both a&b buffers) */
  for(i=0; i < 64; i++)
   {
     *(volatile int *)0x8000000018300000 = i;
   }

  FLUSH;
}
#endif	 /* TFP */

/*
 * dobootduty - called by allowboot
 */
static void
dobootduty(void)
{
	extern int intstacksize;
	register pda_t *pda;
	cnodeid_t node;

	ASSERT(cb_wait > 0 && cb_wait < maxcpus);
	pda = pdaindr[cb_wait].pda;
	node = pda->p_nodeid;

	/*
	 * grab memory for interrupt stack
	 */
	pda->p_intstack = kvpalloc(btoc(intstacksize), VM_DIRECT, 0);
	pda->p_intlastframe =
		&pda->p_intstack[(intstacksize-EF_SIZE)/sizeof(k_machreg_t)];

	/*
	 * Allocate storage for this cpu for the following kernel areas:
	 * ksaptr => Kernel system activity statistics
	 * knaptr => Kernel network activity statistics
	 * kstr_lfvecp => Kernel Streams buffer list vectors
	 * kstr_statsp => Kernel Streams activity statistics
	 * NOTE: We assume that kern_calloc returns a zero'ed out area
	 */
	pda->ksaptr = (struct ksa *)kern_calloc(1, sizeof(struct ksa));
	if (pda->ksaptr == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: pda->ksaptr NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * Initialize tcp/ip statistics area for this cpu
	 */
	pda->knaptr = (char *)kern_calloc_node(1, sizeof(struct kna), node);
	if (pda->knaptr == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: pda->knaptr NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * Initialize streams buffers for this cpu
	 */
	pda->kstr_lfvecp = (char *)str_init_alloc_lfvec(pda->p_cpuid);
	if (pda->kstr_lfvecp == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: kstr_lfvecp NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * Allocate an initialize streams statistics block for this cpu
	 */
	pda->kstr_statsp = (char *)str_init_alloc_strstat(pda->p_cpuid);
	if (pda->kstr_statsp == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: kstr_statsp NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * kstr_lfvecp MUST be set properly prior to init the slave processor!
	 */
	str_init_slave((struct lf_listvec *)pda->kstr_lfvecp, pda->p_cpuid,
		node);

	/* all done - requestor can proceed */
	cb_wait = 0;
}

/*
 *  Mark a previously enabled CPU as not enabled, so cpu_enabled() returns 0
 */
static void
cpu_unenable(int virtid)
{
	int slot, slice, physid;

#if LARGE_CPU_COUNT_EVEREST
	if (virtid & 0x01) {
		if (virtid <= REAL_EV_MAX_CPUS)
			return;
		
		physid = MPCONF[EV_MAX_CPUS-virtid].phys_id;
	} else
#endif
	physid = MPCONF[virtid].phys_id;
	slot   = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	slice  = (physid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	EVCFG_CPUSTRUCT(slot, slice).cpu_vpid = -1;

	EVCFG_CPUSTRUCT(slot, slice).cpu_enable = 0;
}


/*
 * Determine whether a given CPU is present and enabled.
 */
int
cpu_enabled(cpuid_t virtid)
{
	int slot, slice;
	int physid;

	if ((virtid < 0) || (virtid > EV_MAX_CPUS))
		return 0;

#if MULTIKERNEL
	/* we only know about cpus in our cell */

	if (EVMK_CPUID_TO_CELLID(virtid) != evmk_cellid)
		return 0;
#endif
#if LARGE_CPU_COUNT_EVEREST
	if (virtid & 0x01) {
		if (virtid <= REAL_EV_MAX_CPUS)
			return 0;
		
		physid = MPCONF[EV_MAX_CPUS-virtid].phys_id;
	} else
#endif
	physid = MPCONF[virtid].phys_id;

	slot  = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	slice = (physid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	if ((EVCFGINFO->ecfg_board[slot].eb_type & EVCLASS_MASK) != EVCLASS_CPU)
		return 0;

	if ((EVCFG_CPUSTRUCT(slot, slice).cpu_vpid) != virtid)
		return 0;

	return (EVCFG_CPUSTRUCT(slot, slice).cpu_enable);
}


/*
 * Determine the revision of the CPU PROM on a given CPU.
 *   returns -1 on a virtid for a CPU that's not there.
 */
int ev_promrev(uint virtid)
{
	int slot, slice;
	int physid;

#if LARGE_CPU_COUNT_EVEREST
	if (virtid & 0x01) {
		if (virtid <= REAL_EV_MAX_CPUS)
			return 0;
		
		physid = MPCONF[EV_MAX_CPUS-virtid].phys_id;
	} else
#endif
	physid = MPCONF[virtid].phys_id;

	slot  = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	slice = (physid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	if ((EVCFGINFO->ecfg_board[slot].eb_type & EVCLASS_MASK) != EVCLASS_CPU)
		return -1;

	if ((EVCFG_CPUSTRUCT(slot, slice).cpu_vpid) != virtid)
		return -1;

	return EVCFG_CPUPROMREV(slot, slice);
}


static void
check_chip_revs(void)
{
	int i;
	evbrdinfo_t *brd;

	for (i = 0; i < EV_MAX_SLOTS; i++) {
		brd = &(EVCFGINFO->ecfg_board[i]);
		switch(brd->eb_type) {
			case EVTYPE_IP19:
				if (brd->eb_rev < 3)
					cmn_err(CE_CONT, "Downrev A Chip (%d) on the IP19 board in slot %d\n",
						brd->eb_rev, i);
				break;
			case EVTYPE_IO4:
				if (((brd->eb_rev >> 4) & 0xf) < 2)
					cmn_err(CE_CONT, "Downrev IA Chip (%d) on the IO4 board in slot %d\n",
						((brd->eb_rev >> 4) & 0xf), i);
				break;
			case EVTYPE_MC3:
				if (brd->eb_rev < 1)
					cmn_err(CE_CONT, "Downrev MA chip (%d) on the MC3 in slot %d\n",
						brd->eb_rev, i);
			break;
			default:
				break;
		}
	}

}


#if NASTY_BBCC_WAR
/*
 * Make sure the BBCC chips are up to rev
 * If they aren't and a graphics device is attached, disable all
 * but one processor.
 */
static void
check_bbcc_revs(void)
{
	int slot, ioa;
	evbrdinfo_t *brd;
	int bad_bbcc = 0;
	int gfx_found = 0;
	int io4_conf;
	int bad_board_vector = 0;

	if (ip21_bbcc_gfx) {
		cmn_err(CE_NOTE, "Overriding BBCC revision checking.\n");
		return;
	}

	for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
		brd = &(EVCFGINFO->ecfg_board[slot]);

		switch(brd->eb_type) {
			case EVTYPE_IP21:
				/* Rev 4 BBCC chips are bad.  Note them. */
				if (brd->eb_cpu.eb_ccrev == 4) {
					bad_bbcc++;
					bad_board_vector |= (1 << slot);
				}
				break;

			case EVTYPE_IO4:
				/* Check for FCG chips which surely mean gfx */
				for (ioa = 1; ioa < IO4_MAX_PADAPS; ioa++) {
					switch (brd->eb_ioarr[ioa].ioa_type) {
					case IO4_ADAP_FCG:
					case IO4_ADAP_HIP1A:
					case IO4_ADAP_HIP1B:
						gfx_found++;
					default:
						break;
					}
				}
				io4_conf = IO4_GETCONF_REG(slot, IO4_CONF_ADAP);

				/*
				 * If any of these bits are set, the IO4 knows
				 * that an adapter is a graphics adapter.
				 */
				if (io4_conf & 0xff00) {
					gfx_found++;
				}
				break;

			default:
				break;
		}
	}

	/*
	 * Multiprocessor machines with graphics and bad BBCC (rev 3) chips
	 * are not allowed without workarounds.  Only Irix 6.0 has the
	 * appropriate workarounds, since they were removed for patch 104 and
	 * 6.0.1.
	 */
	if (bad_bbcc && gfx_found) {
		int cpus, i;

		for (i = 0; i < maxcpus; i++) {
			if (cpu_enabled(i)) {
				cpus++;
			}
		}

		/* The single CPU case is OK. */
		if (cpus == 1)
			return;

		/*
		 * If we made it here, we have multiple CPUs, gfx, and at
		 * least one bad BBCC chip.  Print a warning, disable the rest
		 * of the CPUs and return.
		 */
		for (i = 0; i < EV_MAX_SLOTS; i++)
			if ((1 << i) & bad_board_vector)
				cmn_err(CE_NOTE, "Downrev BBCC on IP21 in slot"
						 " %d.", i);
		cmn_err(CE_NOTE, "Downrev BBCC chip doesn't support MP operation with graphics -");
		cmn_err(CE_NOTE, "Automatically disabling all but one CPU.");

		/* Disable all but the first processor (which is always 0). */
		for (i = 1; i < maxcpus; i++) {
			if (cpu_enabled(i)) {
				cpu_unenable(i);
			}
		}

		/*
		 * Now, fix SCSI interrupts.
		 * The problem here is that we can't check for graphics until
		 * after cn_init and early_wg_init have run.  We need to
		 * be sure that systems with consoles set to 'd' (ascii
		 * terminal) but have graphics still show up as graphics
		 * systems.  Since we're forced to wait this long to check
		 * for bad BBCCs and graphics, the SCSI interrupt desitnations
		 * have already been programmed.  Some of them have been
		 * pointed to CPUs that are now disabled.  For that reason,
		 * we need to rerun s1_init() for every S1 and SCIP chip in
		 * the system.
		 */
		for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
			int window;

			brd = &(EVCFGINFO->ecfg_board[slot]);
			window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;

			if (brd->eb_type == EVTYPE_IO4)
				for (ioa = 1; ioa < IO4_MAX_PADAPS; ioa++) {
					if (brd->eb_ioarr[ioa].ioa_type ==
                                            IO4_ADAP_SCSI)
						s1_init(slot, ioa, window, 0);
					if (brd->eb_ioarr[ioa].ioa_type ==
					    IO4_ADAP_SCIP)
						s1_init(slot, ioa, window, 1);
				}
		}
	}
}

#endif /* NASTY_BBCC_WAR */

/* ARGSUSED */
static void
init_boardregs(int print)
{
#if TFP
	volatile evreg_t diag;
	volatile evreg_t rev, rev2;
	int	i;

	/*
	 * Newer revisions of the IP21 board has D chip error logic instead
	 * of EAROM (which was broken to begin with). See if we have one of
	 * these. If so, clear D chip error bits since they don't get cleared
	 * at reset (like ERTOIP register).
	 */
	private.p_dchip_err_hw = 0;
	rev = *(volatile evreg_t *)EV_IP21_REV;
	rev &= EV_IP21REV_MASK;

	for (i = 0; i < 10; i++) {
		rev2 = *(volatile evreg_t *)(EV_IP21_REV + sizeof(evreg_t));
		rev2 &= EV_IP21REV_MASK;
		if (rev != rev2)
			break;
	}
	if (rev != 0 && rev == rev2) {
		private.p_dchip_err_hw = rev >> EV_IP21REV_SHFT;
		*(volatile evreg_t *)EV_IP21_REV = 0;
	}
#endif	/* TFP */
}

/*
 * Called when the master processor is satisfied that the system is sane
 * enough to come up with multiple CPU's.
 */
void
allowboot(void)
{
	extern void bootstrap();
	register volatile struct mpconf_blk *mpconf;
	register int i, timeout, cpus;
	cpumask_t curcpumask;
	int badcpu_count = 0;
	int real_maxcpus = 1;
	int numcpus;

#if defined(MULTIKERNEL)
	/* We import slave_loop_ready primarily as a DEBUG aid.  Since
	 * all cpus are initially executing the slave loop from direct mapped
	 * kernel space so ready values are effectively in cell 0 independent
	 * of location of golden cell.
	 */
	evmk_import( 0, slave_loop_ready, sizeof(slave_loop_ready));
#endif /* MULTIKERNEL */
	/* 
	 * machine dependent initialization that required more resources
	 * than at mlreset() level can be done here.
	 */

	init_timebase();	/* do early for best accuracy */
	init_boardregs(1);

#ifdef TFP
	early_init_wg();       /* make sure the write-gatherer is ok */
#endif	

	/*
	 * Set pokeboot=0 to come up with only 1 processor
	 */
	if (!pokeboot)
		return;

#if NASTY_BBCC_WAR
	check_bbcc_revs();	/* Make sure the BBCC chips are up to rev */
#endif

#if	IP25
	initnlock(&configLock, "configLock");
	logR10000Config();
	ecc_init();			/* ready ecc handler Scotty */
#endif

	/*
	 * Use the information left in the MPCONF area by the standalone
	 * code to figure out how many processors are present
	 */
	if (maxcpus > sizeof(cpumask_t)*NBBY) {
		cmn_err(CE_CONT,
	     "\nKernel can only support %d of the %d installed processors.\n\n",
			sizeof(cpumask_t)*NBBY, maxcpus);
		maxcpus = sizeof(cpumask_t)*NBBY;
	}
	CPUMASK_CLRALL(curcpumask);
	cpus = 0;
	for (i = 0; i < maxcpus; i++, mpconf++) {
		mpconf = &MPCONF[i];
#if LARGE_CPU_COUNT_EVEREST
		if (i >= REAL_EV_MAX_CPUS)
			break;
		if (i & 0x01)
			i = EV_MAX_CPUS-i;
#endif
		if (cpu_enabled(i)) {
			cpus++;
			CPUMASK_SETB(curcpumask, i);
		}
#if LARGE_CPU_COUNT_EVEREST
		/* back to normal loop index for "i" */
		if (i & 0x01)
			i = EV_MAX_CPUS-i;
#endif
	}

	check_chip_revs();

	/* let other processors come in */
	for (i = 0; i < maxcpus; i++) {
	    if (master_procid == i)
		continue;

#if LARGE_CPU_COUNT_EVEREST
	    if (i >= REAL_EV_MAX_CPUS)
		break;
#endif
#if MULTIKERNEL
	    /* If cpu is NOT in our cell, don't start it */

	    if (EVMK_CPUID_TO_CELLID(i) != evmk_cellid)
		continue;
#endif
	    mpconf = &MPCONF[i];

#ifdef DEBUG
	    /*
	     * Verify processor in kernel slave loop.
	     */
	    if (!slave_loop_ready[i]) {
 		    cmn_err(CE_WARN, 
			    "Processor #%d (0x%x/0x%x) "
			    "not ready to start\n",
			    i, cpuid_to_slot[i], cpuid_to_cpu[i]);
	    }
#endif

	    mpconf->lnch_parm = 0;
	    mpconf->rendezvous = 0;
	    mpconf->rndv_parm = 0;
#if LARGE_CPU_COUNT_EVEREST
	    /* convert odd numbers to high end of range */
	    if (i & 0x01)
		i = EV_MAX_CPUS-i;
#endif
	    if (CPUMASK_TSTB(curcpumask, i)) {
#if  _MIPS_SIM != _ABI64
		mpconf->stack = (int)pdaindr[i].pda->p_bootlastframe;
#endif
		mpconf->real_sp = pdaindr[i].pda->p_bootlastframe;
		flush_cache();

#if DEBUG
		cmn_err(CE_CONT, "Starting processor #%d (0x%x/0x%x)\n", 
			i, cpuid_to_slot[i], cpuid_to_cpu[i]);
#endif
		mpconf->lnch_parm = KPHYSTO32K1(mpconf);

		if (IS_KSEG1(bootstrap))
			mpconf->launch = KPHYSTO32K1(bootstrap);
		else
			mpconf->launch = KPHYSTO32K0(bootstrap);
		real_maxcpus = i+1;

	    } else {
		/* Make it jump back into the IP19 prom */
		mpconf->launch = KPHYSTO32K1(EV_PROM_FLASHLEDS);
#if DEBUG
		cmn_err(CE_NOTE, "Disabling processor #%d (0x%x/0x%x)", 
			i, cpuid_to_slot[i], cpuid_to_cpu[i]);
#endif
	    }
#if LARGE_CPU_COUNT_EVEREST
	    /* convert back to normal "i" value */
	    if (i & 0x01)
		i = EV_MAX_CPUS-i;
#endif
	}

#if LARGE_CPU_COUNT_EVEREST
	real_maxcpus = EV_MAX_CPUS;	/* test full range */
#endif
#ifndef SABLE
	/* Under SABLE no need to wait.  Also, timer's not yet supported. */
	/*
	** Now wait for a couple seconds. That should leave enough time
	** for other processors to start.
	*/
	timeout = BOOTTIMEOUT*maxcpus;
#if LARGE_CPU_COUNT_EVEREST
	/* decrease the wait time to something more realistic */
	timeout = BOOTTIMEOUT*REAL_EV_MAX_CPUS;
	timeout = BOOTTIMEOUT*18;
#endif
	numcpus = real_maxcpus - 1;
	while (--timeout && numcpus)  {
		if (cb_wait) {
			dobootduty();
			numcpus--;
		}
		DELAY(1);		/* 1 us */
	}

	/*
	 *  Do a quick check to see that the other processors started up.
	 */
	for (i = 0; i < maxcpus; i++) {
	    mpconf = &MPCONF[i];
#if LARGE_CPU_COUNT_EVEREST
	    /* convert odd numbers to high end of range */
	    if (i & 0x01)
		i = EV_MAX_CPUS-i;
#endif
	    if (cpu_enabled(i)  &&  pdaindr[i].CpuId != i) {
		cmn_err(CE_NOTE, "Processor #%d (0x%x/0x%x) did not start!", 
			i, cpuid_to_slot[i], cpuid_to_cpu[i]);
		pdaindr[i].CpuId = -1;
		cpu_unenable(i);
		badcpu_count++;
	    }
#if LARGE_CPU_COUNT_EVEREST
	    /* convert back to normal "i" value */
	    if (i & 0x01)
		i = EV_MAX_CPUS-i;
#endif
	}
	if (badcpu_count)
		cmn_err(CE_PANIC, "Cannot start without all CPUs\n");
#endif

	/*
	 *  If high-numbered processors have been disabled, then reset
	 *  "maxcpus" to indicate the last processor which must be
	 *  considered by the kernel from now on.
	 */
	maxcpus = real_maxcpus;
#if MULTIKERNEL
	if ((maxcpus == 1) && (master_procid >= maxcpus))
		maxcpus = master_procid+1;
#endif

	evhwg_setup(hwgraph_root,curcpumask);
	/*
	 * Finish setting up the I/O
	 */
	cpu_io_setup();

	/*
	 * Initialize the core dump NMI vector.
	 */

	init_nmi_dump();
	/* initialize the stuff needed for R10k mfhi war */
	init_mfhi_war();
}


void
disallowboot(void)
{
	register volatile struct mpconf_blk *mpconf;
	register int i;

	if (!pokeboot)
		return;

	mpconf = (struct mpconf_blk *)MPCONF_ADDR;

	for (i = 0; i < maxcpus; i++) {
#if LARGE_CPU_COUNT_EVEREST
	    /* convert odd numbers to high end of range */
		if (i >= REAL_EV_MAX_CPUS)
			continue;
#endif
		mpconf->launch = 0;
		mpconf = (struct mpconf_blk *)((char *) mpconf + MPCONF_SIZE);
	}
}


/*
 * Get the frequency of the specified CPU
 */
uint
getfreq(int virtid) {
	int slot, slice;
	int physid;

#if LARGE_CPU_COUNT_EVEREST
	/* convert virtid back to contiguous range */
	if (virtid & 0x01)
		virtid = EV_MAX_CPUS-virtid;
#endif
	physid = MPCONF[virtid].phys_id;

	slot  = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	slice = (physid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
	/* Frequency is stored in megahertz
	 */

	return EVCFG_CPUSTRUCT(slot, slice).cpu_speed;
}

/*
 * Get the secondary cache size of the specified CPU
 */
uint
getcachesz(cpuid_t virtid) {
	int slot, slice;
	int physid;
	uint cache_size;

#if LARGE_CPU_COUNT_EVEREST
	/* convert virtid back to contiguous range */
	if (virtid & 0x01)
		virtid = EV_MAX_CPUS-virtid;
#endif
	physid = MPCONF[virtid].phys_id;

	slot  = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	slice = (physid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	/* If value falls out of range, use maximum value.  This is safest
	 * course of action, since typical use of this value is for cache
	 * flushing routines and it's better to flush too much rather than
	 * too little.
	 */
	cache_size = 1 << EVCFG_CPUSTRUCT(slot, slice).cpu_cachesz;
	if ((cache_size < MINCACHE) || (cache_size > MAXCACHE))
		cache_size = MAXCACHE;
	return(cache_size);
}

/*
 * Boot routine called from assembly as soon as the bootstrap routine
 * has context set up.
 *
 * Interrupts are disabled and we're on the boot stack (which is in pda)
 * The pda is already accessible.
 */
cboot()
{
	extern int utlbmiss[];
	extern lock_t bootlck;
	register int id = getcpuid();
	register int s0;
#ifdef R4000
	register int i;
#endif
#if TFP
	extern	int trap_table(void);

	set_trapbase((__psunsigned_t)trap_table);
	set_kptbl((__psunsigned_t)kptbl);	/* used by kernel tlbmiss */
	init_boardregs(0);
#endif

	ASSERT(id < maxcpus);
	mlreset(1);
	/*_hook_exceptions();*/

	private.p_cpuid = id;
	CPUMASK_CLRALL(private.p_cpumask);
	CPUMASK_SETB(private.p_cpumask, id);
	/* plug exception handler vecs in */
	private.common_excnorm = get_except_norm();

	private.p_kvfault = 0;
#if R4000
	for (i=0; i < CACHECOLORSIZE; i++)
		private.p_clrkvflt[i] = 0;
#endif

	cache_preempt_limit();

	/* each cpu might run at a different speed */
	private.cpufreq = findcpufreq();

	/* Find out what kind of CPU/FPU we have. */
	coproc_find();

	/* single thread booting from this point */
	s0 = splock(bootlck);

	/*
	 * we are not allowed to do anything that might
	 * sleep or cause an interrupt,
	 * so we need to ask a process to get
	 * memory for us - our interrupt stack and
	 * ksa struct
	 * we of course spin waiting
	 * this implies that only 1 process can boot at once
	 * We can't even do a cvsema since that lowers the spl
	 * we let clock help us out
	 */
	cb_wait = id;
	while (cb_wait) {
#if 0
		register int wait = 0;

		if (wait++ > 40000) {
			if (kdebug == 1) {
				prom_printf(".");
			}
			wait = 0;
		}
		du_conpoll();
		;
#endif
	}

	/* now safe to claim we're enabled */
	private.p_flags |= (PDAF_ENABLED | PDAF_ISOLATED);
	numcpus++;
	CPUMASK_SETB(maskcpus, id);
	pdaindr[id].CpuId = id;

	/* do initialization required for each processor */
	stopclocks();

	reset_leds();

	spinlock_init(&pdaindr[id].pda->p_special, "pdaS");

	tlbinfo_init();
#if TFP
	icacheinfo_init();

	early_init_wg();       /* make sure the write-gatherer is ok */
#endif

	clkstart();
	/* allow more processors to boot */
	spunlock(bootlck, s0);

#if R4000 && DEBUG
	private.p_vcelog = kvpalloc(1, VM_DIRECT|VM_NOSLEEP, 0);
	private.p_vcelog_offset = 0;
	private.p_vcecount = 0;
#endif
#if TRAPLOG_DEBUG
	*(long *)TRAPLOG_PTR = TRAPLOG_BUFSTART;
#endif


	/*
	 * Problem: After enabling this processor but before doing
	 * a kickidle, the master processor does a contmemall.  It
	 * then hangs in flushcaches waiting for us to flush our
	 * icache.  Unfortunately, we aren't receiving interrupts,
	 * so we never find out about the cpu action request and
	 * the entire system hangs.
	 *
	 * Solution: Mark ourselves as ISOLATED (done above) until 
	 * we're able to accept interrupts.  That way, flushcaches 
	 * knows not to interrupt/wait for us.
	 */
	private.p_flags &= ~PDAF_ISOLATED;
	allowintrs();

	/*
	 * Let the CPU dispatcher know that we're here.
	 */
	joinrunq();

#if CELL
	cell_cpu_init();
#endif	

#if	IP25
	logR10000Config();
	ecc_init();
#endif
	/* initialize the stuff needed for R10k mfhi war */
	init_mfhi_war();
	/* drop interrupt level down to splhi */
	splset(II_SPLHI|II_CELHI);
	ASSERT(getsr() & SR_IE);
	/* everything set up - call idle */
	resumeidle(0);
	/* NOTREACHED */
	cmn_err(CE_PANIC, "cboot\n");
}



#ifdef MULTIKERNEL
#ifdef CELL
evmk_cc_intr_t evmk_cc_intr;

/* evmk_remote_hub_s( id )
 * Emulates operation of SN0 Cross Call interrupt.
 * We set a bit in the destination node's (cpu's) cc_intr_pending register (which
 * is actually just a memory location) corresponding to the sender's cellid
 * (same as partition or region for this simulation).
 * Then we send the remote cpu a "cc interrupt".
 */
void
evmk_send_ccintr( int remoteid )
{
        extern int part_id(void);
	__psunsigned_t dest;
	
	dest = ((__psunsigned_t)(&evmk_cc_intr.cc_intr_pend[remoteid][0])
			& 0x00ffffffff) +
				evmk.cell_k0base[EVMK_CPUID_TO_CELLID(remoteid)];

	/* set our cell (a.k.a. region, parition) bit in destination's
	 * "CC interrupt pending register".  Then we send a "cc interrupt".
	 */
	atomicSetUint64( (uint64_t *)dest, 1<< part_id());
	
	EV_SET_LOCAL( EV_SENDINT,
		EVINTR_VECTOR(EVINTR_LEVEL_CCINTR, EVINTR_CPUDEST(remoteid)));
}
#endif /* CELL */

/* Initialization performed mainly so variable is not placed in bss.
 * This allows golden cell to initialize certain fields before the kernel
 * copies itself and avoids having the bss bzero code wipe out the values.
 */

evmk_t	evmk = {
	{0,0,0,0}, /* cell_k0base[] */
	{0,0,0,0}, /* cellbootcomplete[] */
	0,	/* cellid */
	0,	/* golden_cellid */
	1,	/* cellmask */
	1	/* numcells */
};

void
evmk_replicate( void *addr, int len)
{
	int i;
	char *dest;

	if (!IS_MAPPED_KERN_SPACE(addr))
		cmn_err(CE_PANIC, "evmk_replicate: addr (0x%x) NOT mapped!\n",
			addr);

	for (i=0; i<evmk.numcells; i++) {

		if (i == evmk_cellid)
			continue;	/* don't copy to ourself */

		dest = (char *)(((__psunsigned_t)addr & 0x00ffffffff) +
					evmk.cell_k0base[i]);
		bcopy( addr, dest, len );
	}
}

/* evmk_import
 * Copies block of data from specified cellid into current cell.
 * addr - mapped kernel address
 */
void
evmk_import( int fromcell, void *addr, int len)
{
	char *src;

	if (!IS_MAPPED_KERN_SPACE(addr))
		cmn_err(CE_PANIC, "evmk_import: addr (0x%x) NOT mapped!\n",
			addr);

	if (fromcell == evmk_cellid)
		return;	/* don't import from ourself */

	src = (char *)(((__psunsigned_t)addr & 0x00ffffffff) +
			evmk.cell_k0base[fromcell]);

	bcopy( src, addr, len );
}

/* evmk_export
 * Copies block of data to specified cellid from current cell.
 * addr - mapped kernel address
 */
void
evmk_export( int tocell, void *addr, int len)
{
	char *dest;

	if (!IS_MAPPED_KERN_SPACE(addr))
		cmn_err(CE_PANIC, "evmk_import: addr (0x%x) NOT mapped!\n",
			addr);

	if (tocell == evmk_cellid)
		return;	/* don't export from ourself */

	dest = (char *)(((__psunsigned_t)addr & 0x00ffffffff) +
			evmk.cell_k0base[tocell]);

	bcopy( addr, dest, len );
}

/* evmk_setup_numcells
 * Can be invoked to collapse the number of configured cells into a smaller
 * number of cells.
 */

void
evmk_setup_numcells()
{
	int	collapse, i;
	uint	cellsize;
	int	numcells;

	if (is_specified(arg_evmk_numcells)) {
		numcells = atoi(arg_evmk_numcells);
	} else
		numcells = 0;

	if ((evmk.cellid != evmk.golden_cellid) || (numcells >= evmk.numcells))
		return;

	if (numcells == 0) {

		/* special case - nothing specified, base on memory size */

		cellsize = EVCFGINFO->ecfg_cell[0].cell_memsize;

		/* OK if at least 64MB per cell */

		numcells = evmk.numcells;

		while (cellsize < (0x04000000/256)) {
			cellsize = 2*cellsize;
			numcells = numcells/2;
		}
		if (numcells == evmk.numcells)
			return;
		if (numcells == 0)
			numcells = 1;

		/*
		 * Put at least 2 cpus in each cell
		 */
		while (maxcpus/2 < numcells)
			numcells /= 2;

	}

	switch(numcells) {
	case 1:
		collapse = evmk.numcells;
		EVCFGINFO->ecfg_cellmask = 0;
		break;
	case 2:
		collapse = evmk.numcells/2;
		EVCFGINFO->ecfg_cellmask = 0x1;
		break;
	case 4:
		collapse = evmk.numcells/4;
		EVCFGINFO->ecfg_cellmask = 0x3;
		break;

	default:
		return;
	}

	cellsize = EVCFGINFO->ecfg_cell[0].cell_memsize;

	for (i=0; i < numcells; i++) {
		EVCFGINFO->ecfg_cell[i].cell_memsize = collapse*cellsize;
		EVCFGINFO->ecfg_cell[i].cell_membase = i*collapse*cellsize;
	}

	EVCFGINFO->ecfg_numcells = numcells;

	/* Now update the evmk structure */

	evmk.numcells = numcells;
	evmk.cellmask = EVCFGINFO->ecfg_cellmask;
	for (i=1; i < numcells; i++)
		evmk.cell_k0base[i] =
			evmk.cell_k0base[i-1]+256*collapse*cellsize;

	evmk_replicate( &evmk, sizeof(evmk));

	/* Need to update evmk.cellid for each cell */
	
	for (i=1; i < numcells; i++) {

		*(cell_t*)(((__psunsigned_t)(&evmk.cellid) & 0x00ffffffff)
				+ evmk.cell_k0base[i]) = i;
#if defined(CELL)
		*(cell_t*)(((__psunsigned_t)(&my_cellid) & 0x00ffffffff)
				+ evmk.cell_k0base[i]) = i;
#endif /* CELL */
	}

}

/*
 * Boot routine called to initiate booting the other cells.
 *
 */
extern int dbgmon_loadstop;
extern int dbgmon_symstop;
extern int _argc;
extern __psunsigned_t _argv;
extern __psunsigned_t _envirn;
extern void startothercell(void);

void
allowcellboot()
{
	volatile struct mpconf_blk *mpconf;
	int i;

	/* first we replicate some common variables which "golden cell" has
	 * already initialized so we just copy them.
	 */

	evmk_replicate( &dbgmon_loadstop, sizeof(dbgmon_loadstop));
	evmk_replicate( &dbgmon_symstop, sizeof(dbgmon_symstop));
	evmk_replicate( &kdebug, sizeof(kdebug));

#if	IP25
	initnlock(&configLock, "configLock");
	logR10000Config();
	ecc_init();			/* ready ecc handler Scotty */
#endif

	/* let other cells come in */

	for (i = 0; i < evmk.numcells; i++) {
	    if (evmk_cellid == i)
		continue;

	    /* Note: For each cellid, the "master" processor's cpuid is the
	     * same as the cellid!
	     */
	    mpconf = &MPCONF[i];

	    /*
	     * Verify processor in kernel slave loop.
	     */
	    if (!slave_loop_ready[i]) {
 		    cmn_err(CE_WARN, 
			    "Processor #%d (0x%x/0x%x) "
			    "not ready to start CELL!\n",
			    i, cpuid_to_slot[i], cpuid_to_cpu[i]);
	    }

	    mpconf->lnch_parm = 0;
	    mpconf->rendezvous = 0;
	    mpconf->rndv_parm = 0;

	    mpconf->stack = 0;
	    mpconf->real_sp = 0;
	    flush_cache();

	    cmn_err(CE_CONT, "Starting cell %d MASTER processor #%d (0x%x/0x%x)\n", 
		    i, i, cpuid_to_slot[i], cpuid_to_cpu[i]);

	    mpconf->lnch_parm = KPHYSTO32K1(mpconf);
	    mpconf->launch = KPHYSTO32K0(startothercell);

	    /* for now, only do serial start if initmkcell_serial is set */

	    if (is_specified(arg_initmkcell_serial)) 
		while (evmk_cellbootcomplete[i] == 0)
			du_conpoll() ;
	}

}
#endif /* MULTIKERNEL */

/*
 * Finish setting up the I/O for allowboot()
 */
static void
cpu_io_setup(void)
{
	register int i;
	int cpu;

	/*
	 * If a processor locked driver is locked to a non-existent cpu
	 * then change it to lock on to the master cpu
	 * A driver that wants its intr to locked onto a non-existent cpu
	 * is handled by iointr_at_cpu()
	 */
	for (i = bdevcnt; --i >= 0; ) {
		cpu = bdevsw[i].d_cpulock;
		/* cpu starts from 0 */
		if (!cpu_isvalid(cpu))
			bdevsw[i].d_cpulock = masterpda->p_cpuid;
	}

	for (i = cdevcnt; --i >= 0; ) {
		cpu = cdevsw[i].d_cpulock;
		if (!cpu_isvalid(cpu))
			cdevsw[i].d_cpulock = masterpda->p_cpuid;
	}
}

/*
 * If the debugger isn't present, set up the NMI crash dump vector.
 */
static void
init_nmi_dump(void)
{
	/* If the debugger hasn't been there, set the vector. */
	if (!(GDA->g_nmivec)) {
		if (IS_KSEG1(nmi_dump))
			GDA->g_nmivec = KPHYSTO32K1(nmi_dump);
		else
			GDA->g_nmivec = KPHYSTO32K0(nmi_dump);
	} else {
		return;
	}

	/* Make sure the real master is the one that runs the dump code. */
	GDA->g_masterspnum = (uint)EV_GET_LOCAL(EV_SPNUM) & EV_SPNUM_MASK;
}

/* nmi_dump has set up our stack.  Now we must make everything else
 * ready and call panic.
 */
void
cont_nmi_dump()
{
	/* Wire in the master's PDA. */
	wirepda(pdaindr[master_procid].pda);

	/* there's only one cpu really running.  Don't wait for the others
	 * to reboot.  They have no stacks.
	 */
	nmi_maxcpus = maxcpus;
	maxcpus = 1;
	kdebug = 0;
	cmn_err(CE_PANIC, "User requested vmcore dump (NMI)");
}

int
cpu_isvalid(int cpu)
{
	return !((cpu < 0) || (cpu >= maxcpus) || (pdaindr[cpu].CpuId == -1));
}


void
wbflush()
{
	volatile long dummy;

	dummy = *(long *)EVERROR;
}

#ifdef MC3_CFG_READ_WAR
void
special_unlock(lock_t *lck, int spl)
{
	wbflush();
	*lck &= ~1;
	splx(spl);
}
#endif

void
adjust_nmi_handler(int cmd)
{
	unsigned int 	addr;
	static unsigned int saveaddr = 0;

	if (saveaddr == 0) saveaddr = GDA->g_nmivec;
	addr = GDA->g_nmivec;
	if (cmd < 0) {
		if ((addr == KPHYSTO32K1(nmi_dump)) || 
				(addr == KPHYSTO32K0(nmi_dump)))
			qprintf("Kernel dump on nmi\n");
		else
			qprintf("Symmon on nmi\n");
	} else {
		qprintf("Switching nmi behavior\n");
		if (addr == saveaddr) {
			if (IS_KSEG1(nmi_dump))
				GDA->g_nmivec = KPHYSTO32K1(nmi_dump);
			else
				GDA->g_nmivec = KPHYSTO32K0(nmi_dump);
		} else {
			GDA->g_nmivec = saveaddr;
		}
	}
}
