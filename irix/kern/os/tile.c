/*
 * os/tile.c
 *
 * 	provide 64k physically contiguous "tiles" of memory
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

#ident	"$Revision: 1.47 $"

#ifdef TILES_TO_LPAGES

#undef TILE_DEBUG			/* enable tile debugging */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/immu.h>
#include <os/as/pmap.h>
#include <os/as/region.h>
#include <sys/pda.h>
#include <sys/buf.h>
#include <sys/tuneable.h>
#include <sys/anon.h>
#include <sys/page.h>
#include <sys/nodepda.h>
#include <sys/lpage.h>
#include <sys/sysmp.h>
#include <ksys/vm_pool.h>
#include <sys/tile.h>
#include <sys/rtmon.h>
#include <sys/atomic_ops.h>
#include <ksys/migr.h>
#include <sys/buf.h>

#define TILE_UTRACE(name, addr, len, ra) \
    UTRACE(RTMON_ALLOC, UTN('tile',(name)), (__uint64_t)(addr), \
		   UTPACK((len), (__scunsigned_t)(ra)))

/*
 * Note that the stuff w/i TILE_DATA will eventually go away, but we need
 * to keep it around for reference purposes for the time being.
 */

/*
 * TODO:
 *	- classes support
 *	- coalesced optimizations
 * 	- various optimizations based on tile organization
 */
tile_data_t *tile_data;                 /* alloc'd at startup */

struct tileinfo tileinfo;               /* tileinfo stuff */

static tileq_t tileq[TQ_NQUEUES];       /* queues of tiles */

static tile_data_t *first_tile, *first_full_tile;
static tile_data_t *last_tile, *last_full_tile;

static int tile_cache_clean_threshold;	/* thresh for whole cache flush */
static void tile_cache_clean(int, pfn_t *);
static void tile_ecc_clean(pfd_t *, int);

static void tq_enqti(tile_data_t *, int);
static void tq_enqhi(tile_data_t *, int);
static void tq_deqi(tile_data_t *, int);

static int tile_evict(tile_data_t *, int);

void tilepages_to_unfrag(pfd_t *);
void tilepages_to_frag(pfd_t *);

#define TQ_FIRST(qi)	(!tq_empty(qi) ? tditotile(tileq[qi].tq_head) : NULL)
#define TD_NEXT(td)	((td)->td_next ? tditotile((td)->td_next) : NULL)

#ifdef TILE_DEBUG
static int ecc_verify_tiles;		/* enable ECC checks on free */
#endif /* TILE_DEBUG */

static void tile_dump(__psint_t);	/* idbg funcs */
static void tileq_dump(__psint_t);

#if TILE_DEBUG
static void tilepfn_dump(__psint_t);
static void tile_check(tile_data_t *td);
static void tile_sync(tile_data_t *td);
#endif /* TILE_DEBUG */

static int tile_class_rotor;		/* start class for TILE_CLASS_SINGLE */
static int tile_class_max;              /* last mem bank seen */
static int tile_class_cnt[TILE_CLASS_MAX];	/* # of unlocked tiles/class */

static int page_mvok_tries;
static int page_nomv_tries;
static int tiles_unlocked_lowat;

static int tiled_lowat(void);
static int tiled_hiwat(void);

extern int is_in_pfdat(pgno_t);
extern int dcache_size;

/* locks and stuff */
static sema_t tallocsema;		/* serialize talloc calls */
static int talloc_busy;			/* tallocsema in use */

/*
 * Memory is broken down like this:
 *
 *     | tiled  |    fragmented     |    unfragmented    |
 *     |--------+-------------------+--------------------|
 *     |                    memory                       |
 *     |------------+---------+--------------+-----------|
 *     |   fixed    | movable |   freemem    |  movable  |
 */

static sema_t tiledsema;		/* tiled() synchronization */

static int fragmovelo = 2;		/* 50% movable pgs of unlocked frags */
static int fragmovehi = 5;		/* 20% movable pgs of unlocked frags */
static int fragfreelo = 3;		/* 33% freemem in frag'd tiles */
static int fragfreehi = 2;		/* 50% freemem in frag'd tiles */

static int frag_unlocked_thresh;	/* movable,free pgs in frag'd tiles */

#if 0
/*
 * Very little hope that you can protect the tileqs with a sleep
 * lock given the early calls during boot.
 */
static mutex_t tiles_lock;
#define tiles_lock()		mutex_lock(&tiles_lock, 0)
#define tiles_unlock(s)		mutex_unlock(&tiles_lock, s)
#endif /* 0 */

/* tileq locking */
static lock_t tiles_lock;		
#define tiles_lock()            mutex_spinlock(&tiles_lock)
#define tiles_unlock(s)         mutex_spinunlock(&tiles_lock, s)

#define node	((cnodeid_t)0)

#ifdef _TILE_ECC_CHECK
static void tile_cache_check(pfn_t tpn);
#endif /* _TILE_ECC_CHECK */

#ifdef _TILE_TRACE

#define TILE_TRACE_NONE 0
#define TILE_TRACE_ALLOC 1
#define TILE_TRACE_FREE 2

typedef struct tile_trace_s {
	uint_t	type:4;
	uint_t	pfn : 28;
	uint_t	flags;
	uint_t	pflags;
	__psunsigned_t	ra;
} tile_trace_t;
	
#define MAX_TILE_TRACE_BUF 1024

tile_trace_t tile_trace_buf[MAX_TILE_TRACE_BUF];
volatile int	tile_trace_next = 0;


static void
tile_trace(uint_t type, uint_t pfn, uint_t flags, uint_t pflags, void *ra)
{
	int	ti;
	tile_trace_t *tp;

	ti = tile_trace_next;
	tile_trace_next = ((ti == (MAX_TILE_TRACE_BUF - 1)) ? 0 : (ti + 1));

	tp = tile_trace_buf + ti;
	tp->type = type;
	tp->pfn = pfn;
	tp->flags = flags;
	tp->pflags = pflags;
	tp->ra = (__psunsigned_t) ra;
}


static void
tile_trace_dump_entry(tile_trace_t *tp)
{
    qprintf("%s ra 0x%x pfn 0x%x flags 0x%x pflags 0x%x\n",
	    ((tp->type == TILE_TRACE_ALLOC) ? "alloc" 
	     : ((tp->type == TILE_TRACE_FREE) ? "free "
		: "?    ")),
	    (uint_t) tp->ra,
	    tp->pfn,
	    tp->flags,
	    tp->pflags);
}


/* ARGSUSED */
static void
tile_trace_dump(int arg)
{
	int	ts;
	int	ti;

	ti = (tile_trace_next == 0) ? MAX_TILE_TRACE_BUF - 1 : tile_trace_next - 1;
	ts = ti;
	do {
		tile_trace_dump_entry(tile_trace_buf + ti);
		ti = (ti == 0) ? MAX_TILE_TRACE_BUF - 1 : ti - 1;
	} while (ti != ts);
}

