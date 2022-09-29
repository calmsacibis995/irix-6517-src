/*
 * os/scache.c
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

#ident	"$Revision: 1.16 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/swap.h>
#include <sys/param.h>
#include <sys/pfdat.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <ksys/vm_pool.h>
#include "scache.h"
#include "scache_mgr.h"

#include "sys/tile.h"

/*
 *	Swap Handle Cache Routines.
 *
 *	This package of routines implements the cache of swap handles for
 *	anonymous memory object pages that have been swapped.
 *
 *	One of two different data structures is used to optimize based
 *	on the size of the associated memory object.  A linear array is
 *	used when the memory object is small enough such that the swap
 *	handles for all the lpn's currently in use fits in a single page or
 *	less.  The array is dynamically grown as needed (in increments
 *	specified in the array_sizes[] array below) up to a full page in
 *	size.  The number of swap handles that fits in a page determines
 *	the maximum lpn that can be used in array mode.  When more than
 *	this number of lpn's is needed, an open hash table is used instead.
 *	If an array was previously in use, then it is automatically converted
 *	to a hash table.  The hash table stores small arrays of swap handles
 *	called hash blocks.  Each hash block contains the swap handles for
 *	a contiguous range of lpn's.  The first lpn in the range for the
 *	hash block is used as the key for the block when searching the hash
 *	table.  The number of buckets in the hash table is dynamically
 *	grown as the size of the memory object increases.  The hash_sizes[]
 *	array specifies the sizes that will be used.  The attempts to limit
 *	the hash chain lengths to BLOCKS_PER_CHAIN entries.
 *
 *	A complication in this code is that the space for the array and
 *	hash tables is dynamically allocated, but the code can never sleep
 *	waiting for memory.  Swap handles are assigned by the page out
 *	daemon and so the data structures to store them are allocated while
 *	running in the daemon's context.  Sleeping for memory would block
 *	the page out daemon and cause a memory deadlock.  In addition, the
 *	routine to store a new swap handle can never fail and so it must
 *	always be guaranteed that it can allocate the required memory space.
 *	To accomplish the necessary memory allocations, the code first tries
 *	to allocate memory from the kernel heap with VM_NOSLEEP.  If this
 *	fails, then it makes use of the fact that the page that was just
 *	swapped out (i.e., the one for which we are currently trying to
 *	associate a swap handle) can be scavenged and used to allocate data
 *	structures for the swap handle cache.  This means that the allocations
 *	needed to store any one swap handle can never exceed one page worth
 *	of memory (which is why the array size, hash table bucket, hash
 *	block size is limited to be a page).  To accomplish this, the page
 *	out daemon maintains a private list of free pages which is always
 *	guaranteed to have at least one page on it whenever it invokes a
 *	swap handle cache routine.  If this code needs to scavenge one of
 *	these pages, any unused space is freed back into the kernel heap
 *	and the page out daemon restocks the private free list on the next
 *	iteration.
 *
 *	This layer of code is fairly low level and has no knowledge of the
 *	memory objects themselves.   It simply caches swap handles in a given
 *	swap handle cache structure that the higher level code associates with
 *	the memory object.
 *
 *	The upper level memory object layer is allocates one scache_t
 *	structure for each memory object.  This is passed into the swap
 *	handle cache routines to identify the cache being operated on.
 *
 *	No MP locking is done by this level at all.  The caller is
 *	responsible for using appropriate locks to ensure swap handle cache
 *	updates are atomic for the respective memory object.  Multiple
 *	readers are allowed in the page cache lookup routine if the
 *	memory object chooses to allow it.
 *
 */

/*
 *	The array_sizes array specifies the possible sizes a swap handle cache
 *	can be in terms of the maximum number of pages it can hold when it's
 *	organized as an array.  We want reasonable increments between
 *	each size so that we don't do too many grow operations (which
 *	requires copying all entries).  So we'll grow the caches by
 *	factors of four.  The array size is limited by what will fit in
 *	a single page.
 *
 *	We want some small sizes at the beginning of this list to avoid
 *	wasting space for all the single page dso data regions and other
 *	small regions.
 */

#define KB	1024
#define MB	(1024*1024)

static int array_sizes[] = {
	/*		 	            	      Total VM          */
	/*  Array Size	    Array Size	              --------		*/
	/*  (in lpn's)	    (in bytes)        4K Pages	      16K Pages	*/
	/*  ----------	    ---------- 	      --------	      --------- */
		  4,	/*	16		 16K		 64K	*/
		 16,	/*	64		 64K		256K	*/
		 64,	/*     256		256K		  1M	*/
		256,	/*	1K		  1M		  4M	*/
	       1*KB,	/*	4K		  4M		 16M	*/
	       4*KB,	/*     16K		  --		 64M	*/
};


