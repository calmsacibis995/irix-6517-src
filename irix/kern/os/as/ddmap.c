/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident  "$Revision: 1.61 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <ksys/as.h>
#include "as_private.h"
#include <ksys/ddmap.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/numa.h>
#include "pmap.h"
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uthread.h>
#include <sys/vmereg.h>
#include <ksys/vm_pool.h>
#include <sys/nodepda.h>

/* includes we shouldn't need .. */
#include "region.h"
extern int syssegsz;

/*
 * routines for drivers to map I/O space and/or kernel memory into
 * a processes space.
 * Most all of these routines work on the 'current' process.
 */
static int v_pg(pas_t *, preg_t *, uvaddr_t, caddr_t, pgno_t);
static int ddmap_genphysfault(vhandl_t *vt, void *s, uvaddr_t vaddr, int rw);

#define v_getpreg(VT)	(VT)->v_preg

/*
 * v_getaddr - return virtual address where device mapped
 */
uvaddr_t
v_getaddr(vhandl_t *vt)
{
	return vt->v_addr;
}

/*
 * v_getlen - return length in bytes
 */
size_t
v_getlen(vhandl_t *vt)
{
	return ctob(vt->v_preg->p_pglen);
}

/*
 * v_gethandle - return a handle that uniquely identifies this mapping
 * Note that this must be address space independent since on fork
 * a memory object may get duplicated and the handle must be the same
 */
__psunsigned_t
v_gethandle(vhandl_t *vt)
{
	return (__psunsigned_t)vt->v_preg->p_reg;
}

static int
getprot(vasid_t vasid, uvaddr_t vaddr, int *max, int *prot)
{
	preg_t *prp;
	pas_t *pas;
	ppas_t *ppas;

	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

	if ((prp = findanypreg(pas, ppas, vaddr)) == NULL)
		return EINVAL;

	if (max)
		*max = prp->p_maxprots;
	if (prot)
		*prot = findattr_range_noclip(prp, vaddr, vaddr)->attr_prot;
	return 0;
}

/*
 * v_getprot - return protections
 */
/* ARGSUSED */
int
v_getprot(vhandl_t *vt, uvaddr_t vaddr, int *max, int *prot)
{
	vasid_t vasid;

	as_lookup_current(&vasid);
	return getprot(vasid, vaddr, max, prot);
}

/*
 * v_enterpage - enter page(s) into a mapped devices page table
 * Returns 0 if page(s) entered correctly, errno otherwise
 * Does all locking so can be called from fault-handlers.
 */
int
v_enterpage(vhandl_t *vt, uvaddr_t vaddr, void *addr, pgcnt_t len)
{
	preg_t		*prp;
	reg_t		*rp;
	int		error;
	vasid_t vasid;
	pas_t *pas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);

	mraccess(&pas->pas_aspacelock);

	prp = v_getpreg(vt);
	ASSERT(prp);
	rp = prp->p_reg;
	reglock(rp);
	ASSERT(rp->r_flags & RG_PHYS);
	error = v_pg(pas, prp, vaddr, addr, len);
	/* SPT: if pmap is using Shared Page Tables, p_nvalid is not accurate */
	ASSERT(error || pmap_spt(prp->p_pmap) || 
	       count_valid(prp, prp->p_regva, prp->p_pglen) == prp->p_nvalid);
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	return(error);
}

/*
 * Initialize a PHYS region
 * Returns: 0 if success, errno otherwise
 */
int
v_initphys(vhandl_t *vt,
	int (*faulthandler)(vhandl_t *, void *, uvaddr_t, int),
	size_t len,
	void *special)
{
	return v_setupphys(vt, faulthandler, len, special, NULL, 0);
}

/*
 * Initialize a PHYS region, This version permits specification
 * of cache attributes on a per page basis.
 */
int
v_setupphys(vhandl_t *vt,
	int (*faulthandler)(vhandl_t *, void *, uvaddr_t, int),
	size_t len,
	void *special,
	v_mapattr_t *vmap,
	int vmap_size)
{
	preg_t		*prp;
	reg_t		*rp;
#if IP30
	vasid_t vasid;
	pas_t *pas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
#endif

	prp = v_getpreg(vt);
	ASSERT(prp);
	rp = prp->p_reg;
	ASSERT(mrislocked_update(&rp->r_lock));
	if ((rp->r_flags & RG_PHYS) == 0)
		return EINVAL;
	if (len > ctob(prp->p_pglen)) {
		return EINVAL;
	}

	rp->r_fault = faulthandler;
	rp->r_fltarg = special;

	/*
	 * v_getaddr(vt) contains virtual address
	 * which will be returned to user, which
	 * is not necessarily segment aligned.
	 */
	ASSERT(v_getaddr(vt) == prp->p_regva);

	/*
	 * if given attributes - set them now
	 */
	if (vmap) {
		int i;
		for (i = 0; i < vmap_size; i++) {
			pde_t pde;
			uvaddr_t start = vmap[i].v_start;
			uvaddr_t end = vmap[i].v_end;
			pgi_t bits;
			attr_t *attr;

#if IP30
			/* get protections from table -- tested only on IP30 */
			chgprot(pas, prp, start, numpages(start, end - start), vmap[i].v_prot);
#endif
			bits = 0;
			if (vmap[i].v_prot & PROT_WRITE)
				bits |= PG_M;
			switch (vmap[i].v_cc) {
			case VDD_DEFAULT:
				/*
				 * DEFAULT gives you the best way to access
				 * I/O memory
				 */
#if IP21 || IP26
				bits |= PG_UNC_PROCORD;
#elif IP25 || IP27 || IP30
				bits |= PG_UNCACHED_ACC;
#else
				bits |= PG_TRUEUNCACHED;
#endif
				break;
			case VDD_COHRNT_EXLWR:
				bits |= PG_COHRNT_EXLWR;
				break;
			case VDD_TRUEUNCACHED:
				bits |= PG_TRUEUNCACHED;
				break;
			default:
				return EINVAL;
			}
			switch (vmap[i].v_uc) {
			case VDD_UNCACHED_DEFAULT:
				break;
			case VDD_UNCACHED_IO:
#ifdef IP27
				bits |= PG_UC_IO;
#endif
				break;
			default:
				return EINVAL;
			}

			pde.pgi = mkpde(bits, 0);
			attr = findattr_range(prp, start, end);
			attr->attr_cc = pde.pte.pg_cc;
#ifdef IP27
			attr->attr_uc = pde.pte.pg_uc;
#endif
		}
	}
	return(0);
}

