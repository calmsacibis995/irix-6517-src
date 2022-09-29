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
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 3.413 $"

#include <sys/types.h>
#include <sys/anon.h>
#include <ksys/as.h>
#include "as_private.h"
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/edt.h>
#include <sys/errno.h>
#include <sys/getpages.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/lock.h>
#include <sys/lpage.h>
#include <sys/mman.h>
#include <sys/numa.h>
#include <sys/nodemask.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/prctl.h>
#include "pmap.h"
#include "fetchop.h"
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/uthread.h>
#include <sys/var.h>
#include <ksys/vfile.h>
#include <sys/vmereg.h>
#include <sys/vnode.h>
#include <ksys/vpag.h>
#include <ksys/vm_pool.h>
#include <ksys/vmmacros.h>
#include <sys/miser_public.h>
#include <os/vm/scache.h>
#include <sys/nodepda.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */

static preg_t	*allocpreg(reg_t *, caddr_t, pgno_t, pgno_t, uchar_t, uchar_t, 
			   uchar_t, ushort, struct pm *);
static void	freepreg(pas_t *, ppas_t *,preg_t *, caddr_t, pgno_t, int);
static void	detachpreg(pas_t *, ppas_t *, preg_t *);
static int	chkgrowth(preg_t *, pas_t *, ppas_t *, caddr_t, caddr_t, boolean_t);
static void	chgpteprot(preg_t *, char *, pgno_t, int);
static void	attr_split(attr_t *, char *);
static void	attr_coalesce(preg_t *);
static void	attr_dup(preg_t *, preg_t *);
int		copy_locked_page(preg_t *, attr_t *, preg_t *,
			caddr_t, pde_t *, pde_t *, pas_t *);


extern void	init_anon(void);
extern void 	init_vnode_pcache(void);
extern int	madv_detach(reg_t *);

#define mapattach(prp,vp) \
		(prp)->p_vchain = (vp)->v_mreg; \
		(prp)->p_vchainb = (preg_t *)(vp); \
		(vp)->v_mreg = prp; \
		if ((prp)->p_vchain == (preg_t *)(vp)) \
			(vp)->v_mregb = (prp); \
		else \
			(prp)->p_vchain->p_vchainb = (prp);


/* use zone allocators for quick allocations.  */
struct zone *preg_zone;
static struct zone *attr_zone;
static struct zone *reg_zone;

/*
 * To help in randomizing the vcache - we add a uniqifier. Since we use
 * the zone allocator, we will tend to re-use the same virtual
 * address over and over. To help uniqify them, we have a gen number.
 * This gen # need not be protected ..
 */
static unsigned long reggen;

#define next_rp(rp)	((reg_t *)(rp)->r_anon)

mutex_t	sanon_lock;	/* used to prevent races between freeing sanon */
			/* regions and vhand swapping them out */

/*
 * pregion avl-tree operations.
 */

static __psunsigned_t
preg_avl_start(avlnode_t *np)
{
	return ((__psunsigned_t) (((preg_t *)np)->p_regva));
}

static __psunsigned_t
preg_avl_end(avlnode_t *np)
{
	return ((__psunsigned_t) (((preg_t *)np)->p_regva
		     + ctob(((preg_t *)np)->p_pglen)));
}

avlops_t preg_avlops = {
	preg_avl_start,
	preg_avl_end,
};

/*
 * Routine to initialize the sysreg data structure. 
 * Initialize most of the fields that don't depend on
 * other initializations (like mutex etc) but enough
 * to let kvpalloc/kvpfree work early enough. 
 */
void
sysreg_earlyinit(void)
{

	pde_t	pde;
	sysreg.r_type = RT_MEM;
	sysreg.r_pgsz = syssegsz;
	sysreg.r_swapres = 0;
	syspreg.p_reg = &sysreg;
	syspreg.p_nvalid = 0;
	syspreg.p_noshrink = 0;
	syspreg.p_nofree = 0;
	syspreg.p_fastuseracc = 0;
	syspreg.p_maxprots = PROT_RWX;
	syspreg.p_attrs.attr_prot = PROT_RWX;
	syspreg.p_attrs.attr_start = (char *)K2SEG; /* p_regva */
	syspreg.p_attrhint = &syspreg.p_attrs;
	syspreg.p_attrs.attr_end = syspreg.p_attrs.attr_start + ctob(syssegsz);
	pg_setcachable(&pde);
	syspreg.p_attrs.attr_cc = pde.pte.pte_cc;
	syspreg.p_flags = 0;

}

void
reginit(void)
{

	(void)init_pmap();
	(void)init_anon();
	(void)init_vnode_pcache();

	/*
	 * Allocate sysreg accoutrements.
	 * Sysreg does not get linked into pactive chain
	 * because it shouldn't get aged and it is
	 * considered separately for getpages/tosspages.
	 * The nvalid field of syspreg shows the number of
	 * valid, kvswappable pages.
	 */
	/*
	 * Some parts of sysreg gets initialized early enough
	 * to let kvpalloc/kvpfree work fine. Do the reset
	 * of initialization primarily used in kvswap 
	 * interface 
	 */
	mrinit(&sysreg.r_lock, "sysreg");
	sysreg.r_anon = anon_new();
	sysreg.r_flags = RG_ANON;


	/*	Horrific cheating here... Direct pointer to ptes.
	*/
	syspreg.p_pmap = (pmap_t *)kptbl;

	preg_zone = kmem_zone_init(sizeof(preg_t), "pregions");
	ASSERT(preg_zone);
	attr_zone = kmem_zone_init(sizeof(attr_t), "attributes");
	ASSERT(attr_zone);
	reg_zone = kmem_zone_init(sizeof(reg_t), "regions");
	ASSERT(reg_zone);

	mutex_init(&sanon_lock, MUTEX_DEFAULT, "sanon");
}

#ifdef DEBUG
static int
ismemberprp(preg_set_t *pset, preg_t *prp)
{
	preg_t *nprp;
	for (nprp = PREG_FIRST(*pset); nprp; nprp = PREG_NEXT(nprp)) {
		if (prp == nprp)
			return 1;
	}
	return 0;
}
#endif

/*
 * Allocate a pregion entry.
 */
/* ARGSUSED8 */
static preg_t *
allocpreg(
	reg_t *rp,
	caddr_t regva,
	pgno_t offset,
	pgno_t len,
	uchar_t type,
	uchar_t prots,
	uchar_t maxprots,
	ushort flags,
	struct pm *pm)
{
	register preg_t	*prp;
	pde_t pde;

	ASSERT(mrislocked_update(&rp->r_lock));
	ASSERT((flags & (PF_SHARED|PF_PRIVATE)) != (PF_SHARED|PF_PRIVATE));

	prp = kmem_zone_alloc(preg_zone, KM_SLEEP);
	ASSERT(prp);

	prp->p_reg = rp;
	prp->p_regva = regva;
	prp->p_offset = offset;
	prp->p_pglen = len;
	prp->p_flags = flags;
	prp->p_type = type;
	prp->p_nvalid = 0;
	prp->p_noshrink = 0;
	prp->p_nofree = 0;
	prp->p_fastuseracc = 0;
	prp->p_pghnd = 0;
	/* prp->p_pmap = NULL; always set by caller... */
	prp->p_stk_rglen = rp->r_pgsz;
  
	prp->p_attrhint = &prp->p_attrs;
	prp->p_attrs.attr_attrs = 0;
	prp->p_maxprots = maxprots;
	prp->p_attrs.attr_prot = prots;
	prp->p_attrs.attr_next = (attr_t *)NULL;
	prp->p_attrs.attr_start = regva;
	prp->p_attrs.attr_end = regva + ctob(len);

        /*
         * Set initial policy 
         */
        attr_pm_set(&prp->p_attrs, pm);

	/*	Set the initial cache state for the region.
	 */
	pde.pgi = 0;
	if (rp->r_flags & RG_PHYS)
		pg_setcache_phys(&pde);
	else
		pg_setcachable(&pde);

	prp->p_attrs.attr_cc = pde.pte.pte_cc;

	return(prp);
}


/*
 * Free all or part of a pregion table entry.  If we're freeing a piece out
 * of the middle, then we split the pregion into two pieces to cover the
 * remaining ends; see detachpreg for flags (only RF_TSAVE is used here).
 */
static void
freepreg(
	pas_t		*pas,
	ppas_t		*ppas,
	register preg_t *prp,
	register caddr_t vaddr,
	register pgno_t	 len,
	register int 	 flags)
{
	register preg_t	*nprp;
	register attr_t *attr, *attr_next;
	register reg_t	*rp;
	register caddr_t end;
	register pgno_t	valid;
	vnode_t *vp;
	vhandl_t vt;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	rp = prp->p_reg;
	ASSERT(rp);
	ASSERT(prp->p_pmap);
	vp = rp->r_vnode;

	end = vaddr + ctob(len);

	/* SPT: if pmap is using Shared Page Tables, p_nvalid is not accurate */
	ASSERT(pmap_spt(prp->p_pmap) ||
	       count_valid(prp, prp->p_regva, prp->p_pglen) == prp->p_nvalid);
	

	ASSERT(prp->p_nvalid >= 0);	/* SPT */

	/*
	 * SPT
	 * If we are unmapping part of a segment, check for shared PT's.
	 * (see comments in pmap.c)
	 */
	if (vaddr != prp->p_regva || end != prp->p_regva + ctob(prp->p_pglen))
		prp->p_nvalid += pmap_modify(pas, ppas, 
					prp, vaddr, len);

	ASSERT(prp->p_nvalid >= 0);	/* SPT */

	/*
	 * If we are unmapping any large pages then make sure
	 * that their buddies are broken up to smaller
	 * pages. If unmapping an entire region nothing needs to be done
	 * as large pages do not cross regions. 
	 */
	if (!((vaddr == prp->p_regva) && (len == prp->p_pglen))) {
		pmap_downgrade_addr_boundary(pas, prp, vaddr, len, 
			((flags & RF_NOFLUSH) ? PMAP_TLB_FLUSH : 0));
	}

	/*
	 * If we're taking a piece out of the middle of a pregion, set up
	 * a new pregion struct for the end piece.
	 */
	if (vaddr != prp->p_regva && 
	    end != prp->p_regva + ctob(prp->p_pglen)) {


		nprp = kmem_zone_alloc(preg_zone, KM_SLEEP);
		ASSERT(nprp);
	
		nprp->p_attrhint = &nprp->p_attrs;
		nprp->p_reg = rp;
		rp->r_refcnt++;
		nprp->p_regva = end;
		nprp->p_offset = prp->p_offset + 
					btoc(nprp->p_regva - prp->p_regva);
		nprp->p_stk_rglen = prp->p_stk_rglen;
		nprp->p_pglen = prp->p_pglen -
					btoc(nprp->p_regva - prp->p_regva);
		nprp->p_maxprots = prp->p_maxprots;
		nprp->p_type = prp->p_type;
		nprp->p_flags = prp->p_flags;
		nprp->p_noshrink = 0;
		nprp->p_nofree = 0;
		nprp->p_fastuseracc = 0;
		nprp->p_pghnd = 0;
		nprp->p_pmap = prp->p_pmap;

		if (vp) {
			register int s;

			s = mutex_spinlock(&mreg_lock);
			mapattach(nprp, vp);
			mutex_spinunlock(&mreg_lock, s);
		} else {
			nprp->p_vchain =
			nprp->p_vchainb = NULL; /* sanity */
		}

	} else
		nprp = NULL;


	/* XXX	Isn't this a little coarse? */
	unlockpreg(pas, ppas, prp, vaddr, len, RF_EXPUNGE); 

	/*
	 * If there's an vnode, write out modified pages
	 * and unlink prp from inode chain if freeing the whole prp.
	 */
	if (vp) {
		/* Remove from inode chain if nuking whole pregion */
		if (prp->p_pglen == len) {
			register int s;

			s = mutex_spinlock(&mreg_lock);
			if (prp->p_vchainb == (preg_t *)vp)
				vp->v_mreg = prp->p_vchain;
			else
				prp->p_vchainb->p_vchain = prp->p_vchain;

			if (prp->p_vchain == (preg_t *)vp)
				vp->v_mregb = prp->p_vchainb;
			else
				prp->p_vchain->p_vchainb = prp->p_vchainb;

			mutex_spinunlock(&mreg_lock, s);
		}

		switch(vp->v_type) {
		case VREG:
			/*
			 * if not RT_MAPFILE can't have any non-anon dirty pages
			 * Note however that the vnode may have dirty pages
			 * (say someone copied over an executable - the data
			 * region is RT_MEM, but the vp may still have pages not
			 * written out). Thus we ignore non-RT_MAPFILE
			 * The maxprot check save some time - but basically
			 * if there is anyway that there might be a dirty page
			 * in this pregion, we need to call getmpages to push
			 * that to the delwri cache.
			 */
			if (rp->r_type == RT_MAPFILE && 
			    (prp->p_maxprots & PROT_W) &&
			     (!(rp->r_flags & RG_TEXT)))
				getmpages(pas, prp, 0, vaddr, len);
			break;

		case VBLK:
		case VCHR:
			/*
			 * Inform driver that it is being unmapped
			 * note - this won't be called if unmapping
			 *	  part of a region since we will have
			 * 	  either bumped the refcnt above,
			 *	  or the p_pglen count won't == 'len'.
			 */
			if (   (prp->p_pglen     == len)
			    && (rp->r_refcnt     == 1)
			    && (rp->r_mappedsize  > 0) ) {
				/*REFERENCED*/
				int unused;

				vt.v_preg = prp;
				vt.v_addr = prp->p_regva;
				VOP_DELMAP(vp, &vt, rp->r_mappedsize,
						get_current_cred(), unused);
			}
			break;
		}
	}


	/* Free up pages associated with prp. */
	if ((rp->r_flags & RG_PHYS) == 0) {
		int free = (rp->r_flags & RG_HASSANON) ? PMAPFREE_SANON
						       : PMAPFREE_FREE;
		valid = pmap_free(pas, prp->p_pmap, vaddr, len, free);
		prp->p_nvalid -= valid;
	} else {
		valid = pmap_free(NULL, prp->p_pmap, vaddr, len,
						PMAPFREE_TOSS);
		prp->p_nvalid -=  valid;
	}

	if ((prp->p_flags & PF_NOPTRES) == 0)
		pmap_unreserve(prp->p_pmap, pas, ppas, vaddr, len);

	ASSERT(prp->p_nvalid >= 0);
	/* SPT: if pmap is using Shared Page Tables, p_nvalid is not accurate */
	ASSERT(pmap_spt(prp->p_pmap) || prp->p_nvalid <= prp->p_pglen - len);

	/*
	 *	If we're deleting from the beginning, adjust the offset
	 *	into the region.
	 */
	if (prp->p_regva == vaddr)
		prp->p_offset += len;

	/*
	 * Isolate the attr structs to be deleted.  Any ones before these will
	 * stay in prp.  The ones after will get moved to nprp.
	 */
	attr_next = findattr_range(prp, vaddr, end);
	prp->p_attrhint = &prp->p_attrs;

	/*
	 * If we're not deleting the first one, find the one before attr_next
	 * so we can NULL terminate the list.
	 */
	if (attr_next != &prp->p_attrs) {
		for (attr = &prp->p_attrs; attr->attr_next != attr_next; 
		     attr = attr->attr_next)
			;

		attr->attr_next = NULL;
	}

	/*
	 * Now free attributes starting at vaddr for len pages.
	 */
	attr = attr_next;

	while (attr && end >= attr->attr_end) {
		ASSERT(attr->attr_lockcnt == 0);
		/*
		 * With shared address procs, its easy to get
		 * watchpoints in a region we're detaching. This
		 * doesn't seem to do any harm though ..
		ASSERT(attr->attr_watchpt == 0);
		 */
		attr_next = attr->attr_next;

                attr_pm_unset(attr);
                
		if (attr != &prp->p_attrs)
			kmem_zone_free(attr_zone, attr);

		attr = attr_next;
	}

	/*
	 * Remaining attributes belong to the new pregion if we were taking
	 * a piece out of the middle of the original pregion.  Otherwise
	 * we must have been unmapping off the front of of the pregion.  Note
	 * that the use of structure copy maintains the linked list of any
	 * attribute structs after attr.
	 */
	if (attr != NULL) {
		ASSERT(attr->attr_start == end);
		ASSERT(attr != &prp->p_attrs);

		if (nprp) {
			nprp->p_attrs = *attr;	/* struct copy */
                } else {
			prp->p_attrs = *attr;	/* struct copy */
                }

		kmem_zone_free(attr_zone, attr);
	} else {
		ASSERT(nprp == NULL);
	}

	/* Some pmap tear down is required for private mappings */
	if ((prp->p_flags & PF_PRIVATE) && (pas->pas_flags & PAS_SHARED)) {
		ASSERT(prp->p_pmap == ppas->ppas_pmap);
		pmap_lclteardwn(prp->p_pmap, vaddr, len);
	} else
		ASSERT(prp->p_pmap == pas->pas_pmap);

#ifdef _SHAREII
	/*
	 *	Where the virtual address space of 
	 *	a process actually changes, ShareII 
	 *	needs to update its per-lnode values.
	 */
	SHR_LIMITMEMORY(ctob(len), (LI_UPDATE | LI_FREE));
#endif /* _SHAREII */

	/* Adjust lengths. */
	prp->p_pglen -= len;

	if (!(flags & RF_TSAVE)) {
		mutex_lock(&pas->pas_preglock, 0);
		if (prp->p_flags & PF_PRIVATE) {
			if ((rp->r_flags & RG_PHYS) == 0) {
				ppas->ppas_size -= len;
				ASSERT(ppas->ppas_size >= 0);
			} else {
				ppas->ppas_physsize -= len;
				ASSERT(ppas->ppas_physsize >= 0);
			}
		} else {
			if ((rp->r_flags & RG_PHYS) == 0) {
				pas->pas_size -= len;
				ASSERT(pas->pas_size >= 0);
			} else {
				pas->pas_physsize -= len;
				ASSERT(pas->pas_physsize >= 0);
			}
		}
		mutex_unlock(&pas->pas_preglock);
	}

	if (nprp)
		prp->p_pglen -= nprp->p_pglen;

	/*	Unlink prp from pregion chain if we're 
	 *	free'ing the whole pregion.
	 */
	if (prp->p_pglen == 0) {
		int		s, freeit;
		ASSERT(nprp == NULL);

		/* Remove from process pregion list. */
		if (flags & RF_TSAVE)
			/* in save list, do not call detachpreg */
			;
		else {
			detachpreg(pas, ppas, prp);
		}
		
		/*
		 * There might be a remapf active scanning the p_vchain
		 * list. Clear the p_reg pointer - this indicates the
		 * entry is no longer connected to a valid region. If
		 * the p_nofree count is zero here, we can free the 
		 * structure. Otherwise, remapf will free it when it 
		 * detects the pregion entry is no longer valid.
		 */
		s = mutex_spinlock(&mreg_lock);
		ASSERT(prp->p_reg);
		prp->p_reg = NULL;
		freeit = (prp->p_nofree == 0);
		mutex_spinunlock(&mreg_lock, s);
	
		if (freeit)
			kmem_zone_free(preg_zone, prp);
		rp->r_refcnt--;
		return;
	}

	prp->p_flags &= ~PF_NOPTRES;

	/*
	 *	Otherwise, if we've split prp, then we must re-calculate the
	 *	values for p_nvalid in each pregion and link the new prp onto
	 *	the pregion list.
	 */
	if (nprp) {
		/*
		 * Recompute p_nvalid for the smaller of the two pregions.
		 * The value for the other can then be inferred from this one.
		 */
		if (prp->p_pglen < nprp->p_pglen) {
			valid = count_valid(prp, prp->p_regva, prp->p_pglen);
			nprp->p_nvalid = prp->p_nvalid - valid;
			prp->p_nvalid = valid;
		} else {
			valid = count_valid(nprp, nprp->p_regva, nprp->p_pglen);
			prp->p_nvalid -= valid;
			nprp->p_nvalid = valid;
		}
		ASSERT(prp->p_nvalid >= 0);
		ASSERT(nprp->p_nvalid >= 0);

		/*
		 * Insert the newly allocated nprp, which was split from
		 * prp into appropriate set: into private if was private before,
		 * or into shaddr if was shared before.
		 */
		mutex_lock(&pas->pas_preglock, 0);
		if (nprp->p_flags & PF_PRIVATE)
			PREG_INSERT(&ppas->ppas_pregions, nprp);
		else
			PREG_INSERT(&pas->pas_pregions, nprp);
		mutex_unlock(&pas->pas_preglock);
	}
}

