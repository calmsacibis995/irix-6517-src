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
#ident  "$Revision: 1.28 $"

#include "sys/types.h"
#include "sys/anon.h"
#include "ksys/as.h"
#include "as_private.h"
#include "sys/atomic_ops.h"
#include "sys/cachectl.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/kmem.h"
#include "limits.h"
#include "sys/lock.h"
#include <sys/numa.h>
#include "pmap.h"
#include "region.h"
#include "sys/vnode.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "ksys/vpag.h"


int
pas_msync(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr, size_t len, uint_t flags)
{
	register preg_t		*prp;
	register reg_t		*rp;
	register vnode_t	*vp;
	size_t		size;
	uvaddr_t	pregend;
	int	error = 0;
	off_t	file_start; 	/* Starting byte offset in file */
	off_t	file_end;   	/* Ending byte offset in file */
	off_t	nreg_file_start;/* Starting file offset of new region */
	off_t	nreg_file_end;	/* Ending file offset of new region */
	uvaddr_t toss_va;	/* Starting address of range to be tossed */
	size_t	toss_len;	/* Length of the address range to toss */

	if (flags == 0) {
		/*
		 * Turn on MS_SYNC in the absense of MS_ASYNC.
		 */ 
		flags |= MS_SYNC;
	}

	/*
	 * POSIX compliance.
	 * Can't have both MS_SYNC and MS_ASYNC.
	 */
	if ((flags & (MS_SYNC|MS_ASYNC)) == (MS_SYNC|MS_ASYNC)) {
		return EINVAL;
	}

	while (len > 0) {
		/*
		 * Go through the address range a region at a time
		 */
		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
			error = ENOMEM;
			break;
		}

		rp = prp->p_reg;
		ASSERT(rp);
		reglock(rp);
		vp = rp->r_vnode;

		pregend = prp->p_regva + ctob(prp->p_pglen);
		if (vaddr + len > pregend)
			size = pregend - vaddr;
		else
			size = len;

		/*
		 * If no vp, can't be mapped file region.  Skip.
		 * Forget PHYS regions or any regions that aren't
		 * shared mapped files.   XXX Private mapped files?
		 */
		if (!vp || rp->r_type != RT_MAPFILE || (rp->r_flags & RG_PHYS))
		{
			regrele(rp);
			vaddr += size;
			len -= size;
			continue;
		}

		/* sync to file */
		error = msync1(pas, prp, vaddr, btoc(size), 0);

		file_start = ctob(prp->p_offset) + rp->r_fileoff + 
					(vaddr - prp->p_regva);
		file_end = file_start + size;
		vaddr += size;
		len -= size;

		regrele(rp);

		if (error)
			return error;


		/*	Since the page and buffer cache are unified, the page
		 *	must reflect backing store for local file systems.
		 *	Therefore, there is nothing to do for INVALIDATE on
		 *	local file systems.????
		 *
		 *	The invalidation will fail if the region is isolated
		 *	or page(s) within the region is/are locked.
		 */
		if (flags & MS_INVALIDATE) {
			register preg_t	*nprp;
			register reg_t	*nrp;
			register int 	s;


start_over:
			s = mutex_spinlock(&mreg_lock);
			for (nprp = vp->v_mreg;
			     nprp != (preg_t *)vp;
			     nprp = nprp->p_vchain) {

				nrp = nprp->p_reg;
				ASSERT(nrp);

				if (nrp->r_type != RT_MAPFILE)
					continue;

				nrp->r_nofree++;	/* bump cnt so region */
							/* isn't freed        */
				nprp->p_nofree++;	/* bump cnt so pregion*/
							/* isn't freed        */
				mutex_spinunlock(&mreg_lock, s);
				reglock(nrp);

				/*
				 * See if the (p)region has been detached while we
				 * were asleep.  If so, free the (p)region now and
				 * start over again from the beginning.  We
				 * can't continue from where we were in the
				 * list since our preg pointer is no good
				 * anymore in this case.  (See freereg()/freepreg()
				 * for more details).
				 */
				if (nrp->r_type == RT_UNUSED || nprp->p_reg == NULL) {

					ASSERT( !(nprp->p_reg && nrp->r_type == RT_UNUSED));
					ASSERT(nprp->p_nofree <= nrp->r_nofree);

					/*
					 * Decrement our hold on the pregion. If there are
					 * no other holds and the pregion no longer points
					 * to the region, free the structure. freepreg has
					 * done all the work except for this.
					 * No mreg_lock needed - see below.
					 */
					if (--nprp->p_nofree == 0 && nprp->p_reg == NULL)
						kmem_zone_free(preg_zone, nprp);

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
					if (--nrp->r_nofree == 0 && nrp->r_type == RT_UNUSED)
						freereg(pas, nrp); /* we were last */
					else
						regrele(nrp);

					goto start_over;
				}
				
				nreg_file_start	 = ctob(nprp->p_offset) + 
							nprp->p_reg->r_fileoff;
				nreg_file_end = nreg_file_start + 
						ctob(nprp->p_pglen);
				if ((nreg_file_end > file_start) && 
					(nreg_file_start < file_end)) {

				/*
				 * As we are tossing entire regions out here
				 * we don't have to do any large page operations
				 * on address ranges.
				 */
					toss_va = nprp->p_regva +
						((file_start > nreg_file_start)?						(file_start - nreg_file_start)
						 : 0);

					toss_len = nprp->p_pglen - 
						btoct(toss_va - nprp->p_regva);
					toss_len -= (file_end < nreg_file_end ?
					   btoct(nreg_file_end - file_end) :0);

					pmap_downgrade_addr_boundary(pas, nprp, 
						toss_va, toss_len,
						PMAP_TLB_FLUSH);
					error = getmpages(pas, nprp, 
						GTPGS_TOSS|GTPGS_NOLOCKS,
						toss_va, toss_len);
				}
				s = mutex_spinlock(&mreg_lock);
				nrp->r_nofree--;
				nprp->p_nofree--;
				regrele(nrp);

				if (error) {
					mutex_spinunlock(&mreg_lock, s);
					return error;
				}

			}

			mutex_spinunlock(&mreg_lock, s);
			VOP_FSYNC(vp, FSYNC_INVAL, get_current_cred(), 
				       file_start, file_end - 1, error);
			if (error)
				break;
			continue;
		} else if (!(flags & MS_ASYNC)) {
		    	VOP_FSYNC(vp, FSYNC_WAIT, get_current_cred(), 
				       file_start, file_end - 1, error);
			if (error)
				break;
		}
	}
	return error;
}