/*
 * Map kernel virtual space into user address space.
 * Addr can point into vme, k0, k1 or k2seg space.
 */
int
v_mapphys(vhandl_t *vt, void *addr, size_t len)
{
	register int error;
	vasid_t vasid;
	pas_t *pas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);

	if (error = v_setupphys(vt, ddmap_genphysfault, len, NULL, NULL, 0))
		return error;

	return(v_pg(pas, v_getpreg(vt), v_getaddr(vt), addr, btoc(len)));
}

/*
 *	workhorse for v_mapphys & v_enterpage
 *	Addr can point into vme, k0, k1 or k2seg space.
 * Returns: 0 if success, errno otherwise
 * We only bump nvalid if the pte wasn't already valid - this permits
 * easier locking for PHYS mapped devices since they don't
 * really have to woryy about dropping in the same page twice.
 */
static int
v_pg(pas_t *pas, preg_t *prp, uvaddr_t vaddr, caddr_t addr, pgno_t len)
{
	pgi_t bits;
	pde_t *pt;
	pde_t tpt;
	auto pgno_t sz;
#ifdef MH_R10000_SPECULATION_WAR
        int             is_uncached = 0;
#endif /* MH_R10000_SPECULATION_WAR */

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	sz = len;

	/* Set PG_M on writable regions to avoid pfault. */
	if (findattr(prp, vaddr)->attr_prot & PROT_W)
		bits = PG_VR | PG_SV | PG_M;
	else
		bits = PG_VR | PG_SV;
	
	/* Build block list pointing to map table.  */
	while (len) {
		sz = len;
		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
                /*
                 * pfn locking is not needed here because we're
                 * dealing with physical regions only, which map
                 * non-migratable memory only.
                 */
		ASSERT(pt && sz);
		len -= sz;
		vaddr = (uvaddr_t)((char *)vaddr + ctob(sz));

		for ( ; --sz >= 0; addr += NBPP, pt++) {

			if (IS_VME_A16S(addr)  ||
			    IS_VME_A16NP(addr) ||
			    IS_VME_A24S(addr)  ||
			    IS_VME_A24NP(addr) ||
			    IS_VME_A32S(addr)  ||
			    IS_VME_A32NP(addr))
			{
				/* set non-cached */
				if (!pg_isvalid(pt))
					prp->p_nvalid++;
				pg_setpgi(pt, mkpde(bits, vme_virtopfn(addr)));
				pg_setcache_phys(pt);
				continue;
			}

			if (IS_KSEG2(addr)) {
				register pde_t	*kpt;

				if ((__psunsigned_t)addr >=
						K2SEG + ctob(syssegsz))
					return ENXIO;

				kpt = kvtokptbl(addr);
				if (!pg_isvalid(pt))
					prp->p_nvalid++;
				pg_setpgi(pt, mkpde(bits, pdetopfn(kpt)));

				/*	If the kernel is accessing
				**	this page without caching,
				**	so should user.
				**	pg_copycachemode: target, source
				*/
				pg_copycachemode(pt, kpt);
				/*
				 * Copy any uncache attributes that could be
				  associated with kvaddr.
				 */
				pg_copy_uncachattr(pt, kpt);
#ifdef MH_R10000_SPECULATION_WAR
                                if (pg_isnoncache(kpt))
                                        is_uncached = 1;
#endif /* MH_R10000_SPECULATION_WAR */
				continue;
			}

			if (!IS_KSEGDM(addr))
				return ENXIO;

			if (!pg_isvalid(pt))
				prp->p_nvalid++;
			pg_setpgi(pt, mkpde(bits, btoct(svirtophys(addr))));

			/* KSEG1 addresses are not cached. */
			if (IS_KSEG1(addr)){
				pg_setcache_phys(pt);
				/* 
				 * transfer any uncached attributes on "addr"
				 * to PTE.
				 */
				pg_xfer_uncachattr(pt, addr);
			} else {
				pg_setcachable(pt);
			}
		}
	}
	/*
	 * Ensure that the pregions cc attributes are same as that in
	 * the pte's --- by default allocreg for prp would mark prp's cc
	 * attributes as un-cached. But depending on the allocated addr,
	 * the page might be accessed cached.
	 */ 
	tpt.pgi = 0;
	if (IS_KSEG1(addr)
#ifdef MH_R10000_SPECULATION_WAR
            ||
            is_uncached
#endif /* MH_R10000_SPECULATION_WAR */
	    ) {
		pg_setcache_phys(&tpt);
		pg_xfer_uncachattr(&tpt, addr);
	}
	else
		pg_setcachable(&tpt);
	prp->p_attrs.attr_cc = tpt.pte.pte_cc;
#ifdef IP27
	prp->p_attrs.attr_uc = tpt.pte.pte_uncattr;
#endif
	return 0;
}

/*
 *
 * Much like v_mapphys and v_enterpage, v_mappfns and v_enterpfns
 * allow drivers to map memory into user address space.  Instead of
 * taking a kernel virtual address, like the existing counterparts,
 * and deriving the physical pages to map from the address, these
 * routines take a list of physical pfns directly and map those pages.
 * This is useful for drivers that map "tiles" (see os/tile.c) into
 * user address space without ever mapping them into the kernel address
 * space.
 *
 * v_pfns - workhorse for v_mappfns & v_enterpfns.
 * Map the pages into the user address space.  If flags has VM_UNCACHED
 * the mapping is uncached, otherwise cached.
 * Returns 0 if success, or errno on failure.
 */
