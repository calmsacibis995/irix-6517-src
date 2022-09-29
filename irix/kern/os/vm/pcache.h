#ifndef __SYS_PCACHE_H__
#define __SYS_PCACHE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sys/pcache.h
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

#ident	"$Revision: 1.4 $"

#include <sys/types.h>


/*
 * The functions and data structures described here are used to implement
 * a cache of pages associated with a memory object.  Each memory object
 * must allocate the following pcache structure for the object's page
 * cache.  A pointer to this structure is passed into all the page cache
 * routines.  Each memory object must have its own page cache.  The cache
 * is indexed soley with the object's page number.  Replicas of any given
 * page may be stored in the cache.  Use pcache_next to retrieve replicas.
 *
 * It is assumed that this structure will be imbedded within the structure
 * for the memory object itself, which is why the structure definition is
 * given here.  However, no code outside of pcache.c should access or modify
 * any field in this structure, nor make assumptions about the size or
 * entries in this structure staying constant across releases.  It is an
 * opaque data structure for all intents and purposes.
 */


struct pcache {
	uint	  pc_size;	/* # of hash buckets			   */
	uint	  pc_count;	/* # of pages currently in cache	   */
	union {
	   struct pfdat  *pc_un_list;	/* head of pfdat list		   */
	   struct pfdat **pc_un_hash;	/* pointer to hash buckets	   */
	} pc_un;
};

typedef struct pcache	pcache_t;


/*
 * The following operations may be done on a page cache.
 */


/*
 * pcache_init(pcache)
 *
 * Initialize the pcache_t structure.  This must be done prior to passing
 * the structure to any of the following routines.
 */

extern void pcache_init(pcache_t *);


/*
 * token = pcache_getspace(pcache, extra)
 *
 * This routine pre-allocates space, if needed, to expand the page cache
 * to hold "extra" number of pages.  The space is returned through
 * the opaque token return value.  This value is then passed to the
 * routine pcache_resize which actually does the work of resizing the
 * cache.  This allows the memory allocation to be done before the lock
 * protecting the page cache is acquired.  Returns NULL if the page
 * cache is already large enough or if no memory is currently available.
 */

extern void *pcache_getspace(pcache_t *, int);


/*
 * token = pcache_resize(pcache, token)
 *
 * Resize of the page cache using the space previously allocated with
 * pcache_getspace.  The returned token must be passed to the routine
 * pcache_resize_epilogue() at some point.  This may be done after locks
 * the caller is holding are released.
 */

extern void *pcache_resize(pcache_t *, void *);


/*
 * pcache_resize_epilogue(token)
 *
 * Perform final cleanup from a resize operation.  It is not necessary
 * to hold any locks while calling this routine since the cache is not
 * accessed.
 */

extern void pcache_resize_epilogue(void *);


/*
 * pcache_insert(pcache, new_pfd)
 *
 * Add a new page (specified by passing its pfdat) into the page cache.
 * Duplicates are not allowed, so the page number in new_pfd must not
 * match any pages already in the cache.
 */

extern void pcache_insert(pcache_t *, struct pfdat *);


/*
 * pfd = pcache_find(pcache, pageno)
 *
 * Look for a page in the cache whose page number is pageno.  If found, a
 * pointer to its pfdat is returned.  The page is left in the cache in
 * this case.  If not found, NULL is returned.
 */

extern struct pfdat *pcache_find(pcache_t *, pgno_t);


/*
 * pfd = pcache_next(pcache, cur_pfd)
 *
 * Retrieve a replica of cur_pfd.  The first copy of a page is retrieved
 * using pcache_find.  Passing that pfd to pcache_next will retrieve the
 * next replica of the page if there are any.  The pfd of the replica can
 * then be passed to pcache_next, and so on, to retrieve all subsequent
 * replicas.  NULL is returned if there are no more replicas.
 */

extern struct pfdat *pcache_next(pcache_t *, struct pfdat *);


/*
 * pcache_pagecount(pcache)
 *
 * Returns the number of pages currently in the page cache.  Do NOT rely
 * on the fact that this is currently implemented with a macro.  It may
 * change to a function call some day.
 */

#define pcache_pagecount(pcache)	((pcache)->pc_count)


/*
 * pcache_remove(pcache, pageno, count, func, arg, preempt_func, cookie)
 *
 * Find and remove pages in the range [pageno, pageno+count) from the cache.
 * For each page found in the cache (including replicas), the specified
 * function is invoked with arg as the first argument and the pfdat pointer
 * as its second argument.  Periodically, pcache_remove will invoke the
 * supplied preemption function with cookie as its argument.  This function
 * is expected to temporarily release the lock and allow interrupts to
 * occur.  It should re-acquire the lock before returning.
 */

extern void pcache_remove(pcache_t *, pgno_t, pgno_t,
			  void (*)(void *, struct pfdat *), void *,
			  void (*)(void *), void *);


/*
 * pcache_remove_pfdat(pcache, pfd)
 *
 * Find and remove a specific pfdat from the page cache. 
 * It's necessary to do this when the page cache has replicated pages
 * (i.e. pages with same page number, and just one of them needs to be
 * removed).
 */
extern void pcache_remove_pfdat(pcache_t *, struct pfdat *);

/*
 * pcache_release(pcache)
 *
 * This call is used to destory a page cache.  All dynamically allocated
 * data structures for the cache are freed.  All pages must be removed from
 * the cache prior to calling this routine.
 *
 */

extern void pcache_release(pcache_t *);


#ifdef __cplusplus
}
#endif

#endif /* __SYS_PCACHE_H__ */
