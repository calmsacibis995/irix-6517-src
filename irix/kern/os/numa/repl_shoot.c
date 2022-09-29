#if defined(NUMA_REPLICATION)
/*****************************************************************************
 * repl_shoot.c
 *
 *	This file defines the interfaces defined for shooting down 
 *	replication, and also describes the algorithm used to shoot
 *	down the replicas. 
 *	
 *	Refer to the detailed comment before page_replica_shoot for
 *	shootdown algorithm.
 *	
 *	
 * Copyright 1995, Silicon Graphics, Inc.
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
 ****************************************************************************/

#ident "$Revision: 1.16 $"

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/vnode.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>              /* offtoc */
#include <sys/kmem.h>
#include <ksys/rmap.h>
#include <sys/buf.h>                    /* chunkpush, */
#include <sys/systm.h>                  /* splhi */
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/proc.h>
#include <sys/ktrace.h>
#include <sys/sysmp.h>                  /* numainfo     */
#include "replattr.h"
#include "repldebug.h"

/*
 * pfd_replnext macro.
 * Internally use pf_hchain field to tie together all the pages that
 * are to be freed as part of shooting down replicas.
 */
#define	pfd_replnext(P)	((P)->pf_hchain)


#define	PAGE_PINNED		1
#define	PAGE_SHOOT_FAILED	2

#ifdef	DEBUG

/* 
 * Debugging routines 
 * These will get moved to page cache code ? once it gets checked in.
 * 
 */

/*
 * page_scanfor_replica:
 *	After shooting down all replicated pages in a vnode,
 *	scan if there any replicas in the  vnode.
 */
static void
page_scanfor_replica(vnode_t *vp, pgno_t first, pgno_t last)
{
	int	locktoken;
	pfd_t	*pfd;

	for ( ; first <= last ; first++) {

		pfd = vnode_plookup(vp, first);

		if (pfd ) {
			locktoken = pfdat_lock(pfd);

		    	if (pfd->pf_vp == vp && pfd->pf_pageno == first){
				ASSERT(pfd_replicated(pfd) == 0);
			}
			pfdat_unlock(pfd, locktoken);
		}

	}
}


#else
#define	page_scanfor_replica(vp, f, l)
#endif	/* DEBUG */

/*
 * sleep_wait
 *	sleep a bit to let the busy resource being waited for, to be
 * 	released.
 *	Hashes the pfd to identify the semaphore to sleep on, and 
 *	sleeps for "ticks" time. Wake up is done automatically
 *	by setting a timeout.
 */
static void
sleep_wait(pfd_t *pfdp, int ticks)
{
	int	s;
	/*
	 * Routine to let the caller wait while sleeping.
	 * duration of sleep is <= ticks
	 * using cvsema() avoids sending extra vsema when no one
	 * is waiting. This would avoid psema returning without
	 * waiting.
	 * Do timeout/psema under splhi() to avoid pre-emption.
	 */

	s = splhi();
	timeout_nothrd((void (*)())cvsema, REPLSEMA(pfdp), ticks);
	psema(REPLSEMA(pfdp), PZERO);
	splx(s);
}


/*
 * shootpage:
 * Do whatever needed to shoot down the page.
 *
 * Assumes caller holds an extra reference to the page 
 * to be shotdown, so that the page doesnot get totally freed.
 *
 * Returns REPLOP_SUCCESS if successful in shooting down the page. 
 * PAGE_PINNED otherwise (Only error case for now).
 */

/* 
 * number of iterations of busy wait trying to match the lock count to
 * number of users of the page, before deciding to sleep 
 */
#define	LOCKMATCH_COUNT	32

/*
 * Number of iterations to loop trying to lock all pfns, before deciding to 
 * do a * sleep wait.
 */
#define	LOCKWAIT_COUNT	10


#define	FAIL_MSG "Failed to shoot down pfd %x tag %x use %d locks %d. Retrying\n"

