/*
 * os/pcache.c
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

#ident	"$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/pfdat.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include "pcache.h"
#include "pcache_mgr.h"


/*
 *	Low Level Page Cache Routines.
 *
 *	This package of routines implements the cache of in-core pages
 * 	associated with each memory object.  An open hash table is used for
 *	its space and time efficiency, as well as the simplicity of the
 *	code.  This layer of code is fairly low level and has no 
 *	knowledge of the memory objects themselves.   It simply caches
 *	pages in a given page cache structure that the higher level
 *	code associates with the memory object.  For performance, this
 *	layer knows it is caching pfdats (as opposed to some opaque
 *	object).  This means the code here uses pfdat fields to implement
 *	the cache.  It relies on only two fields: the page number within
 *	the object which is the page's cache id, and the hash chain field
 *	which is used to construct the linked list of pfdats in each hash
 *	bucket.  The only space allocated by this layer is the hash bucket
 *	headers.
 *
 *	This module allows replicas of pages to exist in the cache.  It
 *	is up to the caller to implement the policy of creating and destroying
 *	replicas.  This layer merely allows multiple pfdats with the same
 *	page number to exist in the cache.
 *
 *	The upper level memory object layer is allocates one pcache_t
 *	structure for each memory object.  This is passed into the page
 *	page cache routines to identify the cache being operated on.
 *
 *	No MP locking is done by this level at all.  The caller is
 *	responsible for using appropriate locks to ensure page cache
 *	updates are atomic for the respective memory object.  Multiple
 *	readers are allowed in the page cache lookup routine if the
 *	memory object chooses to allow it.
 *
 *	The number of hash buckets used for a particular object is chosen
 *	based on the number of pages being cached.  The routines here try
 *	to limit the number of pfdats in each bucket to PAGE_PER_CHAIN.  
 *	As more pages are cached, the page cache for an object is re-sized
 *	automatically.  The hash table is grown by allocating a larger array of 
 *	bucket headers and then simply copying the old bucket array over
 *	to the one.  To avoid the expense of re-hashing all the pages
 *	(which can be extremely time consuming for large memory objects)
 *	a lazy re-hashing scheme is used.  No pages are re-hashed when
 * 	the new bucket array is allocated.  Instead, pages are slowly
 *	re-hashed during subusequent search operations.  Since pages are
 *	not immediately re-hashed, this means the search function may have
 *	to examine multiple chains in order to find a page.  The chains
 *	it examines are the ones corresponding to the pages hash index for
 *	the previous table size.  To make things simplier and to allow for
 *	multiple re-sizings to occur, the end of a hash chain is marked
 *	with a "link" value which is the hash array index of the chain that
 *	must be searched for the old hash table size.
 *
 *	As an optimization for small memory objects (those with less than
 * 	or equal to PAGE_PER_CHAIN pages), a simple linked list of pfdats
 *	is used instead.  This code treats this as a hash table with one
 *	bucket, but instead of dynamically allocating space for the single
 *	entry bucket array, the pointer in the pcache structure is used
 *	directly.  The hashing function (PHASH) largely hides this detail.
 */	


/*
 *	Prototypes for internal functions.
 */

static int     pcache_select_hash_size(int);
static pfd_t **pcache_search(pcache_t *, pgno_t, pfd_t **);


/*
 * Initialize a page cache.  We start off by running with one bucket until
 * we find the cache needs to be bigger.
 */

void
pcache_init(pcache_t *cache)
{
	cache->pc_size  = 1;
	cache->pc_list  = NULL;
	cache->pc_count = 0;
}


/* 
 * Pre-allocate space to expand the hash table if needed.  Returns NULL if
 * extra space not needed or no memory is currently available.  Otherwise,
 * it returns a block of memory for pcache_resize to use.
 */