static int
v_pfns(
	pas_t *pas,
	preg_t *prp,
	uvaddr_t vaddr,
	pfn_t *pfns,
	pgcnt_t pfncnt,
	int flags,
	__uint64_t ptebits)	/* random hardware specific bits .. */
{
	pgi_t validbits;
	pde_t *pt, tpt;
	auto pgno_t sz;

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	/* Construct the proper cache bits */
	tpt.pgi = 0;
	if (flags & VM_UNCACHED_PIO)
		pg_setcache_phys(&tpt);
	else
		pg_setcachable(&tpt);

	/*
	 * ptebits is for special hardware bits ONLY. No pfns, No processor
	 * cache algorithms, etc.
	 * We really want to be careful what we allow here so
	 * we do it per PRODUCT
	 */
	validbits = 
#if SN0
		(PG_UC_HSPEC|PG_UC_IO|PG_UC_MSPEC|PG_UC_MEMORY);
#else
		0L;
#endif
	if (ptebits & ~validbits)
		return EINVAL;

	ptebits |= PG_VR | PG_SV | pg_cachemode(&tpt);

	/* Set PG_M on writable regions to avoid pfault. */
	if (findattr(prp, vaddr)->attr_prot & PROT_W)
		ptebits |= PG_M;
	
	/* Build block list pointing to map table. */
	while (pfncnt) {
		sz = pfncnt;
		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
		ASSERT(pt && sz);
		pfncnt -= sz;
		vaddr = (uvaddr_t)((char *)vaddr + ctob(sz));

		for ( ; --sz >= 0; pfns++, pt++) {
			if (!pg_isvalid(pt))
				prp->p_nvalid++;
			pg_setpgi(pt, mkpde(ptebits, *pfns));
		}
	}
	/*
	 * Ensure that the pregions cc attributes are same as that in
	 * the pte's --- by default allocreg for prp would mark prp's cc
	 * attributes as un-cached. But depending on the flags,
	 * the page might be accessed cached.
	 */ 
	prp->p_attrs.attr_cc = tpt.pte.pte_cc;
	return 0;
}

/*
 * v_mappfns - like v_mapphys, map pages into user address space.
 * Instead of determining the pages from the virtual address like
 * v_mapphys, we take a list of arbitrary pages as an argument.
 */
int
v_mappfns(vhandl_t *vt, pfn_t *pfns, pgcnt_t pfncnt, int flags, __uint64_t ptebits)
{
	int error;
	vasid_t vasid;
	pas_t *pas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);

	if (error = v_initphys(vt, ddmap_genphysfault, ctob(pfncnt), NULL))
		return error;

	return v_pfns(pas, v_getpreg(vt), v_getaddr(vt), pfns, pfncnt, 
						flags, ptebits);
}

/*
 * v_enterpfns - like v_enterpage, enter pages into a mapped devices
 * page table, but we take a list of pfns to map instead of a kernel
 * address.
 * Returns 0 if pages entered correctly, errno on failure.
 */
int
v_enterpfns(
	vhandl_t *vt,
	uvaddr_t vaddr,
	pfn_t *pfns,
	pgcnt_t pfncnt,
	int flags,
	__uint64_t ptebits)	/* random hardware specific bits .. */
{
	preg_t *prp;
	reg_t *rp;
	int error;
	vasid_t vasid;
	pas_t *pas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);

	mraccess(&pas->pas_aspacelock);
	prp = v_getpreg(vt);
	ASSERT(prp);
	rp = prp->p_reg;
	reglock(rp);
	ASSERT(rp->r_flags & RG_PHYS);

	error = v_pfns(pas, prp, vaddr, pfns, pfncnt, flags, ptebits);

	/* SPT: if pmap is using Shared Page Tables, p_nvalid is not accurate */
	ASSERT(error || pmap_spt(prp->p_pmap) || 
	       count_valid(prp, prp->p_regva, prp->p_pglen) == prp->p_nvalid);
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	return error;
}

/*
 * v_getmappedpfns - return pfns of mapped space
 * If a page isn't valid, the pfn is set to -1.
 */
int
v_getmappedpfns(vhandl_t *vt, off_t off, pfn_t *pfns, pgcnt_t pfncnt)
{
	pde_t *pt;
	auto pgno_t sz;
	preg_t *prp = v_getpreg(vt);
	uvaddr_t vaddr;
	vasid_t vasid;
	pas_t *pas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	vaddr = v_getaddr(vt) + off;
	while (pfncnt > 0) {
		sz = pfncnt;
		if ((pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0)) == NULL)
			return ENOENT;
		ASSERT(sz);
		pfncnt -= sz;
		vaddr = (uvaddr_t)((char *)vaddr + ctob(sz));

		for ( ; --sz >= 0; pfns++, pt++)
			if (pg_isvalid(pt))
				*pfns = pt->pte.pg_pfn;
			else
				*pfns = -1L;
	}
	return 0;
}

/*
 * v_allocpage - allocate a page of kernel space.
 * This interface takes PolicyModule info into consideration etc..
 * XXX validate that vaddr+len doesn't exceed region bounds
 *
 * Currently flags is ignored - we could use it to signify that we don't
 * want to go sxbrk ...
 *
 * XXX note that none of the v_* interfaces really can handle large pages
 * at all ...
 */
