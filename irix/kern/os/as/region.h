/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_REGION_H__
#define __SYS_REGION_H__

#ident	"$Revision: 3.132 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <ksys/as.h>
#include <sys/sema.h>
#include <sys/mman.h>
#include <sys/avl.h>
#include <ksys/ddmap.h>	/* backward compat for drivers */

/*	Each thread has a number of {p}regions which describe
 *	contiguous address spaces attached to the thread.
 *	Thread-specific attributes are kept in the pregion
 *	structure itself -- these include attach address,
 *	protections, (hooks to the) page table maps, etc.
 *
 *	Thread-independent attributes are kept in the region
 *	structure.  This includes hooks to the anonymous
 *	memory manager for the region, region size and ref count,
 *	and a lock used to single-thread vfault/page access.
 *
 *	The region lock protects most of the region struct fields
 *	except for r_nofree/p_nofree, which is covered by the mreg_lock
 *	spin lock, and p_noshrink, which is updated using atomic
 *	ll/sc ops.  The region lock also protects the fields in
 *	the pregion (most notably, the attributes) and the entries
 *	in the pmap for the region.
 */

struct vhandl;
/*
 * Per region descriptor.
 */

typedef	struct	region	{
	mrlock_t	r_lock;		/* lock	for reg, attrs, pmap, & anon  */
	uchar_t		r_type;		/* type of region OBSOLETE	      */
	uchar_t		r_color;	/* virtual address coherence color    */
	ushort		r_nofree;	/* # of requests not to allow freereg */
	ushort		r_flags;	/* Various flags defined below.	      */
	int		r_refcnt; 	/* number of users pointing at region */
	int		(*r_fault)(vhandl_t *, void *, uvaddr_t, int);
	union {
		void	*g_fltarg;	/* arg to (*r_fault)()		      */
		unsigned int g_gen;	/* for randomizing cachekey	      */
	} r_g;
	pgcnt_t		r_pgsz;		/* size in pages		      */
	pgcnt_t		r_swapres;	/* swap space (availsmem) reservation */
	off_t		r_fileoff;	/* File offset of this mapping.	      */
	off_t		r_maxfsize;	/* Max growable size of the file      */
					/* from which this region is loaded.  */
	off_t		r_mappedsize;	/* size in bytes of original mapping  */
	anon_hdl	r_anon;		/* Handle to anon memory manager.     */
	struct vnode   *r_vnode;	/* pointer to vnode where blocks are  */
	union {
		struct vnode *h_loadvnode;/* loadreg vnode (for PIOCOPENM)    */
		int h_shmid;
	} r_h;
	void		*r_spans;	/* unreserved spans		*/
#ifdef CKPT
	uint		r_ckptflags;	/* region checkpoint flags */
	int		r_ckptinfo;	/* region checkpoint info hande */
#endif

} reg_t;

#define r_fltarg	r_g.g_fltarg
#define r_gen		r_g.g_gen
#define r_loadvnode	r_h.h_loadvnode
#define r_shmid		r_h.h_shmid

/*
 * Region flags
 */
#define	RG_NOFREE	0x0001	/* Don't free region on last detach */
#define	RG_LOCKED	0x0002	/* region is locked for user DMA WAR */
#define RG_LOADV	0x0004	/* region has a loadvnode */
#define RG_CW		0x0008	/* cw region */
#define	RG_DFILL	0x0010	/* Demand fill pages (not demand zero) */
#define	RG_AUTOGROW	0x0020	/* Pages become autogrow after ip shrinks */
#define	RG_PHYS		0x0040	/* Region describes physical memory */
#define	RG_ISOLATE	0x0080	/* Region is isolated for real-time, no aging */
#define	RG_ANON		0x0100	/* (some) pages mapped anonymously */
#define	RG_USYNC	0x0200  /* Region aspace represents usync objects */
#define RG_HASSANON	0x0400	/* region has SANON pages */
#define RG_TEXT		0x0800	/* region contains pure, read-only text */
				/* (unmodified with respect to a.out)   */
