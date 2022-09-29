#ifndef __SYS_PCACHE_MGR_H__
#define __SYS_PCACHE_MGR_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sys/pcache_mgr.h
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

#ident	"$Revision: 1.2 $"

#include <sys/types.h>

/*
 * This header file contains internal definitions for the page cache
 * manager.  It is to be included and used *only* by the page cache
 * manager code and by kernel debuggers.  All other users are restricted
 * to the definitions provided in sys/pcache.h.
 */


/*
 *	Convenience macros for accessing the union in the pcache_t struct.
 */

#define pc_list		pc_un.pc_un_list
#define pc_buckets	pc_un.pc_un_hash

/*
 *	We want to keep the maximum number of pfdats in a hash chain
 *	short since each access will most likely result in a cache miss.
 */

#define PAGES_PER_CHAIN		8

/*
 *	Based on the above, this macro returns the maximum number of
 *	pages we'd like to have in the given cache before we'd want to
 *	grow it.
 */

#define MAX_CAPACITY(cache)	((cache)->pc_size * PAGES_PER_CHAIN)

/*
 *	The GROW_INTERVAL value specifies how much the hash table grows
 *	at a time.  The number of buckets in the hash table must
 *	be a power of 2 so that the hashing algorithm can do a simple modulo
 *	function using and'ing.  We also want reasonable increments between
 *	each size so that we don't do too many grow operations (which
 *	require re-hashing all entries).  For now, we'll grow the caches
 *	by factors of four.  This produces hash tables of the following
 * 	sizes and capacities:
 *
 *			    	         	      Total Mem         
 *	  Table Size	    Table Size	              ---------		
 *	 (in buckets)     (in bytes)	      4K Pages	      16K Pages	
 *			    (32/64 bit)					
 *	  ----------	    ---------- 	      --------	      --------- 
 *	 	1,	        0/  0		 32K		128K	
 *		4,	       16/  32		128K		512K
 *	 	8,	       32/  64		256K		  1M	
 *	       32,	      128/ 256		  1M		  4M	
 *	      128,	      512/  1K		  4M		 16M	
 *	      512,	       2K/  4K		 16M		 64M	
 *	     2*KB,	       8K/ 16K		 64M		256M	
 *	     8*KB,	      32K/ 64K		256M		  1G	
 *	    32*KB,	     128K/256K		  1G		  4G	
 *	   128*KB,	     512K/  1M		  4G		 16G	
 *	   512*KB,	       2M/  4M		  --		 64G	
 *	     2*MB,	       8M/ 16M		  --		256G	
 *	     8*MB,	      32M/ 64M		  --		  1T	
 */


#define GROW_INTERVAL	4


/*
 * The maximum number of times a hash table could be re-sized.  This is
 * the number of rows in the table above minus 1.  This value tells us
 * the maximum number of links we might traverse during any one page
 * search operations.
 */

#define MAX_RESIZES	12

/*
 *	The hash function varies depending on the number of buckets in
 *	the table.  When the cache is operating as a linked list (i.e.,
 *	when there's only a single bucket) a pointer to the list is returned.
 *	When there's a true hash table, a simple modulo function is used.
 *	The number of buckets in the cache must be a power of 2 for this
 *	to work.  The cache is accessed based on the page number being
 *	entered or searched for.
 *
 *	PHASH		returns the first element in the chain.
 *
 *	PHASH_HEAD_ADDR	returns a pointer to the bucket entry for the chain.
 *			Used when one wants to insert or delete entries.
 *
 *	GET_HEAD_ADDR	like PHASH_HEAD_ADDR(), but takes a hash table index
 *			instead of a page number.
 *
 *	HASH_INDEX	returns the hash table index for the given page number.
 */

#define PHASH(cache, pageno)  \
		((cache)->pc_size == 1 ? \
			(cache)->pc_list : \
			(cache)->pc_buckets[(pageno) & ((cache)->pc_size-1)])


#define PHASH_HEAD_ADDR(cache, pageno)  \
		((cache)->pc_size == 1 ? \
			&(cache)->pc_list : \
			&(cache)->pc_buckets[(pageno) & ((cache)->pc_size-1)])

#define GET_HEAD_ADDR(cache, index) \
		((cache)->pc_size == 1 ? \
			&(cache)->pc_list : \
			&(cache)->pc_buckets[index])

#define HASH_INDEX(cache, pageno)	((pageno) & ((cache)->pc_size-1))


/*
 *	To save space, the link pointers are stored in the hash chain link
 *	fields of the pfdats.  Links are only found in the last pfdat in
 *	a chain or in the bucket header itself.  The link value is the
 *	hash table index itself of the next bucket to search, and so has
 *	a range of values from 0 to pc_size-1.  We can distinguish links
 *	from pfdat pointers since pointers are always kernel addresses and
 *	are hence large unsigned numbers while links are relatively small
 *	integers.  A value of zero in a hash chain link field still means
 *	the NULL pointer or end of chain.  Since 0 is also a legal link
 *	value, we encode link values by adding 1 when we store them and
 *	subtracting one when we retrieve them.  This is hidden by these
 *	macros:
 */

#define MK_LINK(link)		((struct pfdat *)((__psint_t)(link) + 1))
#define LINK_INDEX(ptr)		((__psint_t)(ptr) - 1)

#define MAX_BUCKETS		(16*1024*1024)	/* 16 million */

#define IS_LINK_INDEX(ptr)	(((__psunsigned_t)ptr) < MAX_BUCKETS)

#ifdef __cplusplus
}
#endif

#endif /* __SYS_PCACHE_MGR_H__ */
