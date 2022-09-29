/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#ident	"$Revision: 1.31 $"

#include <sys/types.h>
#include <sys/anon.h>
#include <ksys/as.h>
#include "as_private.h"
#include <sys/atomic_ops.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/immu.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/numa.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include "pmap.h"
#include "region.h"
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/uthread.h>
#include <sys/vnode.h>
#include <ksys/vproc.h>
#include <sys/tile.h>
#include <sys/buf.h>
static void unlockmem(pas_t *, preg_t *, caddr_t, pgcnt_t);
static int lock_by_attr(pas_t *pas, ppas_t *ppas, int prot, int which);
static int lockallpages(pas_t *pas, ppas_t *ppas, int mode);
static int pas_shmlock(pas_t *, ppas_t *, int, as_mohandle_t);

/*
 * Subroutines to help with locking pages
 */
int
pas_memlock(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t vaddr,		/* address */
	size_t len,		/* length */
	as_setrangeattr_t *attr)/* info about what to set */
{
	int error = 0, error2 = 0;
	pgcnt_t npgs;

#if DEBUG
	switch (attr->as_op) {
	case AS_UNLOCK_BY_ATTR:
	case AS_UNLOCKALL:
	case AS_UNLOCK:
		ASSERT(mrislocked_update(&pas->pas_aspacelock));
		break;
	default:
		ASSERT(mrislocked_access(&pas->pas_aspacelock));
		break;
	}
#endif

	switch (attr->as_op) {
	case AS_LOCK_BY_ATTR:
		/*
		 * silly plock(2) semantics
		 */
		if ((attr->as_lock_attr & AS_PROCLOCK) &&
			(pas->pas_lockdown & (AS_PROCLOCK|AS_TEXTLOCK|AS_DATALOCK))) {
			error = EINVAL;
			break;
		}
		if ((attr->as_lock_attr & AS_TEXTLOCK) &&
				pas->pas_lockdown & (AS_TEXTLOCK|AS_PROCLOCK)) {
			error = EINVAL;
			break;
		}
		if ((attr->as_lock_attr & AS_DATALOCK) &&
				pas->pas_lockdown & (AS_DATALOCK|AS_PROCLOCK)) {
			error = EINVAL;
			break;
		}

		if (attr->as_lock_attr & AS_TEXTLOCK) {
			error = lock_by_attr(pas, ppas, PROC_TEXT, PGLOCK);
			if (error) {
				/* unlock everything */
				ASSERT(mrislocked_update(&pas->pas_aspacelock));
				(void) lock_by_attr(pas, ppas, PROC_TEXT, UNLOCK);
				/* stop everything now */
				break;
			} else {
				atomicSetUint(&pas->pas_lockdown, AS_TEXTLOCK);
			}
		}
		if (attr->as_lock_attr & AS_DATALOCK) {
			mutex_lock(&pas->pas_brklock, 0);
			error = lock_by_attr(pas, ppas, PROC_DATA, PGLOCK);
			if (error) {
				/* unlock everything */
				ASSERT(mrislocked_update(&pas->pas_aspacelock));
				(void) lock_by_attr(pas, ppas, PROC_DATA,
								UNLOCK);
				mutex_unlock(&pas->pas_brklock);
				/* unlock text if we locked it */
				if (attr->as_lock_attr & AS_TEXTLOCK) {
					ASSERT(pas->pas_lockdown & AS_TEXTLOCK);
					(void) lock_by_attr(pas, ppas,
							PROC_TEXT, UNLOCK);
					atomicClearUint(&pas->pas_lockdown,
								AS_TEXTLOCK);
				}
				break;
			} else {
				mutex_unlock(&pas->pas_brklock);
				atomicSetUint(&pas->pas_lockdown, AS_DATALOCK);
			}
		}
		if (attr->as_lock_attr & AS_FUTURELOCK)
			atomicSetUint(&pas->pas_lockdown, AS_FUTURELOCK);

		if (attr->as_lock_attr & AS_SHMLOCK)
			error = pas_shmlock(pas, ppas, PGLOCK,
							attr->as_lock_handle);
		break;
	case AS_UNLOCK_BY_ATTR:
		if ((attr->as_lock_attr & AS_PROCLOCK) &&
			((pas->pas_lockdown & (AS_PROCLOCK|AS_TEXTLOCK|AS_DATALOCK)) == 0)) {
			error = EINVAL;
			break;
		}
		/*
		 * no matter what, we let them call plock(PROCLOCK) again
		 * by turning off the PROCLOCK bit
		 */
		if (attr->as_lock_attr & AS_PROCLOCK) {
			atomicClearUint(&pas->pas_lockdown, AS_PROCLOCK);
		}
		if ((attr->as_lock_attr & AS_DATALOCK) &&
				(pas->pas_lockdown & AS_DATALOCK)) {
			mutex_lock(&pas->pas_brklock, 0);
			error = lock_by_attr(pas, ppas, PROC_DATA, UNLOCK);
			mutex_unlock(&pas->pas_brklock);
			if (!error)
				atomicClearUint(&pas->pas_lockdown, AS_DATALOCK);
		}

		if ((attr->as_lock_attr & AS_TEXTLOCK) &&
				(pas->pas_lockdown & AS_TEXTLOCK)) {
			error2 = lock_by_attr(pas, ppas, PROC_TEXT, UNLOCK);
			if (!error2)
				atomicClearUint(&pas->pas_lockdown, AS_TEXTLOCK);
		}

		if (attr->as_lock_attr & AS_FUTURELOCK) {
			atomicClearUint(&pas->pas_lockdown, AS_FUTURELOCK);
			break;
		}

		if (error == 0)
			error = error2;

		/* SHMLOCK always called alone */
		if (attr->as_lock_attr & AS_SHMLOCK)
			error = pas_shmlock(pas, ppas, UNLOCK,
							attr->as_lock_handle);
		break;
	case AS_LOCK:
		npgs = numpages(vaddr, len);
		vaddr = (char *)pbase(vaddr);
		error = lockpages2(pas, ppas, vaddr, npgs, LPF_NONE);
		break;
	case AS_LOCKALL:
		error = lockallpages(pas, ppas, PGLOCKALL);
		break;
	case AS_UNLOCK:
		if (len == 0)
			break;
		npgs = numpages(vaddr, len);
		vaddr = (char *)pbase(vaddr);
		error = unlockpages(pas, ppas, vaddr, npgs, 0);
		break;
	case AS_UNLOCKALL:
		/*
		 * this is for munlock and munlockall - POSIX states
		 * that munlock removes ALL locks regardless of how
		 * many time mlock is called - this is different than
		 * mpin
		 */
		if (len) {
			npgs = numpages(vaddr, len);
			vaddr = (char *)pbase(vaddr);
			error = unlockpages(pas, ppas, vaddr, npgs, RF_EXPUNGE);
		} else {
			atomicClearUint(&pas->pas_lockdown, AS_FUTURELOCK);
			error = lockallpages(pas, ppas, UNLOCKALL);
		}
			
		break;
	default:
		panic("pas_lock");
		/* NOTREACHED */
	}
	return error;
}