#define RG_AUTORESRV	0x1000	/* reserve availsmem on demand when page*/
				/* goes anonymous			*/
#define	RG_PHYSIO	0x4000	/* Region maps a physical device space	     */
#define RG_MAPZERO      0x8000  /* region is maps dev zero pages */

/* RG_PHYSIO can be set only if RG_PHYS is set.  A Region should never 
 * have RG_PHYSIO Set  without RG_PHYS being set.
 * RG_PHYS -> Regions maps physical memory.  
 * RG_PHYS|RG_PHYSIO -> Region maps physical device space (e.g. VME Bus space)
 *
 * This provides a mechanism for separating  Regions that map 
 * physical memory space, and those that map physical device space. 
 * This distinction is used ONLY while handling dbx attaching a thread
 * that maps one of these regions, and user tries to dump virtual address 
 * represented by these regions. If only RG_PHYS is set, there are
 * no restrictions. If RG_PHYS|RG_PHYSIO is set, accesses greater than
 * 8 bytes are disallowed. 
 */

/*
 * Region types
 *
 * XXX	These should all go away, replaced by flag values.
 */
#define	RT_UNUSED	0	/* Region not being used.	*/
#define	RT_MEM		3	/* Regular swappable region */
#define RT_MAPFILE	4	/* Mapped file region 		*/

#ifdef CKPT
/*
 * region checkpoint flags
 */
#define CKPT_EXEC       0x01    /* map created by exec or dso mapping */
#define CKPT_MMAP       0x02    /* map created by mmap */
#define CKPT_SHM        0x04    /* map created by shmat */
#define	CKPT_PRIVATE	0x08	/* map created MAP_PRIVATE */
#endif

/*
 * Attributes contain a reference to a Policy Module
 * directing memory allocation for the virtual range
 * they cover.
 */
struct pm;

typedef struct pattribute {
	uint	prot	:  3,	/* read, write, execute	*/
		cc	:  3,	/* cache coherency	*/
		uc	:  2,	/* uncached qualifier bits for the cc field */
		watchpt	:  1,	/* page(s) being watched*/
			:  7,
		lockcnt	: 16;	/* page(s)' lock count	*/
} at_t;

typedef struct pageattr {
	struct pageattr	*attr_next;
	char		*attr_start;
	char	 	*attr_end;
	union {
		uint	attributes;
		at_t	at;
	} at;
        struct pm       *attr_pm;
} attr_t;

#define	attr_attrs	at.attributes
#define	attr_prot	at.at.prot
#define	attr_cc		at.at.cc
#define	attr_uc		at.at.uc
#define	attr_watchpt	at.at.watchpt
#define	attr_lockcnt	at.at.lockcnt

#define attr_pm_set(attr, pm)             \
                (ASSERT(pm),              \
                 (attr)->attr_pm = (pm),  \
                 pmo_incref(pm, pm) /*,qprintf("ADDR(0x%x), PM(0x%x), INC\n", (attr), (pm))*/)

#define attr_pm_get(attr)     \
                ((attr)->attr_pm)
                
#define attr_pm_copy(to, from)                    \
                (ASSERT((from)->attr_pm),         \
                 (to)->attr_pm = (from)->attr_pm, \
                 pmo_incref((from)->attr_pm, (from)) /*,qprintf("ADDR(0x%x), PM(0x%x), INC\n", (attr), (from)->attr_pm)*/)
                
#define attr_pm_equal(a, b) \
                ((a)->attr_pm == (b)->attr_pm)

#define attr_pm_unset(attr)                   \
                (ASSERT((attr)),              \
                 ASSERT((attr)->attr_pm),     \
                 pmo_decref((attr)->attr_pm, pmo_ref_attr),  \
                 (attr)->attr_pm = 0 /*,qprintf("UNSET-PM(0x%x)\n",(attr)->attr_pm)*/)
                        