void *
pcache_getspace(pcache_t *cache, int extra_pages)
{
	pfd_t	**new_buckets;
	int	  i, new_size, link_value;

	/*
	 * Do nothing if the hash table is already big enough.
	 */

	if (MAX_CAPACITY(cache) >= cache->pc_count + extra_pages)
		return NULL;

	new_size    = pcache_select_hash_size(cache->pc_count + extra_pages);
	new_buckets = (pfd_t **)kmem_alloc(new_size * sizeof(pfd_t *),
						    KM_NOSLEEP);

	/*
	 * If we couldn't get the expanded bucket space we wanted, then
	 * just keep using the hash table we already got.  This means that
	 * the chains will get longer and things will slow down a bit, but
	 * that's better than causing a memory deadlock by sleeping here.
	 * We'll make another attempt to grow the hash table when the next
	 * page is inserted.
	 *
	 * If we did get the space, then initialize it now so we don't have
	 * to waste time doing it later while we're holding the lock.
	 */

	if (new_buckets != NULL) {

		/*
		 * Record the size we allocated at the front of the space
		 * so pcache_resize will know how big it is.
		 */

		*(int *)new_buckets = new_size;

		/*
		 * Initialize all the new buckets with links to their old
		 * hash locations.  This is easy since the hashing function
		 * uses the modulo function and the table size is always a
		 * power of 2.  So walk through all the new buckets and link
		 * them back to where the old hash size would have put the
		 * pages.  The link values for sequential buckets are simply
		 * sequential indices that are modulo the old hash table size.
		 *
		 * The old buckets will be copied over in pcache_resize().
		 */
	
		link_value = 0;
	
		for (i = cache->pc_size; i < new_size; i++) {
			new_buckets[i] = MK_LINK(link_value);
	
			if (++link_value >= cache->pc_size)
				link_value = 0;
		}
	}

	return (void *) new_buckets;
}

/*
 * Re-size the hash table for a page cache using the new bucket array
 * passed in by the caller.  This array was previously allocated by
 * pcache_getspace.  If we already have a hash table that's as big as
 * the one passed in or bigger, then we leave the old one alone and
 * free up the new one.  If the new one is bigger, then we move
 * all the pages to the new hash table.  The actual freeing of the old
 * bucket space is deferred until pcache_resize_epilogue() so that we
 * don't hold the cache lock any longer than necessary.
 */

void *
pcache_resize(pcache_t *cache, void *new_space)
{
	pcache_t	new_cache;
	int		new_size;
	void		*old_space;
#if REHASH
	pfd_t		*pfd, *next_pfd, **headp;
#endif

	ASSERT(new_space);

	/*
	 * pcache_getspace stored the size of the new hash table in the
	 * beginning of the memory space.  If that's smaller than what
	 * we've already got, then free up the new space and return.
	 */

	new_size = *(int *) new_space;
	ASSERT(new_size > 0);

	if (new_size <= cache->pc_size)
		return new_space;

	/*
	 * Set up a new cache structure into which we'll copy all the pages
	 * currently in the old cache.
	 */

	new_cache.pc_size    = new_size;
	new_cache.pc_buckets = (pfd_t **) new_space;

#if REHASH
	/* XXX
	 * XXX This is the old aggressive rehashing algorithm.  Some
	 * XXX basic performance tests show that this isn't worthwhile
	 * XXX even for very small hash tables.  The lazy re-hash algorithm
	 * XXX always works better overall.  I'll leave the code here just
	 * XXX we find a use for it some day.
	 */

	bzero(new_space, new_size * sizeof(pfd_t *));

	/*
	 * Walk through each bucket in the old hash table, re-hashing
	 * each page to its new location in the new hash table.
	 */

	for (i = 0; i < cache->pc_size; i++)
		for (pfd = PHASH(cache, i); pfd; pfd = next_pfd) {
			next_pfd = pfd->pf_hchain;
			headp = PHASH_HEAD_ADDR(&new_cache, pfd->pf_pageno);
			pfd->pf_hchain = *headp;
			*headp = pfd;
		}
#endif

	/*
	 * Move the pages to the new hash table using the lazy re-hashing
	 * algorithm.  To do this, we simply copy over the old bucket pointers
	 * and initialize the remaining bucket pointers in the new table
	 * with links to where the corresponding pages can be found.
	 * There is no need to examine each pfdat.
	 *
	 * Step 1: Copy the old bucket pointers into the new array.
	 */

	bcopy(PHASH_HEAD_ADDR(cache, 0), new_cache.pc_buckets,
	      cache->pc_size * sizeof(pfd_t *));


#if DEBUG
{

	int i, link_value;

	/*
	 * Verify that no one messed with the initialization that
	 * pcache_getspace performed on the new buckets.
	 */

	link_value = 0;

	for (i = cache->pc_size; i < new_cache.pc_size; i++) {
		ASSERT(new_cache.pc_buckets[i] == MK_LINK(link_value));

		if (++link_value >= cache->pc_size)
			link_value = 0;
	}

}
#endif

	/*
	 * If we had more than one bucket in the old table, then the bucket
	 * list was dynamically allocated.  So setup the old_space so that
	 * pcache_resize_epilogue() can free it.
	 */

	if (cache->pc_size > 1) {
		old_space = (void *) cache->pc_buckets;
		*(int *)old_space = cache->pc_size;

	} else
		old_space = NULL;

	/*
	 * Copy the new hash table parameters back to the old cache structure.
	 * The page count in the table (pc_count) hasn't changed, so we don't
	 * need to copy it.
	 */

	cache->pc_size    = new_cache.pc_size;
	cache->pc_buckets = new_cache.pc_buckets;

	return old_space;
}