#if JUMP_WAR
extern uint R4000_jump_war_correct;
#ifdef _R5000_JUMP_WAR
extern int R5000_jump_war_correct;
#endif /* _R5000_JUMP_WAR */
#endif

int
pas_mprotect(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t vaddr,
	size_t len,
	mprot_t new_prot,
	int flags)
{
	pgcnt_t npgs;
	register preg_t	*prp;
	register reg_t	*rp;
	register attr_t	*attr;
	register pgcnt_t sz;
	uvaddr_t end;
	int status = 0;
	int clr_tlb = 0;
	uint shared = 0;
	int flushcache = 1;
	uvaddr_t attr_start, ovaddr = vaddr;
	pgno_t	attr_size;
	pgno_t	pgs;
	pde_t	*pt;
#if R10000_SPECULATION_WAR
#define PAS_MPROTECT_T5WAR	0x80000000	/* called for T5 war */
#define PAS_MPROTECT_ASPACE	0x40000000	/* holds mrupdate pas->aspace */
#define PAS_MPROTECT_REGION	0x20000000	/* holds reglock() */
	int t5_war_flags = flags;

	flags &= ~(PAS_MPROTECT_T5WAR|PAS_MPROTECT_ASPACE|PAS_MPROTECT_REGION);
#endif

	/*
	 * access lock protects region(s) from growing/shrinking while
	 * we're messing with the attributes.
	 */
#if R10000_SPECULATION_WAR
	if ((t5_war_flags & PAS_MPROTECT_ASPACE) == 0)
#endif
	mraccess(&pas->pas_aspacelock);
	npgs = numpages(vaddr, len);
	vaddr = (uvaddr_t)pbase(vaddr);

	/*
	 * Get rid of PROT_EXEC_NOFLUSH since it isn't really a valid
	 * protection.  Change it to a flag we can check later without trying
         * to assign it as a protection to a page.
	 */
	if(new_prot & PROT_EXEC_NOFLUSH) {
                new_prot &= ~PROT_EXEC_NOFLUSH;
                flushcache=0;
        }

	while (npgs) {
		/*
		 * Go through the address range a region at a time
		 */
		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
			status = ENOMEM;
			goto out;
		}

		if ((prp->p_maxprots & new_prot) != new_prot) {
			status = EACCES;
			goto out;
		}

		rp = prp->p_reg;

		/*
		 * If we're making a text region writable, turn shared
		 * regions into private copy-on-write regions now.
		 */
		if ((new_prot & PROT_WRITE) &&
		    rp->r_type == RT_MAPFILE &&
		    rp->r_flags & RG_TEXT) {
			mrunlock(&pas->pas_aspacelock);
			mrupdate(&pas->pas_aspacelock);

			if (status = vgetprivatespace(pas, ppas, vaddr,
							(preg_t **)NULL))
				goto out;

			mrunlock(&pas->pas_aspacelock);
			mraccess(&pas->pas_aspacelock);
			continue;	/* start over again for this region */
		}
			
		sz = MIN(npgs, pregpgs(prp, vaddr));
		end = vaddr + ctob(sz);
#if R10000_SPECULATION_WAR
		if ((t5_war_flags & PAS_MPROTECT_REGION) == 0)
#endif
		reglock(rp);

		/*
		 * SPT
		 * If we are removing write permissions, check for
		 * shared PT's (see comment in pmap.c).
		 */
		if ((new_prot & PROT_WRITE) == 0)
			prp->p_nvalid += pmap_modify(pas, ppas, 
						prp, vaddr, sz);

		attr = findattr_range(prp, vaddr, end);

		/*
		 * Hint: May be inefficient but easier to do if the next pregion
		 * does not exist.
		 */
		pmap_downgrade_addr_boundary(pas, prp, vaddr,
							sz, PMAP_TLB_FLUSH);

		ASSERT(attr);

		while (attr && attr->attr_end <= end) {

			/*
			 * If the user is shutting off write permission on
			 * pages that were writable, then we have to go
			 * through the ptes and clear the mod bit.
			 */
			if ((attr->attr_prot & PROT_WRITE) && 
			    (new_prot & PROT_WRITE) == 0) {

				attr_start = attr->attr_start;
				attr_size = btoc(attr->attr_end - attr_start);

				while (attr_size) {
				    pt = pmap_probe(prp->p_pmap, &attr_start, 
						    &attr_size, &pgs);
				    attr_start += ctob(pgs);
				    attr_size -= pgs;

				    while (pgs > 0) {
					    pg_clrmod(pt++);
					    pgs--;
				    }
				}

				/*
				 * permit write_utext to force us NOT
				 * to flush the tlb (see comment there)
				 */
				if (!flags)
					clr_tlb = 1;
				shared |= (prp->p_flags & PF_SHARED);
			}

			/*
			 * If the user is shutting off read permission on
			 * pages that were valid, then we have to go
			 * through the ptes and clear the hardware valid bit.
			 */

			/*
			 * If the user is asking for execute permission, then
			 * we need to check the page for workarounds.  Shut
			 * off the hardware valid bit here so vfault can do
			 * the work.  The check on flags is to allow us
                         * not to do this in the case of write_utext.  It
                         * never did this before the virtualization of
                         * and needed none of this jump_war stuff in this
                         * case so I believe it is safe not to do any of
                         * this in the write_utext now as well.
			 */
			if (((attr->attr_prot & PROT_READ) &&
			     (new_prot & PROT_READ) == 0)
#ifdef JUMP_WAR
			    ||
			    ((new_prot & PROT_EXEC) && flags == 0
                                                    && (R4000_jump_war_correct
#ifdef _R5000_JUMP_WAR
							||
							R5000_jump_war_correct
#endif /* _R5000_JUMP_WAR */
							))
#endif /* JUMP_WAR */			    
			    )
			{

				attr_start = attr->attr_start;
				attr_size = btoc(attr->attr_end - attr_start);

				while (attr_size) {
				    pt = pmap_probe(prp->p_pmap, &attr_start, 
						    &attr_size, &pgs);
				    attr_start += ctob(pgs);
				    attr_size -= pgs;

				    while (pgs > 0) {
					    pg_clrhrdvalid(pt++);
					    pgs--;
				    }
				}

				clr_tlb = 1;
				shared |= prp->p_flags & PF_SHARED;
			}

#if R10000_SPECULATION_WAR
			if ((t5_war_flags & PAS_MPROTECT_T5WAR) == 0)
#endif
			attr->attr_prot = new_prot;
			attr = attr->attr_next;
		}

#if R10000_SPECULATION_WAR
		if ((t5_war_flags & PAS_MPROTECT_REGION) == 0)
#endif
		regrele(rp);
		vaddr = end;
		npgs -= sz;
	}