/*
 * Lock or unlock all pages whose permissions match those given in "prot".
 *
 * XXX It would be nice if this could be merged with lockpages since some
 * XXX of the code is redundant, but the locking problems with regions
 * XXX and attr structs (and sproc) look pretty messy.
 */
static int
lock_by_attr(pas_t *pas, ppas_t *ppas, int prot, int which)
{
	preg_t	*prp;
	reg_t	*rp;
	attr_t	*attr;
	caddr_t	vaddr, attr_end, prp_end;
	pgcnt_t	npgs;
	int	doingshd = 0;
	int	error;

	prp = PREG_FIRST(pas->pas_pregions);

doshd:
	for ( ; prp ; prp = PREG_NEXT(prp)) {
		rp = prp->p_reg;

		/*
		 * plock can only lock private regions.  It's not supposed
		 * to touch regions created via mmap, even if the attrs
		 * match.  All private regions have an anonymous handle or
		 * the RG_TEXT flag set.
		 */

		if (prp->p_type == PT_SHMEM ||
		   (rp->r_anon == NULL && (rp->r_flags & RG_TEXT) == 0))
			continue;

		reglock(rp);

		/*
		 * Go through each attr struct and lock or unlock the address
		 * ranges that match the given protection.  Normally there is
		 * only one attr struct in a pregion, so we're usually only
		 * calling lockpages once per pregion.
		 */

		vaddr = prp->p_regva;
		prp_end = vaddr + ctob(prp->p_pglen);

		while (vaddr < prp_end) {
			attr = findattr(prp, vaddr);
			attr_end = attr->attr_end;

			if (attr->attr_prot == prot) {
				regrele(rp);
				npgs = btoc(attr_end - vaddr);

				if (which == PGLOCK)
					error = lockpages2(pas, ppas, vaddr, npgs, LPF_NONE);
				else
					error = unlockpages(pas, ppas, vaddr, npgs, 1);
				if (error)
					return error;

				reglock(rp);
			}

			vaddr = attr_end;
		}

		regrele(rp);
	}

	if (!doingshd) {
		doingshd++;
		prp = PREG_FIRST(ppas->ppas_pregions);
		goto doshd;
	}

	return 0;
}

