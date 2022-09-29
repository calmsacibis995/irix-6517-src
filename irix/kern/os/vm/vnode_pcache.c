/*
 * os/vm/vnode_pcache.c
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
 */


#ident	"$Revision: 1.36 $"

#include <sys/types.h>
#include <sys/buf.h>			/* chunktoss() 		*/
#include <sys/cmn_err.h>		
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/proc.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>			/* Misc stuff like kdebug 	*/
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <sys/atomic_ops.h>
#include <ksys/migr.h>
#include <ksys/rmap.h>
#include <sys/numa.h>
#include <ksys/pcache.h>
#include <ksys/vnode_pcache.h>
#include <sys/nodepda.h>
#include <sys/ktrace.h>			/* Tracing functions		*/
#include <sys/idbgentry.h>		/* idbg_addfunc 		*/

#ifdef DEBUG1
#define PRINTF(x) printf((x))
#else
#define PRINTF(x)
#endif


#ifdef	PGCACHEDEBUG
static void vnode_anypage_valid(struct vnode *);
static void vnode_oobpages(struct vnode *, pgno_t);
#endif	/* PGCACHEDEBUG */

#define	VNODE_PAGEOP_BAD	(0x1)
#define	VNODE_PAGEOP_NOASSOC	(0x2)

#define	VNODE_PAGEOP_TOSS	(void *)(__psunsigned_t)(VNODE_PAGEOP_BAD|VNODE_PAGEOP_NOASSOC)


#ifdef	DEBUG
int	vnode_broadcast_count;
#endif

/*
 * Vnode pagecache Reference counting mechanism.
 *
 * This mechanism is used to synchronize the threads trying to
 * reclaim a vnode (and hence the vnode page cache), with the  threads coming 
 * down the pagealloc path and trying to steal pages hashed to a vnode.
 *
 * We need this synchronization so that the threads coming down the pagealloc
 * path can pick the page out of free list, drop all the locks it's holding
 * and then call into vnode page cache to free the page. 
 * 
 * Without this synchronization, page cache could vanis when the thread
 * in pagealloc drops the locks. 
 * 
 * Synchronization mechanism:
 *
 *	Threads coming down the pagealloc path (and trying to pick a page out
 * 	of the freelist, check if the page is hashed and belongs to a vnode.
 *	It then sets the P_RECYCLE bit in pfdat, and also atomically bump a 
 * 	counter in vnode pcache structure
 *
 *	If a thread is in the vnode reclaim path at the same time, and
 *	tossing away the vnode pages, at the end of tossing the pages,
 *	it checks for the counter (while holding the pagecache lock)
 *	and if non-zero, sets a bit indicating there is a sleeping process,
 *	and sleeps on this synchronization variable.
 *	(Note that by the time this thread completes tossing all its pages
 *	and decides to reclaim the vnode, all threads which could bump
 *	the reference counter in pagealloc path are already done with their
 *	work, and no one would be able to find a page still hashed to this
 *	vnode in the freelist.
 *	
 *	Later the thread in the pagealloc() path, invokes vnode_page_recycle(), 
 *	which takes the page out of hash. and  decrements the counter.
 *	If counter gets to zero, it checks if there is anyone sleeping,
 *	If so, it clears the sleeping bit, and sends a broadcast 
 *	message to the signal variable on which the vnode reclaim thread would
 *	be waiting on.
 *
 *	All threads that receive the broadcast, wakeup, check if their 
 *	sleeping bit is still set, and if not, they are free to reclaim
 * 	the vnode. If their sleeping bit is still set, it's not their turn 
 *	and they go back to sleeping mode.
 *
 * 	Having just one such variable for the entire system may be a concern.
 *	We need to measure how much of a bottleneck this is, and fix it.
 *	Possibilities are to have an array of these or one in each vnode..
 *
 *	The race condition is supposed to happen only if vnodes are reclaimed,
 *	which happens only if we are running low on memory. It's a good 
 *	chance that all the hashed pages are gone by then!!.
 */

/*
 * VNODE_PCACHE_SYNC is the macro used to get to the synchronization
 * variable given a vnode.  This is used for communication between 
 * the threads trying to reclaim vndoe, and threads trying to hold vnode.
 *
 * Having a larger value for this variable would reduce the contention
 * in signalling.
 * But this contention is not observed to be very high (a few times in 
 * a stress test which ran for about 6 hours on a 36 cpu MP. So,
 * just one signal variable should be sufficient for MP systems too.
 */
#define	VNODE_PCACHE_SYNC(vp)	 (&vnode_pcache_sync)

/*
 * Actual Sync Variable.
 */
sv_t	vnode_pcache_sync;


/*
 * A preemption cookie is used to encapsulate information about the lock
 * currently held so that it can be passed around as a unit.  This is
 * used to allow lock preemption (and hence allow interrupts to occur)
 * during long pcache_remove() operations.
 */

struct preemption_cookie {
	struct vnode	*pc_vp;		/* the vnode who's lock is held */
	int		 pc_locktoken;	/* from the vnode lock call     */
};

typedef struct preemption_cookie preempt_cookie_t;

static void vnode_preempt(void *);


#ifdef	DEBUG
ktrace_t	*vnode_ktrace;
extern void	idbg_vnode_pcache_trace(__psunsigned_t);
#endif	/* DEBUG */

/*
 * init_vnode_pcache
 *	One time initialization routine called at system boot time.
 */
void
init_vnode_pcache(void)
{

	sv_init(&vnode_pcache_sync, SV_DEFAULT, "v_pcsync");

#ifdef	VNODE_PCACHE_TRACE
	vnode_ktrace = ktrace_alloc(1024, 1);
	idbg_addfunc("tracevp", idbg_vnode_pcache_trace);
#endif	/* VNODE_PCACHE_TRACE */
}

/*
 * vnode_pcache_init
 *	Routine that gets called when a vnode is allocated fresh.
 */