/* ARGSUSED */
int
v_allocpage(
	vhandl_t *vt,
	uvaddr_t uvaddr,
	pfn_t *pfn,
	size_t *page_size, /* desired page size, real page size returned */
	int flags)
{
	preg_t *prp;
	reg_t *rp;
	/* REFERENCED */
	attr_t *attr;
	vasid_t vasid;
	pas_t *pas;
	pfd_t *pfd;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);

	/*
	 * we are allocating a non-swapping page for a PHYS region -
	 * do smem/rmem accounting now
	 */
	if (reservemem(GLOBAL_POOL, 1, 1, 1))
		return ENOMEM;
	mraccess(&pas->pas_aspacelock);
	prp = v_getpreg(vt);
	ASSERT(prp);
	rp = prp->p_reg;
	reglock(rp);
	ASSERT(rp->r_flags & RG_PHYS);

	attr = findattr(prp, uvaddr);
	if ((pfd = PMAT_PAGEALLOC(attr, btoc(uvaddr), 0, page_size, uvaddr)) == NULL) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		unreservemem(GLOBAL_POOL, 1, 1, 1);

		/* kick scheduler and tell caller to
		 * try again
		 */
		setsxbrk();
		return EAGAIN;
	}
	*pfn = pfdattopfn(pfd);

	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	return 0;
}

/* ARGSUSED */
void
v_freepage(vhandl_t *vt, pfn_t pfn)
{
	pfd_t *pfd;

	pfd = pfntopfdat(pfn);
	pagefree(pfd);
	unreservemem(GLOBAL_POOL, 1, 1, 1);
}

/*
 * ddv interfaces 
 *
 * These differ from the v_ interfaces primarily in that these can be
 * called from other than drvmap/unmap routines.
 * ddv_handles are only valid on a single cell.
 */
struct __ddv_handle_s {
	struct pregion *ddv_preg;
	uvaddr_t ddv_uvaddr;/* attach address in process space */
	size_t ddv_len;	    /* length in bytes of mapping */
	vasid_t ddv_vasid;  /* handle to (local) address space */
	void *ddv_uq;	    /* uniqifier - currently we use the faulthandler */
};

/*
 * get a ddv handle from a vt handle. By definition vt handles are constructed
 * on the fly on behalf of the 'current' thread and aren't persistent.
 *
 * ddv_handles on the other hand are persistent and hold a reference
 * to the address space. They must be disposed of using ddv_destroy_handle
 * which will drop the reference count and free the ddv_handle
 */
ddv_handle_t
v_getddv_handle(vhandl_t *vt)
{
	/* REFERENCED */
	int error;

	ddv_handle_t ddv = kern_malloc(sizeof(struct __ddv_handle_s));
	ddv->ddv_preg = vt->v_preg;
	ddv->ddv_uvaddr = ddv->ddv_preg->p_regva;
	ddv->ddv_len = ctob(ddv->ddv_preg->p_pglen);
	ddv->ddv_uq = (void *)vt->v_preg->p_reg->r_fault;
	ASSERT(ddv->ddv_uq);
	/*
	 * perform a lookup on the asid - this will validate it and
	 * grab a reference
	 */
	error = as_lookup(curuthread->ut_asid, &ddv->ddv_vasid);
	ASSERT(error == 0);
	return ddv;
}

void
ddv_destroy_handle(ddv_handle_t ddv)
{
	as_rele(ddv->ddv_vasid);
	kern_free(ddv);
}

/*
 * return user virtual address where region specified by ddv_handle_t is mapped
 */
uvaddr_t
ddv_getaddr(ddv_handle_t ddv)
{
	return ddv->ddv_uvaddr;
}

/*
 * ddv_getlen - return length in bytes
 */
size_t
ddv_getlen(ddv_handle_t ddv)
{
	return ddv->ddv_len;
}

/*
 * ddv_getprot - return protections
 */
int
ddv_getprot(ddv_handle_t ddv, off_t off, int *max, int *prot)
{
	return getprot(ddv->ddv_vasid, ddv->ddv_uvaddr + off, max, prot);
}

/*
 * lock the address space protecting 'ddv'
 * XXX until we have a real address space object - assume current process
 * XXX should really be ddv_as ....
 */
void
ddv_lock(ddv_handle_t ddv)
{
	pas_t *pas;
	preg_t *prp = ddv->ddv_preg;

	pas = VASID_TO_PAS(ddv->ddv_vasid);

	mraccess(&pas->pas_aspacelock);
	reglock(prp->p_reg);
}

void
ddv_unlock(ddv_handle_t ddv)
{
	pas_t *pas;
	preg_t *prp = ddv->ddv_preg;

	pas = VASID_TO_PAS(ddv->ddv_vasid);

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	ASSERT(mrislocked_update(&prp->p_reg->r_lock));
	regrele(prp->p_reg);
	mrunlock(&pas->pas_aspacelock);
}

/*
 * map a set of pages. This is really equivalent to v_pg but can be called
 * using a ddv handle. It uses the protections and cache algorithm specified
 * in the pregion attributes.
 * XXX should we do more checking that the page can realistically use the
 * the cache algorithm specified in the attributes??
 * Requires that the ddv be locked via a ddv_lock() call.
 */
