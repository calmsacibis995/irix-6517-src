#ident "$Revision: 1.6 $"

/**************************************************************************
 *									  *
 *            Copyright (C) 1993-1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/splock.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <errno.h>
#include <sys/bpqueue.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"
#include "xlv_mem.h"

extern sema_t xlv_memcfg;		/* protect re-configuring xlv memory */

#ifdef DEBUG
#include <sys/major.h>
#include <sys/xlv_lock.h>
#include "xlv_procs.h"
#include <sys/ktrace.h>

extern int xlvmajor;
extern ktrace_t *xlv_trace_buf;		/* global xlv trace buffer */
extern ktrace_t *xlv_lock_trace_buf;	/* global xlv lock trace buffer */

/*
 *  xlv_trace():    add the current event to the global xlv tracebuffer.
 *
 *  XLV maintains (if DEBUG defined) a global trace buffer. The contents
 *  of this buffer can be examined via idbg's "xlvtrace" command.
 *  Note that you can pass a search field to xlvtrace so that you only
 *  display trace records of interest.
 */

void
xlv_trace (
	char	*id,		/* string describing event */
	int	bp_type,	/* the type of buffer (user, top, low) */
	buf_t	*bp)		/* buffer to be traced */
{
	xlv_tab_subvol_t 	*xlv_p;
	xlv_io_context_t	*io_context;
	buf_t			*parent_bp;
	dev_t			dev;
	char			char1, char2;
	unsigned		lock_count;

	if (xlv_trace_buf == NULL) {
		/*
		 * Not initialized yet.
		 */
		return;
	}

	io_context = NULL;
	parent_bp = NULL;

	switch (bp_type) {

		case XLV_TRACE_USER_BUF:
			/*
			 * If this is a top level xlv buffer then b_sort
			 * should be the io_context.
			 */
			if (emajor(bp->b_edev) == XLV_LOWER_MAJOR
			    || emajor(bp->b_edev) == xlvmajor) {
				io_context = (xlv_io_context_t *)bp->b_sort;
			}
			break;

		case XLV_TRACE_LOW_BUF:
			parent_bp = bp->b_forw;
			while ((parent_bp != NULL) &&
			       (parent_bp != bp)) {
				if (IS_MASTERBUF(parent_bp))
					break;
				parent_bp = parent_bp->b_forw;
			}

			if (parent_bp) {
				dev = parent_bp->b_edev;

				xlv_p = NULL;
				if (xlv_tab) 
					xlv_p = &xlv_tab->subvolume[minor(dev)];

				parent_bp = parent_bp->b_private;

				if ( xlv_p && (xlv_p->num_plexes > 1 ) && 
				     (parent_bp != NULL) ) {
					io_context = (xlv_io_context_t *) 
						parent_bp->b_sort;
				}
			}
			break;

		case XLV_TRACE_TOP_BUF:
			parent_bp = bp->b_private;

			/*
			 * if this volume is not plexed, b_private is not 
	 		 * the parent buffer
			 */
			dev = bp->b_edev;

			xlv_p = NULL;
			if (xlv_tab)
				xlv_p = &xlv_tab->subvolume[minor(dev)];

			if ( (parent_bp) && xlv_p && ( xlv_p->num_plexes > 1) )
				io_context = (xlv_io_context_t *)
					parent_bp->b_sort;
			break;

		default:
			break;
	}

	lock_count = xlv_io_lock[minor(bp->b_edev)].io_count;

	if ( (bp->b_dmaaddr != NULL) && (!(bp->b_flags & B_PHYS)) ) {
		char1 = bp->b_dmaaddr[0];
		char2 = bp->b_dmaaddr[bp->b_bcount-1];
	}
	else {
		char1 = 0xff; char2 = 0xff;
	}

	if (bp_type == XLV_TRACE_DISK_BUF) {
	    ktrace_enter(xlv_trace_buf,
			 (void *) id,
			 (void *) bp,
			 (void *) ((unsigned long)(bp->b_flags)),
			 (void *) ((unsigned long)(bp->b_bcount)),
			 (void *) ((unsigned long)(bp->b_bufsize)),
			 (void *) (bp->b_blkno),
			 (void *) (bp->b_dmaaddr),
			 (void *) NULL,
			 (void *) NULL,
			 (void *) NULL,
			 (void *) NULL,
			 (void *) NULL,
			 (void *) NULL,
			 (void *) ((unsigned long)char1),
			 (void *) ((unsigned long)char2),
			 (void *) ((unsigned long)lock_count));
	}
	else if (io_context != NULL) {
	    ktrace_enter (xlv_trace_buf,
		(void *) id,
		(void *) bp,
		(void *) ((unsigned long)(bp->b_flags)),
		(void *) ((unsigned long)(bp->b_bcount)),
		(void *) ((unsigned long)(bp->b_bufsize)),
		(void *) (bp->b_blkno),
		(void *) (bp->b_dmaaddr),
		(void *) parent_bp,
		(void *) io_context,
		(void *) ((unsigned long)(io_context->state)),
		(void *) (io_context->original_buf),
		(void *) (&io_context->retry_buf),
#ifdef PLEX_MMAP_COPY
		(void *) (io_context->mmap_write_buf),
#else
		NULL,
#endif
		(void *) ((unsigned long)char1),
		(void *) ((unsigned long)char2),
		(void *) ((unsigned long)lock_count));
	}
	else {
	    ktrace_enter (xlv_trace_buf,
                (void *) id,
                (void *) bp,
                (void *) ((unsigned long)(bp->b_flags)),
                (void *) ((unsigned long)(bp->b_bcount)),
                (void *) ((unsigned long)(bp->b_bufsize)),
                (void *) (bp->b_blkno),
                (void *) (bp->b_dmaaddr),
                (void *) parent_bp,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
		(void *) ((unsigned long)char1),
                (void *) ((unsigned long)char2),
		(void *) ((unsigned long)lock_count));
	}
}


