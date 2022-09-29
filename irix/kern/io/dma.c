/* Copyright 1986-1996 Silicon Graphics Inc., Mountain View, CA. */
/*
 * Dma utilities.
 */

#ident "$Revision: 3.67 $"

#include "sys/types.h"
#include "ksys/as.h"
#include "sys/debug.h"
#include "sys/param.h"
#include "sys/buf.h"
#include "sys/cmn_err.h"
#include "sys/proc.h"
#include "sys/errno.h"
#include "sys/sysmacros.h"
#include "sys/var.h"
#include "sys/systm.h"
#include "sys/pfdat.h"
#include "ksys/vproc.h"

#ifdef EVEREST
STATIC void
dma_unaligned_setup(
	caddr_t		baddr,
	size_t		count,
	caddr_t		*first_kva,		    
	caddr_t		*last_kva);

STATIC void
dma_unaligned_copy(
	dma_info_t	*dmap,
	caddr_t		baddr,
	size_t		count,
	size_t		resid);
#endif

/*
 * Shared process locking - we assume that none of these are called with the
 * aspacelock set.
 */
/*
 * Bits we should turn on to make the virtual address kernel addressable
 * We leave off the No-cache bit so that if you copy to a mapped
 * user address it is automatically flushed from the cache - this happens
 * automatically on physio since userdma is called (except on IP5)
 * but for other stuff it seems like a safe idea.
 * Note that MOST drivers are not even going to USE the kernel virtual
 * address that we give them.
 */
#define BASIC	(PG_G | PG_SV | PG_VR)
#define	GOOD	(BASIC | PG_M)


int
iomap(buf_t *bp)
{
	iovec_t	iov;

	iov.iov_base = bp->b_un.b_addr;
	iov.iov_len = bp->b_bcount;

	return iomap_vector(bp, &iov, 1);
}

/*
 * This code is mostly stolen from v_pg in os/as/ddmap.c. It depends 
 * on the iov's being page aligned which has been checked previously.
 * Currently only works for KSEG0/2.
 */
static void
mapkvector(iovec_t *iov, int iovcnt, caddr_t kaddr)
{
	int 	i, j, numpages;
	caddr_t	from;
	pde_t	*tkpt = kvtokptbl(kaddr);

	for (i = 0; i < iovcnt; i++) {
		numpages = (iov[i].iov_len / NBPP);
		from = iov[i].iov_base;
		if (IS_KSEG2(from)) {
			pde_t *fkpt = kvtokptbl(from);

			for (j = 0; j < numpages; j++) {
				*tkpt++ = *fkpt++;
			}
		} else if (IS_KSEG0(from)) {
			pgi_t	bits = PG_VR | PG_SV | PG_M;

			for (j = 0; j < numpages; j++) {
				pg_setpgi(tkpt, mkpde(bits, 
						btoct(svirtophys(from))));
				pg_setcachable(tkpt);
				tkpt++;
				from += NBPP;
			}
		}
	}
}

/*
 * Allocate space in the system segment page table.  Copy the physical page
 * numbers from the segments underlying the buffer into the system segment
 * page table.  Setup b_dmaaddr to point to the system segment where the
 * buffer is now mapped.  Ensure that the memory in the system segment is
 * accessible read/write by the kernel so as to allow the drivers to copy
 * data in and out if they need to.
 */
