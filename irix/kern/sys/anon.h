/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_ANON_H__
#define __SYS_ANON_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.30 $"

#include <sys/types.h>
#include <sys/getpages.h>


/*
 * Anonymous Memory Manager - External Interfaces
 *
 * The Anonymous Manager manages pages that have no copy in a file.
 * This includes things like stack, bss, modified data, and modified
 * MAP_PRIVATE mappings in general.  The purpose of the Anonymous
 * Manager is to remember which pages within a region are anonymous
 * and where they are located (either in memory, on swap, or both).  It
 * also allows sharing of anonymous pages for copy-on-write purposes.
 *
 * All Anonymous Manager data structures are private and can only be
 * accessed through the interfaces in this file.  A handle is returned
 * when anonymous memory is first initialized.  This must be provided
 * on all future calls.  Most calls may sleep while waiting to allocate
 * data structures.
 */


/*******************************************************************************
 * handle = anon_new()
 *
 * Returns a handle to a new anonymous structure.  Called when
 * a new region is initialized.
 */

extern anon_hdl anon_new(void);


/*******************************************************************************
 * anon_free(handle)
 *
 * Called to disassociate the region from its anonymous memory.  The
 * handle is no longer valid after this call.
 */

extern void anon_free(anon_hdl);


/*******************************************************************************
 * child_hdl = anon_dup(&par_hdl)
 *
 * Called to dup the anonymous memory associated with par_hdl using 
 * copy-on-write.  anon_dup may change the par_hdl.
 */

extern anon_hdl anon_dup(anon_hdl *);


/*******************************************************************************
 * anon_hold(anon_hdl)
 *
 * Grab and hold a reference to the anonymous structure associated with the
 * given handle.  The caller must be holding a pfdat lock for a page 
 * associated with the anon handle.  This call is used prior to calling
 * routines like anon_recycle() and ensures that the handle won't be
 * freed in the meantime.
 */

extern void anon_hold(anon_hdl);


/*******************************************************************************
 * key = anon_lookup(handle, lpn, &swp_hdl)
 *
 * Checks to see if lpn is present in the anonymous memory associated
 * with handle.  If found, the cache key associated with the page is returned. 
 * A swap handle is also returned if a copy of the page can be found on swap.  
 * Returns 0 if page not found.
 */

extern void *anon_lookup(anon_hdl, pgno_t, sm_swaphandle_t *);


/*******************************************************************************
 * pfd = anon_pfind(handle, lpn, &id, &swp_hdl)
 *
 * Checks to see if lpn is present in the anonymous memory associated
 * with handle.  If found, a cache key (id) is returned that can be used to
 * identify the page in the page cache.  If the page is in the page
 * cache, a pointer to its pfdat is returned.  A swap handle is also returned
 * if a copy of the page can be found on swap.  Returns 0 if page not found.
 */

extern struct pfdat *anon_pfind(anon_hdl, pgno_t, void **, sm_swaphandle_t *);


/*******************************************************************************
 * key = anon_tag(handle)
 *
 * Return the cache key associated with the given anonymous handle.  The
 * result of anon_tag and anon_lookup is the same for any private pages
 * the region has (those not being shared copy-on-write).
 * (DO NOT RELY on the present implementation of this function!)
 */

#define anon_tag(handle)	((void *)(handle))


/*******************************************************************************
 * anon_insert(handle, pfd, lpn, pdoneflag)
 *
 * Adds pfd as a new anonymous page to be managed by the Anonymous Manager.  
 * Lpn specifies the page's page number.  This new page covers any existing
 * copy-on-write page with the same lpn.  Future calls to anon_lookup will
 * return the data associated with the new page.  The new page is assumed to
 * have no swap associated with it.  Any swap space associated with a covered
 * page will eventually be released when all other references to it are gone.
 */

extern void anon_insert(anon_hdl, struct pfdat *, pgno_t, int);


/*******************************************************************************
 * anon_modify(handle, pfd, sanon_region)
 *
 * Informs anonymous manager that the given page is being modified.  If
 * possible, the page is stolen for this process.  This causes any swap
 * information for the page to be discarded and the physical page to be made
 * private to the given handle.  If stealing isn't possible, the page state
 * is left unchanged and the function return COPY_PAGE to inform the caller
 * to copy it and anon_insert the new copy.  The caller should pass true
 * for the sanon_region flag if this is a shared anonymous region.
 */

typedef enum { PAGE_STOLEN, COPY_PAGE } anon_modify_ret_t;

extern anon_modify_ret_t anon_modify(anon_hdl, struct pfdat *, int);


/*******************************************************************************
 * anon_swapin(handle, pfd, lpn)
 *
 * Informs anonymous manager that the given page has been/will be swapped
 * in and should be inserted into the page cache.  The swap handle associated
 * with the page is left intact.  It is up to the caller to set the P_DONE
 * flag when the swap I/O is complete.  Returns SUCCESS if page was inserted
 * or returns DUPLICATE if the page is already present in the cache (meaning
 * another process has already swapped in the the page).
 */

typedef enum { SUCCESS, DUPLICATE } anon_swapin_ret_t;