/*
 *  xlv_lock_trace(): trace xlv locking
 *
 *  XLV maintains (if DEBUG defined) a global trace buffer for locks.
 *  The contents of this buffer can be examined via idbg's "xlvlhist"
 *  command.
 */

void
xlv_lock_trace (
	char	*id,		/* string describing event */
	int	bp_type,	/* the type of buffer (user, top, low) */
	buf_t	*bp)		/* buffer to be traced */
{
	xlv_tab_subvol_t 	*xlv_p;
	xlv_io_context_t	*io_context;
	dev_t			dev;
	buf_t			*parent_bp;
	unsigned long		lock_count;
	unsigned long		subvol_index;
	short			acc_count, wait_count;

	if (xlv_lock_trace_buf == NULL) {
		/*
		 * Not initialized yet.
		 */
		return;
	}

	io_context = NULL;
	parent_bp = NULL;
	subvol_index = 0;

	switch (bp_type) {

		case XLV_TRACE_USER_BUF:
			io_context = (xlv_io_context_t *) bp->b_sort;
			subvol_index = minor(bp->b_edev);
			break;

		case XLV_TRACE_LOW_BUF:
			parent_bp = bp->b_forw;
			while ((parent_bp != NULL) &&
			       (parent_bp != bp)) {
				if (IS_MASTERBUF(parent_bp))
					break;
				parent_bp = parent_bp->b_forw;
			}


			if (parent_bp) {
				/*
			 	 * if this volume is not plexed, b_private 
				 * is not the parent buffer
			 	 */
				dev = parent_bp->b_edev;
				xlv_p = NULL;
				if (xlv_tab)
					xlv_p = &xlv_tab->subvolume[minor(dev)];

				subvol_index = minor(parent_bp->b_edev);
				parent_bp = parent_bp->b_private;
				if (	(parent_bp != NULL)		 && 
					xlv_p &&
					(xlv_p->num_plexes > 1) ) {
					io_context = (xlv_io_context_t *) 
						parent_bp->b_sort;
				}
			}
			break;

		case XLV_TRACE_TOP_BUF:
			parent_bp = bp->b_private;

			/*
			 * if this volume is not plexed, b_private is not 
	 		 * the parent buffer
			 */
			dev = bp->b_edev;
			xlv_p = NULL;
			if (xlv_tab)
				xlv_p = &xlv_tab->subvolume[minor(dev)];

			if (parent_bp && xlv_p && ( xlv_p->num_plexes > 1) ) {
				io_context = (xlv_io_context_t *)
					parent_bp->b_sort;
			}
			subvol_index = minor(bp->b_edev);
			break;

		case XLV_TRACE_ADMIN:
			subvol_index = (unsigned long) bp;
			break;

		default:
			break;
	}

	lock_count = xlv_io_lock[subvol_index].io_count;
	acc_count = xlv_io_lock[subvol_index].io_count;
	wait_count = xlv_io_lock[subvol_index].wait_count;

	if (bp_type == XLV_TRACE_ADMIN) {
	    ktrace_enter (xlv_lock_trace_buf,
                (void *) id,
		(void *) subvol_index,
		(void *) NULL,
		(void *) NULL,
		(void *) NULL,
		(void *) NULL,
		(void *) NULL,
		(void *) NULL,
		(void *) lock_count,
                (void *) ((unsigned long)acc_count),
                (void *) ((unsigned long)wait_count),
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL);
	}
	else if (io_context != NULL)
	    ktrace_enter (xlv_lock_trace_buf,
		(void *) id,
		(void *) bp,
		(void *) parent_bp,
		(void *) io_context,
		(void *) ((unsigned long)(io_context->state)),
		(void *) (io_context->original_buf),
		(void *) (&io_context->retry_buf),
#ifdef PLEX_MMAP_COPY
		(void *) (io_context->mmap_write_buf),
#else
		NULL,
#endif
		(void *) lock_count,
		(void *) ((unsigned long)acc_count),
		(void *) ((unsigned long)wait_count),
		(void *) NULL,
		(void *) NULL,
		(void *) NULL,
		(void *) NULL,
		(void *) NULL);
	else
	    ktrace_enter (xlv_lock_trace_buf,
		(void *) id,
                (void *) bp,
                (void *) parent_bp,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) lock_count,
                (void *) ((unsigned long)acc_count),
                (void *) ((unsigned long)wait_count),
                (void *) NULL,
		(void *) NULL,
                (void *) NULL,
                (void *) NULL,
                (void *) NULL);


}
#endif /* DEBUG */


