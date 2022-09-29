/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1999 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.83 $"

#include <values.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/sysmacros.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include "stdarg.h"
#include <sys/errno.h>
#include <sys/anon.h>
#include <sys/vnode.h>
#include <sys/nodepda.h>
#include <ksys/migr.h>
#include <sys/numa.h>
#include <sys/atomic_ops.h>
#include <ksys/rmap.h>
#include <sys/buf.h>
#include <sys/sysinfo.h>
#include <sys/idbgentry.h>
#include <sys/fault_info.h>
#include <sys/tile.h>

/*
 * Code to support (multiple page sizes) large page allocations.
 * A brief explanation about page size index.
 * Pfdats are allocated for every page of the minimum page size. The pfdats
 * carry a bit field which contains the page size index. The page size index
 * specifies the page size. Eg. 0 represents 16K, 1 represents 64K, 
 * 2 represents 256K etc. Most large page operations are done using the 
 * page size indices. The starting pfdat of the large page represents it.
 * Only that page is inserted into the free list. The rest of the pfdats
 * carry a sentinel page size index and are NOT INSERTED INTO THE FREE LIST.
 * This representation is valid inside the page allocator only. The page size
 * field is cleared when pfdat is allocated. A large page is treated as a list
 * of pfdats by other parts of VM.
 */


/*
 * Coalescing daemon synchronization variables.
 * sv and lock are used to synchronize the coalescing daemon and pagealloc.
 * The daemon sleeps on sv when it has nothing to do.
 * The timeout sema is used when the daemon wants to wait a short while before
 * it retries its algorithm. The wait time is defined in CO_DAEMON_TIMEOUT
 * The "node" variable is used to specify the node which the coalescing 
 * daemon should try first.
 */


struct	{

	/* Sync. var. on which co daemon sleeps */
	sv_t		daemon_wait;	
	lock_t		lock;	 	/* lock to protect sv */

	/* Process sleeps on this sv_t when it waits for a large page */
	sv_t		page_size_wait[NUM_PAGE_SIZES]; 

	/*
	 * Flag (CO_DAEMON_WAIT) to indicate the coalescing daemon is
	 * sleeping.
	 */
	short		flags;

	cnodeid_t	start_node;
	/*
	 * Sets the wait flag at the appropriate page index. eg.
	 * If a process is waiting for a 64K page then it sets bit 2.
	 */
	short		pgsz_wait_flags;
	toid_t		timeout_id;	/* Pending timeout id */
} co_daemon_sync;

/*
 * Parameters passed by callers when they wakeup coalesced.
 */
typedef struct co_daemon_params_s {
	int		timeout_step; /* Current timeout step */
	cnodeid_t 	start_node; /* Node on which large pages are needed */
	int		policy;
} co_daemon_params_t;

/*
 * Default hi watermarks for various page sizes. They are expresessed in
 * percent of total pages per node.
 */

extern	int	percent_totalmem_16k_pages;
extern	int	percent_totalmem_64k_pages;
extern	int	percent_totalmem_256k_pages;
extern	int	percent_totalmem_1m_pages;
extern	int	percent_totalmem_4m_pages;
extern	int	percent_totalmem_16m_pages;

/*
 * Tunable parameter. Set to 0 if large pages are not enabled 1 otherwise.
 */
extern	int	large_pages_enable;

/*
 * Coalesced runs periodically to get large pages if it cannot meet
 * the requirements. The following tuneable determines the period.
 */
extern	int	coalesced_run_period;

int	page_size_weight[NUM_PAGE_SIZES];

/*
 * If set don't do coalescing in pagefree. It is set if no large pages
 * are needed.
 */
int	large_pages_not_needed;

/*
 * If this systune is set then coalesced will put together as many large pages
 * as possible whenever it is kicked, as opposed to just satisfying the
 * immediate demand for large pages.
 */
extern int coalesced_precoalesce;

/*
 * Default threshold values used by the coalescing daemon. Its the number of
 * pages in a large page chunk that should be free before the coalescing daemon
 * will start to coalesce.
 */


#if	_PAGESZ == 4096

#define	DEF_16K_BIT_THRESHOLD	2
#define DEF_64K_BIT_THRESHOLD	9
#define DEF_256K_BIT_THRESHOLD	40
#define DEF_1MB_BIT_THRESHOLD	240
#define DEF_4MB_BIT_THRESHOLD	1000
#define DEF_16MB_BIT_THRESHOLD	4000


int	numbits_threshold[NUM_PAGE_SIZES] = {
	0,
	DEF_16K_BIT_THRESHOLD,
	DEF_64K_BIT_THRESHOLD,
	DEF_256K_BIT_THRESHOLD,
	DEF_1MB_BIT_THRESHOLD,
	DEF_4MB_BIT_THRESHOLD,
	DEF_16MB_BIT_THRESHOLD,
};

#elif	_PAGESZ == 16384

#define DEF_64K_BIT_THRESHOLD	2
#define DEF_256K_BIT_THRESHOLD	9
#define DEF_1MB_BIT_THRESHOLD	40
#define DEF_4MB_BIT_THRESHOLD	240
#define DEF_16MB_BIT_THRESHOLD	1000

int	numbits_threshold[NUM_PAGE_SIZES] = {
	0,
	DEF_64K_BIT_THRESHOLD,
	DEF_256K_BIT_THRESHOLD,
	DEF_1MB_BIT_THRESHOLD,
	DEF_4MB_BIT_THRESHOLD,
	DEF_16MB_BIT_THRESHOLD,
};

#else
#error	"Unknown page size"
#endif

/*
 * Timeout step function. We max out 5 min wait time per iteration.
 */

#define	MAX_TIMEOUT_STEPS	9

static int	coalesced_timeout_vals[MAX_TIMEOUT_STEPS] = {
		HZ,
		HZ,
		5*HZ,
		5*HZ,
		30*HZ,
		30*HZ,
		60*HZ,
		60*HZ,
		5*60*HZ /* Replaced by the period tuneable */
};

#define	GET_CO_DAEMON_TIMEOUT(step) \
		(((step) < MAX_TIMEOUT_STEPS) ? \
			coalesced_timeout_vals[(step)] : \
			coalesced_timeout_vals[MAX_TIMEOUT_STEPS - 1])

#define	SET_CO_DAEMON_MAX_TIMEOUT(period) \
	coalesced_timeout_vals[MAX_TIMEOUT_STEPS - 1] = ((period) * HZ)

/*
 * Macro to check if anybody is waiting for a large page.
 */

#define PAGE_SIZE_HAS_WAITERS() (co_daemon_sync.pgsz_wait_flags)

/*
 * Page bitmap pointers
 */
#ifdef DISCONTIG_PHYSMEM
bitmap_t page_bitmaps_pure[PAGE_BITMAPS_NUMBER];
bitmap_t page_bitmaps_tainted[PAGE_BITMAPS_NUMBER];
#else
bitmap_t page_bitmap_pure;
bitmap_t page_bitmap_tainted;
#endif

/*
 * Number of mild coalescing tries after which to try a stronc coalescing 
 */
#define	MAX_MILD_COALESCING_ATTS	50

static	void 		lpage_remove(cnodeid_t, pfd_t *);	
static	void 		lpage_insert(cnodeid_t, pfd_t *, int);
static	void		lpage_assemble(cnodeid_t, pfd_t *, int);
static	int  		lpage_needed(cnodeid_t, pg_free_t *);
static	void 		lpage_release_held_pages(cnodeid_t, pfd_t *, pfd_t *);
static	void 		lpage_construct(cnodeid_t, pfd_t *, 
					pgszindx_t, pfd_t *, pfd_t *);
static	int  		lpage_hold_free_pages(cnodeid_t, pfd_t *, bitmap_t, 
					pgszindx_t, pfd_t **);
static void 		lpage_cache_remove(pfd_t *, int);
static void 		large_page_chop(pfd_t *, size_t, size_t);
static pgszindx_t	lpage_get_max_pindx(pfd_t *, pgszindx_t);
static void 		lpage_size_broadcast(pgszindx_t);
static int 		lpage_low_on_memory(cnodeid_t, pg_free_t *);
       pfd_t		*lpage_alloc_contig_physmem(cnodeid_t, size_t, 
						pgno_t, int);
static pfd_t		*lpage_alloc_aligned_page(cnodeid_t, pgszindx_t *, 
						int, int);
static void 		lpage_reserve(void);
static void 		lpage_pre_allocate_pages(pgno_t, size_t);
static void 		lpage_init_weights(void);

static void 		coalesce_chunk(pg_free_t *, cnodeid_t, pfd_t *, 
					pgszindx_t, pfd_t *, int);
static int 		coalesce_leaf_chunk(cnodeid_t, pfd_t *, pgszindx_t, 
							int);
static void 		coalesce_daemon_sleep(int, int, co_daemon_params_t *);
static int 		coalesce_engine(co_daemon_params_t *);
static int		coalesce_node(cnodeid_t, int);

/*
 * Sanity routine to tune watermarks via systune.
 */
int	_lpage_watermarks_sanity(int *, int);

#ifdef  DEBUG
int	is_valid_pfdat(cnodeid_t, pfd_t *);
#endif

#ifdef	VFAULT_TRACE_ENABLE
extern void vfault_print(int, int);
#endif




/*
 * lpage_init_watermarks:
 * Initialize the watermarks for a node. We have watermarks for every
 * node because we keep the watermarks as number of pages and they vary
 * from node to node. Also due to NUMA locality demand for large pages
 * may vary from node to node.
 */

void
lpage_init_watermarks(cnodeid_t node)
{
	pg_free_t	*pfl;
	int		node_totalmem = 
				NODEPDA(node)->node_pg_data.node_total_mem;
	pgszindx_t	i;
	int		large_page_freemem = 0;

	/*
	 * Initialize the hi watermarks for master node's pg_freelist.
	 * It will be copied in meminit to other nodes.
	 */
	if (!large_pages_enable) {
		large_pages_not_needed = 1;
		return;
	}
        pfl = (pg_free_t *)&(NODEPDA(0)->node_pg_data.pg_freelst[(node)]);
	for (i = MIN_PGSZ_INDEX+1; i < NUM_PAGE_SIZES; i++) {
		pfl->hiwat[i] = (page_size_weight[i]* node_totalmem/100)
						/(PGSZ_INDEX_TO_CLICKS(i));
		large_page_freemem += pfl->hiwat[i];
	}
	if (large_page_freemem) {
		pfl->hiwat[MIN_PGSZ_INDEX] = 
				(node_totalmem - large_page_freemem);
		large_pages_not_needed = 0;
	}
	else large_pages_not_needed = 1;
}

/*
 * lpage_init:
 * Routine which reads from system tunables. Also does all the 
 * large page size data structure initializations.
 */