out:
	if (clr_tlb) {
#if R10000_SPECULATION_WAR	/* we call with mrupdate */
		if (pas->pas_flags & PAS_SHARED &&
		    ((t5_war_flags & PAS_MPROTECT_ASPACE) == 0)) {
#else
		if (pas->pas_flags & PAS_SHARED) {
#endif
			/*
			 * Need to be holding update lock
			 * when calling newptes
			 */
			mrunlock(&pas->pas_aspacelock);
			mrupdate(&pas->pas_aspacelock);
		}
		newptes(pas, ppas, shared);
	}
#if R10000_SPECULATION_WAR
	if ((t5_war_flags & PAS_MPROTECT_ASPACE) == 0)
#endif
	mrunlock(&pas->pas_aspacelock);

	/*
	 * If the user is asking for execute permission, make the data and
	 * inst caches consistent and correct.  This is primarily for
	 * applications that generate code in their data and then
	 * execute it (like interpreters).  If there are write-back data
	 * caches on the system, then these must be written back so that
	 * instruction caches misses fetch the newly generated instructions.
	 * We must also invalidate the instruction caches since previously
	 * generated code may have be modified.  The application is
	 * expected to call mprotect each time they change their instructions
	 * before they execute them.  Therefore, it doesn't matter whether
	 * the page previously had execute permission or not.  We have to
	 * do this work each time.
         * If PROT_EXEC_NOFLUSH is specified, then the cache is not flushed.
         * It is up to the application to ensure cache coherency if it specifies
         * this flag.  See mprotect(2).
	 */
	if ((new_prot & PROT_EXEC) && flushcache) {
		cache_operation(ovaddr, len, CACH_ICACHE_COHERENCY);
	}

	return status;
}


static int madv_unreserve(uvaddr_t, size_t, pas_t *, ppas_t *);
static int madv_reserve(uvaddr_t, size_t, pas_t *, ppas_t *);

static void unreserve_span(pas_t *, ppas_t *, preg_t *, pgno_t, pgcnt_t);
static int reserve_span(preg_t *, pgno_t, pgcnt_t, pas_t *);

int
pas_madvise(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr, size_t len, uint_t behav)
{
	int error;

	switch(behav) {
#define MADV_REVALIDATE 6               /* reacquire resources */
	case MADV_REVALIDATE: { /* reacquire resources for this span */
		return (madv_reserve(vaddr, len, pas, ppas));
	}
#define MADV_INVALIDATE 5               /* release resources */
	case MADV_INVALIDATE: { /* give up resources for this span */
		return (madv_unreserve(vaddr, len, pas, ppas));
	}

	case MADV_DONTNEED: {
		register preg_t		*prp;
		register reg_t		*rp;
		register size_t		size;
		uvaddr_t		pregend;

		/*	We'll loop through the address space under the unlikely
		 *	chance that the user has specified an address range
		 *	spanning two (or more) contiguous regions.
		 */
		mraccess(&pas->pas_aspacelock);
		while (len > 0) {
			prp = findfpreg(pas, ppas, vaddr, vaddr+len);
			if (prp == NULL)
				break;

			rp = prp->p_reg;
			reglock(rp);
			pregend = prp->p_regva + ctob(prp->p_pglen);

			if (vaddr + len > pregend)
				size = pregend - vaddr;
			else
				size = len;

			/* For now, only handle mapped file regions.
			 * XXX Fix to toss/swap text, stack and data.
			 */
			if (prp->p_type == PT_MAPFILE && rp->r_vnode && 
			    !(rp->r_flags & RG_PHYS))
				error = msync1(pas, prp,  prp->p_regva, 
					       btoc(pregend-prp->p_regva),
					       GTPGS_TOSS|GTPGS_NOLOCKS);
			regrele(rp);
			vaddr += size;
			len -= size;

			/*
			 * We never criticize advise.
			 */
			if (error)
				break;
		}
		mrunlock(&pas->pas_aspacelock);
		break;
	}

	case MADV_SYNC_POLICY:
		return (PM_SYNC_POLICY(vaddr, len));

	default:
		return EINVAL;
	}
	return 0;
}

/*
 * From here to the bottom of the file are the much-reviled
 * madvise() changes. This stuff is NOT a public interface, and is
 * for use by exactly one user program, the window system. We _may_
 * keep this API, and we may not. We will certainly change its
 * implementation in the next set of releases.
 *
 * Here there be dragons.
 *
 */

static
int
sievelockedpages(pas_t *pas, preg_t *prp, caddr_t vaddr, int npgs)
{
        attr_t          *attr;
        pgno_t          pn;
        pfd_t           *pfd;
        pde_t           *pte;
        pgno_t          sz;

        /* one page at a time, sigh. */
        for (pn = npgs; pn; pn--) {
            sz = NBPP;
            pte = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
            pfd = pdetopfdat(pte);
            attr = findattr(prp, vaddr);
            if (attr->attr_lockcnt || pfd->pf_rawcnt)
                return -1;
            vaddr += NBPP;
        }

        return 0;
}

/*
 * madv_unreserve():
 *
 * give up resources for this set of pages. To simplify matters,
 * we insist on not spanning pregions.
 *
 * If the request does this, EINVAL.
 */
static int
madv_unreserve(uvaddr_t vaddr, size_t length, pas_t *pas, ppas_t *ppas)
{
	register preg_t	*prp;
	register reg_t	*rp;
	caddr_t		pregend;
	caddr_t		addr;
	pgno_t		lpn;
	int		len;
	int		npgs;
	/* REFERENCED */
	int		ret;

	mraccess(&pas->pas_aspacelock);

	prp = findfpreg(pas, ppas, vaddr, vaddr+length);
	if (prp == NULL) {
		mrunlock(&pas->pas_aspacelock);
		return EINVAL;
	}

	/* page-align address to next page */
	addr = (caddr_t)(((__psint_t)vaddr + (NBPP - 1)) & ~POFFMASK);

	/* clamp length to page multiples */
	len = ((length - (addr - vaddr)) / NBPP) * NBPP;

	rp = prp->p_reg;

	reglock(rp);

	/* this has to be an ANON page, and can't be shared */
	if (((rp->r_flags & RG_ANON) == 0) ||
	    (rp->r_flags & RG_AUTORESRV) ||
	    rp->r_refcnt != 1) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return EINVAL;
	}

	pregend = prp->p_regva + ctob(prp->p_pglen);

	/* if they try to span two regions, we tell them they can't */
	if (vaddr + length > pregend) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return EINVAL;
	}

	lpn = btoc(addr - prp->p_regva);
	npgs = numpages(addr, len);

        if (prp->p_noshrink && sievelockedpages(pas, prp, addr, npgs) < 0) {
            regrele(rp);
            mrunlock(&pas->pas_aspacelock);
            return EBUSY;
        }

	unreserve_span(pas, ppas, prp, lpn, npgs);
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	return 0;
}