#ifdef XLV_RESMEM_DEBUG
#include <sys/kmem.h>
#ifndef DEBUG
#include <sys/ktrace.h>
#endif
ktrace_t	 *xlv_rm_trace_buf = NULL;
/*
 * XLV RESERVED MEMORY DEBUG ktrace entry format 1:
 *  0: string identifying where called
 *  1: name of rm structure
 *  2: addr of rm structure
 *  3: size
 *  4: slots
 *  5: hits
 *  6: misses
 *  7: waits
 *  8: start of res mem
 *  9: last slot
 * 10: current slot
 * 11: bp
 * 12: count of the memory wait queue
 * 13: head  of the memory wait queue
 * 14: tail  of the memory wait queue
 *
 * XLV RESERVED MEMORY DEBUG ktrace entry format 2:
 *  0: string identifying where called
 *  1: name of rm structure
 *  2: hits
 *  3: misses
 *  4: waits
 *  5: sema count
 *  6: count of the memory wait queue
 *  7: bp
 *  8: current slot
 *  9: returned slot
 * 10: head  of the memory wait queue
 * 11: tail  of the memory wait queue
 */
#define RESMEM_DEBUG_1(where, rm)
#define RESMEM_DEBUG(where, rm, bp, ptr)				\
	ktrace_enter(xlv_rm_trace_buf, where, rm->name, 		\
		     (void *) (long long) rm->hits,			\
		     (void *) (long long) rm->misses,			\
		     (void *) (long long) rm->waits,			\
		     (void *) (long long) -valusema(&rm->wait),		\
		     (void *) (long long) rm->memory_wait_queue.n_queued,\
		     bp, rm->res_mem, ptr,				\
		     BUF_QUEUE_HEAD(rm->memory_wait_queue),		\
		     BUF_QUEUE_TAIL(rm->memory_wait_queue),		\
		     NULL, NULL, NULL, NULL)
#else
#define RESMEM_DEBUG_1(where, rm)
#define RESMEM_DEBUG(where, rm, bp, ptr)
#endif

extern unsigned int xlv_ioctx_slots, xlv_topmem_slots, xlv_lowmem_slots;

