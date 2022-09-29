/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_PAGE_H__
#define __SYS_PAGE_H__

#ident	"$Revision: 1.39 $"

#include "sys/sema.h"

#if SN
#include <sys/SN/arch.h>
#include <sys/SN/memsupport.h>

#if defined (SN0)
#include <sys/SN/SN0/bte.h>
#endif /* SN0 */

#endif /* SN */
#include "sys/pfdat.h"

/*
 * Contains page free list and related data structure definitions.
 * Should be know only in the VM subsystem.
 */

#if EVEREST || SN || IP30

/* On Everest TFP systems, no need for a stale or in-transit queue since
 * the coherency protocol on the bus keeps all physical addresses up to date
 * and there is no danger of VCEs occuring due to 16K page size and 16K
 * d-cache & i-cache sizes.
 *
 * On non-TFP systems we will implement a scheme to avoid VCEs on
 * the kernel stack and take other VCEs. All page lists will be treated
 * as clean and the flushcaches will be avoided.
 *
 * In a SN0 system, pages may be poisonous after being migrated.
 * A poisonous page has the poisonous bit set  in all its directory entries,
 * causing any access to it to generate a "poisonous bus error",
 */
#define	CLEAN_ASSOC		0	/* Clean, disk association */
#define	CLEAN_NOASSOC		1	/* Clean, no disk association */
#define POISONOUS               2       /* Poisonous page */
#define STALE_ASSOC	        CLEAN_ASSOC
#define STALE_NOASSOC	        CLEAN_NOASSOC
#define INTRANS_ASSOC	        CLEAN_ASSOC
#define INTRANS_NOASSOC	        CLEAN_NOASSOC

#define PH_SHORT                1
#define	PH_NLISTS		3

#if SN0
#define	BTE_PAGE_POISON(addr, len)	bte_poison_range(addr, len)
#define POISON_STATE_CLEAR(pfn) 	poison_state_clear((pfn))
#else
#define	BTE_PAGE_POISON(addr, len)
#define POISON_STATE_CLEAR(pfn)
#endif

#if	defined(NUMA_BASE)
#define	PAGE_POISON(pfd)   page_poison(pfd)
#define PAGE_UNPOISON(pfd) page_unpoison((pfd))
#define PHEAD_UNPOISON(node, pheadp, pfd) phead_unpoison((node), (pheadp), (pfd))
#else
#define	PAGE_POISON(pfd)   
#define PAGE_UNPOISON(pfd)
#define PHEAD_UNPOISON(node, pheadp, pfd) 
#endif 


#else /* !EVEREST && !SN*/
/*
 * The six free page lists define the state of the page, I-cache wise
 */
#define	CLEAN_ASSOC		0	/* Clean, disk association */
#define	INTRANS_ASSOC		1	/* Being cleaned, no disk association */
#define	STALE_ASSOC		2	/* Stale, disk association */
#define	CLEAN_NOASSOC		3	/* Clean, no disk association */
#define	INTRANS_NOASSOC		4	/* Being cleaned, disk association */
#define	STALE_NOASSOC		5	/* Stale, no disk association */

#define PH_LONG                 1
#define	PH_NLISTS		6

#define POISON_STATE_CLEAR(pfn)
#define PAGE_UNPOISON(pfd)
#define PHEAD_UNPOISON(node, pheadp, pfd) 

#endif /* !EVEREST && !SN */

#if _PAGESZ == 4096
#define	NUM_PAGE_SIZES	7	/* Number of page sizes */
#elif _PAGESZ == 16384
#define NUM_PAGE_SIZES  6	/* Number of page sizes */
#else
#error	"Unknown page size"
#endif

#define	LPG_STAT	/* Large page statistics turned on for opt. kernels */
#ifdef	LPG_STAT
#define NUM_STAT_WORDS          32
#endif

#define	TRUE		1
#define	FALSE		0
#define	ALLOC_SUCCESS	TRUE
#define	ALLOC_FAIL	FALSE


/*
 * List header type for free page table -- match link pointers in pfd_t.
 */
typedef struct plist {
	struct pfdat	*pf_next;
	union {
		struct pfdat	*prev;
		sm_swaphandle_t	 swphdl;
	} p_swpun;
} plist_t;

/*
 * Free page table entry
 */
typedef struct phead {
	plist_t	ph_list[PH_NLISTS];	/* list headers */
	int	ph_count;		/* total count on all lists */
	int	ph_flushcount;
	int	ph_flags;
	int	ph_poison_count;	/* Number of poisoned pages */
} phead_t;

