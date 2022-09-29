/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_PMAP_H__
#define __SYS_PMAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.40 $"
#include <sys/immu.h>
#include "as_private.h"

/*
 *	Page maps.
 *	The page map manages all valid address references in an
 *	address space, whether to pageable memory or to mapped
 *	device space.
 *	
 *	The pmap structure is a collection of mostly opaque counters
 *	and pointers, whose definition and use is supplied by the
 *	particular type of pmap.
 */
typedef struct pmap {
	void   		*pmap_ptr;	/* generic pointer */
	short		pmap_scount;	/* generic short counter */
	uchar_t		pmap_type;	/* pmap type	*/
	uchar_t		pmap_flags;	/* flags	*/
	time_t		pmap_trimtime;	/* pmap was trimmed of pages @ 
					   If < 0, trimming is active.
					   Used as interlock to prevent
					   page stealing while trimming */
#if DEBUG
	pgcnt_t		pmap_resv;	/* # pages reserved */
#endif
} pmap_t;

/*	pmap_types
*/
#define PMAP_INACTIVE	0
#define PMAP_SEGMENT	1
#define PMAP_TRILEVEL	2
#define PMAP_FORWARD	3
#define PMAP_REVERSE	4

/* pmap flags */
#define PMAPFLAG_NOTRIM		1
#define	PMAPFLAG_LOCAL		4
#define	PMAPFLAG_SPT		8

/* pmap_pte_scan operation flags */
#define AS_SCAN		0x1
#define JOB_SCAN	0x2

/* SPT macros */
#define pmap_spt(p)		((p) && ((p)->pmap_flags & PMAPFLAG_SPT))
#define	pmap_spt_set(p)		((p)->pmap_flags |= PMAPFLAG_SPT)
#define pmap_spt_unset(p)	((p)->pmap_flags &= ~PMAPFLAG_SPT)

/*
 * Flags to the pmap downgrade routines.
 */

#define	PMAP_TLB_FLUSH		1 	/* flush the tlbs for the corresponding
					 * pmap.
					 */

/*
 * Return values for pmap_check_lpage_buddy.
 */

#define	SMALL_PAGES_PRESENT	0	/* Small pages present at buddy address */
#define	LARGE_PAGE_PRESENT	1	/* Large page present at buddy address */
#define	NO_PAGES_PRESENT	2	/* No pages present at buddy address */

/*
 * Macro to test if the address falls into the odd or even tlb entry
 * for a given page size.
 */

#define	IS_ODD(addr, page_size)		((__psint_t)(addr) & (page_size))


/*
 * Returns the buddy address for a given page size.
 */

#define	GET_LPG_BUDDY_ADDR(addr, page_size) \
		(IS_ODD(addr, page_size) ? (addr) - (page_size) : \
					(addr) + (page_size))

#define	LPAGE_ALIGN_ADDR(addr, page_size) \
		((caddr_t)((ulong)(addr) & ~((page_size) - 1)))

#define	IS_LARGE_PAGE_PTE(pd)	(pg_get_page_mask_index(pd))

#define	PG_INVALIDATE_LPAGE_PTE(pd)	\
		pmap_clr_lpage_pte_flags((pd), (PG_VR|PG_NDREF|PG_SV))

#define	LARGE_PAGE_PTE_ALIGN(pte, num_base_pages) \
	(pde_t *)((__psint_t)(pte) & (~(((num_base_pages * sizeof(*(pte))) - 1))))



/* PTPOOL: page table pooling */

typedef struct ptpool_s {
	lock_t  lock;
	void    *head;
	uint_t  cnt;
	uint_t  maxcnt;
} ptpool_t;

typedef	struct ptpool_stat_s {
	ulong_t hit;
	ulong_t miss;
	ulong_t	mem;
} ptpool_stat_t;

/*
 * Shared Page Tables Descriptor.
 */

typedef void 	*pmap_sptid_t;

typedef struct pmap_sptdesc_s {
	struct pmap_sptdesc_s	*spt_next;
	struct pmap_sptdesc_s	*spt_prev;
	pmap_sptid_t		spt_id;
	uint_t			spt_type;
	uvaddr_t		spt_addr;
	size_t			spt_size;
	pmap_t			*spt_pmap;
} pmap_sptdesc_t;

/*	Special pmap marker used within a page table entry
 *	to indicate that a shared page may exist for that
 *	virtual page and that the shared pmap should be
 *	used instead.  Used by utlbmiss (and vfault?).
 */

#define SHRD_SENTINEL	(PDEFER_SENTRY << PTE_PFNSHFT)