int
iomap_vector(buf_t *bp, iovec_t	*iov, int iovcnt)
{
	caddr_t		to;
	caddr_t		baddr;
	caddr_t		kaddr;
	int		pages;
	long		offset;
	ulong		ptebits;
	k_sigset_t	set;
	int		i, kvector = 0;
	vasid_t		vasid;
#ifdef EVEREST
	dma_info_t	*dmap;
	caddr_t		first_kva;
	caddr_t		last_kva;
	int		first_aligned;
	int		last_aligned;
#endif

	ASSERT(bp->b_bcount);
	ASSERT(!(bp->b_flags & B_SWAP));

	baddr = bp->b_un.b_addr;
	offset = poff(baddr);
	pages = numpages(baddr, bp->b_bcount);

	ASSERT(pages <= v.v_maxdmasz);  /* caller *should* have checked */

#ifdef DEBUG
	if (iovcnt > 1) {
		/*
		 * For I/Os with multiple vectors, all of the buffers
		 * must be page aligned. biophysio already checked, &
		 * iomap calls this with iovcnt == 1.
		 */
		for (i = 0; i < iovcnt; i++) {
			if (((__psunsigned_t)(iov[i].iov_base) & (NBPP - 1)) ||
			    (iov[i].iov_len & (NBPP - 1))) {
				return EINVAL;
			}
		}
	}
#endif

	/*
	 * If buffer cache is already mapped into kernel space,
	 * there is nothing to do.  This assumes that drivers
	 * understand how to decompose K[01]SEG addresses. For
	 * io vectors, we try to do something.
	 */
	if (!IS_KUSEG(baddr)) {
		if (iovcnt == 1)
			return 0;
		else
			kvector = 1;
	}

#ifndef EVEREST
	if (pages == 1) {
		auto pfn_t pfn;

		/*
		 * We don't have to worry about multiple I/O vectors
		 * in this case, because when there are multiple I/O
		 * vectors they are required to all be page aligned.
		 * Thus we could never end up with pages == 1 and
		 * more than one I/O vector.
		 */
		ASSERT(iovcnt == 1);
		if (vtop(baddr, bp->b_bcount, &pfn, 0) == 0) {
			/* error */
			return EFAULT;
		}
		/* pfn must be mappable in K0 for this optimization to work */
		if (small_K0_pfn(pfn)
#ifdef _VCE_AVOIDANCE
			&& (!vce_avoidance || vcache_color(pfn) ==
				pfd_to_vcolor(pfntopfdat(pfn)))
#endif /* _VCE_AVOIDANCE */
			) {
			bp->b_dmaaddr = (caddr_t)offset +
				PHYS_TO_K0((__psunsigned_t)pfn << PNUMSHFT);
			return 0;
		}
	}
#endif /* EVEREST */

	hold_nonfatalsig(&set);
	if ((kaddr =
#ifdef _VCE_AVOIDANCE
		vce_avoidance ?
			kvalloc(pages, VM_BREAKABLE|VM_VACOLOR, colorof(baddr))
			:
#endif /* _VCE_AVOIDANCE */
			kvalloc(pages, VM_BREAKABLE, 0))
	    == NULL)
	{
		release_nonfatalsig(&set);
		return EAGAIN;
	}
	release_nonfatalsig(&set);

	bp->b_dmaaddr = offset + kaddr;
	to = bp->b_dmaaddr;

	/* try to make sure this isn't called at interrupt time */
	ASSERT(!KT_CUR_ISXTHREAD());

	if (kvector == 0) {
		/*
	 	 * Copy pte info from user page tables to system page table.
	 	 */
		as_lookup_current(&vasid);
		ptebits = GOOD | pte_cachebits();
		for (i = 0; i < iovcnt; i++) {
			maputokptbl(vasid, iov[i].iov_base, iov[i].iov_len, 
					to, ptebits);
			to += iov[i].iov_len; 
		}
	} else {
		/*
		 * Copy pte info from kernel page table.
		 * In this case, we are guaranteed not to execute the
		 * foll EVEREST workaround. The rule is that all the
		 * kernel vector pages must have been validated 
		 * before the call. Only understand K0 and K2.
		 */
		mapkvector(iov, iovcnt, kaddr);
	}

#ifdef EVEREST
	if (io4ia_war && io4ia_userdma_war &&
	    (bp->b_flags & B_READ)) {
		first_aligned = SCACHE_ALIGNED(offset + kaddr);
		last_aligned = SCACHE_ALIGNED(offset + kaddr +
					      bp->b_bcount);
		/*
		 * We don't have to worry about having more than one
		 * iovec here, because when there are multiple I/O
		 * vectors the buffers are required to be page aligned.
		 */
		if (!first_aligned || !last_aligned) {
			ASSERT(kvector == 0);
			dma_unaligned_setup(offset + kaddr, bp->b_bcount,
					    &first_kva, &last_kva);
			/*
			 * Update the dma_info structure for this request
			 * and insert it in the kernel address indexed
			 * table so that we can find it in iounmap().
			 */
			dmap = dma_info_lookup_user(current_pid(), baddr,
						    bp->b_bcount, 0);
			ASSERT(dmap != NULL);
			dmap->dma_firstkva = first_kva;
			dmap->dma_lastkva = last_kva;
			dma_info_insert_kern(dmap, offset + kaddr);
		}
	}
#endif
	bp->b_flags |= B_MAPPED;
	return 0;
}


/*
 * Release mapping resources assigned above.
 */
