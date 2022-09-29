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
#ident  "$Revision: 1.16 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "ksys/as.h"
#include "sys/atomic_ops.h"
#include "sys/anon.h"
#include "as_private.h"
#include <sys/ckpt.h>
#include <sys/ckpt_procfs.h>
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/kabi.h"
#include "sys/kmem.h"
#include "sys/numa.h"
#include "pmap.h"
#include "sys/procfs.h"
#include "../../fs/procfs/procfs_n32.h"
#include "sys/resource.h"
#include "ksys/rmap.h"
#include "sys/sysmacros.h"
#include "sys/vnode.h"
#include "sys/cred.h"

/*
 * routines to support /proc interfaces
 */

/*
 * vfault/pfault logging routine
 * We keep a minimal amount per address space
 */
#define FAULTLOG_NELEM	64
#define FAULTLOG_SHFT	23
#define PHI     2654435769      /* (golden mean) * (2 to the 32) */
#define FLHASH(prp, vaddr) \
	(((PHI * ((long)(prp) ^ (pnum(vaddr)))) >> FAULTLOG_SHFT) & \
	(FAULTLOG_NELEM - 1))
struct faultlog {
	ppas_t *ppas;
	preg_t *prp;
	uvaddr_t vaddr;
	int exists;		/* 0 = nothing, 1 = vfault, 2 = pfault */
};

/*
 * update fault log
 */
void
prfaultlog(pas_t *pas, ppas_t *ppas, preg_t *prp, uvaddr_t vaddr, int flag)
{
	struct faultlog *entry;

	/* Logging enabled? */
	if (!pas->pas_faultlog)
		return;

	/* Don't log kernel addresses */
	if (!IS_KUSEG(vaddr))
		return;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	/* Log */
	entry = &pas->pas_faultlog[FLHASH(prp, vaddr)];

	if (flag < entry->exists)
		return;

	mutex_lock(&pas->pas_brklock, 0);
	if (flag < entry->exists)
		goto out;
	entry->ppas = ppas;
	entry->prp = prp;
	entry->vaddr = vaddr;
	entry->exists = flag ? flag : 1;
out:
	mutex_unlock(&pas->pas_brklock);

	return;
}

static int
prfaultlookup(pas_t *pas, ppas_t *ppas, preg_t *prp, caddr_t vaddr, int *offset)
{
	struct faultlog *entry;
	int found = 0;

	if (pas->pas_faultlog == NULL)
		return 0;

	/* Check for a fault for proc in region with vaddr given */
	entry = &pas->pas_faultlog[FLHASH(prp, vaddr)];
	mutex_lock(&pas->pas_brklock, 0);
	if ((entry->ppas == ppas) && (entry->prp == prp) &&
	    (pnum(entry->vaddr) == pnum(vaddr)) && entry->exists) {
		*offset = poff(entry->vaddr);
		found = entry->exists;
		entry->exists = 0;
	}
	mutex_unlock(&pas->pas_brklock);

	return found;
}

static void
prfaultsetup(pas_t *pas)
{
	struct faultlog *f;

	f = (struct faultlog *)kmem_zalloc(sizeof (struct faultlog) *
				FAULTLOG_NELEM, KM_SLEEP);
	
	mutex_lock(&pas->pas_brklock, 0);
	if (pas->pas_faultlog)
		kmem_free(f, sizeof (struct faultlog) * FAULTLOG_NELEM);
	else
		pas->pas_faultlog = f;
	mutex_unlock(&pas->pas_brklock);
}

#ifdef CKPT
static void
prgetanoninfo(preg_t *prp, uvaddr_t vaddr, size_t len, pgd_t *data)
{
	sm_swaphandle_t dummy;
	void *hand;

	if (prp->p_reg->r_anon == NULL)
		return;

	while (len > 0) {
		hand = anon_lookup(prp->p_reg->r_anon, vtoapn(prp, vaddr), &dummy);

		if (hand != (void *)0L)
			data->pr_flags |= PGF_ANONBACK;
		data++;
		len -= NBPP;
		vaddr += NBPP;
	}
}
#endif

/*
 * Get page data in machine independent format for region virtual range
 */
