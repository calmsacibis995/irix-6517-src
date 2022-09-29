/*
 * vm/cell/customs.c
 *
 *
 * Copyright 1997, Silicon Graphics, Inc.
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

#ident	"$Revision: 1.13 $"


#include "sys/sysmacros.h"
#include "sys/pfdat.h"
#include "sys/page.h"
#include "sys/kmem.h"
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/kopt.h"
#include "sys/systm.h"
#include "sys/vnode.h"
#include "sys/idbgentry.h"
#include "ksys/sthread.h"
#include "ksys/cell.h"
#include "ksys/cell/mesg.h"
#include "ksys/cell/subsysid.h"
#include "ksys/cell/service.h"
#include "ksys/partition.h"
#include "customs.h"
#include "invk_exim_stubs.h"
#include "I_exim_stubs.h"

/*
 * Statistics collected by the export/import routines. Stats are collected
 * if DEBUG or if EXIM_STATS is defined. Statistics for a cell may be printed
 * with the "exim" idbg command.
 */
 
#define EXIM_STATS		/* ZZZ - temp for debugging */

#if defined(EXIM_STATS) || defined(DEBUG)

struct {
	uint	export, unexport;
	uint	export_duplicate, export_imported_page;
	uint	import, import_again, import_again_recycling, import_again_not_found;
	uint	import_again_cached, import_again_duplicate;
	uint	proxy_pagefree, proxy_pagefree_zero, proxy_pageunfree;
	uint	proxy_decommission_free_page;
	uint	import_recycling;
	uint	I_unexport_pages;
	uint	unimport_page_demon, unimport_cell_pages;
	uint	unimport_cell_pages_lock_conflict;
	uint	unimport_cell_pages_count, unimport_aged_out;
	uint	pfntopfdat, pfntopfdat_proxy;
} exim_stats;

#define STAT(s)		atomicAddUint(&exim_stats.s, 1)
#define STATN(s,n)	atomicAddUint(&exim_stats.s, n)

static void idbg_exim_print_stats(__psint_t);

#else
#define STAT(s)
#define STATN(s,n)
#endif


#ifdef DEBUG
static void idbg_exim_proxy_list(__psint_t);
#endif



/*
 * This specifies the number of pages to import per RPC request to
 * the server.
 */
#define PFN_LIST_SIZE			64


/*
 * To reduce contention on the proxy_list_lock & keep the list sizes
 * smaller, there are multiple list. Each is protected by its own lock.
 * The number of lists must be a power of 2!!
 *	ZZZ - should be function of memory size
 */
#define NUM_PROXY_LISTS			256

#define LISTHEAD(pfn)			(&proxy_list_head[pfn&(NUM_PROXY_LISTS-1)])


/*
 * Physical Page Import/Export Control
 *
 * This contains routines needed to control the sharing of physical 
 * memory between cells in a DSM environment.
 *
 * There are two main concepts implemented: page exporting and page 
 * importing.  Exporting a page means that a physical page that is
 * local to this cell is made available (exported) to another cell.
 * Importing a page is the act of taking a page exported by another
 * cell and setting up the necessary data structures (namely a proxy
 * pfdat) so that the page can be used by the importing cell.
 *
 * The export operation includes opening the firewall for the specified 
 * page and recording information needed in order to perform recovery
 * should the cell to which the page is exported go down.  The importing
 * cell also records information to enable recovery should the exporting
 * cell go down.
 *
 * For this initial implementation, some very simple mechanisms are used.
 * Since firewalls are not yet supported, exporting is pretty simple
 * for now.  For importing, a proxy pfdat is allocated and placed on
 * a single linked list of all proxies in use by this cell.
 */


extern time_t 	lbolt;

#define	PROXY_LIST_LOCK(head)		mutex_spinlock(&head->pr_lock)
#define PROXY_LIST_UNLOCK(head, t)	mutex_spinunlock(&head->pr_lock, (t))
#define	NESTED_PROXY_LIST_LOCK(head)	nested_spinlock(&head->pr_lock)
#define NESTED_PROXY_LIST_UNLOCK(head)	nested_spinunlock(&head->pr_lock)