/*
 * madv_reserve():
 *
 * reserve memory for this span of virtual address.
 *
 */
static int
madv_reserve(uvaddr_t vaddr, size_t length, pas_t *pas, ppas_t *ppas)
{
	register preg_t	*prp;
	register reg_t	*rp;
	caddr_t		pregend;
	caddr_t		addr;
	int		len;
	pgno_t		lpn;
	int		npgs;
	int		ret;

	mraccess(&pas->pas_aspacelock);

	prp = findpreg(pas, ppas, vaddr);
	if (prp == NULL) {
		mrunlock(&pas->pas_aspacelock);
		return EINVAL;
	}

	/* page-align address to next page */
	addr = (caddr_t)(((__psint_t)vaddr + (NBPP - 1)) & ~POFFMASK);

	/* clamp length to page multiples */
	len = ((length - (addr - vaddr)) / NBPP) * NBPP;

	rp = prp->p_reg;
	reglock(rp);

	/* this has to be an ANON page, and can't be shared */
	if (((rp->r_flags & RG_ANON) == 0) ||
	    (rp->r_flags & RG_AUTORESRV) ||
	    rp->r_refcnt != 1) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return EINVAL;
	}

	pregend = prp->p_regva + ctob(prp->p_pglen);

	/* if they try to span two regions, we tell them they can't */
	if (vaddr + length > pregend) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return EINVAL;
	}

	lpn = vtorpn(prp, addr);
	npgs = numpages(addr, len);
	ret = reserve_span(prp, lpn, npgs, pas);
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	return ret;
}

/* reservation data structure */
typedef
struct aspan {
	pgno_t		lpn;		/* starting lpn of span */
	pgcnt_t		npgs;		/* number of pages in span */
	struct aspan	*flink;		/* link to next span */
	struct aspan	*blink;		/* link to prev span */
} aspan_t;

/*
 * madv_detach():
 *
 * when a region is detached in freereg() adjust
 * how many pages we've got reserved. Also, free
 * the reservation data structures.
 *
 */

int
madv_detach(reg_t *rp)
{
	aspan_t *ap, *sp;
	int     unpgs = 0;

	if (rp->r_spans == NULL)
		return 0;

	for (ap = (aspan_t *)rp->r_spans; ap; ap = sp) {
		unpgs += ap->npgs;
		sp = ap->flink;
		kmem_free(ap, sizeof (aspan_t));
	}
	rp->r_spans = NULL;
	return unpgs;
}

#if 0
static void
dump_spans(reg_t *rp)
{
    aspan_t     *ap;

    printf("dump-spans 0x%x\n", rp->r_anon);
    for (ap = (aspan_t *)rp->r_spans; ap; ap = ap->flink) {
	printf("\t%d - %d (%d)\n",
	       ap->lpn, ap->lpn + ap->npgs - 1, ap->npgs);
        }
}
#endif

int
isunreserved(reg_t *rp, pgno_t lpn)
{
	aspan_t	*ap;

	for (ap = (aspan_t *)rp->r_spans; ap; ap = ap->flink)
	    if (lpn >= ap->lpn && lpn < ap->lpn + ap->npgs) {
		return 1;
	    }

	return 0;
}

static void
unreserve(pas_t *pas, ppas_t *ppas, preg_t *prp, pgno_t lpn, pgcnt_t npgs)
{
	caddr_t	addr = prp->p_regva + ctob(lpn);

	pgno_t dpn;
	pgcnt_t dpgs;
	sm_swaphandle_t onswap;
	void	*id;
	pgno_t nvalid;

	/* XXX shouldn't this pass a '1' for shared */
	newptes(pas, ppas, 0);
	nvalid = pmap_free(pas, prp->p_pmap, addr, npgs, PMAPFREE_FREE);
	prp->p_nvalid -= nvalid;
	pas_unreservemem(pas, npgs, 0);
	anon_remove(prp->p_reg->r_anon, rpntoapn(prp, lpn), npgs);

	/*
	 * this is here because the window system mysteriously decides
	 * to have an unreserved page in two different regions. Why it
	 * does this is an as-yet unsolved Mystery. We also remove it
	 * from the second region. Otherwise, vfault() gets very upset
	 * and kills the process. I want this to go away, it makes
	 * unreserve requests slow.
	 */
	dpgs = npgs;
	for (dpn = rpntoapn(prp, lpn); dpgs; dpgs--, dpn++) {
		id = anon_lookup(prp->p_reg->r_anon, dpn, &onswap);
		if (id)
		    anon_remove(id, dpn, 1);
	}
	
}

static void
rmspan(reg_t *rp, aspan_t *sp)
{
	aspan_t	*np;

	np = sp->flink;
	if (np == NULL) {
	    if (sp->blink == NULL)
		    rp->r_spans = NULL;
	    else
		    sp->blink->flink = NULL;
	} else {
		np->blink = sp->blink;
	    if (sp->blink == NULL)
		    rp->r_spans = (void *)np;
	    else
		    sp->blink->flink = np;
	}
	kmem_free(sp, sizeof (aspan_t));
}

void
insertspan(reg_t *rp, aspan_t *sp, pgno_t lpn, pgcnt_t npgs)
{
	aspan_t	*np = (aspan_t *)kmem_alloc(sizeof (aspan_t), KM_SLEEP);

	np->lpn = lpn;
	np->npgs = npgs;
	np->flink = NULL;
	np->blink = NULL;

	if (rp->r_spans == NULL) {
	    rp->r_spans = (void *)np;
	    return;
	}

	np->flink = sp;
	if (sp->blink == NULL) { 	/* head of list */
	    rp->r_spans = np;
	} else {			/* in front of */
	    sp->blink->flink = np;
	    np->blink = sp->blink;
	}
	sp->blink = np;
}

void
appendspan(reg_t *rp, aspan_t *sp, pgno_t lpn, pgcnt_t npgs)
{
	aspan_t	*np = (aspan_t *)kmem_alloc(sizeof (aspan_t), KM_SLEEP);

	np->lpn = lpn;
	np->npgs = npgs;
	np->flink = NULL;
	np->blink = NULL;

	if (rp->r_spans == NULL) {
	    rp->r_spans = (void *)np;
	    return;
	}

	if (sp->flink) {
	    np->flink = sp->flink;
	    np->flink->blink = np;
	}

	sp->flink = np;
	np->blink = sp;
}

