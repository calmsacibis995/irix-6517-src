/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/procfs/prusrio.c	1.3"*/
#ident	"$Revision: 1.85 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <os/as/as_private.h> /* XXX */
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/prctl.h>
#include <os/as/pmap.h> /* XXX */
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/watch.h>
#include <sys/numa.h>
#ifdef MH_R10000_SPECULATION_WAR
#include <sys/anon.h>
#endif /* MH_R10000_SPECULATION_WAR */
#include <ksys/as.h>
#include <ksys/vmmacros.h>
#include "prdata.h"
#include "os/proc/pproc_private.h"
#include "procfs.h"

static int
prusrio_lock(register proc_t *p, vasid_t curvasid, vasid_t targvasid)
{
	if (current_pid() == p->p_pid) {
		VAS_LOCK(targvasid, AS_EXCL);
		return 1;
	}

	if (current_pid() < p->p_pid) {
		VAS_LOCK(curvasid, AS_EXCL);
		VAS_LOCK(targvasid, AS_EXCL);
	} else {
		VAS_LOCK(targvasid, AS_EXCL);
		VAS_LOCK(curvasid, AS_EXCL);
	}
	return 0;
}

static void
prusrio_unlock(vasid_t curvasid, vasid_t targvasid, int rw_self)
{
	if (!rw_self)
		VAS_UNLOCK(curvasid);
	VAS_UNLOCK(targvasid);
}

/* ARGSUSED */
static void
prusrio_cachop(pas_t *pas, caddr_t addr, int flush_count, unsigned int flags)
{

	cache_operation(addr, flush_count, flags);
#if TFP
	{
	cpumask_t mask;
	mask = pas->pas_isomask;
	if (CPUMASK_NOTEQ(mask, allclr_cpumask) &&
					(flags & CACH_ICACHE_COHERENCY)) {
		cpumask_t flushmask;

		CPUMASK_CLRALL(flushmask);
		CPUMASK_SETM(flushmask, mask);
		/* XXX isomask now set for all isolated procs
		CPUMASK_SETB(flushmask, p->p_kthread.k_mustrun);
		*/

		clean_isolated_icache(flushmask);
	}
	}
#endif
}

/*
 * Perform I/O to/from a process.
 */
