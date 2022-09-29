/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996, Silicon Graphics, Inc.	  *
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

#ident	"$Revision: 3.111 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/anon.h>
#include <ksys/as.h>
#include <os/as/as_private.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <os/as/pmap.h>
#include <sys/kabi.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/watch.h>
#include <ksys/vproc.h>
#ifdef EVEREST
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/pfdat.h>
#include <sys/immu.h>
#endif
#ifdef HEART_COHERENCY_WAR
#include <sys/cpu.h>
/* Do not want this on globaly to avoid most flushing, but want it here */
#define cachewrback 1
#endif
#ifdef	R10000_SPECULATION_WAR
#include <sys/cpu.h>
#endif
#include <sys/tile.h>

#define	TRUE	1
#define	FALSE	0

#ifdef EVEREST
page_counter_t	*page_counters;
pfn_t		num_page_counters;
#endif

/*
 * Check user read/write access
 * If rw has B_PHYS set, the user PTEs will be faulted
 * in and locked down.  Returns 0 on failure, 1 on success
 *
 * NOTE: Returns with shared address region in MR_ACCESS state for !B_PHYS only
 *	a aspaceunlock MUST be done.
 * On error, aspacelock is unlocked and errno is returned.
 *
 * for B_PHYS, all affected regions are made noshrink
 *
 * Protection:
 *	From region shrinking via a share group member process - we hold
 *		aspacelock if not a share group then we are only one that
 *		can change region
 *	From vhand talking away pte - we hold reglock when accessing pte
 *	From sched swapping out page tables - we are either running or sleeping
 *		at kernel priority
 * WARNING - this is new (6.x) useracc used to return 0 on failure
 *	in now returns 0 on success.
 *	To help the transition we have added an extra argument to hopefully catch old usage at compile time..
 * Useracc has been extended to take a flag B_ASYNC. This flag is needed
 * if useracc needs to be called to lock a page for an extended period of
 * time (eg. gfx) by an user program.  When such a page is locked for a long
 * time we need to break the COW for that page as the process can fork before
 * doing an unuseracc. Refer bug 370717. We need a special flag as
 * calling pfault() in a loop will cause a performance degradation and the
 * flag is used to isolate this call to pfault. As useracc with B_ASYNC will
 * be called only  a few times ( ~ 1) from an user program this should not be
 * problem.
 * Note that the B_ASYNC case does something useful only in the B_WRITE case,
 * namely break the cow which would not be done otherwise (the cow is broken
 * by default in B_READ case). Breaking the cow is useful for gfx processes
 * in the B_WRITE case, else they would have a problem if they go back to
 * user mode and try to write to the page, whereby they would get an unlocked
 * page. The same problem exists if a shaddr member is doing a write out of
 * memory with the page in cow mode (because it has a forked child), whilst 
 * another shaddr member writes to the page, getting an unlocked page.
 */

/* ARGSUSED */
int
useracc(void *base, size_t count, int rw, void *readme)
{
	return(fast_useracc(base,count,rw,(int *)0));
}

/*
 * The cookie field is used to inform the caller as to whether a faster
 * path was taken. The caller should pass the cookie to fast_unuseracc
 * The fast path is taken if region for which useracc is called is already
 * mpinned by the process.
 *
 * Returns 0 on success and errno on failure.
 */

int
fast_useracc(void *base, size_t count, int rw, int *cookie)
{
	pgcnt_t		npgs;
	int		x;
	preg_t		*prp;
	preg_t		*previous_prp = NULL;
	reg_t		*rp = NULL;
	uvaddr_t	vaddr = base;
	uvaddr_t	eaddr;
	pfd_t		*pfd;
	int		first;
	int		doavailsmem;
	int		prefaulted = 0;
	attr_t		*attr; 
	int		abi = get_current_abi();
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;
#ifdef EVEREST
	size_t		orig_count;
	int		first_aligned;
	int		last_aligned;
#endif
	int error;
	/* REFERENCED */
	int rv;

	if (count == 0)
		return(EINVAL);

	if (ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi)) {
		if (!IS_KUSEG32((caddr_t)base) ||
		    !IS_KUSEG32((caddr_t)base + count - 1)) {
			return(EFAULT);
		}
	}
#if _MIPS_SIM == _ABI64
	else if (!IS_KUSEG64((caddr_t)base) ||
		 !IS_KUSEG64((caddr_t)base + count - 1)) {
		return(EFAULT);
	}
#endif
		
	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

	/*
	 * If B_PHYS is not set, then caller just wants the page range
	 * validated and faulted in.  We return with aspacelock held in
	 * this case.  We don't lock the pages down in this case since
	 * the caller doesn't intend to do DMA.  The pages will stay
	 * valid at least until aspacelock is released.
	 */
	if ((rw & B_PHYS) == 0) {
		return fault_in(pas, ppas, base, count, rw);
	}

	/*
	 * In many cases, the pages of the buffer will already be present
	 * in memory.  Doing fubytes on all the pages can generate a lot of
	 * full tfaults for large processes since kmiss doesn't drop in
	 * segment table entries if the fault occurred in kernel mode
	 * (because of the possibility of creating a duplicate TLB entry
	 * with one of the wired slots where the kernel stack and upage
	 * are presently mapped). 
	 *
	 * So we'll assume that everything we need is already in the pmap.
	 * If we a page is not present or not writable as we go along,
	 * then we'll stop, let go of the locks, and fault in all the 
	 * rest of the pages.
	 */

	npgs = numpages(vaddr, count);