static int
reserve_span(preg_t *prp, pgno_t lpn, pgcnt_t npgs, pas_t *pas)
{
	aspan_t	*sp;
	int		lepn, sepn, nnpgs;
	reg_t	*rp = prp->p_reg;

	while (npgs) {

	    if (rp->r_spans == NULL)
		return 0;

	    for (sp = (aspan_t *)rp->r_spans; sp; sp = sp->flink) {

		lepn = lpn + npgs - 1;
		sepn = sp->lpn + sp->npgs - 1;

		/* not us */
		if (lepn < sp->lpn)
		    return 0;

		if (lpn >= sp->lpn + sp->npgs) {
		    if (sp->flink == NULL)
			return 0;
		    continue;
		}

		/* front */
		if (lpn < sp->lpn) {
		    /* overlaps front */
		    if (lepn <= sepn) {
			nnpgs = lepn + 1 - sp->lpn;
			if (pas_reservemem(pas, nnpgs, 0))
				return EINVAL;
			sp->lpn = lepn + 1;
			sp->npgs -= nnpgs;
			if (sp->npgs == 0)
			    rmspan(rp, sp);
			return 0;
		    }
		    /* encloses */
		    if (pas_reservemem(pas, sp->npgs, 0))
			return EINVAL;
		    npgs -= (sepn + 1 - lpn);
		    lpn = sepn + 1;
		    rmspan(rp, sp);
		    break;
		}

		if (lpn == sp->lpn) {
		    if (npgs == sp->npgs) {
			if (pas_reservemem(pas, npgs, 0))
			    return EINVAL;
			rmspan(rp, sp);
			return 0;
		    }
		    if (lepn < sepn) {
			if (pas_reservemem(pas, npgs, 0))
			    return EINVAL;
			sp->npgs -= npgs;
			sp->lpn = lepn + 1;
			return 0;
		    }
		    if (pas_reservemem(pas, sp->npgs,0))
			     return EINVAL;
		    lpn = sepn + 1;
		    npgs -= sp->npgs;
		    rmspan(rp, sp);		
		    break;
		}

		/* enclosed */
		if (lpn > sp->lpn && lepn < sepn) {
		    if (pas_reservemem(pas, npgs, 0))
			return EINVAL;
		    sp->npgs = lpn - sp->lpn;
		    appendspan(rp, sp, lepn + 1, sepn - lepn);
		    return 0;
		}

		/* rear */
		if (sp->flink == NULL || lpn > sepn) {
		    nnpgs = sp->lpn + sp->npgs - lpn;
		    if (pas_reservemem(pas, nnpgs, 0))
			return EINVAL;
		    sp->npgs -= nnpgs;
		    return 0;
		}

		nnpgs = sepn + 1 - lpn;
		if (pas_reservemem(pas, nnpgs, 0))
		    return EINVAL;
		sp->npgs -= nnpgs;
		lpn = sepn + 1;
		npgs -= nnpgs;
		break;
	    }
	}

	return 0;
}

static void
unreserve_span(pas_t *pas, ppas_t *ppas, preg_t *prp, pgno_t lpn, pgcnt_t npgs)
{
	aspan_t	*sp, *np;
	pgcnt_t lepn, sepn, nnpgs;
	int swap;
	reg_t *rp = prp->p_reg;

	if (npgs <= 0)
	    return;

	if (rp->r_spans == NULL) {
	    appendspan(rp, NULL, lpn, npgs);
	    unreserve(pas, ppas, prp, lpn, npgs);
	    return;
	}

	while (npgs) {
	    for (sp = (aspan_t *)rp->r_spans; sp; sp = sp->flink) {

		lepn = lpn + npgs - 1;
		sepn = sp->lpn + sp->npgs - 1;	

		/* not us */
		if (lpn > sepn) {
		    if (sp->flink == NULL) {
			appendspan(rp, sp, lpn, npgs);
			unreserve(pas, ppas, prp, lpn, npgs);
			return;
		    } else
		    continue;
		}

		/* duplicate or enclosed, just toss it */
		if (lpn >= sp->lpn && lepn <= sepn)
		    return;

		/* overlaps front */
		if (lpn < sp->lpn) {
		    if (lepn < sp->lpn) {
			insertspan(rp, sp, lpn, npgs);
			unreserve(pas, ppas, prp, lpn, npgs);
			return;
		    }
		    nnpgs = sp->lpn - lpn;
		    unreserve(pas, ppas, prp, lpn, nnpgs);
		    sp->npgs += nnpgs;
		    npgs -= nnpgs;
		    swap = lpn;
		    lpn = sp->lpn;
		    sp->lpn = swap;
		    break;
		}

		/* overlaps rear */
		np = sp->flink;
		if (np == NULL || np->lpn > lepn) {
		    nnpgs = lepn - sepn;
		    unreserve(pas, ppas, prp, sepn + 1, nnpgs);
		    sp->npgs += nnpgs;
		    return;
		}
		
		nnpgs = np->lpn - (sp->lpn + sp->npgs);
		unreserve(pas, ppas, prp, sepn + 1, nnpgs);
		sp->npgs += nnpgs + np->npgs;
		npgs -= (np->lpn - lpn);
		lpn = np->lpn;
		rmspan(rp, np);
		break;
	    }
	}

	return;
}

#ifdef R10000_SPECULATION_WAR
/*
 * pacecar's internal interface for keeping T5 speculative loads
 * and stores from dirtying mpinned pages. We have to worry about
 * address spaces shared by sproc'd processes and by mmap.
 */

/* lockdown data structures */
#define	SLEEPING	1
#define DUPLICATE	2		/* must not == 0 */

typedef
struct prglck {
	pas_t		*p_pas;		/* this AS... */
	ppas_t		*p_ppas;	/* this AS... */
	preg_t		*p_preg;	/* owns this pregion */
	struct prglck	*p_linkf;
	struct prglck	*p_linkb;
	pgno_t		p_lpn;
	pgcnt_t		p_npgs;
	int		p_flags;	/* here for alignment */
} prglck_t;
	
typedef
struct reglck {
	reg_t		*r_reg;		/* region contains mapping to pages */
        struct reglck	*r_linkf;	/* link to next region */
	struct reglck	*r_linkb;	/* link to previous region */
        pgno_t          r_rpn;		/* starting pn of span */
        pgcnt_t         r_npgs;		/* number of pages in span */
} reglck_t;