/*ARGSUSED*/
static int
shootpage(pfd_t *pfdp)
{
	int	lockpfncnt;
	int	lockwaitcnt;
	int	lockmatchcnt;
	int	locktoken;
	
	/*
	 * Ensure that page is suitable to be shot down.
	 */
	ASSERT(!(pfdp->pf_flags & (P_ANON|P_DIRTY|P_QUEUE|P_DQUEUE|P_SQUEUE)));

	ASSERT(pfdp->pf_flags & P_HASH);

	KTRACE_ENTER(vnode_ktrace, 	
			VNODE_PCACHE_REPLSHOOT_PAGE, pfdp->pf_vp, pfdp, 0);

	lockmatchcnt = LOCKMATCH_COUNT;
	while (1) {
		/* 
		 * if page is pinned, return failure.
		 */
		if (pfdat_pinned(pfdp))
			return PAGE_PINNED;

		lockwaitcnt = LOCKWAIT_COUNT;

		/* Grab pfnlock on all ptes attached */
		do {
			locktoken = RMAP_LOCK(pfdp);
			lockpfncnt = rmap_scanmap(pfdp, RMAP_LOCKPFN, 0);
			RMAP_UNLOCK(pfdp, locktoken); 

			if ((lockpfncnt < 0) && (--lockwaitcnt == 0)){
				sleep_wait(pfdp, 1);

				/* While sleeping, page could have been 
				 * pinned down. Check for it.
				 */
				if (pfdat_pinned(pfdp))
					return PAGE_PINNED;

				lockwaitcnt = LOCKWAIT_COUNT;
			}

		} while(lockpfncnt < 0);

		/* 
		 * Check if pf_use is same as (lockpfncnt + 1)
		 *  (+ 1) is to account for the extra reference held
		 * by the caller.
		 */
		if ((lockpfncnt + 1) != pfdp->pf_use) {
			/* Some one else is holding a reference count. 
			 * wait for them to complete.
			 * As long as two processes does not try to 
			 * shoot down the same page, it will not lead 
			 * to a dead-lock.
			 */
			locktoken = RMAP_LOCK(pfdp);
			lockpfncnt = rmap_scanmap(pfdp, RMAP_UNLOCKPFN, 0);
			RMAP_UNLOCK(pfdp, locktoken); 

			/* Sleep every alternate loop */
			if (--lockmatchcnt & 1){
				sleep_wait(pfdp, 1);

				/*
				 * After sleep_wait, check if page can
				 * still be shot down.
				 */
				if (pfdat_pinned(pfdp))
					return	PAGE_PINNED;
			}

			if (lockmatchcnt == 0){
#ifdef	DEBUG
				cmn_err(CE_NOTE|CE_CPUID, FAIL_MSG, 
					pfdp, pfdp->pf_vp, pfdp->pf_use, 
					lockpfncnt);
				debug("ring");
#endif	/* DEBUG */
				lockmatchcnt = LOCKMATCH_COUNT;
			}
			continue;

		}
		/* Lock count matches the use count. Outta while loop. */
		break;
	}

	if (lockpfncnt) {
		int count;
		/*
		 * We now have all the locks needed. Shoot down the page.
		 * RMAP_SHOOTPFN has the side effect of 
		 * unlocking the pfns, and freeing up any
		 * associated data structures (reverse maps)
		 */
		locktoken = RMAP_LOCK(pfdp);
		count = rmap_scanmap(pfdp, RMAP_SHOOTPFN, 0);
		RMAP_UNLOCK(pfdp, locktoken);
		ASSERT(lockpfncnt == count);

		/* 
		 * lockpfncnt is the number of reverse maps that were 
		 * shot down. So we need to free page that many times.
		 * Page will not be completely freed since caller has
		 * an extra reference.
		 */
		while(count--)
			pagefree(pfdp);
	}

	ASSERT(pfdat_pinned(pfdp) == 0);

	return REPLOP_SUCCESS;
}


/*
 * page_replica_free
 *	Free all the pages that were shot down. These pages were placed
 *	on a private list headed by pfdlist while shooting down the pages,
 *	and are chained via whatever pfd_replnext translates to .
 *
 *	Since an extra reference count is held as part of shooting down
 *	on these pages, they could not have gone away.
 */

static void
page_replica_free(pfd_t *pfdlist, int freepgcnt)
{
	pfd_t	*pfd;
	pfd_t	*pfdnext;
	/*REFERENCED*/
	int 	rval;

	tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
	/* 
	 * pfdlist points to head of a list of replicated page
	 * which need to be freed. 
	 * Pages chained via pfdlist are private (not visible on 
	 * hash chain either.) linked via whatever pfd_replnext translates to.
	 *
	 * Scan through the list, and free all the pages. 
	 *
	 * Since this is on a private list, no need to hold any locks.
	 * holding any locks.
	 */

	pfd = pfdlist;

	while (pfd) {
		pfdnext = pfd_replnext(pfd);
		pfd_replnext(pfd) = 0;

		ASSERT(!pfd->pf_pdep1 && !pfd->pf_pdep2);
		ASSERT(pfd->pf_use == 1);

		rval = pagefree(pfd);

		ASSERT(rval);

		freepgcnt--;
		pfd = pfdnext;
	}

	if (freepgcnt != 0 )
		cmn_err(CE_PANIC, 
		"page_replica_free: frepgcnt: (%d) != 0", freepgcnt);
}