/*	Pregion protections.  These are kept the same as those in mman.h
 *	so we don't have to translate flags for mmap and mprotect.
 */

#define	PROT_R		PROT_READ	/* Read permissions.		*/
#define	PROT_W		PROT_WRITE	/* Write permissions.		*/
#define	PROT_X		PROT_EXEC	/* Execute permissions.		*/
#define PROT_RW		(PROT_R|PROT_W)	/* convenient shorthands	*/
#define PROT_RX		(PROT_R|PROT_X)
#define PROT_WX		(PROT_W|PROT_X)
#define PROT_RWX	(PROT_R|PROT_W|PROT_X)


typedef struct pregion {
	/***** avlnode has to be first in struct **** 			*/
	avlnode_t	 p_avlnode;	/* each pregion is an avl node  */
	/***** Do not move avlnode from begining of struct ****		*/
	attr_t		 p_attrs;	/* Pregion protections.		*/
	attr_t		*p_attrhint;	/* last attr_t accessed.	*/
	struct region	*p_reg;		/* Pointer to the region.	*/
	pgcnt_t		 p_offset;	/* Offset into reg where preg   */
					/* begins                       */
	pgcnt_t		 p_pglen;	/* Size of pregion in pages     */
	uchar_t		 p_maxprots; 	/* Max protections allowed.	*/
	uchar_t		 p_type;	/* Type.			*/
	ushort		 p_flags;	/* Flags.			*/
	int              p_stk_rglen;   /* r_pgsz at pregion attach     */
	pgcnt_t		 p_pghnd;	/* getpages index into region	*/
	pgcnt_t		 p_nvalid;	/* nbr of valid pages in region	*/
	int		 p_noshrink;	/* # of requests not to allow shrink  */
	int		 p_fastuseracc; /* # of useraccs that took fast path */
	ushort		 p_nofree;	/* # of requests not to allow freepreg */
	struct pmap	*p_pmap;	/* pmap handle			*/
	struct pregion	*p_vchain;	/* Links on inode chain.	*/
	struct pregion	*p_vchainb;	/* Links on inode chain.	*/
} preg_t;

typedef 	avltree_desc_t 	preg_set_t;

/*
 * 	Macros to manipulate preg_set_t, or its elements.
 * PREG_FIRST:
 * 	Given a set of pregions (prset) [NOT a pointer to it], extract first.
 * PREG_NEXT:
 * 	Given an element (prp), extract next.
 *
 * Modifying functions:
 * -------------------
 *
 * PREG_INIT(prsetp):
 * 	Given a pointer to set of pregions (prsetp), initialize to null set .
 * 
 * PREG_DELETE(prsetp, prp):
 * 	Given a pointer to a set (prpsetp), and an element (prp),
 *		delete element from set; tree balanced if necessary.
 *
 * PREG_INSERT(prsetp, prp):
 * 	Given a pointer to a set (prpsetp), and an element (prp),
 *		insert element into set; tree balanced if necessary.
 *
 * Maintaining regions only as a list (no tree):
 * ---------------------------------------------
 *
 * PREG_SET_NEXT_LINK:
 * 	For efficiency, the elements in the set may be linked on a linear chain
 *	(esp. for the use of tsave, the list of regions saved on exec).
 *	This macro sets the next element in such a chain.
 * PREG_LINK_LVALUE:
 *	Gives the address of link used in a chain;
 *	used in conjunction with tsave.
 *
 * Search functions:
 * ----------------
 *
 * PREG_FINDRANGE(prsetp, value)
 *	given a value, find an element of set prsetp which contains value.
 *
 * PREG_FINDANYRANGE(prsetp, start, end, checklen)
 *	Return any element of set prsetp. Element should correspond to
 *	range contained in [start, end). If checklen is PREG_EXCLUDE_ZEROLEN,
 * 	the size of the range should be non-zero for a hit.
 *
 * PREG_FINDADJACENT(prsetp, value, dir):
 *	Find range r, such that 
 *		1. value is in range (similar to FINDRANGE)
 *	 (OR)	2. range immediately follows value (dir == PREG_SUCCEED) or
 *			range immediately preceeds value (dir == PREG_PRECEED).
 */