void
vnode_pcache_init(struct vnode *vp)
{
        /*
         * Vnode page cache related one time initialization.
         */
	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_INIT, vp, 0, 0);

	vp->v_pcacheref = 0;
	pcache_init(&vp->v_pcache);
}

/*
 * vnode_pcache_reinit
 *	Routine to reinitialize a page cache. (occurs when a vnode gets
 *	recycled. 
 */
void
vnode_pcache_reinit(struct vnode *vp)
{
	int	s;
	VNODE_PCACHE_LOCK(vp, s);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_REINIT, vp, 0, 0);

	vp->v_pcacheref = 0;
	pcache_init(&vp->v_pcache);
	VNODE_PCACHE_UNLOCK(vp, s);
}

/*
 * Gets called when an isolated vnode gets freed, and gets placed in
 * front of the vfreelist.
 */
void
vnode_pcache_free(struct vnode *vp)
{
	int	s;
	/*
	 * release any hash buckets associated with the page cache.
	 * This has the side effect of initializing the
	 * page cache data structures to a known state.
	 */

	VNODE_PCACHE_LOCK(vp, s);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_FREE, vp, 0, 0);

	pcache_release(&vp->v_pcache);
	VNODE_PCACHE_UNLOCK(vp, s);
}

/*
 * Reclaim the page cache related data structure from vnode. 
 * In addition synchronize with threads who still have an 
 * extra reference on the pagecache of this vnode.
 */
void
vnode_pcache_reclaim(struct vnode *vp)
{
	int	s;
        /*
         * Before letting the vnode be reclaimed, see if there are any
         * outstanding page cache waiters, and if so wait for them to
         * finish.
         */
        VNODE_PCACHE_LOCK(vp, s);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_RECLAIM, vp, 0, 0);

        while (vp->v_pcacheref) {

                atomicFieldAssignUint(&vp->v_pcacheflag, VNODE_PCACHE_WAITBIT,
                                        VNODE_PCACHE_WAITBIT);
                sv_bitlock_wait(VNODE_PCACHE_SYNC(vp), PINOD,
                                &vp->v_pcacheflag, VNODE_PCACHE_LOCKBIT, s);

		VNODE_PCACHE_LOCK(vp, s);
        }
        /* Cleanup the pagecache related fields */

        pcache_release(&vp->v_pcache);
        VNODE_PCACHE_UNLOCK(vp, s);
}

/*
 * vnode_page_decommission
 * 	Update the fields  of pfdat to indicate it's out of hash.
 *	'op' values passed in, indicate one the following.
 *	VNODE_PAGEOP_BAD: Mark the page as bad.
 *	VNODE_PAGEOP_NOASSOC: Requeue the page in freelist..
 *	If the op passed in indicate that the page should be marked
 *
 * 	This routine gets passed in as the function pointer to pcace_remove.
 *	which gets called either from vnode_pcache_remove() or as part
 *	of file system tossing pages. 
 *
 *	'op' passed would have the right set of bits set to indicate who 
 *	invoked this routine. 
 *
 *	It's possible to have two different set of routines. But the 
 *	work done between the two is so similar that combining them into
 *	on does not result in much overhead for the other 
 *
 * 	Two versions of this routine (one which holds the pfdat lock, and
 *	other which expects caller to hold the pfdat lock), are provided
 *	for convenience.
 *	If the calling routine is already holding the pfdat lock, there is
 *	no reason to let it go, before acquiring it back again in this
 *	routine.  
 *	In other situations, this routine gets called multiple times from
 *	pcache_remove (once for each page  found in page cache.). In this
 *	case vnode_page_decommission() which grabs the lock is called.
 */
static void
vnode_page_decommission_nolock(void *pageop, struct pfdat *pfd)
{
	int	op = (__psunsigned_t)pageop;

	ASSERT(VNODE_PCACHE_ISLOCKED(pfd->pf_vp));

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PAGEOP, pfd->pf_vp, pfd, pageop);
	ASSERT(pfd->pf_flags & P_HASH);

	if (!(pfd->pf_flags & (P_BAD))) {
		ASSERT(pfd->pf_vp->v_pgcnt > 0);
		pfd->pf_vp->v_pgcnt--;
		if (pfd_replicated(pfd)){
			page_replica_remove(pfd);
		}
	}

	pfd->pf_flags &= ~P_HASH;

	if (pfd->pf_flags & P_QUEUE){
                ADD_NODE_EMPTYMEM((pfdattocnode(pfd)), 1);
#ifdef	NOT_DEFINED
		/*
		 * Earlier we used to place this page back in queue
		 * with no association. This would have made the page
		 * avaiable earlier than otherwise. 
		 * Does it still make sense to do it?
		 * If so, we need to fix the requeue routine to
		 * take the appropriate freelist lock.
		 */
		if (op & VNODE_PAGEOP_NOASSOC) {
			nested_pfdat_unlock(pfd);
			requeue(pfd, STALE_NOASSOC);
			nested_pfdat_lock(pfd);
		}
#endif	/* NOT_DEFINED */

	} else if ((op & VNODE_PAGEOP_BAD) && !(pfd->pf_flags & P_BAD)) { 
		pfd->pf_flags |= P_BAD;
	}
}

static void
vnode_page_decommission(void *pageop, struct pfdat *pfd)
{
	/*
	 * Since we should already be holding object lock, 
	 * doing a nospl lock is sufficient.
	 */

	nested_pfdat_lock(pfd);
	vnode_page_decommission_nolock(pageop, pfd);
	nested_pfdat_unlock(pfd);
}
        
/*
 * vnode_plookup
 * 	Return the page that corresponds to the page number passed in.
 */
pfd_t *
vnode_plookup(vnode_t *vp, pgno_t pgno)
{
	int	s;
	pfd_t	*pfd;

	VNODE_PCACHE_LOCK(vp, s);
	pfd = pcache_find(&vp->v_pcache, pgno);
	VNODE_PCACHE_UNLOCK(vp, s);

	return pfd;
}

/*
 * vnode_pnext:
 *	Given the pfd, find the next page that has the same
 * 	page number as the pfdat passed in.
 *	Primiarily used to get to replicated pages.
 */