/*
 *	The hash_sizes array specifies the possible sizes a swap handle cache
 *	can be in terms of the number of hash buckets in the table.  The sizes
 *	must be a power of 2 so	that the hashing algorithm can do a simple
 *	modulo function using and'ing.
 *
 *	The hash table size is limited by the number of hash bucket pointers
 *	that will fit in a single page.  Once the table reaches that size
 *	we simply let the hash chains get longer.  The last entry in this
 *	array must be for the number of buckets that will fit in an NBPP
 *	sized page for the code to work correctly.
 */


static int hash_sizes[] = {
	/*		    Table Size         	      Total VM          */
	/*  Table Size	    (in bytes)	              --------		*/
	/* (in buckets)     (32/64 bit)	      4K Pages	      16K Pages	*/
	/*  ----------	    ---------- 	      --------	      --------- */
	         2,	/*     8/16		 32M		128M	*/
		 8,	/*    32/64		128M		512M	*/
		32,	/*   128/256		512M		  2G	*/
	       128,	/*   512/1K		  2G		  8G	*/
	       512,	/*    --/4K		  --		 32G	*/
	      2*KB,	/*    --/16K		  --		128G	*/
};


/*
 * Internal scache functions.  For use only by other scache routines.
 */

static void		scache_alloc_array(scache_t *, pgno_t);
static pgno_t		scache_select_maxlpn(pgno_t);
static void		scache_grow_array(scache_t *, pgno_t);
static void		scache_alloc_hash(scache_t *);
static void		scache_grow_hash(scache_t *);
static void		scache_array_to_hash(scache_t *);
static hashblock_t *	scache_find_block(scache_t *, pgno_t);
static hashblock_t *	scache_new_block(scache_t *, pgno_t);
static void		scache_remove_block(scache_t *, hashblock_t *);
static int		scache_select_hash_size(int);
static void		scache_pool_init(void);

static arena_t		*scache_pool_arena;

/*
 * One time initialization called at boot time.
 *
 * Prime our private pool of pages for dynamic memory allocation by placing
 * some pages in it.
 */

void
scache_init(void)
{
	scache_pool_init();
}


/*
 * Insert the given swap handle for the specified lpn into the swap handle
 * cache.  Space is allocated as necessary for cache if it does not already
 * exist or if it needs to be grown.
 */

void
scache_add(scache_t *cache, pgno_t lpn, sm_swaphandle_t swap_handle)
{
	hashblock_t	*block;

	ASSERT(swap_handle != NULL);

	/*
	 * If the lpn is small enough, use a linear array.  Also, if the
	 * the first lpn we get is bigger than the size of a hash block,
	 * assume things will be sparse and start using a hash table
	 * right away.
	 */

	if ((cache->sc_mode == SC_ARRAY && lpn <= MAX_ARRAY_LPN) || 
	    (cache->sc_mode == SC_NONE  && lpn <= HANDLES_PER_BLOCK)) {

		/*
		 * Allocate a new array if this is the first time we've
		 * assigned a swap handle for this memory object.
		 */

		if (cache->sc_mode == SC_NONE)
			scache_alloc_array(cache, lpn);

		/*
		 * If the current array isn't big enough to hold the 
		 * swap handle for lpn, then allocate a bigger one.
		 */

		else if (lpn > cache->sc_maxlpn)
			scache_grow_array(cache, lpn);

		/*
		 * Save the swap handle in the array and we're done.
		 */

		ASSERT(cache->sc_array[lpn] == NULL);
		cache->sc_array[lpn] = swap_handle;

	} else {

		/*
		 * Allocate a hash table if this is the first time we've
		 * assigned a swap handle for this memory object.
		 */

		if (cache->sc_mode == SC_NONE)
			scache_alloc_hash(cache);

		/*
		 * If we were in array mode, then the lpn range is too big
		 * to use an array.  So convert to a hash table.
		 */

		else if (cache->sc_mode == SC_ARRAY)
			scache_array_to_hash(cache);

		/*
		 * Search the hash table to find the block that holds
		 * the swap handle for lpn.  Create the block if it doesn't
		 * already exist.
		 */

		if ((block = scache_find_block(cache, lpn)) == NULL)
			block = scache_new_block(cache, lpn);

		/*
		 * Save the swap handle in the block and we're done.
		 */

		ASSERT(block->hb_sh[LPN_TO_BLOCK_INDEX(lpn)] == NULL);
		block->hb_sh[LPN_TO_BLOCK_INDEX(lpn)] = swap_handle;
		block->hb_count++;
	}
}


/*
 * Find the swap handle for the given lpn.
 */

