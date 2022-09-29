#ident	"$Revision: 1.23 $"

#include <sys/types.h>
#include <arcs/spb.h>
#include <fault.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/immu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include "dma.h"

#define PHYS_KPTEBASE   0x8401000

#define KPTE_INDX       0               /* tlb index for kptebase */
#define DMATLB_INDX     0               /* dma tlb index for gdma entry */

#define USERBASE        0x42f000        /* where to map data in user space */

/* DMA_CTL register bit layout */
#define DMA_CTL_PTEsize_4	0x00000000
#define DMA_CTL_PTEsize_8	0x00000001
#define DMA_CTL_PAGEsize_4	0x00000000
#define DMA_CTL_PAGEsize_16	0x00000002
#define DMA_CTL_CACHEsize_4	0x00000000
#define DMA_CTL_CACHEsize_8	0x00000004
#define DMA_CTL_CACHEsize_16	0x00000008
#define DMA_CTL_CACHEsize_32	0x0000000c
#define DMA_CTL_IntMask_En	0x00000010
#define DMA_CTL_Xlate_En	0x00000100
#define DMA_CTL_DecSlv_En	0x00000200
#define DMA_CTL_SLIM_MASK	0x0001f000
#define DMA_CTL_SLIM_SHIFT	12
#define DMA_CTL_TLIM_MASK	0x3ff00000
#define DMA_CTL_TLIM_SHIFT	20

int received_exception;

long *saved_ge_vec;
static k_machreg_t saved_sr;

/* external variables */
/* Match definition in mc.h */
#ifdef MCREGWBUG
extern void writemcreg();
#endif

extern __psint_t local_ge_vec();

unsigned int vdma_wphys = 0;
unsigned int vdma_rphys = 0;

/* allocate a page of memory, return physical address */
unsigned int
vdma_getpage(void)	
{
	__psunsigned_t rv;

	/* XXX Get 2 pages, the second is only used by virtual addr testing */

	rv = (__psunsigned_t)align_malloc (2*NBPC, NBPC);

	if (rv == NULL)
		msg_printf (VRB, "Out of memory\n");

	return KDM_TO_MCPHYS(rv);
}

void
basic_DMA_setup( int snoop )
{

	int i;

	/*
	 * Get read and write buffers first time through.
	 */
	if (vdma_wphys == NULL) {
		vdma_wphys = vdma_getpage();
		vdma_rphys = vdma_getpage();
		if (vdma_wphys == NULL || vdma_rphys == NULL)
			return;
	}

	/* Set up MC default registers */
#define CPUCTRL0_REFS_4_LINES   0x00000002      /* if no EISA bus */

	/* Refresh 4 lines */
	writemcreg (CPUCTRL0, i =
	    *(volatile uint *)(PHYS_TO_COMPATK1(CPUCTRL0)) | CPUCTRL0_REFS_4_LINES);

	msg_printf(DBG,"basic_DMA_setup: set CPUCTRL0 = %x (refresh 4 lines)\n",
	  i);

	/* Snoop */
	if (snoop)
		writemcreg (CPUCTRL0, i = 
	    *(volatile uint *)(PHYS_TO_COMPATK1(CPUCTRL0)) | CPUCTRL0_SNOOP_EN);
	else
		writemcreg (CPUCTRL0, i = 
	    *(volatile uint *)(PHYS_TO_COMPATK1(CPUCTRL0)) & ~CPUCTRL0_SNOOP_EN);

	msg_printf(DBG,"basic_DMA_setup: set CPUCTRL0 = %x (Snoop %s)\n", i,
						 snoop ? "on" : "off");

	/* Big Endian */
	writemcreg (CPUCTRL0, i = 
	    *(volatile uint *)(PHYS_TO_COMPATK1(CPUCTRL0)) & ~CPUCTRL0_LITTLE);

	msg_printf(DBG,"basic_DMA_setup: set CPUCTRL0 = %x (Big Endian)\n", i);

	/* Use the default burst/delay defined IP2[02].h */

	/* 
	 * Initialize DMA ctl register:
	 *     Tlim = 512, Slim = 4, no decrementing DMA, no translation,
	 *     interrupt disabled, cache size = 32 words, page = 4/16k bytes,
	 *     PTE size = 4/8 bytes 
	 */
	i = (512 << DMA_CTL_TLIM_SHIFT) | (4 << DMA_CTL_SLIM_SHIFT) 
	  | DMA_CTL_CACHEsize_32 
	  | (sizeof(pte_t) == 4 ? DMA_CTL_PTEsize_4 : DMA_CTL_PTEsize_8);

#if _PAGESZ == 4096
	i |= DMA_CTL_PAGEsize_4;

#elif _PAGESZ == 16384
	i |= DMA_CTL_PAGEsize_16;
#else
#error "Invalid Page Size"
#endif 

	/* write out the dma ctl register */
	writemcreg (DMA_CTL, i);

	msg_printf(DBG,"basic_DMA_setup: set DMA_CTL = %x (Tlim 512 Slim 4)\n",
	  i);
	msg_printf(DBG,"basic_DMA_setup: set DMA_GIO_{MASK,SUB} = 0\n");
	msg_printf(DBG,"PHYS, No Int, Cache 32 words, Page %dk, PTE %d bytes\n",
	  (_PAGESZ / 1024), sizeof(pte_t));

	/* Zero out DMA address mask and subst. */
	writemcreg (DMA_GIO_MASK, 0);
	writemcreg (DMA_GIO_SUB, 0);

	msg_printf (DBG,"basic_DMA_setup: done\n");

	wbflush();
}