#ifdef TILES_TO_LPAGES
/*
 * On IP32, we partition the MIN_PGSZ_INDEX list into
 * 3 separate pools:
 *	- 4K pages in unfragmented tiles
 * 	- 4K pages in fragmented tiles
 * 	- 4K pages in low (!DMAable) memory
 */
typedef struct tphead_s {
    phead_t	*phead;			/* phead for this tile page pool */
    phead_t	*pheadend;		/* end of pheads for this pool */
    phead_t	*pheadrotor;		/* rotor for pool */
    int		count;			/* total count in all pheads */
} tphead_t;

#define NTPHEADS		3
#define TPH_UNFRAG		0
#define TPH_FRAG		1
#define TPH_LOWMEM		2

extern tphead_t tphead[];

extern void	tilepages_to_frag(pfd_t *);
extern void	tilepages_to_unfrag(pfd_t *);
#endif /* TILES_TO_LPAGES */

/*
 * Readonly structures per node. 
 * This structure contains information used allocate
 * and free pages. All the fields in the structure are computed once at
 * boot time. So they are replicated in every node.
 */

typedef struct pg_free {
	phead_t	*phead[NUM_PAGE_SIZES];		/* Heads of free page lists. */
	int	hiwat[NUM_PAGE_SIZES];
	phead_t	*pheadend[NUM_PAGE_SIZES];	/* points to just past end */
	pfd_t	*pfd_low,	/* Page with lowest address for a node  */
		*pfd_high;	/* Page with highest address for a node */
} pg_free_t;
	
/*
 * This structure encapsulates several data structres needed for page
 * allocation per node. It is embedded in the nodepda.
 */

typedef	struct pg_data {
	struct pg_free 	*pg_freelst;
	/*
	 * Lock for the free list on a specific node.
	 * This should NOT be part of pg_freelst (locks are not replicable)
	 * Access to this lock is through the nodepda of the specific node.
	 */
	lock_t		pg_freelst_lock;

	/* Number of free base(NBPP) pages per node */
	int		node_freemem;

        /*
         * Future number of free pages per node, according to memsched.
         * This field is used to implement a very light weight reservation
         * scheme. The memory scheduler tries to reserve some amount of
         * free memory on the node it places an MLD on, so that even if
         * memory hasn't been effectively allocated, the memory scheduler
         * knows that there is some potential future pressure on this
         * particular node.
         */
        int             node_future_freemem;

        /*
         * Per node free memory with NO hash association
         * Used by user level system state monitors
         */
        int             node_emptymem;
        

	/*
	 * Total number of pages present in a node after bringup.
	 * Used in calculating hiwater marks for large pages.
	 */
	int		node_total_mem;

	/* Number of free pages of a specific page size */
	int     	num_free_pages[NUM_PAGE_SIZES]; 

	/* Free page hash mask for a specific page size */

	int		pheadmask[NUM_PAGE_SIZES]; 

	/* shift amount for pheadmask  for a specific page size */

	int		pheadshift[NUM_PAGE_SIZES];

	/*
	 * Free page table rotor 
	 * for VM_UNSEQ allocations 
	 */

	phead_t		*pheadrotor[NUM_PAGE_SIZES]; 

#ifdef	LPG_STAT
	/* 
	 * Total number of large pages of a specific page size 
	 * in the system.
	 */

	int		num_total_pages[NUM_PAGE_SIZES]; 
	long		lpage_stats[NUM_PAGE_SIZES][NUM_STAT_WORDS];
#endif

        
} pg_data_t;

#ifdef _KERNEL

/*
 * Page free list operation macros
 */
#define	isempty(l)   ((l)->pf_next == (pfd_t *)(l))
#define	makeempty(l) ((l)->pf_next = (l)->pf_prev = (pfd_t *)(l))

#define	append(l, p)	{ (p)->pf_prev = (l)->pf_prev; \
			  (p)->pf_prev->pf_next = (p); \
			  (l)->pf_prev = (p); \
			  (p)->pf_next = (pfd_t *)(l); \
			}

#define	prefix(l, p)	{ (p)->pf_next = (l)->pf_next; \
			  (p)->pf_next->pf_prev = (p); \
			  (l)->pf_next = (p); \
			  (p)->pf_prev = (pfd_t *)(l); \
			}

#define	combine(lf, lb)	{ (lf)->pf_prev->pf_next = (lb)->pf_next; \
			  (lb)->pf_next->pf_prev = (lf)->pf_prev; \
			  (lf)->pf_prev = (lb)->pf_prev; \
			  (lb)->pf_prev->pf_next = (pfd_t *)(lf); \
			}