static void
detachpreg(pas_t *pas, ppas_t *ppas, preg_t *prp)
{
	mutex_lock(&pas->pas_preglock, 0);
	if (prp->p_flags & PF_PRIVATE) {
		ASSERT(ismemberprp(&ppas->ppas_pregions, prp));
		PREG_DELETE(&ppas->ppas_pregions, prp);
	} else {
		ASSERT(ismemberprp(&pas->pas_pregions, prp));
		PREG_DELETE(&pas->pas_pregions, prp);
	}
	mutex_unlock(&pas->pas_preglock);
}

/*
 * Detach pregion from proc chain and clean it up.
 * Used by elfexec to keep pregions on local lists
 * for possible reattachment (when execing a.out with same sections).
 *
 * NUMA_PM - leave old pm attached - when the region is reattached the
 * pm will be replaced.
 */
#ifdef _SHAREII
/*
 * There is no hook in this routine, because memory detached here is 
 * saved on pas_save by its caller.
 * It will either by reattached in reattachpreg() below (no hook there either)
 * or freed by remove_tsave() which eventually calls vdetachreg()  where 
 * there *is* a share hook.
 * The aim is to avoid double counting the memory while it is on the tsave list.
 */
#endif /* _SHAREII */
void
unattachpreg(pas_t *pas, ppas_t *ppas, preg_t *prp)
{
	pgcnt_t len = prp->p_pglen;
        
	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	unlockpreg(pas, ppas, prp, prp->p_regva, len, RF_EXPUNGE);

	pmap_unreserve(prp->p_pmap, pas, ppas, prp->p_regva, len);
	prp->p_flags |= PF_NOPTRES;

	mutex_lock(&pas->pas_preglock, 0);
	if (prp->p_flags & PF_PRIVATE) {
		if ((prp->p_reg->r_flags & RG_PHYS) == 0) {
			ppas->ppas_size -= len;
			ASSERT(ppas->ppas_size >= 0);
		} else {
			ppas->ppas_physsize -= len;
			ASSERT(ppas->ppas_physsize >= 0);
		}
		ASSERT(ismemberprp(&ppas->ppas_pregions, prp));
		PREG_DELETE(&ppas->ppas_pregions, prp);
	} else {
		if ((prp->p_reg->r_flags & RG_PHYS) == 0) {
			pas->pas_size -= len;
			ASSERT(pas->pas_size >= 0);
		} else {
			pas->pas_physsize -= len;
			ASSERT(pas->pas_physsize >= 0);
		}
		ASSERT(ismemberprp(&pas->pas_pregions, prp));
		PREG_DELETE(&pas->pas_pregions, prp);
	}
	mutex_unlock(&pas->pas_preglock);
}

/*
 * An exec has found a match between a saved pregion and a loadable section.
 * Reattch the pregion to the process' list.
 * XXX Test for overlap?  Assert no overlap?
 */
/* ARGSUSED3 */
int
reattachpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, struct pm *pm)
{
        attr_t *attr;

	if (chkgrowth(prp, pas, ppas, prp->p_regva,
			prp->p_regva + ctob(prp->p_pglen), B_FALSE))
		return ENOMEM;

	if (pmap_reserve(prp->p_pmap, pas, ppas, prp->p_regva, prp->p_pglen))
		return EAGAIN;

	prp->p_flags &= ~PF_NOPTRES;

	/*
	 * Insert prp on the appropriate pregion list in address order.
	 */
	mutex_lock(&pas->pas_preglock, 0);
	if (prp->p_flags & PF_PRIVATE) {
		if ((prp->p_reg->r_flags & RG_PHYS) == 0)
			ppas->ppas_size += prp->p_pglen;
		else
			ppas->ppas_physsize += prp->p_pglen;
		ASSERT(ppas->ppas_pmap == NULL || prp->p_pmap == ppas->ppas_pmap);
		PREG_INSERT(&ppas->ppas_pregions, prp);
	} else {
		if ((prp->p_reg->r_flags & RG_PHYS) == 0)
			pas->pas_size += prp->p_pglen;
		else
			pas->pas_physsize += prp->p_pglen;
		ASSERT((prp->p_flags & PF_SHARED) || prp->p_pmap == pas->pas_pmap);
		PREG_INSERT(&pas->pas_pregions, prp);
	}

        for (attr = &prp->p_attrs; attr != NULL; attr = attr->attr_next) {
                attr_pm_unset(attr);
                attr_pm_set(attr, pm);
	}
        
	mutex_unlock(&pas->pas_preglock);

	return 0;
}


/*
 * Allocate a new region.
 * Always returns a locked region pointer; cannot fail.
 */
reg_t *
allocreg(struct vnode *vp, uchar_t type, ushort flags)
{
	register reg_t	*rp;
#ifdef PDEBUG
	static int alloc_anon = 1;
#endif


	/*
	 * Allocate a zeroed out region and initialize non-zero values.
	 */
	rp = kmem_zone_zalloc(reg_zone, KM_SLEEP);
	ASSERT(rp);

	/* Initialize region fields and bump vnode reference
	 * count.  Vnode is locked by the caller.
	 */
	rp->r_type = type;
	rp->r_vnode = vp;
	rp->r_flags = flags;
	rp->r_color = NOCOLOR;
	reggen += 13;
	rp->r_gen = reggen;
	mrinit(&rp->r_lock, "region");
	reglock(rp);

	if (vp != NULL)
		VN_HOLD(vp);

	if (flags & RG_ANON) {
		ASSERT((flags & RG_PHYS) == 0);
		rp->r_anon = anon_new();
		ASSERT(rp->r_anon);
#ifdef PDEBUG
		if (alloc_anon)
			pfindanyanon(rp->r_anon);
#endif /* PDEBUG */
	}
#ifdef CKPT
	rp->r_ckptflags = 0;
	rp->r_ckptinfo = -1;
#endif
	return(rp);
}

/*
 * Free an unused region table entry.  It must be possible to call this
 * routine multiple times for the same region in the case where we're
 * racing with remapf (see below).
 */
void
freereg(pas_t *pas, reg_t *rp)
{
	vnode_t *vp;
	extern void usync_cleanup(caddr_t);

	ASSERT(mrislocked_update(&rp->r_lock));
	ASSERT(rp->r_refcnt == 0);

	vp = rp->r_vnode;
	if (vp) {
		VN_RELE(vp);
		rp->r_vnode = NULL;
	}

	if (rp->r_flags & RG_LOADV) {
		ASSERT(rp->r_loadvnode);
		VN_RELE(rp->r_loadvnode);
		rp->r_flags &= ~RG_LOADV;
		rp->r_loadvnode = NULL;
	}

	if ((rp->r_flags & (RG_PHYS|RG_ANON)) == RG_ANON) {
		if (rp->r_spans)
			rp->r_swapres -= madv_detach(rp);

		if (pas) 
			pas_unreservemem(pas, rp->r_swapres, 0);
		else 
			unreservemem(GLOBAL_POOL, rp->r_swapres, 0, 0);

		if (rp->r_flags & RG_HASSANON) {

			/*
			 * Make sure vhand isn't swapping sanon pages while
			 * we're trying to free them.  We use one global lock
			 * to cover all sanon regions since vhand doesn't
			 * have access to the individual region locks.  It
			 * only has a list of unreferenced sanon pages.
			 */

			mutex_lock(&sanon_lock, PZERO);
			anon_free(rp->r_anon);
			mutex_unlock(&sanon_lock);
		} else
			anon_free(rp->r_anon);

		rp->r_anon = NULL;
		rp->r_flags &= ~RG_ANON;
	} else 
		ASSERT(rp->r_anon == 0);

	rp->r_type = RT_UNUSED;

	/*
	 * Check to see if we're racing with someone in remapf.  They
	 * could have found the region on the vnode list and might be
	 * waiting for the region lock.  If so, we can't actually free
	 * the region here since it could get recycled to a new use
	 * before they run.  In these cases, we just unlock the region
	 * and let remapf free it.  We don't need to grab the mreg_lock 
	 * before checking since it's guaranteed that anyone who found
	 * the region while it was on the vnode chain has already
	 * incremented the r_nofree field.  The pregion was taken
	 * off the vnode list back in freepreg, so no new processes
	 * came along.  Note that there could be multiple processes in
	 * remapf for the same vnode at the same time.  This means there
	 * may be more than one process waiting for the region lock,
	 * which means they'll run one at a time and the last one will
	 * free the region.
	 */

	if (rp->r_nofree) {			/* someone's in remapf */
		regrele(rp);
		return;
	}

	/*
	 * If the region's aspace represents any usync objects, call
	 * the usync module to clean-up the usync state.
	 */
	if (rp->r_flags & RG_USYNC)
		usync_cleanup((caddr_t) rp);

	mrfree(&rp->r_lock);
	kmem_zone_free(reg_zone, rp);
}