/*
 * Locks OR unlocks all pages currently mapped into the calling processes VAS.
 * 
 * The mode flag is used to select one of the following operation modes: 
 * PGLOCKALL or UNLOCKALL.
 * 
 * Locking: The address space lock and region locks must not be held upon entry!
 * Returns: 0 if successful, non-zero on error.
 */
static int
lockallpages(pas_t *pas, ppas_t *ppas, int mode)
{
	uvaddr_t vaddr;
	pgcnt_t	npgs;
	int	error;
	preg_t  *prp;
	int	doingshd = 0;
	reg_t	*rp;

	prp = PREG_FIRST(pas->pas_pregions);

doshreg:

	for ( ; prp ; prp = PREG_NEXT(prp)) {
		rp = prp->p_reg;

		if (rp->r_flags & RG_PHYS)
			continue;

		vaddr = prp->p_regva;

		/* If the region is marked for autogrow, then only
		 * modify the lock state of the pages mapped and
 		 * through EOF.
		 */
		if (rp->r_flags & RG_AUTOGROW) {
		  	struct vattr va;
			va.va_mask = AT_SIZE;
			/*
			 * if its mapped we certainly have the right to
			 * 'stat' it. Note that using p_cred here is probably
			 * wrong - process may no longer have creds to
			 * access file - the best case would be to use
			 * the creds when the file was mapped.
			 */
			ASSERT(rp->r_vnode);
			VOP_GETATTR(rp->r_vnode, &va, 0, sys_cred, error);
			if (error)
				return EAGAIN;
			npgs = MIN(prp->p_pglen, btoc(va.va_size));
		} else
			npgs = prp->p_pglen;

		if (mode == PGLOCKALL)
			error = lockpages2(pas, ppas, vaddr, npgs, LPF_NONE);
		else 
			error = unlockpages(pas, ppas, vaddr, npgs, RF_EXPUNGE);
		if (error)
			return error;
	}

	if (!doingshd) {
		doingshd++;
		prp = PREG_FIRST(ppas->ppas_pregions);
		goto doshreg;
	}

	return 0;
}

/*
 * lockpages, lockpages2
 *	Lock a group of pages.
 *	Region must not be locked upon entry!
 *	aspacelock must be locked for access
 * Returns: non-zero on error
 *	    0 if ok
 * N.B. - upon error return, aspacelock is locked for UPDATE
 *
 * NOTE: a parameter was added to lockpages but because of
 * uncertainty about whether lockpages was an external interface, a
 * new interface (lockpages2) was created. All kernel functions should
 * use the new interface. The old interface should be deleted someday.
 *	
 */
