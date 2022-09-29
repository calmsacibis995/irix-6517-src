/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
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

#ident	"$Revision: 3.117 $"

#include <sys/types.h>
#include <sys/alenlist.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/pfdat.h>
#include <sys/sema.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/atomic_ops.h>
#include <sys/kthread.h>
#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif
#include <sys/lio.h>	/* for KLISTIO */
#include <sys/kaio.h>
#include <ksys/vproc.h>
#include <sys/nodepda.h>

static void
biophysio_undo_userdma(
	iovec_t		*iov,
	int		iovcnt,
	int		flags,
	opaque_t	*cookies);

/*
 * Bits we should turn on to make the virtual address kernel addressable
 */
#define	GOOD	(PG_G | PG_M | PG_SV | PG_VR)
#ifdef MH_R10000_SPECULATION_WAR
#define       GOOD_R10000     (PG_G | PG_M | PG_SV)
#endif /* MH_R10000_SPECULATION_WAR */

extern int cachewrback;

#ifdef MH_R10000_SPECULATION_WAR
extern caddr_t _maputokv(caddr_t, size_t, int);
extern int is_in_pfdat(pgno_t);
#endif /* MH_R10000_SPECULATION_WAR */


/*
 * Raw I/O.
 */
int
uiophysio(
	int	(*strat)(buf_t *),
	buf_t	*bp,
	dev_t	dev,
	uint64_t flags,
	uio_t	*uio)
{
	return biophysio(strat, bp, dev, flags,
			 uio->uio_offset >> SCTRSHFT, uio);
}

#ifdef MH_R10000_SPECULATION_WAR
void
changePermBits(void *kvaddr, size_t count, register int permbits) {
        int     i;
        pde_t   *pde;
        pgcnt_t npgs;

        /* Do we need this? */
#ifdef R4000
        ASSERT((permbits & PG_CACHMASK) == PG_UNCACHED || \
                (permbits & PG_CACHMASK) == PG_NONCOHRNT || \
                (permbits & PG_CACHMASK) == PG_COHRNT_EXLWR );
#endif

        permbits &= (PG_CACHMASK|PG_M);     /* caller can only set these */
        npgs = numpages(kvaddr, count);
        pde = kvtokptbl(kvaddr);

        ASSERT(pde);
        /* change attributes in list of pdes's */
        for (i=0; i < npgs; i++, pde++) {
                ASSERT(pg_isvalid(pde));
                ASSERT(is_in_pfdat(pdetopfn(pde))); 
                ASSERT(iskvirpte(pde));
                pg_setpgi(pde, mkpde(permbits, pg_getpfn(pde)));
        }
}
#endif /* MH_R10000_SPECULATION_WAR */

