/*
 * os/numa/migr_engine.c
 *
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/immu.h>	
#include <sys/cmn_err.h>
#include <sys/pfdat.h>
#include <sys/pda.h>
#include <sys/kmem.h>
#include <ksys/rmap.h>
#include <sys/systm.h>	/* splhi */
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/uthread.h>
#include <sys/nodepda.h>
#include <sys/anon.h>
#include <sys/lpage.h>
#include <ksys/migr.h>
#include <sys/ktrace.h>
#include <ksys/vnode_pcache.h>
#include <sys/numa.h>
#include <sys/rtmon.h>
#include "pfms.h"
#include "migr_control.h"
#include "debug_levels.h"
#include "migr_engine.h"
#include "migr_states.h"
#include "numa_stats.h"

static int migr_page_prologue(pfd_t *, pfd_t *);
static void migr_page_epilogue(pfd_t *);

#define ptoa(page_number) ((void*)((__psint_t)(page_number) << PNUMSHFT))

/*
 * Find out whether a page could be migrated, in a very
 * loose fashion, without acquiring all required locks.
 * We purposely omit acquiring the pfdat lock.
 * The result returned by this function is only advisory.
 *     0: migratable
 *     Non-zero: not migratable
 */

int
migr_check_migratable(pfn_t pfn)
{
        pfd_t* pfd;
        int nlinks;
        int s;

        pfd =  pfntopfdat(pfn);
        
        /*
         * Loosely check whether this page is involved in
         * some kind of IO operation.
         */
        if (pfd->pf_rawcnt != 0) {
                numa_message(DM_MIGR,
                             1024,
                             "[migr_check_migratable]: rawcnt != 0", 0, 0);
                return (MIGRERR_RAWCNT);
        }

        /*
         * We now need to lock the pfdat's rmap lock
         * to verify it is currently being mapped by
         * base-pages only, and the number of reverse map
         * links is equal to the use count.
         */
        s = RMAP_LOCK(pfd);

        /*
         * First we check large pages
         */
        if (rmap_scanmap(pfd, RMAP_CHECK_LPAGE, 0) != 0) {
                numa_message(DM_MIGR, 1024, "[migr_check_migratable]: large page",
			     0, 0);
                RMAP_UNLOCK(pfd, s);
                return (MIGRERR_LPAGE);
        }
        
        /*
         * Now we verify the number of refs is equal to the number
         * of reverse map links. We won't worry about locked
         * pfn fields in the ptes. These locks are supposed to
         * be very fine grain, and the odds are that later when
         * we really need to do the migration, the lock will have
         * been released.
         */

        nlinks = rmap_scanmap(pfd, RMAP_COUNTLINKS, 0);

        ASSERT(nlinks >= 0);
        if (pfd->pf_use != nlinks) {
                numa_message(DM_MIGR, 1024, "[migr_check_migratable]: use!=links",
                             pfd->pf_use, nlinks);
                RMAP_UNLOCK(pfd, s);
                return (MIGRERR_REFS);
        }
        
        if (nlinks == 0) {
                numa_message(DM_MIGR, 1024, "[migr_check_migratable]: Zero Links",
                             pfd->pf_use, nlinks);
                RMAP_UNLOCK(pfd, s);
                return (MIGRERR_ZEROLINKS);
        }
        
        /*
         * P_BAD pages are never migratable
         */
	if ((pfd->pf_flags & P_BAD) || pfdat_iserror(pfd)) {
                numa_message(DM_MIGR, 1024, "[migr_check_migratable]: P_BAD", 0, 0);
                RMAP_UNLOCK(pfd, s);
                return (MIGRERR_PBAD);
        }

        RMAP_UNLOCK(pfd, s);
        return (0);
}
/*
 * Find out whether it makes sense to migrate a page in a very
 * loose fashion, without acquiring any locks, and just
 * checking state we can check very fast.
 * The result returned by this function is only advisory.
 *     0: migratable
 *     Non-zero: not migratable
 */

