/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.52 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "ksys/as.h"
#include "sys/alenlist.h"
#include "sys/atomic_ops.h"
#include "as_private.h"
#include <sys/ckpt.h>
#include <core.out.h>
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/getpages.h"
#include "sys/kabi.h"
#include "sys/kmem.h"
#include "sys/lock.h"
#include "sys/numa.h"
#include "pmap.h"
#include "sys/resource.h"
#include "ksys/rmap.h"
#include "sys/sysmacros.h"
#include "sys/vnode.h"
#include "sys/prctl.h" /* XXX remove me */
#include "ksys/vpag.h"
#include "sys/lpage.h"
#include <sys/ckpt_procfs.h>
#ifdef CKPT
#include <sys/uthread.h>
#endif
/*
 * implementation routines for physical address space layer
 */
#if CKPT
static void pas_getckptmap(preg_t *, ckpt_getmap_t *);
#endif
static zone_t *pas_zone;
static zone_t *ppas_zone;

void
pas_init(void)
{
	pas_zone = kmem_zone_init(sizeof(pas_t), "pas");
	ppas_zone = kmem_zone_init(sizeof(ppas_t), "ppas");
}

/*
 * allocate a pas and the first ppas
 */
pas_t *
pas_alloc(vas_t *vas, struct utas_s *who, int is64)
{
	pas_t *pas;
	ppas_t *ppas;
	long seq = vas->vas_gen;

	/* init physical and private physical data structures */
	pas = kmem_zone_zalloc(pas_zone, KM_SLEEP);
	ppas = kmem_zone_zalloc(ppas_zone, KM_SLEEP);

	pas->pas_refcnt = 1;
	pas->pas_plist = ppas;
	mrlock_init(&pas->pas_aspacelock,
		    MRLOCK_DBLTRIPPABLE|MRLOCK_ALLOW_EQUAL_PRI,
		    "aspace", -1);
	PREG_INIT(&pas->pas_pregions);
	init_mutex(&pas->pas_preglock, MUTEX_DEFAULT, "preglock", seq);
	mrlock_init(&pas->pas_plistlock, MRLOCK_DBLTRIPPABLE, "plistlock",-1);
	init_mutex(&pas->pas_brklock, MUTEX_DEFAULT, "brklock", seq);

	/*
	 * init ppas - private portion
	 * This first ppas never is marked as PPAS_SPROC, so its stackmax
	 * always comes from pas_stkmax, and any setrimit/prctl call will
	 * change pas_stkmax - this is what is desired/needed for uthreads.
	 */
	spinlock_init(&ppas->ppas_utaslock, "utas");
	PREG_INIT(&ppas->ppas_pregions);
	ppas->ppas_next = NULL;
	ppas->ppas_prevp = &pas->pas_plist;
	ppas->ppas_refcnt = 1;
	ppas->ppas_utas = who;
	if (is64) {
#if _MIPS_SIM == _ABI64
		pas->pas_nextaalloc = LOWDEFATTACH64;
		pas->pas_flags |= PAS_64;
		pas->pas_hiusrattach = (uvaddr_t)HIUSRATTACH_64;
#else
		panic("64 bit AS in 32 bit mode\n");
#endif
	} else {
		pas->pas_nextaalloc = LOWDEFATTACH;
		pas->pas_hiusrattach = (uvaddr_t)HIUSRATTACH_32;
	}
	pas->pas_lastspbase = pas->pas_hiusrattach;

	/* Initialize the behavior descriptor and put it on the chain. */
	bhv_desc_init(&pas->pas_bhv, pas, vas, &pas_ops);
	bhv_insert_initial(&vas->vas_bhvh, &pas->pas_bhv);

	return pas;
}

/*
 * add a new private address space to AS
 */
void
ppas_alloc(pas_t *pas,
	ppas_t *ppas,
	vasid_t *cvasid,
	asid_t *casid,
	struct utas_s *who)
{
	ppas_t *nppas;
	vas_t *vas = BHV_TO_VAS(&pas->pas_bhv);

	cvasid->vas_obj = vas;

	atomicAddInt(&vas->vas_refcnt, 1);

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	nppas = kmem_zone_zalloc(ppas_zone, KM_SLEEP);

	/* init ppas - private portion */
	spinlock_init(&nppas->ppas_utaslock, "utas");
	PREG_INIT(&nppas->ppas_pregions);
	nppas->ppas_refcnt = 1;
	nppas->ppas_flags = ppas->ppas_flags & (PPAS_ISO);
	nppas->ppas_utas = who;

	/* insert in list */
	mrupdate(&pas->pas_plistlock);
	nppas->ppas_next = pas->pas_plist;
	nppas->ppas_prevp = &pas->pas_plist;
	if (pas->pas_plist)
		pas->pas_plist->ppas_prevp = &nppas->ppas_next;
	pas->pas_plist = nppas;
	atomicAddInt(&pas->pas_refcnt, 1);
	mrunlock(&pas->pas_plistlock);