int
ddv_mappages(ddv_handle_t ddv,
	off_t off,		/* byte offset in space to map */
	void *kvaddr,	        /* kernel address (can be physical) to map */
	pgcnt_t pgcnt)		/* # of pages to map */
{
	preg_t *prp = ddv->ddv_preg;
	register pde_t *ptep;
	attr_t *attr;
        pgno_t nvalid, tmp_size;
	pfn_t pagenum;
	pgi_t bits;
	/* REFERENCED */
	pas_t *pas;
	uvaddr_t uvaddr;

	pas = VASID_TO_PAS(ddv->ddv_vasid);

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	ASSERT(mrislocked_update(&prp->p_reg->r_lock));

	uvaddr = ddv->ddv_uvaddr + off;
	pagenum = kvtophyspnum(kvaddr);

#ifdef MH_R10000_SPECULATION_WAR
	/*
	 * Ensure that the pregions cc attributes are same as that in
	 * the pte's --- by default allocreg for prp would mark prp's cc
	 * attributes as un-cached. But depending on the allocated addr,
	 * the page might be accessed cached.
	 */
	if (prp->p_type == PT_MEM) {
		pde_t tpt;
		tpt.pgi = 0;
		if (IS_KSEG1(kvaddr) ||
		    (IS_KSEG2(kvaddr) && pg_isnoncache(kvtokptbl(kvaddr))))
			pg_setcache_phys(&tpt);
		else
			pg_setcachable(&tpt);
		prp->p_attrs.attr_cc = tpt.pte.pte_cc;	
	}
#endif /* MH_R10000_SPECULATION_WAR */

        while (pgcnt) {
                tmp_size = pgcnt;
                /*
                 * pfn locking is not needed here because we're
                 * dealing with physical regions only, which map
                 * non-migratable memory only.
                 */
                ptep = pmap_ptes(pas, prp->p_pmap, uvaddr, &tmp_size, 0);
		if (ptep == NULL) {
			return EINVAL;
		}
                pgcnt -= tmp_size;
		nvalid = 0;

                while (tmp_size) {
			/*
			 * set prot/cc mode based on how region was set up
			 */
			bits = PG_VR|PG_SV;
			attr = findattr(prp, uvaddr);
			if (attr->attr_prot & PROT_WRITE)
				bits |= PG_M;
			if (!pg_isvalid(ptep))
				nvalid++;
                        pg_setpgi(ptep, mkpde(bits, pagenum));
                        
			pg_setccuc(ptep, attr->attr_cc, attr->attr_uc);
			
			if (IS_KSEG2(kvaddr)) {
				if ((__psunsigned_t)kvaddr >=
						K2SEG + ctob(syssegsz))
					return ENXIO;

				/*
				 * Copy any uncache attributes that could be
				 * associated with kvaddr.
				 */
				pg_copy_uncachattr(ptep, kvtokptbl(kvaddr));
			} else if (IS_KSEG1(kvaddr)) {
				/* 
				 * Transfer any uncached attributes on "kvaddr"
				 * to PTE.
				 */
				pg_xfer_uncachattr(ptep, kvaddr);
			}
			ptep++;
			uvaddr = (char *)uvaddr + ctob(1);
			kvaddr = (char *)kvaddr + ctob(1);
                        pagenum++;
                        tmp_size--;
                }
		prp->p_nvalid += nvalid;
        }

	return 0;
}

/*
 * map a set of pages. This is really equivalent to v_pfn but can be called
 * using a ddv handle. It uses the protections and cache algorithm specified
 * in the pregion attributes.
 * XXX should we do more checking that the page can realistically use the
 * the cache algorithm specified in the attributes??
 * Requires that the ddv be locked via a ddv_lock() call.
 */
int
ddv_mappfns(ddv_handle_t ddv,
	off_t off,		/* offset into space */
	pfn_t *pfns,	        /* list of pfn's to map */
	pgcnt_t pgcnt,		/* # of pages to map */
	__uint64_t ptebits)	/* random hardware specific bits .. */
{
	preg_t *prp = ddv->ddv_preg;
	register pde_t *ptep;
	attr_t *attr;
        pgno_t nvalid, tmp_size;
	pgi_t bits, validbits;
	/* REFERENCED */
	pas_t *pas;
	uvaddr_t uvaddr;

	pas = VASID_TO_PAS(ddv->ddv_vasid);

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	ASSERT(mrislocked_update(&prp->p_reg->r_lock));
	uvaddr = ddv->ddv_uvaddr + off;

	/*
	 * ptebits is for special hardware bits ONLY. No pfns, No processor
	 * cache algorithms, etc.
	 * We really want to be careful what we allow here so
	 * we do it per PRODUCT
	 */
	validbits = 
#if SN0
		(PG_UC_HSPEC|PG_UC_IO|PG_UC_MSPEC|PG_UC_MEMORY);
#else
		0L;
#endif
	if (ptebits & ~validbits)
		return EINVAL;

	ptebits |= PG_VR|PG_SV;

        while (pgcnt) {
                tmp_size = pgcnt;
                /*
                 * pfn locking is not needed here because we're
                 * dealing with physical regions only, which map
                 * non-migratable memory only.
                 */
                ptep = pmap_ptes(pas, prp->p_pmap, uvaddr, &tmp_size, 0);
		if (ptep == NULL) {
			return EINVAL;
		}
                pgcnt -= tmp_size;
		nvalid = 0;

		for ( ; --tmp_size >= 0; pfns++, ptep++) {
			/*
			 * set prot/cc mode based on how region was set up
			 */
			bits = ptebits;
			attr = findattr(prp, uvaddr);
			if (attr->attr_prot & PROT_WRITE)
				bits |= PG_M;
			if (!pg_isvalid(ptep))
				nvalid++;
                        pg_setpgi(ptep, mkpde(bits, *pfns));
			pg_setccuc(ptep, attr->attr_cc, attr->attr_uc);
			uvaddr = (char *)uvaddr + ctob(1);
                }
		prp->p_nvalid += nvalid;
        }
	return 0;
}

/*
 * invalidate tlb - this is primarily for use at context switch time.
 * it does no locking and acts on the current CPU only and assume 'current'
 * thread.
 * Passing in a len == -1 means the entire mapped area (uvaddr is ignored)
 */
void
ddv_invaltlb(ddv_handle_t ddv, off_t off, ssize_t len)
{
	int s;
	pgcnt_t npages;
	pgno_t vpn;
	uvaddr_t uvaddr;

	if (len == -1L) {
		len = ddv->ddv_len;
		uvaddr = ddv->ddv_uvaddr;
	} else
		uvaddr = off + ddv->ddv_uvaddr;
	ASSERT((uvaddr + len) <= (ddv->ddv_uvaddr + ddv->ddv_len));

	vpn = btoct(uvaddr);
	npages = btoc(len);
	/* no migration/no preemption */
	s = splhi();
	for (; npages > 0; vpn++, npages--)
		unmaptlb(curuthread->ut_as.utas_tlbpid, vpn);
	splx(s);
}