/*
 * Free up the old buckets space that pcache_resize replaced.
 */

void
pcache_resize_epilogue(void *old_space)
{
	int old_size;

	/*
	 * If the hash table was only a single bucket, then there was no
	 * bucket space allocated (since it used the pointer in the pcache_t
	 * structure).  So there's nothing to free in this case.
	 */

	if (old_space == NULL)
		return;

	/*
	 * pcache_resize stored the size of the old hash table in the
	 * beginning of the memory space.
	 */

	old_size = *(int *) old_space;
	ASSERT(old_size > 0);

	kmem_free(old_space, old_size * sizeof(pfd_t *));
}


/* 
 * Search the hash table for the given page number.  Return the pfdat
 * if found.
 */

pfd_t *
pcache_find(pcache_t *cache, pgno_t pageno)
{
	pfd_t **pfdp;

	pfdp = pcache_search(cache, pageno, PHASH_HEAD_ADDR(cache, pageno));

	if (pfdp == NULL)
		return NULL;

	return *pfdp;
}


/*
 * Return the next replica of a given page.  Starting at cur_pfd, we look
 * on the same chain for another page with the same page number and return
 * the first one we find.  Repetitive calls to the pcache_next return
 * subsequent replicas.  This code assumes that the caller is holding a lock
 * so that no new replicas are inserted inbetween pcache_next calls. 
 */

pfd_t *
pcache_next(pcache_t *cache, pfd_t *cur_pfd)
{
	pfd_t	**pfdp;

	/* Since we know all the replicas for a given page number will be
	 * on the same chain, there's no need to do the whole hash lookup.
	 * Just start are cur_pfd and walk down the hash chain.
	 */

	pfdp = pcache_search(cache, cur_pfd->pf_pageno, &cur_pfd->pf_hchain);

	if (pfdp == NULL)
		return NULL;

	return *pfdp;
}


/*
 * General hash table search function that handles lazy re-hashing of pages.
 * The hash table is searched for a page matching the pageno argument starting
 * from where chainp points.  If a the end of a bucket is reached and we
 * find a link pointer, then we follow the link to the indicated bucket
 * and continue the search.  If we encounter a page that hasn't yet been
 * re-hashed after the hash table was expanded, then we re-hash it now.
 * We stop as soon as we find the page we're looking for.  A pointer to
 * the pointer for this pfdat is returned in case the caller wishes to
 * unlink it.
 */