	cvasid->vas_pasid = (aspasid_t)nppas;

	/* construct handle */
	ASSERT(VASID_TO_VAS(*cvasid) == vas);
	casid->as_obj = (struct __as_opaque *)vas;
	casid->as_pasid = cvasid->vas_pasid;
	casid->as_gen = vas->vas_gen;
}

/*
 * remove a private address space section from the list
 */
void
ppas_free(pas_t *pas, ppas_t *ppas)
{
	ppas_t *tpq;

	mrupdate(&pas->pas_plistlock);
	ASSERT(PREG_FIRST(ppas->ppas_pregions) == NULL);
	ASSERT(ppas->ppas_refcnt == 0);
	ASSERT(ppas->ppas_size == 0);
	ASSERT(ppas->ppas_physsize == 0);
	/*ASSERT(ppas->ppas_utas == NULL);*/
#ifdef DEBUG
	{
	ppas_t *tp;
	for (tp = pas->pas_plist; tp; tp = tp->ppas_next) {
		if (tp == ppas) {
			/* found it */
			break;
		}
	}
	ASSERT(tp);
	}
#endif
	if (tpq = ppas->ppas_next)
		tpq->ppas_prevp = ppas->ppas_prevp;
	*(ppas->ppas_prevp) = tpq;
	mrunlock(&pas->pas_plistlock);

	if (ppas->ppas_pmap)
		pmap_destroy(ppas->ppas_pmap);
	spinlock_destroy(&ppas->ppas_utaslock);
	kmem_zone_free(ppas_zone, ppas);
}

/*
 * free physical address space structures
 */
void
pas_free(pas_t *pas)
{
	vas_t *vas = BHV_TO_VAS(&pas->pas_bhv);

	ASSERT(pas->pas_plist == NULL);
	ASSERT(pas->pas_refcnt == 0);
	ASSERT(pas->pas_nlockpg == 0);
	ASSERT(pas->pas_size == 0);
	ASSERT(pas->pas_physsize == 0);
	ASSERT(pas->pas_tsave == NULL);
	ASSERT(PREG_FIRST(pas->pas_pregions) == NULL);

	aspm_destroy(pas->pas_aspm);

	/* 
	 * Release the reference to VPAGG.
	 */
	if (pas->pas_vpagg)
		VPAG_RELE(pas->pas_vpagg);

	if (pas->pas_pmap)
		pmap_destroy(pas->pas_pmap);
	if (pas->pas_faultlog)
		kern_free(pas->pas_faultlog);
	mrfree(&pas->pas_aspacelock);
	mrfree(&pas->pas_plistlock);
	mutex_destroy(&pas->pas_preglock);
	mutex_destroy(&pas->pas_brklock);
	bhv_remove(&vas->vas_bhvh, &pas->pas_bhv);
	kmem_zone_free(pas_zone, pas);
}

/*
 * pas_getattr sub-opts that scan pregion list for all pregions belonging
 * to a particular thread.
 */
#define	VPIN_USE(vp)	((vp) != NULL && (VN_CMP((vp), fvp) \
			 || contained && (vp)->v_vfsp == fvp->v_vfsp))