int
lockpages(pas_t *pas, ppas_t *ppas, caddr_t vaddr, pgcnt_t npgs)
{
	return lockpages2(pas, ppas, vaddr, npgs, LPF_NONE);
}

int
lockpages2(pas_t *pas, ppas_t *ppas, caddr_t vaddr, pgcnt_t npgs, int lpflags)
{
	int x;
	preg_t *prp;
	reg_t *rp;
	auto pgno_t sz;
	pgcnt_t *lockcntr;
	pgcnt_t totlkd = 0;
	uvaddr_t base_addr = vaddr;
	attr_t *attr;
	int error, limited;
	as_fault_t as;
	as_faultres_t asres;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	if (!IS_KUSEG(vaddr) || npgs < 0)
		return EINVAL;

	if (npgs == 0)
		return(0);

	limited = !_CAP_CRABLE(get_current_cred(), CAP_MEMORY_MGT);
	lockcntr = &pas->pas_nlockpg;

	do {
		pde_t *pt;
		int smem;

		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
			error = EINVAL;
			goto out;
		}
		rp = prp->p_reg;

		/* If availsmem wasn't allocated when
		 * the region was created, must allocate
		 * it here to avoid swap space deadlock.
		 */
		if (rp->r_flags & RG_PHYS) {
			error = EINVAL;
			goto out;
		}

		smem = rp->r_anon ? 0 : 1;

		sz = MIN(npgs, pregpgs(prp, vaddr));
		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
		ASSERT(sz);

		reglock(rp);

		for ( ; sz > 0; vaddr += NBPP, pt++, npgs--, 
				sz --, totlkd++) {
			register pfd_t	*pfd;
			int writeable;

			/*
			 * If pages in the range are already locked, just increment the
			 * lock count.
			 */

			/*
			 * Performance fastpath:
			 * If the caller is vfault, there is a good
			 * chance that a substantial portion of the range of pages
			 * is already locked. In this case, nothing is actually going 
			 * to be done. Without this "fastpath", we end up calling
			 * findattr_range which will split an attr so
			 * the the returned attr covers exactly 1 page. This results in
			 * a lot of overhead (kmemalloc/free, looping, etc). 
			 *
			 * This fastpath calls findattr which does no spliting. We are
			 * able to check a large block of pages at a time. As long as the
			 * block of pages is locked, we can skip over the entire block.
			 */
			if (lpflags&LPF_VFAULT) {
				attr = findattr(prp, vaddr);
				if (attr->attr_lockcnt > 0) {
					pgcnt_t		pgs;		
					pgs = btoc(attr->attr_end - vaddr) - 1;
					pgs = min(pgs, sz-1);
					vaddr += ctob(pgs);
					pt += pgs;
					npgs -= pgs;
					sz -= pgs;
					totlkd += pgs;
					continue;
				}
			}

			/*
			 * Unfortunately, locking problems make us split
			 * the attributes down to the page level.  The main
			 * problem being that we have to release the region
			 * lock to fault in the pages which means that the
			 * attrs could have changed by the time we relock it.
			 */
			attr = findattr_range(prp, vaddr, vaddr + NBPP);

			/*
			 * If this page wasn't already locked by
			 * this process, fault page in and bump per-process 
			 * lock count in pfdat.
			 */
			if (attr->attr_lockcnt > 0) {
				if ((lpflags&LPF_VFAULT) == 0)
					attr->attr_lockcnt++;
				continue;
			}

			writeable = attr->attr_prot & PROT_W;

			if (pg_ishrdvalid(pt)) {
				if (!writeable || pg_ismod(pt))
					goto dolock;
			}

			/*
			 * Check for a performance fast path for
			 * locking down large shmem regions.  If the
			 * region is writable but not copy-on-write and 
			 * doesn't have a watchpoint, then, as long
			 * as the page is already hardware valid,
			 * there's nothing for pfault to do but set the
			 * mod bit and clear any swap info.  So if this
			 * is true, then just do the work here and
			 * save all the trap overhead.  Note that shmem
			 * regions can become RG_CW due to pattaching.
			 * We don't have to rehash any pages here
			 * since shmem only has a 1 level anon tree.
			 */
			/*
			 * Skip the fast path if the page size is
			 * large.
			 */

			if (prp->p_type == PT_SHMEM && writeable && 
			    attr->attr_watchpt == 0 &&
			   (rp->r_flags & RG_CW) == 0 &&
			   (PMAT_GET_PAGESIZE(attr) == NBPP) &&
			    pg_ishrdvalid(pt) && pg_ismod(pt) == 0) {
				pfd_t* pfd = pdetopfdat_hold(pt);
				anon_modify(rp->r_anon, pfd, 1);
				pfdat_release(pfd);
				pg_setmod(pt);
			} else {
				regrele(rp);
				
faultinloop:

				/* The PF_LCKNPROG bit prevents pas_vfault
				 * from recursively calling lockpages with
				 * this region.
				 */
				prp->p_flags |= PF_LCKNPROG;

				/*
				 * First time locked, make sure 
				 * faulted in and really
				 * locked down.
				 */

				/* need to made double word references for fetchop */
				if (pg_isfetchop(pt))
					x = fulong(vaddr);
				else
					x = fubyte(vaddr);


				if (x < 0) {
					prp->p_flags &= ~PF_LCKNPROG;
					error = EINVAL;
					goto out;
				}


				/* Check that page(s) is/are writeable. */
				if (writeable) {
					as.as_uvaddr = vaddr;
					as.as_ut = NULL;
					as.as_rw = W_WRITE;
					as.as_epc = AS_ADD_UNKVADDR;
					as.as_flags = ASF_NOEXC;
					if (pas_pfault(pas, ppas, &as, &asres))
					{
						error = asres.as_code;
						ASSERT(error);
						prp->p_flags &= ~PF_LCKNPROG;
						goto out;
					}
				}


				prp->p_flags &= ~PF_LCKNPROG;

				/*
				 * Check the lock count again.  Some other
				 * process in our share group could have 
				 * already locked the page while we had the
				 * region unlocked.  If so, just go on to the
				 * next page.  Have to call findattr again since
				 * everything could have changed while the
				 * region was unlocked.
				 */
				reglock(rp);
				attr = findattr_range(prp, vaddr, vaddr + NBPP);

				if (attr->attr_lockcnt > 0) {
					attr->attr_lockcnt++;
					continue;
				}

				if (!(pg_isvalid(pt))) {
					/*
					 * Are we not being able to validate
					 * pte because it is a watched page?
					 */
					if (attr->attr_watchpt) {
						if (intlb(curuthread, vaddr, 
						  writeable ? B_READ : B_WRITE))
							goto done;
					}
					/*
					 * Sigh.  Page got tossed before we
					 * could lock it.  Try it again.
					 */
					regrele(rp);
					goto faultinloop;
				}
done:;

#ifdef MH_R10000_SPECULATION_WAR
				pfd = pdetopfdat(pt);
                                if (pfd < pfd_lowmem) {
                                        regrele(rp);
                                        if (error = 
						page_migrate_lowmem_page(
							pfd,NULL, VM_DMA_RW)) {
                                                if (error == ENOMEM) {
                                                        setsxbrk_class(VM_DMA_RW);
                                                        goto faultinloop;
                                                } else {
                                                        error = ENOMEM;
                                                        goto out;
                                                }
                                        }
                                        goto faultinloop;
                                }
#endif /* MH_R10000_SPECULATION_WAR */
			}

dolock:
			/*
			 * root can mpin as much as it wants --
			 * others have a tunable per-process limit.
			 * No one can pin more than their RSS.cur value.
			 * This still gives them the RSSHOGSLOP pages
			 * to work around in.
			 */
			if ((limited && *lockcntr >= tune.t_maxlkmem) ||
					(*lockcntr >= pas->pas_maxrss)) {
				regrele(rp);
				error = ENOMEM;
				goto out;
			}

			pfd = pdetopfdat_hold(pt);
			if (pfd == NULL) {
				regrele(rp);
				goto faultinloop;
			}

			/*
			 * Mark page locked, doing any required
			 * r/smem accounting.
			 */ 
			error = pas_pagelock(pas, pfd, smem, 1);
			pfdat_release(pfd);
			if (error) {
				regrele(rp);
				goto out;
			}
			
			/* successfully locked a page */
			atomicAddInt(lockcntr, 1);

			attr->attr_lockcnt++;
		}

		regrele(rp);
	} while (npgs);

	return(0);