/*
 * XLV reserved memory routines:
 * These routines must be used whenever memory allocation is required
 * while doing io, or when io to a subvolume is blocked (e.g., for error
 * recovery or configuration changes).  Otherwise, there is a potential
 * for memory deadlocks if swapping to an XLV volume.

 * These routines are similar to zone allocators; there are a number
 * of slots of memory of a given size -- the number of slots and the
 * size is determined when the reserved memory structure is created
 * using xlv_res_mem_create().  Allocation is done from this pool
 * whenever possible; slots that were taken from this pool are
 * returned to the pool.  If no memory is available in this pool,
 * kmem_zalloc may be used to get memory; in this case, kmem_free() is
 * called to return this memory.  There are counters that track the
 * number of hits (got the memory from the pool), misses (got the
 * memory from kmem_zalloc), or waits (couldn't get the memory: had to
 * wait for it).  These can be used to tune the number of slots in the
 * pool.

 * The primary difference between zone allocation and reserved memory
 * allocation is what happens when memory is *not* available.  With
 * reserved memory allocation, what happens is dependent on the buf_t
 * pointer that the caller passes in to the allocation routine.  If
 * this is NULL, the caller sleeps on a semaphore; when one of the
 * slots from the pool is returned, the sleeper is woken up and handed
 * a slot.  Thus, in low memory situations, clients of the reserved
 * memory pool cycle through the pool, coordinating access using the
 * semaphore.
 *
 * The other possibility is that the caller passes in a non-NULL buf_t
 * pointer.  In this case, the caller's buf_t gets enqueued, and a
 * NULL pointer is returned to the caller.  When someone returns any
 * memory (i.e., not necessarily one of the reserved pool slots), this
 * memory is handed to the buf_t in the b_sort field, the B_XLV_IOC
 * bit is turned on in b_flags, and the buffer is enqueued in
 * xlvd_work_queue.  This option is currently only for getting
 * xlv_io_context_t memory.  Without this option, xlvd can deadlock as
 * follows:
 * 1) there is an error on volume foo on buf1;
 * 2) io is blocked to foo; buf2 gets enqueued for reissue when recovery
 *	is complete;
 * 3) foo's error recovery is done, xlvd releases the io context memory
 *	for buf1;
 * 4) meanwhile, buf3 comes along for io to bar, grabs the io context just
 *	released;
 * 5) xlvd tries to reissue buf2; there are no free slots in the reserved
 *	memory for io contexts, so xlvd waits on a psema for the reserved
 *	memory;
 * 6) buf3 also gets an error and gets enqueued in xlvd_work_queue.
 *
 * Now, the system is deadlocked: buf3 won't release its io context memory
 * until it gets processed; xlvd cannot proceed until buf3's io context is
 * released because xlvd is blocked on the semaphore.
 *
 * With the option to not block, but enqueue buf2, the xlvd is free to move
 * on, in this case to process buf3.  When buf3's error recovery is done,
 * the io context memory will be freed up so that buf2 can be reissued.
 *
 * The number of slots and size can be changed when by calling
 * xlv_res_mem_resize(): this routine waits for all slots in use to be
 * returned, and only then resizes the reserved memory pool.  (Note:
 * the resize routine should normally only be called when no one is
 * using the reserved memory.  There has not been extensive testing of
 * the case when xlv_res_mem_resize() is called with slots in use.)
 */

void
xlv_res_mem_create(xlv_res_mem_t *rm, char *name)
{
    void	 *p;
    char	name2[32];

#ifdef XLV_RESMEM_DEBUG
    if (xlv_rm_trace_buf == NULL) {
	xlv_rm_trace_buf = ktrace_alloc(512, KM_SLEEP);
    }
#endif

    rm->size = (rm->size + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1);
    rm->active = rm->quiesce = rm->waiters = 0;
    rm->totalhits = rm->totalmisses = rm->hits = rm->misses = rm->waits = 0;
    rm->memresize = B_FALSE;
    rm->resized = rm->resizefailed = 0;

    ASSERT(rm->size > 0 && rm->slots > 0);

    p = kmem_zalloc(rm->size*rm->slots, KM_SLEEP);
    ASSERT(p);

    /* delineate "my" memory, so that we know when it is returned */
    rm->res_mem = rm->res_mem_start = p;
    rm->res_mem_last = (void *) ((size_t) p + rm->size*(rm->slots - 1));

    /* Initialize memory waiters' queue */
    BUF_QUEUE_INIT(rm->memory_wait_queue);

    /* chain each slot (block of reserved memory) to the next */
    /* XXX KK: should avoid splitting slots across page boundaries */
    for (; p < rm->res_mem_last; p = * (void **) p) {
	* (size_t *) p = (size_t) p + rm->size;
    }
    /* terminate the chain */
    * (void **) p = NULL;

    spinlock_init(&rm->lock, name);
    sprintf(name2,"%s%d", name, 1);
    initnsema(&rm->io_wait, 0, name);
    initnsema(&rm->quiesce_wait, 0, name2);
    rm->name = name;

    RESMEM_DEBUG_1("CREATE ", rm);
}

