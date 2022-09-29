/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1999, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_LPAGE_H__
#define __SYS_LPAGE_H__

#ifdef	__cplusplus
extern "C" {
#endif

#ident "$Revision: 1.40 $"
#include <sys/immu.h>		/* Generic low level stuff	*/
#include <sys/page.h>


typedef	signed char	pgszindx_t;	/* Page size index type */
typedef	signed char	pgmaskindx_t;	/* Page mask index type */
typedef char		*bitmap_t;	/* Bitmap type */

typedef	enum {
	PAGE_ALLOC,
	PAGE_FREE
} pageinit_t;

/* Number of allocable page sizes. 64k, 256k, 1M, 4M, 16M */

#define	MAX_PAGE_SIZE		(16*1024*1024)
#define	PAGE_SIZE_SHIFT		2   /* Shift to get to next higher page size */
#define	MIN_PGSZ_INDEX		0   /* Page idx for NBPP */
#define	PGSZ_INDEX_SHIFT	14  /* Bits to shift to set the pg_sz_indx in 
					the pf_flag */
#if _PAGESZ == 4096
#define	MAX_PGSZ_INDEX		6   /* Max. page size index */
#endif

#if _PAGESZ == 16384
#define	MAX_PGSZ_INDEX		5   /* Max. page size index */
#endif

/*
 * Sentinel page index is set on pages which are part of a large page but
 * not the beginning of the large page.
 */

#define	SENTINEL_PGSZ_INDEX	(MAX_PGSZ_INDEX+1)

/*
 * Number of pages of the next smaller page size in a large page.
 * For eg. a 1MB page has 4 256K pages and 256K page has 4 64K pages.
 */

#define	PAGES_PER_LARGE_PAGE	4

/*
 * The flag to _pagefree_size which tells it to not coalesce the page.
 */

#define	 PAGE_FREE_NOCOALESCE	0x1

/*
 * Percentage of hiwater mark that represents the low water mark.
 */
#define	PERCENT_LPAGE_LOWAT	10

/*
 * Large page pte operations.
 * Page size to page mask index conversion.
 * Page mask index is a representation of page size in the ptes.
 * It has been defined so that it is easy to compute the page mask
 * for a particular page size from the the page mask index.
 * page_mask = (1 << page_mask_index) - 1
 * Its mask index is the number of bits to shift 1 to get the page mask.
 * page_mask_index = page_size_index*2 + 2;
 * The page_mask_index is 0 for base page size. The page mask index field is 
 * set only forlarge page ptes.
 */

#define	MIN_PAGE_MASK_INDEX	0  /* Page mask index for base  page size */
#define	PAGE_MASK_SHFT          1


#if defined (BEAST)

#if NBPP == 4096
#define	PAGE_MASK_FACTOR	0
#elif NBPP == 16384
#define	PAGE_MASK_FACTOR	1
#else
#error <PAGE_MASK_FACTOR undefined>
#endif

#else /* BEAST */

#if NBPP == 4096
#define	PAGE_MASK_FACTOR	0
#elif NBPP == 16384
#define	PAGE_MASK_FACTOR	2
#else
#error <PAGE_MASK_FACTOR undefined>
#endif

#endif /* BEAST */



#if defined (BEAST)

#define	PAGE_SIZE_TO_MASK_INDEX(page_size)  \
                    ((page_size) == NBPP ? 0 :(PAGE_SIZE_TO_INDEX(page_size) + PAGE_MASK_FACTOR))

#define	PAGE_MASK_INDEX_TO_PGSZ_INDEX(page_mask_index) \
                    ((page_mask_index) - PAGE_MASK_FACTOR)
#else

#define	PAGE_SIZE_TO_MASK_INDEX(page_size)  ((page_size) == NBPP ? 0 : \
	((PAGE_SIZE_TO_INDEX(page_size) << PAGE_MASK_SHFT) + PAGE_MASK_FACTOR))