/* ARGSUSED */
int
pas_getattrscan(
	pas_t *pas,
	ppas_t *ppas,
	as_getattrop_t op,
	as_getattrin_t *in,
	as_getattr_t *asattr)
{
	struct pregion *prp;
	register attr_t *attr;
	uvaddr_t addr;
	int shared = 0;
	pgcnt_t nattr = 0;
	reg_t *rp;
	int gfxlookup_rv = 0;
	int gfxrecycle_rv = 0;
	int contained;
	struct vnode *fvp;
	ckpt_getmap_t rbuf;
	uvaddr_t	brkbase;
#if CKPT
	int ckptcnt = 0;
	uvaddr_t ckptuvaddr;
#endif

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	switch (op) {
	case AS_INUSE:
		contained = in->as_inuse.as_contained;
		fvp = in->as_inuse.as_vnode;
		break;
#if CKPT
	case AS_CKPTGETMAP:
		ckptuvaddr = in->as_ckptgetmap.as_udest;
		break;
#endif
	case AS_GFXLOOKUP:
	case AS_GFXRECYCLE:
		addr = in->as_uvaddr;
		break;
	}

	prp = PREG_FIRST(ppas->ppas_pregions);
again:
	for (; prp; prp = PREG_NEXT(prp)) {
		switch (op) {
		case AS_NATTRS:
			brkbase = pas->pas_brkbase;
			for (attr = &prp->p_attrs; attr; 
						attr = attr->attr_next) {
				nattr++;
				if ((brkbase > attr->attr_start) && 
						(brkbase < attr->attr_end))
					nattr++;
			}
			break;
		case AS_INUSE:
			if ((rp = prp->p_reg) == 0)
				continue;
			if (!VPIN_USE(rp->r_vnode))
				continue;
			if (rp->r_type == RT_MAPFILE)
				return EBUSY;
			break;
		case AS_GFXLOOKUP:
			rp = prp->p_reg;
			if ((rp->r_flags & RG_PHYS) &&
				((void *)rp->r_fault == (void *)addr)) {
				gfxlookup_rv = 1;
				goto done;
			}
			break;
		case AS_GFXRECYCLE:
			if (prp->p_type == PT_GR && prp->p_regva == addr) {
				asattr->as_recycle.as_fltarg = prp->p_reg->r_fltarg;
				asattr->as_recycle.as_recyclearg = prp; /* XXX */
				gfxrecycle_rv = 1;
				goto done;
			}
			break;
#if CKPT
		case AS_CKPTGETMAP:
			if (in->as_ckptgetmap.as_n <= ckptcnt)
				goto done;
			pas_getckptmap(prp, &rbuf);
			if (copyout((caddr_t)&rbuf, ckptuvaddr, sizeof(rbuf)))
				return EFAULT;
			ckptcnt++;
			ckptuvaddr += sizeof(rbuf);
			break;
#endif
		default:
			panic("as_getattrscan");
			/* NOTREACHED */
		}
	}
	if (!shared) {
		shared = 1;
		prp = PREG_FIRST(pas->pas_pregions);
		goto again;
	}

done:
	switch (op) {
	case AS_NATTRS:
		asattr->as_nattrs = nattr;
		break;
#if CKPT
	case AS_CKPTGETMAP:
		asattr->as_nckptmaps = ckptcnt;
		break;
#endif
	case AS_GFXRECYCLE:
		if (gfxrecycle_rv == 0)
			return ENOENT;
		break;
	case AS_GFXLOOKUP:
		if (gfxlookup_rv == 0)
			return ENOENT;
		break;
	}
	return 0;
}

void
pas_ulimit(pas_t *pas, ppas_t *ppas, as_getasattr_t *asattr)
{
	register preg_t	*prp, *dprp, *prp2;
	int doingshd = 0;
	rlim_t size;
	uvaddr_t rv = 0;

	mraccess(&pas->pas_aspacelock);
	/* Find the data region */
	if ((dprp = findpreg(pas, ppas, pas->pas_brkbase - 1)) != NULL) {
		/*
		 *	Compute how much total memory
		 *	the user can still have
		 */
		size = pas->pas_vmemmax;
		if (pas->pas_vmemmax != RLIM_INFINITY)
			size -= ctob(getpsize(pas, ppas));

		if (pas->pas_datamax != RLIM_INFINITY)
			size = MIN(size,
			      (pas->pas_datamax - ctob(dprp->p_reg->r_pgsz)));
		/* to avoid overflow - clamp size at hiusrattach */
		size = MIN(size, (rlim_t)pas->pas_hiusrattach);

		rv = dprp->p_regva + ctob(dprp->p_reg->r_pgsz) + size;
		rv = MIN(rv, (uvaddr_t)pas->pas_hiusrattach);

		/*	Now find the region with a virtual
		 *	address greater than the data region
		 *	but lower than any other region
		 *	See if a contiguous region would
		 *	limit the data region size.
		 */
		prp = PREG_FIRST(ppas->ppas_pregions);
		prp2 = NULL;

doshd:
		for ( ; prp; prp = PREG_NEXT(prp)) {
			if (prp->p_regva <= dprp->p_regva)
				continue;
			if (prp2 == NULL ||
			    prp->p_regva < prp2->p_regva)
				prp2 = prp;
		}
		if (!doingshd) {
			doingshd++;
			prp = PREG_FIRST(pas->pas_pregions);
			goto doshd;
		}
		if (prp2 != NULL)
			rv = MIN(rv, prp2->p_regva);
	}
	mrunlock(&pas->pas_aspacelock);
	asattr->as_brkmax = rv;
}

#if CKPT
/*
 * helper function to fill in info the ckp maps
 */