#ifdef EVEREST
	/*
	 * Turn off the fast path so that we are sure to look
	 * at every page if we need to do the io4 ia work around.
	 */
	if (io4ia_war && io4ia_userdma_war && (cookie != NULL)) {
		*cookie = FALSE;
		cookie = NULL;
	}
	orig_count = count;
#endif /* EVEREST */
	/* Asynchronous DMA. Turn off fast path. Need to check every page. */
	if (rw & B_ASYNC && (cookie != NULL)) {
		*cookie = FALSE;
		cookie = NULL;
	}

resume:
	mraccess(&pas->pas_aspacelock);

	do {
		register pde_t	*pt;
		auto pgno_t	sz;

		prp = findpreg(pas, ppas, vaddr);

		if (prp == NULL) {

			/* maybe we have to grow the stack first */

			if (!prefaulted) {
				mrunlock(&pas->pas_aspacelock);

				if (error = fault_in(pas, ppas, vaddr, count, rw))
					goto out;

				prefaulted = 1;
				goto resume;
			}

			error = EACCES;
			mrunlock(&pas->pas_aspacelock);
			goto out;
		}

		if (prp != previous_prp) {
			/* shifted pregions...need to set noshrink */
			first = 1;
			previous_prp = prp;

			rp = prp->p_reg;
		}
		reglock_rd(rp);

#ifdef IP32
		/* XXX should this be on for everyone?? */
		if (rp->r_flags & RG_PHYS) {
			sz = MIN(npgs, pregpgs(prp, vaddr));
			npgs -= sz;
			count -= ctob(sz) - poff(vaddr);
			vaddr += ctob(sz) - poff(vaddr);

			atomicAddInt(&prp->p_noshrink, 1);

			regrele(rp);
			rp = NULL;
			first = 0;
			continue;
		}
#endif

		/* If availsmem wasn't allocated when
		 * the region was created, must allocate
		 * it here to avoid swap space deadlock.
		 */
		if (rp->r_anon)
			doavailsmem = 0;
		else
			doavailsmem = 1;

		sz = MIN(npgs, pregpgs(prp, vaddr));
		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, VM_NOSLEEP);
		if (sz == 0) {
			regrele(rp);
			setsxbrk();
			continue;
		}

		if (cookie) {
			/*
			 * We go through the long path if we have to span
			 * multiple regions. Otherwise it is hard to keep
			 * track of multiple useraccs.
			 */
			if (npgs > sz) {
				*cookie = FALSE;
				cookie = NULL;
			} else {
				/*
				 * User called fast_useracc directly 
				 * If the pages have already been mpinned then don't
				 * go through the faulting in process. Increment 
				 * dmalock count. As we are holding the region lock 
				 * shared we have to increment the dmacnt atomically. 
				 */
				eaddr = vaddr + (NBPP *sz - poff(vaddr));
			
				/*
				 * The following routine does not clip or coalesce. It returns
				 * a list of attributes which holds the <vaddr, eaddr>.
				 * This is because that we do not hold the region lock 
				 * in update mode.
				 */

				attr = findattr_range_noclip(prp, 
						vaddr - poff(vaddr), eaddr);

				/*
				 * First check if the entire range we are 
				 * looking for is mpinned. If not go through 
				 * the normal path.
				 */
				do {
					if (attr->attr_lockcnt == 0) {
						*cookie = FALSE;
						cookie = NULL;
						break;
					}
					else if (( rw & B_READ) && 
						(!(attr->attr_prot 
							& PROT_WRITE))) {
						*cookie = FALSE;
						cookie = NULL;
						break;
					}
					if ( attr->attr_end >= eaddr) 
							break;
				} while (attr = attr->attr_next);
			}
			if (cookie) {
				/*
				 * Set noshrink
				 */
				atomicAddInt(&prp->p_fastuseracc, 1);
				atomicAddInt(&prp->p_noshrink, 1);
				*cookie = TRUE; 
				regrele(rp);
				break;
			}
		}

		for ( ; --sz >= 0 ; count -= NBPP - poff(vaddr), 
				    vaddr += NBPP - poff(vaddr), pt++, npgs--) {

			/*
			 * If pte is not yet in the proper state and we haven't
			 * touched all the pages yet on a previous iteration,
			 * then do that now to force any grows to occur.  We
			 * only want to do this once since we have to let go
			 * of all the locks.  If we've done the prefault once
			 * before, then all the grows have been done and we
			 * can fault the individual pages back in without
			 * letting go of aspace lock.  This latter case only
			 * happens if vhand got in and paged things out on us.
			 *
			 * So, the page must be valid and if we're doing a
			 * a read into the page, then it must also be 
			 * writable.  If this isn't the case, then touch the
			 * pages and try to make them that way.
			 *
			 * KAIO uses B_LEADER to indicate the page needs to be locked
			 * down, but no DMA will be done to it in that state. No DMA,
			 * no ia_war needed.
			 */

			if ((!pg_isvalid(pt) || 
			   ((rw & B_READ) && !pg_ismod(pt))) && !prefaulted) {
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);

				if (error = fault_in(pas, ppas, vaddr, count, rw))
					goto out;

				prefaulted = 1;
				goto resume;
			}

                        pg_pfnacquire(pt);

			while ((!pg_isvalid(pt)) || ((rw & B_READ) && !pg_ismod(pt))) {
				/*
				 * Check for watched pages which are never
				 * validated.
				 */
				if (curuthread->ut_watch)
					if (intlb(curuthread, vaddr, rw))
						goto done;
				pg_pfnrelease(pt);
				/* wow! somebody paged it out or made 	*/ 
				/* it COW (shaddr sproc forking)	*/
				regrele(rp);

				/* fault in again */
				x = fubyte(vaddr);
				ASSERT(x != -1);
				if (rw & B_READ) {
					x = subyte(vaddr, x);
					ASSERT(x != -1);
				}

				/* Since region released and going after
				 * memory could go SXBRK and get swapped out.
				 *
				 * XXX	Not sure that ptes can go away
				 * XXX	with aspacelock held.
				 */
				reglock_rd(rp);
				sz = MIN(npgs, pregpgs(prp, vaddr));
				pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
                        	pg_pfnacquire(pt);
				ASSERT(sz && sz <= npgs);
				sz--;	/* match inner test condition */
			}