#define	PAGE_MASK_INDEX_TO_PGSZ_INDEX(page_mask_index) \
		((page_mask_index) ? (((page_mask_index) - PAGE_MASK_FACTOR) \
				>> PAGE_MASK_SHFT) : 0)
#endif

#define	PAGE_MASK_INDEX_TO_SIZE(page_mask_index) \
	PGSZ_INDEX_TO_SIZE(PAGE_MASK_INDEX_TO_PGSZ_INDEX(page_mask_index))

#define	PAGE_MASK_INDEX_TO_CLICKS(page_mask_index) \
	PGSZ_INDEX_TO_CLICKS(PAGE_MASK_INDEX_TO_PGSZ_INDEX(page_mask_index))

/*
 * Page index and size conversion macros.
 */

#define	PGSZ_INDEX_TO_SIZE(pg_sz_indx)\
				(NBPP << ((pg_sz_indx)*PAGE_SIZE_SHIFT))

#define	PGSZ_INDEX_TO_CLICKS(pg_sz_indx)\
				(1 << ((pg_sz_indx)*PAGE_SIZE_SHIFT))

#define	PAGE_SIZE_TO_INDEX(size)	((size) == NBPP ? MIN_PGSZ_INDEX \
						: lpage_size_to_index(size))

/*
 * Maximum number of pfds in a largest page supported (16MB)
 */

#define	MAX_NUM_PAGES		PGSZ_INDEX_TO_CLICKS(MAX_PGSZ_INDEX)

/*
 * Pfdat to page size index and page size conversions.
 */
#define	PFDAT_TO_PGSZ_INDEX(pfd)	(((pfd)->pf_flags & P_LPG_INDEX)\
						>>  PGSZ_INDEX_SHIFT)

#define	PFDAT_TO_PAGE_SIZE(pfd)	 (PGSZ_INDEX_TO_SIZE(PFDAT_TO_PGSZ_INDEX(pfd)))

#define	PFDAT_TO_PAGE_CLICKS(pfd) (PGSZ_INDEX_TO_CLICKS(\
					PFDAT_TO_PGSZ_INDEX(pfd)))

#define	NODE_FREE_PAGES(cnode, pg_sz_indx)	\
		(NODEPDA(cnode)->node_pg_data.num_free_pages[(pg_sz_indx)])

#define	PHEADROTOR(cnode, pg_sz_indx)	\
		(NODEPDA(cnode)->node_pg_data.pheadrotor[(pg_sz_indx)])

/*
 * Setting and clearing page size indices in pfdats.
 */


#define	CLR_PAGE_SIZE_INDEX(pfd)	(pfd->pf_flags &= \
						~(P_LPG_INDEX))

#define	SET_PAGE_SIZE_INDEX(pfd, pg_sz_indx)  \
		{	CLR_PAGE_SIZE_INDEX(pfd); \
			pfd->pf_flags |= (pg_sz_indx << \
				PGSZ_INDEX_SHIFT) & (P_LPG_INDEX); \
		}

/*
 * Page free list macros.
 */
#define	PHEADMASK(pg_sz_indx)	(nodepda->node_pg_data.pheadmask[(pg_sz_indx)])
#define	PHEADSHIFT(pg_sz_indx)	(nodepda->node_pg_data.pheadshift[(pg_sz_indx)])

#ifdef TILES_TO_LPAGES
#define	PFD_TO_PHEAD(pfd, pg_sz_indx) \
	(&((GET_NODE_PFL(pfdattocnode((pfd_t*)(pfd)))->phead[(pg_sz_indx)])\
		[(pfdattopfn((pfd_t*)(pfd)) & PHEADMASK(pg_sz_indx)) +\
		((pg_sz_indx == MIN_PGSZ_INDEX) ?\
		 (pfdattotpindex(pfd) * (PHEADMASK(pg_sz_indx)+1)) : 0)]))
#else
#define	PFD_TO_PHEAD(pfd, pg_sz_indx) \
	(&((GET_NODE_PFL(pfdattocnode((pfd_t*)(pfd)))->phead[(pg_sz_indx)])\
			[((pfdattopfn((pfd_t*)(pfd))>>(2*pg_sz_indx)) \
				& PHEADMASK(pg_sz_indx))]))