static void
pas_getckptmap(preg_t *prp, ckpt_getmap_t *rbuf)
{
	uint ckpt;

	reglock(prp->p_reg);

	rbuf->m_mapid = (caddr_t)prp->p_reg;
	rbuf->m_vaddr = prp->p_regva;
	rbuf->m_size = ctob(prp->p_pglen);
	rbuf->m_prots = prp->p_maxprots;
	rbuf->m_maxoff = prp->p_reg->r_maxfsize;
	rbuf->m_flags = 0;

	if (prp->p_reg->r_flags & RG_AUTOGROW)
		rbuf->m_flags |= CKPT_AUTOGROW;
	if (prp->p_reg->r_flags & RG_MAPZERO)
		rbuf->m_flags |= CKPT_MAPZERO;
	if (prp->p_reg->r_flags & RG_AUTORESRV)
		rbuf->m_flags |= CKPT_AUTORESRV;
	if (prp->p_flags & PF_NOSHARE)
		rbuf->m_flags |= CKPT_LOCAL;
	if (prp->p_flags & PF_DUP)
		rbuf->m_flags |= CKPT_DUP;
	/*
	 * check for Posix semaphores
	 */
	if ((prp->p_reg->r_flags & RG_USYNC)||
	    ((prp->p_reg->r_vnode)&&
	     (prp->p_reg->r_vnode->v_flag & VUSYNC)))
		rbuf->m_flags |= CKPT_PUSEMA;

	switch(prp->p_type) {
	case PT_MEM:
		rbuf->m_flags |= CKPT_MEM;
		break;
	case PT_PRDA:
		rbuf->m_flags |= CKPT_PRDA;
		break;
	case PT_STACK:
		rbuf->m_flags |= CKPT_STACK;
		break;
	default:
		break;
	}
	if (ckpt = prp->p_reg->r_ckptflags) {
		if (ckpt & CKPT_EXEC)
			rbuf->m_flags |= CKPT_EXECFILE;
		if (ckpt & CKPT_MMAP)
			rbuf->m_flags |= CKPT_MAPFILE;
		if (ckpt & CKPT_SHM) {
			rbuf->m_flags |= CKPT_SHMID;
			rbuf->m_shmid = prp->p_reg->r_ckptinfo;
		}
		if (ckpt & CKPT_PRIVATE)
			rbuf->m_flags |= CKPT_MAPPRIVATE;
	}
	regrele(prp->p_reg);
}
#endif /* CKPT */

#if NEVER
/*
 * Build a list of ppas's for the pas - w/ refs count for each.
 * We honor various criteria for list building. Note that some criteria
 * tell things to check that INCLUDE, and some to EXCLUDE.
 *
 * Returns 0 if built a list and !0 if no list
 */
int
pas_buildlist(pas_t *pas, pas_blist_t *bl, int crit)
{
	ppas_t *ppas;
	int doit = 0, isiso = 0;
	int np;
	pas_blist_t *nbl, *obl;

	obl = bl;
	bl->blist_next = NULL;
	bl->blist_n = 0;
	np = 0;

	if (crit & PAS_C_ULI) {
		if (pas->pas_flags & PAS_ULI) {
			/* this is knock-out item */
			return 1;
		}
	}
	mraccess(&pas->pas_plistlock);
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		if (crit & PAS_C_ISO) {
			/* basically if any member is isolated just forget it */
			if (ppas->ppas_flags & PPAS_ISO) {
				isiso = 1;
				/* this is knock-out item */
				break;
			}
		}
		if (crit & PAS_C_RSS) {
			if ((pas->pas_rss + ppas->ppas_rss) > pas->pas_maxrss)
				doit = 1;
		} else {
			/* default is to do something! */
			doit = 1;
		}

		/* get room in list */
		if (np >= PAS_BCHUNK) {
			nbl = kmem_alloc(sizeof(*nbl), KM_NOSLEEP);
			if (nbl) {
				bl->blist_n = np;
				np = 0;
				bl->blist_next = nbl;
				nbl->blist_next = NULL;
				nbl->blist_n = 0;
				bl = nbl;
			}
		}

		if (np >= PAS_BCHUNK) {
			/*
			 * no room - ignore and don't add to list
			 * NOTE: we continue so that we scan for all criteria
			 */
			continue;
		}
		if (compare_and_inc_int_gt_zero(&ppas->ppas_refcnt) == 0) {
			/* whoa! ref was zero - this means
			 * we raced with pas_relepasid which is
			 * now waiting in ppas_free for the plistlock
			 */
			continue;
		}
		bl->blist_ppas[np++] = ppas;
	}
	bl->blist_n = np;
	mrunlock(&pas->pas_plistlock);
	if (!doit || isiso) {
		/*
		 * not going to do anything - release all the grot
		 * we accumulated
		 */
		pas_freelist(pas, obl);
		return 1;
	}
	return 0;
}

/*
 * free up a list the 1st list item is the callers, the rest we free
 */
void
pas_freelist(pas_t *pas, pas_blist_t *bl)
{
	ppas_t *ppas;
	pas_blist_t *nbl, *obl;
	int i;

	/* release all ppas's */
	obl = bl;
	while (bl) {
		nbl = bl->blist_next;
		for (i = 0; i < bl->blist_n; i++) {
			ppas = bl->blist_ppas[i];
			ASSERT(ppas->ppas_refcnt > 0);
			if (atomicAddInt(&ppas->ppas_refcnt, -1) == 0)
				ppas_free(pas, ppas);
		}
		if (bl != obl)
			kern_free(bl);
		bl = nbl;
	}
}
#endif

/*
 * Set up PRDA region.  The PRDA is a special one page, demand zero region
 * that is created on the first reference to the page (in vfault).  Vfault
 * will allocate a demand zero page and fill in the process's pid.
 */
