/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.45 $"

/* heart_mem.c - memory configuration and cache flushing support */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/sysmacros.h>
#include <ksys/cacheops.h>
#include <sys/pda.h>
#include <sys/kthread.h>
#include <sys/rt.h>

#include "RACERkern.h"

/* get memory configuration word for a bank */
static __uint32_t
get_mem_conf(int bank)
{
	return *(volatile __uint32_t *)
		(PHYS_TO_COMPATK1(HEART_MEMCFG0) + bank * 4);
}

/* determine base address of a memory bank */
static paddr_t
bank_addrlo(int bank)
{
	__uint32_t memconfig = get_mem_conf(bank) & HEART_MEMCFG_ADDR_MSK;

	return (paddr_t)memconfig << HEART_MEMCFG_UNIT_SHFT;
}

/* determine the size of a memory bank, 2 GB max per bank */
static uint
banksz(int bank)
{
	__uint32_t memconfig = get_mem_conf(bank);

	if (!(memconfig & HEART_MEMCFG_VLD))
		return 0;

	memconfig &= HEART_MEMCFG_RAM_MSK;
	memconfig >>= HEART_MEMCFG_RAM_SHFT;

	return (memconfig + 1) << HEART_MEMCFG_UNIT_SHFT;
}

#define	INVALID_DIMM_BANK	-1
/*
 * determine which physical memory bank contains the given address.
 * returns 0-3 or -1 for an invalid address.  the given address is
 * an ACTUAL address, not a physical address
 */
static int
addr_to_bank(paddr_t addr)
{
	paddr_t addrlo;
	int bank;
	uint size;

	if (addr >= ABS_SYSTEM_MEMORY_BASE)
		addr -= ABS_SYSTEM_MEMORY_BASE;

	for (bank = 0; bank < 4; bank++) {
		if (!(size = banksz(bank)))
			continue;

		addrlo = bank_addrlo(bank);
		if (addr >= addrlo && addr < addrlo + size)
			return bank;
	}

	return INVALID_DIMM_BANK;        /* address not found */
}

static pgno_t memsize = 0;	/* total ram in clicks */
static paddr_t physaddr_max;	/* highest physical addr */

/* size main memory.  returns # physical pages of memory */
/*ARGSUSED1*/
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
	int bank;
	__uint64_t memsz;

	if (memsize)
		return memsize;

	for (memsz = 0, bank = 0; bank < 4; bank++)
		memsz += banksz(bank);

	memsize = btoct(memsz);			/* get pages */
	if (maxpmem)
		memsize = MIN(memsize, maxpmem);

	physaddr_max = ABS_SYSTEM_MEMORY_BASE + ctob(memsize);
	return memsize;
}

int
is_in_main_memory(paddr_t addr)
{
	return (addr >= ABS_SYSTEM_MEMORY_BASE && addr < physaddr_max) ||
		addr < SYSTEM_MEMORY_ALIAS_SIZE;
}

int
block_is_in_main_memory(paddr_t addr, size_t bytes)
{
    return is_in_main_memory(addr) && is_in_main_memory(addr + bytes - 1);
}

int
is_in_pfdat(pgno_t pfn)
{
	pgno_t min_pfn = btoct(kpbase);

	return (pfn >= min_pfn && pfn < (btoct(PHYS_RAMBASE) + memsize));
}

void
clean_icache(void *addr, uint len, uint pfn, uint flags)
{
	char *kvaddr;

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

	/* Direct-mapped cache, or only one page? */
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/* For user user addresses or indexed ops, use pfn to flush cache.
	 */
	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	}
	else
		kvaddr = addr;

	__icache_inval(kvaddr, len);
}

void
clean_dcache(void *addr, uint len, uint pfn, uint flags)
{
	char *kvaddr;

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

	/* MP:
	 * If original addr is KUSEG, then we must remap it since it may be
	 * a request from another CPU and hence not the current user map.
	 * With a 64-bit kernel we can access all of memory directly w/o
	 * the tlb, which makes it easier than the EVEREST version.
	 *
	 * If it is a K2 address, then use it without remapping, since the
	 * map is global to all CPUs and hence still setup. Also true for K0.
	 *
	 * NOTE: We also need to remap if CACH_NOADDR is set and addr was not
	 *	 a K0 address (i.e. only the pfn was passed in).
	 */
	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	}
	else
		kvaddr = addr;

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK))
		__dcache_wb_inval(kvaddr, len);
	else if (flags & CACH_INVAL)
		__dcache_inval(kvaddr, len);
	else if (flags & CACH_WBACK)
		__dcache_wb(kvaddr, len);
}

#define	PHYS_TO_K0_EXL(x)	((x) | K0BASE_EXL) /* physical to kseg0 - excl*/

/*
 * sync_dcaches_excl
 *
 * This routine not only forces other dirty primary lines to be written
 * back, but performs an exclusive access on each cache line.
 * So, this INVALIDATES the line in the other cpu's cache.
 * NOTE: This effectively performs a flush of this line from ALL of the
 * other dcaches & icaches on IP30 systems.
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
	 * 
	 * Better not be able to migrate to another cpu.
	 *
	 * splhi()/splx() works just fine and is still used early in
	 * system initialization when there is no thread and we can't use
	 * mutex_lock().
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
		*(volatile char *)(kvaddr+i);

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
		*(volatile char *)(kvaddr+i);

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK))
		rt_unpin_thread(old_rtpin);
	else
		splx(s);

	return(0);
}

#if HEART_INVALIDATE_WAR
/*
 * __heart_invalidate_war()
 *
 * Invalidate on the current CPU, and then read the line exclusive on MP
 * in an attempt to remove any stale lines caused by the son of the
 * writeback surprise bug in 2.6/3.0 T5.  We are able on a speculative
 * on the other CPU to push the data back to memory, but that can happen
 * anyway, so we are stuck.
 *
 * Caller must have pinned the thread, or be @ splhi.
 */