lock_t		 locklistlock;
reglck_t	*lockreglist;	/* list of locked regions */
prglck_t	*lockprglist;	/* list of locked pregions */

#if DEBUG	/* saved for debugging */
void
dump_mpin_lockdown(void)
{
	reglck_t	*rp;
	prglck_t	*pp;
	int             s;

	printf("dump-pregions: 0x%x\n",lockreglist);
	s = mutex_spinlock(&locklistlock);
        for (rp = lockreglist; rp; rp = rp->r_linkf) {
	    printf("\tregion 0x%x, flags 0x%x, rpn %d, npgs %d\n",
                    rp->r_reg, rp->r_reg->r_flags, rp->r_rpn, rp->r_npgs);
	}
	printf("\tpregs: 0x%x\n",lockprglist);
	for (pp = lockprglist; pp; pp = pp->p_linkf) {
	    printf("\t\t%c%c[0x%x/0x%x] prp 0x%x, lpn %d, npgs %d\n",
		   pp->p_flags & SLEEPING ? 'S' : ' ',
		   pp->p_flags & DUPLICATE ? 'D' : ' ',
		   pp->p_pas,
		   pp->p_ppas,
		   pp->p_preg,
		   pp->p_lpn,
		   pp->p_npgs);
	}
	mutex_spinunlock(&locklistlock, s);
}
#endif /* DEBUG */

static void
mpin_mprotect(prglck_t *p, mprot_t prot, int flags)
{
	uvaddr_t vaddr = apntov(p->p_preg,p->p_lpn);

	(void)pas_mprotect(p->p_pas,p->p_ppas,vaddr,ctob(p->p_npgs),prot,
			   PAS_MPROTECT_T5WAR | flags);
}

static void
rmlockedregion(reg_t *regp, pgno_t pgn, pgcnt_t npgs)
{
	reglck_t *rp, *rfp, *rbp;
	int count = 0, matched = 0;

	ASSERT(lockreglist);

	for (rp = lockreglist; rp;) {
	    if (rp->r_reg == regp) {
		count++;			/* region used more than 1x? */

		if (rp->r_rpn == pgn && rp->r_npgs == npgs) {
		    matched++;
		    if (matched == 2)		/* only remove 1st one */
			break;

		    /* If this region has another DMA, we will not unlock
		     * it, but we can remove this item as it would be
		     * illegal to have 2 memory writes to the same pages
		     * at the same time.
		     */
		    rmlockedpreg(0,0,0,regp,1);	/* scan lockprglist */

		    rfp = rp->r_linkf;
		    rbp = rp->r_linkb;
		    if (rbp != NULL)
			rbp->r_linkf = rfp;
		    else
			lockreglist = rfp;	/* head of the list */
		    if (rfp != NULL)
			rfp->r_linkb = rbp;
		    kmem_free(rp, sizeof (reglck_t));

		    rp = rfp;			/* next guy */
		    continue;
		}
	    }
	    rp = rp->r_linkf;
	}

	/* This region only shows up once, so unlock it's locked pregs.
	 */
	if (count == 1) {
	    reglock(regp);
	    regp->r_flags &= ~RG_LOCKED;
	    regrele(regp);
	}
}

static void
addlockedregion(reg_t *rp, pgno_t rpn, pgcnt_t npgs)
{
	int		s;
        reglck_t	*xp;

	xp = (reglck_t *)kmem_alloc(sizeof (reglck_t), KM_SLEEP);

	xp->r_reg = rp;
        xp->r_rpn = rpn;
        xp->r_npgs = npgs;
	xp->r_linkb = NULL;

	reglock(rp);		/* lock region till it's on the chain */

	/* If this region is already locked, do not drop it on the list again
	 */
	if (rp->r_flags & RG_LOCKED) {
		regrele(rp);
		kmem_free(xp,sizeof(reglck_t));
		return;
	}

	rp->r_flags |= RG_LOCKED;

 	s = mutex_spinlock(&locklistlock);
        xp->r_linkf = lockreglist;
	if (lockreglist != NULL) lockreglist->r_linkb = xp;
	lockreglist = xp;
	mutex_spinunlock(&locklistlock, s);

	regrele(rp);
}

/* Must hold locklistlock when calling.
 */
static void
addlockedpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, pgno_t lpn, pgcnt_t npgs,
	      int flags)
{
	prglck_t	*rp, *np;
	int		s; 
	int		duplicate = 0;

	for (rp = lockprglist; rp; rp = rp->p_linkf) {
	    /* already in the list? */
	    if (prp == rp->p_preg && lpn == rp->p_lpn) {
		if (pas == rp->p_pas)		/* if same thread, return */
			return;
		duplicate = DUPLICATE;
	    }
	}

	np = kmem_alloc(sizeof(prglck_t), KM_SLEEP);
	np->p_flags = duplicate;
	np->p_pas = pas;
	np->p_ppas = ppas;
	np->p_preg = prp;
	np->p_lpn = lpn;
	np->p_npgs = npgs;
	np->p_linkf = NULL;
	np->p_linkb = NULL;

	if (duplicate == 0) {		/* Only pin the first time */
	    mpin_mprotect(np, PROT_NONE, flags);
	}

	np->p_linkf = lockprglist;
	if (lockprglist != NULL) lockprglist->p_linkb = np;
	lockprglist = np;
}

/* Must hold mrupdate(pas->pas_aspacelock), and reglock(prp->p_reg).
 */
void
duplockedpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, preg_t *srcprp)
{
	prglck_t	*rp;
	int		s; 

	s = mutex_spinlock(&locklistlock);
	for (rp = lockprglist; rp; rp = rp->p_linkf) {
	    /*  If found srcprp, add new copy to the list.  Look for all
	     * copies as different lpn/npgs may be locked.
	     */
	    if (rp->p_preg == srcprp) {
		addlockedpreg(pas, ppas, prp, rp->p_lpn, rp->p_npgs,
			PAS_MPROTECT_ASPACE|PAS_MPROTECT_REGION);
	    }
	}
	mutex_spinunlock(&locklistlock, s);
}

/* Must hold mrupdate(pas->pas_aspacelock), and reglock(prp->p_reg).
 */
void
fixlockedpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, int val)
{
	prglck_t	*rp;
	int		s; 

	s = mutex_spinlock(&locklistlock);
	for (rp = lockprglist; rp; rp = rp->p_linkf) {
	    if (rp->p_pas == pas && rp->p_ppas && rp->p_preg == prp) {
		mpin_mprotect(rp, val ? PROT_READ|PROT_WRITE : PROT_NONE,
			PAS_MPROTECT_ASPACE|PAS_MPROTECT_REGION);
	    }
	}
	mutex_spinunlock(&locklistlock, s);
}

