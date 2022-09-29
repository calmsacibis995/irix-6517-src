/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.49 $"

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
#include <sys/pfdat.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/mc3.h>

#if DEBUG

#include <sys/immu.h>

extern uint _tlbdropin(unsigned char *, caddr_t, pte_t);
extern void _tlbdrop2in(unsigned char, caddr_t, pte_t, pte_t);

uint
tlbdropin(
	unsigned char *tlbpidp,
	caddr_t vaddr,
	pte_t pte)
{
	if (pte.pte_vr)
#ifdef IP21		 
		ASSERT(pte.pte_cc == (PG_COHRNT_EXLWR >> PG_CACHSHFT) || \
				pte.pte_cc == (PG_TRUEUNCACHED >> PG_CACHSHFT) \
				|| pte.pte_cc == (PG_UNC_PROCORD >> PG_CACHSHFT));
#else
		ASSERT(pte.pte_cc == (PG_COHRNT_EXLWR >> PG_CACHSHFT) ||
			pte.pte_cc == (PG_COHRNT_EXCL >> PG_CACHSHFT) ||
#if R10000
			pte.pte_cc == (PG_UNCACHED_ACC >> PG_CACHSHFT) ||
#endif
			pte.pte_cc == (PG_TRUEUNCACHED >> PG_CACHSHFT));
#endif
	return(_tlbdropin(tlbpidp, vaddr, pte));
}

#if R4000 || R10000
void
tlbdrop2in(unsigned char tlb_pid, caddr_t vaddr, pte_t pte, pte_t pte2)
{
#if MP
	/* must avoid process preemption/migration after loading tlb_pid
	 * parameter and before performing the dropin ... need to guarentee
	 * this is really our pid.
	 * There are other ways to guarentee this, but for now we use splhi().
	 */
	ASSERT((tlb_pid == 0) || issplhi(getsr()));
#endif
	if (pte.pte_vr)
		ASSERT(pte.pte_cc ==  (PG_COHRNT_EXLWR >> PG_CACHSHFT) || \
				pte.pte_cc == (PG_TRUEUNCACHED >> PG_CACHSHFT));
	if (pte2.pte_vr)
		ASSERT(pte2.pte_cc ==  (PG_COHRNT_EXLWR >> PG_CACHSHFT) || \
				pte2.pte_cc == (PG_TRUEUNCACHED >> PG_CACHSHFT));
	_tlbdrop2in(tlb_pid, vaddr, pte, pte2);
}
#endif	/* R4000 || R10000 */
#endif	/* DEBUG */

extern mutex_t probe_sema;

/*
 * Initialize everest memory subsystem.
 * For the most part, memory is configured and initialized by 
 * the PROM.
 */
void
mem_init(void)
{
#if IP21
	extern lock_t vmedbe_lock, vmewrtdbe_lock;
#endif /* IP21 */

	/* initialize monitor lock for "badaddr" routines */
	mutex_init(&probe_sema, MUTEX_DEFAULT, "probe");

#if IP21
	/* initialize monitor lock for VME read DBE handling */
	initnlock(&vmedbe_lock, "vmedbe");
	initnlock(&vmewrtdbe_lock, "vmewrtdbe");
#endif /* IP21 */

	/* We only have mc3 memory boards for now */
	mc3_init();
}

static pgno_t memsize = 0;	/* total ram in clicks */

/*
 * Return size (in pages) of usable memory, without corrupting any
 * memory below startpage.
 */
/* ARGSUSED */
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
	if (memsize)
		return memsize;

#ifdef MULTIKERNEL
	memsize = BLOCs_to_pages(EVCFGINFO->ecfg_cell[evmk_cellid].cell_memsize);
#else
	memsize = BLOCs_to_pages(EVCFGINFO->ecfg_memsize);
#endif 
	if (maxpmem)
	    memsize = MIN(memsize, maxpmem);

	
	/* XXX Burn the last page.  Some devices (HIPPI and A64 VME) cannot
	 * DMA from it.
	 */
	memsize--;

	return memsize;
}

/*
 * setupbadmem - mark holes in memory as P_BAD (everest has none).
 *		 return count of bad pages.
 */
/* ARGSUSED */
pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
	return 0;
}


/*****************************************************************************
 *                              Cache Modes                                  *
 *****************************************************************************/

/*
 * pg_setcachable set the cache algorithm pde_t *p. Since there are
 * several cache algorithms for the R4000, (non-coherent, update, invalidate),
 * this function sets the 'basic' algorithm for a particular machine. 
 * Everest uses coherent exclusive on write.
 */
void
pg_setcachable(pde_t *p)
{
	pg_setcohrnt_exlwr(p);
}



uint
pte_cachebits(void)
{
	return PG_COHRNT_EXLWR;
}


/*****************************************************************************
 *           System Physical Memory Parameters /  Access Methods             *
 *****************************************************************************/


/*
 * pmem_getfirstclick - returns pfn of first page of ram
 */

pfn_t
pmem_getfirstclick(void)
{
	return (0);
}


        
/*****************************************************************************
 *           Per Node Physical Memory Parameters /  Access Methods           *
 *****************************************************************************/

/*
 * node_getfirstfree - returns pfn of first page of unused ram.  This allows
 *		  prom code on some platforms to put things in low memory
 *		  that the kernel must skip over when its starting up.
 *		  This isn't used on Everest.
 */

/*ARGSUSED*/
pfn_t
node_getfirstfree(cnodeid_t node)
{
	ASSERT(node == 0);
	return 0;
}

/*
 * getmaxclick - returns pfn of last page of ram on the given node.
 */

/*ARGSUSED*/
pfn_t
node_getmaxclick(cnodeid_t node)
{
	/* 
	 * szmem must be called before getmaxclick because of
	 * its depencency on maxpmem
	 */
	ASSERT(memsize);

	ASSERT(node == 0);
#if MULTIKERNEL
	return(BLOCs_to_pages(EVCFGINFO->ecfg_cell[evmk_cellid].cell_memsize) +
	       BLOCs_to_pages(EVCFGINFO->ecfg_cell[evmk_cellid].cell_membase) -1);

#else
	return memsize - 1;
#endif 
}
