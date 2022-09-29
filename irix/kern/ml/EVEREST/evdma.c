/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.59 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/pfdat.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/dmamap.h>
#include <sys/sbd.h>
#include <sys/pio.h>
#include <sys/var.h>
#include <sys/iotlb.h>
#include <sys/kmem.h>
#include <os/as/as_private.h>
#include <os/as/pmap.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/everror.h>

/*
 * dmap_mapsize defines the number of separate dma_mapalloc requests
 * that could be done for this mapping.
 *
 * "size" passed in is expected to be size of the map table requested
 * by the caller.
 *
 * Size of map table >> 2 gives the number of map table entries.
 * io_ctob() translates this into number of bytes of mapping,
 * dividing by (1024*1024) gives the number of megabytes in mapping.
 *
 * Limit the max number of mapping to be the same as the number of 
 * megabytes of mapping. So, if kernel is configured to support 64MB
 * of simultaneous mapping, allow upto 64 mapping.
 * 'size' is the size of page table in Bytes
 */
#define	dma_mapsize(size)	MAX(20, (io_ctob((size) >> 2)/(1024*1024)))

static int dma_mapx(dmamap_t *, caddr_t, caddr_t, int);

uint vmeprefetch;
pgno_t	gbg_pg;

/*
 * For compatibility with non-Everest systems.  iamap_init does the work
 * previously associated with dma_mapinit, and is called from Everest-
 * specific initialization routines, once for each dma map.
 */
void
dma_mapinit()
{
}

/*
 * Call for each adaptor/dma type, to manage map reg alloc/free.
 */

void
iamap_init(iamap_t	*iamap,
	   ulong	mapram,		/* Address of mapram */
	   ulong	size,		/* Size of map table in bytes */
	   iopaddr_t	iostart,	/* Starting IO address for mapping */
	   iopaddr_t	ioend)		/* Ending IO address for mapping */
{
	int	msize;
	ulong	offset;
	uint	mapaddr;
	caddr_t vmepage;
	volatile evreg_t *curr;
	volatile evreg_t *end;
#ifdef DEBUG
	int i;
#endif /* DEBUG */

	
	/* allocate 1 page for both scsi and VME prefetch */
	if (!vmeprefetch){
		vmepage = kvpalloc(1,VM_DIRECT,0);
		vmeprefetch = SYSTOIOPFN(ev_kvtophyspnum(vmepage), 0);
		gbg_pg = kvatopfn(vmepage) << 4;

#ifdef DEBUG
		for( i = 0 ; i < NBPP ; i++ )
			vmepage[i] = 0xff;
#endif /* DEBUG */
	}
	
	ASSERT((mapram  & 0xFFF) == 0);
	ASSERT((iostart & 0x3FFFFF) == 0);
	ASSERT((ioend   & 0x3FFFFF) == 0);

	initnlock(&iamap->lock, "dmamapl");
	sv_init(&iamap->out, SV_DEFAULT, "dmamapo");

	size = io_ctob(io_btoc(size));
	msize = dma_mapsize(size);
	iamap->map = (struct map*)kern_calloc(1, sizeof(struct map) * msize);
	mapinit(iamap->map, msize, &iamap->lock, &iamap->out);
	mfree(iamap->map, size>>2, 1);

	iamap->size	= size;
	iamap->table	= (uint *)kvpalloc(io_btoc(size), VM_DIRECT, 0);
	bzero(iamap->table, size);	/* Init all map entries to zero */
	mapaddr = (uint)(ev_kvtophyspnum((caddr_t)iamap->table) << PNUMSHFT);
	iamap->iostart	= iostart;
	iamap->ioend	= ioend;

	/*
	 * initializes the 1st level mapping ram completely.
	 * Since we will cycle up the io virtual space by the map size,
	 * the mapram to map pages are fixed and need not change again.
	 */
	curr = (volatile evreg_t*)IAMAP_L1_ADDR(mapram,iostart);
	end  = (volatile evreg_t*)IAMAP_L1_ADDR(mapram,ioend);
	offset = 0;
	for ( ;curr < end; curr++) {
		*curr = IAMAP_L2_ENTRY(mapaddr + offset);
		offset += IAMAP_L2_BLOCKSIZE1;
		if (offset >= iamap->size)
			offset = 0;
	}
	iamap->size = iamap->size << 10; 
}