done:
                        
			pfd = pfntopfdat(pg_getpfn(pt));

			/*
			 * Long term DMA. Break COW on any pages.
			 */
			if (rw & B_ASYNC) {
				/*
				 * COW page. Break the COW.
				 */
				if (rp->r_flags & RG_CW &&
					(anon_modify(rp->r_anon, pfd, 
				rp->r_flags & RG_HASSANON) == COPY_PAGE)) {
					pg_pfnrelease(pt);
					regrele(rp);
					mrunlock(&pas->pas_aspacelock);

					/*
					 * Call pfault to do the COW actions.
					 * If user is going to write page
					 * be sure everything is set up.
					 */
					rv = pfault(vaddr, 0, &error,
						(rw & B_READ) ? W_WRITE : 0);
					ASSERT(rv == 0);
					prefaulted = 1;
					goto resume;
				}
			}

#ifdef MH_R10000_SPECULATION_WAR
                        if (pfd < pfd_lowmem) {
                                int r;
				pg_pfnrelease(pt);
                                regrele(rp);
				mrunlock(&pas->pas_aspacelock);
                                if (r = page_migrate_lowmem_page(pfd, NULL, VM_DMA_RW)) {
                                        if (r == ENOMEM) {
                                                setsxbrk();
                                                goto resume;
                                        } else {
						error = EAGAIN;
                                                goto out;
                                        }
                                }
                                goto resume;
                        }
#endif /* MH_R10000_SPECULATION_WAR */

			/*
			 * Mark page locked, doing any required
			 * r/smem accounting.
			 */ 
			error = pas_pagelock(pas, pfd, doavailsmem, 0);
			if (error) {
				error = EAGAIN;
				pg_pfnrelease(pt);
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				goto out;
			}
			pfd_setflags(pfd, P_USERDMA);
#ifdef EVEREST
			if (io4ia_war && io4ia_userdma_war && 
							!(rw & B_LEADER)) {
				x = pfdat_lock(pfd);
				if (!(rw & B_READ)) {
					error = page_counter_incr(
						  pfdattopfn(pfd),
						  poff(vaddr),
						  MIN(count,
						      NBPP - poff(vaddr)));
				} else if (SCACHE_ALIGNED(vaddr) &&
					   (SCACHE_ALIGNED(count) ||
					    ((poff(vaddr) + count) > NBPP))) {
					error = page_counter_set(
						  pfdattopfn(pfd),
						  poff(vaddr),
						  MIN(count,
						      NBPP - poff(vaddr)));
				} else {
					error = 0;
				}
			 	pfdat_unlock(pfd, x);

				if (error) {
					int unresv = pas_pageunlock(pas,
									pfd);
					pg_pfnrelease(pt);
					regrele(rp);
					if (unresv) {
						pfd_clrflags(pfd, P_USERDMA);
						pas_unreservemem(pas, 
								doavailsmem, 1);
					}
					mrunlock(&pas->pas_aspacelock);
					cmn_err_tag(315,CE_WARN,
						    "User dma failed for process [%s] (pid %d)",
						    get_current_name(),
						    current_pid());
					goto out;
				}
			}
#endif /* EVEREST */
                        pg_pfnrelease(pt);
                        
			/*
			 * we now have at least one page in this pregion
			 * locked down -- mark it as no-shrinky
			 */
			if (first) {
				first = 0;
				atomicAddInt(&prp->p_noshrink, 1);
			}
		}
		regrele(rp);

	} while (npgs);

	/* Success */
	mrunlock(&pas->pas_aspacelock);
#ifdef EVEREST
	if (io4ia_war && io4ia_userdma_war && !(rw & B_LEADER)) {
		first_aligned = SCACHE_ALIGNED(base);
		last_aligned = SCACHE_ALIGNED((caddr_t)base + orig_count);
		if ((rw & B_READ) && (!first_aligned || !last_aligned)) {
			dma_info_insert_user(current_pid(), base, orig_count);
		}
	}
#endif
	return(0);

out:
#ifdef EVEREST
	/*
	 * Tell unuseracc not to do the dma_info_lookup(delete) since
	 * we never got around to inserting anything.
	 */
	unuseracc(base, (size_t)(vaddr - (caddr_t)base),
		  (rw | B_STALE));
#else
	unuseracc(base, (size_t)(vaddr - (caddr_t)base), rw);
#endif
	return(error);
}


/*
 * Fault in all the pages in the given range.  Neither the aspacelock nor
 * the region locks can be held when this routine is called or a deadlock
 * could result.  Returns 0 on sucess, errno on failure.
 * Returns w/ aspacelock held on success AND !(rw & B_PHYS)
 */