/*ARGSUSED*/
int
prusrio(
	register proc_t *p,
	uthread_t *ut,
	enum uio_rw rw,
	register struct uio *uiop,
	int level)
{
	caddr_t addr, maxaddr, vaddr, a;
	auto struct pregion *myprp = NULL;
	int count, error, flush_count;
	int notmoved = uiop->uio_resid;
	int rw_self = 0;
	vnode_t *vnp;
	pid_t hispid;
	auto preg_t		*hisprp;
	register reg_t		*hisrp;
	caddr_t			hisregva;
	vasid_t	targvasid, curvasid;
	pas_t *targpas, *pas;
	ppas_t *targppas, *ppas;
	register attr_t *attr;
	register char *end;
	struct pm *pm;
	anon_hdl an;

#ifdef CELL_IRIX
	if (curprocp == NULL) {		/* message xthread */
		extern int issue_procfs_req(proc_t *p, uthread_t *ut, 
					enum uio_rw rw, struct uio *uiop);
		return(issue_procfs_req(p, ut, rw, uiop));
	}
#endif

	/*
	 * To maintain sanity, do not allow a debugger to share
	 * address space with the subject process.
	 */
	if (IS_SPROC(&curprocp->p_proxy) && curprocp->p_shaddr == p->p_shaddr) {
		return EIO;
	}
	/*
	 * grab a ref to the target process's address space
	 */
	if (as_lookup(ut->ut_asid, &targvasid))
		return EIO;
	as_lookup_current(&curvasid);

	targpas = VASID_TO_PAS(targvasid);
	targppas = (ppas_t *)targvasid.vas_pasid;
	pas = VASID_TO_PAS(curvasid);
	ppas = (ppas_t *)curvasid.vas_pasid;

	/*
	 * Process must not be able to access the space we're trying to
	 * read or write while we do the i/o.
	 * Imagine a process with watch points, and we attach to its space
	 * then read from it, causing a watchpoint to fire.  Since we must
	 * step over the watchpoint, we cannot allow the process to execute.
	 * We accomplish this by holding the target's aspacelock for UPDATE
	 * and flushing all of its TLB info.  Thus it will hang up in tfault
	 * waiting for the aspacelock.
	 * So we need to hold the targets spacelock for UPDATE, but we also
	 * need to hold our aspacelock for UPDATE while we attach and detach.
	 * This creates an ABBA deadlock if two readers try to read each
	 * other's space, so we order the locks by pid.
	 *
	 * NOTE: we MUST lock aspacelock before region lock..
	 */
	rw_self = prusrio_lock(p, curvasid, targvasid);
	hispid = p->p_pid;

	addr = (caddr_t) uiop->uio_offset;

	/*
	 * Check access permissions.
	 */
	if ((hisprp = findpreg(targpas, targppas, addr)) == NULL)
		error = EIO;
	else {
		error = 0;
		hisrp = hisprp->p_reg;
		/*
		 * Iff vnode mapped object, check permissions
		 */
		if ((vnp = hisrp->r_vnode) != NULL) {
			/*
			 * We can't call VOP_ACCESS while holding address space
			 * lock. The other process may be reading the same file
			 * while we execute this code (seen with CaseVision).
			 * The code in read will lock the inode and we will try
			 * to lock the address space if the read takes a page
			 * fault.  Meanwhile, this code is doing the opposite
			 * (aspacelock followed by inode lock in efs_access):
			 * lock hierarchy violation.
			 */
			VN_HOLD(vnp);
			prusrio_unlock(curvasid, targvasid, rw_self);
			VOP_ACCESS(vnp, VREAD, get_current_cred(), error);
			rw_self = prusrio_lock(p, curvasid, targvasid);
			VN_RELE(vnp);
			if (hispid != p->p_pid)
				error = ENOENT;
			else if ((hisprp = findpreg(targpas, targppas, addr)) == NULL)
				error = EIO;
			else {
				hisrp = hisprp->p_reg;
				vnp = hisrp->r_vnode;
			}

#ifdef _VCE_AVOIDANCE
			/* ignore error of using a VCE avoidance vnode */
			if (error && vce_avoidance && vnp) {
				if (vnp->v_type == VNON)
					error = 0;
			}
#endif /* _VCE_AVOIDANCE */
		}

		if (!error && rw == UIO_WRITE && !(hisrp->r_flags & RG_PHYS)){
			/*
			 * Make sure we have a writable region.
			 *
			 * Can't get a private copy of RG_PHYS regions.
			 * Anyway if someone is writing into RG_PHYS
			 * regions, they would like to write into the actual
			 * page that corresponds to RG_PHYS. So, no need
			 * to get a private space if RG_PHYS is set.
			 */
			error = vgetprivatespace(targpas, targppas, addr, &hisprp);
			if (!error)
				hisrp = hisprp->p_reg;
		}
	}
	if (error) {
		prusrio_unlock(curvasid, targvasid, rw_self);
		as_rele(targvasid);
		return error;
	}
/* chunk size to read at a time */
#define USRIOSIZE	4096

	if ((hisrp->r_flags & (RG_PHYS|RG_PHYSIO)) == (RG_PHYS|RG_PHYSIO)){
		/* Handle physically mapped region here. Typically these 
		 * are IO devices mmapped by user via some driver
		 * Expected read sizes are one of 1/2/4/8 bytes		
		 */
		char  ioxfer_buf[8];	/* Max size of IO transfer */
		int   ioxfer_cnt;
		caddr_t	kvaddr;

		/* No support for
		 *   IO access greater than 8 bytes 
		 *   IO access crossing a page boundary.
		 */
prusrio_physio:
		prusrio_unlock(curvasid, targvasid, rw_self);

		if ((uiop->uio_resid > 8) ||	/* Too big a size */
		    (pnum(uiop->uio_offset) != 
			pnum(uiop->uio_offset + uiop->uio_resid - 1))){
			as_rele(targvasid);
			return EIO;
		}

		/*
 		 * Map in user page to our space.
		 *
		 * No need to bother about colors since it's an uncached 
		 * mapping.
		 */
		kvaddr = kvalloc(1, VM_UNCACHED_PIO, 0);

		if (!kvaddr) {	/* Make sure kvaddr is non-zero */
			as_rele(targvasid);
			return EIO;
		}
		if (!maputokptbl(targvasid, (uvaddr_t)uiop->uio_offset, 
			uiop->uio_resid, kvaddr, 
			(long)(pte_uncachephys(PG_VR | PG_G | PG_M | PG_SV)))){
			kvfree(kvaddr, 1);
			as_rele(targvasid);
			return EIO;
		}

		/* add the page offset to get the address we want: */
		kvaddr += poff(uiop->uio_offset);

#ifdef	DEBUG_LATER
		printf("RG_IO: Uncached page kvaddr: 0x%x, 0x%x, count: %d\n",
				(uint)kvaddr, 
				(uint)(kvtokptbl(kvaddr))->pgi,
				uiop->uio_resid);
#endif
		/* 
		 * - use of badaddr makes sure that an access to invalid
		 *   IO space does not cause a system crash
		 * - uiomove should be called to update all the pointers
		 */
		ioxfer_cnt = uiop->uio_resid;
		if (rw == UIO_READ){
			if(!(error=badaddr_val(kvaddr, ioxfer_cnt, ioxfer_buf)))
				/* Read okay, copy it to user space */
				uiomove(ioxfer_buf, ioxfer_cnt, rw, uiop);
		} else {
			uiomove(ioxfer_buf, ioxfer_cnt, rw, uiop);
			error = wbadaddr_val(kvaddr, ioxfer_cnt, ioxfer_buf);
		}

		if (error)
			error = EIO;

		kvfree(kvaddr, 1);
		as_rele(targvasid);

		return error;
	}

	reglock(hisrp);

	hisregva = hisprp->p_regva;
	
	/*
	 * If this is a gfx region, we are definitely going to be
	 * restricting access to one page at the most. Lets do that
	 * right now, and also check page cachability so that we
	 * only allow access to cached pages.
	 */
	if (hisprp->p_type == PT_GR) {
		pde_t *hisPDEptr = pmap_pte(targpas, hisprp->p_pmap,
					(caddr_t)uiop->uio_offset, 0);
		if (pnum(uiop->uio_offset) != 
				pnum(uiop->uio_offset + uiop->uio_resid))
			uiop->uio_resid = (NBPP - poff(uiop->uio_offset));
		if (pg_cachemode(hisPDEptr) != PG_COHRNT_EXLWR) {
			error = EIO;
			goto skip;
		}
	}

	/*
	 * Since we found a region, the address is valid and
	 * points into the region.  No need to check maxaddr,
	 * since the addressed word lies at least partly within
	 * the region. See if we can do a cheap case:
	 */
	if (pnum(uiop->uio_offset) == pnum(uiop->uio_offset + uiop->uio_resid)
	    && !(uiop->uio_offset & 3)) {
		pde_t *myPDEptr;
                pfd_t* pfd;

		/* get his PTE for it: */
		myPDEptr = pmap_pte(targpas, hisprp->p_pmap, 
				   (caddr_t)uiop->uio_offset, 0);

		/* if the PDE is valid and the page is in core,
		 * just DO it. Since the region is locked, the
		 * page isn't going anywhere
		 */
		if (!pg_isvalid(myPDEptr))
			goto do_attach;
		if (rw == UIO_WRITE && !pg_ismod(myPDEptr))
			goto do_attach;


                /*
                 * Hold page against migration
                 */
                pfd = pdetopfdat_hold(myPDEptr);  

		if (pfd == NULL) goto do_attach;
#if SN0
		/*
		 * If it's a fetchop page, treat it like physio
		 */
		if (pg_isfetchop(myPDEptr)) {
			regrele(hisrp);
       		        pfdat_release(pfd);
			goto prusrio_physio;
		}
#endif
		/* unlock our aspace lock now since we may fault */
		VAS_UNLOCK(curvasid);

                /* get the page into our space: */
#if R4000
		/*
		 * To avoid aliasing, allocate with a color that is 
		 * the same as the corresponding color of target 
		 * process' address.
		 */
		vaddr = page_mapin(pfd, VM_VACOLOR, colorof(uiop->uio_offset));
#else
		vaddr = page_mapin(pfd, 0, 0);
#endif
		if (!vaddr) {	/* Make sure vaddr is non-zero */
			regrele(hisrp);
			if (!rw_self)
				VAS_UNLOCK(targvasid);
                        pfdat_release(pfd);
			as_rele(targvasid);
			return EIO;
		}

#ifdef TILE_DATA
		/*
		 * Locking the region is not enough to keep pageevict()
		 * at bay.  Increment the page use count in case we fault
		 * in uiomove below.
		 */
		pageuseinc(pdetopfdat(myPDEptr));
#endif
		/* add the page offset to get the address we want: */
		vaddr += poff(uiop->uio_offset);

		flush_count = uiop->uio_resid;	/* uiomove changes uio_resid */
		error = uiomove(vaddr, uiop->uio_resid, rw, uiop);
#ifdef TILE_DATA
		pagefree(pdetopfdat(myPDEptr));
#endif
		/*
		 * flush out cache (for target process) on write
		 * Since the address we're using hits at a single place in
		 * the cache regardless of which address we use, it doesn't 
		 * really matter what address we use, or which address space
		 * we're in.
		 */
		if (rw == UIO_WRITE) {
			prusrio_cachop(targpas, vaddr, flush_count,
				CACH_IO_COHERENCY|CACH_ICACHE_COHERENCY);
		}

		/* unlock region since we're done with it */
		regrele(hisrp);

		page_mapout(vaddr);

		if (!rw_self)
			VAS_UNLOCK(targvasid);

                pfdat_release(pfd);
		as_rele(targvasid);
		return error;
	}

do_attach:
	vaddr = pas_allocaddr(pas, ppas, hisprp->p_pglen, hisrp->r_color);
	if (vaddr == NULL)
		error = ENOMEM;
	else {
		/*
		 * Note that we can just attach with any permissions since ALL
		 * regions have gone through VOP_ACCESS()/getprivatespace
		 * which makes sure that any regions that were read-only
		 * text regions have been made writable.
		 * Note that we may be writing a mmap file that
		 * is mapped shared - in this case we will NOT have an
		 * anon backing..
		 */
                pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
		error = vattachreg(hisrp, pas, ppas, vaddr, hisprp->p_offset,
				hisprp->p_pglen,
				hisprp->p_type,
				((rw == UIO_READ) ? PROT_R : PROT_RW) &
					hisprp->p_maxprots,
				hisprp->p_maxprots, PF_DUP, pm, &myprp);
                aspm_releasepm(pm);
	}
	
	if (error) goto skip;
	maxaddr = hisregva + ctob(hisprp->p_pglen);
	if ((u_long)(uiop->uio_offset + uiop->uio_resid) >= (u_long)maxaddr)
		uiop->uio_resid = maxaddr - (caddr_t)uiop->uio_offset;
	notmoved -= uiop->uio_resid;

	addr = myprp->p_regva + ((u_long)uiop->uio_offset - (u_long)hisregva);

	/*
	 * Make sure that for locked COW instruction pages, the writer
	 * does not incur COWs - vgetprivatespace has already guaranteed
	 * that all such locked pages are private.
	 */
	attr = findattr(hisprp, (char *)uiop->uio_offset);
	end = (char *)(uiop->uio_offset + uiop->uio_resid);
	an = hisprp->p_reg->r_anon;
	if ((rw == UIO_WRITE) && (an) && (hisprp->p_maxprots & PROT_W)) {
		/*
		 * If we are text, guaranteed that prior vgetprivatespace 
		 * made us anon, or we were already anon due to a prior 
		 * mprotect (write+).
		 * We only want to do this special work if we have perms
		 * to write to the page. If not, we will fail later in
		 * the uiomove.
		 */
		extern int	copy_locked_page(preg_t *, attr_t *, preg_t *,
				caddr_t, pde_t *, pde_t *, pas_t *);
		pde_t		*hispte;
		pde_t		*mypte;

		while (attr) {
			char *hisaddr=(char*)uiop->uio_offset>attr->attr_start?
				(char *)uiop->uio_offset : attr->attr_start;
			char *myaddr = addr + 
				((u_long)hisaddr - (u_long)uiop->uio_offset);

			if (attr->attr_start >= end)
				break;

			if (attr->attr_lockcnt) {
				char	*limit = end > attr->attr_end ?
						attr->attr_end : end;
				pgno_t	size = btoc(limit - hisaddr);

				ASSERT(size);
				if (PMAT_GET_PAGESIZE(attr) > NBPP)
					pmap_downgrade_addr_boundary(targpas,
						hisprp, hisaddr, size,
						PMAP_TLB_FLUSH);
				while (size) {
					pgno_t	sz = size;

					hispte = pmap_ptes(targpas, 
						hisprp->p_pmap, hisaddr, &sz, 
								VM_NOSLEEP);
					ASSERT(hispte && sz);
					mypte = pmap_ptes(pas, myprp->p_pmap,
					    myaddr, &sz, VM_NOSLEEP);
					if (mypte == NULL) {
						regrele(hisrp);
						setsxbrk();
						reglock(hisrp);
						continue;
					}
					ASSERT(sz);
					size -= sz;
					/*
					 * Don't need any migration protection
					 * since the pages are already locked.
					 */
					for (; --sz >= 0; hispte++, mypte++, 
							hisaddr += NBPP) {
						pfd_t *pfd = pdetopfdat(hispte);

						/*
						 * Check if the page has
						 * already been privatized.
						 * On error, fail the write,
						 * but keep the new region
						 * with all its pages locked.
						 */
						if (pfd->pf_tag != an) {
							error=copy_locked_page(
							hisprp, attr, hisprp,
							hisaddr, hispte, hispte,
							targpas);
							if (error) goto skip;
							pfd=pdetopfdat(hispte);
						}
						VPAG_UPDATE_RMAP_ADDMAP_RET(PAS_TO_VPAGG(pas), JOBRSS_INC_FOR_PFD, pfd, mypte, pm, error);
						if (error) {
							error = ENOMEM;
							goto skip;
						}
						pg_setpgi(mypte, 
						     pg_getpgi(hispte));
						pg_setmod(mypte);
						myprp->p_nvalid++;
						pageuseinc(pfd);
					}
				}
			}

			attr = attr->attr_next;
		}

	}

	if (error) {
skip:
		if (myprp) 
			vdetachreg(myprp, pas, ppas, myprp->p_regva, 
							myprp->p_pglen, 0);
		regrele(hisrp);
		prusrio_unlock(curvasid, targvasid, rw_self);
		as_rele(targvasid);
		return error;
	}

	/*
	 * release our aspacelock and region lock so can fault
	 */
	regrele(hisrp);
	VAS_UNLOCK(curvasid);

	/*
	 * make sure target cannot access this address space anymore
	 * Also, invalidate the page so the target will have to re-vfault.
	 * This is since during a write, the page will be potentially
	 * stolen and replaced with a new page. We need to be sure that
	 * the target process doesn't run with the old page
	 * This only matters for writing to processes other than ourselves.
	 */
	if (rw == UIO_WRITE && !rw_self)
		invalidateaddr(targpas, hisprp, (uvaddr_t)uiop->uio_offset,
							uiop->uio_resid);

	/* in case we have watchpoints in us! declare these as uninteresting */
	if (curuthread->ut_watch)
		curuthread->ut_watch->pw_flag |= WIGN;

	flush_count = uiop->uio_resid;

	/* uiop->uio_iov->iov_base is always destination */
	if (rw == UIO_WRITE) {
		a = uiop->uio_iov->iov_base;	/* copying from host */
		uiop->uio_iov->iov_base = addr;	/* copying to target */
	} else {
		a = addr;
	}
	vaddr = kmem_alloc(USRIOSIZE, KM_SLEEP);
	while (uiop->uio_resid > 0) {
		count = MIN(uiop->uio_resid, USRIOSIZE);
#ifdef CELL_IRIX
		if ((!IS_KUSEG(a)) && level) 	/* only for UIO_WRITE */
			bcopy(a, vaddr, count);
		else
#endif
		if (copyin(a, vaddr, count)) {
			error = EFAULT;
			break;
		}
		/*
		 * Uiomove honors segflg and bumps base, offset, count
		 * But when we do a B_WRITE, we are trying to write
		 * TO uiop->uio_iov->iov_base which has been set to the
		 * target process' address space.  Therefore in uiomove
		 * terms it is a UIO_READ into the process' space.
		 */
		error = uiomove(vaddr, count, UIO_READ, uiop);
		if (error)
			break;
		a += count;
	}
	/*
         * flush everyone's icache to make sure new instrs get
	 * picked up.
	 * Note that flushing it in current procs space will also
	 * cause it to be flushed in target procs space since 
	 * both addresses hit at the same place in the caches.
	 *
	 * If we write text space here, we need to flush the 
	 * primary data cache to secondary.  This needs to be 
	 * done AFTER the data is actually written on machines 
	 * with writeback caches.
	 */
	if (rw == UIO_WRITE) {
		prusrio_cachop(targpas, addr, flush_count,
				CACH_ICACHE_COHERENCY);
	}
	kmem_free(vaddr, USRIOSIZE);
	uiop->uio_resid += notmoved;

	/*
	 * while copying did we hit any watchpoints, if so we need to
	 * replant them
	 */
	clr3rdparty(ut, targvasid);
	if (curuthread->ut_watch)
		curuthread->ut_watch->pw_flag &= ~WIGN;

	/* now its ok for target to run - release its aspacelock */
	if (!rw_self)
		VAS_UNLOCK(targvasid);
	as_rele(targvasid);

	mrupdate(&pas->pas_aspacelock);
	reglock(myprp->p_reg);
	vdetachreg(myprp, pas, ppas, myprp->p_regva, myprp->p_pglen, 0);
	mrunlock(&pas->pas_aspacelock);

	return error;
}

typedef struct irix5_prio {
	off64_t		pi_offset;
	app32_ptr_t	pi_base;
	int		pi_len;
} irix5_prio_t;

/* ARGSUSED */
int
irix5_to_prio(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_prio, prio);

	target->pi_offset = source->pi_offset;
	target->pi_base = (void *)(long)source->pi_base;
	target->pi_len = source->pi_len;

	return (0);
}

int
prthreadio(proc_t *p, uthread_t *ut, enum uio_rw rw, prio_t *prio)
{
	uio_t uio;
	struct iovec aiov;

	aiov.iov_base = prio->pi_base;
	aiov.iov_len = prio->pi_len;

	uio.uio_iov = &aiov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = prio->pi_offset;
	uio.uio_resid = prio->pi_len;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_fp = 0;

	return (prusrio(p, ut, rw, &uio, 0));
}