int
setup_prda(pas_t *pas, ppas_t *ppas)
{
	reg_t *rp;
	int error;
	preg_t *prp;
	struct pm *pm;

	rp = allocreg(NULL, RT_MEM, RG_ANON);

        pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
	if (error = vattachreg(rp, pas, ppas, (caddr_t)PRDAADDR, (pgno_t)0,
			(pgno_t)0, PT_PRDA, PROT_RW, PROT_RW,
			PF_DUP | PF_NOSHARE, pm, &prp)) {
                aspm_releasepm(pm);
		freereg(pas, rp);
		goto bad;
	}
        aspm_releasepm(pm);
        
	if (error = vgrowreg(prp, pas, ppas, PROT_RW, btoc(PRDASIZE))) {
		(void) vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				 RF_NOFLUSH);
		goto bad;
	}

	regrele(rp);
	return 0;

bad:
	if (error == EAGAIN)
		nomemmsg("prda setup");

	return error;
}

/*
 * unlock prda if kernel has it locked.
 * Called with aspacelock held for update
 */
void
pas_unlockprda(pas_t *pas, ppas_t *ppas, struct prda *prda)
{
	register preg_t *prp;
	/* REFERENCED - by pfdat_release() */
        pfd_t *pfd;
        
	if (prda) {
		/*
		 * Prda area is locked down in memory. Unlock that
		 * before we detach the region.
		 */
		pfd = kvatopfdat(prda);
		page_mapout((caddr_t)prda);
		prp = findpregtype(pas, ppas, PT_PRDA);
		ASSERT(prp);
		if (prp->p_flags & PF_PRIVATE) {
			/* an sproc */
			ASSERT(ppas->ppas_flags & PPAS_LOCKPRDA);
			ppas_flagclr(ppas, PPAS_LOCKPRDA);
		} else {
			/* a uthread?? */
			ASSERT(pas->pas_flags & PAS_LOCKPRDA);
			pas_flagclr(pas, PAS_LOCKPRDA);
		}
                /*
                 * Now we have to release the hold we have
                 * this pfdat, acquired when we locked the prda.
                 */
                pfdat_release(pfd);
		unlockpages(pas, ppas, prp->p_regva, prp->p_pglen, 0);
	}
	ASSERT(!(ppas->ppas_flags & PPAS_LOCKPRDA));
}

/*
 * Check that a process is not trying to expand
 * beyond the maximum allowed virtual address
 * space size.
 */
int
chkpgrowth(pas_t *pas, ppas_t *ppas, pgcnt_t size)
{
	/* Performing this check first eliminates problem with daddiu
	 * instruction on R4400 rev 4.0. (use of btoc on value within
	 * last virtual page will result in daddiu problem).
	 * This check also fixes problems with btoc64(last page) will of
	 * course wrap...
	 */
	if (btoct64(pas->pas_vmemmax) != btoct64(RLIM_INFINITY))
		if (size + getpsize(pas, ppas) > btoc64(pas->pas_vmemmax))
			return(-1);
	return(0);
}

#if defined(CKPT) & defined(_MIPS3_ADDRSPACE)
/*
 * called when the process to be restored is a different ABI than
 * the cpr restore process.
 * In this case we need to replace the pmap and alter a few other
 * fields in the AS. Note that since we assume that callers are 64 bit
 * the only change occurs when we need to switch to 32 bit programs
 */