#endif /* TILES_TO_LPAGES */

/*
 * Get and set phead rotor.
 */

#define	GET_PHEADROTOR(cnode, pg_sz_indx)	PHEADROTOR(cnode, pg_sz_indx) 
#define	SET_PHEADROTOR(cnode, pg_sz_indx, val) 	(PHEADROTOR(cnode, pg_sz_indx) \
							= (val))
/*
 * Operations on node free pages per page size.
 */

#define	GET_NUM_FREE_PAGES(cnode, pg_sz_indx) NODE_FREE_PAGES(cnode, \
						pg_sz_indx)

#define	INC_NUM_FREE_PAGES(cnode, pg_sz_indx) \
	(NODE_FREE_PAGES(cnode, pg_sz_indx)++)

#define	DEC_NUM_FREE_PAGES(cnode, pg_sz_indx) \
	(NODE_FREE_PAGES(cnode, pg_sz_indx)--)

#define	ADD_NODE_FREE_PAGES(cnode, pg_sz_indx, npgs) \
	NODE_FREE_PAGES(cnode, pg_sz_indx) += (npgs)

#define	SUB_NODE_FREE_PAGES(cnode, pg_sz_indx, npgs) \
	NODE_FREE_PAGES(cnode, pg_sz_indx) -= (npgs)
                
/*
 * Operations on total pages per page size.
 */

#ifdef	LPG_STAT
#define	NODE_TOTAL_PAGES(cnode, pg_sz_indx)	\
		(NODEPDA(cnode)->node_pg_data.num_total_pages[(pg_sz_indx)])

#define	GET_NUM_TOTAL_PAGES(cnode, pg_sz_indx)	NODE_TOTAL_PAGES(cnode, \
						pg_sz_indx)

#define	INC_NUM_TOTAL_PAGES(cnode, pg_sz_indx) \
	(NODE_TOTAL_PAGES(cnode, pg_sz_indx)++)
					
#define	DEC_NUM_TOTAL_PAGES(cnode, pg_sz_indx) \
	(NODE_TOTAL_PAGES(cnode, pg_sz_indx)--)

#define	ADD_NODE_TOTAL_PAGES(cnode, pg_sz_indx, npgs) \
	NODE_TOTAL_PAGES(cnode, pg_sz_indx) += (npgs)

#define	SUB_NODE_TOTAL_PAGES(cnode, pg_sz_indx, npgs) \
	NODE_TOTAL_PAGES(cnode, pg_sz_indx) -= (npgs)
#else
#define	NODE_TOTAL_PAGES(cnode, pg_sz_indx)	0
#define	GET_NUM_TOTAL_PAGES(cnode, pg_sz_indx)	0
#define	INC_NUM_TOTAL_PAGES(cnode, pg_sz_indx)
#define	DEC_NUM_TOTAL_PAGES(cnode, pg_sz_indx)
#define	ADD_NODE_TOTAL_PAGES(cnode, pg_sz_indx, npgs) 
#define	SUB_NODE_TOTAL_PAGES(cnode, pg_sz_indx, npgs)
#endif

#define	GET_LPAGE_LOWAT(pfl, pg_sz_indx) \
		(((pfl)->hiwat[(pg_sz_indx)] * PERCENT_LPAGE_LOWAT)/100)

/*
 * Flags to be passed to the coalesced wakeup routine.
 */
#define	CO_DAEMON_TIMEOUT_WAIT	1 /* Wake up if daemon has timedout */
#define	CO_DAEMON_IDLE_WAIT	2 /* Wakeup if daemon is idle */
#define CO_DAEMON_FULL_SPEED	4 /* Set the daemon to run at full speed */
#define CO_DAEMON_TIMEOUT_EXPIRED	8 /* Woken as timeout expired */

/*
 * Coalescing daemon policy types.
 */