pfd_t *
vnode_pnext(vnode_t *vp, pfd_t *pfd)
{
	int	s;
	VNODE_PCACHE_LOCK(vp, s);
	pfd = pcache_next(&vp->v_pcache, pfd);
	VNODE_PCACHE_UNLOCK(vp, s);

	return pfd;
}


/*
 * vnode_pagebad
 *	Mark a specified page as bad, and remove it from page cache ..
 */
void
vnode_pagebad(vnode_t *vp, pfd_t *pfd)
{
	int	locktoken;
	off_t	offset;

	ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);
	ASSERT(pfd->pf_use > 0);

	VNODE_PCACHE_LOCK(vp, locktoken);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PAGEBAD, vp, pfd, 0);

	/* Hold pfdat_lock to prevent any changes to pf_flags/pf_pageno. */

	nested_pfdat_lock(pfd);	
	if ((pfd->pf_flags & P_BAD) == 0){
		ASSERT((pfd->pf_flags & (P_ANON|P_HASH)) == P_HASH);
		offset = ctob(pfd->pf_pageno);
		
		pcache_remove_pfdat(&vp->v_pcache, pfd);
		vnode_page_decommission_nolock(VNODE_PAGEOP_TOSS, pfd);

		nested_pfdat_unlock(pfd);
		VNODE_PCACHE_UNLOCK(vp, locktoken);

		/* Make sure no buffer maps this page. */
		chunktoss(vp, offset, offset + ctob(1) - 1);
		return;
	}
	nested_pfdat_unlock(pfd);
	VNODE_PCACHE_UNLOCK(vp, locktoken);
}

/*
 * vnode_page_attach
 *	pfd is in the page cache. This routine checks if the
 *	page is in the free list, and tries to take it out of
 *	the free list to return the page to caller.
 *
 *	This has to take care of the race condition with a
 *	different thread trying to steal this page from the
 *	free list.
 *
 * 	This routine expects the caller to be holding the page cache lock.
 */
pfd_t *
vnode_page_attach(struct vnode *vp, pfd_t *pfd)
{

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PAGEATTACH, vp, pfd, 0);

	nested_pfdat_lock(pfd);

	ASSERT(VNODE_PCACHE_ISLOCKED(vp));
	ASSERT((pfd->pf_flags & (P_BAD|P_ANON)) == 0);
	ASSERT(pfd->pf_flags & P_HASH);
	ASSERT(pfd->pf_vp == vp);

	/*
	 * Check if the page is on the free list. If so,
	 * we could be racing with pagealloc trying to
	 * free this page and returning it for some other
	 * purpose. 
	 */
	if (pfd->pf_flags & P_QUEUE) {
		nested_pfdat_unlock(pfd);
		pageunfree(pfd);
		nested_pfdat_lock(pfd);
	}

	/*
	 * At this stage, either pageunfree was able
	 * to take the page out of free list, or
	 * the page is in recycle state. 
	 * Since the vnode lock is still held, page cannot
	 * go out of the recylced state till we are done.
	 */
	
	if (pfd->pf_flags & P_RECYCLE) {

		/*
		 * XXX
		 * Instead of trying to remove the page from pagecache
		 * right here, why not let vnode_page_recycle do it ?
		 */
		/*
		 * We are racing with a different cpu
		 * doing pagealloc which has already
		 * acquired this page and wishes to
		 * recycle it for its new use.
		 * When this race occurs, we let 
		 * pagealloc win as it simplifies the
		 * locking. In this situation, unhash
		 * the page, and let the caller create
		 * a new page and insert. 
		 *
		 * This should not happen very often though..
		 * (Yes it's bad that we have to let a 
		 * page that has valid file data go. 
		 * Complexity otherwise is too large).
		 */
		
		pcache_remove_pfdat(&vp->v_pcache, pfd);
		vnode_page_decommission_nolock((void *)NULL, pfd);

		pfd->pf_vp = NULL;
		/*
		 * Don't turn off P_RECYCLE flag here. It will be
		 * turned off when vnode_page_recycle() gets called
		 * by the thread in pagealloc path.
		 * Otherwise, it would lead to a race condition, where
		 * thread in pagealloc path believes that recycling
		 * is not needed, and messes with the pfdat flag
		 * directly.
		 */
		pfd->pf_flags &= ~P_DONE;
		nested_pfdat_unlock(pfd);
		pfd = NULL;
	} else {

		/* 
		 * Since we already hold pfdat lock, bump use count
		 * directly, release pfdat lock and return.
		 */
		ASSERT((pfd->pf_flags & 
			(P_HASH|P_RECYCLE)) == P_HASH);
		pfdat_inc_usecnt(pfd);
		nested_pfdat_unlock(pfd);
	}

	return pfd;
}

/*
 * vnode_pfind:
 *	Look up the vnode's page cache for the page that corresponds 
 *	to 'pgno'.
 *	If acquire says VM_ATTACH, get an extra reference on the page.
 * returns:
 *	0	-> can't find it
 *	pfd	-> ptr to pfdat entry
 */

pfd_t *
vnode_pfind_nolock(vnode_t *vp, pgno_t pgno, int acquire)
{

	pfd_t	*pfd;

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PFIND, vp, pgno, 0);

	/* Try and get first non bad page */
	pfd = pcache_find(&vp->v_pcache, pgno);

	/*
	 * If callers needs the page to be attached, do it.
	 */
	
	if (pfd && (acquire & VM_ATTACH)) {
		ASSERT((pfd->pf_flags & P_BAD) == 0);
		ASSERT(pfd->pf_vp == vp);
		pfd = vnode_page_attach(vp, pfd);
	}

	return pfd;

}

pfd_t *
vnode_pfind(vnode_t *vp, pgno_t pgno, int acquire)
{
	int	s;
	pfd_t	*pfd;

	VNODE_PCACHE_LOCK(vp, s);
	pfd = vnode_pfind_nolock(vp, pgno, acquire);
	VNODE_PCACHE_UNLOCK(vp, s);

	return(pfd);
}