static pfd_t **
pcache_search(pcache_t *cache, pgno_t pageno, pfd_t **chainp)
{
	pfd_t	*pfd, **linkp[MAX_RESIZES], **headp;
	int	 chain_index, link_index, i;

	/*
	 * We remember the location where we read link pointers from.
	 * This lets us go back and clear the link pointers once we've re-hashed
	 * all the pages from their old locations.  The maximum number
	 * re-sizes is fixed to a small number, so we can easily keep a
	 * small array of the link pointer locations.  We fill the array
	 * starting at index 0.
	 */

	link_index = 0;

	/*
	 * chain_index is the hash table index of the bucket chain we're
	 * currently seaching.  It's used to spot pages hashed to wrong chain.
	 */

	chain_index = HASH_INDEX(cache, pageno);

	/*
	 * Search the hash bucket chain until we reach its end.  Note that
	 * we may follow a link pointer and end up searching other chains
	 * as well.  This loop continues until we find the page we want or
	 * we hit the end of a chain (and it doesn't have a link pointer).
	 */

	while (pfd = *chainp) {

		/*
		 * If we hit a link pointer, then we must continue the search
		 * on the chain that it points to.  We remember the location
		 * of this link pointer in linkp and compute the new chain
		 * location and index.
		 */

		if (IS_LINK_INDEX(pfd)) {
			ASSERT(link_index < MAX_RESIZES);

			linkp[link_index++] = chainp;
			chain_index	    = LINK_INDEX(pfd);
			chainp		    = GET_HEAD_ADDR(cache, chain_index);
			continue;
		}

		/*
		 * Re-hash this page if it's on the wrong chain.
		 */

		if (HASH_INDEX(cache, pfd->pf_pageno) != chain_index) {
			*chainp        = pfd->pf_hchain;
			headp          = PHASH_HEAD_ADDR(cache, pfd->pf_pageno);
			pfd->pf_hchain = *headp;
			*headp         = pfd;

			/*
			 * If we just moved the location of a link index,
			 * then check to see if that was a link that we
			 * followed, and if so, update the linkp array
			 * to reflect it's new location.
			 */

			if (IS_LINK_INDEX(pfd->pf_hchain))
				for (i = 0; i < link_index; i++)
					if (linkp[i] == headp)
						linkp[i] = &pfd->pf_hchain;

			/*
			 * Stop now if this was the page we were looking for.
			 */

			if (pfd->pf_pageno == pageno)
				return headp;

			continue;
		}

		/*
		 * Stop now if this was the page we were looking for.
		 */

		if (pfd->pf_pageno == pageno)
			return chainp;

		chainp = &pfd->pf_hchain;
	}

	/*
	 * At this point, we completely seached to the end of a chain,
	 * including all chains we traversed by following link pointers,
	 * and we didn't find the page.  Since we re-hashed all the pages
	 * as we went through, we now know all pages in these chains have
	 * been re-hashed to their correct locations.  So it isn't necessary
	 * to follow the links anymore on subsequent searches.  So we can
	 * clear out the link pointers now.
	 */

	for (i = 0; i < link_index; i++)
		*linkp[i] = NULL;

	return NULL;
}



/*
 * Add a new pfdat to the hash table.  Duplicates are allowed to support
 * replication.  The pfdat is added to the front of the hash chain for
 * simplicity and to avoid all the cache misses that traversing the chain
 * to the end would cause.
 */

void
pcache_insert(pcache_t *cache, pfd_t *new_pfd)
{
	pfd_t	**chainp;

	chainp = PHASH_HEAD_ADDR(cache, new_pfd->pf_pageno);
	new_pfd->pf_hchain = *chainp;
	*chainp = new_pfd;
	cache->pc_count++;
}


/*
 * Remove pages in the given range from the cache.  Call the specified
 * function for each page found.
 */

void
pcache_remove(pcache_t *cache, pgno_t first, pgno_t count,
	      void (*func)(void *, pfd_t *), void *arg,
	      void (*preempt_func)(void *), void *preempt_cookie)
{
	pfd_t	*pfd, **chainp;
	pgno_t	last;

	/*
	 * If the cache is empty, then there's nothing to do.
	 */

	if (cache->pc_count == 0)
		return;

	last = first + count - 1;

	/*
	 * We want to search the hash table in the most efficient manner
	 * possible.  Since we're using a modulo hashing functions that
	 * puts consecutive pages in consecutive hash buckets.  If
	 * the page count we're scanning through is greater than or equal
	 * to the number of hash buckets, then we'll have to visit each
	 * bucket to find all the pages.  In this case, it's better to
	 * make a linear search of all the buckets looking for pages in
	 * the given range.  If the page count is less than the number
	 * of hash buckets, then we only have to visit a subset of the
	 * buckets and it's better to use the hashing function on each
	 * and just look at those buckets.
	 */

	if (count > cache->pc_size) {
		register int i;

		/*
		 * The linear scan is more efficient.  Walk completely 
		 * through each hash chain looking for pages in the given
		 * range.  Note that each chain may contain several pages
		 * in the range.  We don't bother following link pointers
		 * since we're going to search all the buckets anyway.
		 * We also don't bother re-hashing pages here since this
		 * case is generally used when the memory object is being
		 * destroyed or completely truncated.  Partial truncates
		 * are far less frequent.
		 */

		for (i = 0; i < cache->pc_size; i++) {
rescan_chain:
			chainp = PHASH_HEAD_ADDR(cache, i);

			while ((pfd = *chainp) && !IS_LINK_INDEX(pfd)) {
				if (pfd->pf_pageno >= first &&
				    pfd->pf_pageno <= last) {
					*chainp = pfd->pf_hchain;
					pfd->pf_hchain = NULL;

					if (func)
						(*func)(arg, pfd);

					/*
					 * If that was the last page in the
					 * cache, then we're done.
					 */

					if (--cache->pc_count == 0)
						return;

					/*
					 * Allow a preemption point after each
					 * page.  Once we let go of the lock,
					 * we have to restart from the 
					 * beginning of the chain since it
					 * could have changed.
					 */

					if (preempt_func) {
						(*preempt_func)(preempt_cookie);
						goto rescan_chain;
					}

				} else
					chainp = &pfd->pf_hchain;
			}

			/*
			 * Allow a preemption point after each bucket is
			 * traversed.  Note that since this releases the
			 * lock, all variables need to be taken from the cache
			 * structure again (i.e., we can't use local variables
			 * like chainp or pfd anymore without resetting their
			 * values).
			 */

			if (preempt_func)
				(*preempt_func)(preempt_cookie);
		}

	} else {
		register pgno_t	pageno;

		/*
		 * Looking up each page individually is more efficient.
		 */

		for (pageno = first; pageno <= last; pageno++) {
			chainp = PHASH_HEAD_ADDR(cache, pageno);
		
			while (chainp = pcache_search(cache, pageno, chainp)) {
				pfd = *chainp;
				*chainp = pfd->pf_hchain;
				pfd->pf_hchain = NULL;

				if (func)
					(*func)(arg, pfd);

				/*
				 * If that was the last page in the
				 * cache, then we're done.
				 */

				if (--cache->pc_count == 0)
					return;
			}

			/*
			 * Allow a preemption.
			 */

			if (preempt_func)
				(*preempt_func)(preempt_cookie);
		}
	}
}

