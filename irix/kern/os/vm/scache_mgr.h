#ifndef __SYS_SCACHE_MGR_H__
#define __SYS_SCACHE_MGR_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sys/scache_mgr.h
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

#ident	"$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/immu.h>


/*
 * This header file contains internal definitions for the swap cache
 * manager.  It is to be included and used *only* by the swap cache
 * manager code and by kernel debuggers.  All other users are restricted
 * to the definitions provided in sys/scache.h.
 */


/*
 * Hash blocks can never be bigger than a page since that's the most
 * memory we know we can get without causing a memory deadlock (or
 * running out of K2 space) when the page out daemon is running.  We
 * also don't want them to be too small or else there will be too many
 * entries in the hash table which will slow down insertions and searches.
 * The number of handles in each block must also be a power of two since
 * a simple modulo function is used to determine the index of a page within
 * a block.
 *
 * Having 2Kb worth of swap handles per block seems to be a reasonable
 * compromise.  This yields 512 handles per block.
 */

#define HANDLES_PER_BLOCK	((2*1024)/sizeof(sm_swaphandle_t))


/*
 * The hashblock is an element on the hash chain when the swap handle cache
 * is organized as a hash table.  It holds the swap handles for a contiguous
 * range of lpn's from hb_lpn to (hb_lpn+HANDLES_PER_BLOCK-1).  The value in
 * the hb_lpn field is always a multiple of HANDLES_PER_BLOCK.
 */

struct hashblock {
	pgno_t			hb_lpn;	    /* starting lpn for this block    */
	ushort			hb_count;   /* # of swap handles in this block*/
	struct hashblock       *hb_next;    /* next block in hash chain       */
	sm_swaphandle_t		hb_sh[HANDLES_PER_BLOCK]; /* the swap handles */
};

typedef struct hashblock	hashblock_t;


/*
 * The following are convenience macros for accessing fields in the
 * scache structure (defined in scache.h).
 */

#define sc_hash		sc_un.sc_un_hash
#define sc_list		sc_un.sc_un_list
#define sc_array	sc_un.sc_un_array

/*
 * We use the sc_size field for two different purposes.  Since we can't have
 * a union inside a bit field, we use a #define here instead.  sc_maxlpn is
 * used when the swap cache is organized as an array.  sc_size is used when
 * its a hash table.  This helps make the code more readable.
 */

#define sc_maxlpn	sc_size

/*
 * Possible swap cache organization modes for sc_mode:
 */

#define SC_NONE		0	/* no cache allocated yet		*/
#define SC_ARRAY	1	/* cache is organized as linear array	*/
#define SC_HASH		2	/* cache is organized as hash table	*/


/*
 * When using an array of swap handles, we can hold up to one page worth
 * of swap handles.  We don't want to go over a page since the array has
 * to be contiguous in the kernel's address space and we don't want to
 * bother trying to get physically contiguous memory or allocating K2 space.
 * It's just too much trouble to do this.  (We're better off with a hash
 * table at that point anyway.)  So we define here the maximum lpn that
 * can be stored in the swap handle cache when organized as an array.
 * When the lpn's get bigger than this, the cache is transformed into a
 * hash table.
 */

#define MAX_ARRAY_LPN	((NBPP / sizeof(sm_swaphandle_t)) - 1)


/*
 * When the swap handle cache is organized as a hash table, we need to
 * compute the index of the entry for a particular lpn within its
 * hash block.  Since the number of handles in a hash block is a power
 * of two, a simple masking function can be used.
 */

#define LPN_TO_BLOCK_INDEX(lpn)	((lpn) & (HANDLES_PER_BLOCK - 1))


/*
 * Likewise, given an lpn, we need to compute the starting lpn for its
 * hash block for use in the hashing function.
 */

#define LPN_TO_BLOCK_START(lpn) ((lpn) & ~((HANDLES_PER_BLOCK - 1)))


/*
 * A given lpn is contained in a hash block if it falls in the range
 * [hb_lpn, hb_lpn + HANDLES_PER_BLOCK).
 */

#define LPN_IN_BLOCK(block, lpn) ((lpn) >= (block)->hb_lpn && \
				  (lpn) < (block)->hb_lpn+HANDLES_PER_BLOCK)


/*
 *	We want to keep the maximum number of hash blocks in a hash chain
 *	short since each access will most likely result in a cache miss.
 */

#define BLOCKS_PER_CHAIN	8


/*
 *	The hash function varies depending on the number of buckets in
 *	the table.  When the cache is operating as a linked list (i.e.,
 *	when there's only a single bucket) a pointer to the list is returned.
 *	When there's a true hash table, a simple modulo function is used.
 *	The number of buckets in the cache must be a power of 2 for this
 *	to work.
 *
 *	The cache is accessed based on the starting page number	of the hash
 *	block being entered or searched for.  Since the starting lpn of each
 *	block is a multiple of HANDLES_PER_BLOCK, we divide by this value to
 *	get things spread out in the hash table.
 */

#define SHASH(cache, pageno) 						      \
	((cache)->sc_size == 1 ?					      \
	 (cache)->sc_list :						      \
	 (cache)->sc_hash[(LPN_TO_BLOCK_START(pageno) / HANDLES_PER_BLOCK)  & \
		  	((cache)->sc_size-1)])

#define SHASH_HEAD_ADDR(cache, pageno) 						      \
	((cache)->sc_size == 1 ?					      \
	&(cache)->sc_list :						      \
	&(cache)->sc_hash[(LPN_TO_BLOCK_START(pageno) / HANDLES_PER_BLOCK)  & \
		  	((cache)->sc_size-1)])

#define SHASH_SET_BUCKET(cache, pageno, block)				      \
	{ if ((cache)->sc_size == 1)					      \
		(cache)->sc_list = (block);				      \
	  else								      \
	 	(cache)->sc_hash[(LPN_TO_BLOCK_START(pageno) /		      \
				HANDLES_PER_BLOCK)  & ((cache)->sc_size-1)] = \
				(block);				      \
	}


/*
 * These definitions are for the management of the private pool of pages
 * from which the scache routines dynamically allocate space.
 */


/*
 * Pool of pages to be used to allocate swap cache structures.
 */

typedef struct scache_pool {
        struct pfdat	*swap_reserve_page_list; /* Linked list of pages    */
        int		 num_reserved_pages;     /* Number of pages on list */
	int		 num_pages_needed;	 /* Desired # of pages      */
	lock_t		 scache_pool_lock;       /* Lock for this struct    */
} scache_pool_t;


#define	SCACHE_POOL_LOCK()		\
		mp_mutex_spinlock(&scache_pool.scache_pool_lock)

#define	SCACHE_POOL_UNLOCK(locktoken)	\
		mp_mutex_spinunlock(&scache_pool.scache_pool_lock, locktoken)

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SCACHE_MGR_H__ */