sm_swaphandle_t
scache_lookup(scache_t *cache, pgno_t lpn)
{
	hashblock_t *block;

	switch(cache->sc_mode) {
	case SC_ARRAY:
		if (lpn > cache->sc_maxlpn)
			return NULL;

		return cache->sc_array[lpn];

	case SC_HASH:
		block = scache_find_block(cache, lpn);

		if (block == NULL)
			return NULL;


		return block->hb_sh[LPN_TO_BLOCK_INDEX(lpn)];
	}

	/*
	 * We get here if no cache has been allocated yet.
	 */

	return NULL;
}


/*
 * Check to see if there are any swap handles in the cache and return
 * true if there are.
 */

int
scache_has_swaphandles(scache_t *cache)
{
	pgno_t	lpn;

	switch(cache->sc_mode) {
	case SC_ARRAY:

		/*
		 * Scan the array and check for swap handles.
		 */

		for (lpn = 0; lpn <= cache->sc_maxlpn; lpn++)
			if (cache->sc_array[lpn])
				return 1;

		break;

	case SC_HASH:

		/*
		 * The code here doesn't cache empty blocks.  So if there
		 * are any blocks, then they must have swap handles in them.
		 */

		if (cache->sc_count > 0)
			return 1;

		break;
	}


	/*
	 * We get here if there's no cache at all or there are no swap
	 * handles in the cache.
	 */

	return 0;
}


/*
 * Clear the swap handle for the given lpn if there is one.  If we found
 * one, tell the swap manager as well so it can free the disk space.
 */

void
scache_clear(scache_t *cache, pgno_t lpn)
{
	hashblock_t *block;
	sm_swaphandle_t	*handlep;

	switch(cache->sc_mode) {
	case SC_NONE:
		break;

	case SC_ARRAY:
		if (lpn <= cache->sc_maxlpn) {
			handlep = &cache->sc_array[lpn];

			if (*handlep) {
				sm_free(handlep, 1);
				*handlep = NULL;
			}
		}

		break;

	case SC_HASH:
		block = scache_find_block(cache, lpn);

		if (block) {
			handlep = &block->hb_sh[LPN_TO_BLOCK_INDEX(lpn)];

			if (*handlep) {
				sm_free(handlep, 1);
				*handlep = NULL;

				if (--block->hb_count == 0)
					scache_remove_block(cache, block);
			}
		}

		break;
	}
}


/*
 * Remove swap handles from the cache
 */