static int
getpginfo(
	pas_t *pas,
	ppas_t *ppas,
	preg_t *prp,
	uvaddr_t saddr,
	size_t length,
	pgd_t *bdata,
	pgcnt_t *sizes,
	as_getrangeattr_t *asres)
{
	register pde_t	*pd;
	register pfd_t	*pfd;
	register pgd_t	*data;
	pgcnt_t	lvsize, lpsize, lwsize, lrsize, lmsize;
	pgno_t		sz = 0, len = btoc(length);
	uvaddr_t	vaddr = saddr;
	int		phys, flags, flush = 0, value;

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	phys = prp->p_reg->r_flags & RG_PHYS;
	lvsize = lpsize = lwsize = lrsize = lmsize = 0;

	while (len > 0) {
		/* Find next PTE map */
		pd = pmap_probe(prp->p_pmap, &vaddr, &len, &sz);
		if (pd == NULL)
			break;

		/* Generate PGD offset (note: we know vaddr > saddr) */
		data = &bdata[((ulong_t)vaddr - (ulong_t)saddr) / NBPP];

		/* Walk each entry in the map */
		len -= sz;

		for ( ; sz > 0; pd++, vaddr += NBPP, sz--) {
			/* Always clear non-history PGD flags */
			if (bdata) {
				data->pr_flags &= PGF_NONHIST;
				if (data->pr_flags & PGF_CLEAR) {
					if (!pas->pas_faultlog) {
						prfaultsetup(pas);
					}
					prp->p_flags |= PF_FAULT;
				}
			}
			/* Don't check software invalid pages */
			if (!pg_isvalid(pd)) {
				/* For large pages skip multiple ptes */
				data++;
				continue;
			}

			/* Check basic software flags & page use */
			lvsize++;
			flags = PGF_ISVALID|PGF_VALHISTORY;
                        
			if (!phys) {
                                pg_pfnacquire(pd);
				pfd = pdetopfdat(pd);
				if (pfd->pf_flags & P_SWAP)
					flags |= PGF_SWAPPED;

				if ((value = pfd->pf_use) <= 1) {
                                        pg_pfnrelease(pd);
					goto l_private;
                                }

				/*
				 * Global pages can be shared with those
				 * in the buffer cache.  Need to subtract
				 * out the extra shared count...we don't
				 * want to count the buffer cache here.
				 */
				if (pfd->pf_next || pfd->pf_prev)
					value -= 1;

				if (value <= 1) {
                                        pg_pfnrelease(pd);
					goto l_private;
                                }
                                pg_pfnrelease(pd);

				lwsize += MA_WSIZE_FRAC / value;
			} else {
l_private:
				value = 1;
				flags |= PGF_PRIVATE;
				lwsize += MA_WSIZE_FRAC;
				lpsize++;
			}
                        
			if (pg_isdirty(pd) || (flags & PGF_SWAPPED)) {
				lmsize++;
				flags |= PGF_ISDIRTY|PGF_WRTHISTORY;
			}

			/* Hardware PTE bits */
			if (pg_ishrdvalid(pd)) {
				lrsize++;
				flags |= PGF_REFERENCED|PGF_REFHISTORY;
			}
			if (pg_isglob(pd))
				flags |= PGF_GLOBAL;
			if (pg_ismod(pd))
				flags |= PGF_WRITEABLE|PGF_WRTHISTORY;
			if (pg_isnoncache(pd))
				flags |= PGF_NOTCACHED;

			/* Update PGD data */
			if (bdata) {
				caddr_t va = vaddr;
				/* Update flags */
				data->pr_flags |= flags;
				/* Look for fault info and clear PTE bits */
				if (!phys &&
				    (data->pr_flags & PGF_CLEAR)) {
					flags = prfaultlookup(pas, ppas, prp,
							va, &value);
					if (flags) {
						data->pr_flags |= PGF_FAULT;
					}
					pg_clrhrdvalid(pd);
					pg_clrmod(pd);
					flush = 1;
				}
				data->pr_value = value;
				data++;
			}
		}
	}
#ifdef CKPT
	if (bdata)
		prgetanoninfo(prp, saddr, length, bdata);
#endif
	/* Return size info only if requested */
	if (sizes) {
		sizes[0] = lvsize;
		sizes[1] = lpsize;
		sizes[2] = lwsize;
		sizes[3] = lrsize;
		sizes[4] = lmsize;
	}
	if (asres)
		asres->as_pgd_flush = flush;
	return 0;
}