/* ARGSUSED */
int
biophysio(
	int	(*strat)(buf_t *),
	buf_t	*bp,
	dev_t	dev,
	uint64_t flags,
	daddr_t	blkno,
	uio_t	*uio)
{
	iovec_t			*iov = uio->uio_iov;
	int			iovcnt = uio->uio_iovcnt;
	register size_t		count = iov->iov_len;
	register caddr_t	base = iov->iov_base;
	register int		hpf;
	register int		error = 0;
	opaque_t		cookies[16], *cookiesp;
	int			i;
	int			iov_adjust;

	/*
	 * These are used only on machines that have a writeback cache.
	 */
	int			first_aligned, last_aligned;
	caddr_t			first_kva, last_kva;
	__psunsigned_t		lead_size, trail_size, trail_offset;
	caddr_t			safebuf_base;
	/*KERNEL_ASYNCIO*/
        kaiocb_t                *kaiocb;
        int                     do_kaio = 0;

	/*
	 * readv and writev are only allowed to work through here
	 * with fully page aligned buffers.  Also, we can only
	 * handle up to _MAX_IOVEC individual buffers.
	 */
	if (iovcnt > 1) {
		ASSERT(!(flags & B_MAPUSER));
		ASSERT(uio->uio_pio == 0);
		if (iovcnt > _MAX_IOVEC) {
			return EINVAL;
		}
		count = 0;
		for (i = 0; i < iovcnt; i++) {
			if (((__psunsigned_t)(iov[i].iov_base) & (NBPP - 1)) ||
			    (iov[i].iov_len & (NBPP - 1))) {
				return EINVAL;
			}
			count += iov[i].iov_len;
		}
	}

	/*
	 * MAPUSER not allowed when partial cachelines are involved
	 *
	 * I want alignment restrictions to be consistent across all
	 * architechtures, so we can't just use scache_linemask().  On the
	 * other hand, I can't enforce more restriction than cache alignment,
	 * so I'm stuck at 0x7F.
	 */
	if (((__psunsigned_t) base & 0x7F) ||
	    (do_kaio = (uio->uio_pio == KAIO)))
		flags &= ~B_MAPUSER;

	/*
	 * If parallel I/O, we don't allow bp to be provided, and all I/O
	 * must be cache aligned to simplify cleanup.
	 */
	if (uio->uio_pio &&
	    (bp != NULL || ((__psunsigned_t) base & 0x7F) || (count & 0x7F)))
		return EINVAL;

	/*
	 * Check to see that max DMA length is not exceeded.
	 * This even works for the iovcnt > 1 case since all
	 * buffers are required to be page aligned in that case.
	 */
	if (numpages(base, count) > v.v_maxdmasz)
		return ENOMEM;

	/*
	 * Check that a user process has not passed an
	 * address that looks like a kernel address.
	 * If ok, lock it into place.
	 */
	if (uio->uio_segflg == UIO_USERSPACE) {
		for (i = 0; i < iovcnt; i++) {
			if ((!IS_KUSEG(iov[i].iov_base)) ||
			    (!IS_KUSEG((char *)iov[i].iov_base +
				       iov[i].iov_len - 1))) {
				return EFAULT;
			}
		}
	}

	/*
	 * Make sure cookies vector is big enough.
	 */
	if (iovcnt > sizeof(cookies) / sizeof(cookies[0]))
		cookiesp = kmem_alloc(iovcnt * sizeof(cookies[0]), KM_SLEEP);
	else
		cookiesp = cookies;

	/*
	 * fast_userdma()/fast_userundma() requires
	 * same-process context
	 */
	for (i = 0; i < iovcnt; i++) {
		if (error = fast_userdma(iov[i].iov_base, iov[i].iov_len,
					 flags & B_READ, &cookiesp[i])) {
			biophysio_undo_userdma(iov, i, flags, cookiesp);
			cmn_err_tag(1789, CE_NOTE, 
				"[biophysio]: Process [%s] pid %d Failed userdma %s, error code %d",
					get_current_name(), current_pid(),
					(flags & B_READ) ? "Read" : "Write",
					error);
			goto done;
		}
	}

	/* Some machines (such as IP17) require that DMA reads into 
	 * memory be done to buffers that are cache-aligned.  DMA can
	 * still be done to non-aligned buffers, provided that 
	 * nothing accesses the cache lines during the operation.
	 *
	 * If a user attempts a raw I/O read into cache-misaligned 
	 * buffers, we actually do the read to a kernel buffer (that 
	 * will not be touched during the operation) and then copy 
	 * the data to user space.  Since we want to avoid allocating
	 * and copying potentially large kernel buffers for raw I/O, 
	 * most of the physical pages for the operation are actually 
	 * the user's page frames.  Only the first and/or last pages 
	 * are kernel pages which require copying.
	 *
	 * This code works only for USER-misaligned buffers.  For the
	 * most part, drivers don't do DMA via physio.  When they do,
	 * they must align properly or guarantee that the cache line
	 * they are DMAing into is not accessed during the DMA.
	 *
	 * For calls with multiple I/O vectors, we require all of the
	 * involved buffers to be page aligned.  Thus, the code below
	 * dealing with misaligned buffers does not have to worry about
	 * the multiple I/O vector case.
	 */
	if ((flags & B_READ) && cachewrback) {
#ifdef MH_R10000_SPECULATION_WAR
 		/* MH R10000 WAR requires I/O buffers be page-aligned */
 		first_aligned = IS_R10000() ?
 					poff(base) == 0 :
 					SCACHE_ALIGNED(base);
 		last_aligned = IS_R10000() ?
 					poff(base + count) == 0 :
 					SCACHE_ALIGNED(base + count);
#else /* MH_R10000_SPECULATION_WAR */
		first_aligned = SCACHE_ALIGNED(base);
		last_aligned = SCACHE_ALIGNED(base + count);
#endif /* MH_R10000_SPECULATION_WAR */
	} else {
		first_aligned = 1;
		last_aligned = 1;
	}

	if (!first_aligned || !last_aligned) {
		ASSERT(IS_KUSEG(base) || IS_KSEG2(base));
		ASSERT(cachewrback);

		/* Either the beginning or the end of the user's buffer is 
		 * misaligned.  Map the user's buffer into kernel space, so 
		 * we can refer to these pages with a kernel virtual address.
		 *
		 * For R4000 based platforms, it's much faster to let the
		 * buffer be allocated cacheable.  In fact, for EVEREST
		 * platforms it's mandatory since 128 MB of memory can't
		 * be accessed non-cached.
		 */

#ifdef MH_R10000_SPECULATION_WAR
		if (IS_KUSEG(base)) {

			safebuf_base = _maputokv(base, count, 
					pte_cachebits()|(IS_R10000() ? 
					GOOD_R10000 : GOOD));
		} else {
			/* We're dealing with a K2 address, so
			 * we're going to change just the appropiate bits
			 * so we don't incurred into the 
			 * MH_R10000_SPECULATION_WAR  problem.
			 */
			changePermBits(base, count, pte_cachebits()
					|(IS_R10000() ? GOOD_R10000 : GOOD));
			safebuf_base = base;

		}
		
#else
		safebuf_base = maputokv(base, count, pte_cachebits() | GOOD);
#endif
		if (safebuf_base == NULL) {
			fast_undma(base, count, flags & B_READ, &cookiesp[0]);
			error = ENOSPC;
			goto done;
		}

		/* Don't try to handle alignment on both ends of the buffer
		 * if the whole thing fits on 1 page.
		 */
		if (numpages(base, count) == 1) {
			first_aligned = 0;
			last_aligned = 1;
		}

		if (!first_aligned) {
			register pde_t	*pde;

			/* Replace the first page of the kernel buffer
			 * with a newly-allocated page.
			 */
			pde = kvtokptbl(safebuf_base);
			/* Any physical page is OK. Won't use VA. */
			first_kva =
#ifdef _VCE_AVOIDANCE
				    vce_avoidance ?
				    kvpalloc(1, VM_VACOLOR,
						colorof(safebuf_base)) :
#endif
				    kvpalloc(1, 0, 0);
#ifdef MH_R10000_SPECULATION_WAR
                        if (IS_R10000()) {
                                __psint_t vfn = vatovfn(safebuf_base);

                                krpf_remove_reference(pdetopfdat(pde),vfn);
                                krpf_add_reference(kvatopfdat(first_kva),
                                                   vfn);
                        }
#endif
			pg_setpfn(pde, kvatopfn(first_kva));

			/*
			 * Dropping in the new page table entry into
			 * the tlb ensures that there are no entries
			 * with the old pfn value in the tlb.  While
			 * we haven't referenced the address returned
			 * from maputokv(), it could be that the
			 * address would be the second half in a tlb
			 * entry and that we referenced the first
			 * half since allocating the address.  That
			 * would fault into the tlb an entry pointing
			 * to the user's page rather than our newly
			 * allocated page.
			 */
			tlbdropin(0, safebuf_base, pde->pte);
			lead_size = MIN(ctob(btoc(base+1)) - (__psunsigned_t)base, count);
		}
		if (!last_aligned) {
			register pde_t	*pde;

			/* Replace the last page of the kernel buffer
			 * with a newly-allocated page.
			 */
			pde = kvtokptbl(safebuf_base + count - 1);
			/* Any physical page is OK. Won't use VA. */
			last_kva =
#ifdef _VCE_AVOIDANCE
				   vce_avoidance ?
				   (caddr_t)kvpalloc(1, VM_VACOLOR,
					colorof(safebuf_base + count - 1)) :
#endif
				   (caddr_t)kvpalloc(1, 0, 0);
#ifdef MH_R10000_SPECULATION_WAR
                        if (IS_R10000()) {
                                __psint_t vfn = vatovfn(safebuf_base + \
                                                        count - 1);

                                krpf_remove_reference(pdetopfdat(pde),vfn);
                                krpf_add_reference(kvatopfdat(last_kva),
                                                   vfn);
                        }
#endif
			pg_setpfn(pde, kvatopfn(last_kva));
			/*
			 * Dropping in the new page table entry into
			 * the tlb ensures that there are no entries
			 * with the old pfn value in the tlb.  While
			 * we haven't referenced the address returned
			 * from maputokv(), it could be that the end
			 * address would be the first half in a tlb
			 * entry and that we referenced the second
			 * half since allocating the address.  That
			 * would fault into the tlb an entry pointing
			 * to the user's page rather than our newly
			 * allocated page.
			 */
			tlbdropin(0, safebuf_base + count - 1, pde->pte);
			trail_size = ((__psunsigned_t)base + count) & (NBPC-1);
			trail_offset = count - trail_size;
			ASSERT(((__psunsigned_t)(safebuf_base + trail_offset) \
				& (NBPC-1)) == 0);
		}
#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000() && (flags & B_READ))  {
			if (!first_aligned)
				invalidate_range_references(safebuf_base - poff(safebuf_base),
							    4096,
						    	    CACH_DCACHE|CACH_SCACHE|
						    	    CACH_INVAL|CACH_IO_COHERENCY,
						    	    0);
			if (!last_aligned)
				invalidate_range_references(safebuf_base + count - 
							    poff(safebuf_base+count),
							    4096,
						    	    CACH_DCACHE|CACH_SCACHE|
						    	    CACH_INVAL|CACH_IO_COHERENCY,
						    	    0);
		}
#endif /* MH_R10000_SPECULATION_WAR */	
	} else {
		safebuf_base = base;
	}

	ADD_SYSWAIT(physio);

	if (flags & B_READ)
		SYSINFO.phread += count >> SCTRSHFT;
	else
		SYSINFO.phwrite += count >> SCTRSHFT;

	if (hpf = (bp == NULL)) {
		/*
		 * Get a buffer header off of the
		 * free list.
		 */
		bp = getrbuf(KM_SLEEP);
		bp->b_edev = dev;
		bp->b_flags = B_BUSY | B_PHYS | flags;
	} else {
		bp->b_flags &= ~B_ASYNC;
		bp->b_flags |= B_BUSY | B_PHYS | flags; 
	}

	ASSERT(bp->b_edev == dev);
	ASSERT((curthreadp != NULL) && !(KT_CUR_ISXTHREAD()));

	/* KERNEL_ASYNCIO */
	/* first_aligned and last_aligned were verified above */
	if (do_kaio = (uio->uio_pio == KAIO)) {
		kaiocb = (kaiocb_t *)uio->uio_pbuf;
		ASSERT(kaiocb);

		bp->b_fsprivate = kaiocb; /* preserve aio state */
		bp->b_iodone = kaio_done;
		kaiocb->aio_ioflag = flags; /* remember original flags */
		bp->b_flags |= B_ASYNC;
		uio->uio_pio = 0;
		if (hpf)
			kaiocb->aio_tasks |= KAIO_PUTPHYSBUF | KAIO_UNDMA;
		else
			kaiocb->aio_tasks |= KAIO_UNDMA;
		kaiocb->aio_undma_cookie = cookiesp[0];
	} 

	bp->b_error = 0;
	bp->b_un.b_addr = safebuf_base;
	bp->b_blkno = blkno;
	bp->b_bcount = count;

	if (!(flags & B_MAPUSER)) {
		/*
		 * If iomap couldn't get virtual memory,
		 * report back ``errorless'' failure.
		 */
		if (error = iomap_vector(bp, iov, iovcnt)) {
			biophysio_undo_userdma(iov, iovcnt, flags, cookiesp);
			if (!first_aligned || !last_aligned) {
				if (!first_aligned)
					kvpfree(first_kva, 1);
				if (!last_aligned)
					kvpfree(last_kva, 1);
				unmaputokv(safebuf_base, count);
			}
			goto out;
		}

		/*KERNEL_ASYNCIO*/
		if (do_kaio)
			kaiocb->aio_tasks |= KAIO_IOUNMAP;
	}

	(*strat)(bp);