/*
 * Attach a region to a process' address space.
 * NOTE: all attaches to shared process space MUST
 * be single threaded using aspacelock(UPDATE)
 */
int
vattachreg(
	register reg_t	*rp,	/* pointer to region to be attached */
	pas_t		*pas,
	ppas_t		*ppas,
	register caddr_t vaddr,	/* virtual address at which to attach */
	pgno_t		offset, /* offset into region where pregion begins */
	pgno_t		len,	/* len of pregion */
	uchar_t		type,	/* Type to make the pregion. */
	uchar_t		prots,	/* pregion protections */
	uchar_t		maxprots,/* pregion maximum protections */
	ushort		pflags,	/* dup flags for pregion entries. */
	struct pm       *pm,    /* policy module */
	preg_t		**prpp)	/* result parameter */
{
	register preg_t *prp;
	register preg_set_t *prsetp;
	vnode_t *vp;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	ASSERT(mrislocked_update(&rp->r_lock));
	ASSERT(offset + len <= rp->r_pgsz);

	if (poff(vaddr))
		return EINVAL;

	/*
	 * Determine which list new pregion should go on.
	 * PF_NOSHARE regions always go on private list as do replicatable
	 * text.
	 */
	if ((pflags & PF_NOSHARE) ||
			(REG_REPLICABLE(rp) &&
			 ((prots & (PROT_X|PROT_W)) == PROT_X)))
		pflags |= PF_PRIVATE;
	/*
	 * If the pregion is going on the shared list and the AS is shared
	 * then mark the pregion as PF_SHARED.
	 */
	if (!(pflags & PF_PRIVATE) && (pas->pas_flags & PAS_SHARED))
		pflags |= PF_SHARED;

	/* Allocate a pregion */
	prp = allocpreg(rp, vaddr, offset, len, type, prots, maxprots,
			pflags, pm);
	ASSERT(prp);

	/*
	 * Check that region does not go beyond end of virtual
	 * address space. We permit the new pregion to overlap existing
	 * pregions if the new pregion is private and non-shared.
	 */
	if (chkgrowth(prp, pas, ppas, prp->p_regva,
				prp->p_regva + ctob(prp->p_pglen), B_TRUE)) {
                attr_pm_unset(&prp->p_attrs);
		kmem_zone_free(preg_zone, prp);
		return ENOMEM;
	}

#ifdef _SHAREII
	/*
	 * Tell Share about the memory change.
	 * SHR_LIMITMEMORY() returns non-zero if a limit 
	 * would be exceeded.
	 */
	if (SHR_LIMITMEMORY(ctob(prp->p_pglen),
				(LI_UPDATE|LI_ALLOC|LI_ENFORCE)))
	{	
                attr_pm_unset(&prp->p_attrs);
		kmem_zone_free(preg_zone, prp);
		return ENOMEM;
	}
#endif /* _SHAREII */

	if (prp->p_flags & PF_PRIVATE)
		prsetp = &ppas->ppas_pregions;
	else
		prsetp = &pas->pas_pregions;
	/*
	 * PF_SHARED uses the shared pmap.
	 * If the AS isn't part of a share group
	 * then PF_PRIVATE pregions use the shared pmap otherwise
	 * they use the private pmap
	 */
	ASSERT((prp->p_flags & (PF_SHARED|PF_PRIVATE)) !=
				(PF_SHARED|PF_PRIVATE));

	if (prp->p_flags & PF_SHARED || !(pas->pas_flags & PAS_SHARED)) {
		prp->p_pmap = pas->pas_pmap;
	} else {
		if (ppas->ppas_pmap == NULL)
			ppas->ppas_pmap = pmap_create(pas->pas_flags & PAS_64,
							PMAP_SEGMENT);
		ASSERT(ppas->ppas_pmap);
		prp->p_pmap = ppas->ppas_pmap;
	}

	if (len && pmap_reserve(prp->p_pmap, pas, ppas, vaddr, len)) {
#ifdef _SHAREII
		/*
		 * Tell Share we're not using the memory
		 */
		(void)SHR_LIMITMEMORY(ctob(prp->p_pglen),
				(LI_UPDATE|LI_FREE));
#endif /* _SHAREII */
                attr_pm_unset(&prp->p_attrs);
		kmem_zone_free(preg_zone, prp);
		nomemmsg("page table reservation");
		return EAGAIN;
	}

	/*
	 * Insert prp on the appropriate pregion list in address order.
	 */
	mutex_lock(&pas->pas_preglock, 0);
	if (PREG_INSERT(prsetp, prp) == NULL) {
		 /*
		  * 	This error should have been caught by chkgrowth;
		  * 	but this is not so, because it allows any zero
		  * 	length region to be mapped in. XXX: fix chkgrowth
		  * 	to catch this case.
		  */
		mutex_unlock(&pas->pas_preglock);
#ifdef _SHAREII
		/*
		 * Tell Share we're not using the memory
		 */
		(void)SHR_LIMITMEMORY(ctob(prp->p_pglen),
				(LI_UPDATE|LI_FREE));
#endif /* _SHAREII */
                attr_pm_unset(&prp->p_attrs);
		kmem_zone_free(preg_zone, prp);
                cmn_err(CE_WARN, "[attachreg]:  Failed pregion insertion");
		return  ENOMEM; /* pretend as though chkgrowth failed */
	}
	if (prp->p_flags & PF_PRIVATE) {
		if ((rp->r_flags & RG_PHYS) == 0) {
			ppas->ppas_size += prp->p_pglen;
		} else
			ppas->ppas_physsize += prp->p_pglen;
	} else {
		if ((rp->r_flags & RG_PHYS) == 0) {
			pas->pas_size += prp->p_pglen;
		} else
			pas->pas_physsize += prp->p_pglen;
	}
	mutex_unlock(&pas->pas_preglock);

	if (cachecolormask && rp->r_refcnt == 0)
		rp->r_color = btoct(vaddr) & cachecolormask;

	++rp->r_refcnt;

	/*	Once a region with anon memory is shared, we must make
	 *	sure all anon pages stay around while the region is still
	 *	in use even if the process who modified the anon page has
	 *	gone.
	 */

	if (rp->r_refcnt > 1 && rp->r_anon && !(rp->r_flags & RG_HASSANON)) {
		rp->r_flags |= RG_HASSANON;
	}

	/*	Attach to inode chain.
	 */
	if (vp = rp->r_vnode) {
		register int s;

		s = mutex_spinlock(&mreg_lock);
		mapattach(prp, vp);
		mutex_spinunlock(&mreg_lock, s);
	}

	if ((ppas->ppas_flags & PPAS_ISO) ||
			CPUMASK_IS_NONZERO(pas->pas_isomask)) {
		/* XXX This should go into the pregion! */
		rp->r_flags |= RG_ISOLATE;
	}

	/* Some pmap initialization is required for private mappings */
	if ((prp->p_flags & PF_PRIVATE) && (pas->pas_flags & PAS_SHARED)) {
		ASSERT(prp->p_pmap == ppas->ppas_pmap);
		pmap_lclsetup(pas, ppas->ppas_utas, prp->p_pmap,
						prp->p_regva, prp->p_pglen);
	} else
		ASSERT(prp->p_pmap == pas->pas_pmap);

	if (prpp)
		*prpp = prp;
	return 0;
}

/*	Detach all or part of a region from a process' address space.
 * NOTE: all detaches to shared process space MUST be single threaded
 * using aspacelock(UPDATE)
 *
 * Flags:
 *	RF_NOFLUSH - no need to flush out tlbs
 *	RF_TSAVE   - prp is a tsave'd region, so treat is special. e.g. do not
 *		try to remove it out of proc or shaddr (by freepreg).
 */
int
vdetachreg(
	preg_t *prp,
	pas_t *pas,
	ppas_t *ppas,
	caddr_t vaddr,
	pgno_t len,
	int flags)
{
	reg_t *rp = prp->p_reg;
	int	is_shm_reg;

	ASSERT(rp);
	ASSERT(mrislocked_update(&rp->r_lock));
	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	ASSERT(poff(vaddr) == 0);
	ASSERT(vaddr >= prp->p_regva);
	ASSERT(vaddr + ctob(len) <= prp->p_regva + ctob(prp->p_pglen));

	/* If any IO is going on on this pregion, refuse to detach 
	 * if any page in the specified range has some I/O going on.
	 */
	if (prp->p_noshrink) {
		regrele(rp);
		return EBUSY;
	}

	if ((flags & RF_NOFLUSH) == 0) {
		/*
		 * flush TLBs
		 * Since we have aspacelock(UPDATE), no more threads
		 * can touch any page - and all their translations
		 * have been wiped out
		 * threads that fault will be sitting at
		 * the aspacelock(ACCESS) in [tvp]fault
		 */
		newptes(pas, ppas, prp->p_flags & PF_SHARED);
	}

#ifdef R10000_SPECULATION_WAR
	if (rp->r_flags & RG_LOCKED) {
		rmlockedpreg(pas, ppas, prp, 0, 0);
	}
#endif
	is_shm_reg = (prp->p_type == PT_SHMEM);

	freepreg(pas, ppas, prp, vaddr, len, flags);

	/*
	 * Things we attempt to handle:
	 * Graphics -
	 *	can map in any region - data, shm, mapped file
	 *	BUT all this is cleaned up on process exit. All we
	 *	need to do is make sure graphics apps don't manually
	 *	detach a shdmem or mapped file segment with the p_noshrink
	 *	state set.
	 * Mapped  devices -
	 *	we allow munmaps since the detachreg code will call the
	 *	c_unmap function which should clean up any mapping.
	 *	Thus its ok for mapped devices to userdma... for long
	 *	periods of time.
	 * Regular drivers -
	 *	In order to provide for an arbitrary driver to be able to lock
	 *	down and map in arbitrary user pages, a protocol
	 *	for how to clean up needs to be devised -
	 *	currently driver close functions are only
	 *	called on last close...
	 */

	/*
	 * Decrement use count and free region if zero
	 * and RG_NOFREE is not set, otherwise unlock.
	 */
	if (rp->r_refcnt == 0 && !(rp->r_flags & RG_NOFREE)) {
		freereg((is_shm_reg) ? NULL :pas , rp);
	} else {
		regrele(rp);
	}

	return 0;
}


/*
 * invalidateaddr - invalidate a range of address - this includes
 * pmap and tlb
 */
void
invalidateaddr(pas_t *pas, preg_t *prp, uvaddr_t vaddr, size_t len)
{
	int free;
	reg_t *rp = prp->p_reg;
	pgcnt_t nvalid;
	attr_t	*attr;
	char 	*end = vaddr + len;
	size_t	attr_len, size, tsize = len;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	reglock(rp);
	free = (rp->r_flags & RG_HASSANON) ? PMAPFREE_SANON
					   : PMAPFREE_FREE;
	attr = findattr(prp, vaddr);
	while (attr) {
		if (attr->attr_start >= end)
			break;

		if (attr->attr_lockcnt) {
			/*
			 * No need to do anything in this case since
			 * locked pages would not be subject to COW
			 * processing and so we did not get a new page.
			 */
		} else {
			attr_len = (size_t)(attr->attr_start -
						attr->attr_end);
			size = attr_len < tsize ? attr_len : tsize;
			/*
	 		 * If there are large pages in the region we 
			 * are invalidating then make sure that the 
			 * boundary is broken down to the lowest page
	 		 * size.
	 		 */
			pmap_downgrade_addr_boundary(pas, prp, vaddr, 
						btoc(size), 0);

			nvalid = pmap_free(pas, prp->p_pmap, vaddr, 
						btoc(size), free);
			prp->p_nvalid -= nvalid;
		}

		attr = attr->attr_next;
		tsize -= size;
	}
	regrele(rp);

	/*
	 * invalidate tlb
	 * Since we are called on behalf of another process, we can't
	 * call newptes. But we only need to flush individual tlb
	 * entries, no page tables have changed, so tlbsync is enough
	 * Note that we have not cleared the pmap entry for locked pages
	 * but that's ok since the mapping has not changed.
	 */
	tlbsync(0LL, pas->pas_isomask, FLUSH_MAPPINGS);
}

/*
 * Replace a shared (or shareable) region with a private copy.
 * Caller must have region and inode locked
 * Returns with region locked - if region changes, old is unlocked
 *	and new is locked
 * Note that caller must deal with any pmap changes...
 */