int
pas_getpginfo(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t saddr,
	size_t length,
	as_getrangeattrin_t *as,
	as_getrangeattr_t *asres)
{
	preg_t *prp;
	int ret;

	if ((prp = findpreg(pas, ppas, saddr)) == NULL)
		return EINVAL;

	/* Must have region locked before entering getpginfo() - PV 774040 */
	reglock(prp->p_reg);
	ret = getpginfo(pas, ppas, prp, saddr, length,
			as->as_pgd_data, NULL, asres);
	regrele(prp->p_reg);
	return ret;
}


/*
 * return attribute info.
 * There is some complexity here since the data structure is
 * ABI dependent. We fill in the 'largest' version and let the caller
 * translate.
 */
#if (_MIPS_SIM == _ABI64)
#define ASMAP	prmap_sgi_t
#define ASMAPVADDRT	caddr_t
#elif (_MIPS_SIM == _ABIO32 || _MIPS_SIM == _ABIN32)
#define ASMAP	irix5_n32_prmap_sgi_t
#define ASMAPVADDRT	app32_ptr_t
#else
		<<<BOMB>>>
#endif

static int
prgetmap_hdr_flags(pas_t *pas, ppas_t *ppas, preg_t *prp, attr_t *attr)
{
	int mflags = 0;
	uchar_t prot;
	reg_t *rp = prp->p_reg;
	int refcnt;

	prot = attr->attr_prot;
	if (prot & PROT_READ)
		mflags |= MA_READ;
	if (prot & PROT_WRITE)
		mflags |= MA_WRITE;
	if (prot & PROT_EXEC)
		mflags |= MA_EXEC;

	if ((pas->pas_brkbase >= attr->attr_start) &&
	    (pas->pas_brkbase < attr->attr_end))
		mflags |= MA_BREAK;
	if (ppas->ppas_flags & PPAS_STACK) {
		if ((ppas->ppas_stkbase >= attr->attr_start) &&
	    		(ppas->ppas_stkbase <= attr->attr_end))
				mflags |= MA_STACK;
	} else {
		if ((pas->pas_stkbase >= attr->attr_start) &&
	    		(pas->pas_stkbase <= attr->attr_end))
				mflags |= MA_STACK;
	}

	if ((prp->p_type == PT_SHMEM) ||
	    (prp->p_reg->r_flags & RG_TEXT) || ((prp->p_reg->r_flags & RG_ANON) == 0))
		mflags |= MA_SHARED;

	if (prp->p_reg->r_flags & RG_PHYS)
		mflags |= MA_PHYS;

#if !(R4000 || TFP || R10000 || BEAST)
	<<< BOMB - attr_cc is machine dependent - must define new cpu type >>>
#endif

	if (attr->attr_cc == 2)
		mflags |= MA_NOTCACHED;
	if (attr_is_fetchop(attr))
		mflags |= MA_FETCHOP;
	if (rp->r_flags & RG_CW)
		mflags |= MA_COW;
	if (prp->p_type == PT_SHMEM)
		mflags |= MA_SHMEM;
	refcnt = rp->r_refcnt;
	if (prp->p_flags & PF_SHARED) {
		mflags |= MA_SREGION;
		refcnt += pas->pas_refcnt;
	}
	mflags |= refcnt << MA_REFCNT_SHIFT;
	if (prp->p_flags & PF_PRIMARY)
		mflags |= MA_PRIMARY;
	if (rp->r_flags & RG_MAPZERO)
		mflags |= MA_MAPZERO;

	return mflags;
}