#define VDMA_TIMEOUT 500000
#define DMA_RUN_In_Progress		0x40

int
vdma_wait(void)      /* Poll until DMA is stopped or times out */
{
        int i;
        int rv = 0;     /* assume normal termination */


        for (i = 0; i < VDMA_TIMEOUT; i++) {
                if (*(volatile uint *)(PHYS_TO_COMPATK1(DMA_RUN))&DMA_RUN_In_Progress)
                        DELAY (1);
                else
                        break;
        }

        if (i == VDMA_TIMEOUT) {
		/* return timeout indicator */
                rv = 1; 

                i = *(uint *)(PHYS_TO_COMPATK1(DMA_STDMA));
                writemcreg (DMA_STDMA, 0);

                printf ("VDMA timeout, DMA_MODES  %x DMA_RUN %x\n",
                        *(uint *)(PHYS_TO_COMPATK1(DMA_MODE)),
                        *(uint *)(PHYS_TO_COMPATK1(DMA_RUN)));
        }
        return rv;
}



/* XXX
 * Check args.
 * NOTE : address translation is OFF.
 */

void
dma_go (caddr_t gioaddr, uint buf, int h, int w, int s, int z, int modes)
{
        uint u;

        /* Set DMA memory address */
        writemcreg (DMA_MEMADR, u = buf); /* physical address */
	msg_printf (DBG,"dma_go: wrote %x to DMA_MEMADR\n", u);

        /* Set transfer size parameters */
        writemcreg (DMA_SIZE, u = (h << 16) | w);
	msg_printf (DBG,"dma_go: wrote %x to DMA_SIZE (nlines:width)\n", u);

        /* Set zoom factor and stride */
        writemcreg (DMA_STRIDE, u = (z << 16) | (s & 0xffff));
	msg_printf (DBG,"dma_go: wrote %x to DMA_STRIDE (zoom:stride)\n", u);

        /* Setup DMA mode: long burst, incrementing addressing,
           fill mode disabled, sync disabled*/
        writemcreg (DMA_MODE, u = modes);
	msg_printf (DBG,"dma_go: wrote %x to DMA_MODE\n", u);

        /* Start DMA */
        writemcreg (DMA_GIO_ADRS, KDM_TO_MCPHYS(gioaddr));
	msg_printf (DBG,"dma_go: writing %x to DMA_GIO_ADRS\n", 
	    KDM_TO_PHYS(gioaddr));

        vdma_wait();
}