int
xlv_res_mem_resize(xlv_res_mem_t *rm, __uint32_t size, __uint32_t slots,
		   boolean_t growpool_slots)
{
    void	 *p;
    void	 *oldp;
    int		  s, wait_count;
    int		 error = 0;
    extern	int xlv_maxvols;

    size = (size + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1);

    ASSERT(size > 0 && slots > 0);

    /* can hang up the system if we sit waiting for memory to get freed */
    p = kmem_zalloc(size * slots, KM_NOSLEEP);

    /* if a user program is trying to set a configuration, try very hard
     * to get the right size slots set.  Let the grow code attempt to
     * optimize pool access later
     */
    if (p == NULL && !growpool_slots && slots > xlv_maxvols/3) {
	slots = xlv_maxvols/3 + 1;	/* shrink the number of slots to
					 * cause the allocation to succeed */

	/* Now wait for the reduced size of memory */
    	p = kmem_zalloc(size * slots, KM_SLEEP);
    }

    if (p == NULL) {

	/* if we're trying to grow the # of slots, mark that we need to try
	 * again by clearing memresize 
	 */
	if (growpool_slots) {
    		s = mutex_spinlock(&rm->lock);
    		rm->memresize = B_FALSE;
    		rm->resizefailed++;
    		mutex_spinunlock(&rm->lock, s);
	}
	/* change current mem alloc to new size? */
	return ENOMEM;
    }
    s = mutex_spinlock(&rm->lock);

    /* if pool is still active */
    if (rm->active) {
	rm->quiesce = 1;
	mutex_spinunlock(&rm->lock, s);

	/* wait until the pool has quiesced */
	psema(&rm->quiesce_wait, PRIBIO);

    	s = mutex_spinlock(&rm->lock);
    }

    ASSERT(rm->active == 0);
    
    /* remember the old mem pointer to free later */
    oldp = rm->res_mem_start;

    rm->res_mem = rm->res_mem_start = p;
    rm->res_mem_last = (void *) ((size_t) p + size*(slots - 1));

    /* chain each block of reserved memory to the next */
    for (; p < rm->res_mem_last; p = * (void **) p) {
	* (size_t *) p = (size_t) p + size;
    }
    /* terminate the chain */
    * (void **) p = NULL;

    rm->size = size;

    /* set the new number of slots */
    rm->slots = slots;

    /* mark memory allocated to waiters now. Be careful not to allocate
     * more memory than is in the pool
     */
    if (rm->waiters < slots)
	wait_count = rm->active = rm->waiters;
    else 
	wait_count = rm->active = slots;

    rm->waiters = 0;
    rm->quiesce = 0;

    /* don't do any stat updates unless we were trying to change # of slots */
    if (growpool_slots) {

	/* now that all of the new slots are freed, then
	 * reset the counters and clear memresize.  
	 */

	/* keep a running total of hits/misses */
	rm->totalhits += rm->hits;
	rm->totalmisses += rm->misses;

	/* reset the current counters */
	rm->hits = rm->misses = 0;

	/* Now we're done resizing memory */
	rm->memresize = B_FALSE;
	rm->resized++;
    }
    mutex_spinunlock(&rm->lock, s);

    RESMEM_DEBUG_1("RESIZE ", rm);

    /* free the old memory */
    kmem_free(oldp, 0);

    /* schedule all waiters with their memory */
    for (; wait_count; wait_count--) {
	vsema(&rm->io_wait);
    }

    return error;

}


/*
 * called from timeout handler to size up the reserved memory slots 
 */