/*
 *	pmap locking:
 *
 *	Callers to pmap routines that return pointers to pmap entries
 *	must have 1) p_aspacelock held for, at least, access.
 *	Pmap entries can only be removed by a pmap_destroy call
 *	(which is only called by an exitting process), or by pmap_trim,
 *	which can only be called on behalf of the process itself;
 *	in both cases, the aspacelock must be helf in update mode.
 *
 *	Note, then, that a process accessing pmap entries within its
 *	own address (pmap) space are guaranteed that (pointers to)
 *	entries will persist as long as the access lock is held.
 *	(Note, too, that region locks still control the _content_ of
 *	the pmap entries, the address space lock just ensures their
 *	existence.)  
 *
 *	A process that is accessing pmap entries mapped to a different
 *	address space must ensure that the other process doesn't exit
 *	(and, thus, call pmap_destroy).
 */

/*
 *	External pmap interface and function prototypes:
 */
struct utas_s;
struct pas_s;
struct ppas_s;
struct pregion;

/*
 *	System pmap initialization routine.
 */
extern void	init_pmap(void);

/*
 *	Initialize pmap -- called on creation of a new address space.
 */
extern pmap_t	*pmap_create(int, int);

/*
 *	Destroy pmap -- called on last reference to pmap.
 */
extern void	pmap_destroy(pmap_t *);

/*
 *	pmap_pte(pas_t *, pmap_t *pmap, char *vaddr, int vmflag)
 *
 *	Return a pointer to pmap entry representing vaddr.
 */
extern pde_t	*pmap_pte(pas_t *, pmap_t *, char *, int);

/*
 *	pmap_ptes(pas_t *, pmap_t *pmap, char *vaddr, pgno_t *size, int vmflag)
 *
 *	Return a pointer to pmap entries representing vaddr.
 *	Number of entries of interest passed in size;
 *	number of contiguous entries returned in size.
 */
extern pde_t	*pmap_ptes(pas_t *, pmap_t *, char *, pgno_t *, int);

/*
 *	pmap_probe(pmap_t *pmap, char **vaddr, pgno_t *sizein, pgno_t *sizeout)
 *
 *	Return a pointer to the first extant pmap entry that
 *	is mapped in the address range starting at vaddr.
 *	The number of pages of interest is passed in sizein;
 *	vaddr and sizein are updated to reflect first mapping;
 *	number of contiguous entries returned in sizeout.
 */
extern pde_t    *pmap_probe(pmap_t *, char **, pgno_t *, pgno_t *);

/*
 *	pmap_reserve
 *
 *	Reserve mapping rights for size pages starting at vaddr.
 *	Returns 0 on success, errno otherwise.
 */
extern int	pmap_reserve(pmap_t *, struct pas_s *, struct ppas_s *,
						uvaddr_t, pgcnt_t);

/*
 *	pmap_unreserve
 *
 *	Uneserve mapping rights for size pages starting at vaddr.
 */
extern void	pmap_unreserve(pmap_t *, struct pas_s *, struct ppas_s *,
						uvaddr_t, pgcnt_t);

/*
 *	pmap_free(pas_t *pas, pmap_t *pmap, char *vaddr, pgno_t size, int free)
 *
 *	Free all memory mapped by pmap entries mapping size pages,
 *	starting at vaddr.  Returns number of pages freed.
 */
#define PMAPFREE_TOSS	0
#define PMAPFREE_FREE	1
#define PMAPFREE_SANON	2
#define PMAPFREE_UNHASH	4

extern int	pmap_free(pas_t *, pmap_t *, char *, pgno_t, int);

/*
 *	pmap_trim(pmap_t *pmap);
 *
 *	Trim pmap module of all unused data structures.
 *	Called by vhand when memory is tight.  Returns true
 *	if the pmap was actually trimmed.
 */
extern int	pmap_trim(pmap_t *);

/*
 *	pmap_split(struct pas_s *, struct ppas_s *)
 *
 *	Specialized routine for multi-threaded procs --
 *	manages local and shared address space overlaps.
 */
extern int	pmap_split(struct pas_s *, struct ppas_s *);

/*
 *	pmap_lclsetup(struct pas_s *, struct utas_s *, pmap_t *pmap, 
 *					char *vaddr, pgno_t size)
 *
 *	Perform any initialization needed when attaching or growing a
 *	local mapping in a multi-threaded process
 */
extern void	pmap_lclsetup(struct pas_s *, struct utas_s *, 
					pmap_t *, char *, pgno_t);