/*
 * vnode_hold
 *
 * Hold a reference to given vnode.  The caller must be holding the
 * pfdat lock for a page hashed to this vnode.  Bump the vnode
 * reference count to reflect the hold.
 */

void
vnode_hold(vnode_t *vp)
{
        ASSERT(vp);
	ASSERT(pcache_pagecount(&vp->v_pcache) > 0);

        VNODE_PCACHE_INCREF(vp);
}

/*
 * vnode_release
 *
 * Release vnode hold. If there is any thread waiting to be woken up
 * (Happens only in vnode reclaim path), do so.
 */

static void
vnode_release(vnode_t *vp)
{
        ASSERT(VNODE_PCACHE_ISLOCKED(vp));
	ASSERT(vp && vp->v_pcacheref);

	VNODE_PCACHE_DECREF(vp);

	if ((vp->v_pcacheref == 0) && VNODE_PCACHE_WAITING(vp)){
		/* 
		 * Reset the waiting bit.
		 */
		atomicFieldAssignUint(&vp->v_pcacheflag, VNODE_PCACHE_WAITBIT, 0);
		sv_broadcast(VNODE_PCACHE_SYNC(vp));

#ifdef	DEBUG
		vnode_broadcast_count++;	/* Count for debugging */
#endif	/* DEBUG */
	}
}

/*
 * vnode_pagemigr
 *
 * Check whether the page represented by old_pfd is migratable and
 * if so, copy its state to new_pfd, remove it from the pcache,
 * and insert new_pfd.
 */

int
vnode_pagemigr(vnode_t *vp, pfd_t *old_pfd, pfd_t *new_pfd)
{
	int locktoken;
        int errcode;
        
        ASSERT(vp && old_pfd && new_pfd);

        VNODE_PCACHE_LOCK(vp, locktoken);

	nested_pfdat_lock(old_pfd);

        /*
         * Has the memory object remained constant?
         */
        if (pfd_getmo(old_pfd) != vp) {
                PRINTF(("[vnode_pagemigr]: memory object mutation\n"));
                errcode = MIGRERR_MOMUTATION;
                goto migr_fail0;
        }

        /*
         * At this point we've obtained the vnode pcache lock
         * and the old_pfdat lock in the right order, with
         * the pfdat still being owned by the same memory
         * object we detected in the migration engine (this
         * function's caller).
         */

        /*
         * Check whether the page is in a migratable state
         * and transfer pfdat state
         */
	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_MIGR, vp, old_pfd, new_pfd);

        if ((errcode = MIGR_PFDAT_XFER(old_pfd, new_pfd)) != 0) {
                PRINTF(("[vnode_pagemigr]: failed pfdat_xfer\n"));
                goto migr_fail0;
        }
        
        /*
         * The old page is *not* freed; it's just removed from the cache
         * The pf_pageno field in this pfdat must be intact.
         */
	pcache_remove_pfdat(&vp->v_pcache, old_pfd);

        /*
         * We're completely done with the old pfdat.
         * Release its pfdat lock
         */
        nested_pfdat_unlock(old_pfd);

        /*
         * Now we insert the new pfdat into the pcache
         */
        pcache_insert(&vp->v_pcache, new_pfd);

        errcode = 0;
        goto migr_success;

        /*
         * Exit levels.
         */

  migr_fail0:
        nested_pfdat_unlock(old_pfd);

  migr_success:

        /*
	 * Drop the extra reference the caller acquired on the vnode.
	 * This may cause the vnode vp points at to be freed, so we can't
	 * reference vp anymore after this call.
	 */

	vnode_release(vp);

	VNODE_PCACHE_UNLOCK(vp, locktoken);

        return (errcode);
}


/*
 * Conditionally insert pfd in hash list.  If another page already
 * exists that maps [tag,pgno], toss the passed page and return the
 * found match.
 */
pfd_t *
vnode_pinsert_try(vnode_t *vp, pfd_t *pfd, pgno_t pgno)
{
	register int		locktoken;
	register struct pfdat	*pfd_hash; 
	void			*pcache_token;

	ASSERT((pfd->pf_flags & (P_HASH|P_SWAP|P_ANON|P_QUEUE|P_SQUEUE)) == 0);

	pcache_token = pcache_getspace(&vp->v_pcache, 1);

	if (pcache_token){
		/*
		 * If we successfully allocated the space,
		 * we may as well go and grow the pagecache.
		 * It takes quite an effort to allocate space!!.
		 * If it's already grown to proper size, pcache_resize
		 * frees allocated space.
		 */
		VNODE_PCACHE_LOCK(vp, locktoken);
		pcache_token = pcache_resize(&vp->v_pcache, pcache_token);
		VNODE_PCACHE_UNLOCK(vp, locktoken);
		pcache_resize_epilogue(pcache_token);
	}

	VNODE_PCACHE_LOCK(vp, locktoken);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PINS_TRY, vp, pfd, pgno);

	pfd_hash = pcache_find(&vp->v_pcache, pgno);
	if (pfd_hash) {
		ASSERT((pfd_hash->pf_flags & P_BAD) == 0);

		/*
		 * A page with "pgno" already exists in this cache.
		 * If we can successfully grab it, return it to 
		 * user. vnode_page_attach tries to resolve the
		 * race with pagealloc().
		 */

		pfd_hash = vnode_page_attach(vp, pfd_hash);
		if (pfd_hash) {

			VNODE_PCACHE_UNLOCK(vp, locktoken);
			/*
			 * Free the page allocated earlier.
			 */
			pagefree(pfd);
			return pfd_hash;
		}
	}

	/*
	 * Could not find any page in the page cache. Time to 
	 * insert the page passed in.
	 */

	/*
	 * These assignments are safe since page is not in use yet.
	 */
	pfd->pf_vp     = vp;
	pfd->pf_pageno = pgno;
	pfd->pf_flags |= P_HASH;

	pcache_insert(&vp->v_pcache, pfd);

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		if (pfd_to_vcolor(pfd) < 0)
			pfd_set_vcolor(pfd,vcache_color(pfd->pf_pageno));
	}