void
xlv_time_resize(xlv_res_mem_t *rm)
{
	unsigned int slots;

	/* force our priority to basepri */
	spl0();

	/* protect against a race condition with xlv_assemble, which
	 * also tries to resize memory 
	 */
	psema(&xlv_memcfg, PRIBIO);

	/* memresize has to be true since once set, it can only be cleared
	 * via this timeout path or from the mem_adjust path both of which
	 * cannot clear it without holding the memcfg sema and spinlock
	 */
	ASSERT(rm->memresize);

	if (rm == &xlv_ioctx_mem) {
		/* scale up the number of slots */
		slots = xlv_ioctx_slots + 
			((uint64_t)rm->slots * rm->scale)/100 + 1;

		if (xlv_res_mem_resize(rm, rm->size, slots, B_TRUE) == 0)
			xlv_ioctx_slots = rm->slots;

	} else if (rm == &xlv_top_mem) {
		/* scale up the number of slots */
		slots = xlv_topmem_slots + 
			((uint64_t)rm->slots * rm->scale)/100 + 1;

		if (xlv_res_mem_resize(rm, rm->size, slots, B_TRUE) == 0)
			xlv_topmem_slots = rm->slots;
	} else {
		ASSERT(rm == &xlv_low_mem);

		/* scale up the number of slots */
		slots = xlv_lowmem_slots + 
			((uint64_t)rm->slots * rm->scale)/100 + 1;

		if (xlv_res_mem_resize(rm, rm->size, slots, B_TRUE) == 0)
			xlv_lowmem_slots = rm->slots;
	}

	vsema(&xlv_memcfg);
}

/*
 * Adjust memory pool parameters based on user input
 */
int
xlv_res_mem_adjust(xlv_attr_memory_t *param, xlv_res_mem_t *rm)
{
	int s, error = 0;
	unsigned int slots;

	/* protect against a race condition with xlv_assemble, which
	 * also tries to resize memory 
	 */
	psema(&xlv_memcfg, PRIBIO);

	s = mutex_spinlock(&rm->lock);
	if (!rm->memresize) {
		rm->memresize = B_TRUE;
		rm->threshold = param->threshold;
		rm->scale = param->scale;
		mutex_spinunlock(&rm->lock,s);

		if (rm == &xlv_top_mem) {
			if (param->slots)
				slots = param->slots;
			else 
				slots = xlv_topmem_slots;

			if (error = xlv_res_mem_resize(rm, rm->size, slots,
							B_TRUE)) {
				vsema(&xlv_memcfg);
				return error;
			}

			xlv_topmem_slots = rm->slots;

			/* change the io context params too */
			s = mutex_spinlock(&xlv_ioctx_mem.lock);

			if (!xlv_ioctx_mem.memresize) {
				xlv_ioctx_mem.memresize = B_TRUE;
				xlv_ioctx_mem.threshold = param->threshold;
				xlv_ioctx_mem.scale = param->scale;
				mutex_spinunlock(&xlv_ioctx_mem.lock, s);

				if (param->slots)
					slots = param->slots;
				else 
					slots = xlv_topmem_slots;

				if (error = xlv_res_mem_resize(&xlv_ioctx_mem, 
					   xlv_ioctx_mem.size, slots, B_TRUE)) {
						
					vsema(&xlv_memcfg);
					return error;
				}

				xlv_ioctx_slots = xlv_ioctx_mem.slots;
			}
		} else { 
			ASSERT(rm == &xlv_low_mem);

			if (param->slots)
				slots = param->slots;
			else 
				slots = xlv_lowmem_slots;

			if (error = xlv_res_mem_resize(rm, rm->size, slots,
							B_TRUE)) {
				vsema(&xlv_memcfg);
				return error;
			}
			xlv_lowmem_slots = rm->slots;
		}
	} else {
		mutex_spinunlock(&rm->lock,s);
		error = EBUSY;
	}

	vsema(&xlv_memcfg);
	return error;
}

void
xlv_res_mem_destroy(xlv_res_mem_t *rm)
{
    int		  s;

    s = mutex_spinlock(&rm->lock);

    /* if pool is still active */
    if (rm->active) {
	rm->quiesce = 1;
	mutex_spinunlock(&rm->lock, s);

	/* wait until the pool has quiesced */
	psema(&rm->quiesce_wait, PRIBIO);

    	s = mutex_spinlock(&rm->lock);
    }

    ASSERT(rm->active == 0);

    RESMEM_DEBUG_1("DESTROY", rm);

    kmem_free(rm->res_mem_start, 0);
    rm->res_mem_start = rm->res_mem = rm->res_mem_last = NULL;
    rm->size = rm->slots = 0;
    freesema(&rm->io_wait);
    freesema(&rm->quiesce_wait);
    rm->name = "";
    mutex_spinunlock(&rm->lock, s);
}


/*
 * Allocate memory from a reserved pool.  If none is available,
 * do kmem_alloc(KM_NOSLEEP).  If that fails as well, then:
 *	either sleep waiting for the reserved memory,
 *	or enqueue the buffer bp in memory_wait_queue;
 */