int
migr_fastcheck_migratable(pfn_t pfn)
{
        pfd_t* pfd;

        pfd =  pfntopfdat(pfn);
        
        /*
         * Loosely check whether this page is involved in
         * some kind of IO operation.
         */
        if (pfd->pf_rawcnt != 0) {
                numa_message(DM_MIGR,
                             1024,
                             "[migr_check_migratable]: rawcnt != 0", 0, 0);
                return (MIGRERR_RAWCNT);
        }

        /*
         * We really should not migrate pages with too
         * many references. For now, we'll set the limit
         * to 32 + 2 * numcpus.
         */

        if (pfd->pf_use > (32 + 2 * numcpus)) {
                numa_message(DM_MIGR,
                             1024,
                             "[migr_check_migratable]: too many refs", 0, 0);
                return (MIGRERR_REFS);
        }

        /*
         * P_BAD pages are never migratable
         */
	if ((pfd->pf_flags & P_BAD) || pfdat_iserror(pfd)) {
                numa_message(DM_MIGR, 1024, "[migr_check_migratable]: P_BAD", 0, 0);
                return (MIGRERR_PBAD);
        }

        return (0);
}

int
migr_pfdat_xfer(pfd_t* old_pfd, pfd_t* new_pfd)
{
        int errcode;
        int nlocks;
	cnt_t pf_use;
        unsigned short pf_rawcnt;
        uint pf_flags;
#ifdef _VCE_AVOIDANCE
	unsigned short pf_vcolor;
#endif /* _VCE_AVOIDANCE */
        unsigned long pf_pageno;
        void* mo;
	/* REFERENCED */
        pfn_t old_pfn;
	/* REFERENCED */
        pfn_t new_pfn;

        ASSERT(pfdat_islocked(old_pfd));

        /*
         * Verify rawcount == 0 
         */

        if (old_pfd->pf_rawcnt != 0) {
                numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: rawcnt != 0", 0, 0);
                errcode = MIGRERR_RAWCNT;
                goto migr_fail0;
        }        

        /*
         * If the page is UNDONE, then somebody is either
         * trying to migrate it already or it is in the
         * process of being brought back from its backing
         * store.
         * We don't do migration in this case.
         * Same for P_RECYCLE
         */
        if ((old_pfd->pf_flags & (P_HASH|P_DONE|P_RECYCLE)) != (P_HASH|P_DONE)) {
                numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: Page Undone", 0, 0);
                errcode = MIGRERR_UNDONE;
                goto migr_fail0;
        }
        
        /*
         * P_BAD
         */
        if ((old_pfd->pf_flags & P_BAD) || pfdat_iserror(old_pfd)) {
                numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: P_BAD", 0, 0);
                errcode = MIGRERR_PBAD;
                goto migr_fail0;
        }

	if (pfd_replicated(old_pfd)){
		numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: text page", 0, 0);
		errcode = MIGRERR_PTEXT;
		goto migr_fail0;
	}

        /*
         * We need to lock the pfdat's rmap lock
         * in order to scan all referencing ptes.
         * We punt migration if we cannot acquire it.
         */

        if (NESTED_RMAP_TRYLOCK(old_pfd) == RMAP_LOCKFAIL) {
                numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: rmap lock held", 0, 0);
                errcode = MIGRERR_RMAP;
                goto migr_fail0;
        }
        
	/*
	 * If it's a large page punt migration.
	 */

	if (rmap_scanmap(old_pfd, RMAP_CHECK_LPAGE, 0)) {
                numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: large page", 0, 0);
                errcode = MIGRERR_LPAGE;
                goto migr_fail1;
	}
        
        /*
         * Lock the ptes referencing this pfdat
         * Give up if we can't set a lock.
         * All fault handlers will have to grab this lock
         * before re-loading the translation.
         */

        nlocks = rmap_scanmap(old_pfd, RMAP_LOCKPFN, 0);

        /*
         * nlocks < 0 : Could not acquire a lock
         *              (any locks succesfully taken were released)
         * nlocks == 0 : Pfdat has no links
         * nlocks > 0 : Acquired all locks AND we have at least one link
         */
        if (nlocks < 0) {
                /*
                 * Could not acquire a lock
                 */
                numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: A pfn was locked", 0, 0);
                errcode = MIGRERR_ONELOCK;
                goto migr_fail1;
        } else if (nlocks == 0) {
                /*
                 * Zero rmap links
                 */
                numa_message(DM_MIGR, 1024, "[migr_pfdat_xfer]: Zero links", 0, 0);
                errcode = MIGRERR_ZEROLINKS;
                goto migr_fail1;
        }

        /*
         * For nlocks > 0 we proceed on
         */

        /*
         * Check whether the number of pf_use refs is equal
         * to the number of rmap links. If not, we cannot migrate.
         * Incrementing the use count without adding any rmap link
         * is the mechanism we use to effectively `hold' a pfdat
         * so that it is not migrated.
         */
        if (old_pfd->pf_use != nlocks) {
                (void)rmap_scanmap(old_pfd, RMAP_UNLOCKPFN, 0);
                numa_message(DM_MIGR, 1024,
                             "[migr_pfdat_xfer]: More refs than rmaps", 0, 0);
                errcode = MIGRERR_REFS;
                goto migr_fail1;
        }

	/*
	 * Make final check of ptes to see if there is any state that prevents migration.
	 *
	 */
	if (rmap_scanmap(old_pfd, RMAP_MIGR_CHECK, 0)) {
		(void)rmap_scanmap(old_pfd, RMAP_UNLOCKPFN, 0);
		numa_message(DM_MIGR, 1024,
				"[migr_pfdat_xfer]: pde prevents migration", 0, 0);
		errcode = MIGRERR_NOMIGR;
		goto migr_fail1;
	}

        /*
         * At this point we have to lock the pfms and verify we're
         * still in a pfms state apt for migration.
         * Note that the pfms lock we have to acqquire must
         * always be acquired last relative to all other pfdat
         * locks. In this case, the RMAP_LOCK for this pfdat was
         * acquired a few lines back.
         */

        old_pfn = pfdattopfn(old_pfd);
        new_pfn = pfdattopfn(new_pfd);

#ifdef	NUMA_BASE
        if (migr_xfer_nested_lock(old_pfn)) {
                /*
                 * Not apt for migration anymore
                 */
                errcode = MIGRERR_PFMSSTATE;
                printf("migr_xfer_nested_lock failed...\n");
                goto migr_fail1;
        }
#endif

        /*
         * Move the pfdat into the "in-migration"
         * state, and clear P_DONE.
         * Clearing P_DONE would have sufficed, except
         * that gather_chunck doesn't fully respect
         * the rules for P_DONE, making it necessary
         * for us to distinguish between a normal
         * ~P_DONE, and a ~P_DONE caused by migration.
         */
        pfdat_set_ismigrating(old_pfd);  
        pfd_nolock_clrflags(old_pfd, P_DONE);

        /*
         * Acquire the rmap lock for the new pfdat in
         * order to do the rmap transfer.
         */

        NESTED_RMAP_LOCK(new_pfd);

        /*
         * Copy reverse map pointers
         */
	rmap_xfer(old_pfd, new_pfd);
        /*
         * Transfer/Clear the pfms state
         * Release pfms lock
         */
#ifdef	NUMA_BASE
        migr_xfer_and_nested_unlock(old_pfn, new_pfn);
#endif

        /*
         * Release both rmap locks
         */
        NESTED_RMAP_UNLOCK(new_pfd);
        NESTED_RMAP_UNLOCK(old_pfd);

        /*
         * Save the old_pfdat's state
         */

        pf_use = old_pfd->pf_use;
        pf_rawcnt = old_pfd->pf_rawcnt;
#ifdef _VCE_AVOIDANCE
	pf_vcolor = (unsigned short)old_pfd->pf_vcolor;
#endif /* _VCE_AVOIDANCE */
	pf_flags = old_pfd->pf_flags;
        pf_pageno = old_pfd->pf_pageno;
        mo = pfd_getmo(old_pfd);

        /*
         * Get the old pfdat ready for full release
         * (pf_rflags and rmap fields cleared in rmap_xfer).
	 * Clear all pf_flags except P_ISMIGRATING & MIGR_POISONED_FLAG;
	 * these will be cleared later. MIGR_POISONED_FLAG is cleared
	 * later in migr_pagemove. P_ISMIGRATING must remain set in the
	 * old pfdat until we know that no other thread could be
	 * looking at the old pfdat. This doesnt occur until the pfdat
	 * is finally freed.
         */
	old_pfd->pf_flags = (old_pfd->pf_flags & ~P_ALLFLAGS) | (MIGR_POISONED_FLAG|P_ISMIGRATING);
        old_pfd->pf_use = 1;
        old_pfd->pf_rawcnt = 0;        

        /*
         * Transfer state into new pfdat
         * When we copy the flags into the new pfdat, we also
         * copy the P_BITLOCK state. We have to unlock the new pfdat
         * atfer we're done with the transfer.
         * In addition, note that we're copying the
         * ~P_DONE and "in-migration" states.
         */
	new_pfd->pf_use = pf_use;
	new_pfd->pf_rawcnt = pf_rawcnt;
        new_pfd->pf_flags = pf_flags & P_ALLFLAGS;
	new_pfd->pf_flags &= ~MIGR_POISONED_FLAG;
#ifdef _VCE_AVOIDANCE
	new_pfd->pf_vcolor = pf_vcolor;
#endif /* _VCE_AVOIDANCE */
        new_pfd->pf_pageno = pf_pageno;
        new_pfd->pf_tag = mo;
	pfdat_inc_usecnt(new_pfd); /* Get an additional reference */

        return (0);

  migr_fail1:
        NESTED_RMAP_UNLOCK(old_pfd);

  migr_fail0:
        return (errcode);
        
}