#define	PROXY_FREE_LOCK(cell)		mutex_spinlock(&proxy_freelist[cell].px_lock)
#define PROXY_FREE_UNLOCK(cell, t)	mutex_spinunlock(&proxy_freelist[cell].px_lock, (t))
#define	NESTED_PROXY_FREE_LOCK(cell)	nested_spinlock(&proxy_freelist[cell].px_lock)
#define NESTED_PROXY_FREE_UNLOCK(cell)	nested_spinunlock(&proxy_freelist[cell].px_lock)



/*
 * The following constants are used for aging the proxy free list &
 * returning pages to the server.
 */
int unimport_interval_ticks = HZ*UNIMPORT_INTERVAL;
int unimport_age_ticks = HZ*UNIMPORT_AGE_SEC;	


static zone_t	*proxy_zone;

/*
 * Imported pages are kept in linked listed hashed by the
 * pfn. Each hash chain is protected by its own lock.
 */
typedef struct {
	lock_t		 pr_lock;
	proxy_pfd_t	*pr_next;
} proxy_list_t;
	
proxy_list_t	proxy_list_head[NUM_PROXY_LISTS];


/*
 * Freelist of imported pages that have a pf_use count == 0. A list
 * is kept for each server cell. If pages remain unused for too long,
 * the page is returned to the server.
 * Note that array is indexed by the cell that the page was imported from. 
 * This corresponds to the cell that has the server-side vnode for the
 * file. The page may actually belong to a different cell if the file server 
 * imported the page from a different cell.
 */
typedef struct {
	lock_t		px_lock;	/* lock to protect the list */
	plist_t		px_freelist;	/* hold head/tail for freelist */
} proxy_freelist_t;

proxy_freelist_t	*proxy_freelist;	/* freelist per cell of pages to be unimported */

void unimport_cell_pages(proxy_freelist_t *, cell_t);


/*
 * customs_init
 *
 * Boot time initialization function.  Initialize locks and proxy zone.
 * Set up freelist.
 */

void
customs_init()
{
	cell_t		cell;
	int		hash;

	for (hash=0; hash<NUM_PROXY_LISTS; hash++)
		init_spinlock(&proxy_list_head[hash].pr_lock, "proxylist", hash);
	proxy_zone = kmem_zone_init(sizeof(proxy_pfd_t), "Proxy Structs");
	proxy_freelist = kmem_alloc(sizeof(proxy_freelist_t) * (MAX_CELL_NUMBER+1), KM_NOSLEEP);
	for(cell=0; cell <= MAX_CELL_NUMBER; cell++) {
		init_spinlock(&proxy_freelist[cell].px_lock, "proxyfree", cell);
		makeempty(&proxy_freelist[cell].px_freelist);
	}
	mesg_handler_register(exim_msg_dispatcher, EXIM_SUBSYSID);


#if defined(EXIM_STATS) || defined(DEBUG)
	idbg_addfunc("exim", idbg_exim_print_stats);
#endif

#if defined(DEBUG)
	idbg_addfunc("exim_proxy", idbg_exim_proxy_list);
#endif

}


/*
 * export_page
 *
 * Export a page to a remote cell.  Open the firewall for the page.  The
 * fact that the firewall has been opened is our indication that a given
 * page has been exported to a specific cell.  This is used during clean-up
 * when a remote cell goes down.  So we don't need to maintain a separate
 * data structure to record exports.
 */


/*ARGSUSED*/
pfn_t
export_page(pfd_t *pfd, cell_t cell, int *already_exported)
{
	int			s;

	STAT(export);
	if (pfd->pf_flags&P_PROXY)
		STAT(export_imported_page);

	/*
	 * Open firewall for this page. Note that we dont maintain
	 * any export state (except in the hardware firewall) that indicates
	 * if the firewall is already open. If the firewall was already dropped,
	 * dont increment any holds on the pfd. There will be no
	 * subsequent unexport_page if the page was already exported.
	 */

	s = pfdat_lock(pfd);

	*already_exported = part_export_page(pfdattophys(pfd), NBPP,
			CELLID_TO_PARTID(cell));

	if (*already_exported == 0) {
		pfd->pf_flags |= P_XP;
		pfdat_inc_usecnt(pfd);
	} else {
		STAT(export_duplicate);
	}
	pfdat_unlock(pfd, s);

	/*
	 * Page is now exported.  Return its pfn.
	 */

	return (pfdattopfn(pfd));
}



