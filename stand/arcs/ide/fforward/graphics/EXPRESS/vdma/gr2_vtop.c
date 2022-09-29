#ident	"$Revision: 1.7 $"

#include <arcs/spb.h>
#include <fault.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/gr2hw.h>
#include <sys/sysmacros.h>
#include <sys/immu.h>
#include "libsc.h"
#include <uif.h>
#include "diagcmds.h"
#include "dma.h"

#define PHYS_KPTEBASE   0x8401000

#define KPTE_INDX       0               /* tlb index for kptebase */
#define DMATLB_INDX     0               /* dma tlb index for gdma entry */

#define USERBASE        0x42f000        /* where to map data in user space */

/* external variables */
extern struct gr2_hw *base;
extern int received_exception;

/* external utility routines */
extern int dma_error_handler();
extern int makesegment (void *, ulong, int, void *, int);
extern void set_dmatlb (int, void *, void *, int);
extern void clean_dmatlb (void);
extern void private_tlbpurge();

#define NWORDS 0x1000
#define NPIXELS 512
#define GFX_TO_HOST     0
#define HOST_TO_GFX     1
#define TIMEOUT_VDMA	100000

int localdata[NWORDS];	/* data in K1 seg */
int localdatasize = sizeof (localdata);

static int
utlb_miss_test()
{
	register long i;

	/* Force the VDMA to generate utlb miss interrupt by setting
	   valid bit to zero.  */
	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 0);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD, (unsigned long) (USERBASE+poff(&localdata))); 

	/* always enable sync bit for EXPRESS */
	writemcreg(DMA_MODE, 
		*(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | VDMA_SYNC);

	writemcreg(DMA_SIZE, (1 << 16) | (NWORDS*sizeof(long))); 

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

	base->hq.fin1 = 0;
	base->fifo[DIAG_REDMA] = HOST_TO_GFX;
	base->fifo[DIAG_DATA] = 100; /* x1 */
	base->fifo[DIAG_DATA] = 100; /* y1 */
	base->fifo[DIAG_DATA] = 100 + NPIXELS - 1;
	base->fifo[DIAG_DATA] = 100 + (NWORDS/NPIXELS) - 1;

	writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma); 

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		return -1;
	}

	if (dma_error_handler() != VDMA_UTLBMISS) {
		msg_printf(ERR,"Did not catch utlb miss\n");
		return -1;
	}

	/* Fix DMA utlb miss, and check whether VDM could convert virtual
	   address to pyhsical address or not */
	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 1);

	/* clear the exception flag */ 
	received_exception = 0;

	/* Start DMA again */
	writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma);

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	/* restore original exception handler
	 */
	disconnect_handler ();

	/* Should receive DMA complete interrupt */
	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		return -1;
	}

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA did not complete\n");
		return -1;
	}

	for (i=0; 1; i++) {
		if (base->hq.version & HQ2_FINISH1) 
			break;

		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			return -1;
		}
		DELAY(1);
	}

	base->hq.fin1 = 0;
	return 0;
}