/*
 * Effectively Migrate page.
 */
/*ARGSUSED*/
void
migr_pagemove(pfd_t* old_pfd, pfd_t* new_pfd, void *cookie, int migr_vehicle_type)
{
#if _MIPS_SIM == _ABI64	/* 64 bit kernels can always PHYS_TO_K0() */
        pfn_t old_pfn;
        pfn_t new_pfn;
        paddr_t from;
        paddr_t to;

        ASSERT(old_pfd && new_pfd);

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PMOVE_ENTRY,
                            (__int64_t)old_pfd,
                            (__int64_t)new_pfd,
                            migr_vehicle_type,
                            0);

	old_pfn = pfdattopfn(old_pfd);
        new_pfn = pfdattopfn(new_pfd);
        

        /*
         * At this point nobody can access the old page.
         * If we did a TLB sync, everybody faults, and
         * finds the pde locked.
         * If we are using poisonous bits, anybody finding
         * the translation in the TLB proceeds to do the
         * access only to get a POISONOUS EXCEPTION, in which
         * case the threads  block on the pde pfnlock.
         */
        
        /*
         * Transfer the page using the BTE engine in
         * poisoning mode. Plain bcopy is used
         * in machines without poisonous bit support.
         */

        from = PHYS_TO_K0(ptoa(old_pfn));
        to = PHYS_TO_K0(ptoa(new_pfn));
        numa_message(DM_MIGR, 16, "[migr_migrate_page] from, to\n", from, to);
        
        BTE_POISON_COPY(cookie, migr_vehicle_type, (paddr_t)from, (paddr_t)to, NBPP, BTE_POISON);
        NUMA_STAT_INCR(cnodeid(),  migr_pagemove_number[migr_vehicle_type]);