void
scache_scan(scache_t *cache, scan_ops_t opcode, pgno_t start, pgno_t npgs, 
	    void (*func)(void *, pgno_t, sm_swaphandle_t), void *arg)
{
    pgno_t		 lpn, last;
    int			 bucket, i, start_index, end_index;
    hashblock_t		*block, *next;
    sm_swaphandle_t	 swap_handle;

    switch(cache->sc_mode) {
    case SC_NONE:
	return;

    case SC_ARRAY:

	/*
	 * Scan the array and check for swap handles.
	 */

	last = MIN(start + npgs - 1, cache->sc_maxlpn);

	for (lpn = start; lpn <= last; lpn++)
		if (swap_handle = cache->sc_array[lpn]) {
			if (opcode == REMOVE)
				cache->sc_array[lpn] = NULL;

			if (func)
				(*func)(arg, lpn, swap_handle);
			else
				sm_free(&swap_handle, 1);
		}

	return;

    case SC_HASH:

	/*
	 * If the cache is empty, then there's nothing to do.
	 */

	if (cache->sc_count == 0)
		return;

	last  = start + npgs - 1;

	/*
	 * We want to search the hash table in the most efficient manner
	 * possible.  Since we're using a modulo hashing functions that
	 * puts consecutive blocks in consecutive hash buckets, if the
	 * number of blocks we're scanning through is greater than or
	 * equal to the number of hash buckets, then we'll have to visit
	 * each bucket to find all the pages.  In this case, it's better
	 * to make a linear search of all the buckets looking for pages
	 * in the given range.  If the block count is less than the
	 * number of hash buckets, then we only have to visit a subset
	 * of the buckets and it's better to use the hashing function on
	 * each and just look at those buckets.
	 *
	 * We estimate the number of blocks we'll need to check by
	 * just dividing by the block size.  This won't be exact, but
	 * it's easy and is close enough.
	 */

	if (npgs / HANDLES_PER_BLOCK > cache->sc_size) {

		/*
		 * The linear scan is faster.  Walk through each bucket in
		 * the hash table.
		 */
	
		for (bucket = 0; bucket < cache->sc_size; bucket++) {
			if (cache->sc_size == 1)
				block = cache->sc_list;
			else
				block = cache->sc_hash[bucket];
	
			/*
			 * Walk down the hash chain.
			 */
	
			while (block) {
			    next = block->hb_next;

			    /*
			     * If this block doesn't contain any lpn's for the
			     * range we're looking for, then skip it.
			     */

			    if (block->hb_lpn > last ||
				block->hb_lpn + (HANDLES_PER_BLOCK-1) < start) {
				block = next;
				continue;
			    }
    
			    /*
			     * Compute the portion of the range being searched
			     * that overlaps this block.  We begin either at
			     * the start of the block or at the start of the
			     * range if the range begins in the middle of the
			     * block.  Likewise, we end either at the end of
			     * the search range or the end of the block,
			     * whichever comes first.
			     */

			    lpn         = max(start, block->hb_lpn);
			    start_index = LPN_TO_BLOCK_INDEX(lpn);
			    end_index   = LPN_TO_BLOCK_INDEX(MIN(last,
					    block->hb_lpn+HANDLES_PER_BLOCK-1));

			    for (i = start_index; i <= end_index; i++, lpn++) {

				/*
				 * Check each swap handle in the
				 * block for the given range.  Call the
				 * function if we find a non-zero entry.
				 */

				if (swap_handle = block->hb_sh[i]) {

				    if (opcode == REMOVE) {
					block->hb_sh[i] = NULL;

					/*
					 * If this was the last swap handle in
					 * the block, then free up the block
					 * and end the loop through this block.
					 */

					if (--block->hb_count == 0) {
					    scache_remove_block(cache, block);

					    if (func)
					        (*func)(arg, lpn, swap_handle);
					    else
						sm_free(&swap_handle, 1);

					    break;

					} else {
					    if (func)
					        (*func)(arg, lpn, swap_handle);
					    else
						sm_free(&swap_handle, 1);
					}

				    } else {

					/*
					 * For the SCAN case:
					 *
					 * We need special handling for
					 * the last handle in a block.
					 * The function we call may
					 * choose to clear the handle
					 * which in turn causes the
					 * block to be freed (since it
					 * was the last handle in the
					 * block).  So once we call the
					 * function, we can't use the
					 * block pointer anymore.  At
					 * this point, we're done with
					 * the block, so break out of
					 * the for loop and look at the
					 * next block.
					 */
    
					if (block->hb_count == 1) {
					    if (func)
						(*func)(arg, lpn, swap_handle);

					    break;
    
					} else {
					    if (func)
						(*func)(arg, lpn, swap_handle);
					}
				    }
				}
			    }
    
			    /*
			     * Finished that block.  Try the next one.
			     */
    
			    block = next;
			}
		}

	} else {

		/*
		 * Use the hash function to locate the blocks.
		 */

		block = NULL;

		for (lpn = start; lpn <= last; lpn++, i++) {

			/*
			 * If we don't have a block still around from the
			 * last iteration or if we're at the end of the block,
			 * then try to find the block for the current lpn.
			 */

			if (!block || i >= HANDLES_PER_BLOCK) {
				block = scache_find_block(cache, lpn);
				i     = LPN_TO_BLOCK_INDEX(lpn);
			
				/*
				 * If there's no block for this lpn, then
				 * advance lpn to where the end of the block
				 * would be had we had one since we know none
				 * of these lpn's are cached (because there's
				 * no block).
				 */

				if (block == NULL) {
					lpn += (HANDLES_PER_BLOCK - 1) - i;
					continue;
				}
			}

			/*
			 * We have the block that would contain the swap
			 * handle for lpn.  See if a swap handle has been
			 * set.
			 */

			if (swap_handle = block->hb_sh[i]) {
				if (opcode == REMOVE) {
					block->hb_sh[i] = NULL;

					/*
					 * If this was the last swap handle in
					 * the block, then free up the block
					 * and end the loop through this block.
					 */

					if (--block->hb_count == 0) {
					    scache_remove_block(cache, block);
					    block = NULL;

					    if (func)
						(*func)(arg, lpn, swap_handle);
					    else
						sm_free(&swap_handle, 1);

					    continue;

					} else {
					    if (func)
						(*func)(arg, lpn, swap_handle);
					    else
						sm_free(&swap_handle, 1);
					}

				} else {

					/*
					 * Special case: processing last handle
					 * in a block.  See comment above.
					 */
	
					if (block->hb_count == 1) {
					    if (func)
						(*func)(arg, lpn, swap_handle);

					    block = NULL;
	
					} else {
					    if (func)
						(*func)(arg, lpn, swap_handle);
					}
				}
			}
		}
	}

	return;
    }
}


/*
 * Deallocate the swap handle cache.  All swap handles are released by
 * calling into the swap manager and the space for the cache data structures
 * themselves are freed.
 */