int
fault_in(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr, size_t count, int rw)
{
	pgcnt_t	size, npgs;
	preg_t	*prp;
	int x;

	/*
	 * First touch 1st & last page to force any 'grows' that might
	 * be required
	 * we MUST leave aspacelock unlocked here in case we do any
	 * grows - they need aspacelock(UPD)
	 */
	x = fubyte(vaddr);
	if (x == -1) {
		return(EFAULT);
	}
	if (rw & B_READ)
		if (subyte(vaddr, x) == -1)
			return(EACCES);
	/* now last page */
	x = fubyte(vaddr + count - 1);
	if (x == -1)
		return(EFAULT);
	if (rw & B_READ)
		if (subyte(vaddr + count - 1, x) == -1)
			return(EACCES);

	/*
	 * Now go through all pages, faulting in everything
	 */
	npgs = numpages(vaddr, count);
	mraccess(&pas->pas_aspacelock);

	while (npgs > 0) {
		prp = findpreg(pas, ppas, vaddr);
		if (!prp) {
			mrunlock(&pas->pas_aspacelock);
			return(EACCES);
		}

		size = MIN(npgs, pregpgs(prp, vaddr));

		npgs -= size;

		while (--size >= 0) {
			x = fubyte(vaddr);
			if (x == -1) {
				mrunlock(&pas->pas_aspacelock);
				return(EFAULT);
			}
			
			if (rw & B_READ)
				if (subyte(vaddr, x) == -1) {
					mrunlock(&pas->pas_aspacelock);
					return(EACCES);
				}

			vaddr += ctob(1) - poff(vaddr);
		}
	}

	/*
	 * We return with aspacelock held in this case where B_PHYS is
	 * not set.  The caller is then responsible for
	 * releasing it.
	 */
	if (rw & B_PHYS)
		mrunlock(&pas->pas_aspacelock);

	return(0);
}

void
unuseracc(void *base, size_t count, int rw)
{
	fast_unuseracc(base, count, rw, (int *)0);
}

/*
 * undo page locking
 */
void
fast_unuseracc(void *base, size_t count, int rw, int *cookie)
{
	pgcnt_t	npgs;
	reg_t	*rp;
	preg_t	*previous_prp = NULL;
	preg_t	*prp;
	pde_t	*pt;
	pgno_t	sz;
	uvaddr_t vaddr = base;
	pfd_t	*pfd;
	pgcnt_t	nravail = 0;
	pgcnt_t	nsavail = 0;
	int		doavailsmem;
	int		x;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;
#ifdef EVEREST
	dma_info_t	*dmap;
	size_t		orig_count;
	int		first_aligned;
	int		last_aligned;
#endif

#ifdef EVEREST
	orig_count = count;
#endif

	if (count == 0)
		return;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

	if ((rw & B_PHYS) == 0) {
		mrunlock(&pas->pas_aspacelock);
		return;
	}
	npgs = numpages(vaddr, count);

	mraccess(&pas->pas_aspacelock);

	do {
		prp = findpreg(pas, ppas, vaddr);
		rp = prp->p_reg;
		reglock_rd(rp);

		/*
		 * If somebody had already mpinned these pages then we would
		 * have just incremented the dmacnt and not touched the
		 * pfdats. So decrement the dmacnt. The cookie tells us
		 * if we took the fast path or not.
		 */
		if (cookie && (*cookie == TRUE)) {
#ifdef	DEBUG
			sz = MIN(npgs, pregpgs(prp, vaddr));
			ASSERT(npgs == sz);
#endif
			ASSERT(prp->p_noshrink > 0);
			atomicAddInt(&prp->p_noshrink, -1);
			atomicAddInt(&prp->p_fastuseracc, -1);
			regrele(rp);
			break;
		}

#ifdef IP32
		/* XXX should this be on for everyone?? */
		if (prp->p_reg->r_flags & RG_PHYS) {
			sz = MIN(npgs, pregpgs(prp, vaddr));
			npgs -= sz;
			count -= ctob(sz) - poff(vaddr);
			vaddr += ctob(sz) - poff(vaddr);

			atomicAddInt(&prp->p_noshrink, -1);

			regrele(prp->p_reg);
			continue;
		}
#endif /* IP32 */

		if (prp != previous_prp) {
			/* shifted pregions...need to decrement noshrink */
			ASSERT(prp->p_noshrink > 0);
			atomicAddInt(&prp->p_noshrink, -1);
			previous_prp = prp;
		}

		/* If availsmem wasn't allocated when
		 * the region was created, must allocate
		 * it here to avoid swap space deadlock.
		 */
		if (rp->r_anon)
			doavailsmem = 0;
		else
			doavailsmem = 1;

		sz = MIN(npgs, pregpgs(prp, vaddr));

		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
		ASSERT(sz && sz <= npgs);

#ifdef EVEREST
		if (!io4ia_war || !io4ia_userdma_war || (rw & (B_PARTIAL|B_LEADER))) {
			vaddr += ctob(sz);
			npgs -= sz;
		}
#else
		vaddr += ctob(sz);
		npgs -= sz;
#endif /*EVEREST*/

		for ( ; --sz >= 0 ; pt++) {

			/* Used to set mod bit.
			 * I claim it will always be set - we locked
			 * the page and noone touches nothing if its 
			 * locked.
			 */
#ifdef MH_R10000_SPECULATION_WAR
			pfd = pfntopfdat(pg_getpfn(pt));
                        if (!(IS_R10000() && pfd->pf_rawcnt > 1))
#endif /* MH_R10000_SPECULATION_WAR */
			pg_setref(pt);
			if (rw == B_READ) {
				/* Could be a write-watched page:
				 * page gets modified, but pt stays 
				 * unmod?
				 */
				ASSERT(pg_isdirty(pt));
			}

                        pg_pfnacquire(pt);
                        
#ifndef MH_R10000_SPECULATION_WAR
			pfd = pfntopfdat(pg_getpfn(pt));
#endif	/* MH_R10000_SPECULATION_WAR */
			if (pas_pageunlock(pas, pfd)) {
				pfd_clrflags(pfd, P_USERDMA);
				nravail++;
				if (doavailsmem) nsavail++;
			}
#ifdef EVEREST
/* kaio_cleanup uses B_PARTIAL when calling fast_undma to skip the counter clearing.
   kaio_done has already called page_counter_undo directly to take care of that.
   B_LEADER is used to indicate the mem area will not actually be used for DMA.
 */
			if (io4ia_war && io4ia_userdma_war &&
			    !(rw & (B_PARTIAL|B_LEADER))) {
				x = pfdat_lock(pfd);
				if (!(rw & B_READ)) {
					page_counter_decr(pfdattopfn(pfd),
						  poff(vaddr),
						  MIN(count,
						      NBPP - poff(vaddr)));
				} else if (SCACHE_ALIGNED(vaddr) &&
					   (SCACHE_ALIGNED(count) ||
					    ((poff(vaddr) + count) > NBPP))) {
					page_counter_clear(pfdattopfn(pfd),
						   poff(vaddr),
						   MIN(count,
						       NBPP - poff(vaddr)));
				}

				count -= NBPP - poff(vaddr);
				vaddr += NBPP - poff(vaddr);
				npgs -= 1;
				pfdat_unlock(pfd, x);
			}
#endif /*EVEREST*/
                        pg_pfnrelease(pt);
		}
		regrele(rp);

	} while (npgs);

	/*
 	 * Don't call pas_unreservemem if nsavail and nravail are zero.
	 * This should help an I/O scaling problem 522327.
	 */
	if (nsavail || nravail)
		pas_unreservemem(pas, nsavail, nravail);
	mrunlock(&pas->pas_aspacelock);

#ifdef EVEREST
	if (io4ia_war && io4ia_userdma_war &&
	    (rw & B_READ) && !(rw & (B_STALE|B_LEADER))) {
		first_aligned = SCACHE_ALIGNED(base);
		last_aligned = SCACHE_ALIGNED((caddr_t)base + orig_count);
		if (!first_aligned || !last_aligned) {
			dmap = dma_info_lookup_user(current_pid(), base,
						    orig_count, 1);
			ASSERT(dmap != NULL);
			dma_info_free(dmap);
		}
	}
#endif /*EVEREST*/

}