#else	/* _MIPS_SIM != _ABI64 */

	/*
	 * PHYS_TO_K0() may not be valid for all paddrs for 32bit kernels.
	 * Let page_copy() do the copying (will copy via K0, if possible).
	 */
	page_copy(old_pfd, new_pfd,
#ifdef _VCE_AVOIDANCE
		pfd_to_vcolor(old_pfd), pfd_to_vcolor(new_pfd)
#else
		NOCOLOR, NOCOLOR
#endif
		);

#endif	/* _MIPS_SIM == _ABI64 */

#if defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR)
	pfd_setflags(old_pfd,P_CACHESTALE);
	pfd_setflags(new_pfd,P_CACHESTALE);
#endif /* defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR) */
  
	if (MIGR_VEHICLE_TLBSYNC(migr_vehicle_type)){
		/*
		 * Force the poisoned flag to be turned off
		 * if we are using TLBSYNC for migration. 
		 * This happens since we decide dynamically
		 * the vehicle of migration.
		 * On systems without BTE, MIGR_POISONED_FLAG is
		 * defined to be 0, and hence it's not a problem
		 *
		 * Note: can't use pfd_nolock_clrflags here. Other
		 * threads may be trying to lock the old (stale)
		 * pfd. 
		 */
		pfd_clrflags(old_pfd, MIGR_POISONED_FLAG);
	}

        
        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PMOVE_EXIT,
                            (__int64_t)old_pfd,
                            (__int64_t)new_pfd,
                            migr_vehicle_type,
                            0);       
}

#if	defined(IP32)

#define	MAX_INTERRUPT_LATENCY	200000	/* 2msec for 200MHZ R10000 */

#ifdef	DEBUG
#define	O2_DISABLE_PREEMPT(start_time, s) \
	s = splhi(); \
	start_time = (uint_t)get_r4k_counter()

#define	O2_ENABLE_PREEMPT(s, start_time) \
	ASSERT (((uint_t)get_r4k_counter() - start_time) \
		< MAX_INTERRUPT_LATENCY);	\
	splx(s)
#else
#define	O2_DISABLE_PREEMPT(start_time, s) \
	s = splhi()
#define	O2_ENABLE_PREEMPT(s, start_time) \
	splx(s)
#endif /* DEBUG */

#else
#define O2_DISABLE_PREEMPT(start_time, s)
#define O2_ENABLE_PREEMPT(s, start_time) 
#endif
	