void
scache_release(scache_t *cache)
{
	int		 bucket;
	hashblock_t	*block, *next;

	if (cache->sc_mode == SC_NONE) {
		return;

	} else if (cache->sc_mode == SC_ARRAY) {
		sm_free(cache->sc_array, cache->sc_maxlpn + 1);
		kmem_arena_free(scache_pool_arena, cache->sc_array, 
			 (cache->sc_maxlpn + 1) * sizeof(sm_swaphandle_t));

	} else if (cache->sc_mode == SC_HASH) {

		/*
		 * Walk through each bucket in the hash table.
		 */

		for (bucket = 0; bucket < cache->sc_size; bucket++) {
			if (cache->sc_size == 1)
				block = cache->sc_list;
			else
				block = cache->sc_hash[bucket];

			/*
			 * Walk down the hash chain, freeing the swap handles
			 * and the data structures.
			 */

			while (block) {
				sm_free(block->hb_sh, HANDLES_PER_BLOCK);
				next = block->hb_next;
				kmem_arena_free(scache_pool_arena, block,
						sizeof(hashblock_t));
				block = next;
			}
		}

		if (cache->sc_size > 1)
			kmem_arena_free(scache_pool_arena, cache->sc_hash,
				  cache->sc_size * sizeof(hashblock_t *));

	}

	/*
	 * We don't really need to clear this field since most
	 * likely the anon struct is being freed, but it's always
	 * best not to leave stale pointers around just in case
	 * the anon struct is kept around and recycled some day.
	 * Sched keeps the anon/scache around after unlinking them
	 * till victim process exit - mark the scache to prevent
	 * further lookups.
	 */

	cache->sc_hash = NULL;
	cache->sc_mode = SC_NONE;
}


/*
 * Allocate a linear array of swap handles to serve as the swap handle cache.
 * We know we have to store swap handles up to "lpn" pages.  So we round
 * this up a bit and allocate an array that big.
 */

static void
scache_alloc_array(scache_t *cache, pgno_t lpn)
{
	pgno_t	maxlpn;

	ASSERT(lpn <= MAX_ARRAY_LPN);

	maxlpn = scache_select_maxlpn(lpn);

#if OLD
	/*
	 * See if there's any memory left over in kmem pool.  Many times
	 * we'll be allocating a swap handle array for small regions and
	 * hence only need a small array.  If there's no memory, then allocate
	 * out of our private pool.  We can never sleep waiting for memory
	 * since we're in the context of the page out daemon and that will
	 * deadlock the system.  Since the private pool is replenished
	 * immediately from recently swapped pages and since we never request
	 * more than a page, it's guaranteed to always have what we need.
	 */

	if ((cache->sc_array = (sm_swaphandle_t *) kmem_zalloc(
					(maxlpn + 1) * sizeof(sm_swaphandle_t), 
					KM_NOSLEEP)) == NULL) {

		/*
		 * Get memory from our private pool.  This can never fail.
		 */

		cache->sc_array = (sm_swaphandle_t *)
					kmem_zalloc_alternate(scache_pool,
					(maxlpn + 1) * sizeof(sm_swaphandle_t));
		ASSERT(cache->sc_array != NULL);
	}
#endif

	if ((cache->sc_array = (sm_swaphandle_t *) kmem_arena_zalloc(
					scache_pool_arena,
					(maxlpn + 1) * sizeof(sm_swaphandle_t), 
					KM_NOSLEEP)) == NULL)

		cmn_err_tag(92,CE_PANIC, "scache_alloc_array out of memory");


	cache->sc_mode = SC_ARRAY;
	cache->sc_maxlpn = maxlpn;	
}


/*
 * Choose the maximum number of pages that a swap cache can hold based
 * on the maximum lpn it is currently being asked to hold.  This size
 * is rounded up to the next larger size in the array_sizes[] table.
 */

static pgno_t
scache_select_maxlpn(pgno_t maxlpn)
{
	int i;
	pgno_t	maxsize;

	ASSERT(maxlpn <= MAX_ARRAY_LPN);

	/*
	 * Lpn's run from 0 to n-1.  Sizes run from 1 to n.  So add one to
	 * the maximum lpn to get the maximum size.
	 */

	maxsize = maxlpn + 1;

	for (i = 0; maxsize > array_sizes[i]; i++) {

		/*
		 * Defensive programming here.  The table should be 
		 * configured so that we never walk off the end.  But if
		 * the assumptions made here change some day and someone
		 * forgets to update the table...  So stop at the last entry.
		 */

		if (i == (sizeof(array_sizes) / sizeof(int) - 1))
			break;
	}

	return array_sizes[i] - 1;	/* substract 1 from size since */
				  	/* lpn's start from 0	       */
}


