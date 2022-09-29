/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_PFDAT_H__
#define __SYS_PFDAT_H__

#ident	"$Revision: 3.173 $"

#include "sys/sema.h"
#include <sys/immu.h>
#include <ksys/vnode_pcache.h>

struct pfdat;
/*
 *	pf_next, pf_prev	Free list pointers or private pointers
 *				otherwise (buffer cache or getpages).
 *				These _must_ be first in structure.
 *
 *	pf_hchain		Hash chain list.
 *
 *	pf_forw, pf_prev	p_un.vp object list.
 *
 *
 *	pf_pdep1		Primary pde pointer.
 *	pf_rmapun		Union of pde pointer and rmap pointer.
 *				If acts as pde pointer if just two users of
 *				this page. Doubles as rmap pointer when there
 *				are more then two.
 *
 */

typedef unsigned short pf_use_t;

typedef struct pfdat {
	struct pfdat	*pf_next;	/* Next free pfdat.	*/
	union {
	    struct pfdat     *prev;	/* Previous free pfdat.	   */
	    sm_swaphandle_t   swphdl;	/* Swap hdl for anon pages */
	    void	     *job;	/* miser job if private PT */
	} p_swpun;
#ifdef _VCE_AVOIDANCE
	int		pf_vcolor:8,	/* Virtual cache color. */
					/* bit 0: PE_RAWWAIT rawwait */
					/* bits 1..7: PE_COLOR  */
			pf_flags:24;	/* Regular page flags   */
#else
	uint		pf_flags;	/* Regular & NUMA page flags */
#endif
	pf_use_t	pf_use;		/* Share use count.	*/
	pf_use_t	pf_rawcnt;	/* Count of processes	*/
					/* doing raw I/O to page*/
	unsigned long	pf_pageno;	/* Object page number	*/
	union {
		struct vnode	*vp;	/* Page's incore vnode.	*/
		void 		*tag;	/* Generic hash tag.	*/
		void		*aslink;/* Owning AS if private PT */
	} p_un;
	struct pfdat	*pf_hchain;	/* Hash chain link 	*/

	union	pde	*pf_pdep1;		/* Primary pde ptr	*/
	union {
		struct rmap	*pf_revmapp;	/* Reverse map pointer 	*/
		union	pde	*pf_pdeptr;	/* Page tbl entry ptr	*/
#if CELL
		time_t		pf_utimestamp;	/* timestamp for zero use cnt */
#endif
	} p_rmapun;
#if MULTIKERNEL
	__uint64_t		pf_exported_to;	/* bitstring of cells page is currently export to */
#endif
	
} pfd_t, pfde_t;


#define pf_vp		p_un.vp
#define pf_tag		p_un.tag
#define pf_pas		p_un.aslink	/* pte->as translation */
#define pf_job		p_swpun.job	/* pte->miser job id */
#define pf_prev		p_swpun.prev
#define pf_swphdl	p_swpun.swphdl
#define pf_pchain	pf_next		/* NB: list is not NULL-terminated */
#define pf_vchain	pf_prev

#define pf_sppt		pf_next		/* SPT: shared PT back pointer */

/* Macros for accessing reverse mapping fields in pfdat */
#define	pf_rmapp	p_rmapun.pf_revmapp
#define	pf_pdep2	p_rmapun.pf_pdeptr

#if CELL
/* This shares space with the rmap entries. Used only if pf_use == 0 */
#define pf_timestamp	p_rmapun.pf_utimestamp
#endif

/*
 * pf_flags flags
 *
 * Note: platforms using VCE_AVOIDANCE have fewer flag bits to make room
 *       for the vcolor field.  Therefore, flags common to all platforms
 *	 must be in the low-order 24 bits.  The upper 8 bits can be used
 *	 for MP and NUMA specific flags only.
 */

#define	P_QUEUE		0x000001	/* Page on free queue		*/
#define	P_BAD		0x000002	/* Bad page (parity error, etc.)*/
#define	P_HASH		0x000004	/* Page on hash queue		*/
#define P_DONE		0x000008	/* I/O to read page is done.	*/
#define	P_SWAP		0x000010	/* Page on swap (not file).	*/
#define	P_WAIT		0x000020	/* Waiting for P_DONE		*/
#define	P_DIRTY		0x000040	/* Page stored to programatically */
#define	P_DQUEUE	0x000080	/* Page on vnode dirty queue	 */
#define P_ANON		0x000100	/* Anonymous page		*/
#define P_SQUEUE	0x000200	/* Page on shared anon free queue */
#define P_CW		0x000400	/* Force copy on write		*/
#define P_DUMP		0x000800	/* Dump this page on panics.	*/
#define	P_HOLE		0x001000	/* Page has no backing store	*/
#define	P_CACHESTALE	0x002000	/* Page may be stale in primary caches */

/*
 * The page size for the large page is kept in the three bits below.
 * We cannot use a bit field because we a have bit lock in this integer and
 * the compiler does not take an address of a bit field.
 */
#define P_LPG_INDX1      	0x004000  /* Large page index bits */
#define P_LPG_INDX2	 	0x008000  /* Large page index bits */
#define P_LPG_INDX3	 	0x010000  /* Large page index bits */
#define P_HAS_POISONED_PAGES	0x020000  /* Large pg contains poisonous pgs */