/*
 * Setup user physio
 * NOTE: protection against a shared process shrinking/detaching the region
 *	while its mapped (some drivers may keep a section of a region mapped
 *	for a long while - across a system call)
 *	we bump the p_noshrink cnt. This keeps growreg, munmap, shmdt from
 *	doing anything rash
 * WARNING - as of 6.x returns 0 on success and errno on failure.
 * we have added the unused last argument to help catch old usage.
 */
/* ARGSUSED */
int
userdma(void *base, size_t count, int rw, void *readme)
{
	return(fast_userdma(base,count,rw, (int *)0));
}

/*
 * 'fast' version that considers whether the pages are already pinned
 * Returns 0 on success, errno on error.
 */
int
fast_userdma(void *base, size_t count, int rw, int *cookie)
{
#ifdef R4600SC
	extern int boot_sidcache_size, two_set_pcaches;
#endif
	int error;

	if (IS_KUSEG(base)) {
		extern int async_udma_copy;
		if (async_udma_copy) {
			/*
			 * set B_ASYNC flag to tell fast_useracc to break
			 * COW for pthread shared pages that might get
			 * modified after a fork - see PV 621442
			 */
			rw |= B_ASYNC;
		}
		if (error = fast_useracc(base, count, rw | B_PHYS, cookie)) {
			return error;
		}
	}

#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000() && (rw & B_READ)) {
		invalidate_range_references(base, count,
			CACH_DCACHE|CACH_SCACHE|
			CACH_INVAL|CACH_IO_COHERENCY, 0);
		return(0);
	}
#endif /* MH_R10000_SPECULATION_WAR */

	/*
	 * It is questionable whether we need to do a CACH_INVAL at this
	 * point. In both the B_READ and B_WRITE case, we do need a 
	 * CACH_WBACK though; trivially easy to see that we need it for
	 * B_WRITE, we also need it for B_READ to make sure that a DMAed
	 * line in memory is not overwritten by eviction of a dirty L2
	 * line, as well as not to loose old data on IO failure.
	 */
	if (cachewrback) {
		cache_operation(base, count,
			CACH_DCACHE|CACH_WBACK|CACH_IO_COHERENCY|
#ifdef R4600SC
			/*
			 * on R4600SC the scache is write-through, so on
			 * writes, we just have to flush the data from the
			 * pcache to the scache -- we don't need to invalidate
			 * the lines in the scache.
			 */
			(((two_set_pcaches && boot_sidcache_size) && 
			    !(rw & B_READ)) ? 0 : CACH_INVAL)
#elif HEART_COHERENCY_WAR
			(heart_need_flush(rw & B_READ) ? CACH_FORCE : 0)
#else
			CACH_INVAL
#endif
			  );
	}

	/*
	 * Old code lived here that would flush the data cache for
	 * reads on machines that weren't coherent (dma I/O vs. cache).
	 *
	 * It doesn't make sense to flush now since it is before the
	 * I/O. The flush must be done after the I/O is done.
	 * The problem is that the process might have other threads
	 * that could cause the cache to be re-dirtied before/during the
	 * I/O.
	 *
	 * Soo, this flushing was moved to undma below.
	 *
	 */