/*
 * unexport_page
 *
 * Undo an export operation on the given page for the given cell.
 */

/*ARGSUSED*/
void
unexport_page(pfd_t *pfd, cell_t cell)
{
	int			s, still_exported;

	STAT(unexport);

	/*
	 * Put firewall back up for this page.
	 */
	s = pfdat_lock(pfd);
	ASSERT(pfd->pf_flags&P_XP);
	still_exported = part_unexport_page(pfdattophys(pfd), NBPP,
			CELLID_TO_PARTID(cell));

	/*
	 * If the page is no longer exported to ANY cell, clear the
	 * P_XP bit.
	 */
	if ( !still_exported)
		pfd->pf_flags &= ~P_XP; 
	pfdat_unlock(pfd, s);


	/*
	 * Drop the extra reference we took in export_page().
	 *
	 * XXX Have to decide how pagefreeanon and pagefreesanon fit
	 * XXX in here.  The thought is that maybe this detail is
	 * XXX handled by the memory object manager.
	 */

	pagefree(pfd);
}



/*
 * import_page
 *
 * Make an external physical page available for use by this cell.  This
 * means allocating a proxy pfdat and storing it so that future pfntopfdat
 * calls work correctly.
 */

pfd_t *
import_page(pfn_t pfn, cell_t export_cell, int already_imported)
{

	proxy_pfd_t	*new_proxy=NULL, *proxy;
	proxy_list_t	*listhead;
	int		lock_token;
	cell_t		old_cell;

	listhead = LISTHEAD(pfn);

	if (already_imported) {
		int	reuse_proxy;

		STAT(import_again);
		lock_token = PROXY_LIST_LOCK(listhead);

		/*
	 	 * See if we already know about this page. 
		 *
		 * If the page is in the queue of pages to be unimported,
		 * remove it from the queue & make it look like we just
		 * imported the page. Return a pointer to the pfd.
		 * 
		 * If we already know about the page & it is NOT in the freelist, 
		 * then we hit a race condition where 2 processes tried to import
		 * the same page at the same time. Do nothing & return a NULL pfd.
		 */

		for (proxy = listhead->pr_next; proxy; proxy = proxy->pr_next)
			if (proxy->pr_pfn == pfn) 
				break;
		/*
		 * If not found, then we lost the race where the server tried to
		 * export a page the client was returning. We have to go back to the
		 * server again else the firewall & pf_use counts may get messed up.
		 */
		if ( !proxy) {
			PROXY_LIST_UNLOCK(listhead, lock_token);
			STAT(import_again_not_found);
			return(NULL);
		}
	
		/*
		 * The page already exists. Lock the page, then recheck that it
		 * is not being returned to the server. 
		 */
		nested_pfdat_lock(&proxy->pr_pfd);
		ASSERT(proxy->pr_pfd.pf_flags & P_PROXY);
		if (proxy->pr_pfd.pf_flags&P_RECYCLE) {
			nested_pfdat_unlock(&proxy->pr_pfd);
			PROXY_LIST_UNLOCK(listhead, lock_token);
			STAT(import_again_recycling);
			return(NULL);
		}

		ASSERT(proxy->pr_pfd.pf_use && !(proxy->pr_pfd.pf_flags&P_QUEUE) ||
			!proxy->pr_pfd.pf_use && proxy->pr_pfd.pf_flags&P_QUEUE);

		/*
		 * Its safe to drop the proxy list lock now that we
		 * have the pfd lock.
		 */
		NESTED_PROXY_LIST_UNLOCK(listhead);


		/*
		 * If the page is in the free queue, remove it. If the page
		 * has been decommission from the vnode cache AND is in the
		 * free queue, then no other thread is using the page. Reclaim the 
		 * proxy entry & reuse it. Otherwise, we must ignore the entry -
		 * this happens on duplicate imports caused by race conditions.
		 */
		reuse_proxy = ((proxy->pr_pfd.pf_flags&(P_QUEUE|P_HASH)) == P_QUEUE);
		old_cell = proxy->pr_export_cell;
		if (reuse_proxy && proxy->pr_pfd.pf_flags&P_QUEUE) {
			NESTED_PROXY_FREE_LOCK(old_cell);
			proxy->pr_pfd.pf_next->pf_prev = proxy->pr_pfd.pf_prev;
			proxy->pr_pfd.pf_prev->pf_next = proxy->pr_pfd.pf_next;
			proxy->pr_pfd.pf_next = proxy->pr_pfd.pf_prev = NULL;
			proxy->pr_pfd.pf_flags &= ~P_QUEUE;
			proxy->pr_pfd.pf_timestamp = 0;
			NESTED_PROXY_FREE_UNLOCK(old_cell);
		}

		if (reuse_proxy) {
			STAT(import_again_cached);
			proxy->pr_pfd.pf_flags &= ~P_ALLFLAGS;
			proxy->pr_pfd.pf_flags |= P_PROXY;
			proxy->pr_pfd.pf_pageno = 0;
			proxy->pr_pfd.pf_vp = 0;
			proxy->pr_pfd.pf_use = 1;
			proxy->pr_export_cell = export_cell;
		} else {
			ASSERT(export_cell == proxy->pr_export_cell);
			STAT(import_again_duplicate);
		}
		pfdat_unlock(&proxy->pr_pfd, lock_token);

		return (reuse_proxy ? &proxy->pr_pfd : NULL);
	
	} else {

		/*
		 * This should be the first import of this page.  Initialize a
		 * new proxy and store it in the proxy list.
		 */
	
		STAT(import);
		new_proxy = kmem_zone_zalloc(proxy_zone, KM_SLEEP);

		new_proxy->pr_pfn          = pfn;
		new_proxy->pr_export_cell  = export_cell;
		new_proxy->pr_pfd.pf_use   = 1;
		new_proxy->pr_pfd.pf_flags = P_PROXY;

		lock_token = PROXY_LIST_LOCK(listhead);
#ifdef DEBUG
		for (proxy = listhead->pr_next; proxy; proxy = proxy->pr_next)
			if (proxy->pr_pfn == pfn) 
				break;
		ASSERT(proxy == NULL);
#endif
		new_proxy->pr_next         = listhead->pr_next;
		listhead->pr_next          = new_proxy;
		PROXY_LIST_UNLOCK(listhead, lock_token);
	
		return &new_proxy->pr_pfd;
	}
}



