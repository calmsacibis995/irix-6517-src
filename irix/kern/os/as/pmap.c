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
#ident	"$Revision: 1.142 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <ksys/as.h>
#include "as_private.h"
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fault_info.h>
#include <ksys/exception.h>
#include <sys/idbgentry.h>
#include <sys/immu.h>
#include <sys/lpage.h>
#include <sys/kmem.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include "pmap.h"
#include <ksys/umapop.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uthread.h>
#include <sys/var.h>
#include <sys/vnode.h>
#include "ksys/vm_pool.h"
#include "os/numa/pm.h"
#include <ksys/vpag.h>
#include "ksys/vmmacros.h"

/*
 * The following macros are used to store information about which job/
 * address space is using a page table. This helps in identifying the
 * address space using a pte, since that is the same address space that
 * is using the page table. For shared page tables, currently we do not
 * track all the address spaces/jobs which use it. The underlying 
 * implementation is to track the owning address space in the pf_pas
 * field of the pfdat for the page table page, and the job in pf_job.
 */
#define LINK_PT_TO_ASJOB(pt, pas, vpag)					\
	{								\
		pfd_t *tpfd = pfntopfdat(kvtophyspnum(pt));		\
									\
		/* tpfd->pf_pas = (void *)(pas); */			\
		tpfd->pf_job = (void *)(vpag);				\
	}

#define GET_AS_FOR_PT(pt) \
		(pas_t *)((pfntopfdat(kvtophyspnum(pt)))->pf_pas)

#define GET_JOB_FOR_PT(pt) \
		(vpagg_t *)((pfntopfdat(kvtophyspnum(pt)))->pf_job)

/*
 * In the future, if we want to track the address spaces using a 
 * shared page table, we will have to define the following procedures.
 * For now, they are defined as stubs and compiled out, their sole
 * purpose is to mark where the neccessary calls need to be made.
 */
#define	pmap_spt_linkas(pfd, pas)
#define pmap_spt_unlinkas(pfd, pas)

/*	Local routines to implement external interface.
 */
#if _MIPS_SIM == _ABI64
#define WIRESEGTBL() \
	ASSERT((!utas->utas_segtbl || IS_KSEGDM(utas->utas_segtbl)) && \
	     (!utas->utas_shrdsegtbl || IS_KSEGDM(utas->utas_shrdsegtbl)))
#else
#define WIRESEGTBL() wiresegtbl()
#endif

/*	Definitions for pmap_type == PMAP_SEGMENT
 */

/*	Overlay of pmap_t.
 */
typedef struct pmap_seg {
	pde_t 		**pseg_segptr;	/* pointer to segment table */
	short		pseg_segcnt;	/* number of active segments */
	uchar_t		pseg_type;	/* pmap type	*/
	uchar_t		pseg_flags;	/* flags	*/
	time_t		*pseg_trimtime;	/* last trim time */
#if DEBUG
	pgcnt_t		pseg_resv;	/* # pages reserved */
#endif
#ifdef PMAP_SEG_DEBUG
	char   		*pseg_bitmap;
#define SEG_BITMAPSZ	(NPGTBLS/8)
#endif
} pseg_t;

static void	seg_init_pmap(void);
static void	seg_create(pseg_t *);
static void	seg_destroy(pseg_t *);
static pde_t	*seg_pte(pas_t *, pseg_t *, int, int);
static pde_t	*seg_probe(pseg_t *, int *, int);
static int	seg_free(pas_t *, pseg_t *, char *, pgno_t, int);
static int	seg_trim(pseg_t *);

/* Definitions for pmap_type == PMAP_TRILEVEL */

/* Overlay of pmap_t that is passed to the common routines shared by
 * both PMAP_SEGMENT and PMAP_TRILEVEL pmap types.
 * This structure is reduced to only those field required by the
 * common routines to save space - since the 'base table' of a
 * PMAP_TRILEVEL process is made up of these.
 */

typedef struct pmap_segcommon {
	pde_t	**pcom_segptr;		/* pointer to segment table */
	short	pcom_segcnt;		/* count of active segments in table */
} pcom_t;


/* Overlay of pmap_t for 3-level segment manager. */
typedef struct pmap_3level {
	pcom_t		*p3level_basetable; /* pointer to base table */
	short		p3level_segtabcnt;  /* no. of active segment tables */
	uchar_t		p3level_type;	    /* pmap type	*/
	uchar_t		p3level_flags;	    /* flags	*/
	time_t		p3level_trimtime;   /* last trim time */
#if DEBUG
	pgcnt_t		p3level_resv;	    /* # pages reserved */
#endif
#ifdef PMAP_SEG_DEBUG
	char   		*p3level_bitmap;
#define SEG2LEVEL_BITMAPSZ	(BASETABSIZE/8)
#endif
} p3level_t;

#ifdef _MIPS3_ADDRSPACE
/* Prototypes for PMAP_TRILEVEL */
static void	trilevel_init(void);
static void	trilevel_create(p3level_t *);
static void	trilevel_destroy(p3level_t *);
static pde_t	*trilevel_pte(pas_t *, p3level_t *, int, int);
static pde_t	*trilevel_probe(p3level_t *, int *, int);
static int	trilevel_free(pas_t *, p3level_t *, char *, pgno_t, int);
static int	trilevel_trim(p3level_t *);
#endif

/* Prototypes for routines shared between PMAP_SEG and PMAP_TRILEVEL */
static void	segcommon_lclsetup(pas_t *, utas_t *, pmap_t *, pgno_t, pgno_t);
static void	segcommon_setup(pas_t *, pmap_t *, pgno_t, pgno_t);
static int	segcommon_split(pas_t *, ppas_t *);
static int	segcommon_trim(pcom_t *, int);
static void	segcommon_destroy(pcom_t *, int, int);
static int	segcommon_free(pas_t *, pde_t *, int, int);
static int	segcommon_reserve(pmap_t *, pas_t *, ppas_t *, uvaddr_t, pgcnt_t, int);
static void	segcommon_unreserve(pmap_t *, pas_t *, ppas_t *, uvaddr_t, pgcnt_t, int);

/* SPT
 * Shared Page Tables local prototypes
 */
#ifdef	SPT_DEBUG
#undef	STATIC
#define	STATIC
#endif	/* SPT_DEBUG */

STATIC	int	pmap_spt_free(pas_t *, pmap_t *, uvaddr_t, size_t, int);
STATIC	void	pmap_spt_init(void);

#ifdef	SPT_DEBUG
STATIC void	pmap_ptfree_spt_check(pde_t *pte);
#endif	/* SPT_DEBUG */

/*
 * PTPOOL: Pages containing page tables are usually full of zeroes at
 * release time. The PT Pool pools the page table on a special set of
 * free lists (one per color). Pages allocated from the PT pool do not
 * need to be cleaned up.
 * There are cases in which a page table may contain special
 * entries, called sentinels. These are not cleaned up during the
 * destruction of the address space, because they are external to regions.
 * The setting of the sentinels happens in presence of shared address
 * spaces and local * mappings. Page tables that may contain sentinels
 * should not be released to the page pool. A new flag has been added
 * to the pmap to catch this situation. The flag is set in pmap_lclsetup()
 * and pmap_lclteardwn(). The flag is checked by ptpool_free(). The
 * to the function segcommon_destroy() had to be changed to include
 * the pmap flag.
 */

/* prototypes for PTPOOL */
void		ptpool_init(void);
static void	*ptpool_alloc(pas_t *, uint_t, int, boolean_t);
static void	ptpool_free(void *, int);
static int	ptpool_shake(int);

static zone_t	*pmap_zone;
pgcnt_t		pmapmem;	/* pages in pmaps */
#if DEBUG
pgcnt_t		pmapresv;	/* # pages reserved */
#endif

void	*pmap_segtab_alloc(int);

/*
 *
 *	Externally-defined pmap interfaces:
 *
 */

void
init_pmap(void)
{
	if (pmap_zone != NULL)
		return;

#ifdef PMAP_SEG_DEBUG
	pmap_zone = kmem_zone_init(sizeof(pmap_t) + sizeof(char *), "pmaps");
#else
	pmap_zone = kmem_zone_init(sizeof(pmap_t), "pmaps");
#endif
	ASSERT(pmap_zone);

	/* pmap_type-specific initializations...
	*/
	seg_init_pmap();
#ifdef _MIPS3_ADDRSPACE
	trilevel_init();
#endif
	/* SPT
	 * Shared Page Table initialization...
	 */
	pmap_spt_init();

        rmap_init();
}


/*	Create a pmap module for a process.
 */
/* ARGSUSED */
pmap_t *
pmap_create(int is64, int type)
{
	register pmap_t *pp = kmem_zone_zalloc(pmap_zone, KM_SLEEP);

	switch (type) {

	case PMAP_SEGMENT:

#ifdef _MIPS3_ADDRSPACE
	if (is64)
		trilevel_create((p3level_t *)pp);
	else
#endif
		seg_create((pseg_t *)pp);
		break;


	default:
		;
	}

	return pp;
}

static void
pmap_error(char *caller)
{
	cmn_err(CE_PANIC, "%s: unknown pmap type", caller);
}

/*	Shut down a pmap.  Free all resources.
*/
void
pmap_destroy(register pmap_t *pp)
{
	switch (pp->pmap_type) {

	case PMAP_SEGMENT:
		seg_destroy((pseg_t *)pp);
		break;

#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		trilevel_destroy((p3level_t *)pp);
		break;
#endif

	default:
		pmap_error("pmap_destroy");
	}

	kmem_zone_free(pmap_zone, pp);
}

int
pmap_reserve(
	pmap_t *pp,
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t vaddr,
	pgcnt_t npgs)
{
	switch (pp->pmap_type) {

	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
		return segcommon_reserve(pp, pas, ppas, vaddr, npgs, 0);

	}

	pmap_error("pmap_reserve");
	/*NOTREACHED*/
	return(0);
}

void
pmap_unreserve(
	pmap_t *pp,
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t vaddr,
	pgcnt_t npgs)
{
	switch (pp->pmap_type) {

	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
		segcommon_unreserve(pp, pas, ppas, vaddr, npgs, 0);
		return;
	}

	pmap_error("pmap_unreserve");
	/*NOTREACHED*/
}

/*	pmap_pte:	Return a pointer to the page table entry
 *			for vaddr in pmap pp.  Creates an (empty)
 *			entry if none exists.
 */
pde_t *
pmap_pte(
	register pas_t	*pas,
	register pmap_t *pp,
	register char	*vaddr,
	register int 	 vmflag)
{
	register int 	type;
	register pde_t	*pte;

	type = pp->pmap_type;

	switch (type) {

	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
	    {
		
#ifdef _MIPS3_ADDRSPACE
		if (type == PMAP_TRILEVEL)
			pte = trilevel_pte(pas, (p3level_t *)pp, btost(vaddr),
					    vmflag);
		else
#endif
			pte = seg_pte(pas, (pseg_t *)pp, btost(vaddr), vmflag);

		if (pte)
			return(pte + btoct(soff(vaddr)));
		else
			return NULL;
	    }


	}

	pmap_error("pmap_pte");
	/*NOTREACHED*/
	return 0;
}

/*	pmap_ptes:	Return a pointer to the page table entries
 *			that map (starting at) vaddr for pmap pp.
 *			Number of page table entries desired passed
 *			indirectly; number available returned indirectly.
 *			Creates (empty) entries where none already exist.
 */
pde_t *
pmap_ptes(
	register pas_t	*pas,
	register pmap_t *pp,
	register char	*vaddr,
	pgno_t		*size,	/* IN / OUT */
	int		vmflag)
{
	register int 	type;
	register pde_t	*pte;
	register int	segoff;
	register pgno_t	sz = *size;

	type = pp->pmap_type;

	switch (type) {
	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
	    {
		segoff = btoct(soff(vaddr));
#ifdef _MIPS3_ADDRSPACE
		if (type == PMAP_TRILEVEL)
			pte = trilevel_pte(pas, (p3level_t *)pp, btost(vaddr),
					    vmflag);
		else
#endif
			pte = seg_pte(pas, (pseg_t *)pp, btost(vaddr), vmflag);

		if (!pte) {
			*size = 0;
			return NULL;
		}
		*size = MIN(sz, NPGPT - segoff);
		break;
	    }

	default:
		pmap_error("pmap_ptes");
	}
	
	return(pte + segoff);
}

/*	pmap_probe:	Return a pointer to the first extant pmap
 *			entry that is mapped by (pp,vaddr,sizein).
 *			vaddr and sizein are updated to reflect the
 *			number of entries that are skipped.
 *			The number of available, contiguous entries
 *			are returned in sizeout.
 */
pde_t *
pmap_probe(
	register pmap_t *pp,
	register char   **vaddr,
	register pgno_t *sizein,
	pgno_t          *sizeout)
{
	register pgno_t szin = *sizein;
	register int 	type;
	register char	*va;
	register int	segoff;


	if (szin == 0) {
		*sizein = 0, *sizeout = 0;
		return (pde_t *)NULL;
	}

	type = pp->pmap_type;

	switch (type) {
	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
	    {
		auto int	segno = btost(*vaddr);
		register pgno_t	tmp = btoct(*vaddr) + szin - 1;
		register pde_t	*pte;

#ifdef _MIPS3_ADDRSPACE
		if (type == PMAP_TRILEVEL)
			pte = trilevel_probe((p3level_t *)pp, &segno,
						ctost(tmp));
		else
#endif
			pte = seg_probe((pseg_t *)pp, &segno, ctost(tmp));

		if (pte == NULL) {
			*sizein = 0, *sizeout = 0;
			return (pde_t *)NULL;
		}

		/*	Trim vaddr and sizein if the first entry
		 *	wasn't in the page map.
		 */
		va = (char *)stob(segno);
		if (va > *vaddr) {
			tmp = btoc(va - *vaddr);
			if (tmp >= szin) {
				*sizein = 0, *sizeout = 0;
				return (pde_t *)NULL;
			}
			szin -= tmp;
			*sizein = szin;
			*vaddr = va;
		}

		segoff = btoct(soff(*vaddr));
		*sizeout = MIN(szin, NPGPT - segoff);

		return(pte + segoff);
	    }
	}

	pmap_error("pmap_probe");
	/*NOTREACHED*/
	return((pde_t *)0);
}

/*
 *	pmap_free:
 *
 *	Free all memory mapped by pmap entries mapping pgsz pages,
 *	starting at vaddr.  Returns number of pages freed.
 */
int
pmap_free(
	register pas_t 	*pas,
	register pmap_t *pp,
	register char	*vaddr,
	register pgno_t	pgsz,
	register int	free)
{
	ASSERT((pas) || (free == PMAPFREE_TOSS));
	if (pmap_spt(pp)) {
		if (pmap_spt_free(pas, pp, vaddr, pgsz, free))
			return 0;
	}

	switch (pp->pmap_type) {

	case PMAP_SEGMENT:
		return seg_free(pas, (pseg_t *)pp, vaddr, pgsz, free);
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		return trilevel_free(pas, (p3level_t *)pp, vaddr, pgsz, free);
#endif

	}

	pmap_error("pmap_free");
	/*NOTREACHED*/
	return(0);
}

int
pmap_trim(register pmap_t *pp)
{
	switch (pp->pmap_type) {

	case PMAP_SEGMENT:
		return seg_trim((pseg_t *)pp);
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		return trilevel_trim((p3level_t *)pp);
#endif
	}

	pmap_error("pmap_trim");
	/*NOTREACHED*/
	return(0);
}

/*
 *	pmap_lclsetup:
 *
 *	Perform any initialization needed when attaching or growing a
 * 	local mapping in a shared address process.
 */
void
pmap_lclsetup(
	register pas_t	*pas,
	register utas_t *utas,
	register pmap_t *pp,
	register char	*vaddr,
	register pgno_t	pgsz)
{
	switch (pp->pmap_type) {
	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
		if (pgsz == 0)	/* region hasn't been grown yet */
			return;	/* we'll catch it in growreg */

		segcommon_lclsetup(pas, utas, pp, btoct(vaddr), pgsz);
		pp->pmap_flags |= PMAPFLAG_LOCAL;
		return;
	}

	pmap_error("pmap_lclsetup");
}

/*
 *	pmap_lclteardwn:
 *
 *	Perform any teardown needed when deattaching or shrinking a
 * 	local mapping in a shared address process.
 */

void
pmap_lclteardwn(
	register pmap_t *pp,
	char		*vaddr,
	pgno_t		pgsz)
{

	switch (pp->pmap_type) {
	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
	    {
		register pde_t	*pte;
		pgno_t		pgcnt;

		/*
		 *	Store the SHRD_SENTINEL value into all pte's that
		 *	used to be associated with the given local mapping.
		 */

		while (pgsz) {
			pte = pmap_probe(pp, &vaddr, &pgsz, &pgcnt);
			vaddr += ctob(pgcnt);
			pgsz -= pgcnt;

			while (pgcnt) {
                                pg_setpgi(pte, SHRD_SENTINEL);
                                pte++;
				pgcnt--;
			}
		}
		pp->pmap_flags |= PMAPFLAG_LOCAL;
		return;
	    }
	}

	pmap_error("pmap_lclteardwn");
}

int
pmap_split(pas_t *pas, ppas_t *ppas)
{

	switch (pas->pas_pmap->pmap_type) {
	case PMAP_SEGMENT:
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
#endif
		return segcommon_split(pas, ppas);
	}

	pmap_error("pmap_split");
	/*NOTREACHED*/
	return EINVAL;
}


/*	pmap type 	PMAP_SEGMENT
 *
 *	Manages whole pages of page tables that are mapped into
 *	KPTESEG, for the utlbmiss handler.
 *
 *	Page tables are managed by an array of pointers to page tables.
 *
 *	Shared address processes that don't have local mappings simply
 *	used the shared pmap.  Those that have local mappings also have
 * 	a private pmap.  For these processes, we switch utlbmiss handlers
 *	to one that knows to look in two different segment tables if
 * 	necessary.  One is mapped at KPTESEG, the other immediately preceeds
 *	it.  For segments in which there are either shared regions or
 *	private regions, but not both, the entries in KPTESEG will be used.
 *	For segments that contain both private and shared mappings (where
 *	the private mappings does not completely hide the shared mapping),
 *	the first table at KPTESEG will contain the private mappings and
 *	the second table preceeding it will contain the entries for the
 *	shared mappings not hidden by the private mappings.
 */

static lock_t	seg_lock;
#define	seglock()	mutex_spinlock(&seg_lock)
#define	segunlock(S)	mutex_spinunlock(&seg_lock, (S));

struct zone	*segptr_zone;

#define MAXSEGS	NPGTBLS