extern lock_t		memory_lock;/* spin lock for memory operations */

#define	NODE_PG_DATA(cnode)		(NODEPDA(cnode)->node_pg_data)

#define mem_lock()	mutex_spinlock(&memory_lock)
#define mem_unlock(T)	mutex_spinunlock(&memory_lock, T)

#if 0
#define	PAGE_FREELIST_LOCK(node)	mem_lock()
#define	PAGE_FREELIST_UNLOCK(node, T)	mem_unlock(T)
#ifdef DEBUG
#define	PAGE_FREELIST_ISLOCKED(node)	spinlock_islocked(&memory_lock)
#endif	/* DEBUG */
#endif

#define	PAGE_FREELIST_LOCK(node)	\
		mutex_spinlock(&NODE_PG_DATA(node).pg_freelst_lock)
#define	PAGE_FREELIST_UNLOCK(node, T)	\
		mutex_spinunlock(&NODE_PG_DATA(node).pg_freelst_lock, (T))
#ifdef	DEBUG
#define	PAGE_FREELIST_ISLOCKED(node)	\
		spinlock_islocked(&NODE_PG_DATA(node).pg_freelst_lock)
#endif	/* DEBUG */

/*
 * Return a pointer to the pg_free_t for a specific node (passed as 
 * compact node id).
 */

#define	GET_NODE_PFL(cnode)	((pg_free_t *)&(nodepda->node_pg_data.\
							pg_freelst[(cnode)]))

#define	PFD_LOW(cnode)		((GET_NODE_PFL(cnode))->pfd_low)
#define	PFD_HIGH(cnode)		((GET_NODE_PFL(cnode))->pfd_high)

/*
 * Define the per node variables.
 * For non-NUMA systems this defaults to referencing the global
 * Nodepda.
 */


#define	NODE_FREEMEM(cnode)		\
		(NODEPDA(cnode)->node_pg_data.node_freemem)

#define NODE_TOTALMEM(cnode) \
                (NODEPDA(cnode)->node_pg_data.node_total_mem)        

#define	NODE_FREEMEM_REL(cnode)	\
                ( NODE_TOTALMEM(cnode) ? \
                  ((NODE_FREEMEM(cnode) * 100) / NODE_TOTALMEM(cnode)) : 0 )

#define	NODE_FUTURE_FREEMEM(cnode)		\
		(NODEPDA(cnode)->node_pg_data.node_future_freemem)

#define NODE_FUTURE_FREEMEM_REL(cnode) \
                ( NODE_TOTALMEM(cnode) ? \
              ((NODE_FUTURE_FREEMEM(cnode) * 100) / NODE_TOTALMEM(cnode)) : 0 )

#define NODE_MIN_FUTURE_FREEMEM(cnode) \
                (( NODE_FREEMEM(cnode) < NODE_FUTURE_FREEMEM(cnode) ) ? \
                 NODE_FREEMEM(cnode) : NODE_FUTURE_FREEMEM(cnode))

#define NODE_MIN_FUTURE_FREEMEM_REL(cnode) \
                (( NODE_FREEMEM(cnode) < NODE_FUTURE_FREEMEM(cnode) ) ? \
                 NODE_FREEMEM_REL(cnode) : NODE_FUTURE_FREEMEM_REL(cnode)) 

#define NODE_EMPTYMEM(cnode)  \
                 (NODEPDA(cnode)->node_pg_data.node_emptymem)
#define NODE_TOTALMEM(cnode)  \
                 (NODEPDA(cnode)->node_pg_data.node_total_mem)

/*
 * Operations done on node_future_freemem only (memsched)
 */

#define	ADD_NODE_FUTURE_FREEMEM_ATOMIC(cnode, val)	\
	(atomicAddInt(&(NODE_FUTURE_FREEMEM(cnode)), (val)))
#define	SUB_NODE_FUTURE_FREEMEM_ATOMIC(cnode, val)	\
	(atomicAddInt(&(NODE_FUTURE_FREEMEM(cnode)), -(val)))
#define	SET_NODE_FUTURE_FREEMEM_ATOMIC(cnode, val)	\
	(atomicSetInt(&(NODE_FUTURE_FREEMEM(cnode)), (val)))

/*
 * Operations on node_freemem & node_future_freemem
 */

#define	ADD_NODE_FREEMEM(cnode, val) \
	(NODE_FREEMEM(cnode) += (val))


#define	SUB_NODE_FREEMEM(cnode, val) \
	(NODE_FREEMEM(cnode) -= (val))

