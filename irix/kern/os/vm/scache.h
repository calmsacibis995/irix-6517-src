#ifndef __SYS_SCACHE_H__
#define __SYS_SCACHE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sys/scache.h
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

#ident	"$Revision: 1.3 $"

#include <sys/types.h>


/*
 * The functions and data structures described here are used to implement
 * a cache of swap handles associated with an anonymous memory object.  Each 
 * anonymous memory object must allocate one of the following scache structures
 * for the object's swap handle cache.  A pointer to this structure is passed
 * to the interface functions listed below. 
 *
 * It is assumed that this structure will be imbedded within the structure
 * for the memory object itself, which is why the structure definition is
 * given here.  However, no code outside of scache.c should access or modify
 * any field in this structure, nor make assumptions about the size or
 * entries in this structure staying constant across releases.  It is an
 * opaque data structure for all intents and purposes.
 */


struct scache {
	uint	sc_mode:2,	/* cache organization: none, array, or hash */
		sc_size:12,	/* size of array or hash table		    */
		sc_count:18;	/* # of blocks currently stored in hash     */
	union {
		sm_swaphandle_t	  *sc_un_array;	/* array of swap handles    */
		struct hashblock  *sc_un_list;	/* list of hash blocks	    */
		struct hashblock **sc_un_hash;	/* pointer to hash table    */
	} sc_un;
};

typedef	struct scache	scache_t;


/*
 * scache_init()
 *
 * One time initialization routine called at boot time.
 */

void scache_init(void);


/*
 * The following operations may be done on a swap handle cache.  A new
 * scache_t structure must be zero'ed out before the first call to one
 * of the routine below.
 */



/*
 * scache_add(scache, lpn, swap_handle)
 *
 * Associates the swap_handle with the given lpn and store it in the
 * swap handle cache.  Subsequent scache_lookup's on the same lpn will
 * return the assocated swap handle.  The given lpn must *not* have a
 * swap handle already associated with it.
 */

void scache_add(scache_t *, pgno_t, sm_swaphandle_t);


/*
 * swap_handle = scache_lookup(scache, lpn)
 *
 * Retrieve the swap handle associated with lpn.  If there is no associated
 * swap handle, then NULL is returned.
 */

sm_swaphandle_t	scache_lookup(scache_t *, pgno_t);


/*
 * scache_has_swaphandles(scache)
 *
 * Returns true if there are any swap handles in the given cache.  If there
 * are no swap handles at all, false is returned.
 */

int scache_has_swaphandles(scache_t *);


/*
 * scache_clear(scache, lpn)
 *
 * Clear any swap handle associated with lpn.  If there is no swap handle,
 * then this routine does nothing.
 */

void scache_clear(scache_t *, pgno_t);


/*
 * scache_scan(scache, opcode, start, npgs, func, arg)
 *
 * For each swap handle in the given range, call the given function with
 * the lpn and its swap handle as the second and third arguments (i.e., it 
 * is called as (*func)(arg, lpn, swap_handle)).  The lpn's are not sorted
 * so consecutive calls to the function will have lpn's in random order.  
 * The function is not called for lpn's where the swap handle is NULL.
 * 
 * If opcode is SCAN, then the swap handle is left undisturbed in the cache.
 * If opcode is REMOVE, then the swap handle is removed from the cache before
 * calling func.  In this case, it is the functions responsible to either
 * save the swap handle elsewhere or to free it back to the swap manager.
 */

typedef enum {REMOVE, SCAN} scan_ops_t;

void scache_scan(scache_t *, scan_ops_t, pgno_t, pgno_t,
		 void (*)(void *, pgno_t, sm_swaphandle_t), void *);


/*
 * scache_release(scache)
 *
 * This call is used to destory a swap handle cache.  Any remaining swap handles
 * in the cache are freed back to the swap manager using sm_free.  Then all
 * dynamically allocated data structures for the cache are freed.
 */

void scache_release(scache_t *);


/*
 * These definitions are for the management of the private pool of pages
 * from which the scache routines dynamically allocate space.
 */

/*
 * scache_pool_pages_needed()
 *
 * Returns true if the private pool of pages needs additional pages.  Pages
 * may then be added using scache_pool_add() or scache_pool_reserve().
 */

int scache_pool_pages_needed(void);


/*
 * scache_pool_add(pfd)
 *
 * Add the given page to the private pool.  The page must be unused by
 * anything else.
 */

void scache_pool_add(struct pfdat *);


/*
 * scache_pool_reserve()
 *
 * Add as many pages to the scache pool as are needed.  This call may
 * sleep waiting for memory.
 */

void scache_pool_reserve(void);


#ifdef __cplusplus
}
#endif

#endif /* __SYS_SCACHE_H__ */