/*
 * This routine allocates a new swap handle array since the old one isn't
 * big enough.  The contents of the old array are copied over to the new
 * one and then the old one is freed.
 */

static void
scache_grow_array(scache_t *cache, pgno_t lpn)
{
	sm_swaphandle_t	*old_array;
	int		 old_size;

	old_array = cache->sc_array;
	old_size  = cache->sc_maxlpn + 1;
	scache_alloc_array(cache, lpn);		/* replaces sc_array */
	bcopy(old_array, cache->sc_array, old_size * sizeof(sm_swaphandle_t));
	kmem_arena_free(scache_pool_arena, old_array,
			old_size * sizeof(sm_swaphandle_t));
}


/*
 * Allocate an empty hash table.  There's nothing to cache yet, so just
 * allocate the minimum size, which is a hash table with one bucket.  We
 * use the pointer in the scache structure for this bucket directly to save
 * space for objects with only a few hash blocks.
 */

static void 
scache_alloc_hash(scache_t *cache)
{
	cache->sc_mode  = SC_HASH;
	cache->sc_list  = NULL;
	cache->sc_size  = 1;	
	cache->sc_count = 0;
}


/*
 * Convert an array type cache to a hash table.  Move all the swap handles
 * in the array to the hash table and free up the old array.
 */

static void 
scache_array_to_hash(scache_t *cache)
{
	sm_swaphandle_t	*old_array;
	int		 old_size;
	pgno_t		lpn;
	int		index;
	hashblock_t	*block = NULL;


	/*
	 * Remember where the old array is and its size, then allocate
	 * the new hash table which overwrites the fields used by the array.
	 */

	old_array = cache->sc_array;
	old_size  = cache->sc_maxlpn + 1;

	scache_alloc_hash(cache);

	/*
	 * Go through the array and move every swap handle into the hash
	 * table.  Notice that we don't worry about growing the hash table
	 * here.  That's because the largest sized array that's every used
	 * can fit within the smallest sized hash table (which is what is
	 * allocated by default).
	 */

	for (lpn = 0; lpn < old_size; lpn++) {
		if (old_array[lpn] != NULL) {

			/*
			 * If we don't have a block from the last time
			 * through or this lpn belongs in a different block, 
			 * then allocate a new block now.
			 */
			 
			if (!block || !LPN_IN_BLOCK(block, lpn))
				block = scache_new_block(cache, lpn);

			index = LPN_TO_BLOCK_INDEX(lpn);
			block->hb_sh[index] = old_array[lpn];
			block->hb_count++;

		}
	}

	/*
	 * Everthing has been copied from the old array at this point, so
	 * free it up now.
	 */

	kmem_arena_free(scache_pool_arena, old_array,
			old_size * sizeof(sm_swaphandle_t));
}


/*
 * Find the hashblock containing the given lpn and return a pointer to
 * it.  Return NULL if no hashblock for the page exists.
 */

static hashblock_t *
scache_find_block(scache_t *cache, pgno_t lpn)
{
	hashblock_t	*block;

	block = SHASH(cache, lpn);

	while (block) {
		if (LPN_IN_BLOCK(block, lpn))
			return block;

		block = block->hb_next;
	}

	return NULL;
}


/*
 * Add a new hashblock to the cache which will contain the given lpn.
 * A block for this page must not already exist.  The swap handles in the
 * block are all initialized to zero and a pointer to the block is returned.
 */


static hashblock_t *
scache_new_block(scache_t *cache, pgno_t lpn)
{
	hashblock_t	*block;

	cache->sc_count++;	/* adding one new block to hash table */

	/*
	 * If adding this new block will cause the average chain length
	 * to exceed our desired maximum, then add more hash buckets to
	 * the hash table.
	 */

	if (cache->sc_count > (cache->sc_size * BLOCKS_PER_CHAIN))
		scache_grow_hash(cache);

#if OLD
	if ((block = (hashblock_t *)kmem_zalloc(sizeof(hashblock_t),
					        KM_NOSLEEP)) == NULL) {

		/*
		 * Get memory from our private pool.  This can never fail.
		 */

		block = (hashblock_t *) kmem_zalloc_alternate(scache_pool,
							sizeof(hashblock_t));
		ASSERT(block != NULL);
	}
#endif

	if ((block = (hashblock_t *) kmem_arena_zalloc(scache_pool_arena,
							sizeof(hashblock_t), 
							KM_NOSLEEP)) == NULL)
		cmn_err_tag(93,CE_PANIC, "scache_new_block out of memory");

	block->hb_lpn   = LPN_TO_BLOCK_START(lpn);
	block->hb_count = 0;
	block->hb_next  = SHASH(cache, lpn);
	SHASH_SET_BUCKET(cache, lpn, block);

	return block;
}