static void
seg_init_pmap(void)
{
	segptr_zone = kmem_zone_init(NPGTBLS * sizeof(pde_t *),
							"segment tables");
	ASSERT(segptr_zone);

	spinlock_init(&seg_lock, "seglock");
}

static void
seg_create(register pseg_t *pp)
{
	/*
	 * pp arrives zeroed
	 */
	pp->pseg_type = PMAP_SEGMENT;
	pp->pseg_segptr = (pde_t **)kmem_zone_zalloc(segptr_zone, KM_SLEEP);
#ifdef PMAP_SEG_DEBUG
	pp->pseg_bitmap = kmem_zalloc(SEG_BITMAPSZ, 0);
#endif
}

static void
seg_destroy(register pseg_t *pp)
{
	register int	sc = pp->pseg_segcnt;

	ASSERT(pp->pseg_segptr);
	ASSERT(sc >= 0);
	if (sc)
		segcommon_destroy((pcom_t *)pp, NPGTBLS, pp->pseg_flags);

	kmem_zone_free(segptr_zone, pp->pseg_segptr);
#ifdef PMAP_SEG_DEBUG
	ASSERT(bftstclr(pp->pseg_bitmap, 0, NPGTBLS) == NPGTBLS);
	kmem_free(pp->pseg_bitmap, SEG_BITMAPSZ);
#endif
}

/*	Find the page of page table entries for segno.
*/
static pde_t *
seg_pte(
	register pas_t  *pas,
	register pseg_t *pp,
	register int	segno,
	int vmflag)
{
	register pde_t	**ppt = pp->pseg_segptr + segno;
	register pde_t	*pte;
	register pde_t	*pt;
	register int	s;

	ASSERT(ppt);
	ASSERT(segno < MAXSEGS);

	if (pt = *ppt)
		return (pt);
	

	if ((pte = ptpool_alloc(pas, segno, vmflag, B_TRUE)) == NULL)
		return NULL;


	/*	aspacelock(UPD) protects against deletions only,
	 *	so must use seglock here while we (probably)
	 *	only hold aspacelock(ACC).
	 */
	s = seglock();
	if (pt = *ppt) {
		segunlock(s);
		/*
		 * pass zero as flag because the page is
		 * known to contain zeroes only (never used)
		 */
		ptpool_free(pte, 0);
		return (pt);
	}

	*ppt = pte;
	ASSERT(pp->pseg_segcnt >= 0);
	pp->pseg_segcnt++;
	segunlock(s);

	return (pte);
}

static pde_t	*
seg_probe(
	register pseg_t	*pp,
	register int	*firstseg,
	register int	 lastseg)
{
	register pde_t	**ppt = pp->pseg_segptr;
	register pde_t	*pte;
	register int	segno;

	ASSERT(ppt);
	ASSERT(lastseg <= MAXSEGS);

	for ( segno = *firstseg ; segno <= lastseg ; segno++ ) {

		if (pte = *(ppt + segno)) {
			*firstseg = segno;
			return (pte);
		}
	}

	return (pde_t *)NULL;
}

/*	seg_free flags:	PMAPFREE_TOSS	remove mapping only (no page free)
 *			PMAPFREE_FREE	free page
 *			PMAPFREE_SANON	free shared anon page
 *			PMAPFREE_UNRES	unreserve mapping rights
 */
static int
seg_free(
	register pas_t 	*pas,
	register pseg_t *pp,
	char *vaddr,
	register pgno_t	pgsz,
	register int	free)
{
	register pgno_t	pg = btoct(vaddr);
	register pde_t	*pte;
	register pde_t	**ppt = pp->pseg_segptr + ctost(pg);
	register int	segoff, sz;
	int		ret = 0;

	ASSERT(pgsz == 0 || ctost(pg) < MAXSEGS);

	while (pgsz > 0) {
		segoff = ptoff(pg);
		if (segoff + pgsz > NPGPT)
			sz = NPGPT - segoff;
		else
			sz = pgsz;

		pgsz -= sz;
		pg += sz;

		if (pte = *ppt) {
			pte += segoff;
			ret += segcommon_free(pas, pte, sz, free);
		}

		ppt++;
	}

	return ret;
}



/*	Trim unused data structures in pmap.
 *	MUST be called with and aspacelock held in update mode.
 */
static int
seg_trim(register pseg_t *pp)
{

	ASSERT(pp->pseg_segptr);

	if (pp->pseg_flags & PMAPFLAG_NOTRIM)
		return 0;

	ASSERT(pp->pseg_segcnt >= 0);

	return segcommon_trim((pcom_t *)pp, NPGTBLS);
}

#ifdef _MIPS3_ADDRSPACE

/*	pmap type: 	PMAP_TRILEVEL */

struct zone	*basetable_zone;
struct zone	*segtable_zone;

static void
trilevel_init(void)
{
	/* Both zones are allocated private to prevent merging with
	 * other zones. Allocators from these zones will always
	 * use the VM_DIRECT flag.
	 */
	basetable_zone = kmem_zone_private(BASETABSIZE * sizeof(pcom_t),
						"segment base tables");
	segtable_zone = kmem_zone_private(SEGTABSIZE * sizeof(pde_t *),
						"second-level segment tables");
}

static void
trilevel_create(register p3level_t *pp)
{
	/*
	 * pp arrives zeroed
	 */
	pp->p3level_type = PMAP_TRILEVEL;
	if (basetable_zone)
		pp->p3level_basetable = kmem_zone_zalloc(basetable_zone,
							 KM_SLEEP|VM_DIRECT);
	else
		pp->p3level_basetable = kmem_zalloc(BASETABSIZE*sizeof(pcom_t),
					KM_SLEEP|VM_DIRECT|VM_PHYSCONTIG);
	ASSERT(IS_KSEG0(pp->p3level_basetable));
}

static void
trilevel_destroy(register p3level_t *pp)
{
	register int	sc;
	register pcom_t	*pcomp;
	register int	i;

	sc = pp->p3level_segtabcnt;
	pcomp = pp->p3level_basetable;

	ASSERT(pcomp);
	ASSERT(sc >= 0);
#ifdef PMAPDEBUG
	if (sc > 0) {
		for(i = 0; i < BASETABSIZE; i++, pcomp++)
			if (pcomp->pcom_segptr)
				sc--;

		if (sc != 0)
			cmn_err(CE_PANIC,
				"seg2level_destroy: sc should be 0, is %d\n");
		sc = pp->p3level_segtabcnt;
		pcomp = pp->p3level_basetable;
	}
#endif

	if (sc)
		for (i = BASETABSIZE; --i >= 0; pcomp++)
			if (pcomp->pcom_segptr) {
				segcommon_destroy(pcomp, SEGTABSIZE,
						  pp->p3level_flags);
				if (segtable_zone)
					kmem_zone_free(segtable_zone,
							pcomp->pcom_segptr);
				else
					kmem_free(pcomp->pcom_segptr,
						  SEGTABSIZE * sizeof(pde_t *));
#ifdef PMAPDEBUG
				pcomp->p_segptr = NULL;
#endif
				if (--sc == 0)
					break;
			}

#ifdef PMAPDEBUG
	pcomp = pp->p3level_basetable;
	for (i = 0; i < BASETABSIZE; i++, pcomp++)
		if (pcomp->pcom_segptr)
			cmn_err(CE_PANIC, "seg2level_destroy: pp %x", pp);
#endif

	if (basetable_zone)
		kmem_zone_free(basetable_zone, pp->p3level_basetable);
	else
		kmem_free(pp->p3level_basetable, BASETABSIZE * sizeof(pcom_t));
}

/*	Find the page of page table entries for segno.
*/
static pde_t *
trilevel_pte(
	register pas_t	*pas,
	register p3level_t *pp,
	register int segno,
	int vmflag)
{
	register pde_t	**ppt, **segptr;
	register pde_t	*pte;
	register pde_t	*pt;
	register pcom_t	*pcomp;
	register pde_t	**nsegtab = NULL;
	register int	segindex;
	int		s;


	ASSERT(pp->p3level_basetable);
	pcomp = pp->p3level_basetable + btobasetab(stob(segno));

	segindex = btosegtabindex(stob(segno));
	ASSERT(segindex < SEGTABSIZE);

	segptr = pcomp->pcom_segptr;
	if (segptr) {
		ppt = segptr + segindex;
		if (pt = *ppt)
			return (pt);
	}

	if ((pte = ptpool_alloc(pas, segno, vmflag, B_TRUE)) == NULL)
		return NULL;

	if (!segptr) {

		nsegtab = pmap_segtab_alloc(vmflag);

		if (!nsegtab) {
			/*
			 * pass zero as flag because the page is
			 * known to contain zeroes only (never used)
			 */
			ptpool_free(pte, 0);
			return NULL;
		}

		ppt = nsegtab + segindex;
	}


	/*	aspacelock(UPD) protects against deletions only,
	 *	so must use seglock here while we (probably)
	 *	only hold aspacelock(ACC).
	 */
	s = seglock();
	if (nsegtab) {
		if (pcomp->pcom_segptr) {
			if (segtable_zone)
				kmem_zone_free(segtable_zone, nsegtab);
			else
				kmem_free(nsegtab, SEGTABSIZE*sizeof(pde_t *));
			ppt = pcomp->pcom_segptr + segindex;
		} else {
			ASSERT(pp->p3level_segtabcnt >= 0);
			pcomp->pcom_segptr = nsegtab;
			pp->p3level_segtabcnt++;
		}
	}

	if (pt = *ppt) { /* If a page table also has been allocated free ours */
		segunlock(s);
		/*
		 * pass zero as flag because the page is
		 * known to contain zeroes only (never used)
		 */
		ptpool_free(pte, 0);
		return (pt);
	}

	*ppt = pte;
	ASSERT(pcomp->pcom_segcnt >= 0);
	pcomp->pcom_segcnt++;
	segunlock(s);

	return (pte);
}

static pde_t *
trilevel_probe(
	register p3level_t *pp,
	register int *firstseg,
	register int lastseg)
{
	register pde_t **ppt;
	register pde_t *pte;
	register int segno;
	pcom_t *pcomp;
	int basetabindex;
	int basetabend;
	register int startseg;
	register int endseg;

	ASSERT(pp->p3level_basetable);

	segno = *firstseg;
	basetabindex = btobasetab(stob(segno));
	basetabend = btobasetab(stob(lastseg));

	for ( ; basetabindex <= basetabend; basetabindex++) {
		pcomp = pp->p3level_basetable + basetabindex;
		if ((ppt = pcomp->pcom_segptr) == NULL) {
			segno += SEGTABSIZE - segtosegtabindex(segno);
			continue;
		}

		if (btobasetab(stob(lastseg)) == basetabindex)
			endseg = segtosegtabindex(lastseg);
		else
			endseg = SEGTABSIZE - 1;

		for (startseg = segtosegtabindex(segno);
		     startseg <= endseg;
		     startseg++, segno++) {
			if (pte = *(ppt + startseg)) {
				*firstseg = segno;
				return pte;
			}
		}
	}

	return NULL;
}

static int
trilevel_free(
	register pas_t	*pas,
	register p3level_t *pp,
	char *vaddr,
	register pgno_t	pgsz,
	register int free)
{
	register pgno_t pg = btoct(vaddr);
	register pde_t *pte;
	register pde_t **ppt;
	register int segoff, sz;
	register int stoff;
	pcom_t *pcomp;
	int ret = 0;

	ASSERT(pgsz == 0 || btobasetab(vaddr) < BASETABSIZE);
	pcomp = pp->p3level_basetable + btobasetab(vaddr);

	while (pgsz > 0) {
		if (pcomp->pcom_segptr == NULL) {
			stoff = NPGSEGTAB - ctosegpg(pg);
			pgsz -= stoff;
			pg += stoff;
			pcomp++;
			continue;
		}

		ppt = pcomp->pcom_segptr + ctosegtabindex(pg);

		while (pgsz > 0) {
			segoff = ptoff(pg);
			if (segoff + pgsz > NPGPT)
				sz = NPGPT - segoff;
			else
				sz = pgsz;

			pgsz -= sz;
			pg += sz;

			if (pte = *ppt) {
				pte += segoff;
				ret += segcommon_free(pas, pte, sz, free);
			}

			if (ctosegpg(pg) == 0)
				/* Just crossed a segment table boundary */
				break;

			ppt++;
		}
		pcomp++;
	}

	return ret;
}

static int
trilevel_trim(p3level_t *pp)
{
	register pcom_t *pcomp;
	register int trimmed = 0;
	register int i;
	register int tabcnt = pp->p3level_segtabcnt;

	ASSERT(tabcnt >=0);

	if (!tabcnt)
		return 0;

	if (pp->p3level_flags & PMAPFLAG_NOTRIM)
		return 0;

	pcomp = pp->p3level_basetable;
	ASSERT(pcomp);

	for (i = BASETABSIZE; --i >= 0; pcomp++)
		if (pcomp->pcom_segptr) {
			if (segcommon_trim(pcomp, SEGTABSIZE)) {
				trimmed = 1;
				if (--tabcnt == 0)
					break;
			}
		}
	return trimmed;
}
#endif	/* _MIPS3_ADDRSPACE */

/* common routines for PMAP_SEGMENT and PMAP_TRILEVEL pmap types. */

static int
segcommon_trim(
	pcom_t *pp,
	int tabsize)
{
	register pde_t	**ppt;
	register pde_t	*pte;
	register int	i, j;
	register int	trimmed = 0;

	if (!pp->pcom_segcnt)
		return 0;

	ppt = pp->pcom_segptr;

	for (i = tabsize; --i >= 0; ppt++) {
		pte = *ppt;
		if (pte == NULL)
			continue;

		for (j = NPGPT; --j >= 0; pte++)
			if (pte->pgi)
				goto next;

		/*	No need for seglock here, 'cause we
		 *	should have aspacelock held for update.
		 */
		pte = *ppt;
		*(ppt) = NULL;
		/*
		 * pass zero as flag because the page is
		 * known to contain zeroes only (page checked above)
		 */
		ptpool_free(pte, 0);
		trimmed = 1;
		if (--pp->pcom_segcnt == 0)
			break;
	next:
		;
	}
	return trimmed;
}

static void
segcommon_destroy(
	register pcom_t *pp,
	register int tabsize,
	register int flags)
{
	register int	sc = pp->pcom_segcnt;
	register pde_t	**ppt = pp->pcom_segptr;
	register pde_t	*pte;
	register int	i;

	ASSERT(pp->pcom_segptr);
	ASSERT(sc >= 0);
	if (sc)
		for (i = tabsize; --i >= 0; ppt++) {
			if (pte = *ppt) {
				ptpool_free(pte, flags);
#ifdef PMAPDEBUG
				*ppt = NULL;
#endif
				if (--sc == 0)
					break;
			}
		}

#ifdef PMAPDEBUG
	ppt = pp->pcom_segptr;
	for (i = tabsize; --i >= 0; ppt++) {
		if (*ppt)
			cmn_err(CE_PANIC, "segcommon_destroy: pp %x seg", pp);
	}
#endif
}

#define	MAX_PAGES_FREED	10

/*	flags:	PMAPFREE_TOSS	remove mapping only (no page free)
 *		PMAPFREE_FREE	free page
 *		PMAPFREE_SANON	free shared anon page
 *		PMAPFREE_UNRES	unreserve mapping rights
 */

static int
segcommon_free(
	register pas_t	*pas,
	register pde_t *pte,
	register int sz,
	register int free)
{
	register int ret = 0;
	pfd_t	*pfd, *lpfd;
	int	i;
	pgno_t	npgs;

	if (free == PMAPFREE_FREE) {
		while (sz > 0) {
			/* Got to hold the lock before checking if it's
			 * valid or shot down. 
			 */
			npgs = PAGE_MASK_INDEX_TO_CLICKS(pg_get_page_mask_index(pte));
			sz -= npgs;
			lpfd = NULL;
			for (i = 0; i < npgs ; pte++, i++) {
				pg_pfnacquire(pte);
				if (pg_isvalid(pte) || pg_ismigrating(pte)) {
					pfd = pdetopfdat(pte);
					if (i == 0)
						lpfd = pfd;
					VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pas), JOBRSS_DECREASE, pfd, pte);
					ret++;
				} else if (pg_isshotdn(pte)) {
					pg_clrshotdn(pte);
					ret++;
				}
				pg_pfnrelease(pte);
				pg_clrpgi(pte);
			}
			if (lpfd)
				pagefreeanon_size(lpfd, npgs);
		}
	} else if (free == PMAPFREE_SANON) {
		for ( ; --sz >= 0; pte++) {
			/* Got to hold the lock before checking if it's
			 * valid or shot down. 
			 */
			pg_pfnacquire(pte);
			if (pg_isvalid(pte) || pg_ismigrating(pte)) {
				pfd = pdetopfdat(pte);
				VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pas), JOBRSS_DECREASE, pfd, pte);
				pagefreesanon(pfd, pg_isdirty(pte));
				ret++;
			} else if (pg_isshotdn(pte)) {
				pg_clrshotdn(pte);
				ret++;
			}
			pg_pfnrelease(pte);
			pg_clrpgi(pte);
		}
	} else {
		ASSERT(free == PMAPFREE_TOSS);
		for ( ; --sz >= 0; pte++) {
			if (pg_isvalid(pte)){
				ret++;
			} 
                        pg_clrpgi(pte);
		}
	}

	return ret;
}

#ifdef	PMAP_SEG_DEBUG

static int
_reservemem(pseg_t *pp,int s,int a,int b,int c,int not)
{
	if (!not) {
		if (!reservemem(GLOBAL_POOL, a,b,c)) {
			atomicAddInt(&pmapresv, b);
			ASSERT(btst(pp->pseg_bitmap, s) == 0);
			bset(pp->pseg_bitmap, s);
			return 0;
		}
		return 1;
	} else {
		atomicAddInt(&pp->pseg_resv, b);
		ASSERT(btst(pp->pseg_bitmap, s) == 0);
		bset(pp->pseg_bitmap, s);
	}
	return 0;
}

static void
_unreservemem(pseg_t *pp,int s,int a,int b,int c,int not)
{
	if (!not) {
		atomicAddInt(&pmapresv, -b);
		unreservemem(GLOBAL_POOL, a,b,c);
	}
	atomicAddInt(&pp->pseg_resv, -b);
	ASSERT(btst(pp->pseg_bitmap, s));
	bclr(pp->pseg_bitmap, s);
}
#elif DEBUG

/* ARGSUSED */
static int
_reservemem(pmap_t *pp,int s,int a,int b,int c,int not)
{
	if (reservemem(GLOBAL_POOL, a,b,c))
		return 1;
	atomicAddInt(&pmapresv, b);
	atomicAddInt(&pp->pmap_resv, b);
	return 0;
}

