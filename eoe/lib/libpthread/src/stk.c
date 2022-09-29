/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "common.h"
#include "asm.h"
#include "pt.h"
#include "vp.h"
#include "stk.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/types.h>


#define SCALAR			__psunsigned_t
#define	ALIGN_UP_N(addr, size) \
		(((SCALAR)(addr) + (size) - 1) & ~((size) - 1))
#define ALIGN_UP(addr)		ALIGN_UP_N(addr, 8)
#define ALIGN_DOWN(addr, size)	(((SCALAR)(addr) + size) & ~0x7)


/* Caching:
 * Stacks below a certain size are cached, stacks above
 * the maximum are not cached.
 * Cached stacks are of fixed sizes belonging to bucket free lists.
 *
 * Every allocated stack has a reserved save area at its high end whose
 * contents depends on how the stack was allocated:
 *	cached stacks record the bucket index
 *	user allocated stacks record a special bucket index
 *	uncacheable stacks record the stack low address and the size
 *
 * Additionally, the size of the stack guard is stored in the save
 * area (note that this location has no meaning for user allocated stacks)
 *
 * When freed the save area is the link pointer in bucket list.
 */

/* Max bucket size is
 *	STK_LAST_BUCKET * BUCKETSIZE
 *
 * We burn the 0th entry to make the computation is quicker.
 * Beware making this too large - there is no uncaching.
 */
#define BUCKETSIZE		0x4000
#define STK_LAST_BUCKET		20
#define STK_NO_BUCKET		(STK_LAST_BUCKET + 1)
static	QS_DECLARE(stk_buckets)[STK_LAST_BUCKET + 1];

#define STK_FIND_BUCKET(bytes)	(((bytes) + BUCKETSIZE - 1) / BUCKETSIZE)
#define STK_BUCKET_SIZE(bucket)	(bucket * BUCKETSIZE)

#define STK_RESERVE(addr)	(void *)((SCALAR)(addr)-(sizeof(long long)<<1))

#define STK_SAVE(addr)		(*((__psunsigned_t *)(addr)))
#define STK_SAVE2(addr)		(*(((__psunsigned_t *)(addr))+1))
#define	STK_GUARDSIZE(addr)	STK_SAVE2(addr)

#define STK_BASE_TO_START(base, size) \
			STK_RESERVE((SCALAR)(base) + (size))
#define STK_START_TO_BASE(start, size) \
		(void *)(((SCALAR)(start) - (size)) + (sizeof(long long)<<1))


/*
 * stk_alloc(addr, size, guardsize)
 *
 * Return the properly aligned stack that a thread should use.
 */