static void 
tile_trace_init(void)
{
	idbg_addfunc("tiletrace", tile_trace_dump);
}
#else /* TILE_TRACE */

#define tile_trace(type,pfn,flags,pflags,ra) ((void) 0)

#define tile_trace_init() ((void) 0)
#endif /* TILE_TRACE */

/*
 * tile_init
 *
 * Initialize the tile table.  Place all tiles on
 * the proper tile queue.  This is called early from
 * meminit.
 */
void
tile_init(pfn_t first_pfn, pfn_t last_pfn, int badpages)
{
	int i;
	tile_data_t *td;
	int first_full_tile_pfn, last_full_tile_pfn;
	int curbank, prevbank;
	extern int dcache_size;
	extern int addr_to_ibank(uint);

	idbg_addfunc("tile", tile_dump);
	idbg_addfunc("tileq", tileq_dump);
#ifdef TILE_DEBUG
	idbg_addfunc("tilepfn", tilepfn_dump);
#endif /* TILE_DEBUG */

	tile_trace_init();

	first_full_tile_pfn = roundup(first_pfn, TILE_PAGES);
	last_full_tile_pfn = (last_pfn - TILE_PAGES) & ~TILE_PAGEMASK;

	first_tile = pfntotile(first_pfn);
	first_full_tile = pfntotile(first_full_tile_pfn);
	last_full_tile = pfntotile(last_full_tile_pfn);
	last_tile = pfntotile(last_pfn - 1);

#ifdef TILE_DEBUG
	printf("first 0x%x first_full 0x%x last_full 0x%x last 0x%x bad 0x%x\n",
	       first_pfn, first_full_tile_pfn, last_full_tile_pfn,
	       last_pfn, badpages);
#endif /* TILE_DEBUG */

	/*
	 * There's no guarantee that we can use all the pages
	 * from the first and last tiles, e.g. if the first
	 * free page doesn't happen to be tile-aligned.
	 *
	 * Here we'll mark the pages of the first and last tiles
	 * that we can't use as "locked for long term allocation"
	 * and we won't bother with those pages again.
	 */
	if (first_tile != first_full_tile) {
		for (i = first_pfn & ~TILE_PAGEMASK; i < first_pfn; i++) {
			TILE_SETLOCKED(first_tile, i);
			first_tile->td_nlocked++;
		}
		tilepages_to_frag(tiletopfd(first_tile));
	}

	/*
	 * ...Last tile too.  We treat the last page of memory as a
	 * badpage to avoid DMA boundary problems.
	 */
	if (last_tile != last_full_tile) {
		for (i = last_pfn+1; i < roundup(last_pfn, TILE_PAGES); i++) {
			TILE_SETLOCKED(last_tile, i);
			last_tile->td_nlocked++;
		}
		if (last_tile->td_nlocked)
			tilepages_to_frag(tiletopfd(last_tile));
	}

	/*
	 * ...And some pages in the middle might be holes in
	 * physical memory.
	 *
	 * We treat the last page of memory as a hole to avoid 
	 */
	if (badpages) {
		pfd_t *pfd;

		for (pfd = pfntopfdat(first_pfn);
		     pfd <= pfntopfdat(last_pfn); pfd++) {
			if (PG_ISHOLE(pfd)) {
				tile_data_t *td = pfdtotile(pfd);
				pfn_t pfn = pfdattopfn(pfd);
				int waslocked = td->td_locked;

				TILE_SETLOCKED(td, pfn);
				td->td_nlocked++;
				if (!waslocked)
					tilepages_to_frag(tiletopfd(td));
				badpages--;
			}
		}
	}
	ASSERT(badpages == 0);

	prevbank = addr_to_ibank(first_pfn << PNUMSHFT);
	for (td = first_tile; td <= last_tile; td++) {
		curbank = addr_to_ibank(tiletopfn(td) << PNUMSHFT);
		/* Skip, if the tile is in the LINEAR_BASE hole */
		if (td >= pfntotile(btoct(SEG0_SIZE)) &&
		    td < pfntotile(btoct(SEG1_BASE)))
			continue;
			
		if (curbank != prevbank) {
			prevbank = curbank;
			tile_class_max++;
			ASSERT(tile_class_max < TILE_CLASS_MAX);
		}
		td->td_class = tile_class_max;
#ifdef MH_R10000_SPECULATION_WAR
		if (td < tile_lowmem)
			tq_enqti(td, TQ_LOWMEM);
		else
#endif /* MH_R10000_SPECULATION_WAR */
		if (td->td_nlocked < TILE_PAGES) {
			tq_enqt(td);
		} else { 
			tq_enqti(td, TQ_HOLE);
		}
	}

	/*
	 * In tile_pageselect() we examine this many pages in a phead
	 * looking for pages in the proper type of tile (partial or unlocked).
	 * We want to look at a few to keep tiles from becoming too
	 * fragmented, preventing us from ever allocating them with tile_alloc.
	 * But we don't want to spend too much time looking at pages or
	 * performance will suffer.
	 *
	 * ...mvok_tries is how hard we try to find a good page when
	 * allocating a movable page.
	 *
	 * ...nomv_tries is how hard we try when allocating fixed pages.
	 *
	 * We could make this adaptive and try harder when the number of tiles
	 * on the TQ_UNLOCKED queue gets small.  (It might be too late then.
	 * freemem will also be small so phead queues will be short anyway.)
	 */
	if (page_mvok_tries == 0)
		page_mvok_tries = 2;
    
	if (page_nomv_tries == 0)
		page_nomv_tries = 4;

	if (tiles_unlocked_lowat == 0)
		tiles_unlocked_lowat = 40;
	
        frag_unlocked_thresh = maxmem / 8;
	if (frag_unlocked_thresh > btoc(4 * 1024*1024))
		frag_unlocked_thresh = btoc(4 * 1024*1024);
	    
	/*
	 * Figure out how many tiles we can flush individually,
	 * more efficiently than flushing the whole cache.
	 * We'll set it at the number of tiles that equals half
	 * the data cache size.
	 */
	tile_cache_clean_threshold = (dcache_size >> 1) / TILE_SIZE;

	initnsema(&tallocsema, 1, "tallocsem");
	initnlock(&tiles_lock, "tileslck");
}

/*
 * tile_alloc
 *
 * todo
 *	- add TILE_CLASS support
 */