void
lpage_init()
{
	cnodeid_t	node;
	pgszindx_t	i;

	lpage_init_weights();
	for ( node = 0; node < numnodes; node++) {
		NODEPDA(node)->node_pg_data.node_total_mem 
						= NODE_FREEMEM(node);
		lpage_init_watermarks(node);
	}

	SET_CO_DAEMON_MAX_TIMEOUT(coalesced_run_period);

	/*
	 * Initialize the synch. data structures.
	 */
	for ( i = 0; i < NUM_PAGE_SIZES; i++) {
		init_sv(&co_daemon_sync.page_size_wait[i],
			SV_DEFAULT, "palls", i);
	}

	sv_init(&co_daemon_sync.daemon_wait, SV_DEFAULT, "Cod_sv");
	spinlock_init(&co_daemon_sync.lock, "Cod_lck");
	lpage_reserve();
#ifdef	VFAULT_TRACE_ENABLE
	idbg_addfunc("vfault_trc_print", vfault_print);
#endif
}


/*
 * lpage_init_weights:
 * Initialize page size weights.
 */
static void
lpage_init_weights()
{
#if	_PAGESZ == 4096
#ifdef TILES_TO_LPAGES
	percent_totalmem_64k_pages = 20;
#endif /* TILES_TO_LAPGES */
	page_size_weight[0] = 0;
	page_size_weight[1] = percent_totalmem_16k_pages;
	page_size_weight[2] = percent_totalmem_64k_pages;
	page_size_weight[3] = percent_totalmem_256k_pages;
	page_size_weight[4] = percent_totalmem_1m_pages;
	page_size_weight[5] = percent_totalmem_4m_pages;
	page_size_weight[6] = percent_totalmem_16m_pages;
#elif	_PAGESZ == 16384
	page_size_weight[0] = percent_totalmem_16k_pages;
	page_size_weight[1] = percent_totalmem_64k_pages;
	page_size_weight[2] = percent_totalmem_256k_pages;
	page_size_weight[3] = percent_totalmem_1m_pages;
	page_size_weight[4] = percent_totalmem_4m_pages;
	page_size_weight[5] = percent_totalmem_16m_pages;
#else
#error	"Unknown page size"
#endif
}

/*
 * _lpage_watermarks_sanity:
 * Sanity routine to tune watermarks via systune. Updates all the data
 * structures to reflect the new values of hiwater marks. This function
 * will be called from systune. It is defined as a non-void function so
 * it just returns 0.
 */

int
_lpage_watermarks_sanity(
	int *address, 
	int value)
{
	cnodeid_t	node, tmp_node;
	pg_free_t	*pfl, *orig_pfl;
	int		i;
	int		old_value;

	if (!address)
		return 0;
	old_value = *address;
	*address = value;

	SET_CO_DAEMON_MAX_TIMEOUT(coalesced_run_period);

#if     _PAGESZ == 16384
	percent_totalmem_16k_pages = 0;
#endif
	if ((percent_totalmem_16k_pages + percent_totalmem_64k_pages + 
		percent_totalmem_256k_pages +
		percent_totalmem_1m_pages + 
		percent_totalmem_4m_pages + 
		percent_totalmem_16m_pages) > 100) {
			*address = old_value;
			return 1;
	}

	lpage_init_weights();
	for (node = 0; node < numnodes; node++) {
		lpage_init_watermarks(node);
        	orig_pfl = (pg_free_t *)
				&(NODEPDA(0)->node_pg_data.pg_freelst[0]);
		for (tmp_node = 0; tmp_node < numnodes; tmp_node++) {
			/*
			 * The first node has been initialized.
			 */
			if (!node && !tmp_node) continue;
			pfl = (pg_free_t *)&(NODEPDA(node)->
					node_pg_data.pg_freelst[tmp_node]);
			for (i = 0; i < NUM_PAGE_SIZES; i++)
				pfl->hiwat[i]  = orig_pfl->hiwat[i];
		}
	}
	
	/*
	 * Wake up the coalescing daemon. For now wakeup 0 as coalesced
	 * cycles through all nodes.
	 */
	coalesce_daemon_wakeup(0, CO_DAEMON_IDLE_WAIT|CO_DAEMON_TIMEOUT_WAIT|
					CO_DAEMON_FULL_SPEED);
	return 0;
}

/*
 * lpage_split:
 * Split the pfd. It tries to split it down until it gets at least one
 * page of the size rq_page_size_index. Returns a page of the requested
 * size.
 * The algorithm works as follows. It goes through a loop starting at 
 * the current page size and goes down all the way to the required page size.
 * The sizes are computed in terms of page sizes indices. At each page size
 * in the loop, the number of pages that can be freed at that level is 
 * computed. (The variable 'pages_freed_in_this_size' reflects this quantity).
 * This is determined based on the hi water mark and the maximum number of 
 * pages (3 as at least one page needs to be carried over to the next level) 
 * that we can free at the level. Once we go to the next level, the size 
 * decreases by a factor of 4 and the number of available pages increases by 4 
 * (pages_carried_to_next_size). These are computed by using the shift factor. 
 * When the required page size is reached all pages except one is freed into 
 * the free pool. The last page is returned to the caller.
 */

pfd_t *
lpage_split(
	cnodeid_t node,
	pgszindx_t cur_page_size_index,
	pgszindx_t rq_page_size_index, /* Required page size index */
	pfd_t *pfd)
{
	size_t	cur_size = PGSZ_INDEX_TO_SIZE(cur_page_size_index);
	pg_free_t	*pfl = GET_NODE_PFL(node);
	int	hiwat; 	/* Hiwater mark in num pags for cur. page size */
	int	free_pages; /* Free pages in the curr. page size */ 
	int	pages_freed_in_this_size;
	int	pages_carried_to_next_size; 
	int	locktoken;

	locktoken = PAGE_FREELIST_LOCK(node);
	DEC_NUM_TOTAL_PAGES(node, cur_page_size_index);
	PAGE_FREELIST_UNLOCK(node, locktoken);
	if(0) ASSERT(NODE_TOTAL_PAGES(node, cur_page_size_index) >= 0);
	INC_LPG_STAT(node, cur_page_size_index, PAGE_SPLIT_HITS);

	/*
	 * Split the page down to the required size.
	 */
	cur_page_size_index--;
	cur_size >>= PAGE_SIZE_SHIFT;
	pages_carried_to_next_size = PAGES_PER_LARGE_PAGE;

	while (cur_page_size_index >= rq_page_size_index) {

		/*
		 * Calculate the hiwat and see if this page size is needed.
		 */
		hiwat = pfl->hiwat[cur_page_size_index];
		free_pages = GET_NUM_FREE_PAGES(node, cur_page_size_index);
		pages_freed_in_this_size = 0;

		if ( cur_page_size_index == rq_page_size_index)
			pages_freed_in_this_size = 
					pages_carried_to_next_size - 1;
		else if (hiwat > free_pages)
			pages_freed_in_this_size = MIN(hiwat - free_pages, 
						pages_carried_to_next_size -1);
		pages_carried_to_next_size -= pages_freed_in_this_size;

		/*
		 * Split the pfd and insert it into the appropriate
		 * page pool.
		 */
		while ( pages_freed_in_this_size--) {

			locktoken = PAGE_FREELIST_LOCK(node);
			INC_NUM_TOTAL_PAGES(node, cur_page_size_index);
			_pagefree_size(pfd, cur_size, PAGE_FREE_NOCOALESCE);
			PAGE_FREELIST_UNLOCK(node, locktoken);

			/*
			 * Skip to the next pfdat
			 */
			pfd += btoc(cur_size);

			/*
			 * If some process is waiting for this page size
			 * wake it up.
			 */
			lpage_size_broadcast(cur_page_size_index);
		}

		/*
		 * Goto the next size.
		 */
		cur_page_size_index--;
		cur_size >>= PAGE_SIZE_SHIFT;
		pages_carried_to_next_size <<= PAGE_SIZE_SHIFT;
	}

	/*
	 * Initialize the page
	 */
	lpage_init_pfdat(pfd, cur_size, PAGE_ALLOC);

	return (pfd);
}

/*
 * lpage_coalesce:
 * Test if the page pfd can be coalesced to a larger page. If it can be 
 * coalesced then coalesce it. This is called from page_free.
 */

void
lpage_coalesce(
	cnodeid_t node, 
	pfd_t *pfd)
{
	int		pindx, cur_pindx;
	pg_free_t	*pfl = GET_NODE_PFL(node);
	bitmap_t	bm;
	pfd_t		*lpfd;
	int		bit, numpages;
	/*REFERENCED*/
	pgno_t		lpfn;

	/*
	 * For every page size try coalescing to that size. If not try
	 * a lower size. Dont try coalescing to a page size smaller than the 
	 * current page size.
	 */

	cur_pindx = PFDAT_TO_PGSZ_INDEX(pfd);
	for (pindx =  MAX_PGSZ_INDEX; pindx >= 1 && pindx > cur_pindx; pindx--) {
		/*
		 * Check if we need pages of this size.
		 */
		if (pfl->hiwat[pindx] > GET_NUM_FREE_PAGES(node, pindx)) {
			bm = pfdat_to_pure_bm(pfd);
			bit = pfdat_to_bit(pfd);
			numpages = PGSZ_INDEX_TO_CLICKS(pindx);
			INC_LPG_STAT(node, pindx, PAGE_COALESCE_ATT);
			/*
			 * Do a bit comparison to see if we can coalesce
			 * to this size.
			 */
			if (bftstset(bm, (bit &(~(numpages-1))), 
						numpages) == numpages) {
				lpfd = pfdat_to_large_pfdat(pfd, numpages);
				lpfn = pfdattopfn(lpfd);
				if ( lpfd < pfl->pfd_low) 
					continue;
				if ((lpfd + numpages) > pfl->pfd_high)
					continue;
				/* We can do coalescing */
				INC_LPG_STAT(node, pindx, PAGE_COALESCE_HITS);

				/*
				 * Assemble this list into a larger page.
				 */
				lpage_assemble(node, lpfd, pindx);

				/*
				 * Wake up processes waiting for this
				 * page size.
				 */
				lpage_size_broadcast(pindx);
				return;
			}
		}
	}
}

/*
 * lpage_assemble:
 * Assemble a bunch of pages to a larger page. The pages in this
 * chunk should not have any cached pages as we come here after looking
 * at the pure bitmap. Hence we don't go through a loop trying to remove
 * pages from the page cache. 
 * Note that some of the pages of the chunk can be big pages themselves.
 * The code sets the sentinel page index on all
 * the pages in the chunk. If one of the pages in the chunk is a big page
 * we need to set the sentinel page index only on the starting pfdat of that 
 * page. The other pfdats of that big page already have the sentinel page 
 * index.
 */