#define	STRONG_COALESCING	0
#define	LIGHT_COALESCING	1
#define	MILD_COALESCING		2
#define TILE_COALESCING		3

/*
 * Bitmap operations.
 *
 * There are two kinds of bitmaps. 
 * Pure bitmap contains only those free pages which are not poisoned and which
 * are not in the page cache. This is used by the page_coalesce() routine in
 * page free.
 * Tainted bitmap contains all free pages (including poisoned and cached pages).
 *
 * If discontiguous memory, there is one bitmask of each type per slot.
 */
 
#ifdef	DISCONTIG_PHYSMEM
#define PAGE_BITMAPS_NUMBER	(MAX_NASIDS * MAX_MEM_SLOTS)
#define page_bitmaps_index(nasid,slot)	((nasid)*MAX_MEM_SLOTS+(slot))
extern bitmap_t page_bitmaps_pure[PAGE_BITMAPS_NUMBER];
extern bitmap_t page_bitmaps_tainted[PAGE_BITMAPS_NUMBER];

#define	pfdat_to_pure_bm(pfd)		(page_bitmaps_pure[page_bitmaps_index(pfdattonasid(pfd),pfdat_to_slot_num(pfd))])

#define	pfdat_to_tainted_bm(pfd)	(page_bitmaps_tainted[page_bitmaps_index(pfdattonasid(pfd),pfdat_to_slot_num(pfd))])

#define	pfdat_to_bit(pfd)		(pfdat_to_slot_offset(pfd))

#else /* DISCONTIG_PHYSMEM */
extern bitmap_t page_bitmap_pure;
extern bitmap_t page_bitmap_tainted;

#define	pfdat_to_pure_bm(pfd)		page_bitmap_pure
#define	pfdat_to_tainted_bm(pfd)	page_bitmap_tainted

#ifdef TILES_TO_LPAGES
#define pfdat_to_bit(pfd)		(pfdat_to_eccpfn(pfd))
#else
#define	pfdat_to_bit(pfd)		(pfdattopfn(pfd))
#endif /* TILES_TO_LPAGES */

#endif /* DISCONTIG_PHYSMEM */

/*
 * Get the starting pfdat of large page of size (size) which includes
 * a given pfdat(pfd).
*/

#define	pfdat_to_large_pfdat(pfd,size)	(pfntopfdat(pfdattopfn(pfd)&\
								(~((size) -1))))

/*
 * Free bitmap operations.
 */

#define	set_pfd_bit(bm, pfd)	bset(bm, pfdat_to_bit(pfd))
#define	clr_pfd_bit(bm, pfd)	bclr(bm, pfdat_to_bit(pfd))

#define	set_pfd_bitfield(bm, pfd, len)	bfset(bm, pfdat_to_bit(pfd), len)
#define	clr_pfd_bitfield(bm, pfd, len)	bfclr(bm, pfdat_to_bit(pfd), len)

/*
 * Interval after which second_thread() calls coalesced_kick(). 10 sec.
 */
#define	COALESCED_KICK_PERIOD	10

#if defined(IP27) || defined(IP25) || defined(IP19)
#define	COALESCED_KICK()	coalesced_kick()
#else
#define	COALESCED_KICK()	
#endif


extern void lpage_init_watermarks(cnodeid_t);
extern void lpage_init(void);
extern void lpage_coalesce(cnodeid_t, pfd_t *);
extern int  lpage_size_to_index(size_t);
extern pfd_t *lpage_split(cnodeid_t, pgszindx_t, pgszindx_t, pfd_t *);
extern void lpage_init_pfdat(pfd_t *, size_t, pageinit_t);
extern pfd_t *lpage_alloc_contig_physmem(cnodeid_t, size_t, pgno_t, int);
extern void lpage_free_contig_physmem(pfd_t *, size_t);
extern int  lpage_release_buffer_reference(pfd_t *pfd, int);	
extern size_t lpage_lower_pagesize(size_t);
#ifdef TILES_TO_LPAGES
extern int lpage_evict(cnodeid_t, pfd_t *, pgszindx_t);
#endif /* TILES_TO_LPAGES */