/*
 * proxy_pagefree
 *
 * Put the page on a list of pages to be returned to the server.
 *
 * Returns 0 - if page not really freed
 *         1 if page freed (use count went to zero)
 *
 * NOTE that this routine is always called with the pfd locked.
 * Other objects are locked too so we dont want to do much work here.
 */

int
proxy_pagefree(pfd_t *pfd)
{
	cell_t		cell;
	int		s;

	STAT(proxy_pagefree);

	/*
	 * decrement the use count. If not zero, we are done.
	 */
	s = pfdat_lock(pfd);
	if (pfd->pf_use == 0) 
		cmn_err_tag(89,CE_PANIC, "proxy_pagefree: page use %d (0x%x)",
			pfd->pf_use, pfd);

	if (--pfd->pf_use > 0) {
		pfdat_unlock(pfd, s);
		return(0);
	}

	STAT(proxy_pagefree_zero);
	ASSERT(pfd->pf_rmapp == 0);
	ASSERT(pfd->pf_hchain == NULL || (pfd->pf_flags & P_HASH));
	ASSERT(pfd->pf_use == 0);
	ASSERT(pfd->pf_rawcnt == 0);
	ASSERT((pfd->pf_flags & (P_PROXY|P_RECYCLE|P_QUEUE|P_SQUEUE|P_WAIT)) ==
				 (P_PROXY));

	/*
	 * Page is no longer in use.
	 * Put the page into the free queue for the server that the page
	 * came from.
	 */
	cell = pfdattoproxy(pfd)->pr_export_cell;

	NESTED_PROXY_FREE_LOCK(cell);
	pfd->pf_flags |= P_QUEUE;
	if (pfd->pf_flags&P_HASH) {
		append(&proxy_freelist[cell].px_freelist, pfd);
		pfd->pf_timestamp = lbolt + unimport_age_ticks;
	} else {
		prefix(&proxy_freelist[cell].px_freelist, pfd);
		pfd->pf_timestamp = 0;
	}
	NESTED_PROXY_FREE_UNLOCK(cell);
	pfdat_unlock(pfd, s);
	return(1);
}