/* ARGSUSED */
static void
_unreservemem(pmap_t *pp,int s,int a,int b,int c,int not)
{
	atomicAddInt(&pmapresv, -b);
	atomicAddInt(&pp->pmap_resv, -b);
	ASSERT(pp->pmap_resv >= 0);
	ASSERT(pmapresv >= 0);
	unreservemem(GLOBAL_POOL, a,b,c);
}
#else	/* !PMAP_SEG_DEBUG */
#define _reservemem(p,s,a,b,c,n) reservemem(GLOBAL_POOL, a,b,c)
#define _unreservemem(p,s,a,b,c,n) unreservemem(GLOBAL_POOL, a,b,c)
#endif

/*
 * availsmem is pre-reserved (sort of like queing in line to make
 * reservations to actually _do_ something someday) so that pmap
 * module can't ever fail for want of it (availsmem) -- like at
 * tfault time.
 * We must look for other pregions that overlap our segment. We need to
 * look at all pregions that share our pmap.
 */
/* ARGSUSED */
static int
segcommon_reserve(
	pmap_t *pmap,
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t vaddr,
	pgcnt_t npgs,
	int not)
{
	register pgcnt_t	segno     = btost(vaddr);
	register uvaddr_t	end       = vaddr + ctob(npgs);
	register pgcnt_t	len	  = npgs;
	preg_t *prp1, *prp2;

	ASSERT(len >= 0);

	if (len == 0)
		return 0;
	if (ptoff(btoct(vaddr))) {
		prp1 = PREG_FINDANYRANGE(&pas->pas_pregions, stob(segno),
				(__psunsigned_t) vaddr, PREG_EXCLUDE_ZEROLEN);
		prp2 = PREG_FINDANYRANGE(&ppas->ppas_pregions, stob(segno),
				(__psunsigned_t) vaddr, PREG_EXCLUDE_ZEROLEN);
		if ((prp1 == NULL || prp1->p_pmap != pmap) &&
		    (prp2 == NULL || prp2->p_pmap != pmap)) {
			if ((__psunsigned_t) end < stob(segno+1)) {
				/*
				 * check to see if any regions exist from
				 * end till end of segno. If so, we don't have
				 * to reserve.
				 */
				prp1 = PREG_FINDANYRANGE(&pas->pas_pregions,
						(__psunsigned_t)end,
						stob(segno+1),
						PREG_EXCLUDE_ZEROLEN);
				prp2 = PREG_FINDANYRANGE(&ppas->ppas_pregions,
						(__psunsigned_t)end,
						stob(segno+1),
						PREG_EXCLUDE_ZEROLEN);
				if ((prp1 && prp1->p_pmap == pmap) ||
				    (prp2 && prp2->p_pmap == pmap))
						return 0;
			}

			/*
			 * Either end overflows into a subsequent segment,
			 * or no region in [end, stob(segno+1).
			 */
			if (_reservemem(pmap, segno, 1, 1, 1, not)) {
				segcommon_unreserve(pmap, pas, ppas, vaddr,
							npgs - len, not);
				return 1;
			}
		}
		len -= (NPGPT - ptoff(btoct(vaddr)));
		segno++;
	}
			
	for ( ; (__psunsigned_t) end >= (__psunsigned_t) stob(segno+1); segno++)
	{
#ifdef DEBUG
		prp1 = PREG_FINDANYRANGE(&pas->pas_pregions, stob(segno),
			stob(segno+1), PREG_EXCLUDE_ZEROLEN);
		ASSERT(prp1 == NULL || prp1->p_pmap != pmap);
		prp1 = PREG_FINDANYRANGE(&ppas->ppas_pregions, stob(segno),
			stob(segno+1), PREG_EXCLUDE_ZEROLEN);
		ASSERT(prp1 == NULL || prp1->p_pmap != pmap);
#endif
		if (_reservemem(pmap, segno, 1, 1, 1, not)) {
			segcommon_unreserve(pmap, pas, ppas, vaddr,
							npgs - len, not);
			return 1;
		}
		len -= NPGPT;
	}
	if (len > 0) {
		/*
		 * The last segment in range [vaddr, end).
		 * here is the case when range ends inside of
		 * the segment; see if any pregions of non-zero
		 * length are mapped in the segment not mapped by 
		 * the given range.
		 */
		ASSERT((__psunsigned_t) end < (__psunsigned_t) stob(segno+1));
		prp1 = PREG_FINDANYRANGE(&pas->pas_pregions,
				(__psunsigned_t) end, stob(segno+1),
				PREG_EXCLUDE_ZEROLEN);
		prp2 = PREG_FINDANYRANGE(&ppas->ppas_pregions,
				(__psunsigned_t) end, stob(segno+1),
				PREG_EXCLUDE_ZEROLEN);
		if ((prp1 == NULL || prp1->p_pmap != pmap) &&
		    (prp2 == NULL || prp2->p_pmap != pmap)) {
			if (_reservemem(pmap, segno, 1, 1, 1, not)) {
				segcommon_unreserve(pmap, pas, ppas, vaddr,
							npgs - len, not);
				return 1;
			}
		}
	}
	return 0;
}

/* ARGSUSED */
static void
segcommon_unreserve(
	pmap_t *pmap,
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t vaddr,
	pgcnt_t npgs,
	int not)
{
	register pgcnt_t	segno = btost(vaddr);
	register uvaddr_t	end = vaddr + ctob(npgs);
	preg_t *prp1, *prp2;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	if (npgs == 0)
		return;

#ifdef DEBUG
	if (pmap->pmap_type == PMAP_SEGMENT)
		ASSERT((__psunsigned_t)vaddr < HIUSRATTACH_32);
#ifdef _MIPS3_ADDRSPACE
	else if (pmap->pmap_type == PMAP_TRILEVEL)
		ASSERT((__psunsigned_t)vaddr < HIUSRATTACH_64);
#endif
#endif

	if (ptoff(btoct(vaddr))) {
		/*
		 * The first segment in range [vaddr, end).
		 * See if any mapping exists in the beginning of
		 * this segment, i.e.  range [stob(segno), vaddr)
		 */
		prp1 = PREG_FINDANYRANGE(&pas->pas_pregions, stob(segno),
				(__psunsigned_t) vaddr, PREG_EXCLUDE_ZEROLEN);
		prp2 = PREG_FINDANYRANGE(&ppas->ppas_pregions, stob(segno),
				(__psunsigned_t) vaddr, PREG_EXCLUDE_ZEROLEN);
		if ((prp1 == NULL || prp1->p_pmap != pmap) &&
		    (prp2 == NULL || prp2->p_pmap != pmap)) {

			if ((__psunsigned_t) end < stob(segno+1)) {
				/*
				 * check to see if any regions exist in
				 * [end, stob(segno+1)).
				 * If so, we can't unreserve.
				 */
				prp1 = PREG_FINDANYRANGE(&pas->pas_pregions,
						(__psunsigned_t)end,
						stob(segno+1),
						PREG_EXCLUDE_ZEROLEN);
				prp2 = PREG_FINDANYRANGE(&ppas->ppas_pregions,
						(__psunsigned_t)end,
						stob(segno+1),
						PREG_EXCLUDE_ZEROLEN);
				if ((prp1 && prp1->p_pmap == pmap) ||
				    (prp2 && prp2->p_pmap == pmap))
						return;
			}
			/*
			 * Either end overflows into a subsequent segment,
			 * or no region in [end, stob(segno+1).
			 */
			_unreservemem(pmap, segno, 1, 1, 1, not);
		}

		npgs -= (NPGPT - ptoff(btoct(vaddr)));
		segno += 1;
	}

	for ( ; (__psunsigned_t) end >= (__psunsigned_t) stob(segno+1); segno++)
	{
		/* the given region [vaddr, end) maps the
		 * whole of the segment, segno. Trust the
		 * caller, and un-reserve everything; inuse is zero.
		 */
#ifdef DEBUG
		{
		preg_t 		*tprp;
		__psunsigned_t 	tstart, tend;

		tprp = PREG_FINDANYRANGE(&pas->pas_pregions,
				stob(segno), stob(segno+1),
				PREG_EXCLUDE_ZEROLEN);
		if (tprp == NULL || tprp->p_pmap != pmap)
			tprp = PREG_FINDANYRANGE(&ppas->ppas_pregions,
				stob(segno), stob(segno+1),
				PREG_EXCLUDE_ZEROLEN);
		if (tprp && tprp->p_pmap == pmap) {
			tstart = (__psunsigned_t) tprp->p_regva;
			tend = (__psunsigned_t) (tprp->p_regva + ctob(tprp->p_pglen));
			ASSERT(tstart <= stob(segno) &&
				stob(segno+1) <= tend);
		}
		}
#endif
		_unreservemem(pmap, segno, 1, 1, 1, not);
		npgs -= NPGPT;
	}
	if (npgs > 0) {
		/*
		 * The last segment in range [vaddr, end).
		 * here is the case when range ends inside of
		 * the segment; see if any pregions of non-zero
		 * length are mapped in the segment not mapped by 
		 * the given range.
		 */
		ASSERT((__psunsigned_t) end < (__psunsigned_t) stob(segno+1));
#ifdef DEBUG
		{
		preg_t *tprp;
		__psunsigned_t 	tstart, tend;

		tprp = PREG_FINDANYRANGE(&pas->pas_pregions, stob(segno),
				(__psunsigned_t) end, PREG_EXCLUDE_ZEROLEN);
		if (tprp == NULL || tprp->p_pmap != pmap)
			tprp = PREG_FINDANYRANGE(&ppas->ppas_pregions,
					stob(segno), (__psunsigned_t) end,
					PREG_EXCLUDE_ZEROLEN);
		if (tprp && tprp->p_pmap == pmap) {
			tstart = (__psunsigned_t) tprp->p_regva;
			tend = (__psunsigned_t) (tprp->p_regva + ctob(tprp->p_pglen));
			ASSERT(tstart <= stob(segno) &&
				(__psunsigned_t) end <= tend);
		}
		}
#endif

		prp1 = PREG_FINDANYRANGE(&pas->pas_pregions,
				(__psunsigned_t) end, stob(segno+1),
				PREG_EXCLUDE_ZEROLEN);
		prp2 = PREG_FINDANYRANGE(&ppas->ppas_pregions,
				(__psunsigned_t) end, stob(segno+1),
				PREG_EXCLUDE_ZEROLEN);
		if ((prp1 == NULL || prp1->p_pmap != pmap) &&
		    (prp2 == NULL || prp2->p_pmap != pmap))
			_unreservemem(pmap, segno, 1, 1, 1, not);
	}
}

/*
 * perform pmap magic on sproc.
 * Currently, before an sproc there are (non-overlapping) pregions
 * on both the shared and private list. They both share pas_pmap.
 * On sproc we allocate a new pmap for the private pregions and
 * use the current pas_pmap for the 'shared' ones. We also mark all
 * shared pregions as PF_SHARED.
 */
static int
segcommon_split(pas_t *pas, ppas_t *ppas)
{
	register preg_t *prp;
	register int	segno, ssegno, esegno;
	register int	segoff;
	register int	lowseg = -1;
	register pgno_t	npgs;
	int		availremove = 0;
	int		availalloc = 0;

	/*
	 * First calculate how many pages of page tables will no longer
	 * be needed in the current address chain.
	 * This consists of PF_PRIVATE regions that will be moving to
	 * the private pmap (ppas->ppas_map); Of course we only get to get
	 * rid of the page table if the PF_PRIVATE region (or regions)
	 * are the only ones in the segment.
	 * Since there are 2 pregion lists (each sorted by address)
	 * we have a bad algorithm here - but hopefully there are
	 * few (if any) private pregions at this point
	 */
	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp)) {
		if (prp->p_pglen == 0)
			continue;
		
		/*
		 * for each segment in prp see if there are any pregions
		 * on shared list that share the segment
		 */
		ssegno = btost(prp->p_regva);
		esegno = btost(prp->p_regva + ctob(prp->p_pglen) - 1);
		for (segno = ssegno; segno <= esegno; segno++) {
			if (PREG_FINDANYRANGE(&pas->pas_pregions,
					stob(segno),
					stob(segno + 1),
					PREG_EXCLUDE_ZEROLEN) == NULL) {
				/* no (non-zero) pregions in shared list
				 * so we'll reclaim a page table page
				 * as long as a previous private region
				 * didn't already reclaim it
				 */
				if (segno > lowseg) {
					availremove++;
					lowseg = segno;
				}
			}
		}
	}

	/*
	 * Now calculate how many pages will be needed in the new chain.
	 * (i.e. the private pregions)
	 */
	lowseg = -1;
	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp)) {
		ASSERT(prp->p_flags & PF_PRIVATE);

		if (prp->p_pglen == 0)
			continue;

		segno = btost(prp->p_regva);
		segoff = ptoff(btoct(prp->p_regva));
		npgs = prp->p_pglen;

		while (npgs > 0) {
			if (segno != lowseg) {
				availalloc++;
			}

			npgs -= (NPGPT - segoff);
			segoff = 0;
			lowseg = segno++;
		}
	}

	/*
	 * Now, the point of all the checking above.  After figuring out
	 * the net difference in page tables needed, alloc the difference.
	 * If reservations are refused, we can return will error status
	 * without anything to clean up.
	 */
	npgs = availalloc - availremove;
	ASSERT(npgs >= 0);

	if (npgs && reservemem(GLOBAL_POOL, npgs, npgs, npgs))
		return ENOMEM;

	/*
	 * Allocate a new pmap for the private pregions
	 */
	ASSERT(ppas->ppas_pmap == NULL);
	ppas->ppas_pmap = pmap_create(pas->pas_flags & PAS_64, PMAP_SEGMENT);

#if DEBUG
	atomicAddInt(&pmapresv, npgs);
	atomicAddInt(&pas->pas_pmap->pmap_resv, -availremove);
	ASSERT(pas->pas_pmap->pmap_resv >= 0);
	atomicAddInt(&ppas->ppas_pmap->pmap_resv, availalloc);
#endif
	/*
	 * Even if there wasn't any availalloc, must still go through
	 * the motions because there _might_ be some zero-length pregions
	 * that need to be transferred.
	 */
	mutex_lock(&pas->pas_preglock, 0);

	/*
	 * go through all shared pregions and mark them as SHARED
	 */
	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp)) {
		ASSERT(!(prp->p_flags & PF_PRIVATE));
		prp->p_flags |= PF_SHARED;
	}
	/*
	 * go through all private pregions and perform pmap madness
	 */
	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp)) {
		register pde_t	*old_pt, *new_pt;
		caddr_t		vaddr;
		pgno_t		npgs, cnt, cpy_cnt;

#ifdef PMAP_SEG_DEBUG
		segcommon_unreserve(pas->pas_pmap, prp->p_regva, prp->p_pglen, 1);

		segcommon_reserve(ppas->ppas_pmap, prp->p_regva, prp->p_pglen, 1);
#endif
		prp->p_pmap = ppas->ppas_pmap;

		/* release pregion lock */
		mutex_unlock(&pas->pas_preglock);

		/*
		 * This is a private region.  Must move any pmap
		 * entries that were present in the pas_pmap
		 * (which is now the shared pmap) to the ppas_pmap
		 * (the new private pmap).
		 */

		/* lock region to protect against vhand */
		reglock(prp->p_reg);

		vaddr = prp->p_regva;
		npgs = prp->p_pglen;

		pmap_lclsetup(pas, ppas->ppas_utas, prp->p_pmap, vaddr, npgs);

		while (npgs) {
			old_pt = pmap_probe((pmap_t *)pas->pas_pmap,
							&vaddr, &npgs, &cnt);
			if (npgs == 0)
				break;

			ASSERT(old_pt);
			npgs -= cnt;

			while (cnt) {
				cpy_cnt = cnt;
				new_pt = pmap_ptes(pas, ppas->ppas_pmap,
							vaddr, &cpy_cnt, 0);

				ASSERT(cpy_cnt);

				vaddr += (cpy_cnt << BPCSHIFT);
				cnt -= cpy_cnt;

				while (cpy_cnt) {
					/*
					 * XXX Any way to optimize this loop ?
					 */
                                        pg_pfnacquire(old_pt);
                                        pg_pfnacquire(new_pt);
                                        pg_setpgi(new_pt, pg_getpgi(old_pt));

                                        if (pg_getpfn(new_pt)) {
                                                /* Only if pfn is valid*/
						if (!(prp->p_reg->r_flags & RG_PHYS))
                                                	rmap_swapmap(pdetopfdat(new_pt),
                                                             old_pt, new_pt);
                                        }
                                        
					/* pg_setpgi to 0 also has the
					 * side effect of resetting the lock
					 * bit. So it got moved from being
					 * done before rmap_swap
					 * to after rmap_swap
					 */
                                        pg_clrpgi(old_pt);
                                        pg_pfnrelease(new_pt);
                                        pg_pfnrelease(old_pt);
                                        
					new_pt++;
					old_pt++;
					cpy_cnt--;
				}
			}
		}
		regrele(prp->p_reg);
		mutex_lock(&pas->pas_preglock, 0);
	}
	mutex_unlock(&pas->pas_preglock);

	return 0;
}

/*	Set up the local pmap, pointed to by pp, for the new local
 *	mapping starting at pg for pgsz pages.  For segments which are
 *	not completely covered by the local mapping, which can only
 *	happen in the first and last segment of the mapping, we need
 *	to allocate a page of page table entries in the local pmap now.
 *	We fill in the SHRD_SENTINEL value in all entries for which a
 *	local mappings does *not* exist (i.e. not in the range pg through
 *	pg+pgsz).  We fill in zeros (page not valid) for the local pages
 *	for this mapping.  This way the process will vfault on the local
 *	mapping's pages while utlbmiss will see the SHRD_SENTINEL value
 *	for shared pages and know to check the second segment table.
 */