#if	SN0
	sn0_poq_war((void *)bp);
#endif

	/*
	 * We already know from the check above that first_aligned and
	 * last_aligned are true, so there are no alignment pages or mappings
	 * to free.  Code that calls biophysio will do the iounmap and undma.
	 */
	if (uio->uio_pio == KLISTIO) {
		uio->uio_pbuf = bp;
		goto done;
	}

	/*KERNEL_ASYNCIO*/
	if (do_kaio) {
		goto done;
        }

	/* wait for completion */
	psema(&bp->b_iodonesema, PRIBIO|TIMER_SHIFT(AS_PHYSIO_WAIT));

	if (!(flags & B_MAPUSER))
		iounmap(bp);
	if (uio->uio_segflg == UIO_USERSPACE) {
		biophysio_undo_userdma(iov, iovcnt, flags, cookiesp);
	}

	if (!first_aligned || !last_aligned) {
		if (!first_aligned) {
			lead_size = MIN(lead_size, count - bp->b_resid);
			copyout(safebuf_base, base, lead_size);
			kvpfree(first_kva, 1);
		}

		if (!last_aligned) {
			trail_size = MIN(trail_size,
				count - bp->b_resid - trail_offset);
			copyout(safebuf_base + trail_offset, 
				base + trail_offset, trail_size);
			kvpfree(last_kva, 1);
		}

		unmaputokv(safebuf_base, count);
	}

	/*
	 * Adjust the passed in I/O vectors to match the amount
	 * of data transferred.
	 */
	count -= bp->b_resid;
	for (i = 0; i < iovcnt; i++) {
		if (count <= 0) {
			break;
		}
		if (count > iov[i].iov_len) {
			iov_adjust = iov[i].iov_len;
		} else {
			iov_adjust = count;
		}
		iov[i].iov_base = (char *)iov[i].iov_base + iov_adjust;
		uio->uio_offset += iov_adjust;
		uio->uio_resid -= iov_adjust;
		iov[i].iov_len -= iov_adjust;
		count -= iov_adjust;
	}
			
	error = geterror(bp);