/*
 * proxy_pageunfree
 *
 * Remove a proxy entry from the freelist. Called when a proxy
 * entry that is on the freelist is being attached in vnode_page_find.
 *
 * Always called with the page & vnode pcache locked.
 */

void
proxy_pageunfree(pfd_t *pfd)
{
	cell_t		cell;

	STAT(proxy_pageunfree);
	ASSERT(pfd->pf_use == 0);
	ASSERT((pfd->pf_flags & (P_BITLOCK|P_PROXY|P_QUEUE|P_SQUEUE|P_WAIT|P_BAD)) ==
				 (P_BITLOCK|P_QUEUE|P_PROXY));

	cell = pfdattoproxy(pfd)->pr_export_cell;

	NESTED_PROXY_FREE_LOCK(cell);
	if ( !(pfd->pf_flags&P_RECYCLE)) {
		pfd->pf_flags &= ~P_QUEUE;
		pfd->pf_next->pf_prev = pfd->pf_prev;
		pfd->pf_prev->pf_next = pfd->pf_next;
		pfd->pf_timestamp = 0;
	}
	NESTED_PROXY_FREE_UNLOCK(cell);
}


/*
 * proxy_decommission_free_page
 *
 * Decommission a page in the vnode page cache that is already
 * in the free queue. The page is relinked to the beginning
 * of the queue so it will be returned to the server a little
 * quicker.
 *
 * Always called with the page & vnode pcache locked.
 */

void
proxy_decommission_free_page(pfd_t *pfd)
{
	proxy_pfd_t	*proxy;
	cell_t		 cell;

	STAT(proxy_decommission_free_page);
	ASSERT(pfd->pf_use == 0);
	ASSERT((pfd->pf_flags & (P_BITLOCK|P_RECYCLE|P_PROXY|P_HASH|P_QUEUE|P_SQUEUE|P_WAIT)) ==
				 (P_BITLOCK|P_PROXY|P_QUEUE));

	proxy = pfdattoproxy(pfd);
	cell = proxy->pr_export_cell;

	pfd->pf_timestamp = 0;

	/*
	 * Relink the page to the beginning of the free list. This can
	 * be skipped if the proxy is NOT first in the free list AND
	 * its predecessor has an expired timestamp.
	 */
	if (pfd != proxy_freelist[cell].px_freelist.pf_next && 
			pfd->pf_prev->pf_timestamp > lbolt) {
		NESTED_PROXY_FREE_LOCK(cell);
		pfd->pf_next->pf_prev = pfd->pf_prev;
		pfd->pf_prev->pf_next = pfd->pf_next;
		prefix(&proxy_freelist[cell].px_freelist, pfd);
		NESTED_PROXY_FREE_UNLOCK(cell);
	}
}




/*
 * unimport_page_demon
 *
 * This demon runs periodically to unimport pages that were imported
 * from other cells but are no longer being used.
 */