/*
 * Get rid of a preg reference.  Called from vdetachreg(), mpin_unlockdown(),
 * and rmlockedregion().
 */
void
rmlockedpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, reg_t *regp, int locked)
{
	prglck_t	*fp, *bp, *pp;
	int		s; 

	if (locked == 0)
	    s = mutex_spinlock(&locklistlock);
	pp = lockprglist;
	while (pp) {
	    if ((regp && (regp == pp->p_preg->p_reg)) ||
		(pp->p_pas == pas && pp->p_preg == prp) && pp->p_ppas == ppas) {
		fp = pp->p_linkf;
		bp = pp->p_linkb;
		if (bp == NULL) {	/* head of the list */
		    if (fp == NULL) {	/* only thing on list */
			lockprglist = NULL;
			kmem_free(pp, sizeof (prglck_t));
			pp = NULL;
			break;		/* bail early */
		    } else {		/* more list */
			lockprglist = fp;
			fp->p_linkb = NULL;
			kmem_free(pp, sizeof (prglck_t));
			pp = lockprglist;
		    }
		} else {	/* inside list */
		    bp->p_linkf = fp;
		    if (fp != NULL) fp->p_linkb = bp;
		    kmem_free(pp, sizeof (prglck_t));
		    pp = fp;
		}
	    } else
		pp = pp->p_linkf;
	}
	if (locked == 0)
		mutex_spinunlock(&locklistlock, s);
}

/*
 * called from interrupt level (vfault).  Hence, the spinlocks. We're on a
 * UP, so we could just go ahead and directly spl() here, but spl()s are evil.
 *
 * Called holding aspacelock for pas.  If we sleep, we have to give the
 * lock up so we can be woken up.
 */
int
mpin_sleep_if_locked(pas_t *pas, ppas_t *ppas, preg_t *prp, caddr_t vaddr)
{
	int		s,t,rc;
	prglck_t	*pp;
	pgno_t		lpn = vtoapn(prp, vaddr);

	t = splhi();				/* avoid pre-emption b4 sleep */
	s = mutex_spinlock(&locklistlock);

	/* set proc's sleeping flag */
	for (pp = lockprglist; pp; pp = pp->p_linkf) {
	    if (pp->p_pas == pas && pp->p_lpn <= lpn &&
		pp->p_lpn + pp->p_npgs > lpn) {
		pp->p_flags |= SLEEPING;
		mrunlock(&pas->pas_aspacelock);
		mutex_spinunlock(&locklistlock, s);
		rc = sleep(pp, PZERO);
		splx(t);
		return 1 + rc;			/* return 2 if interrupted */
	    }
	}
	mutex_spinunlock(&locklistlock, s);
	splx(t);

	return 0;
}

struct pinscan {
	pas_t  *origpas;
	preg_t *targprp;
	uvaddr_t vaddr;
	pgcnt_t npgs;
};

int 
pinpagescanner(vasid_t vasid, void *vpinscan)
{
	struct pinscan *pinscan = (struct pinscan *)vpinscan;
	preg_t *targprp = pinscan->targprp;
	uvaddr_t vaddr = pinscan->vaddr;
	pgcnt_t npgs = pinscan->npgs;
	preg_t *prp;
	ppas_t *ppas;
	pas_t *pas;
	int s;

	pas = VASID_TO_PAS(vasid);

	/* We have locked the originator already.
	 */
	if (pas == pinscan->origpas)
		return 0;

	/*  Try and access the aspacelock, if we need it later for
	 * update we will try and upgrade it.  There is a race here
	 * with the the other aspace users that aquire the aspace
	 * lock, then aquire the addr as_scan_lock.
	 */
	if (!mrtryaccess(&pas->pas_aspacelock)) {
#if DEBUG && verbose
	    cmn_err(CE_CONT,
		"pinpagescanner: cannot access aspacelock pas 0x%x\n", pas);
#endif
	    return EBUSY;
	}

	mutex_lock(&pas->pas_preglock,0); /* Should get as have aspacelock */

	/* scan shared pregions */
	for (prp = PREG_FIRST(pas->pas_pregions); prp; prp = PREG_NEXT(prp)) {
	    if (prp->p_reg == targprp->p_reg) {
		ASSERT(prp->p_regva == targprp->p_regva);
		if (mrtrypromote(&pas->pas_aspacelock)) {
		    s = mutex_spinlock(&locklistlock);
		    addlockedpreg(pas, (ppas_t *)vasid.vas_pasid,prp,
				  vtoapn(prp, vaddr),npgs,PAS_MPROTECT_ASPACE);
		    mutex_spinunlock(&locklistlock,s);
		    mrdemote(&pas->pas_aspacelock);
		    break;		/* reg only 1x per pregion */
		}
		else {
		    mutex_unlock(&pas->pas_preglock);
		    mrunlock(&pas->pas_aspacelock);
#if DEBUG
		    cmn_err(CE_CONT,
			"pinpagescanner: "
			"cannot upgrade aspacelock pas 0x%x (shared)\n",
			pas);
#endif
		    return EAGAIN;
		}
	    }
	}
	mutex_unlock(&pas->pas_preglock);

	/* scan all private pregions */
	if (!mrtryaccess(&pas->pas_plistlock)) {
	    mrunlock(&pas->pas_aspacelock);
#if DEBUG && verbose
	    cmn_err(CE_CONT,
		"pinpagescanner: cannot access listlock pas 0x%x\n", pas);
#endif
	    return EBUSY;
	}

	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
	    if (ppas->ppas_refcnt == 0)
		continue;
	    for (prp=PREG_FIRST(ppas->ppas_pregions); prp; prp=PREG_NEXT(prp)) {
		if (prp->p_reg == targprp->p_reg) {
		    ASSERT(prp->p_regva == targprp->p_regva);
		    if (mrtrypromote(&pas->pas_aspacelock)) {
			s = mutex_spinlock(&locklistlock);
			addlockedpreg(pas,ppas,prp,vtoapn(prp,vaddr),
				      npgs,PAS_MPROTECT_ASPACE);
			mutex_spinunlock(&locklistlock,s);
			mrdemote(&pas->pas_aspacelock);
		    }
		    else {
			mrunlock(&pas->pas_aspacelock);
#if DEBUG
			cmn_err(CE_CONT,
				"pinpagescanner: "
				"cannot upgrade aspacelock pas 0x%x\n",pas);
#endif
			return EAGAIN;
		    }
		}
	    }
	}

	mrunlock(&pas->pas_plistlock);
	mrunlock(&pas->pas_aspacelock);

	return 0;
}