static reg_t *
replacereg(pas_t *pas, ppas_t *ppas, preg_t *prp)
{
	reg_t *rp, *rp2;
	/* REFERENCED */
	vnode_t *vp;

	rp = prp->p_reg;
	ASSERT(mrislocked_update(&rp->r_lock));

	/* Region is changing to anon.  Time to allocate smem. */
	ASSERT(rp->r_anon == NULL);
	if (pas_reservemem(pas, prp->p_pglen, 0))
		return NULL;

	/*
	 * We want this CW on fork and dupped
	 * We do NOT want it shared if the process ever sproc's
	 * It also needs to be a PF_PRIVATE region
	 * If this is a pthreaded process, though, the new
	 * region must be shared by all.
	 */
	if (pas->pas_flags & PAS_PSHARED) {
		prp->p_flags |= PF_DUP;
	} else
	if (!(prp->p_flags & PF_PRIVATE)) {
		prp->p_flags |= (PF_TXTPVT|PF_DUP|PF_NOSHARE|PF_PRIVATE);
		mutex_lock(&pas->pas_preglock, 0);
		PREG_DELETE(&pas->pas_pregions, prp);
		if (rp->r_flags & RG_PHYS) {
			pas->pas_physsize -= prp->p_pglen;
			ASSERT(pas->pas_physsize >= 0);
		} else {
			pas->pas_size -= prp->p_pglen;
			ASSERT(pas->pas_size >= 0);
		}

		PREG_INSERT(&ppas->ppas_pregions, prp);
		/* always non-PHYS */
		ppas->ppas_size += prp->p_pglen;
		mutex_unlock(&pas->pas_preglock);
	} else {
		prp->p_flags |= (PF_DUP|PF_NOSHARE);
	}

	/*
	 * Just change the region type
	 */
	if (rp->r_refcnt == 1) {
		rp->r_type = RT_MEM;
		/* turn off PHYS (/dev/zero) */
		ASSERT((rp->r_flags & RG_PHYS) == 0 || rp->r_pgsz == 0);
		rp->r_flags &= ~(RG_PHYS|RG_TEXT);
		rp->r_flags |= RG_CW;
		rp->r_swapres = prp->p_pglen;
		if ((rp->r_flags & RG_ANON) == 0) {
			rp->r_flags |= RG_ANON;
			rp->r_anon = anon_new();
			ASSERT(rp->r_anon);
		}
		return rp;
	}
	ASSERT((rp->r_flags & RG_PHYS) == 0);
	ASSERT(rp->r_mappedsize == 0);

	/* Allocate a region descriptor */
	rp2 = allocreg(rp->r_vnode, RT_MEM, rp->r_flags|RG_ANON|RG_CW);

	/* Copy pertinent data to new region */
	rp2->r_pgsz = rp->r_pgsz;
	rp2->r_swapres = prp->p_pglen;
	rp2->r_maxfsize = rp->r_maxfsize;
	rp2->r_fileoff = rp->r_fileoff;
	rp2->r_flags &= ~RG_TEXT;
	vp = rp2->r_vnode = rp->r_vnode;
	ASSERT(!vp || (vp->v_type != VCHR && vp->v_type != VBLK)); 
	if (rp2->r_flags & RG_LOADV) {
		rp2->r_loadvnode = rp->r_loadvnode;
		VN_HOLD(rp2->r_loadvnode);
	} else
		rp2->r_shmid = rp->r_shmid;
	rp->r_refcnt--;
#ifdef CKPT
	rp2->r_ckptflags = rp->r_ckptflags;
	rp2->r_ckptinfo = rp->r_ckptinfo;
#endif
	regrele(rp);
	rp2->r_refcnt++;
	prp->p_reg = rp2;

	return (rp2);
}

/*
 * Iterate over all the previously privatized text regions, making
 * them sharable.
 */
void
vungetprivatespace(pas_t *pas, ppas_t *ppas)
{
	preg_t		*prp, *tprp;

	/*
	 * Check that we are protected for [p]pas_size and p_flags.
	 */
	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	mutex_lock(&pas->pas_preglock, 0);

	prp = PREG_FIRST(ppas->ppas_pregions);
	while (prp) {
		tprp = prp;
		prp = PREG_NEXT(prp);
		if (tprp->p_flags & PF_TXTPVT) {
			PREG_DELETE(&ppas->ppas_pregions, tprp);
			/* region is guranteed non PHYS */
			ppas->ppas_size -= tprp->p_pglen;
			pas->pas_size += tprp->p_pglen;
			tprp->p_flags &= ~(PF_TXTPVT|PF_NOSHARE|PF_PRIVATE);
			PREG_INSERT(&pas->pas_pregions, tprp);
		}
	}
		
	mutex_unlock(&pas->pas_preglock);
}


/*	Copy all of p_prp's (physical) page mappings to c_prp.
 */
static void
copy_phys_pmap(
	register pas_t	*cpas,
	register preg_t *p_prp,
	register preg_t *c_prp)
{
	register pde_t	*ppte;
	register pde_t	*cpte;
	auto char	*pvaddr = p_prp->p_regva;
	auto pgno_t	 size = p_prp->p_pglen;
	auto pgno_t	 sz;

	/*	Not currently called for non-cw mappings that
	 *	map into pageable memory.
	 */
	ASSERT(p_prp->p_reg->r_flags & RG_PHYS);

	/* Check each page of the parent's and then copy it to
	 * the child's pte.
	 */
	while (size) {
		ppte = pmap_probe(p_prp->p_pmap, &pvaddr, &size, &sz);

		if (ppte == NULL)
			break;

		/*
		 * Won't do any good to call pmap_ptes with VM_NOSLEEP
		 * flag and then release the regions here -- they are
		 * phys regions and don't have any memory to steal.
		 */
		cpte = pmap_ptes(cpas, c_prp->p_pmap,
				c_prp->p_regva + (pvaddr - p_prp->p_regva),
				&sz, 0);

                /*
                 * We don't need to do pfn locking because this routine deals
                 * with physical regions of non-migratable memory.
                 */
                
		ASSERT(sz > 0 && sz <= size);
		pvaddr += ctob(sz); 

		for (size -= sz ; sz > 0; ppte++, cpte++, sz--) {
			if (pg_isvalid(ppte)) {
				/* Copy parent's pte to child.  */
				pg_setpgi(cpte, pg_getpgi(ppte));
				c_prp->p_nvalid++;
			}
		}
	}
}

/*	Copy all of p_prp's valid, locked page mappings to c_prp.
 */
static int
copy_pmap(
	pas_t		*spas,		/* source pas */
	pas_t 		*cpas,		/* child pas */
	register preg_t *p_prp,
	register preg_t *c_prp)
{
	register pde_t	*ppte;
	register pde_t	*cpte;
	register char	*cvaddr;
	register attr_t	*attr;
	register pfd_t	*pfd;
	register reg_t	*p_rp = p_prp->p_reg;
	auto char	*pvaddr;
	auto pgno_t	 size;
	auto pgno_t	 sz;
	int	num_base_pages; /* Num base pages in large page */
	pde_t	*slpte; /* Start of large page pte*/
	pde_t	*elpte; /* End of large page pte*/
	pde_t	*tmp_pte;  
	int	do_lpage_downgrade = 0; /* Downgrade child's page if true */
	/* REFERENCED */
	int	error;
	extern int miser_rss_account;
	int	op = (miser_rss_account ? JOBRSS_INS_PFD : JOBRSS_INC_FOR_PFD);

	for (attr = &p_prp->p_attrs ; attr ; attr = attr->attr_next) {
	/* *** Unindented for more room *** */

	/*	If there are pages locked to the parent process,
	 *	must perform copy-on-write processing now, so the
	 *	locker (parent) won't be forced to abandon the
	 *	page in pfault.  (Dont know if the page address
	 *	has been passed off to a driver for a long-term dma).
	 */
	if ((p_rp->r_flags & RG_CW) && (attr->attr_lockcnt)) {
		pvaddr = attr->attr_start;
		size = btoct(attr->attr_end - attr->attr_start);
		ASSERT(size);
		while (size) {
			sz = size;
			ppte = pmap_ptes(spas, p_prp->p_pmap, pvaddr, &sz, 0);
			cvaddr = c_prp->p_regva + (pvaddr - p_prp->p_regva);
			ASSERT(sz);
			cpte = pmap_ptes(cpas, c_prp->p_pmap, cvaddr, &sz, 
								VM_NOSLEEP);

			if (cpte == NULL) {
				regrele(p_rp);
				if (p_rp != c_prp->p_reg)
					regrele(c_prp->p_reg);
				setsxbrk();
				reglock(p_rp);
				if (p_rp != c_prp->p_reg)
					reglock(c_prp->p_reg);
				continue;
			}
			ASSERT(sz);
			size -= sz;

			for (; --sz >= 0; ppte++, cpte++, cvaddr += NBPP, 
							pvaddr += NBPP) {
				if (copy_locked_page(p_prp, attr, c_prp, cvaddr,
							ppte, cpte, cpas))
					return(EAGAIN);
			}
		}

		/*
		 * If this attribute has large pages then make sure that
		 * the pages around the virtual address range are downgraded
		 * to the right size.
		 */

		if (PMAT_GET_PAGESIZE(attr) > NBPP)
			pmap_downgrade_addr_boundary(NULL, c_prp,
				attr->attr_start,
				btoct(attr->attr_end - attr->attr_start), 0);
	}

	/*	If there's backing store associated with this
	 *	region, must attach the (possibly) anonymous pages
	 *	here so the anon manager won't lose track of them.
	 */
#define MAXCOPY_PGLEN	1000
	else if (p_rp->r_anon || 
		 (p_prp->p_pglen < MAXCOPY_PGLEN)) {

		register int  do_clrmod = p_rp->r_flags & RG_CW;
		pvaddr = attr->attr_start;
		size = btoct(attr->attr_end - attr->attr_start);
		while (size) {
			register int nvalid = 0;

			ppte = pmap_probe(p_prp->p_pmap, &pvaddr,
						&size, &sz);

			cvaddr = c_prp->p_regva +
					(pvaddr - p_prp->p_regva);
			cpte = pmap_ptes(cpas, c_prp->p_pmap, cvaddr, &sz,
					 VM_NOSLEEP);

			if (cpte == NULL) {
				regrele(p_rp);
				if (p_rp != c_prp->p_reg)
					regrele(c_prp->p_reg);
				setsxbrk();
				reglock(p_rp);
				if (p_rp != c_prp->p_reg)
					reglock(c_prp->p_reg);
				continue;
			}

			for (size -= sz, pvaddr += ctob(sz);
			     sz > 0 ;
			     ppte++, cpte++, sz--, cvaddr += NBPP) {

				/*
				 * Check if the page is valid without acquiring
				 * the pte lock.
				 */

				if (!pg_isvalid(ppte)) {
					pfd = (pg_getpfn(ppte) > PG_SENTRY) ? 
						pdetopfdat(ppte) : (pfd_t *)0;
					if (pfd) {
						/* Attempt to copy parent */
					} else {
						pg_clrpgi(cpte);
						continue;
					}
				}
				/*
				 * Hold lock on parent pte, to prevent the
				 * page from being either stolen/shotdown
				 * under you.
				 */

				pg_pfnacquire(ppte);

retry_migrate:
				if (!pg_isvalid(ppte)){
					/* Ensures the private map doesnot
					 * have sentinels if the page is
					 * invalid in the parent space 
					 * Parent's text page could be 
					 * stolen by vhand while we were
					 * sleeping above!!.
					 * If parent's page is invalid,
					 * (and has sentinel), then we
					 * don't want to copy this to
					 * child, since we would end up
					 * seeing a sentinel in vfault!!
					 */
					pfd = (pg_getpfn(ppte) > PG_SENTRY) ? 
						pdetopfdat(ppte) : (pfd_t *)0;
					if (pfd) {
						if (pfdat_ismigrating(pfd)) {
						  /*
						   * Parent page is migrating.
						   * Wait for migration to
						   * complete.
						   */
						  pageuseinc(pfd);
						  pg_pfnrelease(ppte);
						  pagewait(pfd);
						  pg_pfnacquire(ppte);
						  pfdat_release(pfd);
						  goto retry_migrate;
						}
					} else {
						pg_clrpgi(cpte);
						pg_pfnrelease(ppte);
						continue;
					}
				}
                                /*
                                 * We're holding the pfdat currently in
                                 * transit between pte's.
                                 * pfd = pdetopfdat_hold(ppte);
				 *
				 * Since pte has been held above,
				 * use pfdat_hold to grab an extra 
				 * reference on the page.
                                 */
				pfd = pdetopfdat(ppte);

				/*
				 * If a page is locked, break the COW on
				 * that page. Refer bug 370717.
				 */
				if ((p_rp->r_flags & RG_CW) &&
					(((p_prp->p_type == PT_STACK) 
					&& (cvaddr == attr->attr_start) && 
					    !(pg_get_page_mask_index(ppte))) || 
					((p_prp->p_noshrink) && 
						(pfd->pf_rawcnt)))) {
					pg_pfnrelease(ppte);
					if (copy_locked_page(p_prp, attr, c_prp,
					  cvaddr, ppte, cpte, cpas))
						return(EAGAIN);
					/*
					 * If we break COW on a large page
					 * downgrade surrounding pages.
					 */
					if (pg_get_page_mask_index(ppte)) {
						num_base_pages = 
						PAGE_MASK_INDEX_TO_CLICKS(
						pg_get_page_mask_index(ppte));
						slpte = LARGE_PAGE_PTE_ALIGN(
							cpte, 
						NUM_TRANSLATIONS_PER_TLB_ENTRY
							*num_base_pages);
						elpte = slpte + 
						NUM_TRANSLATIONS_PER_TLB_ENTRY
							*num_base_pages - 1;
						/*
						 * Downgrade from slpte to cpte.
						 */
						for (tmp_pte = slpte; 
							tmp_pte < cpte; 
								tmp_pte++)
							pg_set_page_mask_index(
								tmp_pte, 
							MIN_PAGE_MASK_INDEX);
						/*
						 * Enable downgrading from
						 * cpte to elpte.
						 */
						if (cpte < elpte)
							do_lpage_downgrade = 1;
					}
				} else {

					ASSERT(pfd->pf_use > 0);
					pageuseinc(pfd);

					/* turn off _hdwr_ mod bit? */
					if (do_clrmod)
						pg_clrmod(ppte);
					
					/*
					 * Also inherits the page size. Don't
					 * copy the lock.
					 */
					pg_setpgi(cpte, pg_getpgi(ppte) & ~(PG_PFNLOCK));
					if (do_lpage_downgrade) {
						pg_set_page_mask_index(cpte, 
							MIN_PAGE_MASK_INDEX);
						if (cpte == elpte) {
							do_lpage_downgrade = 0;
							/* Defensive */
							elpte = slpte = 0;
						}
					}

					pg_pfnrelease(ppte);

					/*
					 * Job rss can not fail since we are
					 * not adding a new page to the job.
					 * This should really be an inc_for_pfd
					 * op, but since we are sure this page
					 * is not being *newly* inserted, we
					 * can use ins_pfd for performance.
					 * In the inaccurate accounting
					 * method, we have to use inc_for_pfd
					 * to increment job count by 1.
					 */
					VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(cpas), op, pfd, cpte, attr_pm_get(attr));
					nvalid++;
				}
			}
			c_prp->p_nvalid += nvalid;
		}
	}
	/* *** Unindented for more room *** */
	}
	return(0);
}

/*
 * Break COW on a page if it is locked by useracc. This is needed if a process
 * useraccs a page and forks which can occur in gfx applications. It is also
 * needed by debuggers which are writing to locked instruction pages of a 
 * target. Refer bugs 370717 and 486776.
 */