#define P_LPG_INDEX 	(P_LPG_INDX1|P_LPG_INDX2|P_LPG_INDX3)

#define P_SANON		0x040000	/* Page marked as sanon page */
#define P_RECYCLE	0x080000        /* Page being recycled to new use */

#ifdef IP32
#define P_ECCSTALE	0x100000	/* stale ECC, ref via no-ECC alias */
#endif	/* IP32 */

#define	P_REPLICA	0x200000        /* pfdat is a replicated page */
#define P_ISMIGRATING	0x800000        /* page is currently migrating */

/*
 * MP & NUMA flags only beyond bit 23 (for non-VCE_AVOIDANCE platforms)
 *
 * NOTE: P_USERDMA is defined with a different bit flag depending on
 * whether _VCE_AVOIDANCE is defined.  This is because we were out of bits
 * in the pfdat flag and had to maintain compatability.  We only had 
 * 0x10000000 to work with when _VCE_AVOIDANCE was not defined, and 0x400000
 * to work with when _VCE_AVOIDANCE was defined.  This can be simplified if
 * or when we are allowed to change the pfdat structure.
 */

#if !defined (_VCE_AVOIDANCE)

#define P_MULTI_COLOR	0x000000	/* non-flag w/o _VCE_AVOIDANCE */
#define P_POISONED      0x400000        /* page has been poisoned */
#define	P_ERROR		0x1000000
#define P_HWBAD		0x2000000
#define	P_XP		0x4000000	/* Page permitted across partitions */
#define P_BITLOCK	0x8000000	/* spin lock bit to protect pfdat */
#define P_USERDMA	0x10000000	/* page is involved in userland DMA */
#define P_BULKDATA	0x20000000	/* buffer cache, mbufs, etc */
#define	P_HWSBE		0x40000000	/* page has SBE */

/*
 * Mask for all flags except P_BITLOCK
 */

#define P_ALLFLAGS      0x37FFFFFF

#else /* !defined _VCE_AVOIDANCE */

#define P_MULTI_COLOR	0x800000	/* file page has non-color-matched */
					/* current virtual color, or -1 */
#define P_POISONED	0x000000	/* non-flag w/ _VCE_AVOIDANCE */
#define	P_ERROR		0x000000
#define P_HWBAD		0x000000
#define	P_XP		0x000000
#define P_BITLOCK	0x000000
#define P_USERDMA	0x400000	/* page is involved in userland DMA */
#define P_BULKDATA	0x000000
#define	P_HWSBE		0x000000

#define P_ALLFLAGS	0xFFFFFF

#endif /* !defined _VCE_AVOIDANCE */

#define pfdat_hold(P)        pageuseinc((P))
#define pfdat_release(P)     pagefreeanon((P))
#define pfdat_inuse(P)       ((P)->pf_use > 0)
#define pfdat_is_held(P)     ((P)->pf_use > 1)
#define pfdat_held_musers(P) ((P)->pf_use > 2)
#define pfdat_held_oneuser(P) ((P)->pf_use == 2)
/*
 * Since pf_use and rawcnt rollover at 64K references we need to catch it
 * and panic if it occurs.
 */
#define pfdat_inc_usecnt(P) 						\
	do {								\
		(P)->pf_use++;						\
		ASSERT_ALWAYS((P)->pf_use != 0);			\
	} while(0);
#define	pfdat_hold_nolock(P) pfdat_inc_usecnt((P))
#define pfdat_inc_rawcnt(P) 						\
	do {								\
		(P)->pf_rawcnt++;					\
		ASSERT_ALWAYS((P)->pf_rawcnt != 0);			\
	} while(0);
#define pfdat_dec_usecnt(P) 						\
	(								\
		ASSERT_ALWAYS((P)->pf_use != 0),			\
		--(P)->pf_use						\
	)
#define pfdat_dec_rawcnt(P) 						\
	(								\
		ASSERT_ALWAYS((P)->pf_rawcnt != 0),			\
		--(P)->pf_rawcnt					\
	)

/*
 * Pfdat Locking Policy -
 *
 * Each pfdat contains a spin lock to control the update of the pf_usecnt,
 * pf_rawcnt, pf_flags, pf_pageno, and pf_tag fields.  The lock is implemented
 * as a bit lock within the flags field.  The lock is acquired and released
 * using the macros below.
 *
 * The other fields of the pfdat are under control of the object that the
 * pfdat currently belongs to.  For example, when the pfdat is associated
 * with anonymous memory, the p_hchain field is protected by the anonymous
 * tree lock.  When the pfdat is on the free list, the p_next and p_prev
 * fields are protected by the free list lock.  And so on.
 */

#define pfdat_lock(pfd)       mp_mutex_bitlock(&(pfd)->pf_flags, P_BITLOCK)
#define pfdat_unlock(pfd, s)  mp_mutex_bitunlock(&(pfd)->pf_flags, P_BITLOCK, s)
#define pfdat_islocked(pfd)   bitlock_islocked(&(pfd)->pf_flags, P_BITLOCK)

#define nested_pfdat_lock(pfd)	 nested_bitlock(&(pfd)->pf_flags, P_BITLOCK)
#define nested_pfdat_trylock(pfd) nested_bittrylock(&(pfd)->pf_flags, P_BITLOCK)
#define nested_pfdat_unlock(pfd) nested_bitunlock(&(pfd)->pf_flags, P_BITLOCK)