/*ARGSUSED3*/
int
tile_alloc(int ntiles, pfn_t *taddrs, int classmask_arg, int flags)
{
	tile_data_t *td;
	pfd_t *pfd;
	pgno_t npgs;
	int classmask, classidx = 0, scan_wrapped;
	int talloc, tiles_to_check, last_talloc, evicted;
	int vmflags = (flags & TILE_NOSLEEP) ? VM_NOSLEEP : 0;
	int s;

#ifdef TILE_DEBUG
	printf("tile_alloc: ntiles=%x taddrs=%x class=%x flags=%x\n",
	       ntiles, taddrs, classmask_arg, flags);
#endif

	if (flags & TILE_NOSLEEP) {
		if (!cpsema(&tallocsema)) {
			return 0;
		}
	} else {
		/* breakable sleep */
		if (psema(&tallocsema, (PZERO+1))) {
			return 0;
		}
	}

	talloc_busy = 1;
	npgs = ntiles * TILE_PAGES;

	/*
	 * For now fail if we can't reserve the desired mem.
	 * Alternatives:
	 *  - leave it like this (i.e. fail like TD)
	 *  - use kreservemem() like LP
	 * 
	 * XXX do we need to retain TILE_NORESERVEMEM for vtiles???
	 */
	if (reservemem(GLOBAL_POOL, npgs, npgs, npgs)) {
		talloc_busy = 0;
		vsema(&tallocsema);
		return 0;
	}

	if (classmask_arg == 0)
		classmask_arg = TILE_ANYCLASS;
	talloc = 0; last_talloc = -1;

	/*
	 * If we want all tiles to come from a single class,
	 * look for a class from the set in classmask_arg that
	 * has enough unlocked tiles in it.
	 */
	if (flags & TILE_CLASS_SINGLE) {
		scan_wrapped = 0;

		classidx = tile_class_rotor;
		while (((1 << classidx) & classmask_arg) == 0 ||
		       tile_class_cnt[classidx] < ntiles) {
next_class:
			if (++classidx > tile_class_max)
				classidx = 0;
			if (classidx == tile_class_rotor) {
				scan_wrapped = 1;
				break;
			}
		}
		
		/* no single class had enough available tiles */
		if (scan_wrapped)
			goto error_out;

		/* select this class */
		classmask = (1 << classidx);

	} else {
		/* any class from classmask_arg will do */
		classmask = classmask_arg;
	}

	/*
	 * Try grabbing tiles from the 64K large page free list if
	 * caller doesn't care about classes.  These are faster and
	 * easier to dole out since no eviction is required.
	 * Use _pagealloc_size_node because we're interested in one
	 * and only one page size.
	 */
	if (classmask_arg == TILE_ANYCLASS) {
		tiles_to_check = NODE_FREE_PAGES(node, TILE_INDEX);
		while (talloc < ntiles && tiles_to_check--) {
			if (NODE_FREE_PAGES(node, TILE_INDEX) == 0)
				break;
			pfd = _pagealloc_size_node(node, 0, VM_UNSEQ,
						   TILE_SIZE);
			if (pfd) {
				td = pfdtotile(pfd);

				ASSERT(td != last_tile);

				s = tiles_lock();
				tq_deq(td);
				td->td_nlocked = TILE_PAGES;
				td->td_locked  = 0xffff;
				tq_enqt(td);
				td->td_tile = 1;
				tiles_unlock(s);
				atomicAddInt(&tileinfo.ttile, TILE_PAGES);
				taddrs[talloc++] = tiletopfn(td);

				TILE_UTRACE('Aloc',UTPACK(flags,
						((flags & TILE_ECCSTALE) ?
						 pfdat_to_eccpfn(pfd) :
						 taddrs[talloc-1])),
					    pfd->pf_flags,__return_address);
				tile_trace(TILE_TRACE_ALLOC,\
					   ((flags & TILE_ECCSTALE) ?
					    pfdat_to_eccpfn(pfd) :
					    taddrs[talloc-1]),\
					   flags,\
					   pfd->pf_flags,\
					   __return_address);
			}
		}
	}

again:
	/*
	 * Bail now if we have no hopes of getting the
	 * tiles we need.
	 */
	if (tq_ntiles(TQ_UNLOCKED) < ntiles - talloc) {
free_error_out:
		cmn_err(CE_CONT, "!tile_alloc failure; talloc %d ntiles %d\n",
			talloc, ntiles);
		tile_free(talloc, taddrs, flags | TILE_NORESERVEMEM);
error_out:
		talloc_busy = 0;
		vsema(&tallocsema);
		if ((flags & TILE_NORESERVEMEM) == 0)
			unreservemem(GLOBAL_POOL, npgs, npgs, npgs);
		return 0;
	}

	/* check the unlocked queue to see if we can migrate some pages */
	tiles_to_check = tq_ntiles(TQ_UNLOCKED);
	while (talloc < ntiles && tiles_to_check--) {
		s = tiles_lock();
		td = TQ_FIRST(TQ_UNLOCKED);
		tiles_unlock(s);
		if (!td)
			break;

		ASSERT(td != last_tile);

		if ((1 << td->td_class) & classmask) {
			evicted = tile_evict(td, flags & TILE_NOSLEEP ?
					     VM_NOSLEEP : 0);
		} else
			evicted = 0;

		if (evicted > 0) {
			s = tiles_lock();
			tq_deq(td);
			td->td_nlocked = TILE_PAGES;
			td->td_locked  = 0xffff;
			tq_enqt(td);
			td->td_tile = 1;
			tiles_unlock(s);
			taddrs[talloc++] = tiletopfn(td);

			atomicAddInt(&tileinfo.ttile, TILE_PAGES);
			pfd = pfntopfdat(taddrs[talloc-1]);
			TILE_UTRACE('Aloc',UTPACK(flags,((flags & TILE_ECCSTALE) ? pfdat_to_eccpfn(pfd) : taddrs[talloc-1])),pfd->pf_flags,__return_address);
			tile_trace(TILE_TRACE_ALLOC,\
			   ((flags & TILE_ECCSTALE) ? pfdat_to_eccpfn(pfd) : taddrs[talloc-1]),\
			   flags,\
			   pfd->pf_flags,\
			   __return_address);
		} else {
			/*
			 * We can't use this tile.  Put it on the end
			 * of the list.
			 */
			s = tiles_lock();
			tq_deq(td);
			tq_enqt(td);
			tiles_unlock(s);
		}
	}

	if (talloc < ntiles) {
		/*
		 * If we want all our tiles from the same class,
		 * free up the ones we allocated so far, and go
		 * look for another class that might work.
		 */
		if (flags & TILE_CLASS_SINGLE) {
			tile_free(talloc, taddrs, flags | TILE_NORESERVEMEM);
			goto next_class;
		}

		/*
		 * If we preferred some classes over others but didn't
		 * find all the tiles we needed, loosen the class
		 * restriction and try again.  We know the caller didn't
		 * want TILE_CLASS_SINGLE or we wouldn't have gotten here.
		 */
		if (classmask != TILE_ANYCLASS) {
			classmask = TILE_ANYCLASS;
			goto again;
		}
		
		if ((flags & TILE_NOSLEEP) || (talloc == last_talloc)) {
			goto free_error_out;
		}
		last_talloc = talloc;

		/* wait for memory */
		setsxbrk();

		goto again;
	}

	/* update tile class rotor if necessary */
	if (flags & TILE_CLASS_SINGLE) {
		if (classidx + 1 > tile_class_max)
			tile_class_rotor = 0;
		else
			tile_class_rotor = classidx + 1;
	}

	talloc_busy = 0;
	vsema(&tallocsema);

	/* ping tiled if needed */
	if (tiled_lowat())
		cvsema(&tiledsema);

	/* clean pages here */
	if (flags & TILE_ZERO) {
		int t;
		caddr_t tvaddr;

		for (t = 0; t < talloc; t++) {
			tvaddr = tile_mapin(taddrs[t], vmflags);
			if (tvaddr == NULL) {
				tile_free(talloc, taddrs, 0);
				talloc_busy = 0;
				vsema(&tallocsema);
				return 0;
			}
			bzero(tvaddr, TILE_SIZE);
			tile_mapout(tvaddr);
		}
	}

	tile_cache_clean(talloc, taddrs);

	/*
	 * If caller is going to create bad ECC in these tile(s),
	 * mark the pages so they will get mapped in via the
	 * ECC-bypass alias.
	 */
	if (flags & TILE_ECCSTALE) {
		int t;

		for (t = 0; t < talloc; t++) {
			pfd_t *tpfd = pfntopfdat(taddrs[t]);

			if (tpfd->pf_flags & P_ECCSTALE)
				continue;
			for (pfd = tpfd; pfd < tpfd + TILE_PAGES; pfd++)
				pfd->pf_flags |= P_ECCSTALE;
			taddrs[t] = pfdattopfn(tpfd);
		}
	}

#ifdef TILE_DEBUG
	{
		int t;
		for (t=0;t<talloc;t++) {
			pfd_t *tpfd;

			tpfd = pfntopfdat(taddrs[t]);
			printf("tile_alloc:taddrs[%d]=0x%x tpfd=0x%x flags=0x%x\n", t, taddrs[t], tpfd, tpfd->pf_flags);
		}
	}
#endif /* TILE_DEBUG */

	return talloc;
}