#if R10000_SPECULATION_WAR && !MH_R10000_SPECULATION_WAR
	if (spec_udma_war_on && (rw & B_READ) && (count > 0) && IS_KUSEG(base)){
		if (specwar_fast_userdma(base, count, cookie) != 0) {
			fast_unuseracc(base,count,rw,cookie);
			return EPERM;
		}
	}
#endif	/* R10000_SPECULATION_WAR && !MH_R10000_SPECULATION_WAR */

	return 0;
}

void
undma(void *base, size_t count, int rw)
{
	fast_undma(base,count,rw,(int *)0);
}

/*
 * Terminate user physio
 */
void
fast_undma(void *base, size_t count, int rw, int *cookie)
{
	if (!IS_KUSEG(base))
		return;

	/*
	 * Flush the cache. Multiple threads could re-dirty the
	 * cache right up until the dma is done.
	 * So for machine without dma and I/O coherency, flush
	 * the cache after the dma is done.
	 * This will invalidate the L2 lines so that the next access
	 * will cause memory lines to be filled into L2. Incidentally, 
	 * this also does CACH_WBACK, which is okay, even for non cache
	 * aligned transfers.
	 */
	if (rw & B_READ)
		cache_operation(base, count, CACH_DCACHE|CACH_IO_COHERENCY);

#if R10000_SPECULATION_WAR && !MH_R10000_SPECULATION_WAR
	if (spec_udma_war_on && (rw & B_READ) && (count > 0)) {
		specwar_fast_undma( base, count, cookie );
	}
#endif	/* R10000_SPECULATION_WAR && !MH_R10000_SPECULATION_WAR */

	fast_unuseracc(base, count, rw | B_PHYS, cookie);
}


#ifdef EVEREST

/*
 * This is for kernel async I/O to use to clear the page counters
 * for an I/O from interrupt level.  It gets the physical pages from
 * the kernel virtual address obtained by iomap() or maputokv().
 */
void
page_counter_undo(
	caddr_t	kaddr,
	size_t	count,		  
	int	write)
{
	int	page_count;
	int	i;
	pfn_t	pfn;
	pfd_t	*pfd;
	int	s;

	if (!io4ia_war || !io4ia_userdma_war) {
		return;
	}

	/*
	 * This will not work with unaligned addresses.  The
	 * kernel address will point to the extra page we allocated
	 * in iomap() or maputokv(), so we'll get the wrong pfn from
	 * it for decrementing the counters.
	 */
	ASSERT(SCACHE_ALIGNED(kaddr) && SCACHE_ALIGNED(count));
	page_count = numpages(kaddr, count);

	/*
	 * For each virtual page, convert it to the physical page
	 * and drop the page counters for this I/O.
	 */
	for (i = 0; i < page_count; i++) {
		pfn = kvtophyspnum(kaddr);
		pfd = pfntopfdat(pfn);
		s = pfdat_lock(pfd);
		if (write) {
			page_counter_decr(pfn, poff(kaddr),
					  MIN(count, NBPP - poff(kaddr)));
		} else if (SCACHE_ALIGNED(kaddr) &&
			   (SCACHE_ALIGNED(count) ||
			    ((poff(kaddr) + count) > NBPP))) {
			page_counter_clear(pfn, poff(kaddr),
					   MIN(count, NBPP - poff(kaddr)));
		}
		pfdat_unlock(pfd, s);

		count -= NBPP - poff(kaddr);
		kaddr += NBPP - poff(kaddr);
	}
	return;
}

#undef	SCACHE_LINEMASK
#undef	SCACHE_LINESIZE
#undef	SCACHE_LINESHIFT
#define	SCACHE_LINEMASK	127
#define	SCACHE_LINESIZE	128
#define	SCACHE_LINESHIFT 7

int
page_counter_incr(
	pfn_t	pgno,
	int	start_byte,
	int	len)
{
	page_counter_t	*pcp;
	int		line;
	int		start_line;
	int		end_line;
	int		word_index;
	int		counter_index;
	__uint64_t	word_val;
	__uint64_t	word_mask;

	ASSERT(start_byte < NBPP);
	ASSERT(len <= (NBPP - start_byte));

	pcp = &(page_counters[pgno]);
	start_line = start_byte >> SCACHE_LINESHIFT;
	end_line = (start_byte + len + SCACHE_LINEMASK) >> SCACHE_LINESHIFT;
	ASSERT(start_line < (NBPP >> SCACHE_LINESHIFT));
	ASSERT(end_line <= (NBPP >> SCACHE_LINESHIFT));

	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			word_mask = 3ULL << (counter_index * 2);
			if ((word_val & word_mask) == word_mask) {
				return EBUSY;
			}
#ifdef PCDEBUG
			ASSERT((word_val & word_mask) == 0ULL);
#endif
			counter_index++;
			line++;
		}
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}

	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			/*
			 * Add one to the proper counter.  We just shift
			 * a one over to the 2 bit counter and add it.
			 * We know we won't overflow because of the
			 * check above.
			 */
			word_val += (1ULL << (counter_index * 2));
			counter_index++;
			line++;
		}
		pcp->pc_words[word_index] = word_val;
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}