/*
 * Convenience macros for bumping the reference count and setting/clearing
 * the flags.  To use these, the caller must already have a reference to
 * the page and the last reference to the page must not go away during the
 * exeuction of these macros since they assume the state of the page is
 * stable.
 *
 * pfd_useinc(pfd)		increments reference count of given pfd
 * pfd_usedec(pfd)		use pagefree(pfd)
 * pfd_setflags(pfd, flags)	set the given flags in the pf_flags field of
 *				the given pfd
 * pfd_clrflags(pfd, flags)	clear the given flags in the pf_flags field of
 *				the given pfd
 */

#define pfd_useinc(pfd)							\
		do {							\
			int s;						\
									\
			s = pfdat_lock(pfd);				\
			ASSERT((pfd)->pf_use >= 1);			\
			ASSERT(((pfd)->pf_flags & 			\
				(P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);	\
			ASSERT((pfd)->pf_flags & (P_HASH|P_BAD));	\
									\
			pfdat_inc_usecnt(pfd);				\
			pfdat_unlock(pfd, s);				\
		} while (0)

#if MP

#define pfd_setflags(pfd, flags)					\
		do {							\
			int s;						\
									\
			ASSERT((pfd)->pf_use >= 1);			\
			ASSERT(((pfd)->pf_flags &			\
			       (P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);	\
									\
			s = splhi();					\
			bitlock_set(&(pfd)->pf_flags, P_BITLOCK, flags);\
			splx(s);					\
		} while (0)

#define pfd_clrflags(pfd, flags)					\
		do {							\
			int s;						\
									\
			ASSERT((pfd)->pf_use >= 1);			\
			ASSERT(((pfd)->pf_flags &			\
			       (P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);	\
									\
			s = splhi();					\
			bitlock_clr(&(pfd)->pf_flags, P_BITLOCK, flags);\
			splx(s);					\
		} while (0)

#else  /* !MP */

/*
 * Save the bit lock overhead for UP systems.  We still go splhi in
 * case an interrupt handler might update the pfdat.
 */

#define pfd_setflags(pfd, flags)					\
		do {							\
			int s;						\
									\
			ASSERT((pfd)->pf_use >= 1);			\
			ASSERT(((pfd)->pf_flags &			\
			       (P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);	\
									\
			s = splhi();					\
			(pfd)->pf_flags |= (flags);			\
			splx(s);					\
		} while (0)

#define pfd_clrflags(pfd, flags)					\
		do {							\
			int s;						\
									\
			ASSERT((pfd)->pf_use >= 1);			\
			ASSERT(((pfd)->pf_flags &			\
			       (P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);	\
									\
			s = splhi();					\
			(pfd)->pf_flags &= ~(flags);			\
			splx(s);					\
		} while (0)

#endif /* !MP */

#define pfd_nolock_setflags(pfd, flags)                         \
				(pfd)->pf_flags |= (flags)

#define pfd_nolock_clrflags(pfd, flags)                         \
				(pfd)->pf_flags &= ~(flags)

#define pfd_getmo(pfd)      ((pfd)->pf_tag)

#define pfd_isanon(pfd)     ((pfd)->pf_flags & P_ANON)

#define pfd_isincache(pfd)  ((pfd)->pf_flags & P_HASH)

#define	pageuseinc(pfd)		pfd_useinc(pfd)
#define	pageflags(pfd, flag, set)	if (set) pfd_setflags(pfd, flag); \
						else pfd_clrflags(pfd, flag);


/*
 * Indicate if P is pinned down. rawcnt is the only relavent field now.
 * This could change if accounting of pinning page for mpin() and 
 * pinning page for I/O changes.
 */
#define	pfdat_pinned(P)		((P)->pf_rawcnt)

/*
 * Checks for pfdat error and hwbad flags.
 */

#define pfdat_iserror(pfd)    \
        (((volatile pfd_t*)(pfd))->pf_flags & P_ERROR)

#define pfdat_ishwbad(pfd)    \
        (((volatile pfd_t*)(pfd))->pf_flags & P_HWBAD)

#define pfdat_ishwsbe(pfd)    \
        (((volatile pfd_t*)(pfd))->pf_flags & P_HWSBE)

#define pfdat_isxp(pfd)	      \
        (((volatile pfd_t*)(pfd))->pf_flags & P_XP)


/*	The following definitions are involved with the conversions
**	of pfdats to pfns and vice versa.
*/


/*
**	This first block of macros allows one to identify which node
** 	a given page is located in and to construct pfns given a node
**	number and an offset within a node.  These macros work on all
**	platforms so that the main line code does not need to be ifdef'ed.
**	Non-NUMA machines are treated as a degenerate NUMA machine with
**	only one node.
**
**	The macros that may be used are:
**
**	pfn_nasid(pfn)		return the NASID where the physical page 
**				is located
**
**	pfn_nasidoffset(pfn)	return the offset of the pfn relative to
**				the starting address of the node's memory
**
**	mkpfn(nasid, off)	construct a pfn given a NASID and an offset
**				(the reverse of the above two macros)
**
**	pfdattonasid(pfd)	return the NASID for the pfdat's pfn
**/

#if NUMA_BASE

#if SN

/*	Extract node number and page frame within node's memory from
**	a physical pfn or pfdat address itself (which can be done since
**	the pfdats for any node's memory are in that node's memory).
*/

#include <sys/SN/addrs.h>

#define PFN_NASIDSHFT		(NASID_SHFT - PNUMSHFT) 
#define	PFN_NASIDMASK		(NASID_BITMASK << PFN_NASIDSHFT) 
#define	PFD_NASIDMASK		(NASID_BITMASK << NASID_SHFT) 
 
#define pfn_nasid(pfn)		(nasid_t)((pfn) >> PFN_NASIDSHFT)
#define PFN_TO_CNODEID(pfn) 	NASID_TO_COMPACT_NODEID(pfn_nasid(pfn))
#define pfn_nasidoffset(pfn)	((pfn) & ~PFN_NASIDMASK)
#define mkpfn(nasid, off)	(((pfn_t)(nasid) << PFN_NASIDSHFT) | (off))
#define local_pfdattonasid(pfd)	(nasid_t)(((__psunsigned_t)(pfd) & PFD_NASIDMASK) >> NASID_SHFT)

/*
 * There is one pfdat array for each node.
 * It is initially sized so that it could cover
 * the maximum amount of memory we can put on a node.
 * However, there are virtually always gaps in the memory.
 * Thus, there are ranges of pfdats that are not used.
 * This memory is put on the freelist.
 *
 * The pfdat array starts at a fixed offset (defined here).
 * This fixed offset need to be high enough to allow
 * all of lowmem to fit below it, but low enough so that
 * the entire table can still fit into the smallest memory
 * size that can occupy the first slot of memory.
 *
 * A 24MB pfdat offset allows the first slot to have only 32MB.
 * This could occur on a minimum o200.  In this case,
 * you won't be able to have memory in slots beyond the first 4.
 * Since the o200 has only 4 slots, this should not be a problem.
 * If the first slot has 64MB or more in it (expected to be the most
 * common case by far), there are no restrictions.
 */
#if defined(SN1) || defined(SN0XXL)
#define NODE_PFDAT_OFFSET	(32*1024*1024LL)	/* hardwired! */
#else
#define NODE_PFDAT_OFFSET	(24*1024*1024LL)	/* hardwired! */
#endif

#endif /* !SN */

#else /* !NUMA_BASE */

#define pfn_nasid(pfn)		0
#define PFN_TO_CNODEID(pfn)     0
#define pfn_nasidoffset(pfn)	(pfn)
#define mkpfn(nasid, off)	(off)
#define	local_pfdattonasid(pfd)	0

#endif /* !NUMA_BASE */

/*
**	This next block of macros controls the translation of pfns to pfdats
**	and vice versa.
**
**	Machines with discontiguous memory place their pfdat array
**	at a fixed address from the beginning of each node.
**	Gaps in memory result in pfdats that are unused--this memory
**	is initialized and used.
**
**	Machines with physically contiguous memory use the traditional
**	pfdat array that's allocated at boot time.
**
**	Many of the macro definitions here are internal definitions used
**	by this implementation.  These should not be used external to this
**	file.
**
**	The macros that may be used externally are:
**
**	pfntopfdat(pfn)		returns the pfdat for the given pfn
**
**	pfdattopfn(pfd)		returns the pfn for the given pfdat
**
**	pfdattophys(pfd)	return the physical address for the given pfdat
**
**
**	The following macros may be used sparingly external to the
**	implementation here.  These are intended for Cellular IRIX kernels
**	where one knows for certain that they are never dealing with
**	proxy pfdats or imported pages.  No checking is done and therefore
**	their use is strongly discouraged except in special cases such as
**	local error handling and special performance optimizations.
**
**	local_pfntopfdat(pfn)
**	local_pfdattopfn(pfd)
**	local_pfdattonasid(pfd)
*/

#if DISCONTIG_PHYSMEM

#if SN

#include <sys/SN/arch.h>

#define SLOT_PFNSHIFT   	(SLOT_SHIFT - PNUMSHFT) 

#define slot_getbasepfn(node,slot) \
		(mkpfn(COMPACT_TO_NASID_NODEID(node), slot<<SLOT_PFNSHIFT))

#else	/* SN */
<<bomb -- currently only SN is PHYSMEM_DISCONTIG>>
#endif	/* SN */

#define pfn_to_node_base_pfn(pfn)	((pfn) & ~(NODE_MAX_MEM_SIZE/NBPP-1))
#define pfn_to_node_offset_pfn(pfn)	((pfn) &  (NODE_MAX_MEM_SIZE/NBPP-1))

#define pfn_to_node_pfdat_base(pfn)	( (struct pfdat *) \
		PHYS_TO_K0(ctob(pfn_to_node_base_pfn(pfn))+NODE_PFDAT_OFFSET) )

#define local_pfntopfdat(pfn)		(pfn_to_node_pfdat_base(pfn) + \
					 pfn_to_node_offset_pfn(pfn))

#define pfd_to_node_base_pfn(pfd)	\
	( (K0_TO_PHYS(pfd) & ~(NODE_MAX_MEM_SIZE-1) ) >> PNUMSHFT)

#define pfd_to_node_offset_pfn(pfd)	\
	( (((unsigned long)(pfd) & (NODE_MAX_MEM_SIZE-1)) - NODE_PFDAT_OFFSET) \
	  / sizeof(struct pfdat) )


#define local_pfdattopfn(pfd)		(pfd_to_node_base_pfn(pfd) | \
					 pfd_to_node_offset_pfn(pfd))

#define pfn_to_slot_num(pfn)      (((pfn) >> SLOT_PFNSHIFT) & SLOT_BITMASK)
#define pfdat_to_slot_num(pfd)    (pfn_to_slot_num(pfdattopfn(pfd)))

#define pfn_to_slot_offset(pfn)   ((pfn) & ((SLOT_SIZE/NBPP)-1))
#define pfdat_to_slot_offset(pfd) (pfn_to_slot_offset(pfdattopfn(pfd)))

#else /* !DISCONTIG_PHYSMEM */

#define MAX_MEM_SLOTS   1

#ifdef IP32
#include <sys/cpu.h>
#include <sys/sysmacros.h>

#define pfn_to_eccpfn(pfn)	((pfn) & ~btoc(ECCBYPASS_BASE))
#define eccpfn_to_noeccpfn(pfn)	(btoc(ECCBYPASS_BASE) | (pfn))
#define pfdat_to_eccpfn(pfd)	((pfd) - pfdat)
#define pfdat_to_noeccpfn(pfd)	(eccpfn_to_noeccpfn(pfdat_to_eccpfn(pfd)))
#define local_pfdattopfn(pfd)	(((pfd)->pf_flags & P_ECCSTALE) == 0 ? \
					pfdat_to_eccpfn(pfd) : \
					pfdat_to_noeccpfn(pfd))

#ifdef MH_R10000_SPECULATION_WAR
#define pfdattotphead(pfd)      (((pfd) < pfd_lowmem) ? &tphead[TPH_LOWMEM] \
				 : &tphead[pfdtotile(pfd)->td_locked != 0])
#else /* MH_R10000_SPECULATION_WAR */
#define pfdattotphead(pfd)      (&tphead[pfdtotile(pfd)->td_locked != 0])
#endif /* MH_R10000_SPECULATION_WAR */
#ifdef TILES_TO_LPAGES
#define pfdattotpindex(pfd)	(pfdattotphead(pfd) - &tphead[0])
#endif /* TILES_TO_LPAGES */
#else	/* IP32 */
#define pfn_to_eccpfn(pfn)	(pfn)
#define local_pfdattopfn(pfd)	(pfd - pfdat)
#endif	/* IP32 */

#define	local_pfntopfdat(pfn)	(&pfdat[pfn_to_eccpfn(pfn)])
#endif /* !DISCONTIG_PHYSMEM */

#define pfdattophys(pfd) ((__psint_t)pfdattopfn(pfd) << PNUMSHFT)

/*
 * Pfdat to compact node id
 */

#define	pfdattocnode(pfd)	NASID_TO_COMPACT_NODEID(\
					pfdattonasid((pfd_t *)(pfd)))
#define local_pfdattocnode(pfd) NASID_TO_COMPACT_NODEID(\
					local_pfdattonasid((pfd_t *)(pfd)))


/*
 * cpfn to pfn
 */
extern pfn_t cpfn_to_pfn(cnodeid_t node, pfn_t cpfn);

/*
 * pfns in node after PFD_LOW
 */
extern uint pfn_getnumber(cnodeid_t node);

/*
 * To allow page import/export in Cellular IRIX, we need to interpose
 * on the pfdat <-> pfn conversion macros.  This allows us to hide the
 * presence of proxy pfdats from most of the kernel.
 */

#if CELL

extern	struct pfdat *pfntopfdat(pfn_t);
extern	pfn_t         proxy_pfdattopfn(pfd_t *);

#define pfdattopfn(pfd)		local_pfdattopfn(pfd)

#define pfdattonasid(pfd)	local_pfdattonasid(pfd)

/*
 * The following functions compromise the external interface for importing
 * and exporting pages between cells.
 */

/*
 * pfn = export_page(pfd, cellid, &already_exported)
 *
 * The page corresponding to the given local pfdat is exported to the 
 * indicated cell.  The reference count on the page is incremented (to
 * prevent it from going away while the remote cell is using it) and
 * the firewall between the cells is opened for this page.  Since the
 * reference count is incremented, export_page must not be called multiple
 * times for the same page without first doing an unexport_page on it.
 * For convenience, the pfn of the exported page is returned.
 */

extern pfn_t export_page(pfd_t *, cell_t, int *);


/* 
 * unexport_page(pfd, cellid)
 *
 * Reverse an export operation.  The caller must ensure that the remote
 * cell has stopped using the page.  The firewall is raised and the extra
 * reference taken by export_page is dropped.
 */

extern void unexport_page(pfd_t *, cell_t);


/*
 * pfd = import_page(pfn, cell, already_imported)
 *
 * This call makes the given remote page available for use on this cell.
 * The page must have already been exported to this cell by the cell where
 * the page is physcially located.  This call creates a proxy pfdat for
 * the page.  The proxy pfdat is initially zero'ed with the reference
 * count set to 1.  A pointer to this proxy is return.  Typically, the
 * memory object manager for this page will insert it in its page cache.
 * If import_page() discovers the page has already been imported, it
 * does not allocate a new proxy and returns NULL instead.
 */

extern pfd_t *import_page(pfn_t, cell_t, int);


/*
 * unimport_page(pfd)
 *
 * This call reverses the import operation.  The given page must no longer
 * be in use, must not be in a cache or on a free list, nor have any reverse
 * maps when it is unimported.  The proxy allocated by import_page is freed,
 * so no further references to pfd can be made after this call.  It is up to
 * the caller (usually the memory object manager) to inform the remote cell
 * to unexport the page.
 */

extern void unimport_page(pfd_t *);


/*
 * proxy_pagefree(pfd)
 *
 * This call is used whenever the pf_use count on an imported page goes to
 * zero. The page is put into a queue to be returned to the server that
 * the page was imported from. 
 */

extern int proxy_pagefree(pfd_t *);


/*
 * proxy_pageunfree(pfd)
 *
 * This call will remove a page from the freelist in anticipation of
 * the page being reattached.
 * The pfdat lock & vnode lock is held when making the call.
 */

extern void proxy_pageunfree(pfd_t *);


/*
 * proxy_decommission_free_page(pfd)
 *
 * This call is called when a proxy is decommissioned from the
 * vnode page cache and the page is already in the free queue.
 */

extern void proxy_decommission_free_page(pfd_t *);



#else  /* !CELL_IRIX */

#define pfntopfdat(pfn)		local_pfntopfdat(pfn)
#define pfdattopfn(pfd)		local_pfdattopfn(pfd)
#define pfdattonasid(pfd)	local_pfdattonasid(pfd)

#endif /* CELL_IRIX */


#ifdef _VCE_AVOIDANCE
#define PE_RAWWAIT	0x01	/* some waiting for pf_rawcnt	*/
				/* to change			*/
#define PE_COLOR   	0xFE	/* current virtual color, or -1	*/
				/* to indicate none assigned 	*/

#define PE_COLOR_SHIFT	1

#define PFD_WAKEUP_RAWCNT(pfd) pfd_wakeup_rawcnt(pfd)
#else
#define PFD_WAKEUP_RAWCNT(pfd) ((void) NULL)
#endif

#define PG_ISMOD(P)	((P)->pf_flags & P_DIRTY)

#ifdef MH_R10000_SPECULATION_WAR
#define P_NO_DMA        0x200000 /* page is allocated for non-DMA use */
#endif /* MH_R10000_SPECULATION_WAR */

/*
 * Non-existent memory is marked by setting the use count to -1
 * (cause we're out of flag bits).  These pages are never put on
 * a free list and are never referenced.  Some day we'll have more
 * flags, so use the macros below so we can change the implementation.
 */

#define PG_MARKHOLE(P)	((P)->pf_use = (ushort_t)-1)
#define PG_ISHOLE(P)	((P)->pf_use == (ushort_t)-1)

#if R4000 && EVEREST
/* this is to avoid VCEs */
#define KSTACKPOOL		1
#define MIN_KSTACK_PAGES 	6
#define KSTACK_TIMEOUT  	(3 * HZ)
#endif

#ifdef _KERNEL

/*
 * This is a tuneable used to control whether we do the
 * userdma tracking and error reporting of concurrent
 * user level dma operations into the same memory.
 */
extern int io4ia_userdma_war;

#ifdef EVEREST

#define	PC_NUM_WORDS		4
#define	PC_COUNTERS_PER_WORD	32
typedef struct page_counter {
	__uint64_t	pc_words[PC_NUM_WORDS];
} page_counter_t;

struct proc;
typedef struct dma_info {
	pid_t		dma_proc;
	caddr_t		dma_uaddr;
	size_t		dma_count;
	caddr_t		dma_kaddr;
	caddr_t		dma_firstkva;
	caddr_t		dma_lastkva;
	struct dma_info	*dma_uhash;
	struct dma_info	*dma_khash;
} dma_info_t;
#endif

extern lock_t	memory_lock;	/* lock for manipulationg memory*/
extern sv_t	sv_sxbrk;	/* sync waiting for XBRK */
#ifdef MH_R10000_SPECULATION_WAR
extern sv_t	sv_sxbrk_low;	/* sv_sxbrk for low memory only */
extern sv_t	sv_sxbrk_high;	/* sv_sxbrk for high memory only */
#endif /* MH_R10000_SPECULATION_WAR */

#define	pagealloc(cachekey, flags) \
		pagealloc_size((cachekey), (flags), (NBPP))

#define	pagefree(pfd)	pagefree_size((pfd), (NBPP))

#ifdef	NUMA_BASE
#define	pagealloc_node(cnode, cachekey, flags) \
		pagealloc_size_node((cnode), (cachekey), (flags), (NBPP))
#endif /* NUMA_BASE */

#ifdef NUMA_BASE
#define pagealloc_rr(cachekey, flags) \
        pagealloc_size_rr((cachekey), (flags), (NBPP))
#else /* !NUMA_BASE */
#define pagealloc_rr(cachekey, flags) \
        pagealloc_size((cachekey), (flags), (NBPP))
#endif /* !NUMA_BASE */
       
#if !DISCONTIG_PHYSMEM
extern pfd_t	*pfdat;		/* page frame database --       */
				/* allocated at bootup          */
#endif
#ifdef MH_R10000_SPECULATION_WAR
extern pfd_t	*pfd_lowmem;	/* lowest pfd_t for general DMA use */
extern pfn_t	pfn_lowmem;	/* lowest pfn_t for general DMA use */
#endif	/* MH_R10000_SPECULATION_WAR */
#ifdef EVEREST
extern page_counter_t	*page_counters;
extern pfn_t		num_page_counters;
#endif
extern sv_t	pfdwtsv[];	/* page wait sync variables	*/
extern pfd_t	*pfd_direct;	/* highest directly-mappable pfd*/
#ifdef _VCE_AVOIDANCE
extern int	vce_avoidance;	/* set of VCE avoidance enabled */

#define COLOR_VALIDATION(pfd,color,flags,vmflags) \
	(vce_avoidance ? color_validation(pfd,color,flags,vmflags) : 0)
#else
#define COLOR_VALIDATION(pfd,color,flags,vmflags)
#endif 

#ifdef TILES_TO_LPAGES
#define pfdattophead(pfd)       (&pfdattotphead(pfd)-> \
				 phead[pfdattopfn(pfd) & pheadmask])
#else /* TILES_TO_LPAGES */
#define pfdattophead(pfd)       (&phead[pfdattopfn(pfd) & pheadmask])
#endif /* TILES_TO_LPAGES */

#define	NPWAIT			64
#define	PWAITMASK		(NPWAIT-1)
#define	pfdattopwtsv(pfd)	(&pfdwtsv[pfdattopfn((pfd_t *)pfd) & PWAITMASK])
#ifdef _VCE_AVOIDANCE
/* This only works for non MP systems */
#define pfdat_sv_wait(pfd, sv, token)	sv_wait(sv, \
					PZERO|TIMER_SHIFT(AS_MEM_WAIT), \
					NULL, token)
#else
#define	pfdat_sv_wait(pfd, sv, token)	sv_bitlock_wait(sv, \
					PZERO|TIMER_SHIFT(AS_MEM_WAIT), \
					&(pfd)->pf_flags, P_BITLOCK, token)