static void
segcommon_lclsetup(
	register pas_t	*pas,
	register utas_t	*utas,
	register pmap_t	*pp,
	register pgno_t	pg,
	register pgno_t	pgsz)
{
	register int	pgoff, size;
	/* REFERENCED */
	int s;

	if (pgsz == 0)
		return;

	pgoff = ptoff(pg);
	do {
		size = NPGPT - pgoff;
		if (size > pgsz)
			size = pgsz;
 		segcommon_setup(pas, pp, pg, size);
		pgsz -= size;
		pg += size;
		pgoff = 0;
	} while (pgsz > 0);

#if !R4000 && !R10000
	/*	On R4000
	 *	No longer need to set utas_utlbmissswtch to UTLBMISS_DEFER.
	 *	We wait until the first vfault where we detect an
	 *	overlapping segment (private & shared mapping in same segment)
	 *	before we use the more expensive utlbmiss.
	 */
	/*
	 *	Use the sentinel utlbmiss handler that
	 *	understands how to deal with SHRD_SENTINEL pdes.
	 */
	s = utas_lock(utas);
	if (!(utas->utas_utlbmissswtch & UTLBMISS_DEFER)) {
		/* We could always do the following, but common path
		 * already has UTLBMISS_DEFER setup (especially if we're
		 * sproc-ed from a process which has already sproc-ed
		 * once).  
		 */

		utas->utas_utlbmissswtch |= UTLBMISS_DEFER;
		utas->utas_flag |= UTAS_TLBMISS;
		utas_unlock(utas, s);
		utlbmiss_resume(utas);
	} else
		utas_unlock(utas, s);
#endif

	/*
	 *	And _don't_ let vhand trim private segments!
	 *	Their existence signals that the underlying
	 *	shaddr segment must be mapped in high KPTESEG space.
	 */
	pp->pmap_flags |= PMAPFLAG_NOTRIM;

	/*
	 *	Must make sure that shared page tables that map
	 *	same addresses as local ones are no longer mapped
	 *	in KPTESEG, and that new local pages get referenced
	 *	instead of shared ones.
	 */
	if (utas == &curuthread->ut_as)
		setup_wired_tlb(1);
	else
		setup_wired_tlb_notme(utas, 1);
	new_tlbpid(utas, VM_TLBINVAL);
}

/*
 * Initialize a page of page tables in a shared address process's local
 * map.  The new local mapping starts at pg for pgsz pages which must be
 * completely contained within a single segment.
 */
static void
segcommon_setup(
	register pas_t	*pas,
	register pmap_t	*pp,
	register pgno_t	pg,
	register pgno_t	pgsz)
{
	register pde_t	*base_pte;
	register pde_t	*pte;
	register pde_t	*last_pte;
	int 		segno;
	int		type = pp->pmap_type;

	segno = ctost(pg);

	ASSERT(segno == ctost(pg+pgsz-1)); /* last pg must be in same seg */

	/*
	 * See if a page of page table entries has already been
	 * allocated for this segment.  If not, allocate one and fill
	 * it with the SHRD_SENTINEL for all page entries not mapped
	 * by [pg, pg+pgsz].  If the page of ptes already exists, then
	 * there is (or was) another local mapping in this segment.
	 * This means that the page has already been initialized
	 * with the sentinel value in all pages that didn't currently
	 * have local mappings.
	 */
	switch (type) {
	case PMAP_SEGMENT:
		base_pte = seg_probe((pseg_t *)pp, &segno, segno);
		break;
#ifdef _MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		base_pte = trilevel_probe((p3level_t *)pp, &segno, segno);
		break;
#endif
	default:
		cmn_err_tag(88,CE_PANIC, "segcommon_setup: bad pmap_type 0x%x", type);
	}

	if (base_pte == NULL) {

		/* seg_pte, seg2level_pte return zeroed page */
		switch (type) {
		case PMAP_SEGMENT:
			base_pte = seg_pte(pas, (pseg_t *)pp, segno, 0);
			break;
#ifdef _MIPS3_ADDRSPACE
		case PMAP_TRILEVEL:
			base_pte = trilevel_pte(pas, (p3level_t *)pp, segno, 0);
			break;
#endif
		default:
			cmn_err(CE_PANIC,
				"segcommon_setup: bad pmap type 0x%x", type);
		}

		last_pte = base_pte + ptoff(pg);
		for (pte = base_pte; pte < last_pte; pte++)
			pg_setpgi(pte, SHRD_SENTINEL);

		last_pte = base_pte + NPGPT;
		base_pte += ptoff(pg) + pgsz;
		for (pte = base_pte; pte < last_pte; pte++)
			pg_setpgi(pte, SHRD_SENTINEL);

#ifdef PMAPDEBUG
	{
		int sz = pgsz;
		switch (type) {
		case PMAP_SEGMENT:
			base_pte = seg_pte(pas, (pmap_t *)pp, segno, 0) + ptoff(pg);
			break;
#ifdef _MIPS3_ADDRSPACE
		case PMAP_TRILEVEL:
			base_pte =
			trilevel_pte(pas, (p3level_t *)pp, segno, 0) + ptoff(pg);
			break;
#endif
		default:
			cmn_err(CE_PANIC,
				"segcommon_setup: bad pmap type 0x%x", type);
		}
		while (--sz >= 0)
			ASSERT(base_pte++->pgi == 0);
	}
#endif
		return;
	}

	/*
	 * Else zero out all pmap entries in this segment corresponding
	 * to the new local mapping.  This way we'll get vfaults on the
	 * local pages and know to look at the shared pmap for all others.
	 * N.B.: Caller can't have any important info stashed here!
	 */

	pte = base_pte + ptoff(pg);
	last_pte = pte + pgsz;

	for ( ; pte < last_pte; pte++) {
		/*
		 * Strange case -- sproc process with private mappings
		 * could have forked, which would create the page table
		 * segment (base_pte above) via dupreg/pmap_copy.
		 * Now, it creates a private mapping in the same segment
		 * (sets a breakpoint, etc.) and, voila, a private
		 * page entry not valid and not marked SHRD_SENTINEL.
		 */
		ASSERT(pte->pgi == SHRD_SENTINEL || pte->pgi == 0);
		pg_clrpgi(pte);
	}
}

#if _MIPS_SIM != _ABI64
#if R4000
static char *segtbl_vmmap;
static char *shrdsegtbl_vmmap;
#endif /* R4000 */

/* NOT NEEDED IN 64-bit KERNELS WHERE ALL MEMORY IS DIRECTLY ADDRESSABLE */
/*
 * wiresegtbl makes sure that the locore exception handler for segment faults
 * can always address the segment table.  On small memory systems (< 256 MB)
 * the memory allocator returns K0 addresses for page maps.  But on large
 * memory systems compiled in 32-bit mode, you will get a K2 address when the
 * page is above 256 MB.  If you have a K2 address, this routine steals the
 * first one or two  wired tlb entries in order to translate the segment table
 * address.
 *
 * utas_segtbl -- contains the segment table address allocated by the kernel
 * utas_shrdsegtbl -- contains the segment table address of the shared segment
 *		   table, but only when process is both private and shared 
 * utas_segtbl_wired -- upon exit contains a wired address (if utas_segtbl is
 *                   a K0 address, then utas_segtbl_wired == utas_segtbl)
 * utas_shrdsegtbl_wired -- upon exit contains a wired address
 *		     (if utas_shrdsegtbl is a K0 address,
 *		     then utas_shrdsegtbl_wired == utas_shrdsegtbl)
 */
static void
wiresegtbl(void)
{
	uthread_t *ut = curuthread;
#if R4000
#if TLBKSLOTS == 1
	int	tlbidx = UKSTKTLBINDEX-TLBWIREDBASE;
#else
	int	tlbidx = TLBWIREDBASE;
#endif
	pde_t	pde;

	if (ut->ut_as.utas_segtbl && (!IS_KSEGDM(ut->ut_as.utas_segtbl))) {
		/* The segtbl_vmmap is permanently allocated for all cpus
		 * and contains one virtual page for each cache color in
		 * order to avoid VCEs when handling segment faults in
		 * the locore handler.
		 * Processes with segment tables which are not directly
		 * addressable will use one address to privately map the
		 * segment table in the first wired tlb entry.
		 */
		if (!segtbl_vmmap) {
#ifdef MH_R10000_SPECULATION_WAR
                        if (IS_R10000()) {
                                segtbl_vmmap = kvalloc(3,VM_VACOLOR,0);
                                if (((__psint_t) segtbl_vmmap) & NBPP)
                                        segtbl_vmmap += NBPP;
                        } else
#endif /* MH_R10000_SPECULATION_WAR */
			segtbl_vmmap = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
			ASSERT(segtbl_vmmap);
		}
		ut->ut_as.utas_segtbl_wired =
			 (pde_t **)(segtbl_vmmap +
					NBPP*colorof(ut->ut_as.utas_segtbl) +
					poff(ut->ut_as.utas_segtbl));

		/* Now setup the wired tlb mapping for the segment table */
		pde.pgi = mkpde(PG_VR|PG_M|pte_cachebits(),
					kvtophyspnum(ut->ut_as.utas_segtbl));

#if !NO_WIRED_SEGMENTS
		if ((__psint_t)ut->ut_as.utas_segtbl_wired & NBPP) { /* odd page */
			ut->ut_exception->u_ubptbl[tlbidx].pde_low.pgi = 0;
			ut->ut_exception->u_ubptbl[tlbidx].pde_hi.pgi =pde.pgi;
		} else {	/* even page */
			ut->ut_exception->u_ubptbl[tlbidx].pde_low.pgi=pde.pgi;
			ut->ut_exception->u_ubptbl[tlbidx].pde_hi.pgi = 0;
		}
		ut->ut_exception->u_tlbhi_tbl[tlbidx] =
			(caddr_t)((__psint_t)ut->ut_as.utas_segtbl_wired & TLBHI_VPNMASK);

#if TLBKSLOTS == 0
		/* if TLBKSLOTS == 0 then return from kernel may not
		 * restore anything!
		 */
		{
		int stlbpid = splhi();

		tlbwired(TLBWIREDBASE + tlbidx,
			 ut->ut_as.utas_tlbpid,
			 ut->ut_exception->u_tlbhi_tbl[tlbidx],
			 ut->ut_exception->u_ubptbl[tlbidx].pde_low.pte,
			 ut->ut_exception->u_ubptbl[tlbidx].pde_hi.pte,
			 TLBPGMASK_MASK);
		splx(stlbpid);
		}
#endif
		tlbidx++;	/* shrdsegtbl may need next tlb */
#endif	/* !NO_WIRED_SEGMENTS */
	
		/* Since we're in kernel mode, this wired entry is restored
		 * on exit from the kernel
		 */
	} else
		ut->ut_as.utas_segtbl_wired = ut->ut_as.utas_segtbl;


	if (ut->ut_as.utas_shrdsegtbl &&
				(!IS_KSEGDM(ut->ut_as.utas_shrdsegtbl))) {
		/* The segtbl_vmmap is permanently allocated for all cpus
		 * and contains one virtual page for each cache color in
		 * order to avoid VCEs when handling segment faults in
		 * the locore handler.
		 * Processes with segment tables which are not directly
		 * addressable will use one address to privately map the
		 * segment table in the first wired tlb entry.  We 
		 */
		if (!shrdsegtbl_vmmap) {
#ifdef MH_R10000_SPECULATION_WAR
                        if (IS_R10000()) {
                                shrdsegtbl_vmmap = kvalloc(3,VM_VACOLOR,0);
                                if (((__psint_t) shrdsegtbl_vmmap) & NBPP)
                                        shrdsegtbl_vmmap += NBPP;
                        } else
#endif /* MH_R10000_SPECULATION_WAR */
			shrdsegtbl_vmmap = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
			ASSERT(shrdsegtbl_vmmap);
		}
		ut->ut_as.utas_shrdsegtbl_wired =
			 (pde_t **)(shrdsegtbl_vmmap +
					NBPP*colorof(ut->ut_as.utas_shrdsegtbl)
					+ poff(ut->ut_as.utas_shrdsegtbl));

		/* Now setup the wired tlb mapping for the segment table */
		pde.pgi = mkpde(PG_VR|PG_M|pte_cachebits(),
				kvtophyspnum(ut->ut_as.utas_shrdsegtbl));

#if !NO_WIRED_SEGMENTS
		if ((__psint_t)ut->ut_as.utas_shrdsegtbl_wired & NBPP) { /* odd page */
			ut->ut_exception->u_ubptbl[tlbidx].pde_low.pgi = 0;
			ut->ut_exception->u_ubptbl[tlbidx].pde_hi.pgi= pde.pgi;
		} else {	/* even page */
			ut->ut_exception->u_ubptbl[tlbidx].pde_low.pgi=pde.pgi;
			ut->ut_exception->u_ubptbl[tlbidx].pde_hi.pgi = 0;
		}
		ut->ut_exception->u_tlbhi_tbl[tlbidx] =
			(caddr_t)((__psint_t)ut->ut_as.utas_shrdsegtbl_wired & TLBHI_VPNMASK);

#if TLBKSLOTS == 1
		if (tlbidx != UKSTKTLBINDEX-TLBWIREDBASE)
#endif
		{
			int stlbpid = splhi();

			tlbwired(TLBWIREDBASE + tlbidx,
				 ut->ut_as.utas_tlbpid,
				 ut->ut_exception->u_tlbhi_tbl[tlbidx],
				 ut->ut_exception->u_ubptbl[tlbidx].pde_low.pte,
				 ut->ut_exception->u_ubptbl[tlbidx].pde_hi.pte,
				 TLBPGMASK_MASK);
			splx(stlbpid);
		}
#endif	/* !NO_WIRED_SEGMENTS */
		/* Since we're in kernel mode, this wired entry is restored
		 * on exit from the kernel
		 */
	} else
		ut->ut_as.utas_shrdsegtbl_wired = ut->ut_as.utas_shrdsegtbl;

#else	/* !R4000 */
	/*  TFP(R8000)  and R10000 */
	if ((ut->ut_as.utas_segtbl && (!IS_KSEGDM(ut->ut_as.utas_segtbl))) ||
	    (ut->ut_as.utas_shrdsegtbl && (!IS_KSEGDM(ut->ut_as.utas_shrdsegtbl)))) {
		cmn_err(CE_PANIC, "Expected utas_segtbl address in KSEGDM\n");
	}
	ut->ut_as.utas_segtbl_wired = ut->ut_as.utas_segtbl;
	ut->ut_as.utas_shrdsegtbl_wired = ut->ut_as.utas_shrdsegtbl;
#endif	/* !R4000 */
}
#endif /* 64bit */

/*
 * 1.5-level segment handler routines
 * XXX when the new VM system came in, we started using siglock for
 * sprocs...why???
 */
void
initsegtbl(utas_t *utas, pas_t *pas, ppas_t *ppas)
{
	ASSERT(ppas->ppas_utas == utas);

#ifdef _MIPS3_ADDRSPACE
	utas->utas_segflags = (pas->pas_flags & PAS_64) ? PSEG_TRILEVEL : PSEG_SEG;
#else
	utas->utas_segflags = PSEG_SEG;
#endif

	/*	If process's AS is sharing and also has private
	 *	mappings, fill in the processor segptr list.
	 *	Order is shared, then overlay with private.
	 */
	if (pas->pas_flags & PAS_SHARED) {
		if (ppas->ppas_pmap) {
			/* both shared and private pmaps */
			utas->utas_segtbl = ((pseg_t *)ppas->ppas_pmap)->pseg_segptr;
			utas->utas_shrdsegtbl = ((pseg_t *)pas->pas_pmap)->pseg_segptr;
		} else {
			utas->utas_segtbl = ((pseg_t *)pas->pas_pmap)->pseg_segptr;
			utas->utas_shrdsegtbl = 0;
		}
		/* Make sure locore can access - no fault */
		WIRESEGTBL();
	} else {
		utas->utas_segtbl = ((pseg_t *)pas->pas_pmap)->pseg_segptr;
		utas->utas_shrdsegtbl = 0;
		/* Make sure locore can access - no fault */
		WIRESEGTBL();
	}
}

void
zapsegtbl(utas_t *utas)
{
#ifdef ULI
	/* ULI processes are always in seg table mode */
	if (utas->utas_flag & UTAS_ULI)
		return;
#endif
	/* XXX used to lock siglck - why ?? */
	utas->utas_segflags = 0;
}


#ifdef	PTPOOL_DEBUG
extern	void	catchdebug(void);
void	ptpool_log_op(void *, int, int);
#define	PTPOOL_LOG_OP(a, b, c)		ptpool_log_op((a), (b), (c))
#else	/* PTPOOL_DEBUG */
#define	PTPOOL_LOG_OP(a, b, c)
#endif	/* PTPOOL_DEBUG */

static	uint_t		ptpool_ncolor		= 0;
static	ptpool_t	*ptpool			= NULL;
static	boolean_t	ptpool_active		= B_FALSE;
ptpool_stat_t		ptpool_stat;
int			ptpool_vm;	/* # of pt pages K2SEG mapped */

extern	pfn_t		physmem;

void
ptpool_init(void)
{
	ptpool_t	*ptp, *eptp;
	uint_t		maxentries;


	/*
	 * Maximum number of entries per color.
	 * Depends on the size of the physical memory and the number
	 * of colors. If memory is less the 32 Mbyte bypass the pool.
	 * Otherwise the maximum total memory should in the pool
	 * should not exceed 1/1024 of the total memory in the system
	 * with a maximum of 4 entries per color.
	 */
	if (physmem < btoct(32*1024*1024)) {
		ptpool_active = B_FALSE;
		return;
	}

	ptpool_active = B_TRUE;
	ptpool_ncolor = CACHECOLORSIZE;
	maxentries = MIN(((physmem / 1024) / CACHECOLORSIZE), 4);

#if NBPP == 16384
	/*
	 * For systems with 16k page size allow minimum of 2 entries.
	 */
	maxentries = MAX(2, maxentries);
#endif	/* NBPP == 16384 */


	ptpool = kmem_alloc(ptpool_ncolor * sizeof(ptpool_t), 0);
	for (ptp = ptpool, eptp = ptpool + ptpool_ncolor; ptp < eptp; ptp++) {
		ptp->head = NULL;
		ptp->cnt = 0;
		ptp->maxcnt = maxentries;
		spinlock_init(&ptp->lock, "ptpool");
	}

	shake_register(SHAKEMGR_MEMORY, ptpool_shake);

	/*
	 * No need here to register for the VM shakemgr since the only
	 * requestor of colored VM is the page table allocator itself.
	 * Therefore the page table pool would be a candidate for shake
	 * only if the pool had been empty to begin with.
	 */
}