static int
migr_page_prologue(pfd_t *old_pfd, pfd_t *new_pfd)
{
        int	oldspl;
	/* REFERENCED */
	int	s;
        int	errcode;
        pfn_t new_pfn;
	/* REFERENCED */
	uint_t	start_time;

	O2_DISABLE_PREEMPT(start_time, s);
        new_pfn = pfdattopfn(new_pfd);
        
        
        /*
         * Verify the page's state is OK to migrate
         * and if so, do the pcache exchange.
         */

        oldspl = pfdat_lock(old_pfd);
        if (pfd_isanon(old_pfd)) {
                /*
                 * It's an anon memory object
                 */
                void* ap = pfd_getmo(old_pfd);
                anon_hold(ap);
                pfdat_unlock(old_pfd, oldspl);
                if ((errcode = anon_pagemigr(ap, old_pfd, new_pfd)) != 0) {
                        /*
                         * The anon_hold is released in anon_pagemigr
                         */
			O2_ENABLE_PREEMPT(s, start_time);
                        return (errcode);
                }
                /*
                 * The anon_hold is released in anon_pagemigr
                 */ 
        } else if (pfd_isincache(old_pfd)) {
                /*
                 * It's a vnode memory object
                 */
                void* vp = pfd_getmo(old_pfd);
                vnode_hold(vp);
                pfdat_unlock(old_pfd, oldspl);
                if ((errcode = vnode_pagemigr(vp, old_pfd, new_pfd)) != 0) {
                        /*
                         * The vnode_hold is released in vnode_pagemigr
                         */
			O2_ENABLE_PREEMPT(s, start_time);
                        return (errcode);
                }
		/*
		 * The vnode_hold is released in vnode_pagemigr
		 */
        } else {
                pfdat_unlock(old_pfd, oldspl);
		O2_ENABLE_PREEMPT(s, start_time);
		return (MIGRERR_ZEROLINKS);
	}

#if _MIPS_SIM != _ABI64
	pfd_setflags(old_pfd,P_CACHESTALE);
#endif
  
        /*
         * Invalidate the page table entries mapping this page
         * Every cpu accessing this page is forced to
         * take a vfault, where they have to wait for the per
         * pte pfn lock to be released before they can proceed to
         * reload the translation. Either the TLB shootdown or
         * the poisonous bits take care of translations found
         * in the TLB.
         */
        oldspl = RMAP_LOCK(new_pfd);
	(void)rmap_scanmap(new_pfd, RMAP_CLRVALID, 0);

        /*
         * Update pfns in all referencing ptes & release locks
         * Note that at this stage the pfn field and the pfn lock
         * are fully idempotent; pde's that are unlocked can be
         * unlocked again, and pde's with the correct pfn can be
         * set with the correct pfn again.
         */
        (void)rmap_scanmap(new_pfd, RMAP_SETPFN_AND_UNLOCK, &new_pfn);

        RMAP_UNLOCK(new_pfd, oldspl);               

	O2_ENABLE_PREEMPT(s, start_time);
	return 0;
}



static void
migr_page_epilogue(pfd_t *new_pfd)
{
        int oldspl;
	int	s;
	int	do_pagefree_sanon = 0;

        
        /*
         * We are NOT freeing the old page frame.
         */

	/*
	 * Prevent preemption while doing page done and setting the valid
	 * bit. This has to be done atomically. Otherwise vfault() gets
	 * confused.
	 */
	s = splhi();

        /*
         * Set P_DONE, wake up all those threads that tried to use
         * the new pfdat before we actually copied the data over.
         */
        
        pagedone(new_pfd);


	oldspl = RMAP_LOCK(new_pfd);
	(void)rmap_scanmap(new_pfd, RMAP_SETVALID, 0);
	RMAP_UNLOCK(new_pfd, oldspl);

        /*
         * Clear the "in-migration" state
         */
        nested_pfdat_lock(new_pfd);
	KTRACE_ENTER(vnode_ktrace, VNODE_MIGR_EPILOGUE, 
			new_pfd->pf_tag, new_pfd, new_pfd->pf_flags);
        pfdat_clear_ismigrating(new_pfd);

        /*
         * To avoid the race between pagefreesanon and migration
         * where pagefreesanon is setting the P_SANON flag while
         * we're trying to check it, get rid of the extra reference
         * now before unlocking the pfdat lock.
         */
	if (new_pfd->pf_use > 1) {
		new_pfd->pf_use--;
		nested_pfdat_unlock(new_pfd);
		splx(s);
		return;
	}

	/*
	 * If the SANON flag is set, this means that the page
	 * was an sanon page and we have the last reference.
	 * Call pagefreesanon to put the page in sanon queue.
	 */
	if (new_pfd->pf_flags & P_SANON)
		do_pagefree_sanon++;
		
        nested_pfdat_unlock(new_pfd);

	/*
	 * Equivalent of pfdat_release() but takes care of SANON pages.
	 */
	if (do_pagefree_sanon)
		pagefreesanon(new_pfd, 0);
	else 
		pagefreeanon(new_pfd);

	splx(s);
}