#ifdef PCDEBUG
	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			ASSERT(((word_val >> (counter_index * 2)) & 3) == 1);
			counter_index++;
			line++;
		}
		word_index++;
	}
#endif
	return 0;
}

void
page_counter_decr(
	pfn_t	pgno,
	int	start_byte,
	int	len)
{
	page_counter_t	*pcp;
	int		line;
	int		start_line;
	int		end_line;
	int		word_index;
	int		counter_index;
	__uint64_t	word_val;
#ifdef DEBUG
	__uint64_t	word_mask;
#endif

	ASSERT(start_byte < NBPP);
	ASSERT(len <= (NBPP - start_byte));

	pcp = &(page_counters[pgno]);
	start_line = start_byte >> SCACHE_LINESHIFT;
	end_line = (start_byte + len + SCACHE_LINEMASK) >> SCACHE_LINESHIFT;
	ASSERT(start_line < (NBPP >> SCACHE_LINESHIFT));
	ASSERT(end_line <= (NBPP >> SCACHE_LINESHIFT));

#ifdef DEBUG
	/*
	 * ASSERT that all of counters in the given range are non-zero.
	 */
	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			word_mask = 3ULL << (counter_index * 2);
			ASSERT((word_val & word_mask) != 0);
#ifdef PCDEBUG
			ASSERT(((word_val & word_mask) >> (counter_index * 2)) == 1);
#endif
			counter_index++;
			line++;
		}
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}
#endif /*DEBUG*/

	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			/*
			 * Subtract one from the proper counter.  We
			 * just shift a one over to the 2 bit counter
			 * and subtract it.  We know we won't underflow
			 * because of the check above.
			 */
			word_val -= (1ULL << (counter_index * 2));
			counter_index++;
			line++;
		}
		pcp->pc_words[word_index] = word_val;
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}

#ifdef PCDEBUG
	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			ASSERT(word_val == 0ULL);
			counter_index++;
			line++;
		}
		word_index++;
	}
#endif /*PCDEBUG*/
}

int
page_counter_set(
	pfn_t	pgno,
	int	start_byte,
	int	len)
{
	page_counter_t	*pcp;
	int		line;
	int		start_line;
	int		end_line;
	int		word_index;
	int		counter_index;
	__uint64_t	word_val;
	__uint64_t	word_mask;

	ASSERT(start_byte < NBPP);
	ASSERT(len <= (NBPP - start_byte));

	pcp = &(page_counters[pgno]);
	start_line = start_byte >> SCACHE_LINESHIFT;
	end_line = (start_byte + len + SCACHE_LINEMASK) >> SCACHE_LINESHIFT;
	ASSERT(start_line < (NBPP >> SCACHE_LINESHIFT));
	ASSERT(end_line <= (NBPP >> SCACHE_LINESHIFT));

	/*
	 * Check that all of counters in the given range are zero.
	 */
	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			word_mask = 3ULL << (counter_index * 2);
			if ((word_val & word_mask) != 0) {
#ifdef PCDEBUG
				ASSERT(0);
#endif
				return EBUSY;
			}
			counter_index++;
			line++;
		}
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}

	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			word_val |= (3ULL << (counter_index * 2));
			counter_index++;
			line++;
		}
		pcp->pc_words[word_index] = word_val;
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}

#ifdef PCDEBUG
	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			ASSERT(((word_val >> (counter_index * 2)) & 3ULL) == 3ULL);
			counter_index++;
			line++;
		}
		word_index++;
	}
#endif /*PCDEBUG*/
	return 0;
}

void
page_counter_clear(
	pfn_t	pgno,
	int	start_byte,
	int	len)
{
	page_counter_t	*pcp;
	int		line;
	int		start_line;
	int		end_line;
	int		word_index;
	int		counter_index;
	__uint64_t	word_val;
#ifdef DEBUG
	__uint64_t	word_mask;
#endif

	ASSERT(start_byte < NBPP);
	ASSERT(len <= (NBPP - start_byte));

	pcp = &(page_counters[pgno]);
	start_line = start_byte >> SCACHE_LINESHIFT;
	end_line = (start_byte + len + SCACHE_LINEMASK) >> SCACHE_LINESHIFT;
	ASSERT(start_line < (NBPP >> SCACHE_LINESHIFT));
	ASSERT(end_line <= (NBPP >> SCACHE_LINESHIFT));

#ifdef DEBUG
	/*
	 * ASSERT that all of counters in the given range are fully set.
	 */
	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			word_mask = 3ULL << (counter_index * 2);
			ASSERT((word_val & word_mask) == word_mask);
			counter_index++;
			line++;
		}
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}
#endif /*DEBUG*/

	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			word_val &= ~(3ULL << (counter_index * 2));
			counter_index++;
			line++;
		}
		pcp->pc_words[word_index] = word_val;
		word_index++;
		ASSERT((word_index < PC_NUM_WORDS) || (line == end_line));
	}

#ifdef PCDEBUG
	word_index = start_line / PC_COUNTERS_PER_WORD;
	line = start_line;
	while (line < end_line) {
		word_val = pcp->pc_words[word_index];
		counter_index = line % PC_COUNTERS_PER_WORD;
		while ((counter_index < PC_COUNTERS_PER_WORD) &&
		       (line < end_line)) {
			ASSERT(((word_val >> (counter_index * 2)) & 3ULL) == 0ULL);
			counter_index++;
			line++;
		}
		word_index++;
	}