#endif /* _VCE_AVOIDANCE */
	ASSERT(vp->v_pgcnt >= 0);
	vp->v_pgcnt++;

	VNODE_PCACHE_UNLOCK(vp, locktoken);
	return pfd;
}

/*
 * Replace a page found in the page cache with a different page. This
 * interface is for cells & it used to replace a page allocated from
 * the local cell with a different page imported from a server. The
 * page being replaced is NOT in the P_DONE state.
 */
#ifdef CELL
pfd_t *
vnode_page_replace(pfd_t *opfd, pfd_t *npfd)
{
	register int		locktoken;
	vnode_t			*vp;

	ASSERT((opfd->pf_flags & (P_DONE|P_SWAP|P_ANON|P_QUEUE|P_SQUEUE)) == 0);
	ASSERT(opfd->pf_flags & (P_HASH|P_BAD));

	vp = opfd->pf_vp;
	ASSERT(vp);

	VNODE_PCACHE_LOCK(vp, locktoken);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_REPLACE, vp, opfd, npfd);

	/*
	 * These assignments are safe since the new page is not in use yet.
	 */
	nested_pfdat_lock(opfd);
	npfd->pf_vp     = opfd->pf_vp;
	npfd->pf_pageno = opfd->pf_pageno;
	npfd->pf_flags |= opfd->pf_flags&(P_HASH|P_BAD);
	if (npfd->pf_flags&P_BAD)
		npfd->pf_flags &= ~P_HASH;
	nested_pfdat_unlock(opfd);


	/*
	 * If the old page has not been marked P_BAD, remove it
	 * remove the page cache.
	 */
	if (!(opfd->pf_flags & P_BAD)) {
		pcache_remove_pfdat(&vp->v_pcache, opfd);
		vnode_page_decommission(VNODE_PAGEOP_TOSS, opfd);
	}

	if (npfd->pf_flags&P_HASH) {
		pcache_insert(&vp->v_pcache, npfd);
		ASSERT(vp->v_pgcnt >= 0);
		vp->v_pgcnt++;
	}

	VNODE_PCACHE_UNLOCK(vp, locktoken);

	pagedone(opfd);
	pagefree(opfd);

	return npfd;
}
#endif

/*
 * Insert page on hash chain.
 *	vp	-> pointer to incore vnode
 *	pgno	-> logical file page number
 *	Expects the caller to hold the the pager object lock
 */
void
vnode_pinsert(vnode_t *vp, pfd_t *pfd, pgno_t pgno, unsigned flag)
{
	void		*pcache_token;
	int		locktoken;

	ASSERT((flag & (P_ANON|P_SWAP)) == 0);

	pcache_token = pcache_getspace(&vp->v_pcache, 1);

	if (pcache_token) {
		VNODE_PCACHE_LOCK(vp, locktoken);
		pcache_token = pcache_resize(&vp->v_pcache, pcache_token);
		VNODE_PCACHE_UNLOCK(vp, locktoken);
		pcache_resize_epilogue(pcache_token);
	}

	VNODE_PCACHE_LOCK(vp, locktoken);
	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PINSERT, vp, pfd, pgno);

	/*
	 * These are safe since page is not in use yet.
	 */
	pfd->pf_vp     = vp;
	pfd->pf_pageno = pgno;

	ASSERT((pfd->pf_flags & (P_HASH|P_SWAP|P_ANON|P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);

	pfd->pf_flags |= (P_HASH | flag);

#ifdef DEBUG
	/* 
	 * check if the page already exists.
	 */

	ASSERT(!pcache_find(&vp->v_pcache, pgno));
#endif	/* DEBUG */
	
	pcache_insert(&vp->v_pcache, pfd);

	ASSERT(vp->v_pgcnt >= 0);
	vp->v_pgcnt++;

#ifdef _VCE_AVOIDANCE
	ASSERT((pfd->pf_flags & P_LPG_INDEX) == 0); /* XXX */
	if (vce_avoidance) {
		if (pfd_to_vcolor(pfd) < 0)
			pfd_set_vcolor(pfd,vcache_color(pfd->pf_pageno));
	}
#endif /* _VCE_AVOIDANCE */

	VNODE_PCACHE_UNLOCK(vp, locktoken);
}

/*
 * Exported pinsert interface with no locking.
 * 	Caller must have the object lock.
 * 	As it stands now, caller does not take the object lock. 
 *	So, just call vnode_pinsert which does the right thing.
 */

/*ARGSUSED*/
void
vnode_pinsert_nolock(
	vnode_t 	*vp, 
	pfd_t 		*pfd, 
	pgno_t 		pgno, 
	unsigned 	flag, 
	int 		locktoken)
{
        vnode_pinsert(vp, pfd, pgno, flag);
}
        

/*
 * remove page from hash chain
 *	pfd	-> page frame pointer
 *	Assumes the caller holds the pagecache lock.
 *	This routine holds the pfdat lock.
 *	Currently in use by the replication code trying to shootdown
 * 	pages.
 */

void
vnode_premove_nolock(vnode_t *vp, pfd_t *pfd)
{
	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PREMOVE, vp, pfd, 0);

	pcache_remove_pfdat(&vp->v_pcache, pfd);
	vnode_page_decommission((void *)NULL, pfd);

}

void
vnode_premove(vnode_t *vp, pfd_t *pfd)
{
	int	locktoken;

	ASSERT((pfd->pf_flags & (P_ANON|P_SWAP)) == 0);
	ASSERT(pfd->pf_flags & P_HASH);

	VNODE_PCACHE_LOCK(vp, locktoken);
	vnode_premove_nolock(vp, pfd);
	VNODE_PCACHE_UNLOCK(vp, locktoken);
}