static int
page_fault_test()
{
	register long i;
	unsigned int vaddr;
	pde_t *pde;
	int dma_cause;

	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 1);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD,  (uint) (USERBASE+poff(&localdata)));

	/* always enable sync bit for EXPRESS */
	writemcreg(DMA_MODE, 
		*(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | VDMA_SYNC);

	writemcreg(DMA_SIZE,  (1 << 16) | (NWORDS*sizeof(long)));

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

        base->hq.fin1 = 0;
        base->fifo[DIAG_REDMA] = HOST_TO_GFX;
        base->fifo[DIAG_DATA] = 100; /* x1 */
        base->fifo[DIAG_DATA] = 100; /* y1 */
        base->fifo[DIAG_DATA] = 100 + NPIXELS - 1;
        base->fifo[DIAG_DATA] = 100 + (NWORDS/NPIXELS) - 1;

	writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma);

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		return -1;
	}

	dma_cause = dma_error_handler();

	while (dma_cause == VDMA_PAGEFAULT) {

		/* find the pde */
		vaddr = (uint)(PHYS_TO_K1(DMA_MEMADR));
		pde = &((pde_t *)PHYS_TO_K0(PHYS_KPTEBASE))[pnum(soff(vaddr))];

		/* fix the pde entry */
		pde->pgi = mkpde(PG_G|PG_M|PG_VR, pde->pte.pg_pfn);

        	/* clear the exception flag */
        	received_exception = 0;

		/* re-start the DMA */
		writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma);

		/* Poll until DMA is stoped */
		while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
			;

		if (!received_exception) {
			msg_printf(ERR,"No exception received \n");
			return -1;
		}
		dma_cause = dma_error_handler();
	}

	/* restore original exception handler
	 */
	disconnect_handler ();

	if (dma_cause) {
		msg_printf(ERR,"DMA did not complete\n");
		return -1;
	}

	for (i=0; 1; i++) {
		if (base->hq.version & HQ2_FINISH1) 
			break;

		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			return -1;
		}
		DELAY(1);
	}

	base->hq.fin1 = 0;

	return 0;
}

static int
write_protect_test()
{
	register long i;
	unsigned int vaddr;
	pde_t *pde;
	int dma_cause;

	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 1);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD,  (uint) (USERBASE+poff(&localdata)));

	        /* always enable sync bit for EXPRESS */
        writemcreg(DMA_MODE, 
                *(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | VDMA_SYNC);

        writemcreg(DMA_SIZE, (1 << 16) | (NWORDS*sizeof(long)));

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

        base->hq.fin1 = 0;
        base->fifo[DIAG_REDMA] = HOST_TO_GFX;
        base->fifo[DIAG_DATA] = 100; /* x1 */
        base->fifo[DIAG_DATA] = 100; /* y1 */
        base->fifo[DIAG_DATA] = 100 + NPIXELS - 1;
        base->fifo[DIAG_DATA] = 100 + (NWORDS/NPIXELS) - 1;

	writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma);

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		return -1;
	}

	dma_cause = dma_error_handler();

	while (dma_cause == VDMA_CLEAN) {

		/* find the pde */
		vaddr = (uint)(PHYS_TO_K1(DMA_MEMADR));
		pde = &((pde_t *)PHYS_TO_K0(PHYS_KPTEBASE))[pnum(soff(vaddr))];

		/* fix the page table entry */
		pde->pgi = mkpde(PG_G|PG_M|PG_VR, pde->pte.pg_pfn);

                /* clear the exception flag */ 
                received_exception = 0;

		/* re-start the DMA */
		writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma);

		/* Poll until DMA is stoped */
		while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
			;

		if (!received_exception) {
			msg_printf(ERR,"No exception received \n");
			return -1;
		}
		dma_cause = dma_error_handler();
	}

	/* restore original exception handler
	 */
       	disconnect_handler ();

	if (dma_cause) {
		msg_printf(ERR,"DMA did not complete\n");
		return -1;
	}

	for (i=0; 1; i++) {
		if (base->hq.version & HQ2_FINISH1) 
			break;

		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			return -1;
		}
		DELAY(1);
	}

	base->hq.fin1 = 0;
	return 0;
}