/*
 * Cycle up the virtual address by the second level mapping size.
 * Do tlb flushing iff all virtual space for this dmamap is used up.
 */

void
iamap_map(iamap_t *iamap, dmamap_t *dmamap)
{
	dmamap->dma_addr += iamap->size;

	if (dmamap->dma_addr >= iamap->ioend ) { 
		/* used up the io space, time to flush the tlb */
		fchip_tlbflush(dmamap);

		/* reset the address back to the top */
		dmamap->dma_addr = iamap->iostart +
					(dmamap->dma_index << IO_PNUMSHFT);
	}
}

iamap_t *
iamap_get(int xadap, int type)
{
	if (type == DMA_VMEA32) {
		return &vmecc[vmecc_xtoiadap(xadap)].a32map;
	} else if (type == DMA_VMEA24) {
		return &vmecc[vmecc_xtoiadap(xadap)].a24map;
	}

	cmn_err(CE_PANIC, "Bad dma type: %d\n", type);
	/* NOTREACHED */
}

/*
 * I twisted and turned and twisted and turned on how to implement this.
 * All designs to do efficient tlb flush I had come up with so far are all
 * pretty expensive.
 * So I decide to hard wire the mapping and cycle thru on the dmamap basis.
 * The penalty is we may do tlb flush unnecessarily each time some dmamap
 * reach the limit.
 */
/* ARGSUSED */
dmamap_t *
dma_mapalloc(int type, int adap, int npages, int flag)
{
	struct map *map;
	dmamap_t   *dmamap;

	dmamap = (dmamap_t *) kern_malloc(sizeof(dmamap_t));
	dmamap->dma_type = type;
	dmamap->dma_adap = adap;

	if (type == DMA_SCSI)
	{
		if (npages == 0)
			npages = maxdmasz * (NBPP / IO_NBPP);
		dmamap->dma_virtaddr = kmem_alloc(
				(npages + 1) * sizeof(int), VM_PHYSCONTIG);
		ASSERT(dmamap->dma_virtaddr != NULL);
		dmamap->dma_size = (npages + 1);
	}
	else
	{
		map = iamap_get(adap,type)->map;

		/* For DMA prefetch, must map extra page */
		npages++;
		if (npages & 1)
			npages++;
		dmamap->dma_size = npages;

		dmamap->dma_index = malloc_wait(map,npages);
		ASSERT(dmamap->dma_index);
		dmamap->dma_index--;
		/* force a wrap the first time through iamap_map() */
		dmamap->dma_addr = iamap_get(adap,type)->iostart +
					(dmamap->dma_index << IO_PNUMSHFT);
	}

	return dmamap;
}

dmamap_t *
dma_mapallocb(int type, int adap, int nbytes, int flag)
{
	return dma_mapalloc(type, adap, io_btoc(nbytes), flag);
}

void
dma_mapfree(dmamap_t *dmamap)
{
	struct map *map;

	if( dmamap->dma_type != DMA_SCSI ) {
		/* flush all entries used by this dma map to prevent
		 * next user of the same index from hitting the old
		 * stale tlb entires.
		 */
		fchip_tlbflush(dmamap);
		map = iamap_get(dmamap->dma_adap, dmamap->dma_type)->map;
		mfree(map, dmamap->dma_size, dmamap->dma_index+1);
	}
	else {
		ASSERT(dmamap->dma_virtaddr);
		kmem_free(dmamap->dma_virtaddr, 
				(dmamap->dma_size - 1) * sizeof(int));
	}
	kern_free(dmamap);

}