/*
 * Renounce vnode pages [first, last].
 * 	Mark pages in the range <first, last> as bad.
 *	Invoke the chunktoss, which marks the data in this range as
 *	stale. chunktoss() invokes chunkrelease() which is responsible
 *	for freeing the pages in the range <first, last>
 *	NOTE: chunkrelease() invokes vnode_pagesrelease to free the pages!!.
 */


void
vnode_tosspages(vnode_t *vp, off_t first, off_t last)
{
	pfd_t	*pfd;
	int	i, locktoken;
	pgno_t 	pfirst, plast;
	preempt_cookie_t preempt_cookie;

	ASSERT(vp->v_pgcnt >= 0);

	if (first > last)
		return;

	VNODE_PCACHE_LOCK(vp, locktoken);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PTOSS, vp, first, last);

	if (vp->v_pgcnt) {

		pfirst = offtoc(first);
		plast = offtoct(last);

		preempt_cookie.pc_vp        = vp;
		preempt_cookie.pc_locktoken = locktoken;

		pcache_remove(&vp->v_pcache, 
				pfirst, (plast - pfirst + 1), 
				vnode_page_decommission,
				VNODE_PAGEOP_TOSS,
				vnode_preempt, (void *) &preempt_cookie);
	} else {
#ifdef PGCACHEDEBUG
		vnode_anypage_valid(vp);
#endif
	}
	VNODE_PCACHE_UNLOCK(vp, locktoken);

	/*
	 * This is a hack, but EFS and xFS protect against accessing
	 * beyond eof, so save time and only zero page-end for
	 * non-local (NFS) filesystems.
	 */
	if ((i = poff(first)) && !(vp->v_vfsp->vfs_flag & VFS_LOCAL)) {
		pfd = vnode_pfind(vp, offtoct(first), VM_ATTACH);
		if (pfd) {
			page_zero(pfd, 0, i, NBPP-i);
			pagefree(pfd);
		}
	}

	/*
	 * we have marked all pages as BAD and unhashed them.
	 * Now call chunktoss which will remove them (w/o writing)
	 * from the delwri queue if they are there, as well as STALE
	 * out any buffers containing pointers to these pages
	 */
	chunktoss(vp, first, last);
#ifdef PGCACHEDEBUG
	vnode_oobpages(vp, offtoc(first));
#endif

}

#if 0
/*
 * flush and unhash all pages associated with a filesystem
 *	vfsp	-> mounted filesystem
 * returns:
 *	none
 * This routine is not needed anymore. It used to be called from
 * efs while unmounting the root. All that was done here was to toss
 * all pages that belongs to the vnode within the filesystem. 
 * It makes no sens to do that while shutting down the system!!. 
 *
 * Prototype code is here just to show what we need to do if it's 
 * required to implement this routine.
 */
/*ARGSUSED*/
void
vfs_flushinval_pages(struct vfs *vfsp)
{

	/*	Flush out delayed-write pages first.
	 *
	 * binval is always called before pflushinvalvfsp, so any
	 * delwri buffers have already been pushed.
	vfsppush(vfsp, B_STALE);
	*/

	ASSERT(0);
	THIS CODE IS STILL TO BE FIXED. It would look like
	for (vp = first_vp_in_vfs; 
		vp != last_vp_in_vfs; 
			vp = get_next_vp(vfs, vp))
		vnode_flushinval_pages(vp, first, last); 

}
#endif	/* 0 */

/*
 * Flush delayed writes for vp,
 * then invalidate remaining used pages and unhash cached ones.
 *
 * returns: nothing
 */
void
vnode_flushinval_pages(struct vnode *vp, off_t start, off_t end)
{
	register pgno_t pfirst, plast;
	register int	locktoken;
	preempt_cookie_t preempt_cookie;

	ASSERT(vp->v_pgcnt >= 0);
	if (start > end)
		return;

	/*
	 * Always call chunkpush - its possible that we have dirty bad pages
	 * on vp - this can occur on file truncation of a mmaped file.
	 * These pages will have been unhashed so won't be represented by
	 * pgcnt.
	 *
	 * We don't need to map start/end out to page boundaries because
	 * pfirst/plast are mapped inward to page boundaries.
	 */
	(void) chunkpush(vp, start, end, B_STALE);

	if (vp->v_pgcnt == 0)
		return;


	pfirst = offtoc(start);
	plast  = offtoct(end);


	/*
	 * We have some valid pages to be flush-invalidated. 
	 * Invoke pcache_remove on the range. 
	 */
	VNODE_PCACHE_LOCK(vp, locktoken);

	KTRACE_ENTER(vnode_ktrace, 
			VNODE_PCACHE_FLUSHINVAL, vp, pfirst, plast);

	preempt_cookie.pc_vp        = vp;
	preempt_cookie.pc_locktoken = locktoken;

	pcache_remove(&vp->v_pcache, 
		pfirst, 
		(plast - pfirst + 1), 
		vnode_page_decommission, 
		VNODE_PAGEOP_TOSS,
		vnode_preempt, (void *) &preempt_cookie);
	VNODE_PCACHE_UNLOCK(vp, locktoken);


#ifdef PGCACHEDEBUG
	if (vp->v_pgcnt == 0)
		vnode_anypage_valid(vp);
#endif
}

/*
 * flush and unhash all unused, cached pages associated with an vnode
 *	vp	-> vnode
 * returns: nothing
 */
