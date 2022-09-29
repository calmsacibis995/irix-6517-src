#ifndef __SYS_TILE_H__
#define __SYS_TILE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sys/tile.h
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

#ident	"$Revision: 1.8 $"

#define TILE_SIZESHIFT	16			/* log2(TILE_SIZE) */
#define TILE_SIZE	(1 << TILE_SIZESHIFT)	/* 64k bytes per tile */
#define TILE_PAGES	(TILE_SIZE / NBPP)	/* pages per tile */
#define TILE_PAGEMASK	(TILE_PAGES - 1)	/* tile page bits of a pfn */
#define TILE_PAGESHIFT	(TILE_SIZESHIFT - PNUMSHFT) /* log2(TILE_PAGES) */
#define TILE_INDEX	PAGE_SIZE_TO_INDEX(TILE_SIZE)	/* from lpage.h */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

typedef ushort_t	tile_index_t;

typedef struct tile_data_s {
	tile_index_t td_next;	/* next tile on this queue */
	tile_index_t td_prev;	/* previous tile on this queue */
	unsigned td_locked:16;	/* locked pages bitmap */
	unsigned td_nlocked:5;	/* # of locked pages */
	unsigned td_class:4;	/* tile class (memory bank) */
 	unsigned td_tile:1;	/* allocated through tile_alloc */
	unsigned td_pad_:6;
#ifdef _TILE_TRACE
	__psint_t td_allocra;
	__psint_t td_freera;
#endif
} tile_data_t;


#define tile_pfnbit(pfn)		(1 << ((pfn) & TILE_PAGEMASK))

#define TILE_SETLOCKED(td, pfn)		((td)->td_locked |=  tile_pfnbit(pfn))
#define TILE_CLRLOCKED(td, pfn)		((td)->td_locked &= ~tile_pfnbit(pfn))
#define TILE_ISLOCKED(td, pfn)		((td)->td_locked & tile_pfnbit(pfn))

/* tile pointer to tile_data index, and back. */
#define tiletotdi(td)			((td) - tile_data)
#define tditotile(tdi)			(&tile_data[tdi])

/* tile_data index to page frame number, and back.
 * tditopfn goes through pfdattopfn/pfntopfdat conversion to generate
 * pfn in ECCBYPASS address range if necessary.
 */
#define tditoeccpfn(tdi)                ((tdi) << TILE_PAGESHIFT)
#define tditopfn(tdi) \
                  (pfdattopfn(pfntopfdat(tditoeccpfn(tdi))))
#define pfntotdi(pfn)                   (pfn_to_eccpfn(pfn) >> TILE_PAGESHIFT)
	  
/* tile pointer to page frame number, and back. */
#define tiletopfn(td)                   tditopfn(tiletotdi(td))
#define tiletoeccpfn(td)                tditoeccpfn(tiletotdi(td))
#define pfntotile(pfn)                  tditotile(pfntotdi(pfn))

/* tile pointer to pfdat, and back. */
#define tiletopfd(td)			pfntopfdat(tiletopfn(td))
#define pfdtotile(pfd)			pfntotile(pfdattopfn(pfd))

/* tile class of this tile. */
#define tiletoclass(tpfn)		(pfntotile(tpfn)->td_class)

#define page_in_unfrag_tile(pfd)	(pfdtotile(pfd)->td_nlocked == 0)

typedef struct tileq_s {
    tile_index_t	tq_head;	/* first tile in queue */
    tile_index_t	tq_tail;	/* last  tile in queue */
    int			tq_ntiles;	/* count of tiles in queue */
#ifdef DEBUG
    int			tq_ntilesmax;	/* max length this queue has been */
#endif /* DEBUG */
} tileq_t;