/*
 * ddv_invalphys
 *
 * Return 0 if invalidation successful (at 1 page found and removed)
 *	  ENOENT if no valid pages were found
 *	  EINVAL if uvaddr,len aren't properly part of mapping
 */
int
ddv_invalphys(ddv_handle_t ddv, off_t off, ssize_t len)
{
	preg_t *prp;
	pgcnt_t npages;
	int error = 0;
	pas_t *pas;
	ppas_t *ppas;
	pgcnt_t nfree;
	uvaddr_t uvaddr;

	if (len == 0)
		return 0;
	pas = VASID_TO_PAS(ddv->ddv_vasid);
	ppas = (ppas_t *)ddv->ddv_vasid.vas_pasid;

	mraccess(&pas->pas_aspacelock);
	if ((prp = findpreg(pas, ppas, ddv->ddv_uvaddr)) == NULL) {
		mrunlock(&pas->pas_aspacelock);
		return EINVAL;
	}
	ASSERT(prp->p_reg->r_flags & RG_PHYS);

	if (len == -1L) {
		len = ddv->ddv_len;
		uvaddr = ddv->ddv_uvaddr;
	} else
		uvaddr = off + ddv->ddv_uvaddr;
	ASSERT((uvaddr + len) <= (ddv->ddv_uvaddr + ddv->ddv_len));
	npages = btoc(len);

	reglock(prp->p_reg);

	nfree = pmap_free(NULL, prp->p_pmap, uvaddr, npages, PMAPFREE_TOSS);
	if (nfree == 0)
		error = ENOENT;
	prp->p_nvalid -= nfree;
	ASSERT(prp->p_nvalid >= 0);

	regrele(prp->p_reg);
	mrunlock(&pas->pas_aspacelock);
	return error;
}

/*
 * ddv_invalphys_fal
 *
 * Alas, this routine can be called from context switch time (with a len == -1L)
 * which basically means that NO locks may be obtained....
 *
 * We assume callers understand what is going on...
 * N.B. no checking of parameters is done ... no range checking, nothing...
 *
 * Return 0 if  invalidation successful (at 1 page found and removed)
 *	  ENOENT if no valid pages were found
 */
int
ddv_invalphys_fal(ddv_handle_t ddv, off_t off, ssize_t len)
{
	preg_t *prp = ddv->ddv_preg;
	pgcnt_t npages;
	int error = 0;
	pgcnt_t nfree;
	uvaddr_t uvaddr;
	/* REFERENCED */
	ssize_t slen = len;

	if (len == 0)
		return 0;

	ASSERT(prp->p_reg->r_flags & RG_PHYS);
	ASSERT(prp->p_regva == ddv->ddv_uvaddr);

	if (len == -1L) {
		len = ddv->ddv_len;
		uvaddr = ddv->ddv_uvaddr;
	} else
		uvaddr = off + ddv->ddv_uvaddr;
	npages = btoc(len);

	nfree = pmap_free(NULL, prp->p_pmap, uvaddr, npages, PMAPFREE_TOSS);
	if (nfree == 0)
		error = ENOENT;
	prp->p_nvalid -= nfree;
	ASSERT(prp->p_nvalid >= 0);
	ASSERT(slen > 0 || prp->p_nvalid == 0);

	return error;
}

/*
 * Generic PHYS region fault handler
 * Simply returns 1 signifying unable to handle the fault
 */
/* ARGSUSED */
static int
ddmap_genphysfault(vhandl_t *vt, void *s, uvaddr_t vaddr, int rw)
{
	return(1);
}

/*
 * Special GFX interfaces for attaching and detaching a graphics region
 * these guys don't use mmap ...
 */

/*
 * ddv_mmap(pflag, size, vaddr, faultfunc, special, vmap_tab, vmap_size)
 *
 * create a new mapping. The mapping has a starting virtual address
 * of 'vaddr' and is 'size' bytes with a fault handler of 'faultfunc' with the
 * invoking argument 'special'. The mapping's attributes will be set up
 * using 'vmap_tab' and 'vmap_size'.
 */
static int
ddv_mmap(int flag,
	size_t size,
	uvaddr_t uvaddr,
	int (*faultfunc)(vhandl_t *, void *, uvaddr_t, int),
	void *special,
	int regtype,
	v_mapattr_t *vmap_tab,
	int vmap_size,
	ddv_handle_t *ddvp)
{
	reg_t *rp;
	preg_t *prp;
        struct pm *pm;
	vhandl_t vt;
	pgno_t region_size;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;
	v_mapattr_t * new_vmap_tab = NULL;
	int i;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

#if IP32 || IP30
        if (uvaddr == NULL || !IS_KUSEG(uvaddr)) {
		/*
		 * The crime driver uses this path
		 * to map and pin various pages into user address
		 * space, at an address supplied by the system,
		 * not the user.
		 * vaddr == NULL implies we don't care about
		 * cache coloring - either the page is uncached
		 * and will stay that way, or we're mapping
		 * tiles, for which colorof(0) is appropriate,
		 * in the event the tiles are made cacheable.
		 * If vaddr is not NULL it specifies the
		 * desired cachecolor.
		 */
#ifdef _VCE_AVOIDANCE
		extern int vce_avoidance;
#endif
		mrupdate(&pas->pas_aspacelock);
		if (chkpgrowth(pas, ppas, btoc(size)) < 0) {
			mrunlock(&pas->pas_aspacelock);
			return ENOMEM;
		}
                /*
		 * Allocate a user address to map the
		 * pages, appropriately colored if necessary.
		 */
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance)
			uvaddr = pas_allocaddr(pas, ppas,
						btoc(size), colorof(uvaddr));
		else
#endif
			uvaddr = pas_allocaddr(pas, ppas, btoc(size), NOCOLOR);

                if (uvaddr == NULL) {
			mrunlock(&pas->pas_aspacelock);
			return ENOMEM;
		}
		mrunlock(&pas->pas_aspacelock);

#if IP30
		/* create a new table with absolute addresses */
		if (vmap_size && vmap_tab) {

			if (!(new_vmap_tab = kern_malloc(vmap_size * sizeof(v_mapattr_t))))
				return ENOMEM;

			bcopy(vmap_tab, new_vmap_tab, vmap_size * sizeof(v_mapattr_t));

			for (i = 0; i < vmap_size; i++) {
				new_vmap_tab[i].v_start = uvaddr + (__psunsigned_t) new_vmap_tab[i].v_start;
				new_vmap_tab[i].v_end = uvaddr + (__psunsigned_t) new_vmap_tab[i].v_end;
			}

			vmap_tab = new_vmap_tab;
		}
#endif
	}