void
iounmap(buf_t *bp)
{
	long		offset;
	long		pages;
#ifdef EVEREST
	dma_info_t	*dmap;
	int		first_aligned;
	int		last_aligned;
#endif

	/*
	 * If buffer has no dma resources, just return.  This happens
	 * when buffers have errors in them.
	 */
	if (bp->b_dmaaddr == 0) {
		return;
	}
	offset = ((long) bp->b_dmaaddr) & (NBPC - 1);
	pages = btoc(offset + bp->b_bcount);

	/*
	 * If buffer wasn't mapped there's nothing to do.
	 */
	if (bp->b_flags & B_MAPPED) {
#ifdef EVEREST
		if (io4ia_war && io4ia_userdma_war &&
		    (bp->b_flags & B_READ)) {
			first_aligned = SCACHE_ALIGNED(bp->b_dmaaddr);
			last_aligned = SCACHE_ALIGNED(bp->b_dmaaddr +
						      bp->b_bcount);
			if (!first_aligned || !last_aligned) {
				dmap = dma_info_lookup_kern(bp->b_dmaaddr,
							    bp->b_bcount, 1);
				ASSERT(dmap != NULL);
				ASSERT(dmap->dma_proc == current_pid());
				dma_unaligned_copy(dmap, bp->b_dmaaddr,
						   bp->b_bcount,
						   bp->b_resid);
			}
		}
#endif
		/*
		 * Release space back into sptmap.
		 * offset is dropped into bit bucket by kvfree()
		 */
		ASSERT((((long) bp->b_un.b_addr) & (NBPC - 1)) == offset);
		kvfree(bp->b_dmaaddr, pages);
		bp->b_flags &= ~B_MAPPED;
	}
	bp->b_dmaaddr = 0;
}

#ifdef MH_R10000_SPECULATION_WAR
caddr_t _maputokv(caddr_t, size_t, int);

caddr_t
maputokv(caddr_t uvaddr, size_t count, register int perm) 
{ 
	perm &= (PG_CACHMASK|PG_M);
	return(_maputokv(uvaddr, count, perm | BASIC));
}

/*
 * Map a range of user virtual addresses to kernel virtual addresses.
 * Used by drivers that want to copy to locked down pages at interrupt time
 * that don't have access to a buffer header
 * Returns NULL on error, else kernel virtual address.
 */
caddr_t
_maputokv(caddr_t uvaddr, size_t count, register int perm)
#else
caddr_t
maputokv(caddr_t uvaddr, size_t count, register int perm)
#endif /* MH_R10000_SPECULATION_WAR */
{
	register pgcnt_t pages;
	register caddr_t kvaddr;
	k_sigset_t set;
	vasid_t vasid;
#ifdef EVEREST
	dma_info_t	*dmap;
	int		first_aligned;
	int		last_aligned;
	caddr_t		first_kva;
	caddr_t		last_kva;
#endif

	ASSERT(IS_KUSEG(uvaddr));

	pages = numpages(uvaddr, count);
#ifndef MH_R10000_SPECULATION_WAR
	perm &= (PG_CACHMASK|PG_M);	/* caller can only set these */
#endif /* MH_R10000_SPECULATION_WAR */
#ifdef R4000
	ASSERT((perm & PG_CACHMASK) == PG_UNCACHED || \
		(perm & PG_CACHMASK) == PG_NONCOHRNT || \
		(perm & PG_CACHMASK) == PG_COHRNT_EXLWR );
#endif

	/*
	 * Allocate system segment space
	 */
	hold_nonfatalsig(&set);
#if R4000
	/*
	 * Since allocating correctly-colored virtual space
	 * is cheap, and since it saves many VCEs, we'll do
	 * it for the performance gain.
	 */
	kvaddr = kvalloc(pages, VM_BREAKABLE|VM_VACOLOR, colorof(uvaddr));
#else
	kvaddr = kvalloc(pages, VM_BREAKABLE, 0);
#endif
	if (kvaddr != NULL) {
		kvaddr += poff(uvaddr);

		/*
		 * Physical I/O to/from current process.
		 */
		as_lookup_current(&vasid);
		maputokptbl(vasid, uvaddr, count, kvaddr, perm
#ifndef MH_R10000_SPECULATION_WAR
	 	      | BASIC
#endif /* MH_R10000_SPECULATION_WAR */
		           );
#ifdef EVEREST
		if (io4ia_war && io4ia_userdma_war) {
			first_aligned = SCACHE_ALIGNED(uvaddr);
			last_aligned = SCACHE_ALIGNED(uvaddr + count);		
			if (!first_aligned || !last_aligned) {
				dmap = dma_info_lookup_user(current_pid(), 
							    uvaddr,
							    count, 0);
				if (dmap != NULL) {
					dma_unaligned_setup(kvaddr, count,
							    &first_kva,
							    &last_kva);
					dmap->dma_firstkva = first_kva;
					dmap->dma_lastkva = last_kva;
					dma_info_insert_kern(dmap, kvaddr);
				}
			}
		}
#endif
	}

	release_nonfatalsig(&set);
	return(kvaddr);
}