/* ARGSUSED */
static void *
ptpool_alloc(pas_t *pas, register uint_t segno, int vmflag, boolean_t frompool)
{
	ptpool_t	*ptp;
	void		*pt;
	uint_t		color;
	uint_t		flags;
	uint_t		s;

#ifdef	R4000
	color = segno & cachecolormask;
#else	/* R4000 */
	color = 0;
#endif	/* R4000 */

	if (!ptpool_active || !frompool)
		goto skip;

	ptp = ptpool + color; 

	s = mutex_spinlock(&ptp->lock);
	if (ptp->cnt != 0) {
		pt = ptp->head;
		ptp->head = *((void **)pt);
		PTPOOL_LOG_OP(pt, color, 2);
		ptp->cnt--;
		ptpool_stat.hit++;
		mutex_spinunlock(&ptp->lock, s);
		*((void **)pt) = NULL;
		atomicAddInt(&pmapmem, 1);
		LINK_PT_TO_ASJOB(pt, pas, pas ? PAS_TO_VPAGG(pas) : NULL);
		return pt;
	}
	mutex_spinunlock(&ptp->lock, s);

	ptpool_stat.miss++;

skip:

#ifdef	R4000
	flags = VM_VACOLOR|VM_VM|VM_NOAVAIL|(vmflag&VM_NOSLEEP);
#else	/* R4000 */
	flags = VM_NOAVAIL | vmflag & VM_NOSLEEP;
#endif	/* R4000 */

	/* go for full allocation */
	if ((pt = kvpalloc(1, flags, color)) == NULL)
		return NULL;

	if (IS_KSEG2(pt))
		atomicAddInt(&ptpool_vm, 1);
	atomicAddInt(&pmapmem, 1);

	PTPOOL_LOG_OP(pt, color, 3);
	bzero(pt, NBPP);
	LINK_PT_TO_ASJOB(pt, pas, pas ? PAS_TO_VPAGG(pas) : NULL);
	return pt;
}

static void
ptpool_free(void *pt, int flags)
{
	ptpool_t	*ptp;
	uint_t		color;
	uint_t		s;

#ifdef	SPT_DEBUG
	pmap_ptfree_spt_check((pde_t *)pt);
#endif	/* SPT_DEBUG */

	atomicAddInt(&pmapmem, -1);

	if (!ptpool_active || (flags & PMAPFLAG_LOCAL))
		goto out;

#ifdef	PTPOOL_DEBUG
	ptpool_free_ckeck(pt, flags);
#endif	/* PTPOOL_DEBUG */

#ifdef	R4000
	color = colorof(pt);
#else	/* R4000 */
	color = 0;
#endif	/* R4000 */
	ptp = ptpool + color;
	s = mutex_spinlock(&ptp->lock);
	if (ptp->cnt < ptp->maxcnt) {
		ptp->cnt++;
		*((void **)pt) = ptp->head;
		ptp->head = pt;
		LINK_PT_TO_ASJOB(pt, NULL, NULL);
		PTPOOL_LOG_OP(pt, color, 0);
		mutex_spinunlock(&ptp->lock, s);
		return;
	}
	mutex_spinunlock(&ptp->lock, s);
	PTPOOL_LOG_OP(pt, color, 1);
out:
	LINK_PT_TO_ASJOB(pt, NULL, NULL);
	kvpffree(pt, 1, VM_NOAVAIL);
	if (IS_KSEG2(pt))
		atomicAddInt(&ptpool_vm, -1);
}

/* ARGSUSED */
static int
ptpool_shake(int level)
{
	volatile ptpool_t	*ptp;
	int 			freed;
	void			*pt;
	uint_t			color;
	uint_t			s;

	if (!ptpool_active)
		return 0;

	freed = 0;
	for (color = 0; color < ptpool_ncolor; color++) {
		ptp = ptpool + color; 
		do {
			s = mutex_spinlock((lock_t *)&ptp->lock);
			if (ptp->cnt != 0) {
				pt = ptp->head;
				ptp->head = *((void **)pt);
				*((void **)pt) = NULL;
				ptp->cnt--;
			} else
				pt = NULL;
			mutex_spinunlock((lock_t *)&ptp->lock, s);

			if (pt) {
				PTPOOL_LOG_OP(pt, color, 4);
				kvpffree(pt, 1, VM_NOAVAIL);
				if (IS_KSEG2(pt))
					atomicAddInt(&ptpool_vm, -1);
				freed++;
			}

		} while (pt != NULL);
	}
	return freed;
}

#ifdef	PTPOOL_DEBUG
void
ptpool_free_ckeck(void *pt, int flags)
{
	ulong_t	*p, *ep;

	if ((flags & PMAPFLAG_LOCAL) == 0) {
		for (p = (ulong_t *)pt, ep = p + (NBPP / sizeof(ulong_t));
		     p < ep; p++)
			if (*p != 0)
				panic("non-zero pte");
	}
}

typedef struct	ptpool_log_s {
	void	*pt;
	int	color;
	int	op;
} ptpool_log_t;

ptpool_log_t	ptpool_log[1024];
ptpool_log_t	*ptpool_log_cur = ptpool_log;

void
ptpool_log_op(void *pt, int color, int op)
{
	ptpool_log_cur->pt = pt;
	ptpool_log_cur->color = color;
	ptpool_log_cur->op = op;

	if (++ptpool_log_cur == (ptpool_log + 1024))
		ptpool_log_cur = ptpool_log;
}
#endif	/* PTPOOL_DEBUG */


/* LARGE PAGE PMAP OPERATIONS */

/*
 * This routine is necessary in the case of R4000 and R10000 TLBS.
 * The architecture demands that both the odd and even entries of the tlb
 * should be of the same page size. We call the odd or the even entry 
 * buddy entry. So if we cannot construct a large page at a specific virtual
 * address then its buddy has to be broken down to the base page size.
 * This routine checks the buddy of a virtual address for a large page.
 * If a large page exists at that address it returns LARGE_PAGE_PRESENT.
 * If a bunch of small pages exist it returns SMALL_PAGE_PRESENT.
 * If no pages are present for the entire range it returns NO_PAGES_PRESENT.
 */

int
pmap_check_lpage_buddy(
	pas_t	*pas,
	caddr_t	start_addr,
	preg_t	*prp,
	size_t	desired_page_size)
{
	pde_t	*pd;
	pgno_t	npgs = btoct(desired_page_size);
	int	i;
	pgmaskindx_t	page_mask_index;
	caddr_t	buddy_addr;
	int	pfn_present = 0;

	page_mask_index = PAGE_SIZE_TO_MASK_INDEX(desired_page_size);

	/* 
	 * If the address is on the odd page boundary then check the
	 * address on the even boundary else vice versa.
	 */

	if (IS_ODD(start_addr, desired_page_size))
		buddy_addr =  start_addr - desired_page_size;
	else buddy_addr =  start_addr + desired_page_size;

	pd = pmap_ptes(pas, prp->p_pmap, buddy_addr, &npgs, VM_NOSLEEP);

	if (!pd) return NO_PAGES_PRESENT;

	ASSERT(npgs == btoct(desired_page_size));

	for (i = 0; i < npgs; i++, pd++) {
		if (pg_getpfn(pd)) {
			pfn_present++;
			if (pg_get_page_mask_index(pd) != page_mask_index)
					return SMALL_PAGES_PRESENT;
		}
	}
	if ( pfn_present) return LARGE_PAGE_PRESENT;
	else return NO_PAGES_PRESENT;
}

/*
 * Break down the large page into pages of size NBPP. It first checks to
 * see if there is a large page present for the address. If not it
 * checks to see if the buddy address is a large page. If both addresses
 * do not have a large page then it does nothing, else it calls 
 * pmap_downgrade_lpage_pte() to downgrade the ptes for the even and odd
 * large pages.  For all the downgrade routines the region lock should be
 * held before calling in here.
 */
void
pmap_downgrade_to_base_page_size(
	pas_t *pas,
	caddr_t vaddr,
	preg_t *prp,
	size_t	page_size,
	int flags)
{
	pde_t	*pd; 
	caddr_t	lpage_start_addr;

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));
	lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, page_size);
	pd = pmap_pte(pas, prp->p_pmap, lpage_start_addr, VM_NOSLEEP);

	/*
	 * If there are no ptes nothing to do .
	 */
	if (!pd) return;

	/*
	 * If there is no large page for the entire range do nothing.
	 * otherwise pass the large page pd to the downgrade pte
	 * routine.
	 */

	pd = LARGE_PAGE_PTE_ALIGN(pd, NUM_TRANSLATIONS_PER_TLB_ENTRY * 
						btoct(page_size));
	if (!pg_get_page_mask_index(pd)) {

		pd += btoct(page_size);

		if (!pg_get_page_mask_index(pd))
			return;
	}

#pragma set woff 1110	/* on 32 bit systems, can't get here, but no
	* easy/clean way to say so with out turning off the warning;
	* we don't want to use NOTREACHED, because that isn't true for all */
	LPG_FLT_TRACE(PMP_DGRD_LPG_BUDDY, vaddr, pd, prp, lpage_start_addr);
	INC_LPG_STAT(private.p_nodeid,
		PAGE_SIZE_TO_INDEX(page_size), LPG_PAGE_DOWNGRADE);

	if (flags & PMAP_TLB_FLUSH)
		pmap_downgrade_lpage_pte(pas, lpage_start_addr, pd);
	else 
		pmap_downgrade_lpage_pte_noflush(pd);
#pragma reset woff 1110
}


void
pmap_downgrade_pte_to_lower_pgsz(
	pas_t *pas, 
	caddr_t vaddr, 
	pde_t *spd, 
	size_t new_page_size)
{
	pgno_t		num_base_pages;
	int		new_pagemask_index, i;
	pde_t		*pd;

	num_base_pages = NUM_TRANSLATIONS_PER_TLB_ENTRY 
		* PAGE_MASK_INDEX_TO_CLICKS(pg_get_page_mask_index(spd));
	spd = LARGE_PAGE_PTE_ALIGN(spd, NUM_TRANSLATIONS_PER_TLB_ENTRY * 
			PAGE_MASK_INDEX_TO_CLICKS(pg_get_page_mask_index(spd)));
	new_pagemask_index = PAGE_SIZE_TO_MASK_INDEX(
				PGSZ_INDEX_TO_SIZE(new_page_size));

	/*
	 * First invalidate all the pages that belong to the large page.
	 * We need to invalidate  the pages and do a tlbclean before
	 * we can reduce the page size. This is to prevent migration
	 * on the page while some process is using that page as part of a
	 * large page.
	 */
	for(pd = spd, i = 0; i < num_base_pages; i++, pd++)
		pg_clrhrdvalid(pd); 

	/*
	 * Flush all the large pages out. There will be only one tlb 
	 * entry for all the large pages. So one tlbclean is 
	 * sufficient.
	 * when pas == NULL (remapf) then we alas need to flush
	 * isolated processors but we don't know which ones to flush.
	 * XXX at a minimum we need to check whether the underlying
	 * region is RG_ISOLATE then flush them all
	 */
	if (pas)
		tlbclean(pas, btoct(vaddr), pas->pas_isomask);
	else
		tlbsync(0LL, allclr_cpumask, FLUSH_MAPPINGS);

	for(pd = spd, i = 0; i < num_base_pages; i++, pd++)
		if (pg_isvalid(pd))
			pg_set_page_mask_index(pd, new_pagemask_index);
	ASSERT(check_lpage_pte_validity(spd));

}

/*
 * pmap_downgrade_page_size:
 * Downgrade the vaddr range [align(vaddr, 2*old_page_size), 2*old_page_size) 
 * so that vaddr can be mapped with pages of new_page_size.
 */
void
pmap_downgrade_page_size(
	pas_t *pas,
	caddr_t vaddr,
	preg_t *prp,
	size_t old_page_size,
	size_t new_page_size)
{
	pde_t		*pd;
	caddr_t		aligned_va;
	caddr_t		even_aligned_va;
	pgszindx_t 	buddy_index;
	pgszindx_t	old_page_size_index;
	pgszindx_t	new_page_size_index;
	pgszindx_t	cur_page_size_index;
	size_t		cur_page_size;

	new_page_size_index = PAGE_SIZE_TO_INDEX(new_page_size);
	old_page_size_index = PAGE_SIZE_TO_INDEX(old_page_size);

	for (cur_page_size_index = old_page_size_index; 
			cur_page_size_index > new_page_size_index;
						cur_page_size_index--) {
		cur_page_size = PGSZ_INDEX_TO_SIZE(cur_page_size_index);
		aligned_va = LPAGE_ALIGN_ADDR(vaddr, cur_page_size);
		even_aligned_va = LPAGE_ALIGN_ADDR(vaddr, 
				NUM_TRANSLATIONS_PER_TLB_ENTRY*cur_page_size);
		if (aligned_va != even_aligned_va) {
			pd = pmap_pte(pas, prp->p_pmap, even_aligned_va, VM_NOSLEEP);
			if (pd) {
				buddy_index = PAGE_MASK_INDEX_TO_PGSZ_INDEX(
						pg_get_page_mask_index(pd));
				if (buddy_index == cur_page_size_index) {
					pmap_downgrade_pte_to_lower_pgsz(
						pas, even_aligned_va, pd, 
					PGSZ_INDEX_TO_SIZE(buddy_index - 1));
					break;
				}
			}
		} else {
			pd = pmap_pte(pas, prp->p_pmap, even_aligned_va + 
						cur_page_size, VM_NOSLEEP);
			if (pd) {
				buddy_index = PAGE_MASK_INDEX_TO_PGSZ_INDEX(
						pg_get_page_mask_index(pd));
				if (buddy_index == cur_page_size_index) {
					pmap_downgrade_pte_to_lower_pgsz(
						pas, even_aligned_va + 
							cur_page_size, pd,
					PGSZ_INDEX_TO_SIZE(buddy_index - 1));
					break;
				}
			}
		}
	}
}


/*
 * Break down all the even and odd large page ptes related to any mapped
 * large page. Range is identified by vaddr & page_size.
 *
 * It also flushes out the tlb entries belonging to the large page.
 * It first invalidates all the ptes. As we are holding the region lock
 * when we call into this routine, any further accesses to the page will
 * cause them to do a vfault and wait on the region lock. One tlbclean()
 * is sufficient as there can be only one tlbentry for the large page.
 */
void
pmap_downgrade_range_to_base_page_size(
	pas_t *pas, 
	caddr_t vaddr, 
	preg_t *prp,
	size_t page_size)
{
	pgno_t	num_base_pages; /* Number of base pages in the large page */
	caddr_t	lpage_start_addr;
	int	i, nvalid;
	pde_t	*lpd, *pd;

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	num_base_pages = btoct(page_size);
	if (num_base_pages == btoct(NBPP))  return;

	/*
	 * Get the first pte of large page at the even address.
	 */
	lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, page_size);
	pd = pmap_pte(pas, prp->p_pmap, lpage_start_addr, VM_NOSLEEP);
	lpd = LARGE_PAGE_PTE_ALIGN(pd, NUM_TRANSLATIONS_PER_TLB_ENTRY * 
						btoct(page_size));

	/*
	 * If no valid entries found or if all entries are base page size, return.
	 */
	for(pd=lpd, i=0; i < NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages; i++, pd++)
		if (pg_isvalid(pd) && pg_get_page_mask_index(pd) > MIN_PAGE_MASK_INDEX)
			break;

	if (i == NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages)
		return;

	/*
	 * Now invalidate all the pages that belong to the large page.
	 * We need to invalidate  the pages and do a tlbclean before
	 * we can reduce the page size. This is to prevent migration
	 * on the page while some process is using that page as part of a
	 * large page.
	 */
	for(pd=lpd, i=0; i < NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages; i++, pd++)
		pg_clrhrdvalid(pd); 
	
	/*
	 * Flush all the large pages out. There will be only one tlb 
	 * entry for all the large pages. So one tlbclean is 
	 * sufficient.
	 */
	tlbclean(pas, btoct(vaddr), pas->pas_isomask);

	for(pd = lpd, i = 0; i < NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages; 
								i++, pd++)
		if (pg_isvalid(pd))
			pg_set_page_mask_index(pd, MIN_PAGE_MASK_INDEX);

	LPG_FLT_TRACE(PMP_DGRD_LPG_PTE, pd, 0, num_base_pages, 0);
	INC_LPG_STAT(private.p_nodeid,
		PAGE_SIZE_TO_INDEX(page_size), LPG_PAGE_DOWNGRADE);
	ASSERT(check_lpage_pte_validity(lpd));
}

/*
 * Break down all the even and odd large page ptes related to any mapped
 * large page. The parameter is any pte that belongs to the odd or even large 
 * page. It also flushes out the tlb entries belonging to the large page.
 * It first invalidates all the ptes. As we are holding the region lock
 * when we call into this routine, any further accesses to the page will
 * cause them to do a vfault and wait on the region lock. One tlbclean()
 * is sufficient as there can be only one tlbentry for the large page.
 */
void
pmap_downgrade_lpage_pte(
	pas_t *pas, 
	caddr_t vaddr, 
	pde_t *pd)
{
	pgno_t	num_base_pages; /* Number of base pages in the large page */
	int	i;
	pde_t	*lpd;

	num_base_pages = PAGE_MASK_INDEX_TO_CLICKS(pg_get_page_mask_index(pd));

	if (num_base_pages == btoct(NBPP))  return;

	/*
	 * Get the first pte of large page at the even address.
	 */
	lpd = LARGE_PAGE_PTE_ALIGN(pd, 
			NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages);

	/*
	 * First invalidate all the pages that belong to the large page.
	 * We need to invalidate  the pages and do a tlbclean before
	 * we can reduce the page size. This is to prevent migration
	 * on the page while some process is using that page as part of a
	 * large page.
	 */
	for(pd = lpd, i = 0; i < NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages; 
								i++, pd++)
		pg_clrhrdvalid(pd); 

	/*
	 * Flush all the large pages out. There will be only one tlb 
	 * entry for all the large pages. So one tlbclean is 
	 * sufficient.
	 * when pas == NULL (remapf) then we alas need to flush
	 * isolated processors but we don't know which ones to flush.
	 * XXX at a minimum we need to check whether the underlying
	 * region is RG_ISOLATE then flush them all
	 */
	if (pas)
		tlbclean(pas, btoct(vaddr), pas->pas_isomask);
	else
		tlbsync(0LL, allclr_cpumask, FLUSH_MAPPINGS);

	for(pd = lpd, i = 0; i < NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages; 
								i++, pd++)
		pg_set_page_mask_index(pd, MIN_PAGE_MASK_INDEX);

	LPG_FLT_TRACE(PMP_DGRD_LPG_PTE, pd, 0, num_base_pages, 0);
	ASSERT(check_lpage_pte_validity(lpd));
}

/*
 * Downgrade the ptes for a large page but do not flush the tlb.
 * Same as above routine except for not flushing the tlb entries.
 */