void
tile_free(int ntiles, pfn_t *taddrs, int flags)
{
	int t, s;
	pfd_t *tpfd;
	tile_data_t *td;
	int npgs = ntiles * TILE_PAGES;

#ifdef TILE_DEBUG
	printf("tile_free: ntiles=%d flags=0x%x\n",
	       ntiles, flags);
#endif /* TILE_DEBUG */

	if (ntiles == 0)
		return ;

	/*
	 * XXX need to do any per-pfd cleanup here for now
	 */
	for (t = 0; t < ntiles; t++) {
		td = pfntotile(taddrs[t]);
		tpfd = pfntopfdat(taddrs[t]);

		ASSERT(td != last_tile);
		ASSERT(td != first_tile);
		ASSERT(td->td_tile);

		td->td_tile = 0;

		if (tpfd->pf_flags & P_ECCSTALE) {
			(void) tile_ecc_clean(tpfd, 0);
		}

		TILE_UTRACE('Free',UTPACK(tpfd->pf_flags,taddrs[t]),flags,__return_address);
		tile_trace(TILE_TRACE_FREE,taddrs[t],flags,tpfd->pf_flags,__return_address);
		if (tpfd->pf_flags & P_ECCSTALE) {
			(void) tile_ecc_clean(tpfd, 0);
		}
#ifdef TILE_DEBUG
		/*
		 * Someone has just handed us a tile that they claimed
		 * would have valid ECC (no TILE_ECCSTALE flag to tile_alloc
		 * and thus has no P_ECCSTALE flag set in the pfdat's).
		 * Let's map it in and read through it to be sure.  If
		 * there is bad ECC in the tile, our touching it will
		 * generate an ECC exception, and since it is in the kernel
		 * we will panic.  Conceptually, this is
		 *      ASSERT(valid_ecc_in_tile(taddrs[t]));
		 */
		else {
			caddr_t va;
			long *vaend;
			volatile long l, *lp;

			if (ecc_verify_tiles) {
				va = tile_mapin(taddrs[t], VM_UNCACHED);
				if (va) {
					vaend = (long *)(va + TILE_SIZE);
					for (lp = (long *)va; lp < vaend; lp++)
						l = *lp;
					tile_mapout(va);
				}
			}
		}
		printf("tile_free: taddrs[%d]=0x%x pfd=0x%x flags=0x%x\n",
		       t, taddrs[t], start_pfd, start_pfd->pf_flags);
#endif /* TILE_DEBUG */
		s = tiles_lock();
		tq_deq(td);
		td->td_nlocked = 0;
		td->td_locked = 0;
		tq_enqh(td);
		tiles_unlock(s);
		
		/* put tile back on 64K large page free list */
		pagefree_size(tpfd, TILE_SIZE);
	}
	if ((flags & TILE_NORESERVEMEM) == 0) {
		unreservemem(GLOBAL_POOL, npgs, npgs, npgs);
	}
	atomicAddInt(&tileinfo.ttile, -npgs);
}

/*
 * tile_mapin
 *
 * Map a 64k tile in kvas.  Use K0 where possible, K2 otherwise.
 *
 * Honor the following vmflags:
 *	VM_UNCACHED (try to use K1)
 *	VM_NOSLEEP
 *
 * XXX this code is identical to page_mapin_pfn except that we loop
 *     over all pages in the tile; need to rework this to share the
 *     dup'd code
 */
caddr_t
tile_mapin(pfn_t tpfn, int flags)
{
	caddr_t vaddr, v;
	pgi_t state;
	int i;

	/* XXX check VM_UNCACHED_PIO here also??? */
	if (flags & VM_UNCACHED) {
		if (small_K1_pfn(tpfn)) {
			vaddr = vfntova(small_pfntovfn_K1(tpfn));
			return vaddr;
		}
	} else if (small_K0_pfn(tpfn)) {
		vaddr = vfntova(small_pfntovfn_K0(tpfn));
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			for (i = 0; i < TILE_PAGES; i++)
				color_validation(pfntopfdat(tpfn + i),
					vcache_color(tpfn + i), 0, flags);
		}
#endif /* _VCE_AVOIDANCE */
		return vaddr;
	}

	/* need to do the mapped thang; start by allocing K2 vaddr */
	vaddr = kvalloc(TILE_PAGES,
			(flags & (VM_UNCACHED|VM_NOSLEEP)) | VM_VACOLOR,
			vcache_color(tpfn));

	if (vaddr == 0)
		return NULL;

	if (!(flags & (VM_UNCACHED|VM_UNCACHED_PIO)))
		state = (pgi_t)(PG_VR | PG_G | PG_M | PG_SV | pte_cachebits());
	else if (flags & VM_UNCACHED)
		state = pte_noncached(PG_VR | PG_G | PG_M | PG_SV);
	else if (flags & VM_UNCACHED_PIO)
		state = pte_uncachephys(PG_VR | PG_G | PG_M | PG_SV);

	for (v = vaddr, i = 0; i < TILE_PAGES; i++, v += NBPP) {
		kvtokptbl(v)->pgi = mkpde(state, tpfn + i);
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance &&
		    !(flags & VM_UNCACHED) &&
		    is_in_pfdat(tpfn+i))
			color_validation(pfntopfdat(tpfn+i),
					 colorof(v), 0, flags);