int
dma_maps1(dmamap_t *dmamap, caddr_t addr, int len, caddr_t lastpage)
{
	__psunsigned_t	 page, startpage, endpage, vpage;
	uint		*map;
	__psunsigned_t	 ret;
	extern scuzzy_t	 scuzzy[];

	startpage = io_btoct(addr);
	endpage = io_btoct(addr + len - 1);
	map = (uint *) dmamap->dma_virtaddr;

	/*
	 * Add 2 because number of pages is 1 + endpage - startpage.
	 * Plus, we need enough room for the garbage page.
	 */
	if (2 + endpage - startpage > dmamap->dma_size)
	{
		endpage = startpage + dmamap->dma_size - 2;
		ret = (unsigned)
		      ((io_ctob(endpage) + IO_NBPC) - (__psunsigned_t) addr);
	}
	else
		ret = len;

	for (page = startpage; page < endpage; map++, page++)
	{
		vpage = ev_kvtophyspnum((caddr_t) io_ctob(page));
		*map = SYSTOIOPFN(vpage, page) << 4;
	}
	if (endpage >= startpage)
	{
		if (lastpage == 0)
			vpage = ev_kvtophyspnum((caddr_t) io_ctob(page));
		else
		{
			vpage = ev_kvtophyspnum(lastpage);
			page = io_btoct(lastpage);
		}
		*map++ = SYSTOIOPFN(vpage, page) << 4;
	}

	/* set last page to point to first page in memory for read ahead */
	*map = gbg_pg;

	return ret;
}

int
dma_map(dmamap_t *dmamap, caddr_t addr, int len)
{
	if (dmamap->dma_type == DMA_SCSI)
		return dma_maps1(dmamap, addr, len, 0);
	return dma_mapx(dmamap, addr, 0, len);
}

int
dma_map2(dmamap_t *dmamap, caddr_t addr, caddr_t addr2, int len)
{
	if (addr2 == 0)
		cmn_err(CE_PANIC, "dma_map2");
	if (dmamap->dma_type == DMA_SCSI)
		return dma_maps1(dmamap, addr, len, addr2);
	return dma_mapx(dmamap, addr, addr2, len);
}


#ifdef	EV_NOTDEF
/*
 * This does basically what kvtophyspnum() in os/space.c does but covers more
 * since everest architecture (both hw and sw) is a bit different.
 *	1. 0x00000000 - 0x7FFFFFFF	are treated as physical address.
 *	2. 0x80000000 - 0xAFFFFFFF	K0 and part of K1, unmapped part
 *	3. 0xB0000000 - 0xB7FFFFFF	small window
 *	4. 0xB8000000 - 0xBFFFFFFF	rest of K1, not mapped to EBUS
 *	5. 0xC0000000 - 0xC7FFFFFF	first 128MB of K2 reserved for kv
 *	6. 0xC8000000 - 0xF7FFFFFF	iotlb space
 *	7. the rest			treated as physical address.
 */

uint
ev_kvtophyspnum(caddr_t addr)
{
	if ((__psunsigned_t)addr < K0SEG)		/* phys addr given */
		return pnum((__psunsigned_t)addr);

	if ((__psunsigned_t)addr < SWINDOW_BASE)	/* K0 and part of K1 */
		return pnum(svirtophys((__psunsigned_t)addr));

	if ((__psunsigned_t)addr < SWINDOW_CEILING)	/* K1 small window */
		return SWINDOW_PPFN + pnum((__psunsigned_t)addr - SWINDOW_BASE);

	if ((__psunsigned_t)addr < K2SEG)
		return pnum((__psunsigned_t)addr);

	if (pnum((__psunsigned_t)addr - K2SEG) < syssegsz)
		return pdetopfn(kvtokptbl(addr));	/* kernel virtual */

#if _MIPS_SIM != _ABI64
	/* Don't need this when using 64-bit addressing (no iotlb) */
	if ((__psunsigned_t)addr < 0xF8000000)		/* in iotlb space */
		return iotlb[IOMAP_VPAGEINDEX(addr)].pte_pfn >> PGSHFTFCTR;
#endif

	return pnum((__psunsigned_t)addr);
}

#endif	/* EV_NOTDEF */
/*
 * ev_kvtophyspnum is always called with either K0 or K2 address. So, 
 * ASSERT for that, and invoke kvtophyspnum()
 */
uint
ev_kvtophyspnum(caddr_t addr)
{
	ASSERT(IS_KSEGDM(addr) || IS_KSEG2(addr));
	return kvtophyspnum(addr);
}


/*
 * This returns the page number that I/O devices want.
 */