/*
 * Remove a given block from the hash table because it is now empty.
 */

static void
scache_remove_block(scache_t *cache, hashblock_t *block)
{
	hashblock_t **chainp, *bp;
#if DEBUG
	int i;

	ASSERT(block->hb_count == 0);

	/*
	 * Make sure all the swap handles really are zero.  If they aren't
	 * then there's a bug and we'd be leaking swap space.
	 */

	for (i = 0; i < HANDLES_PER_BLOCK; i++) {
		ASSERT(block->hb_sh[i] == NULL);
	}
#endif /* DEBUG */

	/*
	 * Search the hash chain to find the given block.  When found,
	 * unlink it and free its memory.
	 */

	chainp = SHASH_HEAD_ADDR(cache, block->hb_lpn);

	while (bp = *chainp) {
		if (bp == block) {
			*chainp = block->hb_next;
			kmem_arena_free(scache_pool_arena, block,
					sizeof(hashblock_t));
			cache->sc_count--;	/* one less in cache now */
			return;
		}

		chainp = &bp->hb_next;
	}

	cmn_err(CE_PANIC,
		"scache_remove_block: couldn't find block 0x%x, cache 0x%x",
		block, cache);
}


/*
 * Add more buckets to the hash table so that the average chain length
 * will be below the desired maximum.  We grow the number of buckets in
 * the increments given by the hash_sizes array.
 */

static void 
scache_grow_hash(scache_t *cache)
{
	int new_size, old_size, i;
	hashblock_t	*block, *next_block, **old_hash, **new_hash;

	new_size = scache_select_hash_size(cache->sc_count);

	new_hash = (hashblock_t **) kmem_arena_zalloc(scache_pool_arena,
					new_size * sizeof(hashblock_t **),
					KM_NOSLEEP);

	/*
	 * If there's no memory, then just leave the hash table at its current
	 * size and we'll try to grow it next time.  This just means the
	 * chains get a bit longer for awhile.
	 */
	
	if (new_hash == NULL) {
		cmn_err(CE_WARN,
			"scache_grow_hash: scache pool arena out of memory");
		return;
	}

	old_hash = cache->sc_hash;
	old_size = cache->sc_size;

	cache->sc_hash = new_hash;
	cache->sc_size = new_size;

	/*
	 * Re-hash all the blocks into the new hash table.
	 */

	for (i = 0; i < old_size; i++) {
		if (old_size == 1)
			block = (hashblock_t *) old_hash;
		else
			block = old_hash[i];

		for ( ; block; block = next_block) {
			next_block = block->hb_next;
			block->hb_next = SHASH(cache, block->hb_lpn);
			SHASH_SET_BUCKET(cache, block->hb_lpn, block);
		}
	}

	/*
	 * If we had dynamically allocated the hash buckets for the old
	 * hash table, then free it up now.  Otherwise we were using the
	 * sc_list pointer in the scache_t structure directly.
	 */

	if (old_size > 1)
		kmem_arena_free(scache_pool_arena, old_hash,
				old_size * sizeof(hashblock_t **));
}


/*
 * Choose the number of buckets for the hash table based on the number of
 * hash blocks it is currently holding.  This size is rounded up to the
 * next larger size in the hash_sizes[] table.
 */

static int
scache_select_hash_size(int block_count)
{
	int i, num_buckets;

	/*
	 * We want enough buckets so that there are no more than
	 * BLOCKS_PER_CHAIN blocks in each bucket.
	 */

	num_buckets = (block_count + BLOCKS_PER_CHAIN - 1) / BLOCKS_PER_CHAIN;

	/*
	 * Limit the number of buckets to the maximum that will fit in
	 * a single page.
	 */

	if ((num_buckets * sizeof(hashblock_t *)) > NBPP)
		return NBPP / sizeof(hashblock_t *);

	for (i = 0; num_buckets > hash_sizes[i]; i++) {

		/*
		 * Defensive programming here.  The table should be 
		 * configured so that we never walk off the end.  But if
		 * the assumptions made here change some day and someone
		 * forgets to update the table...  So stop at the last entry.
		 */

		if (i == (sizeof(hash_sizes) / sizeof(int) - 1))
			break;
	}

	return hash_sizes[i];
}