void *
xlv_res_mem_alloc(register xlv_res_mem_t *rm, buf_t *bp, __uint32_t zerosize)
{
    void			 *p;
    int				  s;

    s = mutex_spinlock(&rm->lock);

    /* see if reserved memory is available, and not attempting to quiesce */
    if (rm->active < rm->slots && !rm->quiesce) {
redo:
	rm->active++;
	p = rm->res_mem;
	rm->res_mem = * (void **) rm->res_mem;
	rm->hits++;
	mutex_spinunlock(&rm->lock, s);
	bzero((char *) p, zerosize);
	RESMEM_DEBUG("GET RM ", rm, bp, p);

	return p;
    } 

    mutex_spinunlock(&rm->lock, s);

    /* no reserved memory available at this time */
    p = kmem_zalloc(zerosize, KM_NOSLEEP);

    if (p) {
	s = mutex_spinlock(&rm->lock);
	rm->misses++;
	/* before deciding to resize be sure that:
	 * a) we're not alreadying resizing.
	 * b) our miss rate exceeds the percentage set by the user
	 * c) that we've accumulated sufficient samples (100 misses)
	 */
	if (!rm->memresize && 
	    rm->misses > ((uint64_t)rm->hits * rm->threshold)/100 &&
	    rm->misses > 100) {
		rm->memresize = B_TRUE;
		mutex_spinunlock(&rm->lock, s);
		timeout(xlv_time_resize, rm, 0L);
	} else {
		mutex_spinunlock(&rm->lock, s);
	}
		
	RESMEM_DEBUG("ZALLOC ", rm, bp, p);

	return p;
    }

    if (bp) {
	/* not willing to sleep: queue up bp */
    	s = mutex_spinlock(&rm->lock);
	rm->waits++;
	BUF_ENQUEUE(rm->memory_wait_queue, bp);
    	mutex_spinunlock(&rm->lock, s);
	RESMEM_DEBUG("ENQUEUE", rm, bp, NULL);

	return NULL;
    }

    s = mutex_spinlock(&rm->lock);
    /* now that we know we're going to wait for memory, we need to
     * re-evaluate our decision under the lock to make sure it is 
     * still valid.  If memory is now available, go allocate it.  If
     * not, set io_wait so when memory is freed, we'll be woken up
     */
    if (rm->active < rm->slots && !rm->quiesce)
	goto redo;
    else 
    	rm->waiters++;
    mutex_spinunlock(&rm->lock, s);

    /* no heap memory: wait for reserved memory. */
    RESMEM_DEBUG("PSEMA   ", rm, bp, NULL);
    psema(&rm->io_wait, PRIBIO);
    s = mutex_spinlock(&rm->lock);
    rm->waits++;
    p = rm->res_mem;
    rm->res_mem = * (void **) rm->res_mem;
    mutex_spinunlock(&rm->lock, s);
    bzero((char *) p, zerosize);
    RESMEM_DEBUG("GOT    ", rm, bp, p);

    return p;
}


void
xlv_res_mem_free(xlv_res_mem_t *rm, void *ptr)
{
    int				  s;
    sema_t			 *sema = NULL;
    extern bp_queue_t		  xlvd_work_queue;
    register buf_t		 *bp;

    s = mutex_spinlock(&rm->lock);

    BUF_DEQUEUE(rm->memory_wait_queue, bp);
    if (bp) {
	/* some one is waiting for this memory */
	mutex_spinunlock(&rm->lock, s);

	RESMEM_DEBUG("DEQUEUE", rm, bp, ptr);
	bzero(ptr, rm->size);
	bp->b_sort = (__psunsigned_t) ptr;
	bp->b_flags |= B_XLV_IOC;
	bp_enqueue(bp, &xlvd_work_queue);

	return;
    }

    if (ptr >= rm->res_mem_start && ptr <= rm->res_mem_last) {
	/* memory was from the reserved memory pool */
	RESMEM_DEBUG("RETURN ", rm, NULL, ptr);
	* (void **) ptr = rm->res_mem;
	rm->res_mem = ptr;

	 /* If quiescing memory, wait until it's all back */
	if (rm->quiesce) {
		rm->active--;

		if (!rm->active)
			sema = &rm->quiesce_wait;

	} else if (rm->waiters) {
		rm->waiters--;
		sema = &rm->io_wait;
	} else {
		rm->active--;
	}
	mutex_spinunlock(&rm->lock, s);

	if (sema) vsema(sema);

	return;
    }

    /* memory was from the kernel heap */
    mutex_spinunlock(&rm->lock, s);

    RESMEM_DEBUG("MEMFREE", rm, NULL, ptr);
    kmem_free(ptr, 0);
}