#endif


#define PGNULL		0	/* pf_pageno null value		*/


/*
 * vcache(key, lpn)
 * vcache2(prp, attr, lpn)
 *
 * These macros/functions calculate the preferred secondary cache 
 * color for a page. In general, the vcache2 function should be used since 
 * it will correctly color large pages. vcache works correctly only
 * for regions that have base page sizes assigned.
 *
 * In addition, for 6.5.x, vcache does not work correctly for the
 * case that the calculated color equals 128. Unfortunately, this
 * color value is the same as NOCOLOR. See the code for vcache2
 * for more details.
 *
 */
#define vcache(key,lpn)	(uint)((uint)(key) + lpn)

#ifdef _VCE_AVOIDANCE
#define pfn_to_pfde(pfn) pfntopfdat((pfn))
#define pfd_to_pfde(pfd) (pfd)

#define pfde_to_vcolor(pfd) ((pfd)->pf_vcolor >> PE_COLOR_SHIFT)
#define pfde_set_vcolor(pfd,color) ((pfd)->pf_vcolor = \
				     ((color << PE_COLOR_SHIFT) | \
				      ((pfd)->pf_vcolor & PE_RAWWAIT)))
#define	pfd_to_vcolor(pfd)  (pfde_to_vcolor(pfd_to_pfde(pfd)))
#define	pfn_to_vcolor(pfn)  (pfde_to_vcolor(pfn_to_pfde(pfn)))