/* ARGSUSED */
void
__heart_invalidate_war(void *addr, unsigned int len)
{
	char *kvaddr;
	int i;

	/* If not K0, don't bother to do the translation, let cache_operation
	 * do it.  We usually call sync_dcaches(), which does a writeback
	 * invalidate, which should just invalidate the line, unless we have
	 * had a speculative store.
	 */
	if (!IS_KSEG0(addr)) {
		cache_operation(addr,len,CACH_FORCE|CACH_INVAL|CACH_SCACHE);
		return;
	}

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	kvaddr = (char *)PHYS_TO_K0_EXL(KDM_TO_PHYS(addr));

	/* Invalidate the line in our cache, then read the line exclusive
	 * from other CPUs.  If the other CPU has a dirty copy that hit's
	 * the T5 bug due to a speculative store, this will write the line
	 * back, but there isn't that much we can do at this point.
	 */
	__dcache_inval(kvaddr, len);
	if (maxcpus != 1) {
		for(i=0; i<len; i+=CACHE_SLINE_SIZE) {
			*(volatile char *)(kvaddr+i);
		}
	}

	return;
}
#endif	/* HEART_INVALIDATE_WAR */

/* return pfn of first page of ram */
pfn_t
pmem_getfirstclick(void)
{
	return btoct(PHYS_RAMBASE);
}

/*
 * getfirstfree - returns pfn of first page of unused ram.  This allows
 *		  prom code on some platforms to put things in low memory
 *		  that the kernel must skip over when its starting up.
 *		  This isn't used on Everest.
 */

/*ARGSUSED*/
pfn_t
node_getfirstfree(cnodeid_t node)
{
	ASSERT(node == 0);
	return btoct(PHYS_RAMBASE);
}

/* return pfn of last page of ram */
/*ARGSUSED*/
pfn_t
node_getmaxclick(cnodeid_t node)
{
	ASSERT(node == 0);
	ASSERT(memsize);

	return btoct(PHYS_RAMBASE) + memsize - 1;
}

/*ARGSUSED*/
pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
	__uint64_t bsize;

	/*
	 * 9/14/99. Incident #760303
	 * There is a bug in Heart Rev. f (and probably
	 * the previous revs too) with 1G DIMMs which creates an address
	 * aliasing problem with addresses 0x80000000
	 * through 0x80003fff and 0x00000000
	 * through 0x00003fff. i.e. when the kernel
	 * tries to write to, say, 0x80000080 it will
	 * also overwrite location 0x00000080. BAD!!
	 * A similiar problem also exists with .5G DIMM which
	 * affects a different 16K address range. The
	 * quick and easy solution is to just take out the
	 * page repesenting this range of memory from the kernel
	 * free page list. A loss of 16K
	 * is not significant when we are using .5G or 1G DIMMs.
	 */


	/*
	 * Since we know that a bank must contain two
	 * DIMMs of the same size (and DIMMs must be ordered
	 * from largest to smallest in the DIMM slots), 
	 * we can simply check the size of bank 0 to determine what size
	 * DIMMs are installed. DIMM size = 1/2 bank size.
	 */

	bsize = banksz(0);

	if (bsize == (__uint64_t) 0x40000000) { /* .5G DIMMs */
		/* mark the page for address 0x40000000 */
		 PG_MARKHOLE(pfntopfdat(btoct(0x40000000)));
		return (pfn_t)1;
	} else if (bsize == (__uint64_t) 0x80000000) { /* 1G DIMMs */
		/* mark the page for address 0x80000000 */
		 PG_MARKHOLE(pfntopfdat(btoct(0x80000000)));
		return (pfn_t)1;
	} else { 
		/* no holes */
		return (pfn_t)0;
	}
}
	

char                   *
maddr_to_dimm(paddr_t maddr)
{
    int                     bank;
    static char            *dimm[4][2] =
    {{ "S1", "S2" }, { "S3", "S4" }, { "S5", "S6" }, { "S7", "S8" }};

    bank = addr_to_bank(maddr);
    if (bank == INVALID_DIMM_BANK)
	return 0;

    return dimm[bank][(maddr >> 3) & 1];
}

/*
 * log the DIMM number of a detected memory error
 *	addr  - address where error was detected
 *	bad_bit - 0..63 single date bit error
 *		  64..71 single check bit error
 * never log to the console window
 */
char *
log_dimmerr(paddr_t addr, int recoverable, char bad_bit, __uint64_t bad_data,
	int hard_error)
{
	char	       *bad_dimm_str;
	char		buf[8];
	int		lvl;

	bad_dimm_str = maddr_to_dimm(addr);

	if (recoverable) {
		sprintf(buf, ", %cB%d",
			bad_bit >= 64 ? 'C' : 'D', bad_bit & 0x3f);
		lvl = CE_NOTE;
	}
	else {
		buf[0] = '\0';
		lvl = CE_ALERT;
	}

	cmn_err(lvl,
		"!%secoverable %smemory ECC Error at 0x%x (%s%s%s), Bad data: 0x%x\n",
		recoverable ? "R" : "Unr",
		hard_error ? "hard " : "", addr,
		bad_dimm_str ? "DIMM " : "Unknown ",
		bad_dimm_str ? bad_dimm_str : "DIMM",
		buf, bad_data);

	return bad_dimm_str;
}