static int
bad_mem_test()
{
	unsigned int vaddr;
	pde_t *pde;

	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 1);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD,  (uint) (USERBASE+poff(&localdata)));

        /* always enable sync bit for EXPRESS */
        writemcreg(DMA_MODE, 
                *(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | VDMA_SYNC);

        writemcreg(DMA_SIZE, (1 << 16) | (NWORDS*sizeof(long)));

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

        base->hq.fin1 = 0;
        base->fifo[DIAG_REDMA] = HOST_TO_GFX;
        base->fifo[DIAG_DATA] = 100; /* x1 */
        base->fifo[DIAG_DATA] = 100; /* y1 */
        base->fifo[DIAG_DATA] = 100 + NPIXELS - 1;
        base->fifo[DIAG_DATA] = 100 + (NWORDS/NPIXELS) - 1;

	writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma);

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	/* restore original exception handler
	 */
	disconnect_handler ();

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		return -1;
	}

	if (dma_error_handler() != VDMA_BMEM) {
		msg_printf(ERR,"Missed the Bad memory interrupt\n");
		return -1;
	}

	base->hq.fin1 = 0;
	return 0;
}

static int 
bad_pte_test()
{
#define BAD_PHYS_KPTEBASE	0x80000

	/* creating page table at illegal memory location */
	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)BAD_PHYS_KPTEBASE, 1);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD,  (uint) (USERBASE+poff(&localdata)));

        /* always enable sync bit for EXPRESS */
        writemcreg(DMA_MODE, 
                *(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | VDMA_SYNC);

        writemcreg(DMA_SIZE, (1 << 16) | (NWORDS*sizeof(long)));

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

        base->hq.fin1 = 0;
        base->fifo[DIAG_REDMA] = HOST_TO_GFX;
        base->fifo[DIAG_DATA] = 100; /* x1 */
        base->fifo[DIAG_DATA] = 100; /* y1 */
        base->fifo[DIAG_DATA] = 100 + NPIXELS - 1;
        base->fifo[DIAG_DATA] = 100 + (NWORDS/NPIXELS) - 1;

	writemcreg(DMA_GIO_ADRS,  (uint) &base->hq.gedma);

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	/* restore original exception handler
	 */
	disconnect_handler ();

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		return -1;
	}

	if (dma_error_handler() != VDMA_BPTE) {
		msg_printf(ERR,"Missed the Bad PTE interrupt\n");
		return -1;
	}

	return 0;
}

/*
 * This test covered
	1. utlb miss interrupt
	2. page fault interrupt
	3. clean interrupt
	4. bad memory interrupt
	5. bad PTE interrupt
 * And expected a DMA complete interrupt if no errors happened in DMA.
 */

int
gr2_vtop_dma(argc, argv)
int argc;
char **argv;
{
	register long i;
	int j,n;
	int rtval = 0;


   	msg_printf (VRB, "Virtual DMA virtual address translation test\n");

	if (argc == 1 || argc > 2) {
	    msg_printf(ERR,"usage: %s (Utlb_miss=1,Page_fault=2,Clean=3,Bmem=4,Bpte=5)\n", argv[0]);
	    return(-1);
	}
	
	atob(argv[1], &n);

	for (i=0; i<NWORDS; i++) {
		j = i & 0xff;
		localdata[i] = (j<<24) | (j<<16) | (j<<8) | j ;
	}

	private_tlbpurge();
	clean_dmatlb();

	if (makesegment ((void *)USERBASE, (ulong)localdatasize,
            pnum(KDM_TO_PHYS(&localdata)), (void *)PHYS_TO_K0(PHYS_KPTEBASE),
	    n)) {
		printf ("Error:  Could not create page table\n");
        	return -1;
	}

	basic_DMA_setup(0);

	/* Enable DMA interrupt */
        /* memory address of data is virtual */
        writemcreg(DMA_CTL, *(volatile uint *)(PHYS_TO_K1(DMA_CTL)) | 0x110);

	switch (n) {
		case 1:
			rtval = utlb_miss_test();
			break;
		case 2:
			rtval = page_fault_test();
			break;
		case 3: 
			rtval = write_protect_test();
			break;
		case 4:
			rtval = bad_mem_test();
			break;
		case 5:
			rtval = bad_pte_test();
			break;
		default:
			break;
	}

	if (rtval)
    		sum_error ("DMA virtual-to-physical addr./interrupt test");	
	else
		okydoky ();

	return rtval;
}