#endif

        /*
         * Need to create the graphics region.
         *
         * Set the flags to RG_PHYS meaning that this region points to
         * physical memory directly, so its pages won't get freed or
         * paged out.
         *
         * Set flag so that the graphics region's page tables won't ever
         * get swapped out.  Otherwise when we invalidates the PTEs in
         * the page tables, we might try to use invalid pointers to
         * the page tables.
         */
        if ((rp = allocreg(NULL, RT_MEM, RG_PHYS)) == NULL) {
		if (new_vmap_tab) kern_free(new_vmap_tab);
                return ENOMEM;
	}

        /*
         * Attach the region.
         * The aspacelock must be held for update.
         */
	mrupdate(&pas->pas_aspacelock);
        pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
        if (vattachreg(rp, pas, ppas, uvaddr, (pgno_t) 0, rp->r_pgsz, regtype,
	       PROT_RW, PROT_RW, flag & VDD_GFX_NOSHARE ? PF_NOSHARE : 0,
	       pm, &prp)) {
                aspm_releasepm(pm);
                freereg(pas, rp);
		mrunlock(&pas->pas_aspacelock);
		if (new_vmap_tab) kern_free(new_vmap_tab);
                return ENOMEM;
        }

        aspm_releasepm(pm);
	if (vgrowreg(prp, pas, ppas, PROT_RW, btoc(size))) {
		vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen, RF_NOFLUSH);
		mrunlock(&pas->pas_aspacelock);
		if (new_vmap_tab) kern_free(new_vmap_tab);
		return EFAULT;
	}

        /*
         * Grow the region to its full size to cause invalid ptes to
         * be allocated. Set up fault handler.
         * The aspacelock must be held for update.
         */
        vt.v_preg = prp;
        vt.v_addr = uvaddr;

        if (v_setupphys(&vt, faultfunc, size, special, vmap_tab, vmap_size) != 0) {
		vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen, RF_NOFLUSH);
		mrunlock(&pas->pas_aspacelock);
		if (new_vmap_tab) kern_free(new_vmap_tab);
		return EFAULT;
        }

	/* done with duplicate table */
	if (new_vmap_tab) kern_free(new_vmap_tab);

	/*
	 * force pre-allocation of segment table and set the NOTRIM
	 * flag to keep it that way
	 * XXX this means that any gfx program NEVER gets ANY page tables
	 * trimmed - nice benefit!
	 */
        region_size = (pgno_t)btoc(size);
        pmap_ptes(pas, prp->p_pmap, uvaddr, &region_size, 0);
        prp->p_pmap->pmap_flags |= PMAPFLAG_NOTRIM;

        regrele(prp->p_reg);
	mrunlock(&pas->pas_aspacelock);

	/* gfx_unmap automatically frees this */
	*ddvp = v_getddv_handle(&vt);
	return 0;
}

int
gfxdd_mmap(int flag,
	size_t size,
	uvaddr_t uvaddr,
	int (*faultfunc)(vhandl_t *, void *, uvaddr_t, int),
	void *special,
	v_mapattr_t *vmap_tab,
	int vmap_size,
	ddv_handle_t *ddvp)
{
	return (ddv_mmap(flag, size, uvaddr, faultfunc, special, PT_GR,
			 vmap_tab, vmap_size, ddvp));
}

int
gfxdd_mmap_mem(int flag,
	size_t size,
	uvaddr_t uvaddr,
	ddv_handle_t *ddvp)
{
	return (ddv_mmap(flag, size, uvaddr, ddmap_genphysfault, NULL,
			PT_MEM, 0, 0, ddvp));
}

int
gfxdd_mmap_mem_ex(int           flag, 
		  size_t        size, 
		  uvaddr_t      uvaddr, 
		  v_mapattr_t  *vmap_attr, 
		  int           vmap_size, 
		  ddv_handle_t *ddvp)
{
	return (ddv_mmap(flag, size, uvaddr, ddmap_genphysfault, NULL,
			 PT_MEM, vmap_attr, vmap_size, ddvp));
}

/*
 * gfxdd_unmap(ddv)
 * Nothing more than detachreg(), takes care of  MP locking.
 * 'ddv' is an opaque pointer that was returned by a previous called
 * to gfx_mmap()
 */
void
gfxdd_munmap(ddv_handle_t ddv)
{
	preg_t *prp = ddv->ddv_preg;
	pas_t *pas;
	ppas_t *ppas;

	pas = VASID_TO_PAS(ddv->ddv_vasid);
	ppas = (ppas_t *)ddv->ddv_vasid.vas_pasid;

	ASSERT(prp);
	mrupdate(&pas->pas_aspacelock);

        /* Detach this region. */
        reglock(prp->p_reg);
        vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen, 0);

	mrunlock(&pas->pas_aspacelock);
	ddv_destroy_handle(ddv);
}

/*
 * gfxdd_lookup - see whether handle is still mapped to the current process
 * Returns 1 if it is, 0 otherwise.
 */
int
gfxdd_lookup(ddv_handle_t ddv)
{
	int rv;
	as_getattr_t asattr;
	as_getattrin_t asin;

	asin.as_uvaddr = (uvaddr_t)ddv->ddv_uq;
	rv = VAS_GETATTR(ddv->ddv_vasid, AS_GFXLOOKUP, &asin, &asattr);
	return !rv;
}