int
copy_locked_page(
	preg_t *p_prp,
	attr_t  *p_attr,
	preg_t *c_prp,
	caddr_t cvaddr,
	pde_t *ppte,
	pde_t *cpte,
	pas_t *pas)
{
	uint pcolor, vcolor, pagealloc_color;
	auto anon_hdl    c_anon = c_prp->p_reg->r_anon;
	auto pgno_t      apn;
	pfd_t   *pfd;
	pfd_t	*source_pfd;
	int	flags;
	/* REFERENCED */
        size_t           psz;
	reg_t		*p_rp = p_prp->p_reg;
	reg_t		*c_rp = c_prp->p_reg;
	int		procfs = 0;

	procfs = (ppte == cpte);		/* Is the call from procfs? */
        ASSERT(pg_isvalid(ppte));
	if (!procfs) ASSERT(pg_getpgi(cpte) == 0);
	else {
		ASSERT((ppte == cpte) && (p_prp == c_prp) && pas && c_rp 
			&& c_anon && (c_prp->p_type != PT_PRDA));
		/*
		 * First acquire all resources, so that we are guaranteed
		 * one locked page. Depending on whether the old page became 
		 * unlocked, we will release the lockable memory later.
		 */
		if (pas_reservemem(pas, 0, 1)) {
			return(EAGAIN);
		}
	}
	if (VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, 1)) {
		if (procfs) pas_unreservemem(pas, 0, 1);
		return(EAGAIN);
	}

	/*
	 * cache key is difficult to determine
	 * w/o going to alot of trouble.
	 * We'll just assume that its
	 * ANON memory thats being
	 * written and key off of the
	 * region generation #
	 */
	apn = vtoapn(c_prp, cvaddr);

	/*
	 * We really don't need to hold the pfdat
	 * in this case because the rawcnt is non-zero.
	 * However, the page locking mechanism may
	 * change in the future, so in order to
	 * prevent dangerous side-effects, we're just
	 * going to explicitely hold the pfdat.
	 */
	source_pfd = pdetopfdat_hold(ppte);
	ASSERT(source_pfd);

	/*
	 * Is this region if a PRDA then
	 * we need to create the page using the
	 * color of a PRDA to avoid VCE problems
	 */
	if(p_prp->p_type == PT_PRDA){
#if USE_PTHREAD_RSA
		flags = VM_PACOLOR;
#else
		flags = VM_VACOLOR;
#endif /* USE_PTHREAD_RSA */
		pcolor = vcolor = colorof(PRDAADDR);
	} else {
		flags = 0;
		pcolor = vcache2(c_prp, p_attr, apn); 
#ifdef _VCE_AVOIDANCE
		vcolor = colorof(cvaddr);
#else /* _VCE_AVOIDANCE */
		vcolor = NOCOLOR;
#endif
	}	

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		pagealloc_color = vcolor;
		flags |= VM_MVOK;
	} else
#endif /* _VCE_AVOIDANCE */
	{
		pagealloc_color = pcolor;
	}

	psz = NBPP;
	while ((pfd = PMAT_PAGEALLOC(p_attr, pagealloc_color, flags, &psz, cvaddr)) == NULL) {
		regrele(p_rp);
		if (p_rp != c_prp->p_reg)
			regrele(c_prp->p_reg);
		setsxbrk();
		reglock(p_rp);
		if (p_rp != c_prp->p_reg)
			reglock(c_prp->p_reg);
	}

	ASSERT(pfd->pf_rawcnt == 0);
	/* Insert in page table */

	/*
	 * We don't need to explicitly hold the destination
	 * pfdat because the extra ref count acquired in
	 * page alloc prevents migration until we call
	 * rmap_addmap.
	 */

	COLOR_VALIDATION(pfd, vcolor & cachecolormask,0,0);
	if (procfs) {
		int	x;
		int	rmem = 0;
		int 	free = (c_rp->r_flags & RG_HASSANON) ?
					PMAPFREE_SANON : PMAPFREE_FREE;
		/*
		 * Set rawcnt on new page to indicate it is locked by 
		 * the target process.
		 */
		pfd->pf_rawcnt = 1;

		/*
		 * Give up rawcnt on old page - unlockmem lookalike.
		 */
		pg_pfnacquire(cpte);
		x = pfdat_lock(source_pfd);
		if (pfdat_dec_rawcnt(source_pfd) == 0) rmem = 1;
		pfdat_unlock(source_pfd, x);
		PFD_WAKEUP_RAWCNT(source_pfd);
		pg_pfnrelease(cpte);

		/*
		 * Give up reference to old page - invalidateaddr lookalike.
		 */
		pmap_free(pas, c_prp->p_pmap, cvaddr, 1, free);

		/*
		 * The old page became unlocked - unreserve lockable memory.
		 */
		if (rmem) pas_unreservemem(pas, 0, 1);
	}

	if (p_prp->p_type == PT_PRDA) 
		page_copy(source_pfd, pfd, colorof(PRDAADDR), colorof(PRDAADDR));
	else
		page_copy(source_pfd, pfd, vcolor, vcolor);

	/*
	 * Set the pte after the copy is over - this will solve a race
	 * condition in the procfs case when the target process incurs
	 * utlbmisses and picks up the new page after the page copy has
	 * been done.
	 */
	pg_setpfn(cpte, pfdattopfn(pfd));
	pg_setvalid(cpte);

	/*
	 * Instruction pages should not be marked modifiable.
	 */
	if (!procfs) pg_setmod(cpte);
#if R4000 || TFP || R10000 || BEAST
	pg_setccuc(cpte, p_attr->attr_cc, p_attr->attr_uc);
#else
	cpte->pte.pg_n = p_attr->attr_cc & 1;
#endif
	anon_insert(c_anon, pfd, apn, P_DONE);
	VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_INS_PFD, pfd, cpte, 
						attr_pm_get(p_attr));
	pfdat_release(source_pfd);
	if (!procfs) c_prp->p_nvalid++;
	return(0);
}

#ifdef DEBUG
unsigned int pmaps_trimmed;
#endif

/*
 * Attempt to trim a processes pmap pages.
 * This must be called by a process on its own behalf to be sure
 * process can't run (since we call newptes).
 */
void
try_pmap_trim(pas_t *pas, ppas_t *ppas)
{
	pmap_t *pmap;
	int trimmed = 0;

	ASSERT(mrislocked_access(&pas->pas_aspacelock));

	if (mrtrypromote(&pas->pas_aspacelock)) {
		/*
		 * Trimming is allowed only if pmap_trimtime is gt 0.
		 * We dont want to trim the page map while getpages
		 * is looking thru the pmap trying to steal pages.
		 * To prevent races here,
		 *	- getpages sets pmap_trimtime to 0 while it
		 *	  is scanning the pmap. It sets pmap_trimtime
		 *	  to 0 ONLY if it was previously > 0.
		 *      - this routine sets pmap_trimtime to -1 while
		 *        a trim is in progress. It will do this ONLY
		 *	  if pmap_trimtime was > 0.
		 *	- getpages skips pmaps if pmap_trimtime is < 0.
		 */
		if ((pmap = ppas->ppas_pmap) && pmap->pmap_trimtime) {
			if (comparegt_and_swap_int((int*)&pmap->pmap_trimtime, 0, -1)) {
				trimmed += pmap_trim(pmap);
				pmap->pmap_trimtime = 0;
			}
			ASSERT(pas->pas_flags & PAS_SHARED);
		}
		if ((pmap = pas->pas_pmap) && pmap->pmap_trimtime) {
			if (comparegt_and_swap_int((int*)&pmap->pmap_trimtime, 0, -1)) {
				trimmed += pmap_trim(pmap);
				pmap->pmap_trimtime = 0;
			}
		}

		if (trimmed) {
#ifdef DEBUG
			pmaps_trimmed += trimmed;
#endif
			newptes(pas, ppas, pas->pas_flags & PAS_SHARED);
		}
	}
}

/*
 * Duplicate a region
 *
 * Requires the information necessary to allocate a pregion and map it
 * into the user address space (required in order to copy pmap mappings).
 *
 * Returns a pointer to the new prp which maps the (locked) duplicated
 * region.
 *
 * force:
 *	RF_FORCE - force a shared region to be copy-on-write and RT_MEM,
 *		   invoked only from vgetprivatespace.
 */

extern int max_shared_reg_refcnt;

int
dupreg(
	pas_t  *spas,
	preg_t *prp,
	pas_t *pas,
	ppas_t *ppas,
	caddr_t regva,
	uchar_t type,
	ushort pflags,
        struct pm *pm,
	int force,
	preg_t **new_prp)
{
	auto preg_t	*prp2;
	register reg_t	*rp = prp->p_reg;
	vnode_t		*vp = rp->r_vnode;
	register reg_t	*rp2;
	register int	error;
#ifdef PDEBUG
	static int dup_anon = 1;
#endif
	
	*new_prp = NULL;

	ASSERT(mrislocked_update(&rp->r_lock));

	/* If region is RG_PHYS, or is shared and we're not forcing
	 * a duplicate, just attach the region, [optionally] set up
	 * pmap, and return.
	 */
	if ((((prp->p_flags & PF_DUP) == 0) && (force & RF_FORCE) == 0)
	    || rp->r_flags & RG_PHYS) {

		ASSERT(prp->p_type == PT_SHMEM || prp->p_type == PT_MAPFILE || \
				((rp->r_flags & RG_ANON) == 0) || \
				((prp->p_maxprots & PROT_W) == 0));

		rp2 = NULL;

		/*
		 * If this is a MAP_SHARED mapping of some kind (including
		 * shared text mappings), then make a new region struct
		 * once the ref count on the existing one gets too high.
		 * This avoids a region lock contention problem caused
		 * when a parent forks a lot of children.
		 */

		if (rp->r_refcnt >= max_shared_reg_refcnt &&
		    prp->p_type == PT_MAPFILE &&
		   (rp->r_flags & (RG_ANON|RG_PHYS|RG_USYNC)) == 0) {

			ASSERT(rp->r_type == RT_MAPFILE);

			/* 
			 * Get a new region struct.
			 */

			rp2 = allocreg(vp, rp->r_type, rp->r_flags);
			

			/* 
			 * Copy pertinent data to new region
			 */
		
			ASSERT(rp->r_swapres   == 0);
			ASSERT(rp->r_loadvnode == NULL);
			ASSERT(rp->r_fault     == NULL);
			ASSERT(rp->r_spans     == NULL);
			ASSERT(rp->r_anon      == NULL);

			rp2->r_pgsz       = rp->r_pgsz;
			rp2->r_maxfsize   = rp->r_maxfsize;
			rp2->r_mappedsize = rp->r_mappedsize;
			rp2->r_fileoff    = rp->r_fileoff;

#ifdef CKPT
			rp2->r_ckptflags  = rp->r_ckptflags;
			rp2->r_ckptinfo   = rp->r_ckptinfo;
#endif

			/*
			 * The new region is now set up just like the old
			 * one.  So from here on, we can just use and
			 * attach the new region.
			 */

			rp = rp2;
		}

		error = vattachreg(rp, pas, ppas, regva, prp->p_offset,
				  prp->p_pglen, type, 0, prp->p_maxprots,
				  pflags, pm, &prp2);
		if (error) {
			if (rp2)
				freereg(pas, rp2);

			return EAGAIN;
		}

		/* dup all attributes */
		attr_dup(prp, prp2);

		/*	If its not RG_PHYS region, pmap can
		 *	get updated at fault time.
		 */
		if (rp->r_flags & RG_PHYS)
			copy_phys_pmap(pas, prp, prp2);
		else {
			if (copy_pmap(spas, pas, prp, prp2)) {
				miser_inform(MISER_KERN_MEMORY_OVERRUN);
				return ENOMEM;
			}
		}

		*new_prp = prp2;
		return 0;
	}

	/*
	 * Make sure we have enough space to duplicate
	 * this region without potential deadlock.
	 */
	if (error = pas_reservemem(pas, rp->r_swapres, 0)) 
		return(error);

	/* Need to copy the region. Allocate a region descriptor */
	rp2 = allocreg(vp, (force & RF_FORCE) ? RT_MEM : rp->r_type, 0);
	
	/* Copy pertinent data to new region */
	/* If from vgetprivatespace, no need to make shared region COW */
	if ((force & RF_FORCE) == 0)
		rp->r_flags |= RG_CW;

	rp2->r_flags = rp->r_flags | RG_CW;
	if (rp2->r_flags & RG_LOADV) {
		rp2->r_loadvnode = rp->r_loadvnode;
		VN_HOLD(rp2->r_loadvnode);
	} else 
		rp2->r_shmid = rp->r_shmid;
	rp2->r_pgsz = rp->r_pgsz;
	rp2->r_swapres = rp->r_swapres;
	rp2->r_maxfsize = rp->r_maxfsize;
	rp2->r_mappedsize = rp->r_mappedsize;
	rp2->r_fileoff = rp->r_fileoff;

	error = vattachreg(rp2, pas, ppas, regva, prp->p_offset,
		prp->p_pglen, type, 0, prp->p_maxprots, pflags, pm, &prp2);
	if (error) {
		rp2->r_flags = 0;	/* can't be any ANON stuff since we
					 * called allocreg w/o any flags
					 */
		rp2->r_mappedsize = 0;
		freereg(pas, rp2);	/* will free availsmem reservation */
		return EAGAIN;
	}

	/* dup all attributes */
	attr_dup(prp, prp2);

	/*	Set up anonymous memory manager..
	 *	If this is a forced dup of a shared text region,
	 *	allocate new anon manager; otherwise dup it.
	 */
	if (rp->r_anon) {
		ASSERT(rp->r_flags & RG_ANON);
		if (rp->r_flags & RG_HASSANON) {

			/*
			 * Make sure vhand isn't swapping sanon pages while
			 * we're doing an anon_dup since it might collapse
			 * nodes out of the tree.  We use one global lock
			 * to cover all sanon regions since vhand doesn't
			 * have access to the individual region locks.  It
			 * only has a list of unreferenced sanon pages.
			 */

			mutex_lock(&sanon_lock, PZERO);
			rp2->r_anon = anon_dup(&rp->r_anon);
			mutex_unlock(&sanon_lock);
		} else
			rp2->r_anon = anon_dup(&rp->r_anon);

	} else {
		rp2->r_anon = anon_new();
		rp2->r_flags |= RG_ANON;
	}
	ASSERT(rp2->r_anon);
#ifdef PDEBUG
	if (dup_anon) {
		if (rp->r_anon)
			pfindanyanon(rp->r_anon);
		pfindanyanon(rp2->r_anon);
	}
#endif

	/* can sleep for a very long time! */
	if (copy_pmap(spas, pas, prp, prp2)) {
		miser_inform(MISER_KERN_MEMORY_OVERRUN);
		return ENOMEM;
	}

	if (vp && (vp->v_type == VCHR || vp->v_type == VBLK)) {
		int rv;

		VOP_ADDMAP(vp, VDUPMAP, NULL, NULL, (off_t)0L,
			   rp2->r_mappedsize, prp->p_attrs.attr_prot,
			   get_current_cred(), rv);
		ASSERT(rv == 0);

		if (rv) {
			rp2->r_mappedsize = 0;
			(void) vdetachreg(prp2, pas, ppas, prp2->p_regva, 
					prp2->p_pglen, RF_NOFLUSH);
			/* detachreg decrements availsmem ... */
			return EAGAIN;
		}
	}

#ifdef CKPT
	rp2->r_ckptflags = rp->r_ckptflags;
	rp2->r_ckptinfo = rp->r_ckptinfo;
#endif

	*new_prp = prp2;
	return(0);
}