out:
	/* error - unlock any already locked pages */

	/*
	 * Unlock the lock and get it back in update mode
	 * Note that as soon as we released the region lock
	 * that someone in fast_useracc could have set p_noshrink
	 * based on our having set some lockcnt's. That means
	 * that the unlockpages might fail and leave locked
	 * pages....
	 */
	mrunlock(&pas->pas_aspacelock);
	mrupdate(&pas->pas_aspacelock);
	if (unlockpages(pas, ppas, base_addr, totlkd, 0) == EBUSY)
		cmn_err(CE_WARN, "Pid %d potentially orphaned locked pages\n",
			current_pid());
	return(error);
}

/*
 * unlockpages - Unlock a group of (possibly) locked pages.  Page may
 * span multiple regions if they are contiguous.
 * aspacelock must be locked for update
 * WARNING - this routine has side-effect if called with 0 pages then it
 * will still make check and increment availrmem...
 *
 * Note - to support fast useracc/unuseracc, we must prohibit the unlocking
 * of pages if there is DMA in progress. This violates POSIX which states
 * that there are no error returns from munlock/all - oh well..
 * This means that this routine shouldn't be called for serious detaches
 * (like process exit)
 */
int
unlockpages(pas_t *pas, ppas_t *ppas, caddr_t vaddr, pgcnt_t npgs, int flags)
{
	int error = 0;
	reg_t *rp;
	caddr_t	tmp_vaddr;
	pgcnt_t	tmp_npgs;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	/*
	 * We have to fail the unlockpages if dma is going on to
	 * any of the regions. We return EBUSY in this case.
	 * To ensure that regions do not change underneath us, we have to
	 * hold the lock in update mode.
	 * We do this check as part of the performance optimization in
	 * useracc/unuseracc where if the page is mpinned, useracc just 
	 * increments a dma count in the region structure.
	 */
	tmp_vaddr = vaddr;
	tmp_npgs = npgs;

	do {
		register preg_t	*prp;
		register pgcnt_t sz;

		prp = findpreg(pas, ppas, tmp_vaddr);
		if (prp == NULL) {
			/* address not in process space */
			return EINVAL;
		}

		sz = MIN(tmp_npgs, pregpgs(prp, tmp_vaddr));
		rp = prp->p_reg;
		reglock(rp);

		if (prp->p_fastuseracc > 0) {
			regrele(rp);
			return EBUSY;
		}

		regrele(rp);
		tmp_npgs -= sz;
		tmp_vaddr += ctob(sz);
	} while (tmp_npgs);

	do {
		preg_t	*prp;
		pgcnt_t sz;

		prp = findpreg(pas, ppas, vaddr);
		if (prp == NULL) {
			/* address not in process space */
			error = EINVAL;
			break;
		}

		sz = MIN(npgs, pregpgs(prp, vaddr));
		reglock(prp->p_reg);
		unlockpreg(pas, ppas, prp, vaddr, sz, flags);
		regrele(prp->p_reg);
		vaddr += ctob(sz);
		npgs -= sz;
	} while (npgs);

	return error;
}