int
dma_error_handler(void)
{
	if (*(uint *)(PHYS_TO_COMPATK1(DMA_RUN)) & 0x1) {
		msg_printf(ERR, "Attempt to access non-resident page\n");	
		writemcreg (DMA_CAUSE,
			*(volatile uint *)(PHYS_TO_COMPATK1(DMA_RUN)) & ~0x1);
		return VDMA_PAGEFAULT;
	}
	else if (*(uint *)(PHYS_TO_COMPATK1(DMA_RUN)) & 0x2) {
		msg_printf(ERR, "VDMA uTLB miss\n");	
		writemcreg (DMA_CAUSE,
			*(volatile uint *)(PHYS_TO_COMPATK1(DMA_CAUSE)) & ~0x2);
		return VDMA_UTLBMISS;
	}
	else if (*(uint *)(PHYS_TO_COMPATK1(DMA_RUN)) & 0x4) {
		msg_printf(ERR, "Attempt to access a write-protected page\n");	
		writemcreg (DMA_CAUSE,
			*(volatile uint *)(PHYS_TO_COMPATK1(DMA_CAUSE)) & ~0x4);
		return VDMA_CLEAN;
	}
	else if (*(uint *)(PHYS_TO_COMPATK1(DMA_RUN)) & 0x10) {
		msg_printf(ERR, "Memory address is invalid\n");	
		writemcreg (DMA_CAUSE,
			*(volatile uint *)(PHYS_TO_COMPATK1(DMA_CAUSE)) & ~0x10);
		return VDMA_BMEM;
	}
	else if (*(uint *)(PHYS_TO_COMPATK1(DMA_RUN)) & 0x20) {
		msg_printf(ERR, "PTE address is invalid\n");	
		writemcreg (DMA_CAUSE,
			*(volatile uint *)(PHYS_TO_COMPATK1(DMA_CAUSE)) & ~0x20);
		return VDMA_BPTE;
	}
	else if (*(uint *)(PHYS_TO_COMPATK1(DMA_RUN)) & 0x8) {
		msg_printf(DBG, "DMA completed\n");
		return VDMA_COMPLETE;
	}
	else
		return VDMA_UNKNOWN;
}

/*
 * connect the new utlbmiss handler
 */
void
connect_handler (void)
{
    /* Save the old... */

	saved_ge_vec = SPB->GEVector;

	saved_sr = get_sr();

    /* ...and connect the new. */
	SPB->GEVector = (long *)local_ge_vec;

	set_sr (saved_sr | SR_IBIT3 | SR_IE);

}

/*
 * disconnect the new utlbmiss handler
 */
void
disconnect_handler (void)
{
    /* Replace the new with the old */
	SPB->GEVector = saved_ge_vec;
	set_sr (saved_sr);
}

/*
 * Handle the utlbmiss exception in the manner of the kernel.
 * No provision is made for second level TLB misses, so the tlb
 * must contain the address referenced for the new pte.  The
 * code in utlbasm.s does the save/restore of all registers so
 * we can use do this in 'C' here.
 */
void
local_exception_handler (void)
{

    received_exception = 1;

#if 0
    extern int _regs[];
    ulong pte;

    pte = _regs[R_CTXT] << 1;
    pte = *(ulong *)pte;
    set_tlblo(pte);
#endif

    return;
}

/*
 * Create a page table pt to represent the segment 
 * containing vaddr.
 *
 * pt must be page-aligned and the address space 
 * of vaddr/size must be mappable into a single segment
 *
 * pfn is the first physical page number to be mapped
 *
 * Note ptes are 8 bytes, each page holds 512 ptes, the
 * page table can map 2 MB total.
 *
 * Returns 0 if table is created OK, 1 if not.
 */