uint
ev_kvtoiopnum(caddr_t addr)
{
	return SYSTOIOPFN(ev_kvtophyspnum(addr), io_btoct(addr));
}

	
static int
dma_mapx(dmamap_t *dmamap, caddr_t addr, caddr_t addr2, int len)
{
	iamap_t	*iamap = iamap_get(dmamap->dma_adap, dmamap->dma_type);
	uint	*maptable;
	paddr_t	startpage, endpage, p;
	int	j, map_addr2 = 1;

	if ((__psint_t)addr & 0x3) {
		cmn_err(CE_WARN, "dma_map: address not word aligned\n");
		return 0;
	}

	if (!(IS_KSEGDM(addr) || IS_KSEG2(addr))) {
		cmn_err(CE_WARN, "dma_map: address not valid\n");
		return 0;
	}

	/* get map table address, cycle up io bus address to avoid */
	/* flush f chip tlb cache every time */
	iamap_map(iamap, dmamap);

	/* dma_index usually doesn't change unless cycle to the end */
	maptable = iamap->table + dmamap->dma_index;

	/* adjust the range */
	startpage = io_btoct(addr);
	endpage   = io_btoct(addr+len-1);
	if ((endpage-startpage) >= dmamap->dma_size - 1) {
		endpage = startpage + dmamap->dma_size - 1;
		len = (io_ctob(endpage) + IO_NBPC) - (__psunsigned_t)addr;
		map_addr2 = 0;
	}
	if (map_addr2 && addr2)
		endpage--;

	dmamap->dma_virtaddr = (caddr_t)((__psunsigned_t)addr & ~(IO_NBPC-1));

	/* Now update the second level mapping table page */
	for (j = startpage % PGFCTR, p = startpage; p <= endpage;
				p++, maptable++, j++) {
		*maptable = SYSTOIOPFN(ev_kvtophyspnum((caddr_t)io_ctob(p)), j);
	}

	/* map one more page if requested */
	if (map_addr2 && addr2) {
		*maptable = SYSTOIOPFN(ev_kvtophyspnum(addr2),
					io_btoct(addr2));
		maptable++;
	}

	*maptable = vmeprefetch; /* for DMA prefetch */

	return len;
}


/*
 * Shamelessly copied from vtopv in os/region.c
 * Difference is that it is optimized for S1 and different I/O page size.
 */
int
dma_usertophys(register struct buf *bp)
{
	register preg_t	*prp;
	register int	 i;
	register pde_t	*pt;
	register pgcnt_t npgs;
	register uint	*map;
	register char	*vaddr;
#if PGFCTR > 1
	register int	 offset;
#else
#define offset 0
#endif
	auto pgno_t	 sz;
	vasid_t		 vasid;
	pas_t		*pas;
	ppas_t		*ppas;

	vaddr = bp->b_dmaaddr;
	ASSERT((__psunsigned_t)vaddr < (__psunsigned_t)K0SEG);

	npgs = numpages(vaddr, bp->b_bcount);
	ASSERT(npgs != 0);
	map = (uint *) kmem_alloc((npgs * PGFCTR + 1) * 4, VM_PHYSCONTIG);
	bp->b_private = map;
#if PGFCTR > 1
	offset = io_btoct(poff(vaddr));
#endif
	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *) vasid.vas_pasid;

	mraccess(&pas->pas_aspacelock);
	do {
		prp = findpreg(pas, ppas, vaddr);
		if (prp == NULL) {
			mrunlock(&pas->pas_aspacelock);
			kern_free(map);
			return 1;
		}


		sz = npgs;
		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);

		i = min(npgs, sz);
		vaddr += ctob(i);
		npgs -= i;

		while (--i >= 0) {
			ASSERT(pg_isvalid(pt));

                        /*
                         * These pages must be locked for IO
                         */
                        ASSERT((pfntopfdat(pg_getpfn(pt)))->pf_rawcnt > 0);
#if PGFCTR > 1
			while (offset < PGFCTR) {
#endif

				{
				*map = (pg_getpfn(pt) * PGFCTR + offset) << 4;
				map++;
				}
#if PGFCTR > 1
				offset++;
			}
			offset = 0;
#else
#undef offset
#endif
			pt++;
		}
	} while (npgs);
	*map = gbg_pg;

	mrunlock(&pas->pas_aspacelock);
	return 0;
}


/*
 * Free the map allocated in dma_usertophys.
 */