/*
 * unlockpreg - unlock the specified pages from the given pregion which
 * must fully contained within the pregion.  Must be called with the region
 * locked.
 * 'RF_EXPUNGE' means to zero out the lock count. This is used when
 * the process exits or in the munlock/munlockall functions.
 */
void
unlockpreg(
	pas_t *pas,
	ppas_t *ppas,
	preg_t *prp,
	caddr_t vaddr,
	pgcnt_t npgs,
	int flags)
{
	caddr_t end;
	attr_t *attr;
	pgcnt_t *lockcntr;
	int klockprda = 0;

	ASSERT(mrislocked_update(&prp->p_reg->r_lock));
	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	ASSERT(vaddr >= prp->p_regva);
	ASSERT(prp->p_fastuseracc == 0);

	ASSERT(npgs <= pregpgs(prp, vaddr));
	if (prp->p_type == PT_PRDA) {
		if ((prp->p_flags & PF_PRIVATE) &&
				(ppas->ppas_flags & PPAS_LOCKPRDA))
			klockprda = 1;
		else if (!(prp->p_flags & PF_PRIVATE) &&
				(pas->pas_flags & PAS_LOCKPRDA))
			klockprda = 1;
	}

	lockcntr = &pas->pas_nlockpg;

	end = vaddr + ctob(npgs);
	attr = findattr_range(prp, vaddr, end);

	for ( ; ; ) {
		pgcnt_t sz;

		ASSERT(attr);

		if (attr->attr_lockcnt > 0) {
			/*
			 * if kernel has locked PRDA don't let lockcnt
			 * go less than 1
			 */
			if (klockprda && attr->attr_lockcnt == 1)
				goto skip;
			if (flags & RF_EXPUNGE)
				attr->attr_lockcnt = 0;
			else
				attr->attr_lockcnt--;

			if (attr->attr_lockcnt == 0) {
				sz = btoc(attr->attr_end - attr->attr_start);

				unlockmem(pas, prp, attr->attr_start, sz);    
				sz = atomicAddInt(lockcntr, -sz);
				ASSERT(sz >= 0);
			}
		}

skip:
		if (attr->attr_end == end)
			break;

		attr = attr->attr_next;
	}
}