/*
 * Unmap the request
 */
void
unmaputokv(caddr_t kvaddr, size_t count)
{
#ifdef EVEREST
	int		first_aligned;
	int		last_aligned;
	dma_info_t	*dmap;

	if (io4ia_war && io4ia_userdma_war) {
		first_aligned = SCACHE_ALIGNED(kvaddr);
		last_aligned = SCACHE_ALIGNED(kvaddr + count);		
		if (!first_aligned || !last_aligned) {
			dmap = dma_info_lookup_kern(kvaddr, count, 1);
			if (dmap != NULL) {
				ASSERT(dmap->dma_proc == current_pid());
				dma_unaligned_copy(dmap, kvaddr, count, 0);
			}
		}
	}
#endif

	ASSERT(IS_KSEG2(kvaddr));
	kvfree(kvaddr, numpages(kvaddr, count));
}

#ifdef EVEREST

STATIC void
dma_unaligned_setup(
	caddr_t		baddr,
	size_t		count,
	caddr_t		*first_kva,		    
	caddr_t		*last_kva)
{
	int	first_aligned;
	int	last_aligned;

	*first_kva = NULL;
	*last_kva = NULL;

	first_aligned = SCACHE_ALIGNED(baddr);
	last_aligned = SCACHE_ALIGNED(baddr + count);
	if (!first_aligned || !last_aligned) {
		/* 
		 * Don't try to handle alignment on both ends
		 * of the buffer if the whole thing fits on 1 page.
		 */
		if (numpages(baddr, count) == 1) {
			first_aligned = 0;
			last_aligned = 1;
		}
		if (!first_aligned) {
			pde_t	*pde;

			/*
			 * Replace the first page of the kernel
			 * buffer with a newly-allocated page.
			 */
			pde = kvtokptbl(baddr);
			/* Any physical page is OK. Won't use VA. */
			*first_kva = kvpalloc(1, 0, 0);
			pde->pte.pg_pfn = kvatopfn(*first_kva);
		}
		if (!last_aligned) {
			pde_t	*pde;

			/*
			 * Replace the last page of the kernel
			 * buffer with a newly-allocated page.
			 */
			pde = kvtokptbl(baddr + count - 1);
			/* Any physical page is OK. Won't use VA. */
			*last_kva = kvpalloc(1, 0, 0);
			pde->pte.pg_pfn = kvatopfn(*last_kva);
		}
	}
}


STATIC void
dma_unaligned_copy(
	dma_info_t	*dmap,
	caddr_t		baddr,
	size_t		count,
	size_t		resid)
{
	ssize_t		copy_size;
	ssize_t		copy_offset;
	int		first_aligned;
	int		last_aligned;

	ASSERT(count == dmap->dma_count);

	first_aligned = SCACHE_ALIGNED(baddr);
	last_aligned = SCACHE_ALIGNED(baddr + count);

	if (first_aligned && last_aligned) {
		return;
	}

	/* 
	 * Don't try to handle alignment on both ends
	 * of the buffer if the whole thing fits on 1 page.
	 */
	if (numpages(baddr, count) == 1) {
		first_aligned = 0;
		last_aligned = 1;
	}
	if (!first_aligned) {
		/*
		 * Copy the minimum of the number of bytes in the
		 * first page, the total number of bytes requested,
		 * and the total number of bytes actually
		 * transferred.
		 */
		copy_size = MIN((ctob(btoc(baddr+1)) -
				 (__psunsigned_t)baddr),
				count);
		copy_size = MIN(copy_size, count - resid);
		copyout(baddr, dmap->dma_uaddr, copy_size);
		ASSERT(dmap->dma_firstkva != NULL);
		kvpfree(dmap->dma_firstkva, 1);
	}
	if (!last_aligned) {
		/*
		 * Copy the minimum of the number of bytes in the
		 * last page and the number of those bytes actually
		 * transferred.
		 */
		copy_size = ((__psunsigned_t)baddr + count) & (NBPC - 1);
		copy_offset = count - copy_size;
		ASSERT(copy_offset >= 0);
		copy_size = MIN(copy_size, (count - resid - copy_offset));
		if (copy_size > 0) {
			copyout(baddr + copy_offset,
				dmap->dma_uaddr + copy_offset,
				copy_size);
		}
		ASSERT(dmap->dma_lastkva != NULL);
		kvpfree(dmap->dma_lastkva, 1);
	}
}
#endif /* EVEREST */