void *
stk_alloc(void *stk_addr, size_t stk_size, size_t stk_guardsize)
{
	void		*stk_base;
	void		*stk_start;
	__psunsigned_t	bucket;
	size_t		alloc_size;
	int		dev_zero;
	size_t		cur_guardsize;
	extern int	pt_page_size;
	extern int	_open(const char *, int, ...);
	extern int	_close(int);
	extern void *	_valloc(size_t);

	TRACE(T_PT, ("stk_alloc(%#x, %#x, %#x)",
		stk_addr, stk_size, stk_guardsize));

	if (stk_addr) {
		/*
		 * A user stack - align and reserve space for the
		 * bucket "index".
		 */
		stk_start = STK_RESERVE(ALIGN_DOWN(stk_addr, stk_size));
		STK_SAVE(stk_start) = STK_NO_BUCKET;
		return (stk_start);
	}

	if (stk_guardsize > 0) {
		stk_guardsize = ALIGN_UP_N(stk_guardsize, pt_page_size);
		stk_size += stk_guardsize;
	}

	cur_guardsize = 0;
	if ((bucket = STK_FIND_BUCKET(stk_size)) > STK_LAST_BUCKET) {
		/*
		 * An uncacheable stack - allocate and reserve space to
		 * save the base address (for deallocation).
		 */
		stk_size = ALIGN_UP(stk_size);
		if ((dev_zero = _open("/dev/zero", O_RDWR)) == -1) {
			panic("stk_alloc()", "couldn't open /dev/zero");
		}
		if ((stk_base =
			mmap(0, stk_size, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE, dev_zero, 0)) == MAP_FAILED) {
			(void)_close(dev_zero);
			return (0);
		}
		(void)_close(dev_zero);

		stk_start = STK_BASE_TO_START(stk_base, stk_size);
		STK_SAVE(stk_start) = (SCALAR)stk_size;
	} else {
		/*
		 * A cacheable stack - check the cache or allocate and
		 * reserve space to save the bucket index.
		 */
		sched_enter();
		QS_REM_FIRST(&stk_buckets[bucket], stk_start, void *);
		sched_leave();
		if (!stk_start) {
			alloc_size = STK_BUCKET_SIZE(bucket);
			/*
			 * In order to support stack guards, use
			 * valloc to get a page-aligned stack base.
			 */
			if (!(stk_base = _valloc(alloc_size))) {
				return (0);
			}
			stk_start = STK_BASE_TO_START(stk_base, alloc_size);
		} else {
			cur_guardsize = STK_GUARDSIZE(stk_start);
			stk_base = STK_START_TO_BASE(stk_start,
				STK_BUCKET_SIZE(bucket));
		}
		STK_SAVE(stk_start) = bucket;
	}

	/*
	 * Set protections on stack guard.  If we are using a cached
	 * stack, then only change the protections on the pages that
	 * need to change.  'cur_guardsize' is the current size of
	 * the guard, and 'stk_guardsize' is the size we want.
	 */
	if (cur_guardsize != stk_guardsize) {
		int	prot;
		void	*addr;
		size_t	len;

		if (cur_guardsize > stk_guardsize) {
			prot = PROT_READ | PROT_WRITE;
			addr = (void *)((char *)stk_base + stk_guardsize);
			len = cur_guardsize - stk_guardsize;
		} else {
			prot = PROT_NONE;
			addr = (void *)((char *)stk_base + cur_guardsize);
			len = stk_guardsize - cur_guardsize;
		}
		TRACE(T_PT, ("stk_alloc: mprotect(%#x, %#x, %#x)",
			addr, len, prot));
		if (mprotect(addr, len, prot)) {
			panic("stk_alloc()", "couldn't mprotect stack guard");
		}
	}
	STK_GUARDSIZE(stk_start) = stk_guardsize;
	TRACE(T_PT, ("stk_alloc stack info: %#x, %#x, %#x, %#x",
		stk_base, stk_start, stk_size, stk_guardsize));

	return (stk_start);
}


/*
 * stk_free(stk_start)
 *
 * Free the stack.
 */
void
stk_free(void *stk_start)
{
	__psunsigned_t	value;

	TRACE(T_PT, ("stk_free(%#x)", stk_start));

	ASSERT(sched_entered());

	if (stk_start) {
		if ((value = STK_SAVE(stk_start)) <= STK_LAST_BUCKET) {
			/*
			 * A cacheable stack - add it to the free list.
			 */
			QS_ADD_FIRST(&stk_buckets[value], stk_start);

		} else if (value != STK_NO_BUCKET) {
			/*
			 * An uncacheable stack - deallocate it.
			 */
			(void)munmap(STK_START_TO_BASE(stk_start,value), value);
		}
	}
}


/*
 * stk_fork_child()
 *
 * Reset the stack cache after fork()
 *
 */
void
stk_fork_child(void)
{
	int	i;

	/* Stack bucket lists may have been left busy by the parent.
	 * We just reset the queue - the list is lost XXX.
	 */
	for (i = 0; i < sizeof(stk_buckets)/sizeof(stk_buckets[0]); i++) {
		if (stk_buckets[i] == QS_BUSY) {
			QS_INIT(&stk_buckets[i]);
		}
	}
}