#endif /* _VCE_AVOIDANCE */		
	}

	return vaddr;
}

void
tile_mapout(caddr_t tvaddr)
{
	if (IS_KSEG2(tvaddr))
		kvfree(tvaddr, TILE_PAGES);
}

/*
 * tiled's purpose is to keep some free pages available in
 * partial tiles so pagealloc of fixed pages will find some
 * there.  We do this by moving some movable pages from partial
 * tiles into tiles in the unlocked tile queue (TQ_UNLOCKED).
 * This process actually works against what tile_alloc would
 * like--more free pages in the unlocked tile queue.
 */
static int
do_tiled(void)
{
	tile_data_t *td;
	pfd_t *pfd, *npfd = NULL;
	int s, i, qi, nevicted, tiles_to_check;
#ifdef TILE_DEBUG
	int nfailed;

	nfailed = 0;
#endif /* TILE_DEBUG */

	nevicted = 0;
	for (qi = TILE_PAGES - 1; qi > 0; qi--) {
		tiles_to_check = tq_ntiles(qi);
		while (tiles_to_check--) {
			s = tiles_lock();
			td = TQ_FIRST(qi);
			tiles_unlock(s);
			if (!td)
				break;

#ifdef MH_R10000_SPECULATION_WAR
			/* we don't really expect to find these here */
			if (td < tile_lowmem)
				goto skip_tile;
#endif /* MH_R10000_SPECULATION_WAR */

			for (pfd = tiletopfd(td), i = 0;
			     i < TILE_PAGES; i++, pfd++) {
				/*
				 * No need to look at locked pages.  This
				 * also keeps us from looking at pages that
				 * don't really have a valid pfdat associated
				 * with them, e.g. pages in the first
				 * (partial) tile.
				 */
				if (TILE_ISLOCKED(td, pfdattopfn(pfd)))
					continue;

				if (pfd->pf_flags & P_QUEUE)
					continue;

				if (!npfd) {
					npfd =
#ifdef _VCE_AVOIDANCE
						vce_avoidance ?
						pagealloc(pfd_to_vcolor(pfd),
							  VM_VACOLOR | VM_STALE | VM_MVOK)
						:
#endif /* _VCE_AVOIDANCE */
						pagealloc(pfdattopfn(pfd),
							  VM_STALE | VM_MVOK);

					if (!npfd)
						goto out;
				}

				COLOR_VALIDATION(npfd,
						 pfd_to_vcolor(pfd), 0, 0);

				if (migr_migrate_frame(pfd, npfd) == 0) {
					/* Put this page on the free list. */
					pagefree(pfd);
					nevicted++;
					npfd = NULL;	/* npfd was used */
				}
#ifdef TILE_DEBUG
				else {
					nfailed++;
				}
#endif /* TILE_DEBUG */
			}

skip_tile:
			/* put this tile back at the end of the list */
			s = tiles_lock();
			tq_deq(td);
			tq_enqt(td);
			tiles_unlock(s);

			if (tiled_hiwat())
				goto out;
		}
	}
out:
	if (npfd)
		pagefree(npfd);		/* couldn't use npfd */
#ifdef TILE_DEBUG
	printf("do_tiled: evicted %d failed %d\n", nevicted, nfailed);
#endif /* TILE_DEBUG */
	return nevicted;
}

void
tiled(void)
{
	int nevicted;

	initnsema(&tiledsema, 0, "tiled");

	for (;;) {
		psema(&tiledsema, PSWP);

		/*
		 * We don't want to run if there is someone allocating tiles,
		 * since we work against tile allocation by moving active pages
		 * into the unlocked tile queue.
		 */
		psema(&tallocsema, PSWP);

		nevicted = do_tiled();
		tileinfo.tiledmv += nevicted;
	
		vsema(&tallocsema);
    }
    /* NOTREACHED */
}

/*
 * Return true if we are *below* the low water condition
 * and tiled() needs to run.
 */
static int
tiled_lowat(void)
{
	/* free pages in fragmented tiles */
	int fragfree = tphead[TPH_FRAG].count;
	/* free+movable pages in frag'd tiles */
	int fragunlk = tileinfo.tfrag - tileinfo.fraglock;
	/* movable pages in frag'd tiles */
	int fragmove = fragunlk - fragfree;
	/* free memory in potential tiles */
	int tfreemem = (NODE_FREE_PAGES(node, TILE_INDEX) * TILE_PAGES) +
		tphead[TPH_UNFRAG].count + fragfree;

	/*
	 * Return true if we have a reasonable amount of free memory,
	 * and a decent number of unlocked pages in fragmented tiles,
	 * and a reasonable number of movable pages of those unlocked,
	 * and free pages in fragmented tiles is low.
	 */
	return tfreemem > tune.t_gpgshi &&
		fragunlk > frag_unlocked_thresh &&
		fragmove > fragunlk / fragmovelo &&
		fragfree < tfreemem / fragfreelo;
}

/*
 * Return true if we are *above* the high water condition
 * and tiled() can take a break.
 */
static int
tiled_hiwat(void)
{
	/* free pages in fragmented tiles */
	int fragfree = tphead[TPH_FRAG].count;
	/* free+movable pages in frag'd tiles */
	int fragunlk = tileinfo.tfrag - tileinfo.fraglock;
	/* movable pages in frag'd tiles */
	int fragmove = fragunlk - fragfree;
	/* free memory in potential tiles */
	int tfreemem = (NODE_FREE_PAGES(node, TILE_INDEX) * TILE_PAGES) +
		tphead[TPH_UNFRAG].count + fragfree;

	/*
	 * Opposite of above (with lo/hi slop)
	 * If there's plenty of freemem, then we'll stop tiled
	 * when there's not much left to move out of fragunlk.
	 * If freemem is getting low, then we'll stop when
	 * we've evened up free pages in frag'd/unfrag'd tiles.
	 */
	return tfreemem < tune.t_gpgshi ||
		fragunlk < frag_unlocked_thresh ||
		fragmove < fragunlk / fragmovehi ||
		fragfree > tfreemem / fragfreehi;
}