#define pfde_to_rawwait(pfde) ((pfde)->pf_vcolor & PE_RAWWAIT)
#define pfde_set_rawwait(pfde) ((pfde)->pf_vcolor |= PE_RAWWAIT)
#define pfde_clear_rawwait(pfde) ((pfde)->pf_vcolor &= ~PE_RAWWAIT)
#endif


/*
 * Macros created/used in the context of Replication
 */

/* Macros specific to Replication	*/
#ifdef	NUMA_REPLICATION
extern void page_replica_remove(pfd_t *);
#define	pfd_replicated(p)	(p->pf_flags & P_REPLICA)
#else
#define	pfd_replicated(p)	(0)	/* Never replicated 		*/
#define	page_replica_remove(pfd)
#endif	/* NUMA_REPLICATION */


#define pfdat_set_ismigrating(pfd)   \
        ((pfd)->pf_flags |= P_ISMIGRATING)

#define pfdat_clear_ismigrating(pfd) \
        ((pfd)->pf_flags &= ~P_ISMIGRATING)

#define pfdat_spinwhile_ismigrating(pfd)                                           \
        while (((volatile pfd_t*)(pfd))->pf_flags & P_ISMIGRATING) {               \
               printf("<Spinning on P_ISMIGRATING, cpu [%d]>\n", private.p_cpuid); \
        }