void
pmap_downgrade_lpage_pte_noflush(pde_t *pd)
{
	pde_t	*lpd;
	pgno_t	num_base_pages; /* Number of base pages in the large page */
	int	numpgs;

	num_base_pages = PAGE_MASK_INDEX_TO_CLICKS(pg_get_page_mask_index(pd));

	/*
	 * Get the first pte of large page at the even address.
	 */

	lpd = LARGE_PAGE_PTE_ALIGN(pd, 
				NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages);

	numpgs = NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages;

	for(pd = lpd; numpgs; numpgs--, pd++) {
		pg_clrhrdvalid(pd); 
		pg_set_page_mask_index(pd, MIN_PAGE_MASK_INDEX);
	}

	ASSERT(check_lpage_pte_validity(lpd));
	LPG_FLT_TRACE(PMP_DGRD_LPG_PTE_NOFLUSH, pd, lpd, num_base_pages, 0);
}

/*
 * This routine checks the boundaries of <vaddr, vaddr+len>. If the boundaries
 * contain large pages then those large page ptes are broken down to ptes of
 * base page size. Flags indicate if a tlbclean is needed.
 * Note that pas might be NULL here (from remapf) where we don't have
 * a current context.
 */
void
pmap_downgrade_addr_boundary(
	pas_t *pas,
	register preg_t *prp, 
	register caddr_t vaddr,
	register pgcnt_t len,
	register int	flags)
{
	caddr_t	end_addr, align_addr;
	size_t	page_size;

	/*
	 * If pregion size is zero, there is nothing to downgrade.
	 */
	if (!prp->p_pglen)
		return;

	page_size = PMAT_GET_PAGESIZE(findattr(prp, vaddr));
	if (page_size > NBPP) {
		LPG_FLT_TRACE(PMP_ADDR_BOUNDARY, vaddr, prp, len, flags);

		/*
		 * If the address is on the even tlb boundary, no need to
		 * do anything.
		 */
		if (vaddr > LPAGE_ALIGN_ADDR(vaddr, 
				NUM_TRANSLATIONS_PER_TLB_ENTRY*page_size))
			pmap_downgrade_to_base_page_size(pas, vaddr, prp, page_size, flags);
	}

	/*
	 * Get the page size of the last page in the range.
	 */
	end_addr = vaddr + ctob(len - 1);
	page_size = PMAT_GET_PAGESIZE(findattr(prp, end_addr));

	if (page_size > NBPP) {
		LPG_FLT_TRACE(PMP_ADDR_BOUNDARY, vaddr, prp, end_addr, flags);

		align_addr = LPAGE_ALIGN_ADDR(end_addr,
                                NUM_TRANSLATIONS_PER_TLB_ENTRY*page_size);

		/*
		 * If the address ends on the odd tlb boundary, no need to
		 * do anything.
		 */
		if ((vaddr + ctob(len)) < (align_addr +  
				NUM_TRANSLATIONS_PER_TLB_ENTRY*page_size))
			pmap_downgrade_to_base_page_size(pas, end_addr, prp, page_size,
							flags);
	}
}

/*
 * Clear a bunch of pte flags on all the ptes of a large page.
 * The caller ensures that the process will not be faulting in this
 * pte at the time of modifying it. Called from write_utext().
 */
void
pmap_clr_lpage_pte_flags( pde_t	*pd, int flags)
{
	pde_t	*lpd;
	pgno_t	num_base_pages; /* Number of base pages in the large page */

	num_base_pages = NUM_TRANSLATIONS_PER_TLB_ENTRY * 
			PAGE_MASK_INDEX_TO_CLICKS(pg_get_page_mask_index(pd));


	/*
	 * Get the first pte of large page at the even address.
	 */

	lpd = LARGE_PAGE_PTE_ALIGN(pd, num_base_pages);


	for(pd = lpd; num_base_pages; num_base_pages--, pd++) 
		PG_CLR(pd->pgi, flags);

	ASSERT(check_lpage_pte_validity(pd));
}

/*
 * Downgrade an entire range of virtual addresses in a region.
 */
void
pmap_downgrade_range(
	pas_t *pas,
	ppas_t *ppas,
	preg_t *prp,
	caddr_t vaddr,
	size_t len)
{
	attr_t	*attr;
	caddr_t	end_addr;
	size_t	page_size;

	end_addr = vaddr + len;
	attr = findattr_range_noclip(prp, vaddr, end_addr);
	
	newptes(pas, ppas, prp->p_flags & PF_SHARED);

	page_size = PMAT_GET_PAGESIZE(attr);

	while (vaddr < end_addr) {
		pmap_downgrade_to_base_page_size(pas, vaddr, prp, page_size, 0);
		vaddr = LPAGE_ALIGN_ADDR(vaddr,
			NUM_TRANSLATIONS_PER_TLB_ENTRY * page_size) + 
				NUM_TRANSLATIONS_PER_TLB_ENTRY * page_size;
		if (vaddr >= attr->attr_end) {
			attr = attr->attr_next;
			if (attr == NULL) break;
			page_size = PMAT_GET_PAGESIZE(attr);
		}
	}
}
	

#ifdef	DEBUG
/*
 * Debug routines for large page ptes. This one checks that the pte is
 * the beginning of a large page pte. 
 */
int
check_lpage_pte_begin(pde_t *pte)
{
	pde_t	*lpte;
	pgno_t	num_base_pages; /* Number of base pages in the large page */

	num_base_pages = PAGE_MASK_INDEX_TO_CLICKS(
				pg_get_page_mask_index(pte));

	if (num_base_pages == btoc(NBPP))  return 1;

	/*
	 * Get the first pte of large page at the even address.
	 */

	lpte = LARGE_PAGE_PTE_ALIGN(pte, num_base_pages);
	if( lpte != pte)  {
		cmn_err(CE_NOTE,"Pte not starting pte %x\n",pte);
		return 0;
	}
	return 1;
}

/*
 * Ensures that all the ptes of a large page are consistent.
 */

int
check_lpage_pte_validity(pde_t *pte)
{
	pde_t	*lpte;
	pgno_t	num_base_pages; /* Number of base pages in the large page */
	pgmaskindx_t	page_mask_index;
	int	mod, valid;
	int	i;
	int	num_large_ptes = 0;
	int	num_no_ptes = 0;
	int	large_page_present = 0;
	pgno_t	pfn;

	num_base_pages = 4;	/* Assume 64K pages */

	lpte = LARGE_PAGE_PTE_ALIGN(pte, NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages);

	for (i = 0; i < NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages; i++, lpte++) {
		if (pg_getpfn(lpte)) {
			if (pg_get_page_mask_index(lpte)) {
				large_page_present =1;
				break;
			}
		}
	}
	
	if (!large_page_present) return 1;


	/*
	 * Get the first pte of even large page.
	 */

	lpte = LARGE_PAGE_PTE_ALIGN(pte, NUM_TRANSLATIONS_PER_TLB_ENTRY*num_base_pages);

	page_mask_index = pg_get_page_mask_index(lpte);
	mod = pg_ismod(lpte);
	valid = pg_isvalid(lpte);

	pfn = pg_getpfn(lpte);
	for (i = 0; i < num_base_pages; i++, lpte++) {
		if (pg_getpfn(lpte)) {
			if (page_mask_index != 	pg_get_page_mask_index(lpte))
				cmn_err(CE_PANIC,"lpte invalid:page mask index does not match %x\n",lpte);
			if (pfn != pg_getpfn(lpte))
				cmn_err(CE_PANIC,"lpte invalid:pfns not contiguous pte %x\n",lpte);
			else pfn++;
			if (mod != pg_ismod(lpte))
				cmn_err(CE_PANIC,"lpte invalid:page mod bit does not match %x\n",lpte);
			if (valid != pg_isvalid(lpte))
				cmn_err(CE_PANIC,"lpte invalid:page mod bit does not match %x\n",lpte);
			num_large_ptes++;
		} else num_no_ptes++;
	}

	ASSERT((num_no_ptes == 0) || (num_no_ptes == num_base_pages));
	ASSERT((num_large_ptes == 0) || (num_large_ptes == num_base_pages));

	/*
	 * Check the odd pte entry. lpte already is correctly aligned.
	 * If the even entry was all 0s we need to get the new page_mask_index
	 */

	if (!page_mask_index)
		page_mask_index = pg_get_page_mask_index(lpte);
	mod = pg_ismod(lpte);
	valid = pg_isvalid(lpte);

	num_no_ptes = 0;
	num_large_ptes = 0;
	pfn = pg_getpfn(lpte);
	for (i = 0; i < num_base_pages; i++, lpte++) {
		if (pg_getpfn(lpte)) {
			if (page_mask_index != 	pg_get_page_mask_index(lpte))
				cmn_err(CE_PANIC,"lpte invalid:page mask index does not match %x\n",lpte);
			if (pfn != pg_getpfn(lpte))
				cmn_err(CE_PANIC,"lpte invalid:pfns not contiguous pte %x\n",lpte);
			else pfn++;
			if (mod != pg_ismod(lpte))
				cmn_err(CE_PANIC,"lpte invalid:page dirty bit does not match %x\n",lpte);
			if (valid != pg_isvalid(lpte))
				cmn_err(CE_PANIC,"lpte invalid:page dirty bit does not match %x\n",lpte);
			num_large_ptes++;
		} else num_no_ptes++;
	}

	ASSERT((num_no_ptes == 0) || (num_no_ptes == num_base_pages));
	ASSERT((num_large_ptes == 0) || (num_large_ptes == num_base_pages));
	return 1;
}
#endif /* DEBUG */

void *
pmap_segtab_alloc(int vmflag)
{
	void	*nsegtab;

#ifdef	_MIPS3_ADDRSPACE
	if (segtable_zone)
		nsegtab = kmem_zone_zalloc(segtable_zone,
				VM_DIRECT | (vmflag & VM_NOSLEEP));
	else
		nsegtab = kmem_zalloc(SEGTABSIZE * sizeof(pde_t *),
		     VM_PHYSCONTIG | VM_DIRECT | (vmflag & VM_NOSLEEP));
#else	/* _MIPS3_ADDRSPACE */
	nsegtab = kmem_zalloc(SEGTABSIZE * sizeof(pde_t *),
	     VM_PHYSCONTIG | VM_DIRECT | (vmflag & VM_NOSLEEP));
#endif	/* _MIPS3_ADDRSPACE */

	return nsegtab;
}

/*
 * Shared Page Tables
 */

/*
 * The following set of routines are used to manage shared Page Tables for SHM.
 * The idea is to allow processes that map the same SHM segment at the same
 * address to share the Page Tables (PT).  This has two advantages:
 *
 * a) it saves physical memory and kernel virtual space,
 * b) processes attaching to a SHM segment automatically inherit the
 *    translation tree, this drastically reduces the number of page faults.
 *    
 * A page table maps a full NBPS (4 or 64 Megs) section of the
 * address space. Two  processes sharing a PT are forced to share the
 * slice of address space that is mapped by the PT. The mapping attributes
 * (protection, cache policies) must also be shared. 
 *
 * For SHM we have decided to restrict the shared PT option to those processes
 * attaching a SHM segment in the range [addr, addr + size], where both "addr"
 * and "addr + size" are on segment boundaries (NBPS).
 *
 * This is a reasonable restriction for SHM, it simplifies the implementation
 * and reduces the impact of the shared PT enhancement on the rest of the OS.
 * The above restriction implies that the size of the SHM should be an integral
 * multiple of NBPS.
 *
 * A process can select, at attach time, if shared PT's are desirable or not.
 * It does that by passing an additional flag: SHM_SHATTR to the shmat()
 * system call. Processes that do not pass the flag do not get shared PT's.
 * If the flag is set and shared PT's could not be used, shmat() fails and an
 * error is returned.
 *
 *
 * Data Structures
 * ---------------
 *
 * Pointers to the shared PT's are kept in a special non operational pmap
 * called template pmap (not used for translation). This pmap is accessible
 * through a descriptor associated with the SHM descriptor, called:
 * "pmap_sptdesc". The descriptor and the pmap (but not the shared PT) are
 * released when the SHM descriptor is removed. The segment table in the
 * template pmap is called template segment table.
 *
 * In 6.2 there are two type of pmaps: trilevel and segment. 64 bit processes
 * use a "trilevel" pmap. 32-bit processes use the "segment" pmap.
 * To simpify the implementation, we have made the decision of not allowing
 * PT sharing across different pmaps. 64-bit processes can only share their
 * PT with other 64-bit processes, the same applies to 32-bit processes.
 *
 * Different "pmap_sptdesc" are allocated for 32-bit and 64-bit processes.
 * The template pmap associated with a 32-bit descriptor is a segment pmap.
 * The template pmap associated with a 64-bit descriptor is a trilevel pmap.
 *
 * We found that maintaining a descriptor for each shared PT allowed us to
 * reduce the number of hooks in the VM and to perform efficiently allocations
 * and deallocations of shared PT.
 * This descriptor contains two fields:
 *	i)	a reference count (number of processes sharing that PT)
 *	ii)	a back pointer to the location in the template segment table
 *		pointing to this shared PT.
 *
 * To avoid the extra complexity related to the maintenance of PT "descriptors"
 * we chose to overload the pfdat of the physical page that contain the shared
 * PT. The field "pf_use" is used as a reference count. The field "pf_nxt" has
 * been given a new name ("pf_sppt") and used as the back pointer. 
 *
 * The pmap of a process using shared PT is marked with the flag PMAPFLAG_SPT.
 * The flag is used to avoid the execution of the additional shared PT code,
 * if the pmap doesn't have shared PT.
 * The flag is set when the process attaches to the SHM and never cleared. That
 * means that a pmap may still be marked with the PMAPFLAG_SPT after having
 * lost all of its shared PT's (see section on private PT's below).
 *
 *
 * Implementation
 * --------------
 *
 * Let's first examine the following scenario:
 *
 *	i)	SHM segment is created (shmget()),
 *	ii)	a set of processes attach to the SHM segment (shmat()),
 *	iii)	all of the attached processes detach the SHM segment (either
 *		shmdt() or exit()),
 *	iv)	the SHM segment is removed (shmctl(IPC_RMID))
 *
 * We will also assume that attached processes do not modify the properties
 * of address space range where the SHM is attached (mprotect, munmap ..).
 *
 * The very first process attaching the SHM segment creates the "pmap_sptdesc"
 * data structure and allocates the template pmap (pmap data structure and
 * segment table). The descriptor is tagged with the address of the SHM
 * descriptor and the attaching base address.
 *
 * At attach time a process is given all the shared PT for the SHM segment.
 * The process looks up the "pmap_sptdesc" and checks the segment table entries
 * in the template pmap. If the entry is set then it copies the pointer to its
 * segment table. If the entry is not set it allocates a new shared PT, sets
 * the pointer in the template pmap and then copies the pointer to its segment
 * table. The pf_use counter is incremented (its value after the increment is
 * 2, the kernel memory allocator initialized it to 1).
 *
 * The pf_use counter is incremented for each new user of the shared PT. The
 * shared PT code checks the pf_use counter to determine if a PT is shared or
 * private (private if  pf_use == 1, shared if pf_use > 1).  
 *
 * A value of 3 implies that 2 pmap's are using the shared PT.
 *
 * At detach time a process checks each PT is the address range. If a PT is
 * shared the pf_use counter is decremented. If the reference count becomes 1
 * the PT is released. The back pointer is then used to clear the entry in the
 * template segment table.
 *
 * When the SHM segment (shmctl(IPC_RMID)) is removed, "pmap_sptdesc" is
 * released along with the template pmap.
 *
 * Let's now examine the case in which the SHM segment is removed before all
 * of the processes have either detached it or exited. In this case the process
 * performing the removal will also release the template pmap and the
 * "pmap_sptdesc" data structure.
 * The remove code takes care of clearing the pg_sppt pointer in the PT's pfdat
 * since the template segment table has been removed.
 *
 *
 * Private PT's
 * ------------
 *
 * The following actions on an address range that intersects the SHM segment
 * cause the interested shared PT to be replaced by a private one.
 *
 *	i)	mprotect() to read only,
 *	ii)	partial unmap in freepreg(),
 *	iii)	cachectl(),		
 *	iv)	chgpteprot(),
 *	v)	invalidateaddr()
 *	
 * The function pmap_modify() is called from outside the pmap layer to replace
 * shared PT's. The idea is similar to the copy on write optimization for data
 * pages.
 * When a shared PT is replaced, a new PT is allocated. The content of the
 * shared PT is copied into the private PT. During this process the use count
 * on the data pages must be incremented (see below for a detailed explanation
 * on reference counts on SHM pages). If the replacement causes the reference
 * count on the shared PT to go to 1, the shared PT is released and the pf_sppt
 * back pointer cleared.
 *
 * It's easy to see that (due to replacement) one process may end up having
 * both private and shared PT for the same SHM segment.
 *
 * Special care must be taken for reference counts on SHM pages. Irix uses the
 * pf_use field on data pages to count how many different PTE's are mapping
 * that physical page.
 * A PTE in a shared PT should be counted as one reference. It's up to the last
 * process detaching the shared PT to decrement the "pf_use" count in the data
 * pages.
 * When a shared PT is replaced the OS creates another reference on the data
 * pages. The "pf_use" of the data pages (not the PT)  must be incremented.
 * This is performed by the routine seg_spt_modify().
 *
 * The function pmap_spt_free() may end up calling the regular seg_free()
 * function. This happens when the current process is removing the last
 * reference to the shared PT. Here's the call diagram:
 *
 *	pmap_free()
 *		pmap_spt_free()
 *			seg_spt_free()
 *				segcommn_spt_free()		
 *					segcommon_free()
 *
 *  Locking
 *  -------
 *
 *  In 5.3 and 6.2 the region lock (UPDATE)  protects all the pmap_spt_*()
 *  operations. A simple spin lock protects the link list of pmap_sptdesc.
 *  The field pf_use in the pfdat for the page of PTE's is protected by the
 *  region lock of the SHM segment.
 *
 *  TLB invalidation
 *  ----------------
 *
 *  PT's are mapped in the KPTESEG space. When a PT is changed (replacement)
 *  the TLB entries relative to that section of KPTESEG must be invalidated.
 *  This is performed by the function newptes(). The function newptes() also
 *  invalidates the wired TLB if necessary.
 *
 *  PMAP trimming
 *  -------------
 *
 *  All the pmap's that are marked with PMAPFLAG_SPT are also marked as
 *  non trimmable (PMAPFLAG_NOTRIM).
 *
 *  Sproc
 *  -----
 *
 *  Sproc's are not affected by shared PT. The sproc shared pmap can also
 *  be using shared PT.
 *
 *  Nvalid conter and RSS
 *  ---------------------
 *
 *  The field "p_nvalid" in the pregion descriptor is generally used to
 *  contain the number of valid pages in that pregion. The counter is
 *  incremented on page faults and decremented when translations are
 *  unloaded (unmap, vhand, ..). When shared PT are used the OS
 *  cannot count on the fact that translations for new valid pages are
 *  always loaded on page faults. A lot of complicated changes would be
 *  required to keep the counters in sync. We have chose instead to
 *  do our best to avoid inconsistent situations (like negative counters). 
 */