extern avlops_t preg_avlops;		/* ops vector for pregion avl trees */

#define PREG_INIT(prsetp)		avl_init_tree(prsetp, &preg_avlops)
#define PREG_DELETE(prsetp, prp)	avl_delete(prsetp, (avlnode_t *) prp) 
#define PREG_INSERT(prsetp, prp) 	avl_insert(prsetp, (avlnode_t *) prp)

#define PREG_LINK_LVALUE(prp)   	((preg_t **) &(prp->p_avlnode.avl_forw))
#define PREG_GET_NEXT_LINK(prp) 	((preg_t *) prp->p_avlnode.avl_forw)
#define PREG_SET_NEXT_LINK(prp, nextp) 	\
				prp->p_avlnode.avl_forw = (avlnode_t *) nextp

#define PREG_FIRST(prset)		((preg_t *) ((prset).avl_firstino))
#define	PREG_NEXT(prp)			((preg_t *) ((prp)->p_avlnode.avl_nextino))

#define PREG_INSERT_IMMEDIATE(prsetp, afterp, prp) 	\
	avl_insert_immediate(prsetp, (avlnode_t *) afterp, (avlnode_t *) prp) 

#define PREG_FINDRANGE(prsetp, value)			\
	(preg_t *) avl_findrange(prsetp, (__psunsigned_t) (value))

#define	PREG_FINDANYRANGE(prsetp, start, end, checklen)		\
	(preg_t *) avl_findanyrange(prsetp, start, end, checklen)

#define	PREG_FINDADJACENT(prsetp, value, dir)		\
	(preg_t *) avl_findadjacent(prsetp, value, dir)

/* SPT
 * If a pregion is using Shared Page Tables the p_nvalid count
 * can be inaccurate. To avoid negative values the value should
 * be tested before decrement.
 */
#define	PREG_NVALID_DEC(prp, n)	{ if ((prp)->p_nvalid >= (n)) \
					(prp)->p_nvalid -= (n); }

/*
 * checklen values for FINDANYRANGE - includes or exclues
 * zerolength regions for finding match.
 */
#define PREG_INCLUDE_ZEROLEN		AVL_INCLUDE_ZEROLEN
#define PREG_EXCLUDE_ZEROLEN		AVL_EXCLUDE_ZEROLEN

/*
 * dir values for FINDADJACENT
 */
#define PREG_SUCCEED			AVL_SUCCEED
#define PREG_PRECEED			AVL_PRECEED


#define	p_regva		p_attrs.attr_start

/*	Pregion types.
 *
 *	Note: do not change these without checking core() first.
 */
#define	PT_UNUSED	0x00		/* Unused region.		*/
#define	PT_STACK	0x03		/* Stack region.		*/
#define	PT_SHMEM	0x04		/* Shared memory region.	*/
#define	PT_MEM		0x05		/* Generic memory region.	*/
					/* Space was Double mapped memory.*/
#define PT_GR 		0x08		/* Graphics region 		*/
#define PT_MAPFILE	0x09		/* Memory mapped file region 	*/
#define PT_PRDA		0x0a		/* PRDA region			*/

/*
 * Pregion flags.
 */
#define	PF_DUP		0x01		/* Dup on fork.			*/
#define	PF_NOPTRES	0x02		/* No pmap reservations made	*/
#define PF_NOSHARE	0x04		/* Do not share on sproc	*/
#define PF_SHARED	0x08		/* It is possible that this pregion's
					 * pmap is shared with someone
					 */