out:

	if (hpf) {
		if (BP_ISMAPPED(bp))
			bp_mapout(bp);
		freerbuf(bp);
	}

	SUB_SYSWAIT(physio);
done:
	if (cookiesp != cookies)
		kmem_free(cookiesp, iovcnt * sizeof(cookies[0]));
	return error;
}


/*
 * This is a simple subroutine for biophysio to call fast_undma on
 * a set of iovecs.
 */
static void
biophysio_undo_userdma(
	iovec_t		*iov,
	int		iovcnt,
	int		flags,
	opaque_t	*cookies)
{
	int	i;

	for (i = 0; i < iovcnt; i++) {
		fast_undma(iov[i].iov_base, iov[i].iov_len,
			   flags & B_READ, &cookies[i]);
	}
}
	

/*
 * Convert a buf handle into a Physical Address/Length List.
 */
alenlist_t
buf_to_alenlist(alenlist_t	alenlist,
		buf_t		*bp,
		unsigned	flags)
{
	int created_alenlist;

	if (bp->b_flags & B_PAGEIO) {
		pfd_t *pfd = NULL;
		size_t remaining;
		int offset = poff(bp->b_un.b_addr);

		if (alenlist == NULL) {
			alenlist = alenlist_create(0);
			created_alenlist = 1;
		} else {
			alenlist_clear(alenlist);
			created_alenlist = 0;
		}

		/* Handle first page */
		pfd = getnextpg(bp, pfd);
		if (offset + bp->b_bcount > NBPP) {
			if (alenlist_append(alenlist, pfdattophys(pfd)+offset, 
				NBPP-offset, flags) == ALENLIST_FAILURE)
					goto failure;
			remaining = bp->b_bcount - (NBPP-offset);
		} else {
			if (alenlist_append(alenlist, pfdattophys(pfd)+offset, 
				bp->b_bcount, flags) == ALENLIST_FAILURE)
					goto failure;
			remaining = 0;
		}

		/* Handle middle pages */
		while (remaining >= NBPP) {
			pfd = getnextpg(bp, pfd);
			if (alenlist_append(alenlist, pfdattophys(pfd), 
				NBPP, flags) == ALENLIST_FAILURE)
					goto failure;
			remaining -= NBPP;
		}

		/* Handle last page */
		if (remaining) {
			ASSERT(remaining < NBPP);
			pfd = getnextpg(bp, pfd);
			if (alenlist_append(alenlist, pfdattophys(pfd), 
				remaining, flags) == ALENLIST_FAILURE)
					goto failure;
		}
	} else if (bp->b_flags & B_MAPPED) {
		ASSERT(bp->b_dmaaddr != NULL);
		alenlist = kvaddr_to_alenlist(alenlist, bp->b_dmaaddr, bp->b_bcount, flags);
	} else {
		ASSERT(bp->b_un.b_addr != 0);
		ASSERT(!IS_KUSEG(bp->b_un.b_addr));
		alenlist = kvaddr_to_alenlist(alenlist, bp->b_un.b_addr, bp->b_bcount, flags);
	}

	alenlist_cursor_init(alenlist, 0, NULL);

	return(alenlist);

failure:
	if (created_alenlist)
		alenlist_destroy(alenlist);
	return(NULL);
}