/*
 * Change the size of a region
 *  change == 0  -> no-op
 *  change  < 0  -> shrink change pages
 *  change  > 0  -> expand change pages
 * For expansion, you get ``type'' pages
 *
 * For shrink, the caller must flush the TLB/zap segment table
 *
 * Returns errno on failure, 0 on success.
 * NOTE: all grows of shared process space MUST be single threaded using
 * aspacelock(UPDATE)
 * Pregions with a positive p_noshrink count get automatic errors - this is
 * used so that it is possible to give away implicit references to pages for a
 * long period of time, and not require that the aspacelock be held.
 * It is always ok to grow - just shrinking is checked
 */

int
vgrowreg(
	register preg_t *prp,
	pas_t *pas,
	ppas_t *ppas,
	uchar_t prots,
	register pgno_t	change)
{
	register reg_t	*rp;
	register char	*start;
	attr_t		*attr, *nextattr;
	int		error = 0;
#if R4000
	pgno_t		end = btoc(prp->p_regva) + prp->p_pglen;
#endif

	rp = prp->p_reg;
	ASSERT(mrislocked_update(&rp->r_lock));
	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	if (change == 0)
		return 0;
	
	if (change < 0) {
		ASSERT(prp->p_type != PT_STACK);

		if (prp->p_noshrink || rp->r_refcnt > 1)
			return EBUSY;

		ASSERT(-change <= prp->p_pglen);

		start = prp->p_regva + ctob(prp->p_pglen + change);
		freepreg(pas, ppas, prp, start, -change, 0);

		if (rp->r_anon && ((rp->r_flags & RG_PHYS) == 0)) {
			pas_unreservemem(pas, -change, 0);
			rp->r_swapres += change;

			/*
			 * Make sure vhand isn't swapping sanon pages while
			 * we're trying to free them.  We use one global lock
			 * to cover all sanon regions since vhand doesn't
			 * have access to the individual region locks.  It
			 * only has a list of unreferenced sanon pages.
			 */
	
			if (rp->r_flags & RG_HASSANON) {
				mutex_lock(&sanon_lock, PZERO);
				anon_setsize(rp->r_anon, prp->p_offset +
					     prp->p_pglen);
				mutex_unlock(&sanon_lock);
			} else
				anon_setsize(rp->r_anon, prp->p_offset +
					     prp->p_pglen);
		}
		rp->r_pgsz += change;

	} else {
		/*	We are expanding the region.  Make sure that
		 *	the new size is legal and then allocate
		 *	page tables rights.  Private autoreserve mappings
		 *	reserve availsmem at demand zero time.
		 */
		if (rp->r_anon &&
		  ((rp->r_flags & (RG_PHYS|RG_AUTORESRV)) == 0)){
			if (prp->p_type == PT_SHMEM) 
				error = reservemem(GLOBAL_POOL, change, 0, 0);
			else 
				error = pas_reservemem(pas , change, 0);

			if (error) 
				return error;
			rp->r_swapres += change;
		}

		if (prp->p_type == PT_STACK)
			start = prp->p_regva - ctob(change);
		else 
			start = prp->p_regva + ctob(prp->p_pglen);

		if (chkgrowth(prp, pas, ppas, start, start + ctob(change), B_TRUE))
			error = ENOMEM;

		else  if (pmap_reserve(prp->p_pmap, pas, ppas, start, change)) 
				error = EAGAIN;
#ifdef _SHAREII
		/*
		 * Ask Share if the additional memory will 
		 * exceed a limit
		 */
		else if (SHR_LIMITMEMORY(ctob(change), (LI_ALLOC|LI_UPDATE|LI_ENFORCE)))
			error = ENOMEM;
#endif /* _SHAREII */

		if (error) {
			if (rp->r_anon && 
			   (rp->r_flags & (RG_PHYS|RG_AUTORESRV)) == 0) {
				if (prp->p_type == PT_SHMEM)
					unreservemem(GLOBAL_POOL, change, 0, 0);
				else
					pas_unreservemem(pas, change, 0);
				rp->r_swapres -= change;
			} 
			return error;
		}

		if (prp->p_pglen == 0) {
			attr = &prp->p_attrs;
			ASSERT(attr->attr_next == NULL);
			ASSERT(attr->attr_start == attr->attr_end);
			if (prp->p_type == PT_STACK) {
				attr->attr_start -= ctob(change);
				prp->p_stk_rglen += change;
			} else
				attr->attr_end = attr->attr_start+ctob(change);
			attr->attr_prot = prots;
		} else {
			struct pageattr pattr;
			pde_t pte;

			pte.pgi = 0;
			if (rp->r_flags & RG_PHYS)
				pg_setcache_phys(&pte);
			else
				pg_setcachable(&pte);

			pattr.attr_attrs = 0;
			pattr.attr_cc = pte.pte.pte_cc;
			pattr.attr_prot = prots;

			if (prp->p_type == PT_STACK) {
				attr = &prp->p_attrs;
				if ((attr->attr_attrs != pattr.attr_attrs) || 
					!aspm_isdefaultpm(pas->pas_aspm, 
						attr->attr_pm, MEM_STACK)) {
					nextattr = kmem_zone_alloc(attr_zone,
								KM_SLEEP);
					*nextattr = *attr;

                                        /*
					 * When regions grow they get the 
					 * default
					 * PM for the appropriate region type.
					 * nextattr actually covers the range
					 * of attr and attr is updated to the
					 * new range. So nextattr will have the
					 * old pm and attr will have the new pm.
                                         */

					attr->attr_pm = aspm_getdefaultpm(
						pas->pas_aspm, MEM_STACK);
					attr->attr_end = attr->attr_start;
					attr->attr_next = nextattr;
					attr->attr_attrs = pattr.attr_attrs;
                                      
				}
				attr->attr_start -= ctob(change);
				prp->p_stk_rglen += change;

				if (cachecolormask)
					rp->r_color = btoct(prp->p_regva) &
								cachecolormask;
			} else {
				attr = prp->p_attrhint;
				while (nextattr = attr->attr_next)
					attr = nextattr;

				ASSERT(attr->attr_end == \
					prp->p_regva + ctob(prp->p_pglen));

				if ((attr->attr_attrs != pattr.attr_attrs)
					|| !aspm_isdefaultpm(pas->pas_aspm, 
						attr->attr_pm, MEM_DATA)) {
					nextattr = attr->attr_next =
						kmem_zone_alloc(attr_zone,
								KM_SLEEP);
					nextattr->attr_start = attr->attr_end;

					nextattr->attr_pm = aspm_getdefaultpm(
						pas->pas_aspm, MEM_DATA);

					attr = nextattr;
					attr->attr_attrs = pattr.attr_attrs;
					attr->attr_next = NULL;
				}
				attr->attr_end = prp->p_regva +
						ctob(prp->p_pglen + change);
			}
		}

		/* Some pmap initialization is required for private mappings */
		if ((prp->p_flags & PF_PRIVATE) && (pas->pas_flags & PAS_SHARED)) {
			ASSERT(prp->p_pmap == ppas->ppas_pmap);
			pmap_lclsetup(pas, ppas->ppas_utas,
				      prp->p_pmap, 
				      prp->p_regva + ctob(prp->p_pglen), 
				      change);
		} else
			ASSERT(prp->p_pmap == pas->pas_pmap);
			

		prp->p_pglen += change;
		mutex_lock(&pas->pas_preglock, 0);
		if (prp->p_flags & PF_PRIVATE) {
			if (rp->r_flags & RG_PHYS)
				ppas->ppas_physsize += change;
			else {
				ppas->ppas_size += change;
			}
		} else {
			if (rp->r_flags & RG_PHYS)
				pas->pas_physsize += change;
			else {
				pas->pas_size += change;
			}
		}
		mutex_unlock(&pas->pas_preglock);
		rp->r_pgsz += change;
	}

#if R4000
	if (ppas->ppas_utas == &curuthread->ut_as) {
		/* 
		 * If crossing a segment boundary, blast out the 
		 * wired slots for R4000.  This prevents problems 
		 * with handling Tlbinvalid exceptions in the fast 
		 * segment handler in locore.s
		 */
		if (ctos(end) != ctos(end-change))
			newptes(pas, ppas, prp->p_flags & PF_SHARED);
	}
#endif
	return 0;
}

/*
 * remove any pregions that have been saved in pas_tsave that might
 * overlap a 'real' pregion
 */
static void
trim_tsave(pas_t *pas, ppas_t *ppas, char *start, char *end)
{
	preg_t **ppprp = &pas->pas_tsave;
	preg_t *prp;

	while (prp = *ppprp) {
		if (prp->p_regva + ctob(prp->p_pglen) > start &&
		    prp->p_regva < end) {
			*ppprp = PREG_GET_NEXT_LINK(prp);
			reglock(prp->p_reg);
			/*
			 * For processors with dual tlb entries, it is
			 * possible that accessing a vaddr might pull in
			 * the entry for the buddy page. Then, if the 
			 * buddy vpage is elfmap'ed to a new object,
			 * unless we invalidate the old mapping, we will
			 * keep seeing the old mapping. Bug 614040.
			 */
			vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
#if R4000 || R10000
				  RF_TSAVE);
#else
				  RF_TSAVE|RF_NOFLUSH);
#endif
		} else {
			ppprp = PREG_LINK_LVALUE(prp);
		}
	}
}

void
remove_tsave(pas_t *pas, ppas_t *ppas, int flags)
{
	register preg_t *prp, *tsave;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	if (tsave = pas->pas_tsave) {
		while (prp = tsave) {
			tsave = PREG_GET_NEXT_LINK(prp);
			reglock(prp->p_reg);
			vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
					RF_NOFLUSH|RF_TSAVE);
		}
		if (!(flags & RF_NOFLUSH))
			newptes(pas, ppas, 1);
		pas->pas_tsave = NULL;
	}
}

/*
 * Check that growth of a pregion is legal.
 * Returns 0 if legal, -1 if illegal.
 *
 * Code can handle arbitrary ordered regions with multiple hitolo regions
 */
static int
chkgrowth(
	preg_t *prp, 		/* pregion being attached or grown 	*/
	pas_t *pas,
	ppas_t *ppas,
	caddr_t	start, 		/* start of new area			*/
	caddr_t	end,		/* end of new area			*/
	boolean_t chktsave)
{
	preg_t *tprp;
	uvaddr_t hiusrattach;

	hiusrattach = pas->pas_hiusrattach;
	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	/* first make sure not outside user virtual limits */
	if ((__psunsigned_t)start > (__psunsigned_t)hiusrattach
	 || (__psunsigned_t)end > (__psunsigned_t )hiusrattach
	 || (__psunsigned_t)end < (__psunsigned_t)start)
		return(-1);

	if ((prp->p_flags & (PF_PRIVATE|PF_NOSHARE)) == (PF_PRIVATE|PF_NOSHARE)) {
		tprp = PREG_FINDANYRANGE(&ppas->ppas_pregions,
			(__psunsigned_t) start, (__psunsigned_t) end,
			PREG_INCLUDE_ZEROLEN);
	} else {
		/* can't overlap anything */
		tprp = PREG_FINDANYRANGE(&pas->pas_pregions,
				(__psunsigned_t) start, (__psunsigned_t) end,
				PREG_INCLUDE_ZEROLEN);
		if (tprp == NULL)
			tprp = PREG_FINDANYRANGE(&ppas->ppas_pregions,
				(__psunsigned_t) start, (__psunsigned_t) end,
				PREG_INCLUDE_ZEROLEN);
	}
	if (tprp) {
		/*
		 * There is some clash; If it's a stack region that we are
		 * trying to attach, and the stack's length is currently
		 * zero, then it's okay iff the clash occors at the very
		 * beginning of the clashing region --- the stack is going to
		 * grow down, and hence, once grown there will not be any
		 * overlap.
		 */
		if ((start != end) ||
				(tprp->p_regva != start) ||
				prp->p_type != PT_STACK)
			return(-1);
	}

	/* no region clashes with [start, end) */
	/*
	 * if this process has any saved pregions, remove any that
	 * overlap what is being requested here
	 */
	if (chktsave && pas->pas_tsave)
		trim_tsave(pas, ppas, start, end);

	return(0);
}

/*
 * Pre-read entire section - used for coff 410 and non-aligned elf
 */
int
loadreg(register preg_t	*prp,
	pas_t		*pas,
	ppas_t		*ppas,
	caddr_t		vaddr,
	vnode_t		*vp,
	off_t		off,
	size_t		count)
{
	register reg_t	*rp;
	int		error;
	ssize_t		resid;
	caddr_t		zero_start;
	size_t		zero_count;

	rp = prp->p_reg;
	ASSERT(mrislocked_update(&rp->r_lock));

	/*
	 * Grow the region to the proper size to load the file.
	 * Grow to RWX so can read in pages, caller will reset prots
	 */
	ASSERT(prp->p_pglen == 0);
	if (error = vgrowreg(prp, pas, ppas, PROT_RWX,
					btoc(count + (vaddr - prp->p_regva))))
		return error;

	ASSERT(rp->r_loadvnode == NULL && rp->r_shmid == 0);
	rp->r_loadvnode = vp;
	rp->r_fileoff = ctooff(offtoct(off));
	rp->r_flags |= RG_DFILL | RG_LOADV;

	/*	We must unlock the region here because we are going
	 *	to fault in the pages as we read them.  No one else
	 *	will try to use the region before we finish because
	 *	the vnode's pagelock is held
	 */
	regrele(rp);

	/* Unlock now, since we are going to fault in the pages. */
	mrunlock(&pas->pas_aspacelock);

	VN_HOLD(vp);		/* for r_loadvnode */
	error = vn_rdwr(UIO_READ, vp, vaddr, count, off, UIO_USERSPACE, 0, 0,
			get_current_cred(), &resid, &curuthread->ut_flid);

	if (error == 0) {
		/* Clear the last (unused)  part of the last page. */
		zero_start = vaddr + count;
		zero_count = (NBPP - poff(zero_start)) & (NBPP-1);
		bzero(zero_start, zero_count);

		/*
		 * The read and the bzero above have left dirty primary
		 * cache lines.  Since we could have been loading a region
		 * that contains instructions, we have to write-back the
		 * data from the primary cache so that the instruction cache
		 * won't fetch stale data from memory.
		 */

		cache_operation(vaddr, count + zero_count,
				CACH_ICACHE_COHERENCY);
	}

	mrupdate(&pas->pas_aspacelock);
	reglock(rp);

	/*
	 * now that we're done reading in object - turn off DFILL
	 * so that bss gets zeroed!
	 */
	rp->r_flags &= ~RG_DFILL;
	return resid ? ENOEXEC : 0;
}