/*
 * unlockmem - decrement the lock count on the physical page and
 * free it if it goes to zero.  All pages must be within the specified
 * pregion.
 *
 * The underlying region may (detachreg) or may not (munpin) be locked
 * we don't need to lock it here since we have the aspacelock, so the region
 * itself cannot change, and the pages are locked so vhand won't touch them
 */
static void
unlockmem(pas_t *pas, preg_t *prp, caddr_t vaddr, pgcnt_t npgs)
{
	register pde_t *pt;
	register pfd_t *pfd;
	auto pgno_t sz;
	int nravail = 0;
	int nsavail;

	while (npgs) {
		sz = npgs;
		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
		npgs -= sz;
		vaddr += ctob(sz);

		for ( ; --sz >= 0; pt++) {
			ASSERT(pt);
                        pg_pfnacquire(pt);
			pfd = pdetopfdat(pt);
			if (pas_pageunlock(pas, pfd))
				nravail++;
                        pg_pfnrelease(pt);
		}
	}

	if (nravail) {
		if (prp->p_reg->r_anon)
			nsavail = 0;
		else
			nsavail = nravail;

		pas_unreservemem(pas, nsavail, nravail);
	}
}

static int
pas_shmlock(pas_t *pas, ppas_t *ppas, int lorunl, as_mohandle_t handle)
{
	preg_t *prp;
	int doingshd = 0;
	int found = 0;
	int error;

	prp = PREG_FIRST(ppas->ppas_pregions);
	doingshd = 0;
doshdlk:
	for ( ; prp ; prp = PREG_NEXT(prp)) {
		if (prp->p_reg == (reg_t *)handle.as_mohandle) {
			doingshd++;
			found = 1;
			break;
		}
	}
	if (!found && !doingshd) {
		/* didn't find in local regions, try shared ones */
		doingshd = 1;
		prp = PREG_FIRST(pas->pas_pregions);
		goto doshdlk;
	}
	if (!found)
		return EINVAL;

	if (lorunl == PGLOCK)
		error = lockpages(pas, ppas, prp->p_regva, prp->p_pglen);
	else
		error = unlockpages(pas, ppas, prp->p_regva, prp->p_pglen, 0);
	return error;
}