void
vnode_invalfree_pages(vnode_t *vp, off_t filesize)
{
	int	locktoken;
	pgno_t	pgno;

	if (filesize == 0)
		return;

	chunkinvalfree(vp);

	ASSERT(vp->v_pgcnt >= 0);

	VNODE_PCACHE_LOCK(vp, locktoken);

	for (pgno = 0; pgno <= offtoct(filesize); pgno++) {
		pfd_t	*pfd;
		/* Got to make a single interface for following operations */
		if (pfd = pcache_find(&vp->v_pcache, pgno)) {

			/*
			 * Hold pfdat lock across the pagecache operation.
			 * This avoids the race condition between CPU A
			 * looking at pf_use field, and deciding to remove
			 * this page, and CPU B bumping the reference 
			 * count on this. (But.. remapf() should have 
			 * removed this race condition.. If that's the
			 * case, take this lock out..
			 */
			ASSERT(pfd->pf_vp == vp);
			nested_pfdat_lock(pfd);
			if (!(pfd->pf_flags & P_BAD) && (pfd->pf_use == 0)) {
				KTRACE_ENTER(vnode_ktrace, 
						VNODE_PCACHE_INVALFREE, 
						vp, pfd, pgno);

				pcache_remove_pfdat(&vp->v_pcache, pfd);
				vnode_page_decommission_nolock(
						VNODE_PAGEOP_TOSS, pfd);
			}
			nested_pfdat_unlock(pfd);
		}
	}
	VNODE_PCACHE_UNLOCK(vp, locktoken);

}

/*
 * Flush delayed writes for vp.
 *
 * returns: 0 or an errno
 *	Called from filesystem to write back all the pages that
 *	could be dirty. This interface was established when there
 *	was not unified page cache. Since we have one now, all 
 *	that's needed will be done in chunkpush()
 */
int
vnode_flush_pages(struct vnode *vp, off_t first, off_t last, uint64_t bflags)
{
	if (last == 0)
		return 0;
	
	return chunkpush(vp, first, last, bflags);
}

/*
 * vnode_pagesrelease
 *	Release all the pages associated with a vnode. 
 *	This gets invoked via chunkrelse routine and is responsible
 *	for freeing all the pages linked via the "special" field
 *	"pf_pchain" in pfdat.  The caller holds the buffer lock
 *	protecting the chain itself.
 */
void
vnode_pagesrelease(vnode_t *vp, pfd_t *pfd, int count, uint64_t flags)
{
	pfd_t	*tpfd;
	int	locktoken;

	for ( ; --count >= 0; pfd = tpfd) {
		ASSERT(pfd);

		KTRACE_ENTER(vnode_ktrace, 
				VNODE_PCACHE_PAGESRELEASE, vp, pfd, flags);

		/*
		 * The pf_pchain field is protected by the buffer lock
		 * held by our caller.
		 */
		tpfd = pfd->pf_pchain;

		ASSERT(pfd->pf_vp == vp);
		ASSERT(!(pfd->pf_flags & (P_ANON|P_SWAP)));

		if ((flags & B_ERROR) && !(pfd->pf_flags & P_BAD)) {

			/*
			 * We acquire the pcache lock for each page so
			 * that we don't hold it for too long if we're
			 * releasing a long list of pages.
			 */
			VNODE_PCACHE_LOCK(vp, locktoken);
			/*
			 * The P_BAD flag is checked inside the PCACHE lock as
			 * it is set holding this lock.
			 */
			if (!(pfd->pf_flags & P_BAD)) {
				pcache_remove_pfdat(&vp->v_pcache, pfd);
				vnode_page_decommission(VNODE_PAGEOP_TOSS, pfd);
			}
			VNODE_PCACHE_UNLOCK(vp, locktoken);
		}
		/*
		 * mark page as done, and wakeup anyone sleeping on it.
		 */
		pagedone(pfd);

		/*
		 * free page back to its pool.
		 * This may be a lengthy operation (measured at over 50
		 * microseconds or so on 100 Mhz SN0), which is only a
		 * problem if we're in a loop releasing lots of pages.
		 */
		pagefree(pfd);
	}
}

/*
 *  Set the P_HOLE bit in the pages of the given list.
 */
/*ARGSUSED*/
void
vnode_pages_sethole(vnode_t *vp, pfd_t *pfd, int count)
{

	int	locktoken;
	pfd_t	*pfd1;

	ASSERT(pfd != NULL);
	ASSERT(vp);

	VNODE_PCACHE_LOCK(vp, locktoken);

	while (count > 0) {
		ASSERT(pfd != NULL);
		ASSERT(pfd->pf_vp == vp);
		nested_pfdat_lock(pfd);
		pfd_nolock_setflags(pfd, P_HOLE);
		pfd1 = pfd->pf_pchain;
		count--;
		nested_pfdat_unlock(pfd);
		pfd = pfd1;
	}

	VNODE_PCACHE_UNLOCK(vp, locktoken);
}


/*
 * vnode_page_recycle
 *	Recycle the page belonging to vnode for a different purpose.
 *	Gets invoked from the pagealloc() path if the page to be
 *	allocated still has P_HASH bit set, and belongs to a vnode.
 *	
 *	This routine is responsible for taking the page out of the
 *	page cache and returning it for use by the thread in pagealloc()
 *	
 *	If the page has P_HASH bit still set, then this routine removes the
 *	page from vnode page cache, turns off the appropriate flags, and
 *	returns it.
 *	If P_HASH is not set, some one else raced with us, and already
 *	removed the page from the pagecache. In this case, there is no
 *	work to be done.
 *
 *	In addition, this routine has to synchronize with the threads trying
 *	to reclaim the vnode. 
 *	
 *	Refer to the detailed description of how the threads are synchronized
 *	at the top of file.
 */