static void
lpage_assemble(
	cnodeid_t node, 
	pfd_t *lpfd, 
	int pindx)
{
	int		numbits = PGSZ_INDEX_TO_CLICKS(pindx);
	pfd_t		*pfd = lpfd;
	pgno_t		clicks; 
	pfd_t		*pfree_list = NULL;
	int		list_type;

	list_type = STALE_NOASSOC;
	while (numbits > 0) {
		clicks = PFDAT_TO_PAGE_CLICKS(pfd);
		ASSERT(clicks != 0);

		/*
		 * Remove this page from the free list.
		 */
		lpage_remove(node, pfd);

		/*
		 * Assert that this pfd is not in the page cache.
		 * We are looking at the pure bitmap now.
		 */
		ASSERT(!(pfd->pf_flags & P_HASH));

		/*
		 * Insert into the remove list.
		 */
		pfd->pf_next = pfree_list;
		pfree_list = pfd;

#if	EVEREST || SN0
		if ( pfd->pf_flags & (P_POISONED|P_HAS_POISONED_PAGES))
			list_type = POISONOUS;
#endif
		nested_pfdat_lock(pfd);
		SET_PAGE_SIZE_INDEX(pfd, SENTINEL_PGSZ_INDEX);
		nested_pfdat_unlock(pfd);
		numbits -= clicks;
		pfd += clicks;
	}

	/*
	 * Clear the previous page size index and set a new index.
	 */
	nested_pfdat_lock(lpfd);
	SET_PAGE_SIZE_INDEX(lpfd, pindx);
#if	 EVEREST || SN0
	if (list_type == POISONOUS) 
		pfd_nolock_setflags(lpfd, P_HAS_POISONED_PAGES);
#endif
	nested_pfdat_unlock(lpfd);
	lpage_insert(node, lpfd, list_type);
	ASSERT(check_freemem());
}


/*
 * lpage_size_timed_wait:
 * Wait on the appropriate synchronization variable for a given page size.
 * This might be called with any size. PAGE_SIZE_TO_INDEX macro rounds it
 * up to the next appropriate page size index. The wait is timed. If a timeout
 * expires or a signal interrupts the sleep the function returns a non-zero
 * value.
 */
int
lpage_size_timed_wait(
	size_t size, 
	int pri,
	timespec_t *wait_ts)
{
	int		spl;
	pgszindx_t	page_size_index = PAGE_SIZE_TO_INDEX(size);

	spl = mutex_spinlock(&co_daemon_sync.lock);
	co_daemon_sync.pgsz_wait_flags |= (1 << page_size_index);
	return (sv_timedwait_sig_notify(
		&co_daemon_sync.page_size_wait[page_size_index],
			pri, &co_daemon_sync.lock, spl, 0, wait_ts, 0));
}

/*
 * lpage_size_broadcast:
 * Wakeup all the process sleeping for a page of  a particular page size.
 * All processes sleeping on page sizes lower or equal to the page size
 * are woken up.
 */
void
lpage_size_broadcast(
	pgszindx_t page_size_index)
{
	int		flag;
	int		spl;

	spl = mutex_spinlock(&co_daemon_sync.lock);
	for (; page_size_index >= MIN_PGSZ_INDEX; page_size_index--) {
		flag = 1 << page_size_index;	
		if (co_daemon_sync.pgsz_wait_flags & flag) {
			co_daemon_sync.pgsz_wait_flags &= ~flag;
			sv_broadcast(&co_daemon_sync.page_size_wait[
							page_size_index]);
		}
	}
	mutex_spinunlock(&co_daemon_sync.lock, spl);
}

/* 
 * lpage_get_max_pindx:
 * Return the maximum page size that can be formed beginning at this page.
 */
static pgszindx_t
lpage_get_max_pindx(
	pfd_t *pfd, 
	pgszindx_t max_pgsz_indx)
{
	int bit = pfdat_to_bit(pfd);
	pgszindx_t	i;

	for ( i = max_pgsz_indx; i; i--)
		if (!(bit % PGSZ_INDEX_TO_CLICKS(i))) return (i);
	return 0;
}

/*
 * lpage_lower_pagesize:
 * Return the next lower page size which is available.
 */
size_t
lpage_lower_pagesize(size_t page_size)
{
	pgszindx_t	cur_page_size_index = PAGE_SIZE_TO_INDEX(page_size) - 1;
	pgszindx_t	tmp_page_size_index;

	for (tmp_page_size_index = MAX_PGSZ_INDEX; 
		tmp_page_size_index > MIN_PGSZ_INDEX; tmp_page_size_index--) {
		if (page_size_weight[tmp_page_size_index])
			break;
	}

	if (tmp_page_size_index > cur_page_size_index)
		return PGSZ_INDEX_TO_SIZE(cur_page_size_index);
	else return PGSZ_INDEX_TO_SIZE(tmp_page_size_index);
}

/*
 * lpage_release_held_pages:
 * Free the pages  that are held in pfreelist and pmig_list. These are freed
 * back into the old pool as we could not construct a large page.
 */
static void
lpage_release_held_pages(
	cnodeid_t node, 
	pfd_t *pmig_list, 
	pfd_t *pfree_list)
{
	pfd_t *pfd, *pfnext;
	int	list_type = STALE_NOASSOC;
	int	locktoken;

	locktoken = PAGE_FREELIST_LOCK(node);

	for (pfd = pfree_list; pfd; pfd = pfnext) {
		pfnext = pfd->pf_next;
#if	EVEREST || SN0
		if ( pfd->pf_flags & (P_POISONED|P_HAS_POISONED_PAGES))
			list_type = POISONOUS;
#endif
		lpage_insert(node, pfd, list_type);
	}
	for (pfd = pmig_list; pfd; pfd = pfnext ) {
		pfnext = pfd->pf_next;
		pfd->pf_use = 1; /* So that page free will not complain */
		_pagefree_size(pfd, NBPP, 0);
	}
	ASSERT(check_freemem());
	PAGE_FREELIST_UNLOCK(node, locktoken);
}

/*
 * lpage_construct:
 * Construct a large page of page index pindx using pages from pfree_list
 * and pmig_list. The page starts with pfd start_pfd. 
 * PAGE_FREELIST_LOCK() should be held when calling this page.
 * return FALSE if the construction is not successful.
 */
static void
lpage_construct(
	cnodeid_t node, 
	pfd_t *start_pfd, 
	pgszindx_t pindx, 
	pfd_t *pmig_list, 
	pfd_t *pfree_list)
{
	pfd_t	*pfd, *pfnext;
	int	list_type;
	int	locktoken;

	locktoken = PAGE_FREELIST_LOCK(node);

	/*
	 * Clear the page sizes of the pfds.
	 */
	list_type = CLEAN_NOASSOC;
	for (pfd = pfree_list; pfd; pfd = pfnext ) {
		SET_PAGE_SIZE_INDEX(pfd, SENTINEL_PGSZ_INDEX);

#if	EVEREST || SN0
		if ( pfd->pf_flags & (P_POISONED|P_HAS_POISONED_PAGES))
			list_type = POISONOUS;
#endif
		ASSERT(!(pfd->pf_flags & P_QUEUE));
		pfnext = pfd->pf_next;
		pfd->pf_next = NULL;
	}
	for (pfd = pmig_list; pfd; pfd = pfnext ) {

#if	EVEREST || SN0
		if (pfd->pf_flags & P_POISONED)
			list_type = POISONOUS;
#endif
		pfnext = pfd->pf_next;
		SET_PAGE_SIZE_INDEX(pfd, SENTINEL_PGSZ_INDEX);
		pfd->pf_next = NULL;
	}
	INC_LPG_STAT(node, pindx, CO_DAEMON_SUCCESS);
	SET_PAGE_SIZE_INDEX(start_pfd, pindx);

#if	 EVEREST || SN0
	if (list_type == POISONOUS) 
		pfd_nolock_setflags(start_pfd, P_HAS_POISONED_PAGES);
#endif
	lpage_insert(node, start_pfd, list_type);

	/*
	 * Wake up threads waiting for this page size.
	 */
	lpage_size_broadcast(pindx);
	PAGE_FREELIST_UNLOCK(node, locktoken);
}




/*
 * lpage_release_buffer_reference:
 * Release the buffer cache reference for a page. Note that we have
 * to first get a reference to the vnode before can  call chunkpush
 * to get rid of the buffer.
 */
int
lpage_release_buffer_reference(
	pfd_t *pfd, 
	int is_vnode_locked)
{
	int	err;
	int	s;
	vnode_t	*vp;
	vmap_t	vmap;

	/*
	 * First get a reference on the vnode before we call chunkpush.
	 */

	if ((pfd->pf_flags & (P_ANON|P_HASH)) != P_HASH)
		return EINVAL;
	s = pfdat_lock(pfd);

	if ((pfd->pf_flags & (P_ANON|P_HASH)) != P_HASH) {
		pfdat_unlock(pfd, s);
		return EINVAL;
	}

	vp = pfd->pf_vp;
	VMAP(vp, vmap);	
	pfdat_unlock(pfd, s);
	vp = vn_get(vp, &vmap, VN_GET_NOWAIT);

	/*
 	 * If we cannot get a reference return failure.
	 */
	if (!vp) return 1;
	if (!is_vnode_locked) VOP_RWLOCK(vp, VRWLOCK_WRITE);
	err = chunkpush(vp, ctooff(pfd->pf_pageno), ctooff(pfd->pf_pageno + 1) - 1,
								B_STALE);
	if (!is_vnode_locked) VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
	VN_RELE(vp);
	if (err == 0) {
		err = migr_check_migratable(pfdattopfn(pfd));
	}

	return err;
}

/*
 * lpage_hold_free_pages:
 * Remove all the free pages that are in the chunk and chain
 * them as a singly linked list in pfree_list. Start_pfd is the 
 * starting page of the chunk.
 * PAGE_FREELIST_LOCK() is held across this routine. 
 * Returns -1 if a page cannot be removed from the free list.
 * Otherwise returns total number of base(size == NBPP) pages on the pfree_list
 */