#define pfdat_ismigrating(pfd)    \
        (((volatile pfd_t*)(pfd))->pf_flags & P_ISMIGRATING)

#ifdef	NUMA_BASE
#define pfdat_ispoison(pfd)    \
        (((volatile pfd_t*)(pfd))->pf_flags & P_POISONED)
#endif


/* Function Prototypes */

struct region;
struct proc;
struct vfs;
struct vnode;
struct pregion;
struct pageattr;

/*
 * Allocate a large page of a specific size, FirstTouch placement
 */
extern pfd_t *pagealloc_size(uint, int, size_t);

#ifdef NUMA_BASE
/*
 * Allocate a large page of a specific size, RR placement
 */
extern pfd_t *pagealloc_size_rr(uint, int, size_t);
#endif /* NUMA_BASE */

/*
 * Allocate a large page of a specific size, FirstTouch placement,
 * but not at the end of any given memory chunk.  This is used
 * in places that need pages for DMA that can run off the end of pages.
 */
extern pfd_t *pagealloc_notatend(uint, int, size_t);

/*
 * Wait for a large page of a specific size.
 * lpage_size_wait( page_size, pri).
 * It internally calls sv_wait_sig and returns whatever sv_wait_sig returns.
 */

extern int lpage_size_timed_wait(size_t, int, timespec_t *);
#define	lpage_size_wait(x, y)	lpage_size_timed_wait((x), (y), 0)