#define TQ_UNLOCKED	(0)		/* queue index of unlocked tiles */
#define TQ_LOCKED	TILE_PAGES	/* queue index of locked tiles */
#define TQ_ECCSTALE     (TILE_PAGES+1)  /* queue index of ECC-stale tiles */
#ifdef MH_R10000_SPECULATION_WAR
#define TQ_LOWMEM	(TILE_PAGES+2)	/* queue index of lowmem tiles */
#endif
#define TQ_HOLE		(TILE_PAGES+3)	/* queue index of tiles in hole */
#define TQ_NQUEUES	(TQ_HOLE+1)	/* total number of queues */

#define tq_empty(qi)			(tileq[qi].tq_head == 0)
#define tq_ntiles(qi)			(tileq[qi].tq_ntiles)
#define tiletoqi(td)			((td)->td_nlocked)
#define tq_enqh(td)			tq_enqhi(td, tiletoqi(td))
#define tq_enqt(td)			tq_enqti(td, tiletoqi(td))
#define tq_deq(td)			tq_deqi(td, tiletoqi(td))

/*
 * TILE_CLASS_MAX is the maximum number of memory banks
 * returned by addr_to_bank().
 */
#ifdef IP22
#define TILE_CLASS_MAX		4
#endif
#ifdef IP32
#define TILE_CLASS_MAX		16
#endif
#ifndef TILE_CLASS_MAX
/* default to single class/bank */
#define TILE_CLASS_MAX		1
#endif

/* classmask for "don't care" class for tile_alloc() */
#define TILE_ANYCLASS		(-1)	/* don't care which tile class */

/* flags options for tile_alloc() and tile_free() */
#define TILE_SLEEP		0x0	/* ok to sleep for tiles (default) */
#define TILE_NOSLEEP		0x1	/* don't sleep for tiles */
#define TILE_CLASS_SINGLE	0x2	/* all tiles from same class */
#define TILE_ZERO		0x4	/* zero tile pages */
#define TILE_ECCSTALE		0x8	/* stale ecc tiles */
#define TILE_NORESERVEMEM	0x10	/* don't reservemem for tiles */
#ifdef _KERNEL

extern tile_data_t *tile_data;		/* tile table */
#ifdef MH_R10000_SPECULATION_WAR
extern tile_data_t *tile_lowmem;        /* lowest tile for general use */
#endif /* MH_R10000_SPECULATION_WAR */

void tile_init(pfn_t, pfn_t, int);

void tile_pagelock(pfd_t *, int);
void tile_pageunlock(pfd_t *, int);

int tile_alloc(int, pfn_t *, int, int);
void tile_free(int, pfn_t *, int);
int tile_nclasses(void);
caddr_t tile_mapin(pfn_t, int);
void tile_mapout(caddr_t);

pfd_t *tile_migr_pagealloc(cnodeid_t, pfn_t);

#define COALESCED_TILES()	coalesce_daemon_wakeup(0, CO_DAEMON_FULL_SPEED)

#ifdef TILES_TO_LPAGES
#define TILE_PAGELOCK(pfd,cnt)		tile_pagelock(pfd,cnt)
#define TILE_PAGEUNLOCK(pfd,cnt)	tile_pageunlock(pfd,cnt)
#define TILE_PAGECONDLOCK(pfd,cnt)	if (!TILE_ISLOCKED(pfdtotile(pfd),\
							  pfdattopfn(pfd))) \
					         TILE_PAGELOCK(pfd,cnt)
#define TILE_TPHEAD_INC_PAGES(pfd)	pfdattotphead(pfd)->count++;
#define TILE_TPHEAD_DEC_PAGES(pfd)	pfdattotphead(pfd)->count--;
#else
#define TILE_PAGELOCK(pfd,cnt)
#define TILE_PAGEUNLOCK(pfd,cnt)
#define TILE_PAGECONDLOCK(pfd,cnt)
#define TILE_TPHEAD_INC_PAGES(pfd)
#define TILE_TPHEAD_DEC_PAGES(pfd)
#endif /* TILES_TO_LPAGES */
#endif /* _KERNEL */

#endif /* C || C++ */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_TILE_H__ */