void
vnode_page_recycle(struct vnode *vp, struct pfdat *pfd)
{
	int	locktoken;

	ASSERT(vp && vp->v_pcacheref);
	VNODE_PCACHE_LOCK(vp, locktoken);
	ASSERT(vp && vp->v_pcacheref);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_PAGERECYCLE, 
						vp, pfd, vp->v_pcacheflag);

	nested_pfdat_lock(pfd);
	ASSERT((pfd->pf_flags & (P_ANON|P_SQUEUE)) == 0);
	ASSERT(pfd->pf_use == 0);

	/*
	 * Page in recycled state could have already been removed from
	 * the page cache by the thread that got this page while in 
	 * pagealloc. So try to remove this page only if it's still 
	 * on the page cache.
	 */
	
	if (pfd->pf_flags & P_HASH) {
		ASSERT(pfd->pf_vp == vp);
		ASSERT((pfd->pf_flags & (P_HASH|P_RECYCLE)) ==
					(P_HASH|P_RECYCLE));

		/*
		 * Turn off the hash and recycle flags, and let the
		 * page cache remove this page.
		 * Since we are holding the object lock, it's not
		 * a problem to turn off flags here.
		 */
		pcache_remove_pfdat(&vp->v_pcache, pfd);
		vnode_page_decommission_nolock((void *)NULL, pfd);

		pfd->pf_vp = NULL;
		pfd->pf_flags &= ~P_RECYCLE;
	} else {
		/*
		 * Page has already been removed from page cache. 
		 * This could happen either due to a second CPU
		 * trying to acquire this page, would look at the 
		 * P_RECYCLE bit, and do the job for us.
		 * Second possibility is if a flushinval_ operation
		 * came in and decided to mark page as bad. 
		 * So, just turn off all relavent bits.
		 */
		pfd->pf_flags &= ~(P_BAD|P_RECYCLE);
		pfd->pf_vp = NULL;
	}
	nested_pfdat_unlock(pfd);

	/*
	 * Decrement the page cache reference on the vnode.
	 * and if there is any thread waiting to be woken up
	 * (Happens only in vnode reclaim path), do so.
	 */
        vnode_release(vp);
	VNODE_PCACHE_UNLOCK(vp, locktoken);

	return;
}


/*
 * Momentarily release the vnode pcache lock to allow any pending interrupts
 * to occur.
 */

static void
vnode_preempt(void *cookie)
{
	preempt_cookie_t	*preempt_cookie;

	preempt_cookie = (preempt_cookie_t *) cookie;

	VNODE_PCACHE_UNLOCK(preempt_cookie->pc_vp, preempt_cookie->pc_locktoken);
	delay_for_intr();
	VNODE_PCACHE_LOCK(preempt_cookie->pc_vp, preempt_cookie->pc_locktoken);
}


#ifdef	VNODE_PCACHE_TRACE

static char *vnode_pcache_ops[] = {
	"none       ", 	"init       ",	"reinit     ", 	"pgfree     ",
	"reclaim    ", 	"pageop     ",	"pagebad    ",	"pgattach   ",
	"pfind      ",	"pinstry    ",	"pinsert    ",	"pgremove   ",
	"ptoss      ",	"pflushinval",	"pinvalfree ",	"pgsrelease ",
	"pagerecycle",	"replinsert ",	"replfound  ",	"replinsertd",
	"pg_findnode",	"pg_foundnod",	"replshoot  ",  "replsh_pg  ",
	"replsh_end ",	"pagefree   ",	"pagefreanon",	"migr	    ",
	"epilogue   ",  "pg_ismigr  ",  "preplace"
};


void
idbg_vnode_pcache_trace(__psunsigned_t val)
{
	struct vnode *vp = (struct vnode *)val;
	ktrace_entry_t	*kt;
	ktrace_snap_t	ktsnap;

	if (vnode_ktrace == NULL)
		return;

	qprintf("Vnode pagecache operations for vnode 0x%x\n", vp);
	kt = ktrace_first(vnode_ktrace, &ktsnap);
	while (kt != NULL) {
		if (val == -1)
			qprintf("vp: 0x%x", kt->val[1]);
		if ((val == -1) || ((vnode_t *)(kt->val[1]) == vp)) {
			qprintf("  %s @%d RA:0x%x 0x%x 0x%x \n", 
					vnode_pcache_ops[(long)kt->val[0]], 
					kt->val[4],
					kt->val[5],
					kt->val[2], kt->val[3]);
		}
		kt = ktrace_next(vnode_ktrace, &ktsnap);
	}
}

#endif	/* VNODE_PCACHE_TRACE */

#if defined(PGCACHEDEBUG)

#if defined(NUMA_BASE)
static void
vnode_anypage_valid(vnode_t *vp)
{
	int	node, slot, slot_psize, base_pfn, pfn;
	struct 	pfdat	*pfd;


	for (node=0; node < numnodes; node++){
	    for (slot = 0; slot < node_getnumslots(node); slot++){
		slot_psize = slot_getsize(node,slot);
		base_pfn = slot_getbasepfn(node,slot);
		for (pfn = base_pfn; pfn < base_pfn + slot_psize; ++pfn) {
		    pfd = pfntopfdat(pfn);
		    /*
		     * We are looking without holding locks. 
		     * So, be aware of the potential race
		     * conditions.
		     */

		    if ((pfd->pf_vp == vp) && 
			!(pfd->pf_flags & P_BAD) &&
			 (pfd->pf_flags & P_HASH)) {

			printf("pfd 0x%x still hashed on vp 0x%x\n", 
						pfd, vp);
			debug("");
		    } 
		}
	    } /* for slots */
	} /* for node */

}
#else /* !(NUMA_BASE) */
static void
vnode_anypage_valid(vnode_t *vp)
{
        int     node;

        for (node=0; node < numnodes; node++){
                pfd_t   *pfd;

                for (pfd = PFD_LOW(node); pfd <= PFD_HIGH(node); pfd++){
                        if ((pfd->pf_vp  == vp) &&
			    !(pfd->pf_flags & P_BAD) &&
			    (pfd->pf_flags  & P_HASH)) {
			    	printf("pfd 0x%x still hashed on vp 0x%x\n",
						pfd, vp);
				debug("");
			}
		}
        }
}
#endif	/* NUMA_BASE */

static void
vnode_oobpages(vnode_t *vp, pgno_t pgno)
{
	int		locktoken;
	struct pfdat	*pfd;

	VNODE_PCACHE_LOCK(vp, locktoken);
	while (pfd = pcache_find(&vp->v_pcache, pgno++)){
		if ((pfd->pf_flags & P_BAD) == 0){
			cmn_err(CE_WARN, "residual cached pfd 0x%x for vp 0x%x",
				pfd, vp);
			debug(0);
		}
	}
	VNODE_PCACHE_UNLOCK(vp, locktoken);
}

#endif /* PGCACHEDEBUG */