/*
 *	pmap_lclteardwn(pmap_t *pmap, char *vaddr, pgno_t size)
 *
 *	Perform any tear down needed when detaching or shrinking a
 * 	local mapping in a shared address process.
 */
extern void	pmap_lclteardwn(pmap_t *, char *, pgno_t);

/*
 * Break down the ptes of the even and odd large pages for a given addr.
 */
extern	void	pmap_downgrade_range_to_base_page_size(pas_t *, 
			caddr_t, struct pregion *, size_t);
extern	void	pmap_downgrade_to_base_page_size(struct pas_s *, 
			caddr_t, struct pregion *, size_t, int);
/*
 * Downgrades a virtual address range to a given page size.
 */
extern	void	pmap_downgrade_page_size(struct pas_s *, 
			caddr_t, struct pregion *, size_t, size_t);

/*
 * Downgrades a pte to a given page size.
 */
extern	void	pmap_downgrade_pte_to_lower_pgsz(struct pas_s *, 
			caddr_t, pde_t *, size_t);

/*
 * Check if the buddy of a large page is also a large page.
 * Returns 1 if that is the case else 0.
 */
extern	int	pmap_check_lpage_buddy(pas_t *, caddr_t, struct pregion *, 
						size_t);

/*
 * This routine checks the boundaries of the address range passed in 
 * and downgrade them to base page size.
 */
extern	void	pmap_downgrade_addr_boundary(struct pas_s *, struct pregion *,
				caddr_t, pgcnt_t, int);

/*
 * Downgrade all the even and odd large page ptes to base page size
 */
extern	void	pmap_downgrade_lpage_pte(struct pas_s *, caddr_t, pde_t *);
extern	void	pmap_downgrade_lpage_pte_noflush(pde_t *);

/*
 * Clears a set of flags on all the ptes of a large page.
 */
extern	void	pmap_clr_lpage_pte_flags(pde_t *, int);

/*
 * Downgrades a virtual address range of a region to base page size.
 */ 
extern void	pmap_downgrade_range(struct pas_s *, struct ppas_s *,
			struct pregion *, caddr_t, size_t);

/* Shared Page Tables */

/*
 *
 *	pmap_spt_get(struct pas_s *pas, pmap_sptid_t, pmap_t *, uvaddr_t, size_t)
 *
 *	Allocation and initialization of pmap_sptdesc and
 *	associated pmap. Returns 0 on success errno on failure.
 */
extern int		pmap_spt_get(struct pas_s *pas, pmap_sptid_t, pmap_t *, 
				uvaddr_t, size_t);

/*
 *	pmap_spt_remove(pmap_sptid_t)
 *
 *	Removal of pmap_sptdesc and associated pmap.
 */
extern void		pmap_spt_remove(pmap_sptid_t);

/*
 * 	pmap_spt_attach(pmap_sptid_t, pas_T *, pmap_t *, uvaddr_t)
 *
 *	Attach to shared PT. Returns 0 on success and errno on failure.
 */
extern int		pmap_spt_attach(pmap_sptid_t, pas_t *, pmap_t *, 
								uvaddr_t);

/*
 *	pmap_spt_check(pmap_t *, uvaddr_t, size_t)
 *
 *	Check range [addr, addr + size] for shared PT. Returns B_TRUE
 *	if at least on PT was found.
 */
extern boolean_t	pmap_spt_check(pmap_t *, uvaddr_t, size_t);

/*
 *	pmap_modify(struct pas_s *, struct ppas_s *, 
 *				struct pregion *, uvaddr_t, size_t)
 *
 *	Replace shared PT in address range [addr, addr + size].
 */	
extern int		pmap_modify(struct pas_s *, struct ppas_s *, 
				struct pregion *, uvaddr_t, size_t);

/*
 *	pmap_pt_is_shared(pde_t *pt)
 *
 *	Check range [addr, addr + size] for shared PT. Returns B_TRUE
 *	if at least on PT was found.
 */
extern boolean_t	pmap_pt_is_shared(pde_t *pt);

/*
 *	pmap_pte_scan(pde_t *, func, void *, void *)
 *
 *	Go thru all the address spaces using the pte and apply
 *	the input function on all those address spaces, stopping
 *	the scan as soon as the function returns 1.
 */
extern int pmap_pte_scan(pde_t *, int ((*)(void *, void *, void *)),
					void *, void *, int);

#ifdef DEBUG
extern	int	check_lpage_pte_begin(pde_t *);
extern	int	check_lpage_pte_validity(pde_t *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_PMAP_H__ */