void
unimport_page_demon()
{
	proxy_freelist_t	 *pxfree;
	int			 cell;

	for(;;) {
		STAT(unimport_page_demon);
		for (cell=0, pxfree=proxy_freelist; cell<=MAX_CELL_NUMBER; pxfree++, cell++) {
			if (!isempty(&pxfree->px_freelist) && 
					pxfree->px_freelist.pf_next->pf_timestamp < lbolt)
				unimport_cell_pages(pxfree, cell);
		}

		delay(unimport_interval_ticks);
	}
}



/*
 * unimport_cell_pages
 *
 * Called to unimport pages that have been imported from a cell.
 *
 */

void
unimport_cell_pages(proxy_freelist_t *pxfree, cell_t cell)
{
	proxy_pfd_t		*proxy, *next_proxy, *stop_proxy, **prev;
	proxy_list_t		*listhead;
	pfn_t			 pfnlist[PFN_LIST_SIZE];
	service_t		 svc;
	int			 lock_token;
	int			 count;
	/* REFERENCED */
	int			 msgerr;

	STAT(unimport_cell_pages);

	/*
	 * Remove the chain of proxys to be freed from the freelist. The
	 * chain is the portion starting with the head (next) & going thru 
	 * all entries having a timestamp less or equal to lbolt.
	 */

	for(;;) {
		/*
		 * Walk down the freelist starting with the oldest entries and
		 * mark the entries as being returned to the server. Pages with
		 * P_RECYCLE are being returned & cannot be reclaimed. This
		 * keeps import_page from finding the entries and reusing them.
		 * Stop when we find the end, the appropriate timestamp or
		 * when we reach the max number to free.
		 */
		lock_token = PROXY_FREE_LOCK(cell);
		proxy = next_proxy = pfdattoproxy(pxfree->px_freelist.pf_next);
		stop_proxy = pfdattoproxy(&pxfree->px_freelist);
		count = 0;
		while (proxy != stop_proxy && proxy->pr_pfd.pf_timestamp <= lbolt 
					&& count < PFN_LIST_SIZE) {
			ASSERT((proxy->pr_pfd.pf_flags & (P_PROXY|P_RECYCLE|P_QUEUE|P_SQUEUE|P_WAIT)) ==
				 (P_PROXY|P_QUEUE));
			if (!nested_pfdat_trylock(&proxy->pr_pfd)) {
				STAT(unimport_cell_pages_lock_conflict);
				break;
			}
			proxy->pr_pfd.pf_flags |= P_RECYCLE;
			proxy->pr_pfd.pf_flags &= ~P_QUEUE;
			nested_pfdat_unlock(&proxy->pr_pfd);
			if (proxy->pr_pfd.pf_flags & P_HASH) {
				STAT(unimport_aged_out);
				VNODE_PCACHE_INCREF(proxy->pr_pfd.pf_vp);
				proxy->pr_pfd.pf_prev = (pfd_t*)proxy->pr_pfd.pf_vp;
			} else
				proxy->pr_pfd.pf_prev = NULL;
			proxy = pfdattoproxy(proxy->pr_pfd.pf_next);
			count++;
		}

		if (count) {
			pxfree->px_freelist.pf_next = &proxy->pr_pfd;
			proxy->pr_pfd.pf_prev->pf_next = NULL;
			if (proxy == stop_proxy)
				pxfree->px_freelist.pf_prev = &stop_proxy->pr_pfd;
			else
				proxy->pr_pfd.pf_prev = &stop_proxy->pr_pfd;
		}
		PROXY_FREE_UNLOCK(cell, lock_token);

		if (!count)
			return;

		/* 
		 * Now remove the entries from the proxy list.
		 */
		proxy = next_proxy;
		count = 0;
		while (proxy) {
			if (proxy->pr_pfd.pf_prev)
				vnode_page_recycle((vnode_t*)proxy->pr_pfd.pf_prev, &proxy->pr_pfd);
			listhead = LISTHEAD(proxy->pr_pfn);

			lock_token = PROXY_LIST_LOCK(listhead);
			for (prev = &listhead->pr_next; *prev; prev = &((*prev)->pr_next))
				if (*prev == proxy)
					break;
			if (*prev == NULL)
				cmn_err_tag(90,CE_PANIC, "unimport_page: proxy not found for 0x%x",
					proxy);
			*prev = proxy->pr_next;
			PROXY_LIST_UNLOCK(listhead, lock_token);

			pfnlist[count++] = proxy->pr_pfn;
			next_proxy = pfdattoproxy(proxy->pr_pfd.pf_next);

			kmem_zone_free(proxy_zone, proxy);
			proxy = next_proxy;
		}

		STATN(unimport_cell_pages_count, count);
		SERVICE_MAKE(svc, cell, SVC_EXIM);
		msgerr = invk_exim_unexport_pages(svc, pfnlist, count, cellid());
		ASSERT(!msgerr);
	}
	
}