static void
tile_cache_clean(int ntiles, pfn_t *taddrs)
{
	int i, t;

	/* if we're flushing enough tiles, just flush the entire cache */
	if (ntiles > tile_cache_clean_threshold) {
		flush_cache();
		return;
	}

	for (t = 0; t < ntiles; t++) {
		for (i = 0; i < TILE_PAGES; i++) {
#ifdef _VCE_AVOIDANCE
			if (vce_avoidance)
				(void)color_validation(pfntopfdat(taddrs[t]+i),
						       -1, 0, 0);
			else
#endif /* _VCE_AVOIDANCE */
				_bclean_caches(0, NBPP, taddrs[t]+i,
					CACH_DCACHE|CACH_ICACHE|CACH_WBACK|
					CACH_INVAL|CACH_FORCE|CACH_NOADDR);

		}
#ifdef _TILE_ECC_CHECK
		tile_cache_check(taddrs[t]);
#endif /* _TILE_ECC_CHECK */
	}
}

/*
 * Return the total number of tile classes
 * in the system.
 */
int
tile_nclasses(void)
{
	return tile_class_max + 1;
}

/*
 * tile_ecc_clean
 *
 * Cleanup ECCSTALE tile for general use.
 */
static void
tile_ecc_clean(pfd_t *tpfd, int flags)
{
	pfd_t *pfd;
	caddr_t tvaddr;
	auto pfn_t tpfn;

	if ((tpfd->pf_flags & P_ECCSTALE) == 0) {
#ifdef TILE_DEBUG
		printf("tile_ecc_clean: no P_ECCSTALE bit set\n");
#endif /* TILE_DEBUG */
		return ;
	}

	tpfn = pfdattopfn(tpfd);
	tvaddr = tile_mapin(tpfn, flags);
	if (tvaddr == NULL) {
#ifdef TILE_DEBUG
		printf("tile_ecc_clean: tile_mapin failure\n");
#endif /* TILE_DEBUG */
		return ;
	}
	bzero(tvaddr, TILE_SIZE);
	tile_mapout(tvaddr);

	/*
	 * It is important to flush the cache before we
	 * release the pages in this tile, in order to be
	 * sure it gets written to memory where the correct
	 * ECC is generated.
	 */
	tile_cache_clean(1, &tpfn);

	for (pfd = tpfd; pfd < tpfd + TILE_PAGES; pfd++) {
		pfd->pf_flags &= ~P_ECCSTALE;
	}

	return ;
}