int
makesegment (void *vaddr, ulong size, int pfn, void *pt, int n)
{
    int np;
    pde_t *pde;
    int firsttime = 1;

    if ((np = (int)numpages(vaddr, size)) > NPGPT)
	return 1;
    if (!samesegment(vaddr, (ulong)vaddr+size-1))
	return 1;
    if (poff(pt))
	return 1;

    bzero (pt, NBPP);

    switch (n) {

	default:
	case 0:
	case 1:
	case 5: /* bad PTE interrupt */
		for (pde = &((pde_t *)pt)[pnum(soff(vaddr))]; np > 0; 
		  --np, ++pde, pfn++)
			pde->pgi = mkpde(PG_G|PG_M|PG_VR, pfn);
		break;

	case 2:	/* force it generating page fault interrupt */
    		for (pde = &((pde_t *)pt)[pnum(soff(vaddr))]; np > 0; 
		  --np, ++pde, pfn++)
			pde->pgi = mkpde(PG_G|PG_M, pfn);
		break;

	case 3: /* force it generating clean (write protected) interrupt */ 
    		for (pde = &((pde_t *)pt)[pnum(soff(vaddr))]; np > 0; 
		  --np, ++pde, pfn++)
			pde->pgi = mkpde(PG_G|PG_VR, pfn);
		break;

	case 4: /* bad memory interrupt */
    		for (pde = &((pde_t *)pt)[pnum(soff(vaddr))]; np > 0; 
		  --np, ++pde, pfn++) {
			/* Give fake pfn by adding 0x80 (which should be in
			   the EISA I/O space, hopelly it will generate bad 
			   memory fault */
			if (firsttime) {
				pde->pgi = mkpde(PG_G|PG_VR, pfn+0x80);
				firsttime = 0;
			}
			else 
				pde->pgi = mkpde(PG_G|PG_VR, pfn);
		
		}
		break;
    }

    return 0;
}


/* 
 * invalidate the entire tlb
 */
void
private_tlbpurge()
{
    register int i;

    for (i = 0; i < NTLBENTRIES; i++)
	invaltlb(i);
}

/* Prototype code for the virtual dma tlb */
#define DTLB_VPNHIMASK		(~(((_PAGESZ*_PAGESZ)/(sizeof(pde_t)))-1))
#ifdef DOG
#define DTLB_VPNHIMASK		0xffe00000	/* upper 11 bits */
#endif

#define DTLB_VALID		0x2		/* bit 1 */

#define	NDTLBENTRIES	4

void 
clean_dmatlb (void)
{
    writemcreg  (DMA_TLB_HI_0, 0);
    writemcreg  (DMA_TLB_LO_0, 0);
    writemcreg  (DMA_TLB_HI_1, 0);
    writemcreg  (DMA_TLB_LO_1, 0);
    writemcreg  (DMA_TLB_HI_2, 0);
    writemcreg  (DMA_TLB_LO_2, 0);
    writemcreg  (DMA_TLB_HI_3, 0);
    writemcreg  (DMA_TLB_LO_3, 0);
}


/* 
 * setup the dmatlb for a gdma transfer.  vaddr is the
 * virtual address of the data to be transferred.
 * pt is the physical address of the page table that maps
 * the segment containing the virtual address.
 */
/*ARGSUSED1*/
void 
set_dmatlb (int index, void *vaddr, void *pt, int v)
{
    writemcreg (DMA_TLB_HI_0, (int)((__psunsigned_t)vaddr & DTLB_VPNHIMASK));

    if (v)
    	writemcreg (DMA_TLB_LO_0,
		(int)(pnum (KDM_TO_PHYS(pt)) << (BPCSHIFT-6)) | DTLB_VALID);
    else 
	/* force it generating utlb miss interrupt */
    	writemcreg (DMA_TLB_LO_0, (int)pnum (KDM_TO_PHYS(pt)) << (BPCSHIFT-6));
}

void
set_dmapte (pde_t *pde, uint vaddr)
{
	unsigned long index;
	uint pfn;
	uint *fudge;

	index = (vaddr >> PNUMSHFT) & PTOFFMASK;

/*
	pfn = (vaddr & 0xfffff000) >> 12;
*/
	pfn = (int)btoct(vaddr) ;

	fudge = (uint *)(pde+index);

	fudge[0] = (pfn << (BPCSHIFT-6)) | PG_M | PG_VR;
	fudge[1] = (pfn << (BPCSHIFT-6)) | PG_M | PG_VR;
	

	msg_printf(VRB,"set_dmapte: addr %x pde %x pte %x at %x idx %x pfn %x\n", 
		vaddr, pde, *fudge, fudge, index, pfn); 
	msg_printf (VRB, "TLBhi %x lo %x\n",
    			*(uint *) PHYS_TO_COMPATK1(DMA_TLB_HI_0),
    			*(uint *) PHYS_TO_COMPATK1(DMA_TLB_LO_0)); 
}