pmap_sptdesc_t		*pmap_sptdesc_list = NULL;

/* prototypes */
STATIC void		pmap_sptdesc_insert(pmap_sptdesc_t *);
STATIC void		pmap_sptdesc_remove(pmap_sptdesc_t *);
STATIC pmap_sptdesc_t	*pmap_sptdesc_find(pmap_sptid_t, uint_t, addr_t);
STATIC int		pmap_spt_modify(pas_t *, pmap_t *, addr_t, size_t, boolean_t *, struct pm *);

#ifdef	_MIPS3_ADDRSPACE
STATIC void		trilevel_spt_attach(pas_t *, p3level_t *, p3level_t *, addr_t, size_t);
STATIC void		trilevel_spt_remove(p3level_t *, addr_t, size_t);
STATIC boolean_t	trilevel_spt_check(p3level_t *, addr_t, size_t);
STATIC int		trilevel_spt_modify(pas_t *, p3level_t *, addr_t, size_t, boolean_t *, struct pm *);
STATIC boolean_t	trilevel_spt_free(pas_t *, p3level_t *, addr_t, size_t, int);
#endif	/* _MIPS3_ADDRSPACE */

STATIC void		seg_spt_attach(pas_t *, pseg_t *, pseg_t *, addr_t, size_t);
STATIC void		seg_spt_remove(pseg_t *, addr_t, size_t);
STATIC boolean_t	seg_spt_check(pseg_t *, addr_t, size_t);
STATIC int		seg_spt_modify(pas_t *, pseg_t *, addr_t, size_t, boolean_t *, struct pm *);
STATIC boolean_t	seg_spt_free(pas_t *, pseg_t *, addr_t, size_t, int);

STATIC void		segcommon_spt_attach(pas_t *, pcom_t *, pcom_t *, addr_t, size_t);
STATIC void		segcommon_spt_remove(pcom_t *, addr_t, size_t);
STATIC boolean_t	segcommon_spt_check(pcom_t *, addr_t, size_t);
STATIC int		segcommon_spt_modify(pas_t *, pcom_t *, addr_t, size_t, boolean_t *, struct pm *);
STATIC boolean_t	segcommon_spt_free(pas_t *, pcom_t *, addr_t, size_t, int);

STATIC void		pmap_spt_ptalloc(pde_t **, uint_t);
STATIC void		pmap_spt_ptremove(pde_t *);
STATIC void		pmap_spt_ptfree(pde_t *);

#ifdef	SPT_DEBUG

void
pmap_checkzero(char *p, size_t size)
{
	char	*pend;

	for (pend = p + size; p < pend; p++)
		ASSERT(*p == 0);
}

void
pmap_pt_checkvalid(pde_t *pte)
{
	pde_t	*epte = pte + NPGPT;

	while (pte < epte) {
		ASSERT(!pg_isvalid(pte) || pte->pte.pg_pfn != 0);
		pte++;
	}
}

/*ARGSUSED*/
void
pmap_ptfree_spt_check(pde_t *pte)
{
	ASSERT((pfntopfdat(kvtophyspnum(pte)))->pf_use == 1);
}

#endif	/* SPT_DEBUG */

#define	SPT_BPBASETABSIZE		((ulong_t)stob(SEGTABSIZE))

#define	spt_btott(addr)		btobasetab((addr))
#define spt_ttob(index)		((__psunsigned_t)(index) << BPBASETABSHIFT)
#define	spt_btot(addr)		(((__psunsigned_t)(addr) + SPT_BPBASETABSIZE - 1) >> BPBASETABSHIFT)

#define	SPT_ANYTYPE	((uint_t)-1)
#define	SPT_ANYADDR	((addr_t)-1L)

static lock_t	pmap_sptlock;

STATIC void
pmap_spt_init(void)
{
	initnlock(&pmap_sptlock, "pmap_sptlock");
}

/*
 * List manipulation routines: insertion, deletion, lookup.
 * The list contains all the pmap_sptdesc.
 */

STATIC void
pmap_sptdesc_insert(pmap_sptdesc_t *desc)
{
	pmap_sptdesc_t	*ndesc;

	if ((ndesc = pmap_sptdesc_list) == NULL) {
		desc->spt_next = desc->spt_prev = desc;
		pmap_sptdesc_list = desc;
		return;
	}
	desc->spt_next = ndesc;
	desc->spt_prev = ndesc->spt_prev;
	desc->spt_next->spt_prev = desc->spt_prev->spt_next = desc;
	pmap_sptdesc_list = desc;
}

STATIC void
pmap_sptdesc_remove(pmap_sptdesc_t *desc)
{
	if (pmap_sptdesc_list == desc) {
		if (desc->spt_next == desc && desc->spt_prev == desc) {
			pmap_sptdesc_list = NULL;
			return;
		}
		pmap_sptdesc_list = desc->spt_next;
	}
	desc->spt_next->spt_prev = desc->spt_prev;
	desc->spt_prev->spt_next = desc->spt_next;
	desc->spt_next = desc->spt_prev = NULL;
}

STATIC pmap_sptdesc_t *
pmap_sptdesc_find(pmap_sptid_t id, uint_t type, addr_t addr)
{
	pmap_sptdesc_t	*desc;
	boolean_t	found;

	if ((desc = pmap_sptdesc_list) == NULL)
		return NULL;

	found = B_FALSE;
	do {
		if (desc->spt_id == id &&
		    ((desc->spt_type == type) || (type == SPT_ANYTYPE)) &&
		    ((desc->spt_addr == addr) || (addr == SPT_ANYADDR))) {
			found = B_TRUE;
			break;
		}
	} while ((desc = desc->spt_next) != pmap_sptdesc_list);

	return (found ? desc : NULL);
}

/*
 * Main interface for shared PT replacement (see comment above).
 * Called from: cachectl(), mprotect(), freepreg(), chgpteprot().
 * Caller must take care of locking region.
 */

int
pmap_modify(pas_t *pas, ppas_t *ppas, preg_t *prp, addr_t addr, size_t pgsz)
{
	boolean_t	tlbclean;
	int		count;
	pmap_t		*pmap = prp->p_pmap;

	/* region lock must be held (UPDATE) */

	if (pmap_spt(pmap) == B_FALSE)
		return 0;

	count = pmap_spt_modify(pas, pmap, addr, pgsz, &tlbclean, 
			attr_pm_get(findattr(prp, addr)));

	/* this region might always be shared */
	if (tlbclean)
		newptes(pas, ppas, 1);

	return count;
}

boolean_t
pmap_pt_is_shared(pde_t *pt)
{
	pfd_t	*pfd;

	pfd = pfntopfdat(kvtophyspnum(pt));
	if (pfd->pf_use > 1)
		return B_TRUE;
	else
		return B_FALSE;
}

/*
 * Allocation and initialization of pmap_sptdesc.
 * Creates template pmap as well. Called from shmat().
 * Region lock must be locked by caller.
 */ 
int
pmap_spt_get(
	pas_t *pas, 
	pmap_sptid_t id, 
	pmap_t *dpmap, 
	uvaddr_t addr, 
	size_t size)
{
	pmap_sptdesc_t	*desc;
	int		s;
	uint_t		type;
	uvaddr_t	hiusrattach;


	/* region lock must be held (UPDATE) */

	if (soff(addr) || soff(size))
		return EINVAL;

	hiusrattach = pas->pas_hiusrattach;

	if (((__psunsigned_t)addr > (__psunsigned_t)hiusrattach)
	 || ((__psunsigned_t)(addr + size) > (__psunsigned_t)hiusrattach))
		return EINVAL;

	type = dpmap->pmap_type;

#ifdef	_MIPS3_ADDRSPACE
	if (type != PMAP_SEGMENT && type != PMAP_TRILEVEL)
		return EINVAL;
#else	/* _MIPS3_ADDRSPACE */
	if (type != PMAP_SEGMENT)
		return EINVAL;
#endif	/* _MIPS3_ADDRSPACE */

	s = splock(pmap_sptlock);

	/* look for existing descriptor */
	if (pmap_sptdesc_find(id, type, addr) != NULL) {
		spunlock(pmap_sptlock, s);
		return 0;
	}
	spunlock(pmap_sptlock, s);

#ifdef _MIPS3_ADDRSPACE
	if (pas->pas_flags & PAS_64) {
		if (type != PMAP_TRILEVEL)
			return EINVAL;
	} else {
		if (type != PMAP_SEGMENT)
			return EINVAL;
	}
#endif /* _MIPS3_ADDRSPACE */	

	desc = kmem_zalloc(sizeof(*desc), KM_SLEEP);
	desc->spt_id = id;
	desc->spt_size = size;
	desc->spt_pmap = pmap_create((pas->pas_flags & PAS_64), PMAP_SEGMENT);
	desc->spt_pmap->pmap_type = type;
	desc->spt_type = type;
	desc->spt_addr = addr;

	s = splock(pmap_sptlock);
	if (pmap_sptdesc_find(id, type, addr) != NULL) {
		spunlock(pmap_sptlock, s);
		pmap_destroy(desc->spt_pmap);
		kmem_free(desc, sizeof(*desc));
		return 0;
	}
	pmap_sptdesc_insert(desc);
	spunlock(pmap_sptlock, s);
	return 0;
}


/*
 * Removal of pmap_sptdesc. Also releases template pmap.
 * Called from shmctl(IPC_RMID), region lock must be locked
 * by caller.
 */
void
pmap_spt_remove(pmap_sptid_t id)
{
	pmap_sptdesc_t	*desc;
	pmap_t		*pmap;
	int		s;

	/* region lock must be held (UPDATE) */

	for ( ; ; ) {
		s = splock(pmap_sptlock);
		desc = pmap_sptdesc_find(id, SPT_ANYTYPE, SPT_ANYADDR);
		if (desc == NULL) {
			spunlock(pmap_sptlock, s);
			return;
		}
		/* remove from list */ 
		pmap_sptdesc_remove(desc);
		spunlock(pmap_sptlock, s);

		pmap = desc->spt_pmap;
#ifdef	SPT_DEBUG
		cmn_err(CE_CONT, "pmap_spt_remove: removing desc 0x%x, 0x%x\n",
			desc, desc->spt_addr);
#endif	/* SPT_DEBUG */
		switch (pmap->pmap_type) {
		case PMAP_SEGMENT:
			seg_spt_remove((pseg_t *)pmap, desc->spt_addr,
					desc->spt_size);
			break;
#ifdef	_MIPS3_ADDRSPACE
		case PMAP_TRILEVEL:
			trilevel_spt_remove((p3level_t *)pmap, desc->spt_addr,
					desc->spt_size);
			break;
#endif	/* _MIPS3_ADDRSPACE */
		}
		pmap_destroy(pmap);
		kmem_free(desc, sizeof(*desc));
	}
}

/*
 * Called by pmap_modify() if shared PT have been
 * detected in the address range. Calls seg_spt_modify()
 * to do real work.
 */

STATIC int
pmap_spt_modify(
	pas_t  *pas,
	pmap_t *pmap, 
	uvaddr_t addr, 
	size_t pgsz, 
	boolean_t *tlbclean,
	struct pm *pm)
{
	size_t		size;
	int		count;

	size = ctob(pgsz);
	*tlbclean = B_FALSE;

	if (pmap_spt_check(pmap, addr, size) == B_FALSE)
		return 0;

	switch (pmap->pmap_type) {
	case PMAP_SEGMENT:
		count = seg_spt_modify(pas, (pseg_t *)pmap, addr, size, 
						tlbclean, pm);
		break;
#ifdef	_MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		count = trilevel_spt_modify(pas, (p3level_t *)pmap, addr,
					     size, tlbclean, pm);
		break;
#endif	/* _MIPS3_ADDRSPACE */
	}
	return count;
}

/*
 * Called by pmap_free() to check for shared PT in the
 * address range [addr, addr + ctob(pgsz)]. If at least
 * one shared PT is found calls seg_spt_free() which works
 * for both shared and non shared PT. Otherwise returns 0
 * and lets pmap_free() do its regular work.
 */

STATIC int
pmap_spt_free(pas_t *pas, pmap_t *pmap, uvaddr_t addr, size_t pgsz, int free)
{
	size_t		size;
	boolean_t	tlbclean;

	ASSERT(pmap->pmap_flags & PMAPFLAG_SPT);
	size = ctob(pgsz);

	if (pmap_spt_check(pmap, addr, size) == B_FALSE)
		return 0;

	switch (pmap->pmap_type) {
	case PMAP_SEGMENT:
		tlbclean = seg_spt_free(pas, (pseg_t *)pmap, addr, size, free);
		break;
#ifdef	_MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		tlbclean = trilevel_spt_free(pas, (p3level_t *)pmap, addr,
					     size, free);
		break;
#endif	/* _MIPS3_ADDRSPACE */
	}

#if 0
	if (tlbclean)
		newptes(u.u_procp, addr, pgsz, 1);
#else
	tlbclean = tlbclean;	/* XXX quiet compiler remark */
#endif
	return 1;
}

/*
 * Attach to shared PT. Called by shmat(), assumes
 * region lock to the be locked.
 */
int
pmap_spt_attach(pmap_sptid_t id, pas_t *pas, pmap_t *dpmap, uvaddr_t addr)
{
	pmap_sptdesc_t	*desc;
	int		s;

	s = splock(pmap_sptlock);
	if ((desc = pmap_sptdesc_find(id, dpmap->pmap_type, addr)) == NULL) {
		spunlock(pmap_sptlock, s);
		return ENOENT;
	}

	/*
	 * release lock since region lock protects
	 * this specific descriptor.
	 */
	spunlock(pmap_sptlock, s);

	dpmap->pmap_flags |= (PMAPFLAG_SPT|PMAPFLAG_NOTRIM);

	switch (desc->spt_type) {
	case PMAP_SEGMENT:
		seg_spt_attach(pas, (pseg_t *)desc->spt_pmap, (pseg_t *)dpmap,
				desc->spt_addr, desc->spt_size);
		break;
#ifdef	_MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		trilevel_spt_attach(pas, (p3level_t *)desc->spt_pmap,
				    (p3level_t *)dpmap,
				    desc->spt_addr, desc->spt_size);
		break;
#endif	/* _MIPS3_ADDRSPACE */
	}
	return 0;
}

/*
 * Look for shared PT in the range [addr, addr + size],
 * externally called by pflip(). Internally called by
 * pmap_spt_modify() and pmap_spt_free(). Returns B_TRUE
 * if a shared PT was found, B_FALSE otherwise.
 */
boolean_t
pmap_spt_check(pmap_t *pmap, uvaddr_t addr, size_t size)
{
	boolean_t	ret;

	switch (pmap->pmap_type) {
	case PMAP_SEGMENT:
		ret = seg_spt_check((pseg_t *)pmap, addr, size);
		break;
#ifdef	_MIPS3_ADDRSPACE
	case PMAP_TRILEVEL:
		ret = trilevel_spt_check((p3level_t *)pmap, addr, size);
		break;
#endif	/* _MIPS3_ADDRSPACE */
	}
	return ret;
}

#ifdef	_MIPS3_ADDRSPACE
/*
 * Trilevel routines: wrappers around the "segment" level
 * routines that perform the real work. See comments in the
 * equivalent "segment" routines.
 */
STATIC void
trilevel_spt_attach(pas_t *pas, p3level_t *spp, p3level_t *dpp, 
						uvaddr_t addr, size_t size)
{
	pcom_t		*spcomp, *dpcomp;
	int		btindex;
	size_t		btsize;
	off_t		btoff;
	size_t		resid;
	uvaddr_t	saddr;
	size_t		ssize;

	btindex = spt_btott(addr);
	btoff = addr - (uvaddr_t)spt_ttob(btindex);
	btsize = spt_btot(btoff + size);
	saddr = addr;
	resid = size;
	ssize = MIN(resid, (SPT_BPBASETABSIZE - btoff));
	spcomp = spp->p3level_basetable + btindex;
	dpcomp = dpp->p3level_basetable + btindex;

	for (; btsize > 0; btsize--, spcomp++, dpcomp++) {
		if (spcomp && (spcomp->pcom_segptr == NULL)) {
			spcomp->pcom_segptr = pmap_segtab_alloc(0);
			spp->p3level_segtabcnt++;
		}
		if (dpcomp && (dpcomp->pcom_segptr == NULL)) {
			dpcomp->pcom_segptr = pmap_segtab_alloc(0);
			dpp->p3level_segtabcnt++;
		}
		if(spcomp && dpcomp)
			segcommon_spt_attach(pas, spcomp, dpcomp, saddr, ssize);
		resid -= ssize;
		saddr += ssize;
		ssize = MIN(resid, SPT_BPBASETABSIZE);
	}
}

STATIC void
trilevel_spt_remove(p3level_t *pp, uvaddr_t addr, size_t size)
{
	pcom_t		*pcomp;
	int		btindex;
	size_t		btsize;
	off_t		btoff;
	size_t		resid;
	uvaddr_t	saddr;
	size_t		ssize;

	btindex = spt_btott(addr);
	btoff = addr - (uvaddr_t)spt_ttob(btindex);
	btsize = spt_btot(btoff + size);
	saddr = addr;
	resid = size;
	ssize = MIN(resid, (SPT_BPBASETABSIZE - btoff));
	pcomp = pp->p3level_basetable + btindex;

	for (; btsize > 0; btsize--, pcomp++) {
		if (pcomp && pcomp->pcom_segptr)
			segcommon_spt_remove(pcomp, saddr, ssize);
		resid -= ssize;
		saddr += ssize;
		ssize = MIN(resid, SPT_BPBASETABSIZE);
	}
}

STATIC boolean_t
trilevel_spt_check(p3level_t *pp, uvaddr_t addr, size_t size)
{
	pcom_t		*pcomp;
	int		btindex;
	size_t		btsize;
	off_t		btoff;
	size_t		resid;
	uvaddr_t	saddr;
	size_t		ssize;

	btindex = spt_btott(addr);
	btoff = addr - (uvaddr_t)spt_ttob(btindex);
	btsize = spt_btot(btoff + size);
	saddr = addr;
	resid = size;
	ssize = MIN(resid, (SPT_BPBASETABSIZE - btoff));
	pcomp = pp->p3level_basetable + btindex;

	for (; btsize > 0; btsize--, pcomp++) {
		if (pcomp && segcommon_spt_check(pcomp, saddr, ssize))
			return B_TRUE;
		resid -= ssize;
		saddr += ssize;
		ssize = MIN(resid, SPT_BPBASETABSIZE);
	}
	return B_FALSE;
}