/* ARGSUSED */
int
replace_pmap(pas_t *pas, ppas_t *ppas, uchar_t abi)
{
	pmap_t	*opmap;
	pmap_t 	*npmap;
	preg_t	*prp;
	pde_t	*old_pt;
	pde_t	*new_pt;
	char	*vaddr;
	pgno_t	npgs, cnt, cpy_cnt;
	int doingprivate = 0;

	mrupdate(&pas->pas_aspacelock);

	/* lots of ckpt code assumes that caller is 64 bit */
	ASSERT(!ABI_IS_IRIX5_64(abi));
	/*
	 * first check that all addresses are within range 
	 */
	doingprivate = 0;
	prp = PREG_FIRST(pas->pas_pregions);
addrchk:
	for ( ; prp ; prp = PREG_NEXT(prp)) {
		vaddr = prp->p_regva;
		if ((vaddr + ctob(prp->p_pglen)) > (uvaddr_t)HIUSRATTACH_32) {
			mrunlock(&pas->pas_aspacelock);
			return EINVAL;
		}
	}

	/* deal with private pregions */
	if (!doingprivate) {
		doingprivate = 1;
		prp = PREG_FIRST(ppas->ppas_pregions);
		goto addrchk;
	}
	/*
	 * Shared address space procs can get here with shared part
	 * of the address space already converted
	 */
	if (pas->pas_flags & PAS_64) {

		pas_flagclr(pas, PAS_64);
		pas->pas_nextaalloc = LOWDEFATTACH;
		pas->pas_hiusrattach = (uvaddr_t)HIUSRATTACH_32;
		pas->pas_lastspbase = pas->pas_hiusrattach;

		prp = PREG_FIRST(pas->pas_pregions);

		opmap = pas->pas_pmap;
		npmap = pas->pas_pmap = pmap_create(pas->pas_flags & PAS_64,
								PMAP_SEGMENT);
		ASSERT(opmap->pmap_type != npmap->pmap_type);

		doingprivate = 0;

		prp = PREG_FIRST(pas->pas_pregions);
	} else {
		ASSERT(pas->pas_flags & PAS_SHARED);

		prp = PREG_FIRST(ppas->ppas_pregions);

		opmap = ppas->ppas_pmap;
		npmap = ppas->ppas_pmap = pmap_create(
				pas->pas_flags & PAS_64, PMAP_SEGMENT);
		ASSERT(opmap->pmap_type != npmap->pmap_type);

		doingprivate = 1;
	}
	newpmap(pas, ppas);
priv:
	for ( ; prp ; prp = PREG_NEXT(prp)) {
		/*
		 * transfer pmap reservation
		 */
		pmap_unreserve(prp->p_pmap, pas, ppas, prp->p_regva, prp->p_pglen);
		if (pmap_reserve(npmap, pas, ppas, prp->p_regva, prp->p_pglen))
			/*
			 * we're hosed
			 */
			return (EAGAIN);

		prp->p_pmap = npmap;

		vaddr = prp->p_regva;
		npgs = prp->p_pglen;
		ASSERT(npgs);
		reglock(prp->p_reg);

		if ((pas->pas_flags & PAS_SHARED) && doingprivate)
			pmap_lclsetup(pas, ppas->ppas_utas, prp->p_pmap, vaddr, npgs);

		while (npgs) {
			old_pt = pmap_probe(opmap, &vaddr, &npgs, &cnt);
			if (npgs == 0)
				break;

			ASSERT(old_pt);
			ASSERT(cnt);

			npgs -= cnt;

			while (cnt) {
				cpy_cnt = cnt;
				new_pt = pmap_ptes(pas, npmap, vaddr, &cpy_cnt, 0);
				ASSERT(cpy_cnt);

				vaddr += ctob(cpy_cnt);
				cnt -= cpy_cnt;
				while (cpy_cnt) {
					pg_pfnacquire(old_pt);
					pg_pfnacquire(new_pt);
					pg_setpgi(new_pt, pg_getpgi(old_pt));
	
					if (pg_getpfn(new_pt)) {
						/* only if pfn is valid */
						if (!(prp->p_reg->r_flags & RG_PHYS))
							rmap_swapmap(pdetopfdat(old_pt),
								old_pt, new_pt);
					}
					pg_clrpgi(old_pt);
					pg_pfnrelease(new_pt);
					pg_pfnrelease(old_pt);

					new_pt++;
					old_pt++;
					cpy_cnt--;
				}
			}
		}
		ASSERT(count_valid(prp, prp->p_regva, prp->p_pglen) == prp->p_nvalid);
		regrele(prp->p_reg);
	}

	/* deal with private pregions */
	if (!doingprivate) {
		doingprivate = 1;
		prp = PREG_FIRST(ppas->ppas_pregions);
		/*
		 * if we're not part of a share group then use same pmap
		 * else use private pmap
		 */
		if (pas->pas_flags & PAS_SHARED) {
			pmap_destroy(opmap);
			opmap = ppas->ppas_pmap;
			npmap = ppas->ppas_pmap = pmap_create(
					pas->pas_flags & PAS_64, PMAP_SEGMENT);
		}
		goto priv;
	}
	pmap_destroy(opmap);
	mrunlock(&pas->pas_aspacelock);
	return 0;
}
#endif /* CKPT */

/*
 * This routine returns a set of vaddr/len pairs that the core dumping
 * routine uses to decide which parts of its AS it should dump into the
 * core file. The routine makes sure not to try to dump any PROT_NONE
 * pages. It holds aspacelock to prevent region growth/shrink, and
 * holds the region locks while scanning the attribute chains. The
 * core dumping process is racy, since a share group member can always
 * change the attributes in the middle of core dumping.
 */