#if (!defined(EVEREST) || !defined(SN0))
#define	INC_GLOBAL_FREEMEM(val)		(GLOBAL_FREEMEM_VAR += (val))
#define	DEC_GLOBAL_FREEMEM(val)		(GLOBAL_FREEMEM_VAR -= (val))
#endif


/*
 * Operations on node_emptymem
 */

#define ADD_NODE_EMPTYMEM(cnode, val) \
        (atomicAddInt(&(NODE_EMPTYMEM(cnode)), (val)))

#define SUB_NODE_EMPTYMEM(cnode, val) \
        (atomicAddInt(&(NODE_EMPTYMEM(cnode)), -(val)))           

extern void     pagedequeue(pfd_t *, phead_t *);
extern	int	_pagefree_size(pfd_t *, size_t, uint);
extern pfd_t * _pagealloc_size_node(cnodeid_t, uint, int, size_t);

#ifdef DEBUG
extern int	check_freemem_node(cnodeid_t);
extern int	freelist_sanity(cnodeid_t);
extern int	freelist_sanity_nolock(cnodeid_t,int);
#endif

#ifdef	NUMA_BASE
pgno_t	contmemall_node(cnodeid_t, int, int, int);
#else
#define contmemall_node(node, npgs, align, flags) contmemall(npgs, align, flags)
#endif

/*
 * Declarations and
 * Macros to manage vm system relaxed global variables
 * We keep a relaxed freemem variable, updated only
 * once a second, or whenever an accurate value is needed (for now,
 * when vhand is deciding whether to continue or not).
 */

extern pfn_t relaxed_global_freemem;
extern pfn_t global_freemem_calculate(int);
extern pfn_t global_freemem_init(void);

#define GLOBAL_FREEMEM_UPDATE()    {                                       \
                                          relaxed_global_freemem =         \
                                          global_freemem_calculate(1);     \
                                   }

#define GLOBAL_FREEMEM_INIT()      {                                       \
                                          relaxed_global_freemem =         \
                                          global_freemem_init();           \
                                   }

#define GLOBAL_FREEMEM_GET()	global_freemem_calculate(1) 	/* update */

#define GLOBAL_FREEMEM_SNAP()	global_freemem_calculate(0)	/* no update */

#define GLOBAL_FREEMEM()            (relaxed_global_freemem)

#define GLOBAL_FREEMEM_VAR          (relaxed_global_freemem)


#ifdef NUMA_BASE
extern void memfit_init(void);
extern void memfit_master_update(pfn_t cm_freemem);
#define MEMFIT_INIT()               memfit_init()
#define MEMFIT_MASTER_UPDATE(mf)    memfit_master_update((mf))
#else
#define MEMFIT_INIT() 
#define MEMFIT_MASTER_UPDATE(mf)
#endif

/*
 * Per node low freemem threshold to update relaxed global
 * freemem to cause the next clock tick to accurately check
 * whether vhand needs to be waken up or not.
 */
#ifdef NUMA_BASE

extern int numa_paging_node_freemem_low_threshold;

#define NODE_FREEMEM_LOW_THRESHOLD() numa_paging_node_freemem_low_threshold

#else /* !NUMA_BASE */

#define  NODE_FREEMEM_LOW_THRESHOLD() tune.t_gpgslo

#endif /* !NUMA_BASE*/


/*
 * Relaxed emptymem
 */

extern int relaxed_global_emptymem;
extern pfn_t global_emptymem_calculate(void);

#define GLOBAL_EMPTYMEM_GET()        global_emptymem_calculate() 

extern lock_t   sxbrk_lock;     /* sxbrk/sched synch. lock */


#define sxbrk_lock()          mutex_spinlock(&sxbrk_lock)
#define sxbrk_lockspl(SPL)    mutex_spinlock_spl(&sxbrk_lock, SPL)
#define sxbrk_unlock(T)       mutex_spinunlock(&sxbrk_lock, T)
  
#ifdef	NUMA_BASE
extern void page_poison(pfd_t*);
extern void page_unpoison(pfd_t*);
extern void phead_unpoison(cnodeid_t, phead_t*, pfd_t *);
#endif


extern int	page_discard(paddr_t, int, int);
extern int	page_isdiscarded(paddr_t);
extern int	page_ispoison(paddr_t);
extern int	page_error_clean(pfd_t *);
extern void	page_discard_enqueue(pfd_t *);
extern int	page_validate_pfdat(pfd_t *);


#endif /* _KERNEL */
#endif /* !__SYS_PAGE_H__ */