static int
lpage_hold_free_pages(
	cnodeid_t node, 
	pfd_t *start_pfd, 
	bitmap_t bm, 
	pgszindx_t pindx, 
	pfd_t **pfree_list)
{
	pgno_t	clicks;
	int	start_chunk = pfdat_to_bit(start_pfd);
	int	numbits = PGSZ_INDEX_TO_CLICKS(pindx);
	int	end_chunk = start_chunk + numbits;
	int	i;
	pfd_t	*pfd; 
	int	num_free_pages = 0;
	int	error_page = 0;
	int	locktoken;

	pfd = start_pfd;

	locktoken = PAGE_FREELIST_LOCK(node);

	/* If this page is part of a bigger page just bail out */
	if (PFDAT_TO_PGSZ_INDEX(start_pfd) == SENTINEL_PGSZ_INDEX) {
		PAGE_FREELIST_UNLOCK(node, locktoken);
		return FALSE;
	}

	for (i = start_chunk; i < end_chunk;i += clicks) {
		clicks = PFDAT_TO_PAGE_CLICKS(pfd);
		if (btst(bm, i)) {

			/*
			 * Remove the page from the free list.
			 * Hold the pfdat lock and check if the page still
			 * has an association with the object. If not nothing
			 * needs to be done.
	 		 */
			nested_pfdat_lock(pfd);
			lpage_remove(node,pfd);

			/*
			 * If the page has page cache association then
			 * remove it from page cache.
			 */
			if (pfd->pf_flags & (P_HASH|P_ANON)) {
				lpage_cache_remove(pfd, locktoken);
				locktoken = PAGE_FREELIST_LOCK(node);
			} else 
				nested_pfdat_unlock(pfd);

			ASSERT(!(pfd->pf_flags & P_HASH));
			ASSERT(pfd->pf_hchain == NULL);
			ASSERT(pfd->pf_use == 0);
			if (pfdat_iserror(pfd)) {
				error_page = 1;
				break;
			}

			/*
			 * Insert into the remove list.
			 */
			pfd->pf_next = *pfree_list;
			*pfree_list = pfd;
			num_free_pages += clicks;
		}
		pfd += clicks;
	}
	PAGE_FREELIST_UNLOCK(node, locktoken);

	if (error_page) {
		if (page_error_clean(pfd) == 0) 
		    page_discard_enqueue(pfd);
	}

	return num_free_pages;
}


/*
 * lpage_needed:
 * Returns TRUE if large pages are needed for this node.
 */
/* ARGSUSED */
static int
lpage_needed(
	cnodeid_t node, 
	pg_free_t *pfl)
{
	pgszindx_t	pindx;

	for ( pindx = 1; pindx < NUM_PAGE_SIZES; pindx++) {
		if (pfl->hiwat[pindx] > GET_NUM_FREE_PAGES(node, pindx))
				return TRUE;
	}
	return (PAGE_SIZE_HAS_WAITERS());
}


/*
 * lpage_low_on_memory:
 * Check to see if we are low on large pages. We check the low
 * watermark which is 10% of the high watermark. 
 * Basically the routine checks to see if there are enough pages
 * in the base page size and other sizes which are not needed anymore
 * (eg., page sizes which have been turned off by dynamically resetting the 
 * tuneable parameters) to satisfy at least 10% of the hiwatermark of atleast
 * one of the needed page sizes.  If there is not enough memory than we skip 
 * the node.
 */ 
/* ARGSUSED */
static int
lpage_low_on_memory(
	cnodeid_t node, 
	pg_free_t *pfl)
{
	pgszindx_t	pindx;
	pgno_t		num_base_pages;
	pgno_t		num_needed_pages, num_available_pages;

	num_base_pages = 1  << PAGE_SIZE_SHIFT;
	num_needed_pages = 0;

	num_available_pages = GET_NUM_FREE_PAGES(node, 0);

	for ( pindx = 1; pindx < NUM_PAGE_SIZES; pindx++, 
					num_base_pages <<= PAGE_SIZE_SHIFT) {
		if (pfl->hiwat[pindx]) {
			if (num_needed_pages)
				num_needed_pages = MIN(num_needed_pages, 
				GET_LPAGE_LOWAT(pfl, pindx) * num_base_pages);
			else num_needed_pages = 
				GET_LPAGE_LOWAT(pfl, pindx) * num_base_pages;
		} else num_available_pages += 
			GET_NUM_FREE_PAGES(node, pindx) * num_base_pages;
	}
	return (num_available_pages <= num_needed_pages);
}

/*
 * lpage_insert:
 * Insert  a specfic page into the free list. list_type points to the
 * type of list where it will be inserted.
 */

/* ARGSUSED1 */
static void
lpage_insert(
	cnodeid_t node, 
	pfd_t *pfd, 
	int list_type)
{
	pgszindx_t	indx = PFDAT_TO_PGSZ_INDEX(pfd);
	phead_t		*pheadp = PFD_TO_PHEAD(pfd, indx);

	ASSERT(is_valid_pfdat(node, pfd));

	/*
	 * If the page is not free don't insert into the free list.
	 * This can happen if the daemon removes a list of pages from the
	 * free list and fails when trying to remove them from the hash list.
	 */
	ASSERT(pfd->pf_use == 0);
	if (pfd->pf_use > 0) return;

#if	EVEREST || SN0
	if (list_type == POISONOUS)
		pheadp->ph_poison_count++;
#endif
	append(&pheadp->ph_list[list_type], pfd);
	pheadp->ph_count++;
	nested_pfdat_lock(pfd);
	pfd->pf_flags |= P_QUEUE;
	nested_pfdat_unlock(pfd);

#ifdef TILES_TO_LPAGES
	if (indx == MIN_PGSZ_INDEX)
		TILE_TPHEAD_INC_PAGES(pfd);
#endif /* TILES_TO_LPAGES */
	INC_NUM_TOTAL_PAGES(node, indx);
	INC_NUM_FREE_PAGES(node, indx);
	ADD_NODE_FREEMEM(node, PGSZ_INDEX_TO_CLICKS(indx));
	ADD_NODE_EMPTYMEM(node, PGSZ_INDEX_TO_CLICKS(indx));

        /*
         * We are *not* updating GLOBAL_FREEMEM here. GLOBAL_FREEMEM
         * is updated only once a second by the clock maintanance
         * routine, or when needed by vhand.
         */

	ASSERT(check_freemem());
	set_pfd_bitfield(pfdat_to_pure_bm(pfd), pfd, 
					PGSZ_INDEX_TO_CLICKS(indx));
	set_pfd_bitfield(pfdat_to_tainted_bm(pfd), pfd,
					PGSZ_INDEX_TO_CLICKS(indx));

	TILE_PAGEUNLOCK(pfd, PGSZ_INDEX_TO_CLICKS(indx));
}

/*
 * lpage_remove:
 * Remove a page from the free list. Assumes that PAGE_FREELIST_LOCK is held 
 * when this routine is called.
 */

/* ARGSUSED1 */
static void
lpage_remove(
	cnodeid_t node, 
	pfd_t *pfd)
{
	int	indx = PFDAT_TO_PGSZ_INDEX(pfd);

	ASSERT(is_valid_pfdat(node, pfd));
	pagedequeue(pfd, PFD_TO_PHEAD(pfd, indx));

	DEC_NUM_FREE_PAGES(node, indx);
	DEC_NUM_TOTAL_PAGES(node, indx);
	if(0) ASSERT(NODE_TOTAL_PAGES(node, indx) >= 0);
	SUB_NODE_FREEMEM(node, PGSZ_INDEX_TO_CLICKS(indx));

        /*
         * We are *not* updating GLOBAL_FREEMEM here. GLOBAL_FREEMEM
         * is updated only once a second by the clock maintanance
         * routine, or when needed by vhand.
         */

	clr_pfd_bitfield(pfdat_to_pure_bm(pfd), pfd, 
				PGSZ_INDEX_TO_CLICKS(indx));
	clr_pfd_bitfield(pfdat_to_tainted_bm(pfd), pfd,
				PGSZ_INDEX_TO_CLICKS(indx));

	TILE_PAGELOCK(pfd, PGSZ_INDEX_TO_CLICKS(indx));

	ASSERT(check_freemem());
}

/*
 * lpage_cache_remove:
 * Disassociate the page from the cache. The free list lock is held while
 * calling into this routine. It is also assumed that the page has been
 * removed from the freelist. This routine unlocks the freelist lock.
 */