#if NUMA_BASE
extern pfd_t *pagealloc_size_node(cnodeid_t, uint, int, size_t);
#else
#define pagealloc_size_node(node, key, flags, size) \
					pagealloc_size(key, flags, size)
#define	pagealloc_node(node, key, flags) \
				pagealloc(key,flags)
#endif
extern int pagefree_size(pfd_t *, size_t);

extern void pagefreeanon_size(pfd_t *, pgno_t);
#define	pagefreeanon(pfd)	pagefreeanon_size((pfd), 1)

extern void pagefreesanon(pfd_t *, int);
extern pfd_t *pgetsanon(int);
extern pfd_t *pglock(pfd_t *);
extern void pagewait(pfd_t *);
extern void pagedone(pfd_t *);
extern int pagespatch(pfd_t *, int);    
extern void prehash(pfd_t *, void *, pgno_t, unsigned);
extern void pcrehash(void *, pgno_t, void *, pgno_t);
extern void ptagremove(void *, pgno_t, pgno_t);

extern void pagebad(pfd_t *);

extern void pageflagsinc(pfd_t *, int, int);
extern void pdinsert(pfd_t *);
extern void pdflush(struct vnode *, uint64_t);
extern void psanonfree(void *, pfd_t *);
extern void pageunfree(pfd_t *);