/*
 * gfxdd_recycle - look for a graphics region at the specified address
 * If one is found, do a callback and assign a new fltarg
 * The callback gets the same handle that gfxdd_map would get.
 */
int
gfxdd_recycle(void (*pfunc)(void *, void *, void *), void *special,
							uvaddr_t uvaddr)
{
	int rv = 0;
	vhandl_t vt;
	vasid_t vasid;
	as_getattr_t asattr;
	as_getattrin_t asin;

	as_lookup_current(&vasid);
	VAS_LOCK(vasid, AS_SHARED);

	asin.as_uvaddr = uvaddr;
	if (VAS_GETATTR(vasid, AS_GFXRECYCLE, &asin, &asattr) == 0) {
		/* found it */
		vt.v_preg = (preg_t *)asattr.as_recycle.as_recyclearg;
		vt.v_addr = uvaddr;
#ifdef DEBUG
		/*
		 * XXX we may leak a ddv_handle here
		 * since ''pfunc' in many gfx implementations
		 * simply zaps the old pointer..
		 */
		cmn_err(CE_WARN, "gfxdd_recycle:leaked a ddv_handle??");
#endif
		pfunc(asattr.as_recycle.as_fltarg,
			(void *)v_getddv_handle(&vt),
			special);
		vt.v_preg->p_reg->r_fltarg = special; /* XXX */
		rv = 1;
	}
	VAS_UNLOCK(vasid);
	return rv;
}

#include <sys/sg.h>

int
kvsgset(caddr_t vaddr, int len, struct sg *vec, int maxvec, unsigned *_resid)
{
	register int		nvec = 0;

	/*
	**	For each (partial) page of memory,
	**	set up one scatter / gather descriptor.
	*/

	if (IS_KSEGDM(vaddr)) {

		for ( ; nvec < maxvec; nvec++) {
			if (len == 0)
				break;

			if ((vec->sg_bcount = NBPP - poff(vaddr)) > len)
				vec->sg_bcount = len;
			
			vec->sg_ioaddr = svirtophys(vaddr);

			len -= vec->sg_bcount;
			vaddr += vec->sg_bcount;
			vec++;
		}


	} else if (IS_KSEG2(vaddr)) {
		register pde_t	*pde = kvtokptbl(vaddr);
		register int	offset = poff(vaddr);

		for ( ; nvec < maxvec; nvec++) {
			if (len == 0 || !pg_ishrdvalid(pde))
				break;

			if ((vec->sg_bcount = NBPP - offset) > len)
				vec->sg_bcount = len;

			vec->sg_ioaddr = pdetophys(pde++) + offset;
			len -= vec->sg_bcount;
			vec++;
			offset = 0;
		}
	}

	*_resid = len;
	return nvec;
}

/*
**	Test whether a particular address is considered
**	probeable by /dev/kmem.
*/

int
is_kmem_space(void *vaddr, ulong size)
{
	register struct kmem_ioaddr	*km;
	register __psunsigned_t addr = (__psunsigned_t)vaddr;
	extern struct kmem_ioaddr kmem_ioaddr[];

	/* don't permit overflows */
	if (addr + size < addr)
		return 0;
	for (km = kmem_ioaddr; km->v_base; km++) {
		if (addr < km->v_base)
			continue;

		if (addr + size <= km->v_base + km->v_length)
			return(addr);
	}

	return 0;
}

/*
**	Convert K[012]SEG address to physical page number.
*/
pfn_t
kvtophyspnum(void *kv)
{
	if (IS_KSEGDM(kv))
		return pnum(KDM_TO_PHYS(kv));
#if _MIPS_SIM == _ABI64
	if (IS_COMPAT_PHYS(kv))
		return pnum(COMPAT_TO_PHYS(kv));
#endif	/* if _MIPS_SIM == _ABI64 */
	if (pnum((__psunsigned_t)kv - K2SEG) < syssegsz)
		return pdetopfn(kvtokptbl(kv));
	return pnum(kv);
}

/* System 5.4 DKI interfaces */

/*
**	Convert K[012]SEG address to physical.
*/
paddr_t
kvtophys(void *kv)
{
#if MAPPED_KERNEL && CELL_IRIX
	if (IS_MAPPED_KERN_RO(kv))
		return(MAPPED_KERN_RO_TO_PHYS(kv));
	if (IS_MAPPED_KERN_RW(kv))
		return(MAPPED_KERN_RW_TO_PHYS(kv));

#endif
	/* Allow kernel stack translation */

	if (pnum(kv) == pnum(KSTACKPAGE))
		return pdetophys(&curuthread->ut_kstkpgs[KSTKIDX]) + poff(kv);

	if (IS_KSEGDM(kv))
		return KDM_TO_PHYS(kv);
#if _MIPS_SIM == _ABI64
	if (IS_COMPAT_PHYS(kv))
		return COMPAT_TO_PHYS(kv);
#endif	/* if _MIPS_SIM == _ABI64 */
	if (pnum((__psint_t)kv - K2SEG) < syssegsz)
		return pdetophys(kvtokptbl(kv)) + poff(kv);
	return (paddr_t)kv;
}

/*
**	Convert K[012]SEG address to physical page.
*/
paddr_t
kvtophyspg(void *kv)
{
	if (IS_KSEGDM(kv))
		return KDM_TO_PHYS(kv) & ~(NBPP-1);
#if _MIPS_SIM == _ABI64
	if (IS_COMPAT_PHYS(kv))
		return COMPAT_TO_PHYS(kv) & ~(NBPP-1);
#endif	/* if _MIPS_SIM == _ABI64 */
	if (pnum((__psint_t)kv - K2SEG) < syssegsz)
		return pdetophys(kvtokptbl(kv));
	return (paddr_t)kv & ~(NBPP-1);
}

paddr_t
pptophys(register pfd_t *pfd)
{
	return (pfdattopfn(pfd));
}