#define PF_AUDITR	0x10		/* Read audit			*/
#define PF_AUDITW	0x20		/* Write audit			*/
#define	PF_FAULT	0x40		/* Vfault logging enabled	*/
#define	PF_TSAVE	0x80		/* if a tsave'd  pregion 	*/
#define PF_PRIVATE	0x100		/* pregion is on private list	*/
#define PF_PRIMARY	0x200		/* pregion is 'primary' exec'd object */
#define PF_TXTPVT	0x400		/* privatized text pregion      */
#define PF_LCKNPROG     0x800           /* pregion lockdown in progress  */


#define PREG_MARK_TSAVE(prp)	prp->p_flags |= PF_TSAVE
#define PREG_UNMARK_TSAVE(prp)	prp->p_flags &= ~PF_TSAVE
#define PREG_IS_TSAVE(prp)	(prp->p_flags & PF_TSAVE)

/*	Several ``logical page numbers'' are needed by various routines:
 *	a monatomically increasing page number used to generate a key for
 *	the page allocator, to ensure that logically successive user pages
 *	get memory with logically successive d- and i-cache indices;
 *	a unique page identifier used by the page cache and anonymous
 *	memory manager.
 */
#define vtorpn(P,V)	(btoct((V) - (P)->p_regva) + (P)->p_offset +    \
				((P)->p_type != PT_STACK ? 0 :  \
					(P)->p_reg->r_pgsz-(P)->p_stk_rglen))

#define vtoapn(P,V)	((P)->p_type == PT_STACK ? \
				(P)->p_reg->r_pgsz - 1 - vtorpn(P,V) : \
				vtorpn(P,V))

#define rpntoapn(P,N)	((P)->p_type == PT_STACK ? \
				(P)->p_reg->r_pgsz - 1 - (N) : \
				(N))

#define rpntov(P,N)	((P)->p_regva + (ctob((N) - (P)->p_offset)))

#if R10000_SPECULATION_WAR
#define apntov(P,APN)	((P)->p_type == PT_STACK ? \
         			rpntov(P, (P)->p_reg->r_pgsz - 1 - (APN)) : \
         			rpntov(P, APN))
#endif

/*	macro to find number of pages left in pregion starting at vaddr 
 */

#define pregpgs(P, V)	((P)->p_pglen - btoct((V) - (P)->p_regva))

/*	Some generic flags for region routines.
 */
#define RF_FORCE	0x0002		/* force a private region */
#define RF_NOFLUSH	0x0004		/* do not flush tlbs (detachreg) */
#define RF_TSAVE	0x0008		/* preg is tsave'd (detachreg) */
#define RF_EXPUNGE	0x0010		/* expunge lockings */
#define RF_EXITING	0x0020		/* detach due to process exit */

extern preg_t	syspreg;	/* Pregion for dynamic system space */
extern reg_t	sysreg;		/* Region for dynamic system space */

struct vnode;
struct pas_s;
struct ppas_s;

extern void vrecalcrss(struct pas_s *);

/* region allocator */
reg_t *allocreg(struct vnode *, uchar_t, ushort);

/* region free routine */
void freereg(struct pas_s *,reg_t *);

/* Attach region to thread. */
int vattachreg(reg_t *, struct pas_s *, struct ppas_s *,
		caddr_t, pgno_t, pgno_t, uchar_t, uchar_t, uchar_t,
		ushort, struct pm*, preg_t **);

/* Detach region from thread. */
int vdetachreg(preg_t *, struct pas_s *, struct ppas_s *, caddr_t, pgno_t, int);

/* Duplicate region (fork). */
int dupreg(struct pas_s *, preg_t *, struct pas_s *, struct ppas_s *, caddr_t, uchar_t, ushort, struct pm *, int, preg_t **);

/* Grow region. */
int vgrowreg(preg_t *, struct pas_s *, struct ppas_s *, uchar_t, pgno_t);

/* Load region from file. */
int loadreg(preg_t *, struct pas_s *, struct ppas_s *, caddr_t, struct vnode *, off_t, size_t);