/*
 * Remove a specific pfdat from the page cache. 
 */

void
pcache_remove_pfdat(pcache_t *cache, pfd_t *in_pfd)
{
	pfd_t	*pfd, **chainp;
	pgno_t	 pageno;

	ASSERT(cache->pc_count > 0);

	pageno = in_pfd->pf_pageno;
	chainp = PHASH_HEAD_ADDR(cache, pageno);

	while (chainp = pcache_search(cache, pageno, chainp)) {
		pfd = *chainp;

		if (pfd == in_pfd) {
			*chainp = pfd->pf_hchain;
			pfd->pf_hchain = NULL;
			--cache->pc_count;
			return;
		}

		chainp = &pfd->pf_hchain;
	}

	cmn_err_tag(91,CE_PANIC, "pcache_remove_pfdat couldn't find pfd 0x%x", in_pfd);
}

/*
 * Deallocate the hash table if there is one.  The page cache is
 * assumed to be empty at this point.
 */

void
pcache_release(pcache_t *cache)
{
	if (cache->pc_count != 0)
		cmn_err(CE_PANIC,
		"pcache_release: page cache 0x%x not empty, contains %d pages",
		cache, cache->pc_count);

	if (cache->pc_size > 1) {

#if DEBUG
		int i;

		/*
		 * Make doubly sure the cache is empty just in the case the
		 * count is messed up.
		 */

		for (i = 0; i < cache->pc_size; i++) {
			ASSERT(cache->pc_buckets[i] == NULL ||
			       IS_LINK_INDEX(cache->pc_buckets[i]));
		}
#endif

		kmem_free(cache->pc_buckets, cache->pc_size * sizeof(pfd_t *));

		/*
		 * Switch back into linked list mode.
		 */

		cache->pc_list    = NULL;
		cache->pc_size    = 1;

	} else {
		ASSERT(cache->pc_list == NULL);
	}

}


/*
 * Choose the number of buckets for the hash table based on the number
 * of pages it is currently holding.  The table must be a power of 2
 * plus we don't want to grow the table too often.  So the size is size is
 * rounded up to the next larger size that is a power of 2 and a factor
 * of GROW_INTERVAL.
 */

static int
pcache_select_hash_size(int page_count)
{
	int num_buckets, size;

	/*
	 * We want enough buckets so that there are no more than
	 * PAGES_PER_CHAIN pages in each bucket.
	 */

	num_buckets = (page_count + PAGES_PER_CHAIN - 1) / PAGES_PER_CHAIN;

	for (size = 1; size < num_buckets; size *= GROW_INTERVAL)
		;

	return size;
}

#if DEBUG

int
pcache_check_for_links(pcache_t *cache)
{
	int i;
	pfd_t *pfd;

	for (i = 0; i < cache->pc_size; i++) {
		pfd = PHASH(cache, i);

		while (pfd) {
			if (IS_LINK_INDEX(pfd))
				return 1;

			pfd = pfd->pf_hchain;
		}
	}

	return 0;
}
#endif