/* print name of xlv device
 *
 */
void
xlvprint( dev_t dev, char *fmt)
{
	va_list 		ap;
	dev_t			minor_dev;
	xlv_tab_subvol_t	*xlv_p;

	minor_dev = minor(dev);
	if (! xlv_tab) {
		return;
        }

	xlv_p = &xlv_tab->subvolume[minor_dev];
	if ( (!(xlv_p) ) || ( !(xlv_p->vol_p) ) ) {
		return;
	}

	cmn_err( CE_CONT,"%s: ",xlv_p->vol_p->name);
	va_start( ap, fmt);
	icmn_err( CE_CONT, fmt, ap );
	va_end(ap);

	cmn_err(CE_CONT,"\n");
}


#ifdef XLV_MEM_DEBUG
#undef kmem_alloc
#undef kmem_zalloc
#undef kvpalloc
#undef kern_malloc

int MEM_DRAIN = 0, MEM_PRINT = 0;
int mem_drains = 0, mem_count = 0, mem_size = 0, mem_pcount = 0, mem_zeros = 0;
void *mem_list = NULL, *mem_pages = NULL;

void *
KMEM_ALLOC(size_t s, int f)
{
    void *foo = NULL;
    int   count = 0;

    while (MEM_DRAIN && (foo = kmem_alloc(s, KM_NOSLEEP)) != NULL) {
	* (void **) foo = mem_list;
	mem_list = foo;
	mem_count++;
	count = 1;
	mem_size += s;
    }

    if (!count) mem_zeros++; else mem_drains++;
    if ((count || !(mem_zeros & 0xff)) && MEM_PRINT) {
	printf(" ALLOC: drains: %d, count: %d, size: %d, pages: %d, "
	       "zeros: %d.\n",
	       mem_drains, mem_count, mem_size, mem_pcount, mem_zeros);
    }

    return kmem_alloc(s, f);
}


void *
KMEM_ZALLOC(size_t s, int f)
{
    void *foo = NULL;
    int   count = 0;

    while (MEM_DRAIN && (foo = kmem_alloc(s, KM_NOSLEEP)) != NULL) {
	* (void **) foo = mem_list;
	mem_list = foo;
	mem_count++;
	count = 1;
	mem_size += s;
    }

    if (!count) mem_zeros++; else mem_drains++;
    if ((count || !(mem_zeros & 0xff)) && MEM_PRINT) {
	printf("ZALLOC: drains: %d, count: %d, size: %d, pages: %d, "
	       "zeros: %d.\n",
	       mem_drains, mem_count, mem_size, mem_pcount, mem_zeros);
    }

    return kmem_zalloc(s, f);
}


void *
KVPALLOC(unsigned s, int f, int f2)
{
    void *foo = NULL;
    int   count = 0;

    while (MEM_DRAIN && (foo = kvpalloc(1, KM_NOSLEEP, 0)) != NULL) {
	* (void **) foo = mem_pages;
	mem_pages = foo;
	mem_pcount++;
	count = 1;
    }

    if (!count) mem_zeros++; else mem_drains++;
    if ((count || !(mem_zeros & 0xff)) && MEM_PRINT) {
	printf("PALLOC: drains: %d, count: %d, size: %d, pages: %d, "
	       "zeros: %d.\n",
	       mem_drains, mem_count, mem_size, mem_pcount, mem_zeros);
    }

    return kvpalloc(s, f, f2);
}


void *
KERN_MALLOC(size_t s)
{
    void *foo = NULL;
    int   count = 0;

    while (MEM_DRAIN && (foo = kmem_alloc(s, KM_NOSLEEP)) != NULL) {
	* (void **) foo = mem_list;
	mem_list = foo;
	mem_count++;
	count = 1;
	mem_size += s;
    }

    if (!count) mem_zeros++; else mem_drains++;
    if ((count || !(mem_zeros & 0xff)) && MEM_PRINT) {
	printf("MALLOC: drains: %d, count: %d, size: %d, pages: %d, "
	       "zeros: %d.\n",
	       mem_drains, mem_count, mem_size, mem_pcount, mem_zeros);
    }

    return kern_malloc(s);
}
#endif /* XLV_MEM_DEBUG */