/*
 * page_replica_shoot
 * 	Shoot down replicated pages in the range "first:last" 
 *
 * 	This is invoked when a vnode has replicas, and a new process
 *	wishes to map this vnode for writing. So, we need to 
 *	shoot down all replicas.
 *	
 * Algorithm.
 *    
 *	First change the state of vnode to be "transient". This prevents
 *	creation of all further replicas for this vnode.
 *
 *	Wait for any other replication process that may be going on to 
 *	complete. This avoids the race between the replicators, and
 *	shooting down mechanism.
 *
 *	
 * Shoot down mechanism:
 *	Shoot down process tries to remove all replicated pages from this
 *	vnode. (We try to avoid taking out "all" pages from the vnode.)
 *	If there are any pages with "non-zero" raw count, allow one such 
 *	page for each pageno on vnode to be left on the page cache.
 *	
 *	As part of shooting down, 
 *		Invoke plookup() to get the first page in the hash chain
 *		Invoke pnext() to get pages corresponding to this page number
 *		beyond this on the hash chain,	and shoot down those pages.
 *		(Since no new replica would be created, no new page will
 *		be introduced before "first page".

 *	Special handling if pages with (rawcnt > 0 ) exist.
 *		If vnode has pages with rawcnt > 0, then these pages cannot
 *		be shot down. But we can allow one such page to exist for
 *		each page number on the vnode. So, while shooting down,
 *		if you encounter a page with non-zero rawcnt, make this to
 *		be the page that will be left in the hash chain. If you
 *		detect more then one such page in the hash chain, 
 *		return error.
 *
 *	Shooting down a page.
 *		First, get an extra reference on the page so that it won't
 *		go away.
 *		Scan through the rmaps, and lock pfns. If the number of
 *		locks obtained doesnot match the (usecount - your reference)
 *		drop all locks, wait for a while and retry.
 *
 *		Once you get all locks, call rmap_makeshotdn() to 
 *		"Clear valid bit, zero pfn, set shotdn bit, and release lock".
 *		Any spare rmaps are released as part of this operation.
 *
 *	Releasing pages shot down:
 *
 *	  	If successful in shooting down the page, add this page to a 
 *	  	private list (remember you are still holding the reference. So 
 *	  	no one else can get the page yet. Private list is connected 
 *	  	via pf_hchain. So, this makes it necessary to completely 
 *		unhash these pages.
 *
 * 	TLBSync Optimization:
 * 		Algorithm here uses tlbsync() regularly to disassociate the
 * 		mapping between the user process virtual address and 
 * 		the replicated pages, which is toooo expensive for SN0.
 * 		On the other hand, poisoning pages has the overhead of
 * 		using up bandwidth, particularly if the number of
 * 		pages being freed is really large.
 *
 * 		Perhaps an intermediate step where we use poisoning if there
 * 		are a few pages, and tlbsync() if there are large number
 * 		of pages may be best fit.
 *
 * 		We want to avoid doing tlbsync each time a replicated page 
 * 		is removed. 
 * 
 */ 


/* Shoot this many pages before calling tlbsync */
#define	MIN_FREEPGCNT	32	

#ifdef	DEBUG
int	repl_page_already_free;
#endif