void
tile_pagelock(pfd_t *pfd, int cnt)
{
	tile_data_t *td;
	pfn_t pfn;
	int s, i;
	int waslocked;

	ASSERT(cnt == PGSZ_INDEX_TO_CLICKS(PFDAT_TO_PGSZ_INDEX(pfd)));
	pfn = pfdattopfn(pfd);
	td  = pfntotile(pfn);
#ifdef MH_R10000_SPECULATION_WAR
	if (td < tile_lowmem) {
		return;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	s = tiles_lock();
	waslocked = td->td_locked;
	
	ASSERT(td->td_nlocked < TILE_PAGES);

	tq_deq(td);
	for (i = 0; i < cnt; i++) {
#ifdef DEBUG
		if (td != first_tile && td != last_tile)
		    ASSERT(!TILE_ISLOCKED(td, pfn+i));
#endif
		TILE_SETLOCKED(td, pfn+i);
		td->td_nlocked++;
	}
	tq_enqt(td);

	tileinfo.pglocks++;

	if (!waslocked && (PFDAT_TO_PGSZ_INDEX(pfd) == MIN_PGSZ_INDEX))
		tilepages_to_frag(tiletopfd(td));
		
	tiles_unlock(s);

	if (tiled_lowat())
		cvsema(&tiledsema);

#ifdef TILE_DEBUG
	if (td != last_tile && td != first_tile)
		tile_sync(td);
#endif
}

void
tile_pageunlock(pfd_t *pfd, int cnt)
{
	tile_data_t *td;
	pfn_t pfn;
	int s, i;
	int waslocked = 0;

	pfn = pfdattopfn(pfd);
	td  = pfntotile(pfn);
#ifdef MH_R10000_SPECULATION_WAR
	if (td < tile_lowmem) {
		return ;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	s = tiles_lock();
	for (i = 0; i < cnt; i++) {
		if (TILE_ISLOCKED(td, pfn+i)) {
			ASSERT(td->td_nlocked > 0);
			if (! waslocked) {
				tq_deq(td);
				waslocked = td->td_nlocked;
			}
			TILE_CLRLOCKED(td, pfn+i);
			td->td_nlocked--;
		}
	}
	if (waslocked)
		tq_enqt(td);

	if (!td->td_locked && waslocked &&
	    (PFDAT_TO_PGSZ_INDEX(pfd) == MIN_PGSZ_INDEX))
		tilepages_to_unfrag(tiletopfd(td));

	tiles_unlock(s);

#ifdef TILE_DEBUG
	if (td != last_tile && td != first_tile)
		tile_sync(td);
#endif

	return ;
}

/*
 * If VM_MVOK is set in vmflags, we can relocate this page
 * later if we need to allocate a tile so try to choose a
 * page from a tile with fewer fixed pages in the hopes that
 * the tile may become unfragmented soon.
 *
 * If VM_MVOK is clear in vmflags, we can't relocate the page
 * so try to choose a page from a tile that has more fixed pages.
 */
pfd_t *
tile_pageselect(phead_t *ph, int phlist, int vmflags)
{

	pfd_t *pfd, *phead, *pfdgood;
	int qi, qigood, tries;

	pfd   = ph->ph_list[phlist].pf_next;
	phead = pfd->pf_prev;

	ASSERT(pfd != phead);	/* assume non-empty list */

	pfdgood = pfd;

	if (vmflags & VM_MVOK) {
		tries = page_mvok_tries;

		/*
		 * Try harder if we're running low on unlocked tiles.
		 */
		if (tq_ntiles(TQ_UNLOCKED) < tiles_unlocked_lowat)
			tries <<= 1;

		/*
		 * Try harder if someone is actively trying
		 * to allocate tiles, otherwise we may just
		 * end up moving this page soon.
		 */
		if (talloc_busy)
			tries <<= 1;

		qigood = TQ_LOCKED;

		for ( ; pfd != phead && tries; pfd = pfd->pf_next, tries--) {
#ifdef MH_R10000_SPECULATION_WAR
			if ((vmflags & VM_DMA_RW) && pfd < pfd_lowmem)
				continue;
#endif /* MH_R10000_SPECULATION_WAR */
			if ((qi = tiletoqi(pfdtotile(pfd))) < qigood) {
				pfdgood = pfd;
				if ((qigood = qi) == TQ_UNLOCKED + 1)
					break;
			}
		}
	} else { /* (vmflags & VM_MVOK) == 0 */
		tries = page_nomv_tries;

		/*
		 * Try harder if we're running low on unlocked tiles.
		 */
		if (tq_ntiles(TQ_UNLOCKED) < tiles_unlocked_lowat)
			tries <<= 1;

		/*
		 * Try harder if someone is actively trying
		 * to allocate tiles.
		 */
		if (talloc_busy)
			tries <<= 1;

		qigood = TQ_UNLOCKED - 1;
		for ( ; pfd != phead && tries; pfd = pfd->pf_next, tries--) {

#ifdef MH_R10000_SPECULATION_WAR
			if ((vmflags & VM_DMA_RW) && pfd < pfd_lowmem)
				continue;
#endif /* MH_R10000_SPECULATION_WAR */

			if ((qi = tiletoqi(pfdtotile(pfd))) > qigood) {
				pfdgood = pfd;
				if ((qigood = qi) == TQ_LOCKED - 1)
					break;
			}
		}
	}
	return (pfdgood);
}

/*
 * Enqueue a tile at the head of this queue.
 */
/*REFERENCED*/
static void
tq_enqhi(tile_data_t *td, int qi)
{
	tileq_t *tq;
	tile_index_t tdi;
	tile_data_t *qhead;

	tq = &tileq[qi];
	tdi = tiletotdi(td);

	if (!tq_empty(qi)) {
		qhead = tditotile(tq->tq_head);

		td->td_next = tq->tq_head;
		td->td_prev = 0;
		tq->tq_head    = tdi;
		qhead->td_prev = tdi;
	} else {
		td->td_next = td->td_prev = 0;
		tq->tq_head = tq->tq_tail = tdi;
	}

        tq->tq_ntiles++;

#ifdef DEBUG
	if (tq->tq_ntiles > tq->tq_ntilesmax)
		tq->tq_ntilesmax = tq->tq_ntiles;
#endif
	if (qi > 0 && qi < TQ_LOCKED) {
		tileinfo.tfrag += TILE_PAGES;
		tileinfo.fraglock += td->td_nlocked;
	} else if (qi == 0) {
		tileinfo.tavail += TILE_PAGES;
		tile_class_cnt[td->td_class]++;
	} else if (qi == TQ_LOCKED || qi == TQ_LOWMEM) {
		tileinfo.tfull += TILE_PAGES;
	}

#ifdef TILE_DEBUG
	if (qi == 0)
		tile_check(td);
#endif
}

/*
 * Enqueue a tile at the tail of this queue.
 */
static void
tq_enqti(tile_data_t *td, int qi)
{
	tileq_t *tq;
	tile_index_t tdi;
	tile_data_t *qtail;

	tq = &tileq[qi];
	tdi = tiletotdi(td);

	ASSERT(td->td_next == (tile_index_t)-1 || td->td_next == 0);
	ASSERT(td->td_nlocked == qi || qi > TILE_PAGES);

	if (!tq_empty(qi)) {
		qtail = tditotile(tq->tq_tail);

		td->td_next = 0;
		td->td_prev = tq->tq_tail;
		qtail->td_next = tdi;
		tq->tq_tail    = tdi;
	} else {
		td->td_next = td->td_prev = 0;
		tq->tq_head = tq->tq_tail = tdi;
	}

	tq->tq_ntiles++;

#ifdef DEBUG
	if (tq->tq_ntiles > tq->tq_ntilesmax)
		tq->tq_ntilesmax = tq->tq_ntiles;
#endif

	if (qi > 0 && qi < TQ_LOCKED) {
		tileinfo.tfrag += TILE_PAGES;
		tileinfo.fraglock += td->td_nlocked;
	} else if (qi == 0) {
		tileinfo.tavail += TILE_PAGES;
		tile_class_cnt[td->td_class]++;
	} else if (qi == TQ_LOCKED || qi == TQ_LOWMEM) {
		tileinfo.tfull += TILE_PAGES;
	}

#ifdef TILE_DEBUG
	if (qi == 0) {
		tile_check(td);
	}
#endif
}

/*
 * Dequeue this tile from its queue.
 */
/*REFERENCED*/
static void
tq_deqi(tile_data_t *td, int qi)
{
	tileq_t *tq;
	tile_data_t *tnext, *tprev;
	tile_data_t *qhead, *qtail;

	tq = &tileq[qi];

	ASSERT(tq->tq_ntiles > 0);
	ASSERT(td->td_next != (tile_index_t)-1);
	ASSERT(td->td_nlocked == qi || \
	       (qi == TQ_ECCSTALE && td->td_nlocked == TILE_PAGES));

	qhead = tditotile(tq->tq_head);
	qtail = tditotile(tq->tq_tail);

	if (td == qhead) {
		tq->tq_head = td->td_next;
	} else {
		tprev = tditotile(td->td_prev);
		tprev->td_next = td->td_next;
	}

	if (td == qtail) {
		tq->tq_tail = td->td_prev;
	} else {
		tnext = tditotile(td->td_next);
		tnext->td_prev = td->td_prev;
	}

	td->td_next = -1;
	tq->tq_ntiles--;

	if (qi > 0 && qi < TQ_LOCKED) {
		tileinfo.tfrag -= TILE_PAGES;
		tileinfo.fraglock -= td->td_nlocked;
	} else if (qi == 0 || qi == TQ_ECCSTALE) {
		tile_class_cnt[td->td_class]--;
		tileinfo.tavail -= TILE_PAGES;
	} else if (qi == TQ_LOCKED || qi == TQ_LOWMEM) {
		tileinfo.tfull -= TILE_PAGES;
	}
}

/*ARGSUSED*/
static int
tile_evict(tile_data_t *td, int flags)
{
	int rc;

	rc = lpage_evict(0, tiletopfd(td), TILE_INDEX);

	tileinfo.tallocmv++;

	return (rc);
}

/*
 * tile_migr_pagealloc()
 *
 * We do some extra stuff when migrating pages for tiles:
 *	- large page splitting if no movable 4K pages available
 *	- color validation
 */
pfd_t *
tile_migr_pagealloc(cnodeid_t cnode, pfn_t pfn)
{
	pfd_t *pfd;


	pfd = _pagealloc_size_node(cnode, pfd_to_vcolor(pfntopfdat(pfn)),
			(vce_avoidance) ? (VM_MVOK|VM_VACOLOR|VM_STALE)
				   : (VM_MVOK|VM_STALE), NBPP);
	if (pfd)
		COLOR_VALIDATION(pfd, pfd_to_vcolor(pfd), 0, 0);

	return (pfd);
}

static void
tile_print(tile_data_t *td)
{
	if (td < first_tile || td > last_tile) {
		qprintf("%x: invalid tile address\n", td);
		return;
	}
	qprintf("tile 0x%x (0x%x) istile %d class %d nlocked %d (%x)\n"
		"    next 0x%x prev 0x%x pfdat 0x%x\n",
		td, tiletotdi(td), td->td_tile, tiletoclass(tiletopfn(td)),
		td->td_nlocked, td->td_locked,
		td->td_next, td->td_prev, tiletopfd(td));
#ifdef _TILE_TRACE
	qprintf("    alloc 0x%x free 0x%x\n", td->td_allocra, td->td_freera);
#endif
}
static void
tile_dump(__psint_t p1)
{
	tile_data_t *td;

	if (p1 == -1) {
		qprintf("tile_data 0x%x class_max 0x%x\n",
			tile_data, tile_class_max);
		qprintf("first 0x%x (%d) first_full 0x%x (%d)\n",
			first_tile, tiletotdi(first_tile),
			first_full_tile, tiletotdi(first_full_tile));
		qprintf("last_full 0x%x (%d) last 0x%x (%d)\n",
			last_full_tile, tiletotdi(last_full_tile),
			last_tile, tiletotdi(last_tile));

		for (td = first_tile; td <= last_tile; td++)
			tile_print(td);
		     
		/* Maybe it's a tile index... */
        } else if (p1 >= (int)tiletotdi(first_tile) &&
		   p1 <= (int)tiletotdi(last_tile)) {
	        td = tditotile(p1);
	        tile_print(td);
		
		/* ...maybe it's a pfdat... */
	} else if (p1 >= (int)tiletopfd(first_tile) &&
		   p1 <= (int)tiletopfd(last_tile)) {
		td = pfdtotile((pfd_t *)p1);
		tile_print(td);

		/* ...maybe it's a pfn... */
	} else if (p1 >= (int)tiletopfn(first_tile) &&
		   p1 <= (int)tiletopfn(last_tile)) {
		td = pfntotile((pfn_t)p1);
		tile_print(td);
		
		/* ...maybe it's a tile pointer. */
	} else {
		td = (tile_data_t *)p1;
		tile_print(td);
	}
}

static void
tileq_print(int qi, int pelems)
{
	pfd_t *pfd;
	tile_data_t *td;
	tileq_t *tq;

	tq = &tileq[qi];

#ifdef DEBUG
	qprintf("%d: ntiles %d ntilesmax %d head 0x%x tail 0x%x\n",
		tq-tileq, tq->tq_ntiles, tq->tq_ntilesmax,
		tq->tq_head, tq->tq_tail);
#else
	qprintf("%d: ntiles %d head 0x%x tail 0x%x\n",
		tq-tileq, tq->tq_ntiles, tq->tq_head, tq->tq_tail);
#endif /* DEBUG */
	
	if (qi == TQ_UNLOCKED) {
		int class, free;

		for (class = 0; class < tile_class_max; class += 4)
			qprintf("    class%d: %d  class%d: %d  class%d: %d  class%d: %d\n",
				class + 0, tile_class_cnt[class + 0],
				class + 1, tile_class_cnt[class + 1],
				class + 2, tile_class_cnt[class + 2],
				class + 3, tile_class_cnt[class + 3]);

		free = 0;
		for (td = TQ_FIRST(qi); td; td = TD_NEXT(td)) {
			if (tiletopfd(td)->pf_flags & P_LPG_INDEX)
				free++;

			for (pfd = tiletopfd(td);
			     pfd < tiletopfd(td) + TILE_PAGES; pfd++) {
				if (pfd->pf_flags & P_DUMP) {
					qprintf("tile[0x%x]: kernel page @ 0x%x\n", tiletotdi(td), pfd);
				}
			}
		}
		qprintf("qfree: %d\n", free);
	}

	if (pelems)
		for (td = TQ_FIRST(qi); td; td = TD_NEXT(td))
			tile_print(td);
}

static void
tileq_dump(__psint_t x)
{
	if (x == -1) {
		for (x = 0; x < TQ_NQUEUES; x++) {
			if (tileq[x].tq_ntiles)
				tileq_print(x, 0);
		}
	} else if (x >= 0 && x < TQ_NQUEUES)
		tileq_print(x, 1);
}

#ifdef TILE_DEBUG
extern void idbg_dopfdat(pfd_t *pfd);

static void
tilepfn_dump(__psint_t x)
{
	tile_data_t *td = (tile_data_t *)x;
	int pfn;
	int i;

	pfn = tiletopfn(td);
	for (i = 0; i < TILE_PAGES; i++) {
		(void)idbg_dopfdat(pfntopfdat(pfn+i));
	}
}

static void
tile_check(tile_data_t *td)
{
	pfd_t *pfd;

	for (pfd = tiletopfd(td); pfd < tiletopfd(td) + TILE_PAGES; pfd++) {
		if (pfd->pf_flags & P_DUMP) {
			qprintf("tile[0x%x]: kernel page @ 0x%x\n",
			       tiletotdi(td), pfd);
		}
	}
}

static void
tile_sync(tile_data_t *td)
{

	pfd_t *pfd;
	int cnt = 0, i;

	for (i = 0; i < TILE_PAGES; i++) {
		if (td->td_locked & (1 << i))
			cnt++;
	}

	if (cnt != td->td_nlocked) {
		printf("tile_sync: td 0x%x bitcnt %d cnt %d\n", td, cnt,
		       td->td_nlocked);
	}

	cnt = 0;
	for (pfd = tiletopfd(td); pfd < tiletopfd(td) + TILE_PAGES; pfd++)
		if (pfd->pf_flags & P_DUMP)
			cnt++;

	if (cnt > td->td_nlocked) {
		printf("tile_sync: td 0x%x dumpcnt %d cnt %d\n", td, cnt,
		       td->td_nlocked);
	}
}
#endif /* TILE_DEBUG */

#ifdef _TILE_ECC_CHECK
#include <sys/sbd.h>

extern int pdcache_size;
extern void _c_ilt(int,caddr_t,uint_t *);

int	tile_cache_check_bad_lines = 0;

static void
tile_cache_check(pfn_t tpn)
{
	uint_t tags[2];
	caddr_t	ap;
	caddr_t	al;

	if (IS_R10000())
		return;

	for (ap = (caddr_t) K0BASE, al = ap + pdcache_size; ap < al; ap += 32) {
		_c_ilt(CACH_PD,ap,tags);
		if (((tags[0] & PSTATEMASK) == PDIRTYEXCL) &&
		    ((((tags[0] & PADDRMASK) >> (12 - PADDR_SHIFT)) & 0xFFF0) == 
		     (tpn & 0xFFF0))) {
			tile_cache_check_bad_lines++;
			cmn_err_tag(283,CE_WARN,"Dirty cache tag 0x%x at cache offset 0x%x for tile 0x%x\n",
				    tags[0],(ap - ((caddr_t) K0BASE)),tpn);
		}
	}
}
#endif /* _TILE_ECC_CHECK */

#endif /* TILES_TO_LPAGES */