/*
 * Migrate a page to a specified new page frame
 * 
 */
int
migr_migrate_frame(pfd_t* old_pfd, pfd_t* new_pfd)
{
	int	errcode;
	void	*bte_cookie;
	int	migr_vehicle_type;

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_FRAME_ENTRY,
                            (__int64_t)old_pfd,
                            (__int64_t)new_pfd,
                            0,
                            0);        

	migr_vehicle_type = MIGR_VEHICLE_TYPE_GET();
        /*
         * We need to acquire the BTE we will
         * use to do the actual transfer.
         */
	if (MIGR_VEHICLE_BTE(migr_vehicle_type)){
		bte_cookie = BTE_ACQUIRE();
		if (!bte_cookie) {
			return (MIGRERR_BTE);
		}
	}

        
	if (errcode = migr_page_prologue(old_pfd, new_pfd)) {
		if (MIGR_VEHICLE_BTE(migr_vehicle_type))
			BTE_RELEASE(bte_cookie);
		return errcode;
	}

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_FRAME_TLBSTART,
                            0,
                            0,
                            0,
                            0);
        
        /*
         * We've locked the page for migration.
         * We have the following resources:
         * -- BTE is taken
         * -- all pfn locks in ptes are taken
         * -- the new page is in the cache, UNDONE
         */

        /*
         * Do a TLB shootdown on machines without POISON bits
         */
	if (MIGR_VEHICLE_TLBSYNC(migr_vehicle_type)){
		MIGR_TLB_SYNC();
	}

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_FRAME_TLBEND,
                            0,
                            0,
                            0,
                            0);           

	/*
	 * migr_pagemove Uses BTE if it is enabled.
         */
        
        migr_pagemove(old_pfd, new_pfd, bte_cookie, migr_vehicle_type);

	if (MIGR_VEHICLE_BTE(migr_vehicle_type)){
		BTE_RELEASE(bte_cookie);
	}

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_FRAME_CACHESTART,
                            0,
                            0,
                            0,
                            0);           

	/* If we're NOT using the BTE for the migration, then we may have
	 * dirty data in the primary dcache for the new page location.  If
	 * we were to execute this new page we might execute the previous
	 * contents of the new page since the cache is not write-through.
	 * So we use a cache_operation to update our local cache.
	 */
	if (!MIGR_VEHICLE_BTE(migr_vehicle_type)) {	
#if defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR)
		caddr_t kvaddr;
 
		kvaddr = page_mapin(new_pfd,0,0);
		cache_operation(kvaddr,
		                NBPP,
		                CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
		page_mapout(kvaddr);
		if (new_pfd->pf_flags & P_CACHESTALE)
			pageflags(new_pfd, P_CACHESTALE, 0);
#else /* defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR) */
		cache_operation((void*)PHYS_TO_K0(ptoa(pfdattopfn(new_pfd))),
			NBPP,  CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
#endif /* defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR) */
	}

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_FRAME_CACHEEND,
                            0,
                            0,
                            0,
                            0);            
        
	migr_page_epilogue(new_pfd);

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_FRAME_EXIT,
                            0,
                            0,
                            0,
                            0);            

	return 0;
}

/*
 * Migrate a page to a destination node
 */
/* ARGSUSED */
int 
migr_migrate_page(pfn_t old_pfn, cnodeid_t dest_node, pfn_t* dest_pfnp)
{
	pfd_t* old_pfd;
        pfd_t* new_pfd;
        int    errcode;

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PAGE_ENTRY,
                            old_pfn,
                            dest_node,
                            0,
                            0);
        
        *dest_pfnp = old_pfn;
        old_pfd = pfntopfdat(old_pfn);
        
        /*
         * We need to allocate a new page.
         * For testing functionality we don't need this
         * page to come from a different node.
         */

        new_pfd = pagealloc_node(dest_node, pfntocachekey(old_pfn, NBPP), VM_MVOK);
        if (new_pfd == NULL) {
                numa_message(DM_MIGR,
                             12,
                             "[migr_engine]: page_alloc failed", 0, 0);

                VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PAGE_FAIL,
                                    0,
                                    0,
                                    0,
                                    MIGRERR_NOMEM);                
                
                return (MIGRERR_NOMEM);
        }

        if ((errcode = migr_migrate_frame(old_pfd, new_pfd)) != 0) {
                pagefree(new_pfd);
                
                VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PAGE_FAIL,
                                    0,
                                    0,
                                    0,
                                    errcode);  
                
                return (errcode);
        }

	/* Free the old page */ 
        pagefree(old_pfd);

        *dest_pfnp = pfdattopfn(new_pfd);
        
        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PAGE_EXIT,
                            0,
                            0,
                            0,
                            0);  

        
        return (0);
}