#endif /*PCDEBUG*/
}


typedef struct dma_info_hash {
	dma_info_t	*dma_list;
	lock_t		dma_lock;
} dma_info_hash_t;

#define	DMA_INFO_UHASH_SIZE	64
#define	DMA_INFO_KHASH_SIZE	64
#define	DMA_INFO_UHASH(pid, uaddr) \
	    ((((ulong)(pid) ^ (ulong)(uaddr)) >> 8) % DMA_INFO_UHASH_SIZE)
#define	DMA_INFO_KHASH(kaddr) \
	    (((ulong)(kaddr) >> 8) % DMA_INFO_KHASH_SIZE)

dma_info_hash_t	dma_uhash[DMA_INFO_UHASH_SIZE];
dma_info_hash_t	dma_khash[DMA_INFO_KHASH_SIZE];
zone_t		*dma_info_zone;

void
dma_info_init(void)
{
	int	i;

	if (!io4ia_war || !io4ia_userdma_war) {
		return;
	}

	for (i = 0; i < DMA_INFO_UHASH_SIZE; i++) {
		spinlock_init(&(dma_uhash[i].dma_lock), "dmaulock");
	}
	for (i = 0; i < DMA_INFO_KHASH_SIZE; i++) {
		spinlock_init(&(dma_khash[i].dma_lock), "dmaklock");
	}

	dma_info_zone = kmem_zone_init(sizeof(dma_info_t), "dma_info");
}

void
dma_info_insert_user(
	pid_t		pid,
	caddr_t		uaddr,
	size_t		count)
{
	dma_info_t	*dmap;
	dma_info_hash_t	*dma_hashp;
	int		s;

	ASSERT(pid == current_pid());

	dmap = kmem_zone_alloc(dma_info_zone, KM_SLEEP);

	dmap->dma_proc = pid;
	dmap->dma_uaddr = uaddr;
	dmap->dma_count = count;
	dmap->dma_kaddr = NULL;
	dmap->dma_khash = NULL;

	dma_hashp = &(dma_uhash[DMA_INFO_UHASH(pid, uaddr)]);
	s = mutex_spinlock(&(dma_hashp->dma_lock));
	dmap->dma_uhash = dma_hashp->dma_list;
	dma_hashp->dma_list = dmap;
	mutex_spinunlock(&(dma_hashp->dma_lock), s);
}		     


dma_info_t *
dma_info_lookup_user(
	pid_t		pid,
	caddr_t		uaddr,
	size_t		count,
	int		delete)
{
	dma_info_t	*dmap;
	dma_info_t	**dmapp;
	dma_info_hash_t	*dma_hashp;
	int		s;

	ASSERT(pid == current_pid());

	dma_hashp = &(dma_uhash[DMA_INFO_UHASH(pid, uaddr)]);
	s = mutex_spinlock(&(dma_hashp->dma_lock));
	dmapp = &(dma_hashp->dma_list);
	while (*dmapp != NULL) {
		dmap = *dmapp;
		if ((dmap->dma_proc == pid) &&
		    (dmap->dma_uaddr == uaddr) &&
		    (dmap->dma_count == count) &&
		    (dmap->dma_kaddr == NULL)) {
			break;
		}
		dmapp = &((*dmapp)->dma_uhash);
	}
	if (*dmapp == NULL) {
		dmap = NULL;
	} else if (delete) {
		ASSERT(dmap != NULL);
		*dmapp = dmap->dma_uhash;
	}
	mutex_spinunlock(&(dma_hashp->dma_lock), s);

	return dmap;
}

void
dma_info_insert_kern(
	dma_info_t	*dmap,
	caddr_t		kaddr)
{
	dma_info_hash_t	*dma_hashp;
	int		s;

	ASSERT(current_pid() == dmap->dma_proc);
	dmap->dma_kaddr = kaddr;

	dma_hashp = &(dma_khash[DMA_INFO_KHASH(kaddr)]);
	s = mutex_spinlock(&(dma_hashp->dma_lock));
	dmap->dma_khash = dma_hashp->dma_list;
	dma_hashp->dma_list = dmap;
	mutex_spinunlock(&(dma_hashp->dma_lock), s);
}

dma_info_t *
dma_info_lookup_kern(
	caddr_t		kaddr,
	size_t		count,
	int		delete)
{
	dma_info_t	*dmap;
	dma_info_t	**dmapp;
	dma_info_hash_t	*dma_hashp;
	int		s;

	dma_hashp = &(dma_khash[DMA_INFO_KHASH(kaddr)]);
	s = mutex_spinlock(&(dma_hashp->dma_lock));
	dmapp = &(dma_hashp->dma_list);
	while (*dmapp != NULL) {
		dmap = *dmapp;

		if ((dmap->dma_kaddr == kaddr) &&
		    (dmap->dma_count == count)) {
			break;
		}
		dmapp = &((*dmapp)->dma_khash);
	}
	if (*dmapp == NULL) {
		dmap = NULL;
	} else if (delete) {
		ASSERT(dmap != NULL);
		*dmapp = dmap->dma_khash;
		dmap->dma_kaddr = NULL;
	}
	mutex_spinunlock(&(dma_hashp->dma_lock), s);

	return dmap;
}

void
dma_info_free(
	dma_info_t	*dmap)
{
	kmem_zone_free(dma_info_zone, dmap);
}


#endif /*EVEREST*/