/*
 * pfntopfdat
 *
 * Find the pfdat for the given pfn.  The pfn could be either local or
 * remote.  If the pfn is remote, we must have already imported the
 * page, so return the proxy pfdat.  Otherwise, return the local pfdat
 * for local pfns.
 */

pfd_t *
pfntopfdat(pfn_t pfn)
{
	proxy_pfd_t	*proxy;
	proxy_list_t	*listhead;
	int		 lock_token;
	pfd_t		*ret;

	/*
	 * Check for a proxy first.
	 */

	STAT(pfntopfdat);
	listhead = LISTHEAD(pfn);
	lock_token = PROXY_LIST_LOCK(listhead);

	for (proxy = listhead->pr_next; proxy; proxy = proxy->pr_next)
		if (proxy->pr_pfn == pfn) {
			PROXY_LIST_UNLOCK(listhead, lock_token);
			ASSERT( !(proxy->pr_pfd.pf_flags&P_RECYCLE));
			STAT(pfntopfdat_proxy);
			return &proxy->pr_pfd;
		}

	PROXY_LIST_UNLOCK(listhead, lock_token);

	/*
	 * If we didn't find a proxy, then this must be a local pfn.
	 */

	ret = local_pfntopfdat(pfn);
	return ret;
}


/*
 * proxy_pfdattopfn
 *
 * Given a pfdat pointer for a proxy, return its pfn.
 */

pfn_t
proxy_pfdattopfn(pfd_t *pfd)
{
	proxy_pfd_t	*proxy;

	ASSERT(pfd->pf_flags & P_PROXY);

	proxy = pfdattoproxy(pfd);
	return proxy->pr_pfn;
}


/*===============================================================================*/



/*
 * I_exim_unexport_pages
 *
 * Called from clients to drop the export of a page to the client.
 */
void
I_exim_unexport_pages(pfn_t *pfnlist, size_t count, cell_t cell) {
	int	i;

	STAT(I_unexport_pages);
	for (i=0; i<count; i++)
		unexport_page(pfntopfdat(pfnlist[i]), cell);
}


/*
 * idbg_pfntopfdat
 *
 * Find the pfdat for the given pfn.  The pfn could be either local or
 * remote.  If the pfn is remote, we must have already imported the
 * page, so return the proxy pfdat.  Otherwise, return the local pfdat
 * for local pfns.
 *
 * For use by the debugger ONLY. No locking & returns NULL if page not
 * found in the proxy list.
 */

pfd_t *
idbg_pfntopfdat(pfn_t pfn)
{
	proxy_pfd_t	*proxy;
	proxy_list_t	*listhead;

	/*
	 * Check for a proxy first.
	 */

	listhead = LISTHEAD(pfn);
	for (proxy = listhead->pr_next; proxy; proxy = proxy->pr_next)
		if (proxy->pr_pfn == pfn)
			return &proxy->pr_pfd;

	return (NULL);
}

#ifdef DEBUG
/*
 * Print the list of pfd proxy's.
 *	exim_proxy [<cell>]
 *
 *		- if cell not supplied, then print list of all imported
 *		  pages on the current cell.
 *		- if <cell> is supplied, then then current freelist on the
 *		  current cell of pages imported from <cell> is printed.
 */