static void
lpage_cache_remove(
	pfd_t *pfd, 
	int locktoken)
{
	void	*tag;
	int	anon_tag;

	/*
	 * pfd is out of free list.
	 * If it's hashed (either on vnode or anon page cache)
	 * invoke the proper sequence of operations to grab the
	 * page for ourselves.
	 * Look in page.c _pagealloc_size_node code for detailed comments
	 * about this sequence of operation.
	 */
	if (pfd->pf_flags & P_HASH){
		pfd->pf_flags |= P_RECYCLE;
		tag = pfd->pf_tag;

		if (pfd->pf_flags & P_ANON) {
			anon_hold((anon_hdl)tag);
			anon_tag = 1;
		} else {
			VNODE_PCACHE_INCREF((vnode_t *)tag);
			anon_tag = 0;
		}
	}

	nested_pfdat_unlock(pfd);
	PAGE_FREELIST_UNLOCK(pfdattocnode(pfd), locktoken);
	if (pfd->pf_flags & P_RECYCLE) {
		if (anon_tag) 
			anon_recycle((anon_hdl)tag, pfd);
		else
			vnode_page_recycle((vnode_t *)tag, pfd);
		
		ASSERT((pfd->pf_flags & (P_ANON|P_HASH|P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);
	}
}

/*
 * lpage_init_pfdat:
 * Initializes the pfdat. pstate says if the call is from pagealloc or
 * pagefree. In the alloc case the pfdat flags are initialized to sane values
 * and the page size field is cleared. If the call is from pagefree() then
 * the page size index is set in the first pfdat and sentinel index is set in
 * the other pfdats.
 */

void
lpage_init_pfdat(
	pfd_t *pfd, 
	size_t size, 
	pageinit_t pinit)
{
	int		num_pages = btoc(size);
	int		i;
	pfd_t		*lpfd = pfd; /* First pfdat of the large page */

	if (pinit == PAGE_ALLOC) {
		for ( i = 0; i < num_pages; i++, pfd++) {
			pfd->pf_next = pfd->pf_prev = NULL;
			pfd->pf_flags = P_CACHESTALE;
			pfd->pf_pageno = PGNULL;
			pfd->pf_vp = 0;
			pfd->pf_use = 1;
			pfd->pf_rawcnt = 0;
			CLR_PAGE_SIZE_INDEX(pfd);
		}
	} else {
		for (i = 0; i < num_pages; i++, pfd++) {
			SET_PAGE_SIZE_INDEX(pfd, SENTINEL_PGSZ_INDEX);
			pfd->pf_use = 0;
		}
		SET_PAGE_SIZE_INDEX(lpfd, PAGE_SIZE_TO_INDEX(size));
	}	
}


/*
 * lpage_size_to_index:
 * Routine to convert page size to page index.
 */

int
lpage_size_to_index(
	size_t size)
{
	pgszindx_t	page_size_index;
	int		tmp_size;

	for (page_size_index = 0, tmp_size = NBPP;  tmp_size < size;
						tmp_size <<= PAGE_SIZE_SHIFT)
		page_size_index++;
	return page_size_index;
}

/*
 * lpage_alloc_contig_physmem:
 * Allocate contiguous pages. Called from kmem_contig_alloc to allocate
 * large pages for drivers.
 */
pfd_t *
lpage_alloc_contig_physmem(
	cnodeid_t	node,
	size_t		requested_size, /* Aligned to a page */
	pgno_t		alignment, 	/* Number of pages */
	int		flags)
{
	pgszindx_t	page_size_index;
	pfd_t		*pfd;
	pgno_t		pfn;
	size_t		actual_size;
	
	page_size_index = PAGE_SIZE_TO_INDEX(requested_size);

	/*
	 * Check to see if we can allocate a large page from one of the
	 * standard lists and chop it up to the right size.
	 * Otherwise we call contmemall_node which scans every pfdat on
	 * the node to see if we can form a large page. This is necessary to
	 * form very large pages > 16MB.
	 */
	if (page_size_index < NUM_PAGE_SIZES
#ifdef MH_R10000_SPECULATION_WAR
		&& (!IS_R10000() || !(flags & VM_DIRECT))
#endif /* MH_R10000_SPECULATION_WAR */
	) {
		actual_size = PGSZ_INDEX_TO_SIZE(page_size_index);

		/*
		 * If there are no alignment restrictions or alignment is of a 
		 * lower page size compared to page size we are going to 
		 * allocate then any page of a specific size will suffice. So 
		 * use fast path and call pagealloc_size_node.
		 */
		if (!(flags & VM_DIRECT) && (!alignment || 
					     (alignment <= PGSZ_INDEX_TO_CLICKS(page_size_index))))
			pfd = pagealloc_size_node(node, 0, VM_UNSEQ, 
								actual_size);
		else {
			pfd = lpage_alloc_aligned_page(node, &page_size_index, 
							alignment, flags);
			actual_size = PGSZ_INDEX_TO_SIZE(page_size_index);
		}

		/*
		 * If a pfdat was available and the requested size is less 
		 * than the size of the allocated page split the page to 
 		 * requested size.
		 */
		if (pfd ) {
			if(requested_size != actual_size)
				large_page_chop(pfd, requested_size, 
								actual_size);
			return pfd;
		}
	}
	pfn = contmemall_node(node, btoct(requested_size), alignment, flags);
	if (pfn) pfd = pfntopfdat(pfn);
	else pfd = NULL;
	return pfd;
}

/*
 * Check to see if pfd is aligned to the right alignment. 
 * alignment is in number of pages and should be a power of 2.
 */

#define	PFD_ALIGNED(pfd, alignment) \
		((pfdattopfn(pfd) & ~(alignment - 1)) == pfdattopfn(pfd))

/*
 * lpage_alloc_aligned_page:
 * Goes through all the pages of a particular page size and gets one that 
 * satisfies alignment and VM_DIRECT requirements. If no page can be found
 * try the next higher page size. Returns the pfd the new page_size_index is 
 * passed back in page_size_indexp.
 */

static pfd_t *
lpage_alloc_aligned_page(
	cnodeid_t 	node, 
	pgszindx_t 	*page_size_indexp, 
	int 		alignment, 
	int 		flags)
{
	pg_free_t	*pfl;
	phead_t		*pheadend, *pheadp;
	pfd_t		*pfd, *sentinel_pfd;
	int		locktoken;
	pgszindx_t	page_size_index;
	int		wakeup_coalesced=0;
	int		i;

	locktoken = PAGE_FREELIST_LOCK(node);
	pfl = GET_NODE_PFL(node);
	for (page_size_index = *page_size_indexp; 
			page_size_index < NUM_PAGE_SIZES; page_size_index++) {
		if (GET_NUM_FREE_PAGES(node, page_size_index) == 0) {
			wakeup_coalesced = 1;
			continue;
		}
		pheadend = pfl->pheadend[page_size_index];
		pheadp = &(pfl->phead[page_size_index][0]);
		for (; pheadp < pheadend; pheadp++) {
			if (!pheadp->ph_count) continue;
			for ( i = 0; i < PH_NLISTS; i++) {
				pfd = pheadp->ph_list[i].pf_next;
				sentinel_pfd = (pfd_t *)(&pheadp->ph_list[i]);
				for (; pfd != sentinel_pfd; 
							pfd = pfd->pf_next) {
					if ((flags & VM_DIRECT) 
							&& (pfd > pfd_direct))
						continue;
					/*
					 * Found a page. Remove it from the
					 * freelist and return it.
					 */
					if (PFD_ALIGNED(pfd, alignment)) {
						nested_pfdat_lock( pfd );
						lpage_remove(node, pfd);
						lpage_cache_remove(pfd,
								locktoken);
						lpage_init_pfdat(pfd, 
							PGSZ_INDEX_TO_SIZE(
						       page_size_index),
							PAGE_ALLOC);
						*page_size_indexp = 
							page_size_index;
						goto done;
					}
				}
			}
		}
	}
	PAGE_FREELIST_UNLOCK(node, locktoken);
	pfd = NULL;
	
done:
	/*
	 * The call to coalesce_daemon_wakeup cannot be done with the page free
	 * list lock held: 
	 *	coalesce_daemon_wakeup requires a spinlock.
	 *
	 * If the daemon is going to sleep:
	 *	coalesce_daemon_sleep holds this spinlock & may try to grab the 
	 *		freelist lock (via call to timeout if callout queue 
	 *		is empty
	 *
	 * This causes a AB/BA deadlock.
	 */
	if (wakeup_coalesced)
		coalesce_daemon_wakeup(node, CO_DAEMON_IDLE_WAIT|
				CO_DAEMON_TIMEOUT_WAIT|CO_DAEMON_FULL_SPEED);
	return pfd;
}

/*
 * lpage_free_contig_physmem:
 * Free contiguous physical memory of a specific size. It tries to 
 * free the memory into large chunks as possible.
 */

void
lpage_free_contig_physmem(pfd_t *pfd, size_t size)
{
	large_page_chop(pfd, 0, size);
}
	
/*
 * large_page_chop:
 * Chop down a page chunk of actual_size to requested_size and free the rest.
 * The algorithm tries to free the rest of the large page in as large chunks
 * as possible. So if requested_size is 0, it will free the entire chunk.
 */
static void
large_page_chop(
	pfd_t	*lpfd,
	size_t	requested_size, 
	size_t	actual_size)
{
	pfd_t		*pfd, *epfd;
	pgszindx_t	max_possible_pgindx, page_size_index;
	pg_free_t 	*pfl;
	int		locktoken;

	/*
	 * Get the first pfdat of the remainder.
	 */
	pfd = lpfd + btoc(requested_size);
	epfd = lpfd + btoc(actual_size);
	if (actual_size <= NBPP) {
		pagefree_size(pfd, NBPP);
		return;
	}
	/*
	 * Compute the maximum possible page size index for pfd. It is the
	 * next page size index to actual_size. 
	 */
	while (pfd < epfd) {
		max_possible_pgindx = PAGE_SIZE_TO_INDEX(actual_size);
		if (PGSZ_INDEX_TO_SIZE(max_possible_pgindx) > actual_size)
			max_possible_pgindx--;
		if (max_possible_pgindx > MIN_PGSZ_INDEX) {
			page_size_index = lpage_get_max_pindx(pfd, 
							max_possible_pgindx);
			pfl = GET_NODE_PFL(pfdattocnode(pfd));
			if (pfl->hiwat[page_size_index] <=
			    GET_NUM_FREE_PAGES(pfdattocnode(pfd),
					       page_size_index))
				page_size_index = MIN_PGSZ_INDEX;
		} else page_size_index = MIN_PGSZ_INDEX;

		locktoken = PAGE_FREELIST_LOCK(pfdattocnode(pfd));
		INC_NUM_TOTAL_PAGES(pfdattocnode(pfd), page_size_index);
		_pagefree_size(pfd, PGSZ_INDEX_TO_SIZE(page_size_index), 0);
		PAGE_FREELIST_UNLOCK(pfdattocnode(pfd), locktoken);
		pfd += PGSZ_INDEX_TO_CLICKS(page_size_index);
		actual_size -= PGSZ_INDEX_TO_SIZE(page_size_index);
	}

	/*
	 * We should have freed all the pages in the large page and no other
	 * pages.
	 */
	ASSERT(pfd == epfd);
}

/*
 * lpage_pre_allocate_pages:
 * Called by lpage_reserve to allocate num_pages of page size 'page_size'.
 * It uses contmemall() to reserve the pages. 
 */
static void
lpage_pre_allocate_pages(pgno_t num_pages, size_t page_size)
{
	pgno_t	pfn;
	pfd_t	*pfd, *start_pfd, *next_pfd;
	pgno_t	num_requested_pages = num_pages;

	start_pfd = NULL;
	while (num_pages) {
		pfn = 	contmemall(btoct(page_size), btoct(page_size), 
						VM_NOSLEEP);
		if (pfn == NULL) break;
		pfd = pfntopfdat(pfn);

		/*
		 * Chain them here before freeing them into the freelist.
		 * Otherwise contmemall will reallocate them.
		 */
		if (start_pfd != NULL) {
			pfd->pf_next = start_pfd;
			start_pfd = pfd;
		} else  {
			start_pfd = pfd;
			pfd->pf_next = NULL;
		}
		num_pages--;
	}


	pfd = start_pfd;
	while (pfd) {
		next_pfd = pfd->pf_next;
		INC_NUM_TOTAL_PAGES(pfdattocnode(pfd), PAGE_SIZE_TO_INDEX(page_size));
		_pagefree_size(pfd, page_size, PAGE_FREE_NOCOALESCE);
		pfd = next_pfd;
	}

	cmn_err(CE_NOTE,"Reserved %d pages of %d bytes each",
				num_requested_pages - num_pages, page_size);
}

/*
 * lpage_reserve:
 * Reserve large pages based on tunable parameters.
 */
void
lpage_reserve(void)
{
	pgno_t	num_pages;
	pgszindx_t	page_size_index;

	if (!(nlpages_16m || nlpages_4m || nlpages_1m || nlpages_256k))
		return;

	/* start with larger pages first */
	num_pages = nlpages_16m;
	page_size_index = MAX_PGSZ_INDEX;
	lpage_pre_allocate_pages(num_pages, PGSZ_INDEX_TO_SIZE(page_size_index));
	num_pages = nlpages_4m;
	page_size_index--;
	lpage_pre_allocate_pages(num_pages, PGSZ_INDEX_TO_SIZE(page_size_index));
	num_pages = nlpages_1m;
	page_size_index--;
	lpage_pre_allocate_pages(num_pages, PGSZ_INDEX_TO_SIZE(page_size_index));
	num_pages = nlpages_256k;
	page_size_index--;
	lpage_pre_allocate_pages(num_pages, PGSZ_INDEX_TO_SIZE(page_size_index));
	num_pages = nlpages_64k;
	page_size_index--;
	lpage_pre_allocate_pages(num_pages, PGSZ_INDEX_TO_SIZE(page_size_index));
}

/*
 * coalesced:
 * The coalesce daemon. It tries to coalesce pages on demand. It goes 
 * through all the nodes sequentially. The coalescing is based on the
 * hiwater mark for that node for that size. If large pages are not needed
 * then the daemon sleeps and can be woken up using coalesce_daemon_wakeup().
 * The node where the daemon will start coalescing attempts can be passed
 * as a parameter to coalesce_daemon_wakeup(). 
 */
#ifndef TILES_TO_LPAGES
void 
coalesced()
{
	int			mild_coalescing_attempts;
	int			coalesce_successful;
	co_daemon_params_t 	co_daemon_parms, *cdp;

	cdp = &co_daemon_parms;
	cdp->timeout_step = 0;
	cdp->start_node = 0;
	mild_coalescing_attempts = 0;

	for(;;) {
		coalesce_successful = TRUE;

		/*
 		 * If pages are in high demand then reduce the timeout 
		 * value to the lowest. Otherwise increase the timeout value
		 * by one step.
		 */
		if (PAGE_SIZE_HAS_WAITERS() || coalesced_precoalesce) {
			cdp->policy = STRONG_COALESCING;
			cdp->timeout_step = 0;
			coalesce_successful = coalesce_engine(cdp);
		} else {
			mild_coalescing_attempts++;
			cdp->policy = LIGHT_COALESCING;
			coalesce_successful = coalesce_engine(cdp);

			/*
			 * If the attempt was not successful try mild
			 * coalescing with some migration.	
			 */
			if (!coalesce_successful) {
				cdp->policy = MILD_COALESCING;
				coalesce_successful = coalesce_engine(cdp);
			}

			/*
			 * If we are not successfull after several mild
		 	 * attempts lets try a strong attempt. Also do
		 	 * strong coalescing if the wakeup call reset the
			 * timeout val. This can happen if wakeup was
			 * due to a config change.
			 */
			if (((mild_coalescing_attempts > 
				MAX_MILD_COALESCING_ATTS) || 
				(cdp->timeout_step == 0)) && 
					(!coalesce_successful)) {
				cdp->policy = STRONG_COALESCING;
				coalesce_successful = coalesce_engine(cdp);
				mild_coalescing_attempts = 0;
			}
		}

		/*
		 * Bump to a higher timeout after each iteration.
		 */
		cdp->timeout_step++;

		if (coalesce_successful) {
			/*
			 * If running low on freememory then wait until we 
			 * get more memory.
			 */
			do {
				coalesce_daemon_sleep(CO_DAEMON_IDLE_WAIT, 0, 
									cdp);
			} while (GLOBAL_FREEMEM() < tune.t_gpgshi);
		}
	}
}

/*
 * coalesce_engine:
 * Routine to start coalescing pages starting with cur_node. It cycles
 * through all nodes in a system.
 */
static int
coalesce_engine(co_daemon_params_t *cdp)
{
	int		i;
	int		coalesce_successful;
	int		timeout_val;
	cnodeid_t 	cur_node;

	/*
	 * Loop through all nodes once. If not successful
	 * timeout and retry again.
	 */

	cur_node = cdp->start_node;

	INC_LPG_STAT(cur_node, 1, CO_DAEMON_ATT);
	coalesce_successful = TRUE;

	/*
	 * Clear up zone bloat.
	 */
	if ((cdp->policy == STRONG_COALESCING) || coalesced_precoalesce)
		kmem_zone_curb_zone_bloat();

	for ( i = 0; i < numnodes; i++) {
		if (!coalesce_node(cur_node, cdp->policy))
			coalesce_successful = FALSE;	
		cur_node++;
		if ( cur_node == numnodes) 
			cur_node = 0;

		timeout_val = 	GET_CO_DAEMON_TIMEOUT(cdp->timeout_step);
		if (timeout_val) {
			/*
			 * If running low on freememory then wait until we get
			 * more memory.
			 */
			do {
				coalesce_daemon_sleep(CO_DAEMON_TIMEOUT_WAIT, 
					timeout_val, cdp);
			} while (GLOBAL_FREEMEM() < tune.t_gpgshi);
		}
	}

	return coalesce_successful;
}
#else
/*
 * coalesced
 *
 * For tiles, we only use a policy of strong because we ask coalesced
 * to run in only 2 situations:  a process is waiting for tiles or we've
 * dipped below the lowater mark for tiles.  The daemon will go back to
 * sleep if it successfully coalesced a 64k page, or if we've exceeded
 * MAX_STRONG_COALESCING_ATTS w/o any luck.  In this latter situation,
 * we queue a wakeup timeout to fire and start over.
 */
#define MAX_STRONG_COALESCING_ATTS	6

int tile_lowat(void);

void 
coalesced()
{
	int			coalesce_successful;
	co_daemon_params_t 	co_daemon_parms, *cdp;
	int			strong_coalescing_attempts;

	cdp = &co_daemon_parms;
	cdp->timeout_step = 0;
	cdp->start_node = 0;
	strong_coalescing_attempts = 0;

	for(;;) {
		coalesce_successful = TRUE;

		/*
 		 * If pages are in high demand then reduce the timeout 
		 * value to the lowest. Otherwise increase the timeout value
		 * by one step.
		 */
		if (PAGE_SIZE_HAS_WAITERS() || tile_lowat()) {
			strong_coalescing_attempts++;
			cdp->policy = STRONG_COALESCING;
			cdp->timeout_step = 0;
			coalesce_successful = coalesce_engine(cdp);
		}

		/*
		 * Bump to a higher timeout after each iteration.
		 */
		cdp->timeout_step++;

		if (coalesce_successful ||
		    strong_coalescing_attempts > MAX_STRONG_COALESCING_ATTS) {
			strong_coalescing_attempts = 0;
			if (coalesce_successful) {
				coalesce_daemon_sleep(CO_DAEMON_IDLE_WAIT,
						      0, cdp);
			} else {
				/*
				 * We've made some number of attempts to
				 * gen some tiles to no avail; better
				 * wake any sleepers now and give them
				 * a chance to run.
				 */
				if (PAGE_SIZE_HAS_WAITERS())
					lpage_size_broadcast(PAGE_SIZE_TO_INDEX(TILE_SIZE));
				coalesce_daemon_sleep(CO_DAEMON_TIMEOUT_WAIT,
					GET_CO_DAEMON_TIMEOUT(cdp->timeout_step), cdp);
			}
		}
	}
}

/*
 * coalesce_engine:
 *
 * For tiles case, we let caller of coalesce_engine worry about putting
 * the daemon to sleep.
 */
static int
coalesce_engine(co_daemon_params_t *cdp)
{
	int		i;
	int		coalesce_successful;
	cnodeid_t 	cur_node;

	/*
	 * Loop through all nodes once. If not successful
	 * timeout and retry again.
	 */

	cur_node = cdp->start_node;

	INC_LPG_STAT(cur_node, 1, CO_DAEMON_ATT);
	coalesce_successful = TRUE;

	/*
	 * Clear up zone bloat.
	 */
	if (cdp->policy == STRONG_COALESCING)
		kmem_zone_curb_zone_bloat();

	for ( i = 0; i < numnodes; i++) {
		if (!coalesce_node(cur_node, cdp->policy))
			coalesce_successful = FALSE;	
	}

	return coalesce_successful;
}
#endif /* TILES_TO_LPAGES */

/*
 * coalesce_node:
 * Try the coalescing algorithm for a particular node. This routine can be
 * called periodically.
 */

static int
coalesce_node(
	cnodeid_t cur_node, 
	int coalescing_policy)
{
	pg_free_t	*pfl;
	pfd_t		*cur_pfd;
	pfd_t		*end_pfd;	/* End pfd of the range */
	pgszindx_t	pindx;
	int		npgs;		/* Number of pages in the range */

	pfl = GET_NODE_PFL(cur_node);
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000())
		/* don't coalesce smallk0 pages */
		cur_pfd = pfd_lowmem;
	else
#endif /* MH_R10000_SPECULATION_WAR */
	cur_pfd = pfl->pfd_low;

	/*
	 * Compute the last pfd of this range.
	 */
	end_pfd = pfdat_probe(cur_pfd, &npgs);
	ASSERT(end_pfd == cur_pfd);
	end_pfd += npgs;
	while (cur_pfd < pfl->pfd_high) {
		if (!lpage_needed(cur_node, pfl) && !coalesced_precoalesce)
			return TRUE;

#ifndef TILES_TO_LPAGES
		if (lpage_low_on_memory(cur_node, pfl)) {
			return FALSE;
		}
#endif /* TILES_TO_LPAGES */

		pindx = lpage_get_max_pindx(cur_pfd, MAX_PGSZ_INDEX);

		/*
		 * If a large sized page can be formed, try
		 * that. Else goto the next page.
		 */
		if (pindx)
			coalesce_chunk(pfl, cur_node, cur_pfd, pindx, end_pfd, 
						coalescing_policy);
		cur_pfd += PGSZ_INDEX_TO_CLICKS(pindx);

		/*
		 * We might have scanned until pfd high in coalesce_chunk.
		 * So break.
		 */
		if (cur_pfd >= pfl->pfd_high)
			break;

		/*
		 * If we hit the end of the pfd range,
		 * go get a new one.
		 */
		if ( cur_pfd >= end_pfd) {

			/*
			 * Compute the last pfd of this range.
			 */
			cur_pfd = pfdat_probe(cur_pfd, &npgs);
			end_pfd = cur_pfd + npgs;
		}
	}
	return FALSE;
}

/*
 * coalesce_chunk:
 * Try to coalesce a chunk of pages starting at start_pfd to a maximum
 * size indicated by page size index pindx.
 */

static void
coalesce_chunk(
	pg_free_t *pfl, 
	cnodeid_t node, 
	pfd_t *start_pfd, 
	pgszindx_t pindx, 
	pfd_t *pfd_high, 
	int coalescing_policy)
{
	int		numbits;
	pfd_t		*end_pfd;
	pgszindx_t	tmp_indx;
	int		npgs;

	/*
	 * If the page is already a large page then just return
	 */
	if (PFDAT_TO_PGSZ_INDEX(start_pfd) >= pindx) {
		return;
	}

	/*
	 * If we need this page size then try to coalesce it else
	 * try a lower size.
	 */
	if (pfl->hiwat[pindx] > GET_NUM_FREE_PAGES(node, pindx)) {
		numbits = PGSZ_INDEX_TO_CLICKS(pindx);
		end_pfd = start_pfd + numbits;
		/*
		 * If coalescing succeeds then returns.
		 */
		if ((end_pfd < pfd_high) &&
			(coalesce_leaf_chunk(node, start_pfd, pindx, 
							coalescing_policy)))
			return;
	}
	tmp_indx = --pindx; /* Try lower size. */

	/*
	 * Find if we need lower page sizes. If not we can bail out
	 * here instead recursing all the way down and realizing that
	 * we don't need any more pages.
	 */
	while (tmp_indx) {
		if (pfl->hiwat[tmp_indx] > 
			GET_NUM_FREE_PAGES(node, tmp_indx)) break;
		tmp_indx--;
	}
	if (!tmp_indx) return; /* We don't need any smaller pages */
	npgs = PAGES_PER_LARGE_PAGE;

	/*
	 * Make a recursive call with a lower page size.
	 */
	INC_LPG_STAT(node, pindx, CO_DAEMON_SPLIT);
	while (npgs--) {
		numbits = PGSZ_INDEX_TO_CLICKS(pindx);
		/*
		 * If this is the smallest chunk possible check to see
		 * if we fit the boundary. The smallest possible chunk
		 * is for the next higher page size than NBPP
		 */
		if (pindx == (MIN_PGSZ_INDEX + 1)) {
			end_pfd = start_pfd + numbits;
			if ( end_pfd > pfd_high) break;
		}
		coalesce_chunk(pfl, node, start_pfd, pindx, pfd_high, 
							coalescing_policy);
		start_pfd += numbits;
		if (start_pfd >= pfd_high) break;
	}
}

/*
 * coalesce_leaf_chunk:
 * Try to coalesce pages starting from start_pfd to the size specified by
 * pindx. Returns 1 on success and 0 on failure. This is done in the following
 * steps.
 * The first few checks are done without any locks. We check to see if the
 * number of pages that are free is above a certain threshold and if so whether
 * the rest of the pages are migratable. If all these conditions are satisfied
 * the daemon acquires the memory lock. It then creates two local lists.
 * It collects all the pages that are free and puts them in the pfree_list
 * The pages that are not free are migrated and put in pmig_list
 * If migration fails the pages in the pfree_list are put back in the free 
 * pool.If everything succeeds the pages in the freelist and migrated list are
 * collected to form a large page.
 */


int
coalesce_leaf_chunk(
	cnodeid_t node, 
	pfd_t *start_pfd, 
	pgszindx_t pindx, 
	int coalescing_policy)
{
	int	start_chunk = pfdat_to_bit(start_pfd);
	int	threshold;
	int	num_bits_set;	/* Number of bits set in the bitmap */
	int	numpages = PGSZ_INDEX_TO_CLICKS(pindx); 
	bitmap_t bm;
	pfd_t	*pfree_list;	/* Local list of free pages */
	int	num_mig_pages;		/* Number of migrated pages */
	pfd_t	*pmig_list;	/* Local list of migrated pages */
	int	num_free_pages; 	/* Number of migrated pages */

	/*
	 * Assert the chunk is correctly aligned.
	 */
	ASSERT((start_chunk &(~(numpages-1))) == start_chunk);

	/*
	 * Coalesce dameon uses the heavy bitmap.
	 */
	bm = pfdat_to_tainted_bm(start_pfd);
	num_bits_set = bfcount(bm, start_chunk, numpages);
	if (coalescing_policy == LIGHT_COALESCING)
		threshold = numpages;
	else if (coalescing_policy == MILD_COALESCING)
		threshold = numbits_threshold[pindx];
	else threshold = 0;

	/*
	 * First check if this is a good chunk to operate on without holding
	 * any locks. If not bail out immediately return FALSE.
	 */
#ifndef TILES_TO_LPAGES
	if (coalescing_policy != TILE_COALESCING) {
#endif /* TILES_TO_LPAGES */
	if (num_bits_set >= threshold) {
		if ( num_bits_set < numpages) 
			if (!migr_coald_test_pagemoves(start_pfd, bm, pindx)) 
					return FALSE;
	} else {
		return FALSE;
	}
#ifndef TILES_TO_LPAGES
	}
#endif /* TILES_TO_LPAGES */
	INC_LPG_STAT(node, pindx, CO_DAEMON_HITS);

	/*
	 * Got a good chunk
	 * Remove all the pages from the free list so
	 * that they don't get allocated. Hold free pages  will remove
 	 * the pages from the free list and their cache association. 
	 */
	pfree_list = NULL;
	num_free_pages = lpage_hold_free_pages(node, start_pfd, bm, pindx, 
					&pfree_list);
	if ( num_free_pages == -1)
		return FALSE;

	pmig_list = NULL;

	/*
	 *  If all pages are not free then try to migrate the rest of the 
	 *  pages. If everything goes ok
	 *  then we recheck the count and coalesce the page. 
	 * Otherwise we free back the old pages and fail the attempt.
	 */
	if (num_free_pages < numpages) {
		num_mig_pages = 
			migr_coald_move_pages(node, start_pfd, bm, pindx,
						&pmig_list);
		if ( num_mig_pages == -1)  {
			lpage_release_held_pages(node, pmig_list, pfree_list);
			return FALSE;
		}

		/*
		 * Check if the entire chunk is free since we have
		 * got the lock. As we have cleared the bits of the
		 * free pages we have to check to see that the remaining bits
		 * are set.
		 */

		if ((num_mig_pages + num_free_pages) < numpages) {
			lpage_release_held_pages(node, pmig_list, pfree_list);
			return FALSE;
		}
	}

	/* All bits are free. Just assemble the pages */
	INC_LPG_STAT(node, pindx, CO_DAEMON_SUCCESS);
#ifdef TILES_TO_LPAGES
	if (coalescing_policy != TILE_COALESCING)
#endif /* TILES_TO_LPAGES */
	lpage_construct(node, start_pfd, pindx, pmig_list, pfree_list);

	return TRUE;
}

/*
 * coalesce_daemon_sleep:
 * Put the coalescing daemon to sleep.
 */

static void 
coalesce_daemon_sleep(
	int flags, 
	int timeout_val, co_daemon_params_t  *cdp)
{
	int		spl;

	spl = mutex_spinlock(&co_daemon_sync.lock);

	if (flags & CO_DAEMON_TIMEOUT_WAIT) {
		co_daemon_sync.timeout_id = 
			timeout((void (*)())coalesce_daemon_wakeup, 
			(void *)0, timeout_val, CO_DAEMON_TIMEOUT_WAIT
					|CO_DAEMON_TIMEOUT_EXPIRED);
	}

	co_daemon_sync.flags |= 
		flags & (CO_DAEMON_TIMEOUT_WAIT|CO_DAEMON_IDLE_WAIT);
	sv_wait(&co_daemon_sync.daemon_wait, PZERO|PLTWAIT, 
						&co_daemon_sync.lock, spl);

	/*
	 * Get the node to coalesce.
	 */
	spl = mutex_spinlock(&co_daemon_sync.lock);

	cdp->start_node = co_daemon_sync.start_node;
	if (co_daemon_sync.flags & CO_DAEMON_FULL_SPEED) {
		cdp->timeout_step = 0;
		co_daemon_sync.flags &= ~CO_DAEMON_FULL_SPEED;
	}

	mutex_spinunlock(&co_daemon_sync.lock, spl);
}

/*
 * coalesce_daemon_wakeup:
 * Wakeup the coalescing daemon. The node passed will be tried first by
 * the coalescing daemon.
 */

void
coalesce_daemon_wakeup(
	cnodeid_t start_node, 
	int flags)
{
	int spl;
	int	wait_flags;

	spl = mutex_spinlock(&co_daemon_sync.lock);	

	wait_flags = flags & (CO_DAEMON_TIMEOUT_WAIT|CO_DAEMON_IDLE_WAIT);
	if (co_daemon_sync.flags & wait_flags) {
		co_daemon_sync.start_node = start_node;
		sv_signal(&co_daemon_sync.daemon_wait);

		/*
		 * If not called from timeotu clear pending timeout.
		 */
		if ((!(flags & CO_DAEMON_TIMEOUT_EXPIRED)) &&
			(co_daemon_sync.flags & CO_DAEMON_TIMEOUT_WAIT))
			untimeout(co_daemon_sync.timeout_id);

		co_daemon_sync.flags &= ~wait_flags;
		co_daemon_sync.flags |= (flags & CO_DAEMON_FULL_SPEED);
	}
	mutex_spinunlock(&co_daemon_sync.lock, spl);
}

/*
 * coalesced_kick():
 * Kick coalasced if the number of large pages has gone below high watermark. 
 * We need to kick the coalesced() if coalesced() went IDLE after it
 * achieved its goals. After the coalesced() is asleep a large process can
 * break down all the large pages. The kick routine is periodically
 * called to ensure that coalesced() is woken up if that happens.
 * We don't to want to wakeup coalesced() if memory is low as it is useless.
 * Do additional checks to ensure that there is enough memory in the node
 * to form large pages. This is called from the second thread every
 * COALESCED_KICK_PERIOD seconds.
 */
void
coalesced_kick()
{
	cnodeid_t	node;
	pg_free_t	*pfl;

	/*
 	 * If the daemon is already waiting for a timeout then don't wake it up.
	 */
	if (!(co_daemon_sync.flags & CO_DAEMON_IDLE_WAIT)) {
		return;
	}
	for (node = 0; node < numnodes; node++) {
		pfl = GET_NODE_PFL(node); 	
		if (lpage_needed(node, pfl) && (!lpage_low_on_memory(node, pfl))
				&& (GLOBAL_FREEMEM() >= tune.t_gpgshi)) {
			coalesce_daemon_wakeup(node, CO_DAEMON_IDLE_WAIT);
			break;
		}
	}
}


/*
 * collect_node_info:
 * Collect free memory information per node. Used by osview.
 */
void
collect_node_info(nodeinfo_t *node_info)
{
	cnodeid_t	node;
	pgszindx_t	page_size_index;
	
	for (node = 0; node < numnodes; node++) {
		node_info->totalmem = ctob(NODEPDA(node)->node_pg_data.node_total_mem);
		node_info->freemem = ctob(NODE_FREEMEM(node));
		node_info->node_device = cnodeid_to_vertex(node);

		page_size_index = MAX_PGSZ_INDEX;
		node_info->num16mpages = 
				GET_NUM_FREE_PAGES(node, page_size_index) ;
		page_size_index--;
		node_info->num4mpages = 
				GET_NUM_FREE_PAGES(node, page_size_index) ;
		page_size_index--;
		node_info->num1mpages = 
				GET_NUM_FREE_PAGES(node, page_size_index) ;
		page_size_index--;
		node_info->num256kpages = 
				GET_NUM_FREE_PAGES(node, page_size_index) ;
		page_size_index--;
		node_info->num64kpages = 
				GET_NUM_FREE_PAGES(node, page_size_index) ;
		node_info++;
	}
}

/*
 * collect_lpg_stats:
 * Collect large page statistics.
 */
void
collect_lpg_stats(lpg_stat_info_t *lpg_stat_buf)
{
	cnodeid_t	node;
	pgszindx_t	pindx;


	for (node = 0; node < numnodes; node++)
		for (pindx = 0; pindx < MAX_PGSZ_INDEX; pindx++) {
			lpg_stat_buf->coalesce_atts += 
				GET_LPG_STAT(node, pindx, CO_DAEMON_ATT);
			lpg_stat_buf->coalesce_succ += 
				GET_LPG_STAT(node, pindx, PAGE_COALESCE_HITS);
			lpg_stat_buf->coalesce_succ += 
				GET_LPG_STAT(node, pindx, CO_DAEMON_SUCCESS);
			lpg_stat_buf->num_lpg_faults += 
				GET_LPG_STAT(node, pindx, LPG_VFAULT_SUCCESS);
			lpg_stat_buf->num_lpg_faults += 
				GET_LPG_STAT(node, pindx, LPG_PFAULT_SUCCESS);
			lpg_stat_buf->num_lpg_allocs += 
				GET_LPG_STAT(node, pindx, LPG_PAGEALLOC_SUCCESS);
			lpg_stat_buf->num_lpg_downgrade += 
				GET_LPG_STAT(node, pindx, LPG_PAGE_DOWNGRADE);
			lpg_stat_buf->num_page_splits += 
				GET_LPG_STAT(node, pindx, PAGE_SPLIT_HITS);
		}

	lpg_stat_buf->enabled_16k = 1;
#if     _PAGESZ == 4096
	lpg_stat_buf->enabled_16k = percent_totalmem_16k_pages ? 1 : 0;
#endif
	lpg_stat_buf->enabled_64k = percent_totalmem_64k_pages ? 1 : 0;
	lpg_stat_buf->enabled_256k = percent_totalmem_256k_pages ? 1 : 0;
	lpg_stat_buf->enabled_1m = percent_totalmem_1m_pages ? 1 : 0;
	lpg_stat_buf->enabled_4m = percent_totalmem_4m_pages ? 1 : 0;
	lpg_stat_buf->enabled_16m = percent_totalmem_16m_pages ? 1 : 0;
}

/*
 * System call invoked from user code to  allocate and free a large page.
 */


#define	TEST_PAGE_FREE		0	/* Large page free */
#define	TEST_PAGE_ALLOC		1	/* Large page alloc */
#define	TEST_PAGE_ALLOC_NOWAIT	2	/* Large page alloc  but don't wait*/
#define	TEST_PAGE_ALLOC_FREE	3	/* Large page alloc and free */
#define	KMEM_CONTIG_ALLOC_FREE	4	/* Kmem alloc and free of large pages */
#ifdef TILES_TO_LPAGES
#define	TEST_TILE_ALLOC_NOWAIT	5
#define	TEST_TILE_FREE	6
#define TEST_TILE_ALLOC_FREE 7
#endif /* TILES_TO_LPAGES */
int
lpage_syscall(sysarg_t op, sysarg_t arg1, sysarg_t arg2, rval_t *rvp)
{
	pfd_t	*pfd;
	uint	key;
	size_t	size;
	pfn_t	pfn;
	caddr_t vaddr;
#ifdef TILES_TO_LPAGES
	int rc;
	int cnt;
	pfn_t *pfns;
#endif /* TILES_TO_LPAGES */

	switch (op) {

	case	TEST_PAGE_ALLOC:
	case	TEST_PAGE_ALLOC_NOWAIT:
	case	TEST_PAGE_ALLOC_FREE:

		size = (size_t)arg1;
		key = (uint)arg2;
		pfd = NULL;

		while (!pfd) {

			pfd = pagealloc_size(key, VM_UNSEQ, size);
			if ( pfd != NULL) {
				pfn = pfdattopfn(pfd);
				rvp->r_val1 = (uint)pfn;
				if ( op == TEST_PAGE_ALLOC_FREE) {
					pagefree_size(pfd, size);
				}
				return 0;
			}
			if (op == TEST_PAGE_ALLOC_NOWAIT) return ENOMEM;

			if (lpage_size_wait(size, PWAIT) != 0)
				 return EINTR;
		}

		break;

	case	TEST_PAGE_FREE:

		pfn = (pfn_t)arg2;
		pfd = pfntopfdat(pfn);
		size = (size_t)arg1;
		rvp->r_val1 = pagefree_size(pfd, size);
		return 0;

	case	KMEM_CONTIG_ALLOC_FREE :
		
		vaddr = kmem_alloc((size_t)arg1, KM_PHYSCONTIG|KM_NOSLEEP);
		if (!vaddr) return ENOMEM;
		kmem_free(vaddr, (size_t)arg1);
		return 0;

#ifdef TILES_TO_LPAGES
	case TEST_TILE_ALLOC_NOWAIT:
		cnt = (int)arg1;
		rc = tile_alloc(cnt, &pfn, 0, TILE_ZERO);
		if (rc != cnt)
			return ENOMEM;
		rvp->r_val1 = pfn;
		return 0;
	case TEST_TILE_FREE:
		cnt = (int)arg1;
		pfn = (pfn_t)arg2;
		tile_free(cnt, &pfn, 0);
		return 0;
	case TEST_TILE_ALLOC_FREE:
		cnt = (int)arg1;
		pfns = kern_malloc(cnt * sizeof(pfn_t));
		rc = tile_alloc(cnt, pfns, 0, TILE_ZERO|TILE_ECCSTALE);
		if (rc != cnt) {
			return ENOMEM;
		}
		(void) tile_free(cnt, pfns, 0);
		return 0;
#endif /* TILES_TO_LPAGES */
	default :
		cmn_err_tag(108,CE_PANIC, "Lpage syscall:Invalid op %d\n",op);
	}
	return EINVAL;
}

#include <sys/proc.h>
#include <sys/pda.h>

int	debug_page_size = 65536;

/* ARGSUSED */
int
map_lpage_syscall(
	size_t page_size,
	caddr_t vaddr,
	size_t len, 
	rval_t *rvp)
{
	return 0;
}
	
int
valid_page_size(size_t size)
{
	if ( size % NBPP) return FALSE;
	size = btoc(size);

#if	_PAGESZ == 4096
	return ( size == 1 || size == 4 || size == 16 || size == 64 ||
		size == 256 || size == 1024 || size == 4096);
#elif	_PAGESZ == 16384
	return ( size == 1 || size == 4 || size == 16 || size == 64 ||
		size == 256 || size == 1024);
#else
#error	"Unknown page size"
#endif
}


int	lost_pagecount = 0;
int
check_freemem()
{
        /*
         * This check doesn't make sense anymore, since GLOBAL_FREEMEM
         * has become a relaxed global variable.
         */

        return TRUE;
}

int
check_freemem_node(cnodeid_t node)
{
	int 	tcnt, pgsz_tcnt;
	int	j, k;
	pg_free_t       *pfl = GET_NODE_PFL(node);
	phead_t		*pheadp;

	tcnt = 0;
	for (k = 0; k < NUM_PAGE_SIZES; k++) {
		pgsz_tcnt = 0;
		for (j = 0, pheadp = &pfl->phead[k][0];
			pheadp < pfl->pheadend[k]; j++, pheadp++) {
			pgsz_tcnt += pheadp->ph_count;
		}
		ASSERT(pgsz_tcnt == GET_NUM_FREE_PAGES(node, k));
		tcnt += (pgsz_tcnt * PGSZ_INDEX_TO_CLICKS(k));
	}
	if (tcnt != NODE_FREEMEM(node)) {
		printf("check_freemem_node: node: %d sum of ph_cnt: %d NODE_FREEMEM: %d\n", node, tcnt, NODE_FREEMEM(node));
	}

#ifdef TILES_TO_LPAGES
	{
		pgsz_tcnt = 0;
		for (j=0; j < NTPHEADS; j++) {
			k = 0;
			for (pheadp = tphead[j].phead;
			     pheadp < tphead[j].pheadend; pheadp++)
				k += pheadp->ph_count;
			ASSERT(k == tphead[j].count);
			pgsz_tcnt += tphead[j].count;
		}
		ASSERT(pgsz_tcnt == GET_NUM_FREE_PAGES(node, MIN_PGSZ_INDEX));
	}
#endif /* TILES_TO_LPAGES */	
	return (tcnt == NODE_FREEMEM(node));
}

#ifdef DEBUG
int
is_valid_pfdat(cnodeid_t node, pfd_t *pfd)
{
	pg_free_t	*pfl = GET_NODE_PFL(node);
	pfn_t		pfn = pfdattopfn(pfd);

	ASSERT(pfd >= pfl->pfd_low && pfd <= pfl->pfd_high);
	ASSERT(((ulong)pfn % (PFDAT_TO_PAGE_CLICKS(pfd))) == 0);
	return 1;
}
#endif /* DEBUG */


#ifdef TILES_TO_LPAGES
/*
 * tile_lowat
 *
 * Return true if our free page cnt is 10% or less of our hiwater mark
 * for tiles.
 */
int
tile_lowat(void)
{
        pg_free_t *pfl = GET_NODE_PFL(0);

	return (GET_NUM_FREE_PAGES(0, PAGE_SIZE_TO_INDEX(TILE_SIZE)) <
		(GET_LPAGE_LOWAT(pfl, PAGE_SIZE_TO_INDEX(TILE_SIZE))));
}

/*
 * tile_wait()
 *
 * Special version of lpage_size_wait() for tiles.  We need to put
 * the caller to sleep atomically w/the wakeup event sent to
 * coalesced.
 */
int
tile_wait(int pri)
{
	int		spl;
	pgszindx_t	page_size_index = PAGE_SIZE_TO_INDEX(TILE_SIZE);

	spl = mutex_spinlock(&co_daemon_sync.lock);
	/* for tiles, we need to wake up coalesced NOW!!! */
	co_daemon_sync.pgsz_wait_flags |= (1 << page_size_index);
	co_daemon_sync.start_node = 0;
	co_daemon_sync.flags = CO_DAEMON_IDLE_WAIT|
				CO_DAEMON_TIMEOUT_WAIT|CO_DAEMON_FULL_SPEED;
	sv_signal(&co_daemon_sync.daemon_wait);
	return (sv_wait_sig(&co_daemon_sync.page_size_wait[page_size_index],
			pri, &co_daemon_sync.lock, spl));
}

int
lpage_evict(cnodeid_t node, pfd_t *start_pfd, pgszindx_t pindx)
{
	int rc;

	rc = coalesce_leaf_chunk(node, start_pfd, pindx, TILE_COALESCING);

	if (rc)
		lpage_init_pfdat(start_pfd, PGSZ_INDEX_TO_SIZE(pindx),
			PAGE_ALLOC);

	return (rc);
}
#endif /* TILES_TO_LPAGES */