/*
 * The following routines maintain the private pool of pages from
 * which the above routines allocate the scache data structures.
 *
 * Scache structures are allocated even when the global memory pool
 * has no memory. For this reason, the scache module cannot use kmem_alloc
 * and kmem_free interfaces. Instead, the scache module maintains its own
 * list of free pages and replenishes the list when the number of pages in 
 * the pool runs low.  The scache module ensures that it does not allocate
 * more than a page of memory while trying to record the swap handle for any
 * one page.  This ensures that the scache_pool never runs out of memory.
 *
 * Since the scache structures are of arbitrary sizes, we need a generic
 * allocation mechanism. This modules makes use of the private arena
 * facility provided by the kernel memory allocator. A private arena is
 * set up for managing scache structures. This arena is used to allocate
 * memory for scache structures using the kmem_arena_alloc interfaces.
 * When kmem_arena_alloc wants to allocate memory it calls the arena specific
 * page allocation routine (scache_pool_alloc()). This routine allocates
 * memory from the general pool and if there is no memory it uses a page from
 * the local pool. The local pool is  maintained as a list of pages in 
 * scache_pool. Vhand or other swap daemons can call into this pool to 
 * replenish this pool by call scache_pool_add() with a free page. 
 */


scache_pool_t	scache_pool;

static void *scache_pool_alloc(arena_t *, size_t, int, int);
static void scache_pool_free(void *, uint, int);
uint_t scachemem;
extern int scache_pool_size;	/* tuneable parameter, the number of Kbytes */
				/* of memory to initially be placed in the  */
				/* scache pool				    */
int	scache_pool_min_pages;  /* value of scache_pool_size converted	    */
				/* to pages				    */

/*
 * Initialize the scache pool arena and add the pages to the pool.
 */

static void
scache_pool_init()
{

	scache_pool_arena = kmem_arena_create(scache_pool_alloc,
					scache_pool_free);

	scache_pool_min_pages = (scache_pool_size * 1024 + NBPP - 1) / NBPP;

	scache_pool.swap_reserve_page_list = NULL;
	scache_pool.num_reserved_pages     = 0;
	scache_pool.num_pages_needed	   = scache_pool_min_pages;

	spinlock_init(&scache_pool.scache_pool_lock, "scache pool");
	scache_pool_reserve();
}


/*
 * Return true if the pool needs more pages.
 */

int
scache_pool_pages_needed()
{
	if (scache_pool.num_reserved_pages < scache_pool.num_pages_needed)
		return 1;

	else
		return 0;
}


/*
 * Stoke the scache_pool til it's full.
 */

void
scache_pool_reserve()
{
	pfd_t *pfd;

	while (scache_pool.num_reserved_pages < scache_pool.num_pages_needed) {
		pfd = pagealloc(0, VM_UNSEQ);
		
		if (pfd == NULL) {
			setsxbrk();
			continue;
		}

		scache_pool_add(pfd);
	}
}


/*
 * Add a page to the pool.
 */
void
scache_pool_add(pfd_t *pfd)
{
	int	spl;

	ASSERT(pfd->pf_use == 1);
	ASSERT((pfd->pf_flags &
		(P_HASH|P_DONE|P_ANON|P_QUEUE|P_SQUEUE|P_RECYCLE|P_CW)) == 0);

	/* these pages are not swappable & are freed by kvpfree  */
	kreservemem(GLOBAL_POOL, 1, 1, 1);
	spl = SCACHE_POOL_LOCK();
	pfd->pf_next = scache_pool.swap_reserve_page_list;
	scache_pool.swap_reserve_page_list = pfd;
	scache_pool.num_reserved_pages++;
	SCACHE_POOL_UNLOCK(spl);
	atomicAddUint(&scachemem, 1);
}


/*
 * Arena allocation routine. This routine is called from amalloc() if it
 * needs a new page.  If things go wrong, we return NULL and let the
 * caller deal with it.
 */

/* ARGSUSED */
static void *
scache_pool_alloc(arena_t *ap, size_t size, int flags, int color)
{
	caddr_t	vaddr;
	pfd_t	*pfd;
	int	spl;

	/*
	 * It's an error to allocate more than a page.
	 */

	if (size > 1)
		return NULL;

	spl = SCACHE_POOL_LOCK();

	/*
	 * If the pool ran out of memory, return NULL.  This is an error.
	 */

	if (scache_pool.num_reserved_pages == 0) {
		SCACHE_POOL_UNLOCK(spl);
		return NULL;
	}

	pfd = scache_pool.swap_reserve_page_list;
	scache_pool.swap_reserve_page_list = pfd->pf_next;
	scache_pool.num_reserved_pages--;
	SCACHE_POOL_UNLOCK(spl);

	pfd->pf_flags |= P_DUMP;
	if (flags & VM_BULKDATA)
	    pfd->pf_flags |= P_BULKDATA;

	TILE_PAGECONDLOCK(pfd, 1);

	vaddr = page_mapin(pfd, flags, color);

	return vaddr;
}

static void
scache_pool_free(void *vaddr, uint size, int flags)
{
	ASSERT(scachemem > 0);
	atomicAddUint(&scachemem, -size);
	kvpffree(vaddr, size, flags);
}