int
page_replica_shoot(
	struct	vnode	*vp,
	replattr_t	*replattr,
	pgno_t		first,
	pgno_t		last)
{
	pfd_t		*pfd_next;
	pfd_t		*pfd_first;
	pfd_t		*pfdlist_head;
	int		locktoken;
	pgno_t		pgno;
	int		retval;
	int		freepgcnt;
	int		pages_shotdn;

#if REPL_DEBUG
	int		repl_pages = 0;
	int		free_pages = 0;
	int		busy_pages = 0;
#endif 


	ASSERT(VN_ISREPLICABLE(vp) && replattr);

	REPL_PAGE_STATS.replshootdn++;

	pfdlist_head = 0;

	ASSERT (last > first);	

	/*
	 * If vnode doesnot have any pages,  or if there are no replicated
	 * pages attached at this time, mark the attribute blocked, 
	 * and return.
	 */
	if (!vp->v_pgcnt || !replattr->replpgcnt){
		return REPLOP_SUCCESS;
	}


	/* Push buffers mapping [first,last]
	 * Call chunkpush to blow away the pages in the range
	 * first:last. It's a little drastic than what is right
	 * Ideally we would like an interface that blows all but one
	 * page for each page number.
	 * We have to acquire the VOP_RWLOCK() for write across
	 * the call to chunkpush() because that is what is
	 * required by the file systems and chunk code.
	 */
	VOP_RWLOCK(vp, VRWLOCK_WRITE);
	(void) chunkpush(vp, ctooff(first), ctooff(last + 1) - 1, B_STALE);
	VOP_RWUNLOCK(vp, VRWLOCK_WRITE);

	retval       = 0;
	freepgcnt    = 0;
	pages_shotdn = 0;

	VNODE_PCACHE_LOCK(vp, locktoken); 

	KTRACE_ENTER(vnode_ktrace, 	
			VNODE_PCACHE_REPLSHOOT_START, vp, first, last);
	/* 
	 * Scan through the range of pages to be shot down, and
	 * try to shoot down each page. Place pages shotdown in
	 * a private list to release later. In case there is some
	 * error in shoot down bailout.
	 */
#ifdef	REPL_DEBUG
	cmn_err(CE_NOTE|CE_CPUID, 
		"Shooting down %d replicated pages in vnode 0x%x @ %x\n", 
			replattr->replpgcnt, vp, lbolt);
	repl_pages = replattr->replpgcnt;
#endif
	for (pgno = first; pgno < last; pgno++) {
		int	pinned_page_count;

		/*
		 * For each logical page number, find all pages that
		 * are in the cache, and shoot them down if possible.
		 * If any of these pages has rawcnt set,
		 * we cannot shoot that page. But,
		 * We can have upto ONE pinned page for each 
		 * logical page in the vnode. So, keep track of 
		 * pinned_page_count down pages.
		 */

		if (!replattr->replpgcnt) {
			/* No more pages. quit the loop */
			break;
		}

		pfd_first = vnode_pfind_nolock(vp, pgno, VM_ATTACH);
		if (!pfd_first)
			continue;

		pinned_page_count = pfdat_pinned(pfd_first) ? 1 : 0;

		/*
		 * pfd_first is the first page in the page cache for pgno
		 * Scan through page cache calling vnode_pnext.
		 * For each page, 
		 *	if free, remove from page cache.
		 *	if pinned, see if it can be switched with pfd_first
		 *	or else return error.
		 *	if succesful in shooting down, place it in private list
		 *	if not successful, return error.
		 */ 
		while (pfd_next = pcache_next(VNODE_PCACHE(vp), pfd_first)) {

			ASSERT(pfd_next);
			ASSERT(pfd_next != pfd_first);

			nested_pfdat_lock(pfd_next);
			if (pfd_next->pf_flags & P_QUEUE) {
				/*
				 * Page is free. Remove it from the 
				 * hash chain, in order to avoid this
				 * page being handed over to another
				 * process 
				 * While shooting down, we can't leave
				 * any replicas on the hash.
				 */
#ifdef	REPL_DEBUG
				free_pages++;
#endif	/* REPL_DEBUG */

				ASSERT(pfd_next->pf_use == 0);
				nested_pfdat_unlock(pfd_next);
				vnode_premove_nolock(vp, pfd_next);
				continue; /* Get next page */
			}

			/* 
			 * check for page being pinned down, and
			 * bail out if there are more then one pages
			 * pinned for this pageno.
			 */
			if (pfdat_pinned(pfd_next)){
				if (pinned_page_count++){
					/* 
					 * Second pinned page for same
					 * pgno. Return error 
					 */
					retval = PAGE_PINNED;
					nested_pfdat_unlock(pfd_next);
					break; /* Out of while loop */
				} else {
					pfd_t	*pfd_tmp;
					/*
					 * Now that we are holding the 
					 * lock on pfd_next, take 
					 * an extra reference on that,
					 * and release pfd lock.
					 * After that take the 
					 * pfdlock for pfd_first. 
					 * We already have an extra
					 * reference on that page. 
					 */
					pfdat_hold_nolock(pfd_next);
					nested_pfdat_unlock(pfd_next);
					/* 
					 * First pinned down page for this
					 * page number. So, instead of
					 * keeping "pfd_first" for this 
					 * pageno, we keep this page, and
					 * shoot down pfd_first.
					 * So, switch roles.
					 */
					pfd_tmp   = pfd_first;
					pfd_first = pfd_next;
					pfd_next  = pfd_tmp;
					/*
					 * Now, pfd_first is the
					 * pfdat that has rawcnt > 0,
					 * and pfd_next is the page
					 * that's got to be shot down.
					 */
				}
			} else {
				/*
				 * pfd_next is the page to be shot down.
				 * Take an extra reference on this pfdat.
				 */
				pfdat_hold_nolock(pfd_next);
				nested_pfdat_unlock(pfd_next);
			}


			ASSERT(pinned_page_count <= 1);

			VNODE_PCACHE_UNLOCK(vp, locktoken);

			if (shootpage(pfd_next) == REPLOP_SUCCESS){
				/* Successful in shooting down  page.
				 * add to private list for freeing
				 * later 
				 */
#ifdef	REPL_DEBUG
				busy_pages++;
#endif	/* REPL_DEBUG */
				freepgcnt++;  
				VNODE_PCACHE_LOCK(vp, locktoken);

				/* 
				 * Take the page out of hash chain, since
				 * we should be the only user of that page.
				 * Call the pcache interface routine to
				 * take this page out of hash chain.
				 * and move it to the private list.
				 */
				vnode_premove_nolock(vp, pfd_next);
				ASSERT(pfd_replnext(pfd_next) == 0);

				/* 
				 * Add to the private list.
				 */
				if (pfdlist_head){
					pfd_replnext(pfd_next) = pfdlist_head;
				}
				pfdlist_head = pfd_next;

			} else {
				ASSERT(0);
				retval = PAGE_SHOOT_FAILED;
				/* Need to hold the lock to
				 * be freed later.
				 */
				VNODE_PCACHE_LOCK(vp, locktoken);
				break;
			}

		}

		/*
		 * pfd_first is the page that would be left on the 
		 * page cache chain. Move it from being a replicated
		 * page to a non-replicated page.
		 * Do this only if successful in shooting down pages.
		 */ 
		if (!retval && pfd_replicated(pfd_first)){
			/*
			 * We are holding pagecache lock. So, doing 
			 * nested locking is sufficient.
			 */
			nested_pfdat_lock(pfd_first);
			/* Make this page non-replicated. */
			page_replica_remove(pfd_first);
			nested_pfdat_unlock(pfd_first);
		}

		/* Drop extra reference we took on pfd_first */
		/* Invoke the actual pagefree. It's ok to grab freelist
		 * lock while holding object lock.
		 */
		pagefree(pfd_first);

		if (retval){
			/* 
			 * Some error in shooting down pages. bail from
			 * for loop.
			 */
			break;
		}

		/*
		 * If we have gathered sufficient number of pages,
		 * release them from private chain.
		 */
		if (freepgcnt > MIN_FREEPGCNT) {
			VNODE_PCACHE_UNLOCK(vp, locktoken);
			page_replica_free(pfdlist_head, freepgcnt);
			pages_shotdn += freepgcnt;
			pfdlist_head = 0;
			freepgcnt  = 0;
			VNODE_PCACHE_LOCK(vp, locktoken);
		}

	}

	ASSERT(retval || !replattr->replpgcnt);

	KTRACE_ENTER(vnode_ktrace, 	
			VNODE_PCACHE_REPLSHOOT_END, vp, pages_shotdn, 0);
	VNODE_PCACHE_UNLOCK(vp, locktoken);

	if (freepgcnt){
		pages_shotdn += freepgcnt;
		page_replica_free(pfdlist_head, freepgcnt);
	}

	REPL_PAGE_STATS.replpgshotdn += pages_shotdn;

	if (retval == 0){
		/* Extra work done only in debug mode */
		page_scanfor_replica(vp, first, pgno);
	}

#ifdef	REPL_DEBUG
	cmn_err(CE_NOTE|CE_CPUID, 
		"Shooting down page stats: total %d free_pages %d busy_pages %d pages_shotdn %d\n",
		repl_pages, free_pages, busy_pages, pages_shotdn);
#endif	/* REPL_DEBUG */

	return retval;
}
#endif	/* defined(NUMA_REPLICATION) */