STATIC int
trilevel_spt_modify(pas_t *pas, p3level_t *pp, uvaddr_t addr,
		    size_t size, boolean_t *tlbclean, struct pm *pm)
{
	pcom_t		*pcomp;
	int		btindex;
	size_t		btsize;
	off_t		btoff;
	size_t		resid;
	uvaddr_t	saddr;
	size_t		ssize;
	int		count;

	btindex = spt_btott(addr);
	btoff = addr - (uvaddr_t)spt_ttob(btindex);
	btsize = spt_btot(btoff + size);
	saddr = addr;
	resid = size;
	ssize = MIN(resid, (SPT_BPBASETABSIZE - btoff));
	pcomp = pp->p3level_basetable + btindex;
	*tlbclean = B_FALSE;
	count = 0;

	for (; btsize > 0; btsize--, pcomp++) {
		if(pcomp) {
			count += segcommon_spt_modify(pas, pcomp, saddr, ssize, 
						      tlbclean, pm);
		}
		resid -= ssize;
		saddr += ssize;
		ssize = MIN(resid, SPT_BPBASETABSIZE);
	}
	return count;
}


STATIC boolean_t
trilevel_spt_free(pas_t *pas, p3level_t *pp, uvaddr_t addr, size_t size, int free)
{
	pcom_t		*pcomp;
	int		btindex;
	size_t		btsize;
	off_t		btoff;
	size_t		resid;
	uvaddr_t	saddr;
	size_t		ssize;
	boolean_t	tlbclean;

	btindex = spt_btott(addr);
	btoff = addr - (uvaddr_t)spt_ttob(btindex);
	btsize = spt_btot(btoff + size);
	saddr = addr;
	resid = size;
	ssize = MIN(resid, (SPT_BPBASETABSIZE - btoff));
	pcomp = pp->p3level_basetable + btindex;
	tlbclean = B_FALSE;

	for (; btsize > 0; btsize--, pcomp++) {
		if (pcomp && segcommon_spt_free(pas, pcomp, saddr, ssize, free))
			tlbclean = B_TRUE;
		resid -= ssize;
		saddr += ssize;
		ssize = MIN(resid, SPT_BPBASETABSIZE);
	}
	return tlbclean;
}
#endif	/* _MIPS3_ADDRSPACE */

/* SEGMENT level routines */

STATIC void
seg_spt_attach(pas_t *pas, pseg_t *spseg, pseg_t *dpseg, 
						uvaddr_t addr, size_t size)
{
	segcommon_spt_attach(pas, (pcom_t *)spseg, (pcom_t *)dpseg,
			     addr, size);
}
STATIC void
seg_spt_remove(pseg_t *pseg, uvaddr_t addr, size_t size)
{
	segcommon_spt_remove((pcom_t *)pseg, addr, size);
}

STATIC boolean_t
seg_spt_check(pseg_t *pseg, uvaddr_t addr, size_t size)
{
	return segcommon_spt_check((pcom_t *)pseg, addr, size);
}

STATIC int
seg_spt_modify(pas_t *pas, pseg_t *pseg, uvaddr_t addr,
	       size_t size, boolean_t *tlbclean, struct pm *pm)
{
	*tlbclean = B_FALSE;
	return segcommon_spt_modify(pas, (pcom_t *)pseg, addr, size, tlbclean, pm);
}

STATIC boolean_t
seg_spt_free(pas_t *pas, pseg_t *pseg, uvaddr_t addr, size_t size, int free)
{
	return segcommon_spt_free(pas, (pcom_t *)pseg, addr, size, free);
}

/* COMMON level routines: perform real SPT work */
/*
 * Attach to shared PT's. Copy entries from the segment
 * table of the template pmap to the segment table of
 * this process pmap. If the share PT has not been allocated
 * yet, perform an allocation first (on the template pmap)
 * and then copy the entry. Increment reference count of
 * the shared PT. 
 */
/* ARGSUSED */
STATIC void
segcommon_spt_attach(pas_t *pas, pcom_t *spcom, pcom_t *dpcom, 
						uvaddr_t addr, size_t size)
{
	uint_t	segno;
	size_t	segsz;
	pde_t	**sppt, **dppt;
	pfd_t	*pfd;
	int	s;

	/* addr and size are always aligned on segment boundary */
	segno = btosegtabindex(addr);
	segsz = btost(size);
	ASSERT(segno + segsz <= SEGTABSIZE);
	sppt = spcom->pcom_segptr + segno;
	dppt = dpcom->pcom_segptr + segno;

	/* copy entries in segment table */
	for ( ;segsz > 0; sppt++, dppt++, segno++, segsz--) {

		if (*sppt == NULL) {
			pmap_spt_ptalloc(sppt, segno);
#ifdef	SPT_DEBUG
			cmn_err(CE_CONT,"allocating SPT segno: %d, "
					"pfnum: %d\n",
					segno, kvtophyspnum(*sppt));
#endif	/* SPT_DEBUG */
		}

		ASSERT(*sppt != NULL);

		if (*dppt != NULL) {
			/*
			 * Process has already a private page table.
 			 * This situation may happen if a process had
			 * a region mapped in the same address range or
			 * if its pmap was saved during an exec. In either
			 * cases pmap_free() should have been previously
			 * called on the address range and the PT should
			 * only contain invalid entries. 
			 * Release private PT and link to shared PT.
			 */
#ifdef	SPT_DEBUG
			pmap_checkzero((char *)*dppt, NBPP);
			cmn_err(CE_CONT, "pmap_spt_attach: freeing inherited "
					 "page table pfnum %d\n",
					 kvtophyspnum(*dppt));
			ASSERT((pfntopfdat(kvtophyspnum(*dppt)))->pf_use == 1);
#endif	/* SPT_DEBUG */
			ptpool_free(*dppt, PMAPFLAG_LOCAL);
			atomicAddInt(&pmapmem, -1);
		} else {
			s = seglock();
			dpcom->pcom_segcnt++;
			segunlock(s);
		}

		/* copy entry in Segment Table */
		*dppt = *sppt;

#ifdef	SPT_DEBUG
		pmap_pt_checkvalid(*sppt);
#endif	/* SPT_DEBUG */

		/* adjust reference counts */
		pfd = pfntopfdat(kvtophyspnum(*sppt));
		/* region lock protects reference count */
		pfdat_inc_usecnt(pfd);
		pmap_spt_linkas(pfd, pas);
	}
}

/*
 * Call pmap_spt_remove on all the entries in the
 * segment table of the template pmap.  
 */

STATIC void
segcommon_spt_remove(pcom_t *pcom, uvaddr_t addr, size_t size)
{
	pde_t	**ppt;
	size_t	segsz;
	uint_t	segno;

	segno = btosegtabindex(addr);
	segsz = btost(size);
	ASSERT(segno + segsz <= SEGTABSIZE);
	ppt = pcom->pcom_segptr + segno;

	for ( ; segsz > 0; ppt++, segsz--) {
		if (*ppt == NULL)
			continue;
#ifdef	SPT_DEBUG
		cmn_err(CE_CONT, "removing SPT pfnum: %d\n",
			kvtophyspnum(*ppt));
		pmap_pt_checkvalid(*ppt);
#endif	/* SPT_DEBUG */
		pmap_spt_ptremove(*ppt);
		*ppt = NULL;
	}
}

/*
 * Check range [addr, addr + size] for shared PT.
 * Returns B_TRUE if at least one was found, B_FALSE otherwise.
 */
STATIC boolean_t
segcommon_spt_check(pcom_t *pcom, uvaddr_t addr, size_t size)
{
	uint_t		segno;
	uint_t		segoff;
	uint_t		segsz;
	pfd_t		*pfd;
	pde_t		**ppt;

	segno = btosegtabindex(addr);
	segoff = addr - (uvaddr_t)stob(btost(addr));
	segsz = btos(size + segoff);
	ASSERT(segno + segsz <= SEGTABSIZE);

	if (pcom->pcom_segptr == NULL)
		return B_TRUE;

	ppt = pcom->pcom_segptr + segno;

	for ( ; segsz > 0; ppt++, segsz--) {
		pfd = pfntopfdat(kvtophyspnum(*ppt));
#ifdef	SPT_DEBUG
		pmap_pt_checkvalid(*ppt);
#endif	/* SPT_DEBUG */
		if (pfd->pf_use > 1) {
			return B_TRUE;
		}
	}
	return B_FALSE;
}


/*
 * Make a private copy of any shared PT the range [addr, addr + size].
 * Adjust reference count on data pages. Set shared PT free if reference
 * on PT goes to 1.
 */
STATIC int
segcommon_spt_modify(pas_t *pas, pcom_t *pcom, uvaddr_t addr,
		     size_t size, boolean_t *tlbclean, struct pm *pm)
{
	uint_t		segno;
	uint_t		segoff;
	size_t		segsz;
	pfd_t		*pfd;
	pde_t		**ppt, *spt, *dpt, *spte, *dpte;
	int		count;

	segno = btosegtabindex(addr);
	segoff = addr - (uvaddr_t)stob(btost(addr));
	segsz = btos(size + segoff);
	ASSERT(segno + segsz <= SEGTABSIZE);
	count = 0;

	ppt = pcom->pcom_segptr + segno;

	for ( ; segsz > 0; segsz--, segno++, ppt++) {

		if (*ppt == NULL)
			continue;

		pfd = pfntopfdat(kvtophyspnum(*ppt));

		if (pfd->pf_use == 1)
			/* not a shared PT continue */
			continue;
		/*
		 * Allocate a private Page Table
		 */
		*tlbclean = B_TRUE;
		dpt = dpte = ptpool_alloc(pas, segno, 0, B_FALSE);
		atomicAddInt(&pmapmem, 1);
		spt = spte = *ppt;

		/* region lock protects pf_use */
		pfd->pf_use--;
		pmap_spt_unlinkas(pfd, pas);

#ifdef	SPT_DEBUG
		/* copy all valid entries */
		cmn_err(CE_CONT, "seg_spt_modify: private addr 0x%x\n",
		 	stob(btost(addr)) + stob(segno));
		pmap_pt_checkvalid(*ppt);
#endif	/* SPT_DEBUG */
		for ( ; spte < (spt + NPGPT); spte++, dpte++) {
			pfd_t	*tpfd;

			if (spte->pte.pg_pfn == 0) {
				/*
				 * Invalid page.
				 * set pte ro zero since we
				 * have not bzeroed the page
				 */
				ASSERT(!pg_isvalid(spte));
				*dpte = *spte;
				continue;
			} 

			if (!pg_isvalid(spte)) {
				/*
				 * Intransit page.
				 * At this point we have the
				 * address space lock for update.
				 * That means that the page is
				 * being faulted by another address
				 * space which implies that there is
				 * at least another user of the
				 * the shared PT.
				 * Just set the pte to zero, this
				 * will cause this address space
				 * to fault it again.
				 */
				pg_clrpgi(dpte);
				continue;
			}

			/* Page is valid */ 

			*dpte = *spte;
			count++;

			/*
			 * If this is the last user of the
			 * shared PT no need to increment
			 * the use count on the data page,
			 * in fact the shared PT will be
			 * released. Otherwise increment
			 * count.
 			 */
			if (pfd->pf_use > 1)
				pageuseinc(pdetopfdat(dpte));
			else {
				tpfd = pfntopfdat(spte->pte.pg_pfn);
				DELETE_MAPPING(tpfd, spte);
				pg_clrpgi(spte);
			}

			tpfd = pfntopfdat(dpte->pte.pg_pfn);
			RMAP_ADDMAP(tpfd, dpte, pm);
		}

		if (pfd->pf_use == 1) {
#ifdef SPT_DEBUG	
			cmn_err(CE_CONT,
				"seg_spt_modify: releasing PT: pfnum %d\n",
				pfdattopfn(pfd));
#endif	/* SPT_DEBUG */
			pmap_spt_ptfree(spt);
		}

		*ppt = dpt;
	}
	return count;
}

/*
 * Called when a shared PT has been detected in the
 * address range by pmap_spt_free().
 */

STATIC boolean_t
segcommon_spt_free(pas_t *pas, pcom_t *pcom, uvaddr_t addr, size_t size, int free)
{
	uint_t		segno;
	size_t		segsz;
	uint_t		segoff;
	pfd_t		*pfd;
	pde_t		**ppt;
	int		s;
	boolean_t	tlbclean;

	tlbclean = B_FALSE;
	segno = btosegtabindex(addr);
	segoff = addr - (uvaddr_t)stob(btost(addr));
	segsz = btos(size + segoff);
	ASSERT(segno + segsz <= SEGTABSIZE);
	ASSERT(btost(addr) == btos(addr) &&
	       btost(addr + size) == btos(addr + size));

	ppt = pcom->pcom_segptr + segno;

	for ( ; segsz > 0; segno++, ppt++, segsz--) {

		if (*ppt == NULL)
			continue;
#ifdef	SPT_DEBUG
		pmap_pt_checkvalid(*ppt);
#endif	/* SPT_DEBUG */

		pfd = pfntopfdat(kvtophyspnum(*ppt));

		if (pfd->pf_use == 1) {
			/*
			 * Not a shared page table,
			 * go through normal path
			 */
			(void)segcommon_free(pas, *ppt, NPGPT, free);
			continue;
		}

		/* shared page table */

		/* region lock protects pf_use */
		if (pfd->pf_use == 2) {
			/*
			 * Last user. Set pages free.
			 */
			(void)segcommon_free(pas, *ppt, NPGPT, free);
		}
		pfd->pf_use--;
		pmap_spt_unlinkas(pfd, pas);
		tlbclean = B_TRUE;

		if (pfd->pf_use == 1) {

#ifdef	SPT_DEBUG
			cmn_err(CE_CONT, "freeing SPT pfnum: %d\n",
				pfdattopfn(pfd));
#endif	/* SPT_DEBUG */

			/*
			 * Release page table
			 */
			pmap_spt_ptfree(*ppt);
		}

		s = seglock();
		pcom->pcom_segcnt--;
		segunlock(s);

		*ppt = NULL;
	}
	return tlbclean;
}

/*
 * Allocate a new shared PT. Ppt is a pointer
 * to a segment table entry of the template
 * pmap. Also save ppt in the pf_sppt field.
 */

STATIC void
pmap_spt_ptalloc(pde_t **ppt, uint_t segno)
{
	pfd_t	*pfd;
	pde_t	*pte;

	pte = ptpool_alloc(NULL, segno, 0, B_FALSE);
	atomicAddInt(&pmapmem, 1);
	pfd = pfntopfdat(kvtophyspnum(pte));
	pfd->pf_sppt = (void *)ppt;
	*ppt = pte;
}

/*
 * Called when the descriptor is removed.
 * Clean pf_sppt field, since the template
 * pmap is being removed.
 */

STATIC void
pmap_spt_ptremove(pde_t *pte)
{
	pfd_t	*pfd;

	pfd = pfntopfdat(kvtophyspnum(pte));
	pfd->pf_sppt = NULL;
}

/*
 * Release shared PT to kernel pool. Clean
 * pf_sppt pointer in the pfd and null the
 * pointer to this page from the segment table
 * of the template pmap.
 */
STATIC void
pmap_spt_ptfree(pde_t *pte)
{
	pfd_t	*pfd;
	pde_t	**sppt;

	pfd = pfntopfdat(kvtophyspnum(pte));
	ASSERT(pfd->pf_use == 1);
#ifdef FUTURE
	ASSERT(pfd->pf_pas == NULL);
#endif
	if ((sppt = (pde_t **)pfd->pf_sppt) != NULL) {
		pfd->pf_sppt = NULL;
		*sppt = NULL;
	}
	
	ptpool_free(pte, PMAPFLAG_LOCAL);
	atomicAddInt(&pmapmem, -1);
}

/*
 * Called to apply input function on all address spaces using the
 * pte. No protection against the "owning" pas being switched
 * underneath - as during exec with tsaved regions, or during
 * exit, when the pas is being torn down; or the underlying pas
 * fields from changing - as in cpr restart. The aspacelock might 
 * provide protection in most cases. The "owning" job is better
 * protected.
 */
int
pmap_pte_scan(pde_t *pte, int (*func)(void *, void *data1, void *data2), 
				void *arg1, void *arg2, int flags)
{
	void	*obj;
	pfd_t	*pfd = pfntopfdat(kvtophyspnum(pte));
	int	stop_scan;

	if (pfd->pf_use == 1) {
		/*
		 * Private page table.
		 */
		switch(flags) {
			case JOB_SCAN:
				obj = GET_JOB_FOR_PT(pte);
				break;
			case AS_SCAN:
				cmn_err(CE_PANIC, "Using unimplemented function in the kernel");
				obj = GET_AS_FOR_PT(pte);
				break;
			default:
				ASSERT(0);
				break;
		}
		stop_scan = (*func)(obj, arg1, arg2);
		return (stop_scan);
	} else {
		/*
		 * Shared page table.
		 * Currently not used, hence returns indicator to keep
		 * scanning. 
		 */
		return(0);
	}
}

#ifdef FUTURE
/*
 * The singly linked list of ASs using a shared PT is protected by the 
 * shared memory region lock.
 */
typedef struct as_info {
	struct as_info	*as_next;
	pas_t		*pt_pas;
} as_info_t;

#define pf_last pf_hchain
	
static void
pmap_spt_linkas(pfd_t *pfd, pas_t *pas)
{
	as_info_t	*ptr = (as_info_t *)kmem_alloc(sizeof(as_info_t), 
							KM_SLEEP);

	ptr->pt_pas = pas;
	ptr->as_next = NULL;
	ASSERT(pfd->pf_use > 1);
	if (pfd->pf_pas == NULL) {
		pfd->pf_pas = pfd->pf_last = (void *)ptr;
	} else {
		((as_info_t *)(pfd->pf_last))->as_next = ptr;
		pfd->pf_last = (void *)ptr;
	}
}

static void
pmap_spt_unlinkas(pfd_t *pfd, pas_t *pas)
{
	as_info_t	*current = pfd->pf_pas, *prev = NULL;

	ASSERT((pfd->pf_pas != NULL) && (pfd->pf_last != NULL));
	while (current->pt_pas != pas) {
		prev = current;
		current = current->as_next;
		ASSERT(current != NULL);
	}
	if (prev == NULL) {		/* first element in chain matches */
		pfd->pf_pas = (void *)(current->as_next);
		if ((void *)(pfd->pf_last) == (void *)current)
			pfd->pf_last = NULL;	/* single element chain */
	} else {
		if ((void *)(pfd->pf_last) == (void *)current)
			pfd->pf_last = (void *)prev; /*last element deletion*/
		prev->as_next = current->as_next;
	}
	kmem_free((void *)current, sizeof(as_info_t));
}
#endif