extern void coalesce_daemon_wakeup(cnodeid_t, int);
extern void coalesced_kick(void);

struct nodeinfo;
struct lpg_stat_info;
extern void collect_node_info(struct nodeinfo *);
extern void collect_lpg_stats(struct lpg_stat_info *);

/*
 * Tunable parameter variables.
 */

extern int nlpages_16m;         /* 16 Mbyte pages */
extern int nlpages_4m;          /* 4 Mbyte pages */
extern int nlpages_1m;          /* 1 Mbyte pages */
extern int nlpages_256k;        /* 256 Kbyte pages */
extern int nlpages_64k;		/* 64K pages */

extern int large_pages_enable;

#ifdef	DEBUG
extern int check_freemem(void);
extern	int do_full_throttle;
#endif

/*
 * Statistics defines.
 */

#ifdef	LPG_STAT
#define	CO_DAEMON_SPLIT		0
#define	PAGE_COALESCE_ATT	1
#define	PAGEALLOC_TIME		2
#define	PAGEFREE_TIME		3
#define	PAGE_COALESCE_HITS	4
#define	PAGE_SPLIT_ATTS		5
#define	PAGE_SPLIT_HITS		6
#define	CO_DAEMON_ATT		7
#define	CO_DAEMON_HITS		8
#define	PAGE_SPLIT_WAKEUPS	9
#define	CO_DAEMON_MIG_FAIL	10
#define	CO_DAEMON_SUCCESS	11
#define	PAGE_SPLIT_FAILS	12
#define	CO_DAEMON_MIG_SUCC	13
#define	CO_DAEMON_TEST_MIG_FAIL	14
#define	CO_DAEMON_TEST_MIG_SUCC	15
#define	CO_DAEMON_KERNEL_PAGE	16
#define	LPG_VFAULT_ATT		17
#define	LPG_VFAULT_SUCCESS	18
#define	LPG_PFAULT_ATT		19
#define	LPG_PFAULT_SUCCESS	20
#define	LPG_PAGEALLOC_SUCCESS	21
#define	LPG_PAGEALLOC_FAIL	22
#define	LPG_PAGE_DOWNGRADE	23
#define	LPG_PAGEALLOC_ONEP	24
#define	CO_DAEMON_PUSH_SUCC	25
#define	LPG_VFAULT_RETRY_SAME_PGSZ	26
#define LPG_VFAULT_RETRY_LOWER_PGSZ	27	
#define	LPG_PFAULT_RETRY_SAME_PGSZ	28
#define LPG_PFAULT_RETRY_LOWER_PGSZ	29
#define	LPG_SWAPIN_RETRY_SAME_PGSZ	30
#define LPG_SWAPIN_RETRY_LOWER_PGSZ	31	


#define	INC_LPG_STAT(node, pg_sz_indx, stype)	\
	NODEPDA(node)->node_pg_data.lpage_stats[(pg_sz_indx)][(stype)]++

#define	DEC_LPG_STAT(node, pg_sz_indx, stype)	\
	NODEPDA(node)->node_pg_data.lpage_stats[(pg_sz_indx)][(stype)]--

#define	SET_LPG_STAT(node, pg_sz_indx, stype, val)	\
	NODEPDA(node)->node_pg_data.lpage_stats[(pg_sz_indx)][(stype)] = (val)

#define	GET_LPG_STAT(node, pg_sz_indx, stype)	\
	NODEPDA(node)->node_pg_data.lpage_stats[(pg_sz_indx)][(stype)]

#else
#define	INC_LPG_STAT(node, pg_sz_indx, stype)
#define	DEC_LPG_STAT(node, pg_sz_indx, stype)
#define	SET_LPG_STAT(node, pg_sz_indx, stype, val)
#define	GET_LPG_STAT(node, pg_sz_indx, stype, val)	0
#endif

#ifdef	__cplusplus
}
#endif

#endif /* __SYS_LPAGE_H__ */