/*ARGSUSED*/
void
idbg_exim_proxy_list (__psint_t cell)
{
	proxy_pfd_t	*proxy;
	proxy_list_t	*listhead;
	int		hash;


	if (cell >= 0) {
		qprintf("\nFreelist on cell %d of pages from cell %d\n", cellid(), cell);
		proxy = pfdattoproxy(proxy_freelist[cell].px_freelist.pf_next);
		while (proxy != pfdattoproxy(&proxy_freelist[cell].px_freelist)) {
			idbg_dopfdat1(&proxy->pr_pfd);
			if (proxy->pr_pfd.pf_timestamp)
				qprintf("  age %d", proxy->pr_pfd.pf_timestamp-lbolt);
			qprintf("\n");
			proxy = pfdattoproxy(proxy->pr_pfd.pf_next);
		}
	} else {
		qprintf("\nPROXYs for cell %d\n", cellid());
		for (hash=0, listhead=proxy_list_head; 
				hash < NUM_PROXY_LISTS; listhead++, hash++) {
			for (proxy = listhead->pr_next; proxy; proxy = proxy->pr_next) {
				idbg_dopfdat1(&proxy->pr_pfd);
				if ((proxy->pr_pfd.pf_flags&P_QUEUE) && proxy->pr_pfd.pf_timestamp)
					qprintf("  age %d", proxy->pr_pfd.pf_timestamp-lbolt);
				qprintf("\n");
			}
		}
	}
	
}
#endif /* DEBUG */

#if defined(EXIM_STATS) || defined(DEBUG)

/*
 * Print the entire behavior lock log. Called from idbg command (blalog).
 */
/*ARGSUSED*/
void
idbg_exim_print_stats (__psint_t x)
{

	qprintf("\nEXIM Statistics, cell %d....\n", cellid());
	qprintf("%10d export page\n", exim_stats.export);
	qprintf("%10d   duplicate export\n", exim_stats.export_duplicate);
	qprintf("\n");
	qprintf("%10d export an imported page\n", exim_stats.export_imported_page);
	qprintf("\n");
	qprintf("%10d unexport page\n", exim_stats.unexport);
	qprintf("\n");
	qprintf("%10d currently exported\n", exim_stats.export-exim_stats.unexport-
				exim_stats.export_duplicate);
	qprintf("\n");
	qprintf("%10d import page\n", exim_stats.import+exim_stats.import_again);
	qprintf("%10d   import new page\n", exim_stats.import);
	qprintf("%10d   import already exported page\n", exim_stats.import_again);
	qprintf("%10d     page not found\n", exim_stats.import_again_not_found);
	qprintf("%10d     page being unimported\n", exim_stats.import_again_recycling);
	qprintf("%10d     duplicate import\n", exim_stats.import_again_duplicate);
	qprintf("\n");
	qprintf("%10d currently imported\n", exim_stats.import-exim_stats.unimport_cell_pages_count);
	qprintf("\n");
	qprintf("%10d proxy pagefree page\n", exim_stats.proxy_pagefree);
	qprintf("%10d   use count now zero\n", exim_stats.proxy_pagefree_zero);
	qprintf("\n");
	qprintf("%10d proxy pageunfree page\n", exim_stats.proxy_pageunfree);
	qprintf("%10d proxy decommission page\n", exim_stats.proxy_decommission_free_page);
	qprintf("\n");
	qprintf("%10d unimport demon run count\n", exim_stats.unimport_page_demon);
	qprintf("%10d   unimport cell pages called\n", exim_stats.unimport_cell_pages);
	qprintf("%10d   aged out\n", exim_stats.unimport_aged_out);
	qprintf("%10d   total pages unimported\n", exim_stats.unimport_cell_pages_count);
	qprintf("\n");
	qprintf("%10d unimport lock conflict\n", exim_stats.unimport_cell_pages_lock_conflict);
	qprintf("\n");
	qprintf("%10d I_unexport_pages\n", exim_stats.I_unexport_pages);
	qprintf("\n");
	qprintf("%10d pfntopfdat\n", exim_stats.pfntopfdat);
	qprintf("%10d   proxy\n", exim_stats.pfntopfdat_proxy);
	qprintf("\n");
}
#endif