/*
 * Find the pregion of a particular type.
 */
preg_t *
findpregtype(pas_t *pas, ppas_t *ppas, unchar type)
{
	register preg_t	*prp;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	for (prp = PREG_FIRST(pas->pas_pregions); prp ; prp = PREG_NEXT(prp))
		if (prp->p_type == type)
			return(prp);

	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp))
		if (prp->p_type == type)
			return(prp);

	return(NULL);
}

/*
 * Locate first (in address order) pregion that maps any of start to end.
 * Return if there is a pregion in the private or the shared list 
 * based on the private only flag.
 */
preg_t *
findfpreg_select(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t start,
	uvaddr_t end,
	int privateonly)
{
	preg_set_t *pregstart;
	preg_t *prp;

	ASSERT(end >= start);
	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	if (privateonly)
		pregstart = &ppas->ppas_pregions;
	else
		pregstart = &pas->pas_pregions;
	prp = (preg_t *) PREG_FINDADJACENT(pregstart,
				(__psunsigned_t) start, PREG_SUCCEED);
	if (prp && (prp->p_regva < end))
		return prp;
	return NULL;
}

/*
 * Locate first (in address order) pregion that maps any of start to end.
 */
preg_t *
findfpreg(pas_t *pas, ppas_t *ppas, uvaddr_t start, uvaddr_t end)
{
	register preg_t	*prp;

	ASSERT(end >= start);
	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	prp = (preg_t *) PREG_FINDADJACENT(&ppas->ppas_pregions,
				(__psunsigned_t)start, PREG_SUCCEED);
	if (prp && (prp->p_regva < end))
		return prp;

	prp = (preg_t *) PREG_FINDADJACENT(&pas->pas_pregions,
				(__psunsigned_t) start, PREG_SUCCEED);
	if (prp && (prp->p_regva < end))
		return prp;
	return NULL;
}

/*
 * Locate pregion that maps vaddr, but include zero-length pregions.
 * We could use the avl tree to do this faster, but this is not perf.
 * critical at this point.
 */
preg_t *
findanypreg(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr)
{
	preg_t	*prp;
	char	*prend;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp)) {
		prend = (prp->p_pglen == 0) ? prp->p_regva : 
				(prp->p_regva + ctob(prp->p_pglen) - 1);
		if ((prend >= vaddr) && (prp->p_regva <= vaddr))
			return(prp);
	}

	for (prp = PREG_FIRST(pas->pas_pregions); prp ; prp = PREG_NEXT(prp)) {
		prend = (prp->p_pglen == 0) ? prp->p_regva : 
				(prp->p_regva + ctob(prp->p_pglen) - 1);
		if ((prend >= vaddr) && (prp->p_regva <= vaddr))
			return(prp);
	}

	return(NULL);
}

/*
 * Locate process region for a given virtual address.
 */
preg_t *
findpreg(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr)
{
	register preg_t	*prp;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	if (prp = PREG_FINDRANGE(&ppas->ppas_pregions, (__psunsigned_t) vaddr))
		return prp;
	return(PREG_FINDRANGE(&pas->pas_pregions, (__psunsigned_t) vaddr));
}

/*
 * Return pointer to attribute structure that maps prp's vaddr.
 */
attr_t *
findattr(preg_t *prp, register char *vaddr)
{
	register attr_t *attr = prp->p_attrhint;

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	if (attr->attr_start > vaddr)
		attr = &prp->p_attrs;

	ASSERT(vaddr >= attr->attr_start);

	while (vaddr >= attr->attr_end) {
		ASSERT(attr->attr_next);
		attr = attr->attr_next;
	}
	return (prp->p_attrhint = attr);
}

/*	Return pointer to first attribute structure that maps prp's
 *	address range starting at ``start'' to ``end''.
 *	Attribute structs at beginning and end are clipped if they map
 *	too much.
 */
attr_t *
findattr_range(preg_t *prp, register char *start, register char *end)
{
	register attr_t *attr;

	ASSERT(!poff(start) && !poff(end));
	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	attr_coalesce(prp);	/* clean up from previous calls if possible */
	attr = &prp->p_attrs;	/* can't use the hint after a coalesce */
	ASSERT(start >= attr->attr_start);

	/*	Special case: region hasn't been grown yet.  Only one
	 *	attr struct, so just return it.
	 */

	if (attr->attr_start == attr->attr_end) {
		ASSERT(start == end);
		return attr;
	}

	while (start >= attr->attr_end) {
		ASSERT(attr->attr_next);
		attr = attr->attr_next;
	}

	if (attr->attr_start != start)
		attr = attr_clip(attr, start, MIN(attr->attr_end, end));

	prp->p_attrhint = attr;		/* first one in range */

	if (attr->attr_end != end) {

		while (attr->attr_end < end) {
			attr = attr->attr_next;
			ASSERT(attr);
		}

		if (attr->attr_end != end)
			(void) attr_split(attr, end);
	}

	return (prp->p_attrhint);
}

/*
 * Same as find attribute range but it will not do coalescing or clipping.
 * Assumes that this function is called with the region lock in access mode.
 * Currently called from unuseracc and pm_getpmhandles( . If an exact match
 * is not found it returns NULL.
 */
/* ARGSUSED */
attr_t *
findattr_range_noclip(preg_t *prp, register char *start, register char *end)
{
	register attr_t *attr;

	ASSERT(!poff(start) && !poff(end));
	ASSERT(mrislocked_update(&prp->p_reg->r_lock) ||
		mrislocked_access(&prp->p_reg->r_lock));

	attr = &prp->p_attrs;
	ASSERT(start >= attr->attr_start);

	/*	Special case: region hasn't been grown yet.  Only one
	 *	attr struct, so just return it.
	 */
	if (attr->attr_start == attr->attr_end) {
		ASSERT(start == end);
		return attr;
	}

	while (start >= attr->attr_end) {
		ASSERT(attr->attr_next);
		attr = attr->attr_next;
	}

	prp->p_attrhint = attr;		/* first one in range */

	return (prp->p_attrhint);
}

/*
 * Duplicate an attribute chain
 */
/* ARGSUSED2 */
static void
attr_dup(register preg_t *prp, preg_t *nprp)
{
	register attr_t *attr;
	attr_t *nattr, **lattr;
	/* REFERENCED */
        struct pm* pm;

	ASSERT(nprp->p_attrs.attr_next == NULL);
	nprp->p_attrhint = &nprp->p_attrs;

        /*
         * Unset the pm in the base
         * attr before overwriting it.
         */
        attr_pm_unset(&nprp->p_attrs);
        
        /*
         * Copy the source attrs into the new base
         * (overwrite pm)
         */
	nprp->p_attrs = prp->p_attrs;

        /*
         * Copy the pm from the base attr in the
         * source pregion.
         */
        pm =  attr_pm_get(&prp->p_attrs);
        
        /*
         * Reset pm to correct value
         */
        attr_pm_set(&nprp->p_attrs, pm);

	if (nprp->p_attrs.attr_watchpt)
		nprp->p_attrs.attr_watchpt = 0;
	if (nprp->p_attrs.attr_lockcnt)
		nprp->p_attrs.attr_lockcnt = 0;
	lattr = &nprp->p_attrs.attr_next;

	for (attr = prp->p_attrs.attr_next; attr; attr = attr->attr_next) {
		nattr = kmem_zone_alloc(attr_zone, KM_SLEEP);

                /*
                 * Copy the pm from the source attr
                 */
                pm =  attr_pm_get(attr);

                /*
                 * Copy the actual attrs
                 */
		*nattr = *attr;
                
		/*
		 * Some attributes don't tmake sense to copy
		 */
		if (nattr->attr_watchpt)
			nattr->attr_watchpt = 0;
		if (nattr->attr_lockcnt)
			nattr->attr_lockcnt = 0;
                /*
                 * Set pm to default pm in specified aspm
                 */
                attr_pm_set(nattr, pm);

		*lattr = nattr;
		lattr = &nattr->attr_next;
	}
	*lattr = NULL;
}

/*
 * Coalesce adjacent attribute structures that are now identical.
 */

static void
attr_coalesce(register preg_t *prp)
{
	register attr_t *attr, *attrnext;

	for (attr = &prp->p_attrs; attrnext = attr->attr_next; attr = attrnext)
	{
		if (attr->attr_attrs == attrnext->attr_attrs &&
                    attr_pm_equal(attr, attrnext)) {
			attr->attr_end = attrnext->attr_end;
			attr->attr_next = attrnext->attr_next;
                        attr_pm_unset(attrnext);
			kmem_zone_free(attr_zone, attrnext);
			attrnext = attr;
		}
	}

	prp->p_attrhint = &prp->p_attrs;
}

/*	Split attr at the passed address border.
*/
static void
attr_split(register attr_t *attr, register char *border)
{
	register attr_t *attrnext = kmem_zone_alloc(attr_zone, KM_SLEEP);

	attrnext->attr_next = attr->attr_next;
	attr->attr_next = attrnext;

	attrnext->attr_attrs = attr->attr_attrs;
	attrnext->attr_end = attr->attr_end;
	attrnext->attr_start = attr->attr_end = border;

        /*
         * We also have to copy the pm
         */
        attr_pm_copy(attrnext, attr);
}

/*
 *	attr_clip(attr, start, end)
 *
 *	Clip the attribute structure into attibute-identical pieces, and
 *	return a pointer to the attribute struct that maps [start, end).
 */
attr_t *
attr_clip(
	register attr_t	*attr,
	register char	*start,
	register char	*end)
{
	ASSERT(attr->attr_start <= start || attr->attr_end > end);

	if (attr->attr_start != start) {
		attr_split(attr, start);
		attr = attr->attr_next;
	}

	if (attr->attr_end != end)
		attr_split(attr, end);

	return attr;
}

/*
 * Load pregion's ptes with protection specified by region flags.
 */
static void
chgpteprot(preg_t *prp, char *vaddr, pgno_t npgs, int prot)
{
	register pde_t	*pte;
	register int	i;
	auto pgno_t	sz;
	auto pgno_t	tsize = npgs;

	/*
	 *	If there are no valid ptes in this region then don't
	 *	go through the motions.
	 *	If write permission is given, there's nothing that
	 *	must be done to the page table entries.
	 *	SPT: if pmap is using Shared Page Tables p_nvalid is
	 *	     not accurate
	 */
	if ((!pmap_spt(prp->p_pmap) && prp->p_nvalid == 0) || prot & PROT_W)
		return;

	new_tlbpid(&curuthread->ut_as, VM_TLBINVAL);

	while (tsize) {
		/*	pmap_probe is only guaranteed to return pointers
		 *	to ptes that exist (may or may not be valid) in
		 *	the pmap.  That's adequate, since those that
		 *	don't exist don't need to be changed.
		 */
		pte = pmap_probe(prp->p_pmap, &vaddr, &tsize, &sz);
		if (pte == NULL)
			return;

		i = sz;
		vaddr += ctob(i); 
		tsize -= i;


		while (i > 0) {
			if (pg_getpgi(pte))
				pg_clrmod(pte);
			pte++;
			i--;
		}
	}
}

/*
 * Change protection of a (portion of a) pregion.
 */
void
chgprot(pas_t *pas, preg_t *prp, char *vaddr, pgno_t len, uchar_t prot)
{
	register attr_t *attr;
	register char	*end = vaddr + ctob(len);

	attr = findattr_range(prp, vaddr, end);

	/*
	 * If the address boundaries are part of a large page we
	 * need to downgrade them now.
	 */
	pmap_downgrade_addr_boundary(pas, prp, vaddr, len, PMAP_TLB_FLUSH);

	for ( ; ; ) {
		ASSERT(attr);
		attr->attr_prot = prot;

		if (attr->attr_end == end)
			break;

		attr = attr->attr_next;
	}

	/* Modify ptes to reflect new region protection mode */
	chgpteprot(prp, vaddr, numpages(vaddr, ctob(len)), prot);
}

/*
 * getpsize - get current size in pages of process
 * This sum everything including PHYS mapped areas
 */
pgcnt_t
getpsize(pas_t *pas, ppas_t *ppas)
{
	return pas->pas_size + ppas->ppas_size +
		pas->pas_physsize + ppas->ppas_physsize;
}

/*
 * ispcwriteable - return true if region described by vaddr is steppable
 *	If its a read-only executable AND it is marked ANON|CW OR
 *	if its writable
 */
int
ispcwriteable(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr, reg_t *locked_rp)
{
	preg_t *prp;
	reg_t *rp;
	int rv = 1;
	register attr_t *attr;

	if ((prp = findpreg(pas, ppas, vaddr)) == NULL)
		rv = 0;
	else {
		rp = prp->p_reg;
		if (rp != locked_rp)
			reglock(rp);
		attr = findattr(prp, vaddr);
		if ((attr->attr_prot & PROT_WX) == PROT_X) {
			/* read-only executable */
			if ((rp->r_flags & (RG_ANON|RG_CW)) != (RG_ANON|RG_CW))
				rv = 0;
		} else if (!(attr->attr_prot & PROT_W))
			rv = 0;
		else if (rp->r_flags & RG_PHYS)
			rv = 0;
		if (rp != locked_rp)
			regrele(rp);
	}
	return(rv);
}