extern anon_swapin_ret_t anon_swapin(anon_hdl, struct pfdat *, pgno_t);


/*******************************************************************************
 * anon_pagemigr(handle, old_pfd, new_pfd)
 *
 * Check whether the page represented by old_pfd is migratable and
 * if so, copy its state to new_pfd, remove it from the pcache,
 * and insert new_pfd.
 */

extern int anon_pagemigr(anon_hdl, struct pfdat *, struct pfdat *); 


/*******************************************************************************
 * anon_flip(old_pfd, new_pfd)
 *
 * Exchanges new_pfd for old_pfd in the page cache.  All future lookup
 * operations will find the new page.  The old page is disassociated from
 * the anonymous memory region and returned in the correct state for use
 * as a kernel data page.
 */

extern void anon_flip(struct pfdat *, struct pfdat *);


/*******************************************************************************
 * anon_setsize(handle, newsize)
 *
 * Set the size of an anonymous region to 'newsize' pages.  Must be called
 * when a region is shrunk so that stale anonymous pages can no longer be
 * referenced by the reion. Does not need to be called when a region is
 * grown.  Does not allocate physical memory or swap on grow.
 * Any swap space released as a result of a shrink will eventually be freed 
 * when the last reference goes away.
 */

extern void anon_setsize(anon_hdl, pgno_t);


/*******************************************************************************
 * anon_getsizes(handle, start, len, anonpages, maxanon)
 *
 * Returns some size information about the anonymous region associated with
 * the given handle.  'Start' and 'len' specify the range of pages being
 * enquired about.  'Anonpages' and 'maxanon' are output parameters that, if
 * non-NULL, give the total number of anonymous pages and the maximum anonymous
 * page (as set by anon_insert) in the given range respectively.  If there are 
 * no anonymous pages in the range, 'anonpages' is set to zero and 'maxanon'
 * is set to -1.  This function does not change the state of the anonymous
 * memory in any way.
 */

extern void anon_getsizes(anon_hdl, pgno_t, pgno_t, pgno_t *, pgno_t *);


/*******************************************************************************
 * anon_setswap_and_pagefree(pfd, swaphandle, do_pagedone)
 *
 * Set the swap handle on the given page and free the page.  If the
 * 'do_pagedone' flag is set, then a pagedone() call is made for this
 * page.
 */

extern void anon_setswap_and_pagefree(struct pfdat *, sm_swaphandle_t, int);


/*******************************************************************************
 * anon_pagefree_and_cache(pfd)
 *
 * Free an anonymous page.  Continue caching the page even if the use
 * count goes to zero.  This is different than pagefreeanon() which tosses
 * pages that have a use count of zero.
 */

extern void anon_pagefree_and_cache(struct pfdat *);


/*******************************************************************************
 * anon_clrswap(key, lpn, swaphandle)
 *
 * Clearr the swap handle and free the swap space for the given lpn.  Replace
 * it with the new swaphandle if it is non-NULL.  The 'key' must be the result
 * of a anon_lookup request.
 */

extern void anon_clrswap(void *, pgno_t, sm_swaphandle_t);


/*******************************************************************************
 * anon_remove(handle, lpn, npgs)
 *
 * Remove 'npgs' contiguous pages starting at lpn from the anonymous space
 * associated with 'handle', and free any swap space backing those pages.  
 * The pages must have previously been inserted with anon_insert.
 */

extern void anon_remove(anon_hdl, pgno_t, pgno_t);


/*******************************************************************************
 * anon_uncache(pfd)
 *
 * Remove the given page from the page cache.  Any associated swap handle
 * is left intact.
 */

extern void anon_uncache(struct pfdat *);


/*******************************************************************************
 * anon_recycle(handle, pfd)
 *
 * Remove the given page from the page cache so that pagealloc can recycle
 * it to a new use.  The page must be in the recycle state: P_RECYCLE is
 * set and the page has been removed from the free list.  The caller must
 * already have done an anon_hold on the anonymous handle.  A swap handle
 * must have been associated with the page at some previous point in time
 * before the page can be recycled.
 */

extern void anon_recycle(anon_hdl, struct pfdat *);


/*******************************************************************************
 * anon_isdegenerate(handle)
 *
 * Return true if the passed anon handle is the leaf of a degenerate tree.
 * This is a tree where there is only one leaf which means there are no 
 * other processes sharing any of the pages with copy-on-write.
 */

extern int anon_isdegenerate(anon_hdl);


/*******************************************************************************
 * anon_shake_tree(handle)
 *
 * Garbage collect any swap pages that are no longer needed.  This happens
 * when all processes that were sharing a page with copy-on-write have
 * modified the page.  This should only be called when swap space is running
 * low.  Swap space is normally reclaimed whenever a process exec's or exit's.
 */

extern void anon_shake_tree(anon_hdl);

/*******************************************************************************
 * anon_pagebad(struct pfdat *)
 *
 * Remove the anon page from the page cache and mark it P_BAD.
 */
extern void anon_pagebad(struct pfdat *);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_ANON_H__ */