int
pas_getkvmap(pas_t *pas, ppas_t *ppas, as_getattr_t *as)
{
	preg_t 		*prp;
	int 		npsegs = 0, shared = 0, type;
	kvmap_t 	*kvmap, *kvp;
	attr_t		*attr, *lattr;
	size_t		dumplen, maxlen, runlen, incr, t1, t2;


	/* count segments */
	prp = PREG_FIRST(ppas->ppas_pregions);
again0:
	for (; prp; prp = PREG_NEXT(prp)) {
		maxlen = ctob(getpregionsize(prp));
		dumplen = 0;
		lattr = NULL;
		reglock(prp->p_reg);
		for (attr = &prp->p_attrs; (attr && (dumplen != maxlen)); 
					attr = attr->attr_next) {
			t1 = attr->attr_end - attr->attr_start;
			t2 = maxlen - dumplen;
			incr = MIN(t1, t2);
			dumplen += incr;
			/* Find the first dumpable attr in a run */
			if ((attr->attr_prot != PROT_NONE) && (lattr == NULL))
				lattr = attr;
			/* Is this the end of the run? */
			else if ((lattr!=NULL)&&(attr->attr_prot==PROT_NONE)) {
				npsegs++;
				lattr = NULL;
			}
		}
		if (lattr) npsegs++;
		regrele(prp->p_reg);
	}
	if (!shared) {
		shared = 1;
		prp = PREG_FIRST(pas->pas_pregions);
		goto again0;
	}
	
	/* return non-zero value if there is nothing to dump */
	if (npsegs == 0)
		return ENOENT;

	shared = 0;

	/* now fill in kvmaps */
	kvp = kvmap = kmem_zalloc(npsegs * sizeof(kvmap_t), KM_SLEEP);

	prp = PREG_FIRST(ppas->ppas_pregions);
again:
	for (; prp; prp = PREG_NEXT(prp)) {
		if (prp->p_type == PT_GR)
			type = VGRAPHICS;
		else if (prp->p_reg->r_flags & RG_PHYS)
			type = VPHYS;
		else if (prp->p_reg->r_flags & RG_TEXT)
			type = VTEXT;
		else if (prp->p_type == PT_STACK)
			type = VSTACK;
		else if (prp->p_type == PT_SHMEM)
			type = VSHMEM;
		else
			type = VMAPFILE;
		maxlen = ctob(getpregionsize(prp));
		dumplen = 0;
		lattr = NULL;
		reglock(prp->p_reg);
		for (attr = &prp->p_attrs; (attr && (dumplen != maxlen)); 
				attr = attr->attr_next) {
			t1 = attr->attr_end - attr->attr_start;
			t2 = maxlen - dumplen;
			incr = MIN(t1, t2);
			dumplen += incr;
			/* Find the first dumpable attr in a run */
			if ((attr->attr_prot != PROT_NONE) && (lattr == NULL)){
				lattr = attr;
				runlen = incr;
			}
			/* If this is part of the dumpable run */
			else if ((lattr!=NULL)&&(attr->attr_prot!=PROT_NONE)) {
				runlen += incr;
			}
			/* Is this the end of the run? */
			else if ((lattr!=NULL)&&(attr->attr_prot==PROT_NONE)) {
				kvp->v_vaddr = lattr->attr_start;
				kvp->v_len = runlen;
				kvp->v_type = type;
				kvp++;
				lattr = NULL;
			}
		}
		regrele(prp->p_reg);
		if (lattr) {
			kvp->v_vaddr = lattr->attr_start;
			kvp->v_len = runlen;
			kvp->v_type = type;
			kvp++;
		}
	}
	if (!shared) {
		shared = 1;
		prp = PREG_FIRST(pas->pas_pregions);
		goto again;
	}

	as->as_kvmap.as_kvmap = kvmap;
	as->as_kvmap.as_nentries = npsegs;
	return 0;
}

int
pas_getnshm(pas_t *pas, ppas_t *ppas)
{
	int nshmem = 0;
	preg_t *prp;

	/* count shmem regions */
	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp)) {
		if (prp->p_type == PT_SHMEM)
			nshmem++;
	}
	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp)) {
		if (prp->p_type == PT_SHMEM)
			nshmem++;
	}
	return nshmem;
}

/*
 * translate virtual to physical.
 * Returns - 0 success (regardless of valid pages or not)
 * EINVAL - any of [vaddr, vaddr+nbytes] is bad
 * ENOENT - no pages requested
 * i.e. if returns 0 then the pfn array is all filled in.
 */

#ifdef MH_R10000_SPECULATION_WAR
extern int syssegsz;
extern int is_in_pfdat(pgno_t);
#endif /* MH_R10000_SPECULATION_WAR */