/*
 * get address space non-shared so it can be written/breakpointed/single-stepped
 * The main issue here is sproc'd shared executables where we would like
 * to give a private version of the text so each one gets a seperate
 * breakpoint. Other than text, every process that encounters this
 * breakpoint/single-step will see it
 * Since there are no more 'text' sections we go on the assumption that
 * if the page is marked ro-executable then we will make a private copy.
 *
 * Note: if a region is marked read-only but one has write permission on
 * the file - any writes to that region will overwrite the file
 */
int
vgetprivatespace(pas_t *pas, ppas_t *ppas, caddr_t cmaddr, preg_t **prpp)
{
	preg_t *prp;
	reg_t *rp;
        struct pm *pm;
	preg_t *nprp;
	int error = 0;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	if ((prp = findpreg(pas, ppas, cmaddr)) == NULL)
		return ENOENT;

	if ((rp = prp->p_reg)->r_flags & RG_PHYS)
		return EINVAL;

	if ((rp->r_flags & (RG_ANON|RG_CW)) == (RG_ANON|RG_CW))
		goto done;

	reglock(rp);

	/*
	 * only need to do things to read-only executables
	 */
	if ((findattr(prp, cmaddr)->attr_prot & PROT_WX) == PROT_X) {
		if (prp->p_flags & PF_SHARED &&
		    !(pas->pas_flags & PAS_PSHARED)) {
			/* make this region private - copy on write */
                        pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
			error = dupreg(pas, prp, pas, ppas, prp->p_regva, 
				      prp->p_type, PF_NOSHARE|PF_DUP|PF_PRIVATE, 
				      pm, RF_FORCE, &nprp);
                        aspm_releasepm(pm);
			regrele(rp);
			if (error) {
				/*
				 * Override error from dupreg.
				 * We don't need to handle EMEMRETRY.
				 */
				error = EAGAIN;
				goto done;
			}

			/* dupreg dups prp */
			prp = nprp;
			rp = prp->p_reg;
			rp->r_flags &= ~RG_TEXT;
			newptes(pas, ppas, 1);
		} else {
			reg_t *new_rp;

			if ((new_rp = replacereg(pas, ppas, prp)) == NULL)
				error = EAGAIN;
			else
				rp = new_rp;
		}
	}
	regrele(rp);

done:
	if (error == 0 && prpp)
		*prpp = prp;

	if (error == EAGAIN)
		nomemmsg("address space setup for debugging");

	return error;
}

/* 
 * return the size of a region in pages
 */
unsigned
getpregionsize(register preg_t *prp)
{
	register reg_t *rp = prp->p_reg;
	
	if ((prp->p_type == PT_MAPFILE) &&
	    (rp->r_flags & (RG_AUTOGROW|RG_AUTORESRV))) {
		if (rp->r_flags & RG_ANON) {
			pgno_t		high_apn;

			/*
			 * This is an autogrow, anon region (i.e. /dev/zero).
			 * Find the highest anon lpn and use that for the
			 * region size.  
			 */

			anon_getsizes(rp->r_anon, vtoapn(prp, prp->p_regva),
				      prp->p_pglen, NULL, &high_apn);

			/*
			 * Backing store could end before start of
			 * this pregion, so be careful not to return
			 * a bogus (negative) value.
			 */
			high_apn -= prp->p_offset;
			return MAX(high_apn + 1, 0);

		} else {
			struct vattr va;
			/*REFERENCED*/
			int unused;
	
			va.va_mask = AT_SIZE;
			VOP_GETATTR(rp->r_vnode, &va, ATTR_LAZY,
					   get_current_cred(), unused);
			return min(offtoc(va.va_size),prp->p_pglen);
		}

	} else
		return prp->p_pglen;
}

/*
 * Count the number of valid pages in the given pregion range.
 */
pgcnt_t
count_valid(preg_t *prp, caddr_t vaddr, pgno_t len)
{
	register pgcnt_t nvalid = 0;
	register pde_t	*pd;
	pgno_t		sz;

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	while (len) {
		pd = pmap_probe(prp->p_pmap, &vaddr, &len, &sz);

		if (pd == NULL)
			break;

		vaddr += ctob(sz);
		len -= sz;

		/*
		 * There is an inherent race between this and migration as
		 * that is not protected by the region lock. We need a way
		 * serialize this.
		 */
		while (sz > 0) {
			pg_pfnacquire(pd);
			if (pg_isvalid(pd) || pg_isshotdn(pd) || pg_ismigrating(pd))
				nvalid++;
			pg_pfnrelease(pd);
			sz--; 
			pd++;
		}
	}

	return nvalid;
}

/*
 *	vp	incore vnode pointer
 *	size	new logical size; all mapping beyond
 *		size's page are remapped.
 *	flush	write pages out to disk.
 *
 *	All logical pages get disassociated from physical pages.
 *	The pages get unhashed.
 *	It is important that remapf NEVER fail - i.e. once remapf
 *		returns there must be NO pages beyond size still
 *		validly hashed.
 *		This is difficult since
 *		1) there might be locked (via mpin) pages
 *		2) someone might be doing physio ...
 *		
 *		Therefore, mysnc1/tossmpages will set all locked pages P_BAD
 *			so that they will not be found again.
 */
void
remapf(vnode_t *vp, off_t size, int flush)
{
	register reg_t	*rp;
	register preg_t	*preg;
	register caddr_t start;
	int		gtpgsflags = GTPGS_TOSS;
	int		s;

	ASSERT(vp->v_type == VREG);

	if (flush == 0)
		gtpgsflags |= GTPGS_NOWRITE;

	/* Promote to the next integral page. */
	size = ctooff(offtoc(size));

start_over:
	s = mutex_spinlock(&mreg_lock);
	for (preg = vp->v_mreg; preg != (preg_t *)vp; preg = preg->p_vchain) {
		rp = preg->p_reg;
		ASSERT(rp);
		rp->r_nofree++;		/* bump cnt so region isn't freed */
		preg->p_nofree++;	/* bump cnt so pregion isn't freed */
		mutex_spinunlock(&mreg_lock, s);
		reglock(rp);

		/*
		 * See if the (p)region has been detached while we
		 * were asleep.  If so, free the (p)region now and
		 * start over again from the beginning.  We
		 * can't continue from where we were in the
		 * list since our preg pointer is no good
		 * anymore in this case.  (See freereg()/freepreg()
		 * for more details).
		 */
		if (rp->r_type == RT_UNUSED || preg->p_reg == NULL) {

			ASSERT( !(preg->p_reg && rp->r_type == RT_UNUSED));
			ASSERT(preg->p_nofree <= rp->r_nofree);

			/*
			 * Decrement our hold on the pregion. If there are
			 * no other holds and the pregion no longer points
			 * to the region, free the structure. freepreg has
			 * done all the work except for this.
			 * No mreg_lock needed - see below.
			 */
			if (--preg->p_nofree == 0 && preg->p_reg == NULL)
				kmem_zone_free(preg_zone, preg);

			/*
			 * Once the region goes UNUSED and we 
			 * get here, then no one else can 
			 * increment the count (cause the
			 * region isn't on the vnode list
			 * anymore), nor can they decrement it
			 * until they get the region lock, so
			 * it's safe to update the count without
			 * holding the spin lock. 
			 */
			if (--rp->r_nofree == 0 && rp->r_type == RT_UNUSED)
				freereg(NULL, rp); /* we were last */
			else
				regrele(rp);

			goto start_over;
		}

		if (rp->r_fileoff + ctob(preg->p_offset + preg->p_pglen) <= size) {
			s = mutex_spinlock(&mreg_lock);
			rp->r_nofree--;
			preg->p_nofree--;
			regrele(rp);
			continue;
		}

		/*
		 * Extract all valid cached pages from mapping processes.
		 * msync1 calls the pager which will disassociate the page
		 * from the region.  With GTPGS_NOWRITE the pager will just
		 * just free the pages without writing them to disk.
		 */
		start = MAX(preg->p_regva + 
			   (size > (rp->r_fileoff + ctob(preg->p_offset)) ?  
			   (size - (rp->r_fileoff + ctob(preg->p_offset))) : 0),
			    preg->p_regva);

		(void) msync1(NULL, preg, start, 
		       preg->p_pglen - btoc(start - preg->p_regva), 
		       gtpgsflags);

		s = mutex_spinlock(&mreg_lock);
		rp->r_nofree--;
		preg->p_nofree--;
		regrele(rp);
	}

	mutex_spinunlock(&mreg_lock, s);
}

int
msync1(pas_t *pas, preg_t *prp, uvaddr_t vaddr, pgcnt_t	npgs, int flags)
{
	ASSERT(ismrlocked(&prp->p_reg->r_lock, MR_UPDATE));
	ASSERT(vaddr >= prp->p_regva);
	ASSERT(vaddr + ctob(npgs) <= prp->p_regva + ctob(prp->p_pglen));
	ASSERT(npgs);

	/*
	 * We have to ensure that if the address range spans a large page
	 * that the large page ptes are converted to base page ptes around
	 * the virtual address range. As the ptes are getting modified the
	 * a tlbflush is needed.
	 */
	pmap_downgrade_addr_boundary(pas, prp, vaddr, npgs, PMAP_TLB_FLUSH);

	return getmpages(pas, prp, flags, vaddr, npgs);
}


/*
 * vcache2 - calculate the preferred secondary cache color for a page
 *
 *	If the pagesize for the pregion is the base pagesize, the color
 *	is based on the logical pagenumber of the page within the region.
 *	To randomize colors for different regions, the r_gen (essentially
 *	a random number) is added to the color so that page 0 of regions
 *	are distributed across the scache.
 *
 *	If the pagesize is larger than the base pagesize, things are more
 *	complicated due to mapping restrictions in the TLBs - specifically
 *	the requirement that the starting vaddr of a large page must be
 *	0 mod the page size & must be mapped to a physical address that
 *	is also 0 mod the page size. The algorithm is further complicated
 *	by the desire to have a range of virtual address mapped by pages
 *	with differing page sizes & having the color assignment such that
 *	maximal use is made of the scache.
 *		See comments below for more details.
 */
uint
vcache2(preg_t *prp, attr_t *attr, pgno_t lpn)
{
	uint		color, pgsz;
	uint		indx, pagmul, delta;
	__psint_t	addr;


	pgsz = PMAT_GET_PAGESIZE(attr);
	if (pgsz == NBPP) {
		/*
		 * For base pagesizes, the color of a page is equal to the
		 * region r_gen + the logical page number within the region.
		 * This gives a pseudo random distribution of pages across the scache.
		 */
		color = ((uint)(prp->p_reg->r_gen) + lpn);
	} else {
		indx = PAGE_SIZE_TO_INDEX(PMAT_GET_PAGESIZE(attr));

		/*
		 * For regions with large pages, the color is the same as for regions
		 * with a base pagesize with 2 exceptions:
		 *	- some low bits of the r_gen field are discarded. Since
		 *	  the page allocator ignores them, it has no effect as far as 
		 *	  the FIRST base page of a large page is concerned. However,
		 *	  ALL base pages in a large page must have the same
		 *	  color (after ignoring low bits) & if we dont clear the
		 *	  low order bits, this wont be true after adding lpn. 
		 *	  The number of bits to ignore is a function of 
		 *	  page size - 2 bits for 64K, 4 bits for 256k, 6 bits for 1m, etc. 
		 *
		 *	- add a large-page-size dependent fudge factor to the
		 *	  page color so that the effective color used by the page
		 *	  allocator is increased by 1. This is because 1) some pages
		 *	  of the bss section get assigned before we know that large pages 
		 *	  are suppose to be used, OR 2) some pregions dont start
		 *	  on boundaries that allow large pages to assigned.
		 *	  Adding the fudge factor prevents duplicate colors 
		 *	  from being assigned for blocks of consecutive
		 *	  pages as we cross boundaries where page sizes increase. 
		 *
		 *	  Specifically (example is for p_reg = 0x108, regva=0x10010000):
		 *		vaddr	  pagesize      color	fudged-color
		 *		10010000     16k	  108    108
		 *		10014000     16k	  109    109
		 *		10018000     16k	  106    116
		 *		1001c000     16k	  107    117
		 *		10020000     64k	  108    118
		 *		10030000     64k	  10c    11c
		 *		10040000     64k	  110    120
		 *
		 */
		pagmul = (1 << (2*indx));
		color = (((uint)(prp->p_reg->r_gen))&(~(pagmul-1))) + lpn + pagmul;

		/*
		 * Now offset the color by the number of base pages that
		 * are required to get to an offset that is a multiple of the
		 * large page size. If the region was not attached at an address
		 * that is 0 mod large-page-size, some small pages must be
		 * assigned until we get to a boundary that allows for large
		 * pages. If we dont offset page color for small pages, there
		 * will be a discontinuity in page colors when we change
		 * pagesizes. 
		 *
		 * Note that this is effective only if all processes that share 
		 * the page either 1) agree on it's pagesize, OR 2) map the 
		 * region at the same (or compatible) addresses.
		 */
		addr = ((__psint_t) (prp->p_regva)) & (pgsz-1);
		if (addr) {
			delta = btoc(addr);
			color += delta;
		}

	}


	/*
	 * Here is an ugly hack!!! The value of NOCOLOR is 128. This
	 * is unfortunate because on some systems there are more than 128
	 * valid page colors & the algorithms above will generate 128
	 * as a valid color. A simple fix would be to change the value
	 * of NOCOLOR to a large value. This cant be done in 6.5.x because
	 * of binary compatibility issues. 
	 *
	 * Since most place use color as an opaque data type & simply 
	 * pass to to the page allocators, we test the color here & if 
	 * it value is equal to NOCOLOR, we change it to another value. 
	 * The new value is guaranteed not to be a valid color. 
	 *
	 * The page allocator tests color against this value & undoes
	 * the hack.
	 */
	if (color == NOCOLOR)
		color = 0xbadc;

	return color;
}


/*
 * pfntocachekey - convert a pfn to the corresponding page color cachekey
 *
 * This function converts a pfn to the equivalent page cachekey. 
 * The cachekey, when passed as a cachekey to the pagealloc routines,
 * will allocate a page that has the same color as the original page.
 */
uint
pfntocachekey(pfn_t pfn, size_t size)
{

	uint    	color;

	color = pfn & PHEADMASK(MIN_PGSZ_INDEX);

	/*
	 * I'm not sure about this but right now it doesnt matter since
	 * we dont know how to migrate large pages.
	 */
	if (size != NBPP) 
		color = color << (PAGE_SIZE_TO_INDEX(size)*2);

	/*
	 * See vcache2 for a description of this hack.
	 */
	if (color == NOCOLOR)
		color = 0xbadc;

	return color;
}