/* routines for execself support */
void unattachpreg(struct pas_s *, struct ppas_s *, preg_t *);
int reattachpreg(struct pas_s *, struct ppas_s *, preg_t *, struct pm *);

/* Find pregion from virtual address. */
preg_t *findpreg(struct pas_s *, struct ppas_s *, uvaddr_t);
preg_t *findanypreg(struct pas_s *, struct ppas_s *, uvaddr_t);

/* Find first pregion within address range. */
preg_t *findfpreg(struct pas_s *, struct ppas_s *, uvaddr_t, uvaddr_t);

/* Find first pregion within address range restricted to private or shared  */
preg_t *findfpreg_select(struct pas_s *, struct ppas_s *, uvaddr_t, uvaddr_t, int);

/* Find pregion of given type. */
preg_t *findpregtype(struct pas_s *, struct ppas_s *, uchar_t);

/* Find attributes structure for an address within a pregion */
attr_t *findattr(preg_t *, char *);

/* Clip an attribute structure */
attr_t *attr_clip(attr_t *, char *, char *);

/* Find attributes structure that maps (exactly) [start, end) */
attr_t *findattr_range(preg_t *, char *, char *);

/* Same as findattr_range but don't clip or coalesce */
attr_t *findattr_range_noclip(preg_t *, char *, char *);

/* Change protection for region. */
void chgprot(struct pas_s *, preg_t *, char *, pgno_t, uchar_t);

/* Returns size of specified region in pages */
unsigned getpregionsize(preg_t *);

/* Returns current size of uthread */
pgcnt_t getpsize(struct pas_s *, struct ppas_s *);

/* invalidate an address range */
void invalidateaddr(struct pas_s *, preg_t *, uvaddr_t, size_t);

/* change text region to a PRIVATE writable version */
int vgetprivatespace(struct pas_s *, struct ppas_s *, uvaddr_t, preg_t **);
void vungetprivatespace(struct pas_s *, struct ppas_s *);
int ispcwriteable(struct pas_s *, struct ppas_s *, uvaddr_t, reg_t *);

/* remove any saved text-style regions */
extern void remove_tsave(struct pas_s *, struct ppas_s *, int);

/* trim pmap */
extern void try_pmap_trim(struct pas_s *, struct ppas_s *);

/* return # valid pages in pregion */
extern  pgcnt_t	count_valid(preg_t *, caddr_t, pgno_t);

extern int unswapreg(struct pas_s *, reg_t *, int, int, preg_t *);

/* Initialize the region table. */
void reginit(void);

/* preg zone */
extern struct zone *preg_zone;

/* sync back mmaped pages */
extern int msync1(struct pas_s *, preg_t *, uvaddr_t, pgcnt_t, int);

#define	reglock(RP)	mrupdate(&(RP)->r_lock)
#define reglock_rd(RP)	mraccess(&(RP)->r_lock)
#define	creglock(RP)	mrtryupdate(&(RP)->r_lock)
#define	regrele(RP)	{ ASSERT(mrislocked_any(&(RP)->r_lock)); \
					mrunlock(&(RP)->r_lock); }
#define reglocked(RP)   (mrislocked_any(&(RP)->r_lock))

/*
 * Macros Used for Page replication.
 */
#ifdef	NUMA_REPLICATION
/* Region is replicable if it has been flagged as Text */
#define	REG_REPLICABLE(rp) \
		((rp) && !(rp)->r_anon && \
		(rp)->r_vnode && VN_ISREPLICABLE((rp)->r_vnode))

#else	/* !NUMA_REPLICATION */	

#define	REG_REPLICABLE(rp)	(0)	/* Evaluate as a constant  */

#endif	/* NUMA_REPLICATION */

/* XXX should be fairly private - see sys/ddmap.h */
struct __vhandl_s {
	struct pregion	*v_preg;	/* Pointer to the pregion.	*/
	uvaddr_t	v_addr;		/* Virtual address of region.	*/
};

#ifdef __cplusplus
}
#endif

#endif /* __SYS_REGION_H__ */