static int
prgetmap_hdr(
	pas_t *pas,
	ppas_t *ppas,
	preg_t *prp,
	attr_t *attr,
	ASMAP *pmp,
	uvaddr_t saddr,
	uvaddr_t *eaddrp)
{
	uvaddr_t eaddr = attr->attr_end;
	pgcnt_t sizes[5];
	int error;

	pmp->pr_off = prp->p_reg->r_fileoff + ctob(prp->p_offset) +
			(attr->attr_start - prp->p_regva);
	pmp->pr_mflags = prgetmap_hdr_flags(pas, ppas, prp, attr);

	/* If break starts in the middle, split up this segment */
	if (pmp->pr_mflags & MA_BREAK) {
		if (saddr == pas->pas_brkbase) {
			pmp->pr_mflags &= ~MA_SHARED;
			pmp->pr_off = 0;
		} else if(saddr < pas->pas_brkbase) {
			pmp->pr_mflags &= ~MA_BREAK;
			eaddr = pas->pas_brkbase;
		} else {  /* just in case */
			pmp->pr_mflags &= ~MA_BREAK;
		}
	}
	ASSERT(eaddr != saddr);
	pmp->pr_vaddr = (ASMAPVADDRT) saddr;
	pmp->pr_size = eaddr - saddr;
	pmp->pr_dev = pmp->pr_ino = 0;

	pmp->pr_lockcnt = attr->attr_lockcnt;

	if (error = getpginfo(pas, ppas, prp, saddr, eaddr - saddr,
						NULL, sizes, NULL)) {
		return error;
	}
	pmp->pr_vsize = sizes[0];
	pmp->pr_psize = sizes[1];
	pmp->pr_wsize = sizes[2];
	pmp->pr_rsize = sizes[3];
	pmp->pr_msize = sizes[4];

	*eaddrp = eaddr;
	return 0;
}

static void
prgetmap_exthdr_vattr(vnode_t *vp, ASMAP *pmp)		      
{
	struct vattr	vattr;
	int		error;

	ASSERT(vp != NULL);

	vattr.va_mask = AT_FSID|AT_NODEID;
	VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
	if (!error) {
		pmp->pr_dev = vattr.va_fsid;
		pmp->pr_ino = vattr.va_nodeid;
	}
}

/*
 * Fill an array of structures with memory map information.
 * Return the # of structures filled in to the caller or an error.
 * NB: caller must acquire and release aspacelock()
 */
int
pas_getprattr(
	pas_t *pas,
	ppas_t *ppas,
	as_getattrin_t *attrin,
	as_getattr_t *attrres)
{
	struct pregion *prp;
	int cnt = 0, shared = 0;
	int error, maxentries;
	ASMAP *pmp, tpmp;
	uvaddr_t vaddr, vend;
	attr_t *attr;

	maxentries = attrin->as_prattr.as_maxentries;
	pmp = attrin->as_prattr.as_data;

	mraccess(&pas->pas_aspacelock);

	prp = PREG_FIRST(ppas->ppas_pregions);
again:
	while (prp) {
		/*
		 * If the underlying region is a mapped file,
		 * then we want to get some of the attributes
		 * of the vnode as well.  However, we had to
		 * do this before the region lock.  
		 * This is because internal
		 * file system locks must always be acquired
		 * before the region lock in order to prevent
		 * deadlock.  It is OK to look at the region
		 * and its vnode pointer after dropping the
		 * region lock, though, because we hold the
		 * address space lock which keeps the structure
		 * of this stuff from changing out from under
		 * us.
		 */
		if (prp->p_reg->r_vnode != NULL) {
			prgetmap_exthdr_vattr(prp->p_reg->r_vnode, &tpmp);
		}
                reglock(prp->p_reg);
		vaddr = prp->p_regva;
		vend = prp->p_regva + ctob(prp->p_pglen);
		(void) findattr_range(prp, vaddr, vend);	/* coalesce */
		while (vaddr < vend) {
			/* Find attribute for this virtual address */
			if ((attr = findattr(prp, vaddr)) == NULL)
				break;

			/* fill in primary header info */
			error = prgetmap_hdr(pas, ppas, prp, attr,
						   pmp, vaddr, &vaddr);
				
			if (error) {
				regrele(prp->p_reg);
				mrunlock(&pas->pas_aspacelock);
				return error;
			}
			if (prp->p_reg->r_vnode != NULL) {
				pmp->pr_dev = tpmp.pr_dev;
				pmp->pr_ino = tpmp.pr_ino;
			}
			pmp++;
			cnt++;
			if (--maxentries <= 0) {
				regrele(prp->p_reg);
				goto outearly;
			}
		}
		regrele(prp->p_reg);
		prp = PREG_NEXT(prp);
	}

	if (!shared) {
		shared = 1;
		prp = PREG_FIRST(pas->pas_pregions);
		goto again;
	}

outearly:
	attrres->as_nentries = cnt;
	mrunlock(&pas->pas_aspacelock);

	return 0;
}