extern void setsxbrk(void);
#if EXTKSTKSIZE == 1
extern void stackext_setsxbrk(void);
#endif
#ifdef MH_R10000_SPECULATION_WAR
extern void setsxbrk_class(int);
#endif /* MH_R10000_SPECULATION_WAR */

extern void	ghostpagedone(int);
extern void	ghostpagewait(struct region *, int);

extern caddr_t	page_mapin(pfd_t *, int, int);
extern caddr_t	page_mapin_pfn(pfd_t *, int, int, uint);
extern void	page_mapout(caddr_t);
extern void *	pfn_mapin_i_init(void);
extern void	pfn_mapin_i_done(void *);
extern caddr_t	pfn_mapin_i(void *, pfn_t, int);
extern void	pfn_mapout_i(caddr_t);
extern void	page_copy(pfd_t *, pfd_t *, int, int);
extern void	page_zero(pfd_t *, int, uint, uint);
#ifdef SN0
extern void	page_zero_nofault(pfd_t *, int, uint, uint);
#endif
extern uint	vcache2(struct pregion *, struct pageattr *, pgno_t);
extern uint	pfntocachekey(pfn_t, size_t);

extern int	valid_page_size(size_t);

extern pfd_t* pdetopfdat_hold(pde_t*);

#ifdef MH_R10000_SPECULATION_WAR
extern int	page_migrate_lowmem_page(pfd_t *, pfd_t **, int);
#endif /* MH_R10000_SPECULATION_WAR */

#ifdef _VCE_AVOIDANCE
extern int pfd_wait_rawcnt(pfd_t *,int);
extern void pfd_wakeup_rawcnt(pfd_t *);
extern void pfd_set_vcolor(pfd_t *,int);
extern int color_validation(register pfd_t *,register int,int,int);
#endif 


#if KSTACKPOOL
extern void	kstack_pool_init(void);
extern pfd_t	*kstack_alloc(void);
extern void	kstack_free(pfd_t *);
#endif

#ifdef EVEREST
extern int	page_counter_incr(pfn_t, int, int);
extern void	page_counter_decr(pfn_t, int, int);
extern int	page_counter_set(pfn_t, int, int);
extern void	page_counter_clear(pfn_t, int, int);
extern void	page_counter_undo(caddr_t, size_t, int);

extern void		dma_info_insert_user(pid_t, caddr_t,
					     size_t);
extern dma_info_t	*dma_info_lookup_user(pid_t, caddr_t,
					      size_t, int);
extern void		dma_info_insert_kern(dma_info_t *, caddr_t);
extern dma_info_t	*dma_info_lookup_kern(caddr_t, size_t, int);
extern void		dma_info_free(dma_info_t *);
#endif

#ifdef MH_R10000_SPECULATION_WAR
void krpf_early_init(caddr_t *);
void krpf_init(void);
void krpf_add_reference(pfd_t *,	/* pfdat for page referenced */
		__psint_t);		/* kernel VPN to add */
void krpf_remove_reference(pfd_t *,	/* pfdat for page referenced */
		__psint_t);		/* kernel VPN to remove */
void krpf_visit_references(pfd_t *,
		void (*)(void *, pfd_t *, __psint_t), void *);
#endif /* MH_R10000_SPECULATION_WAR */

#endif /* _KERNEL */
#endif /* !__SYS_PFDAT_H__ */