void
dma_userfree(struct buf *bp)
{
	kern_free(bp->b_private);
	bp->b_private = NULL;
}


/*
 * dma_mapbp -  Map `len' bytes from starting address `addr' for a DMA
 *              operation, using the given DMA map registers.
 *              Returns the actual number of bytes mapped.
 * We assume that on the 1st call len == b_bcount
 * Note that we can be called more than once for the same mapping
 * when SCSI disconnects/reconnects. We compute the offset into the
 * buffer by looking at the difference between bcount and len.
 *
 * Due to prefetch, we map one more page than required.
 */
int
dma_mapbp(dmamap_t *dmamap, buf_t *bp, int len)
{
	register uint		*maptable;
	register pgno_t		 npgs;
	int			 i;
	iamap_t			*iamap;
	register struct pfdat	*pfd = NULL;
	int			xoff;	/* bytes already xferred */
	extern scuzzy_t 	scuzzy[];

	if (len <= 0)
		return 0;

	if (BP_ISMAPPED(bp))
		panic("dma_mapbp");

	if (dmamap->dma_type == DMA_SCSI)
		maptable = (uint *) dmamap->dma_virtaddr;
	else {
		iamap = iamap_get(dmamap->dma_adap, dmamap->dma_type);
		/* get map table address, cycle up io bus address to avoid */
		/* flush f chip tlb cache every time */
		iamap_map(iamap, dmamap);

		/* dma_index usually doesn't change unless cycle to the end */
		maptable = iamap->table + dmamap->dma_index;
	}

	pfd = getnextpg(bp, pfd);

        /* compute page offset into xfer (i.e. pages to skip this time) */
        xoff = bp->b_bcount - len;
        npgs = btoct(xoff);
        while (npgs--) {
		pfd = getnextpg(bp, pfd);
                if (pfd == NULL)
                        cmn_err(CE_PANIC, "dma_mapbp: !pfd, bp 0x%x len 0x%x",
                                bp, len);
        }

        /*
         * compute # pages for this xfer and trim to max dma size
         * We take into cosideration that we may be part way into a page
         * and only xfering a partial page.
         */
        npgs = io_btoc(io_poff(xoff) + len);
        if (npgs >= dmamap->dma_size) {
                npgs = dmamap->dma_size - 1;
                len = io_ctob(npgs) - io_poff(xoff);
        }

	/*
	 * Set up the DMA operation.
	 */
	if (dmamap->dma_type == DMA_SCSI) {

		i = io_btoct(xoff) % PGFCTR;
		while (npgs--)
		{
			if (!pfd)
				cmn_err(CE_PANIC,
					"dma_mapbp: !pfd, bp %x len %x",
					bp, len);
			*maptable = SYSTOIOPFN(pfdattopfn(pfd), i) << 4;
			maptable++;
			if (++i >= PGFCTR) {
				pfd = getnextpg(bp, pfd);
				i = 0;
			}
		}
		*maptable = gbg_pg;
	} else {
		dmamap->dma_virtaddr = 0;	/* page aligned */
		i = io_btoct(xoff) % PGFCTR;
		while (npgs--)
		{
			if (!pfd)
				cmn_err(CE_PANIC,
					"dma_mapbp: !pfd, bp %x len %x",
					bp, len);
			*maptable = SYSTOIOPFN(pfdattopfn(pfd), i);
			maptable++;
			if (++i >= PGFCTR) {
				pfd = getnextpg(bp, pfd);
				i = 0;
			}
		}
		*maptable = gbg_pg;
	}

	return len;
}

/*
 * dma_mapaddr, Convert from virtual address to bus virtual address
 */
paddr_t
dma_mapaddr(dmamap_t *dmamap, caddr_t addr)
{
	paddr_t ioaddr;

	ASSERT(dmamap->dma_addr != 0);
	ASSERT(addr >= dmamap->dma_virtaddr);
	ASSERT(addr < (dmamap->dma_virtaddr + io_ctob(dmamap->dma_size)));

	ioaddr = dmamap->dma_addr + (addr - dmamap->dma_virtaddr);

	if (dmamap->dma_type == DMA_A24VME)
		ioaddr &= 0xFFFFFF;

	return ioaddr;
}