/*
 * Migrate a list of pages
 */
void
migr_migrate_page_list(struct migr_page_list_s* migr_page_list,
                       int len,
                       int caller)
{
	int			i;
	int			pages_to_migrate = 0;
	migr_page_list_t	*mpl;
	void			*bte_cookie;
	int			migr_vehicle_type;

	if (len == 0) {
                return;
        }

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PLIST_ENTRY,
                            len,
                            caller,
                            0,
                            0);        

	migr_vehicle_type = MIGR_VEHICLE_TYPE_GET();

	mpl = migr_page_list;
	mc_msg(DC_MIGR, 10, "mpl 0x%x len 0x%x\n", migr_page_list, len);

	/*
	 * Acquire BTE at the begining. 
	 * Once we do the prologue portion successfully, there
	 * is no going back with errors. So, we cannot return 
	 * due to errors in BTE_ACQUIRE. 
	 * Fix now is to acquire BTE for the entire duration.
	 */

	if (MIGR_VEHICLE_BTE(migr_vehicle_type)){
		bte_cookie = BTE_ACQUIRE();
		if (!bte_cookie) {
			/* 
			 * Can't acquire BTE, mark all pages as error and
			 * return.
			 */
			for (i = 0; i < len; i++, mpl++) {
				mpl->migr_err = MIGRERR_BTE;
			}
                        
                        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PLIST_FAIL,
                                            0,
                                            0,
                                            0,
                                            MIGRERR_BTE); 
			return;
		}
	}

	for (i = 0; i < len; i++, mpl++) {
		/*
		 * If somebody has already allocated a new page,
		 * skip allocating a page.
		 */
		if (caller != CALLER_COALDMIGR) {
			mpl->new_pfd = pagealloc_node(mpl->dest_node, 
				pfntocachekey(pfdattopfn(mpl->old_pfd), NBPP), VM_MVOK);

			if (mpl->new_pfd == NULL) {
				mpl->migr_err = MIGRERR_NOMEM;
				continue;
			}
		}

                mc_msg(DC_MIGR, 10,
                       " prologue mpl 0x%x oldpfd 0x%x newpfd 0x%x\n",
                       mpl, mpl->old_pfd, mpl->new_pfd);
        
		if (mpl->migr_err = migr_page_prologue(mpl->old_pfd, mpl->new_pfd)) {
			pagefree(mpl->new_pfd);
		} else {
			mpl->migr_err = 0;
			pages_to_migrate++;
		}
	}
	
	if (pages_to_migrate == 0) {
		if (MIGR_VEHICLE_BTE(migr_vehicle_type)) {
			BTE_RELEASE(bte_cookie);
                }

                VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PLIST_FAIL,
                                    0,
                                    0,
                                    0,
                                    0); 
                
		return;
	}

	if (MIGR_VEHICLE_TLBSYNC(migr_vehicle_type)) {
		MIGR_TLB_SYNC();
        }
		
	mpl = migr_page_list;
	for (i = 0; i < len; i++, mpl++)  {
		if (!(mpl->migr_err)) {
			mc_msg(DC_MIGR, 10, 
				"Migr page move old_pfd 0x%x new_pfd 0x%x\n",
						mpl->old_pfd, mpl->new_pfd);
			migr_pagemove(mpl->old_pfd,
                                      mpl->new_pfd,
                                      bte_cookie,
                                      migr_vehicle_type);
		}
        }
	if (MIGR_VEHICLE_BTE(migr_vehicle_type)) {
		BTE_RELEASE(bte_cookie);
        }

	/* If we're NOT using the BTE for the migration, then we may have
	 * dirty data in the primary dcache for the new page location.  If
	 * we were to execute this new page we might execute the previous
	 * contents of the new page since the cache is not write-through.
	 * So we use a cache_operation to update our local cache.
	 */
	if (!MIGR_VEHICLE_BTE(migr_vehicle_type)) {
#if defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR)
		caddr_t kvaddr;

		mpl = migr_page_list;
		for (i = 0; i < len; i++, mpl++)  {
			if (!(mpl->migr_err)) {
				kvaddr = page_mapin(mpl->new_pfd,0,0);
				cache_operation(kvaddr,
				                NBPP,
				                CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
				page_mapout(kvaddr);
				if (mpl->new_pfd->pf_flags & P_CACHESTALE)
					pageflags(mpl->new_pfd, P_CACHESTALE, 0);
			}
		}
#else /* defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR) */
                extern int dcache_size;
		cache_operation((void*)K0BASE,
                                dcache_size,
                                CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
#endif /* defined(_VCE_AVOIDANCE) || defined(MH_R10000_SPECULATION_WAR) */				
	}
        
	mpl = migr_page_list;
	for (i = 0; i < len; i++, mpl++) {
		if (!(mpl->migr_err)) {
                        mc_msg(DC_MIGR,
                               10,
                               " epilogue mpl 0x%x oldpfd 0x%x newpfd 0x%x\n",
                               mpl,
                               mpl->old_pfd,
                               mpl->new_pfd);
			migr_page_epilogue(mpl->new_pfd);

			if (caller != CALLER_COALDMIGR) {
				pagefree(mpl->old_pfd);
			} 
		}
        }

        VM_LOG_TSTAMP_EVENT(VM_EVENT_MIGR_PLIST_EXIT,
                            0,
                            0,
                            0,
                            0); 
        
}

#ifdef DEBUG_PFNLOCKS
void
migr_verify_locks(pfn_t pfn, int errcode)
{
        int error_count;
        int s;
        pfd_t *pfd;
        
        ASSERT(curuthread);

        pfd = pfntopfdat(pfn);

        s = RMAP_LOCK(pfd);
        error_count = rmap_scanmap(pfd,
                                   RMAP_VERIFYLOCKS,
                                   (void*)(__uint64_t)current_pid());
        RMAP_UNLOCK(pfd, s);
        
        if (error_count != 0) {
                printf("[PFNLOCK ERROR]: error_count: %d, pfn: 0x%x, errcode: %d\n",
                       error_count, pfn, errcode);
                ASSERT(0);
        }
}

#define MIGR_VERIFY_LOCKS(pfn, ec) migr_verify_locks((pfn), (ec))

#else /* ! DEBUG_PFNLOCKS */
#define MIGR_VERIFY_LOCKS(pfn, ec)
#endif /* ! DEBUG_PFNLOCKS */


#ifdef	NUMA_BASE
/*
 * Check if a page is poisoned due to migration. If the page is marked 
 * poisoned in the pfdat and it has no reverse map entries and it is not
 * in any hash chains, we claim it was migrated.
 */
int
migr_check_migr_poisoned(paddr_t paddr)
{
	pfd_t *pfd;
	if (page_ispoison(paddr)) {
		pfd = pfntopfdat(pnum(paddr));

		if ((GET_RMAP_PDEP1(pfd) == 0) &&
			 ((pfd->pf_flags & P_HASH) == 0))
		    return 1;
	}
	
	return 0;
}
	    
/*
 * Given a page, unmap it from tlb if it is in a poisoned state.
 */
void
migr_page_unmap_tlb(paddr_t paddr, caddr_t vaddr)
{
	int s;
	pfn_t		tlbpfn, pfn = pnum(paddr);
	pfd_t *pfd 	= pfntopfdat(pfn);
	uint 		page_mask;
	k_machreg_t 	tlblo;

	/*
	 * Check if the page is marked as poisoned. If so, we just got an
	 * access and need to ensure the next access to the page is via
	 * a new pte that replaced the current one. Mask off interrupts, so
	 * that tlb state does not change.
	 */
	if (!pfdat_ispoison(pfd))
	    return;

	/* no migration */
	s = splhi();

	if ((tlblo = tlb_probe(tlbgetpid(), vaddr, &page_mask)) != -1) {
		tlbpfn = (tlblo & TLBLO_PFNMASK) >> TLBLO_PFNSHIFT;
		/*
		 * tlbpfn is now a 4k pfn.
		 * Convert from a 4k pfn to the pfn of required page size
		 */
		tlbpfn = pnum((__uint64_t)tlbpfn << MIN_PNUMSHFT);
		if (tlbpfn == pfn) 
		    unmaptlb(curuthread->ut_as.utas_tlbpid, btoct(vaddr));
	}
	splx(s);

	return;
}
#endif