/*
 * add this region to our list, and go find everybody else whose
 * pregion also owns it, and trash their ptes.  Needs to be called
 * w/o pas->aspacelock as pinpagescanner() will re-aquire it.
 */
static void
pin_pages(pas_t *pas, preg_t *prp, caddr_t vaddr, pgcnt_t npgs)
{
	struct pinscan scanarg;
	reglck_t *rl;
	int error;

	addlockedregion(prp->p_reg, vtorpn(prp, vaddr), npgs);

	/* scan all ASs (including ourselves) and add to list */
	scanarg.targprp = prp;
	scanarg.vaddr = vaddr;
	scanarg.npgs = npgs;
	scanarg.origpas = pas;
	(void)as_scan((as_scanop_t)99, 0, (as_scan_t *)&scanarg);
	/* XXX handle EAGAIN somehow? */
}

void
mpin_unlockdown(void *arg, caddr_t vaddr, size_t length)
{
	preg_t	*prp = (preg_t *)arg;
	caddr_t addr;
	pas_t	*pas;
	ppas_t	*ppas;
	off_t	len, size;
        caddr_t	pregend;
	pgno_t	lpn;
        reglck_t	*rp;
	prglck_t	*pp, *ap;
        int		s;
#if DEBUG
	uint tlbtmp;
#endif

	/* Single threaded fast path. */
	if (prp == (preg_t *)1L)
		return;

	addr = (caddr_t)pbase(vaddr);
	len = ctob(numpages(vaddr,length));
	lpn = vtoapn(prp, addr);

        s = mutex_spinlock(&locklistlock);

        if (lockreglist == NULL) {
		mutex_spinunlock(&locklistlock,s);
		return;
	}

#if DEBUG
	if (tlb_probe(tlbgetpid(),addr,&tlbtmp) != -1) {
		cmn_err(CE_WARN,"mpin_unlockdown: vaddr 0x%x mapped!\n",addr);
	}
#endif

	while (len > 0) {
	    for (pp = lockprglist; pp; pp = pp->p_linkf) {
		if (prp == pp->p_preg && lpn == pp->p_lpn) {
		    pas = pp->p_pas;
		    ppas = pp->p_ppas;
		    mrupdate(&pas->pas_aspacelock);
		    if ((pp->p_flags & DUPLICATE) == 0) {
			mpin_mprotect(pp, PROT_READ|PROT_WRITE,
				PAS_MPROTECT_ASPACE);
		    }
		    if (pp->p_flags & SLEEPING) {
			/* should be ok to wakeup vfault holding locklistlock */
			wakeup(pp);
		    }

		    rmlockedpreg(pas,pp->p_ppas,prp,0,1);
		    /* pp is now stale, but ok as flag below */

		    pregend = prp->p_regva + ctob(prp->p_pglen);
		    if (addr + len > pregend)
			size = pregend - addr;
		    else
			size = len;

		    rmlockedregion(prp->p_reg, vtorpn(prp, addr), btoc(size));

		    addr += size;
		    len  -= size;

		    /* If we are not done, find next preg_t past cookie.
		     */
		    if (len > 0) {
			prp = findfpreg(pas, ppas, addr, addr+len);
			if (prp == NULL)
			    len = 0;	/* done for better or worse */
			else
			    lpn = vtoapn(prp, addr);
		    }

		    mrunlock(&pas->pas_aspacelock);

		    break;		/* cannot have more matches in prp */
		}
	    }
	    /* prp+lpn match not found, try to unlock just via the region.
	     */
	    if (pp == 0) {
		rmlockedregion(prp->p_reg, vtorpn(prp, addr), btoc(len));
		break;			/* done with loop */
	    }
	}

        mutex_spinunlock(&locklistlock, s);
}

void *
mpin_lockdown(caddr_t vaddr, size_t length)
{
        register preg_t	*prp;
        register reg_t	*rp;
        caddr_t		pregend;
        caddr_t		addr;
	off_t		len;
	size_t		size;
	vasid_t		vasid;
	pas_t		*pas;
	ppas_t		*ppas;
	int		s,found;
#if DEBUG
	uint tlbtmp;
#endif

	/* page align the address, + get length */
	addr = (caddr_t)pbase(vaddr);
	len = ctob(numpages(vaddr,length));

	/*
	 * handle requests that span regions
	 */
	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

	mrupdate(&pas->pas_aspacelock);

	/* If address space is not shared, callers of mpin_lockdown
	 * sleep, so do not bother getting fancy, just unmap the
	 * request from the tlb to avoid idle speculation.
	 */
	if ((pas->pas_flags & (PAS_SHARED|PAS_PSHARED)) == 0) {
		newptes(pas,ppas,0);
		mrunlock(&pas->pas_aspacelock);
		return (void *)1L;
	}

	while (len > 0) {
		prp = findfpreg(pas, ppas, addr, addr + len);
		if (prp == NULL)
			break;

		rp = prp->p_reg;
		pregend = prp->p_regva + ctob(prp->p_pglen);

		if (addr + len > pregend)
			size = pregend - addr;
		else
			size = len;
		/*
		 * the region has to be PT_MAPFILE and not a PHYS mapping,
		 * or it can be ANON or SANON.
		 */
		reglock_rd(rp);
		found = (prp->p_type == PT_MAPFILE && !(rp->r_flags&RG_PHYS)) ||
			(rp->r_flags & RG_ANON) || (rp->r_flags & RG_HASSANON);
		regrele(rp);
		if (found) {
		    /* Lock down the one we have ASAP, then search for
		     * others.  We will not get a duplicate as we hold
		     * the lock for update (will return EAGAIN).
		     */
		    s = mutex_spinlock(&locklistlock);
		    addlockedpreg(pas,ppas,prp,vtoapn(prp,addr),
			 	  btoc(size),PAS_MPROTECT_ASPACE);
		    mutex_spinunlock(&locklistlock,s);
		    pin_pages(pas, prp, addr, btoc(size));
#if DEBUG
		    if (tlb_probe(tlbgetpid(),addr,&tlbtmp) != -1) {
			cmn_err(CE_WARN,
				"mpin_lockdown: vaddr 0x%x still valid! "
				"pas = 0x%x\n",
				addr,pas);
		    }
#endif
		}
		addr += size;
		len -= size;
	}
	mrunlock(&pas->pas_aspacelock);

        return prp;
}
#endif	/* R10000_SPECULATION_WAR */