int
pas_vtopv(
	pas_t *pas,
	ppas_t *ppas,
	as_getrangeattrop_t op,
	uvaddr_t vaddr,
	size_t nbytes,
	as_getrangeattrin_t *attrin,
	as_getrangeattr_t *attrout)
{
	preg_t *prp;
	pgcnt_t i, nmapped = 0;
	pde_t *pt;
	pgcnt_t npgs;
	auto pgno_t sz;
	pfn_t *ppfn;
	pde_t *ppde;
	alenlist_t palen;
	int step, shift, flags;
	pgi_t bits;
	int poffset;
        size_t *ppgsz;
        
	ASSERT((__psunsigned_t)vaddr < (__psunsigned_t)K0SEG);

	npgs = numpages(vaddr, nbytes);
	if (npgs == 0)
		return ENOENT;

	switch (op) {
	case AS_GET_VTOP:
		ppfn = attrin->as_vtop.as_ppfn;
                ppgsz = attrin->as_vtop.as_ppgsz;   
		step = attrin->as_vtop.as_step;
		shift = attrin->as_vtop.as_shift;
		bits = attrin->as_vtop.as_bits;
		break;
	case AS_GET_PDE:
	case AS_GET_KPTE:
		ppde = attrin->as_pde.as_ppde;
		bits = attrin->as_pde.as_bits;
		break;
	case AS_GET_ALEN:
		palen = attrin->as_alen.as_alenlist;
		flags = attrin->as_alen.as_flags;
		poffset = poff(vaddr);
		break;
	default:
		panic("as_vtopv: bad op code");
		/* NOTREACHED */
	}
	mraccess(&pas->pas_aspacelock);

	do {
		prp = findpreg(pas, ppas, vaddr);
		if (prp == NULL) {
			mrunlock(&pas->pas_aspacelock);
			return EINVAL;
		}

		sz = npgs;
		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);

                /*
                 * No pfn field locking.
                 * These pages must have been locked.
                 */
		i = min(npgs, sz);
		vaddr += ctob(i);
		npgs -= i;

		/*
		 * we switch here so avoid all the if's per pte
		 *
		 * If these valid pages are not locked, then
		 * we may have a problem with migration
		 * For now I'll just assume callers
		 * are doing the right thing.
		 */
		switch (op) {
		case AS_GET_VTOP:
			ASSERT(ppfn);
			/* fill in list of pfn's */
			while (--i >= 0) {
				if (pg_isvalid(pt)) {
					*ppfn = (pg_getpfn(pt) << shift) | bits;
                                        if (ppgsz) {
                                                *ppgsz = PAGE_MASK_INDEX_TO_SIZE(
                                                        pg_get_page_mask_index(pt));
                                        }
#ifdef MH_R10000_SPECULATION_WAR
					if (iskvirpte(((pde_t *) ppfn)) &&
					    pg_isvalid(((pde_t *) ppfn)) &&
					    pte_iscached(*ppfn) &&
					    is_in_pfdat(pdetopfn(((pde_t *) ppfn))))
						krpf_add_reference(pdetopfdat(((pde_t *) ppfn)),
						                   kvirptetovfn(((pde_t *) ppfn)));
#endif /* MH_R10000_SPECULATION_WAR */
					nmapped++;
				} else {
					*ppfn = bits;
                                        if (ppgsz) {
                                                *ppgsz = 0;
                                        }
				}
				ppfn = (pfn_t *)((char *)ppfn + step);
                                if (ppgsz) {
                                        ppgsz++;
                                }
				pt++;
			}
			break;
		case AS_GET_PDE:
			ASSERT(ppde);
			/* fill in list of pdes's */
			while (--i >= 0) {
				if (pg_isvalid(pt)) {
					*ppde = *pt;
#ifdef MH_R10000_SPECULATION_WAR
					if (iskvirpte(ppde) &&
					    pg_isvalid(ppde) &&
					    pte_iscached(*((__uint32_t *)ppde)) &&
					    is_in_pfdat(pdetopfn(ppde)))
						krpf_add_reference(pdetopfdat(ppde),
						                   kvirptetovfn(ppde));
#endif /* MH_R10000_SPECULATION_WAR */
					nmapped++;
				} else {
					pg_setpgi(ppde, mkpde(bits, 0));
				}
				ppde++;
				pt++;
			}
			break;
		case AS_GET_KPTE:
			ASSERT(ppde);
			/* fill in list of pdes's */
			while (--i >= 0) {
				if (pg_isvalid(pt)) {
					pg_setpgi(ppde,
						  mkpde(bits, pg_getpfn(pt)));
					pg_copy_uncachattr(ppde, pt);
#ifdef MH_R10000_SPECULATION_WAR
					if (iskvirpte(ppde) &&
					    pg_isvalid(ppde) &&
					    pte_iscached(*((__uint32_t *)ppde)) &&
					    is_in_pfdat(pdetopfn(ppde)))
						krpf_add_reference(pdetopfdat(ppde),
						                   kvirptetovfn(ppde));
#endif /* MH_R10000_SPECULATION_WAR */
					nmapped++;
				} else {
					pg_setpgi(ppde, mkpde(bits, 0));
				}
				ppde++;
				pt++;
			}
			break;
		case AS_GET_ALEN:
			ASSERT(palen);
			/* fill in alenlist */
			while (--i >= 0) {
				int nbytes_in_entry;

				/* number of bytes is the minimum of 
				 * the number of bytes till the end of this page
				 * the number of bytes remaining
				 */
				nbytes_in_entry = MIN(NBPP-poffset, nbytes);

				ASSERT(pg_isvalid(pt));
				if (alenlist_append(palen,
					pdetophys(pt)+poffset, 
					nbytes_in_entry,
					flags) == ALENLIST_FAILURE) {
						mrunlock(&pas->pas_aspacelock);
						return EINVAL;
				}
				pt++;
				nbytes -= nbytes_in_entry;
				poffset = 0;
			}
			break;
		}
	} while (npgs);

	mrunlock(&pas->pas_aspacelock);
	if (attrout)
		attrout->as_vtop_nmapped = nmapped;
	return(0);
}
