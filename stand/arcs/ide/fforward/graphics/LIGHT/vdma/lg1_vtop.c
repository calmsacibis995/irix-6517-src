#ident	"$Revision: 1.17 $"

#include <arcs/spb.h>
#include <fault.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/lg1hw.h>
#include <sys/sysmacros.h>
#include <sys/immu.h>
#include <uif.h>
#include "dma.h"

#define PHYS_KPTEBASE   0x8401000

#define KPTE_INDX       0               /* tlb index for kptebase */
#define DMATLB_INDX     0               /* dma tlb index for gdma entry */

#define USERBASE        0x42f000        /* where to map data in user space */

/* external variables */
extern Rexchip *REX;

/* external utility routines */
extern void basic_dma_setup();
extern int dma_error_handler();
extern void rex_clear();
extern void wbflush();

extern int makesegment (void *, ulong, int, void *, int);
extern void set_dmatlb (int, void *, void *, int);
extern void clean_dmatlb (void);
extern void private_tlbpurge();

#define NWORDS 0x1000
#define NPIXELS 512
static int localdata[NWORDS] = {0x1020304};	/* data in K1 seg */
static int localdatasize = sizeof (localdata);

extern int received_exception;

static int
utlb_miss_test()
{
	int i;

	/* Force the VDMA to generate utlb miss interrupt by setting
	   valid bit to zero.  */
	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 0);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD, (unsigned long) (USERBASE+poff(&localdata))); 

	writemcreg(DMA_SIZE, ((NWORDS/NPIXELS) << 16) | (NPIXELS & 0xffff));

	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

	wbflush();

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		disconnect_handler ();
		return -1;
	}

	if (dma_error_handler() != VDMA_UTLBMISS) {
		msg_printf(ERR,"Did not catch utlb miss\n");
		disconnect_handler ();
		return -1;
	}

	/* Fix DMA utlb miss, and check whether VDM could convert virtual
	   address to pyhsical address or not */
	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 1);

	/* clear the exception flag */ 
	received_exception = 0;

	/* Start DMA again */
	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

	wbflush();

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

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA did not complete\n");
		return -1;
	}
	return 0;
}

static int
page_fault_test()
{
	unsigned int vaddr;
	pde_t *pde;
	int dma_cause;

	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 1);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD, (uint) (USERBASE+poff(&localdata)));

	writemcreg(DMA_SIZE, ((NWORDS/NPIXELS) << 16) | (NPIXELS & 0xffff));

	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

	wbflush();

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		disconnect_handler ();
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
		writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

		wbflush();

		/* Poll until DMA is stoped */
		while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
			;

        	/* restore original exception handler
         	 */
		if (!received_exception) {
			msg_printf(ERR,"No exception received \n");
			disconnect_handler ();
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
	return 0;
}

static int
write_protect_test()
{
	unsigned int vaddr;
	pde_t *pde;
	int dma_cause;

	set_dmatlb (DMATLB_INDX, (void *)USERBASE, (void *)PHYS_KPTEBASE, 1);

	/* put local exception handler in the SPB
	 */
	received_exception = 0;
	connect_handler ();

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADRD, (uint) (USERBASE+poff(&localdata)));

	writemcreg(DMA_SIZE, ((NWORDS/NPIXELS) << 16) | (NPIXELS & 0xffff));

	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

	wbflush();

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	if (!received_exception) {
		msg_printf(ERR,"No exception received \n");
		disconnect_handler ();
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
		writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

		wbflush();

		/* Poll until DMA is stoped */
		while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
			;

		if (!received_exception) {
			msg_printf(ERR,"No exception received \n");
			disconnect_handler ();
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
	writemcreg(DMA_MEMADRD, (uint) (USERBASE+poff(&localdata)));

	writemcreg(DMA_SIZE, ((NWORDS/NPIXELS) << 16) | (NPIXELS & 0xffff));

	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

	wbflush();

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
	writemcreg(DMA_MEMADRD, (uint) (USERBASE+poff(&localdata)));

	writemcreg(DMA_SIZE, ((NWORDS/NPIXELS) << 16) | (NPIXELS & 0xffff));

	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

	wbflush();

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
lg1_vtop_dma(argc, argv)
int argc;
char **argv;
{
	int n;
	int rtval = 0;
	int lio_mask;


   	msg_printf (VRB, "Virtual DMA virtual address translation test\n");

	if (argc == 1 || argc > 2) {
	    msg_printf(ERR,"usage: %s (Utlb_miss=1,Page_fault=2,Clean=3,Bmem=4,Bpte=5)\n", argv[0]);
	    return(-1);
	}
	
	atob(argv[1], &n);

	if (n < 1 || n > 5) {
	    msg_printf(ERR,"usage: %s (Utlb_miss=1,Page_fault=2,Clean=3,Bmem=4,Bpte=5)\n", argv[0]);
	    return(-1);
	}
	
	private_tlbpurge();
	clean_dmatlb();

	if (makesegment ((void *)USERBASE, (ulong)localdatasize,
            pnum(KDM_TO_PHYS(&localdata)), (void *)PHYS_TO_K0(PHYS_KPTEBASE),
	    n)) {
		printf ("Error:  Could not create page table\n");
        	return -1;
	}

	/* creating color map for color index 0 to 7 */
        rex_setclt();

        /* Initialize Rex */
        rex_clear();
	vc1_on();

        REX->set.xstarti = 100;
        REX->set.xendi = 100 + NPIXELS - 1;
        REX->set.ystarti = 100;
        REX->set.yendi = 100 + (NWORDS/NPIXELS) - 1;
        REX->go.command = ( 0);     /* NO-OP */

        REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | QUADMODE | REX_DRAW);

        basic_DMA_setup(0);


	/*
	 * Make sure no pending interrupt
	 */

	writemcreg(DMA_CAUSE, 0);

	/* Enable DMA interrupt */
        /* memory address of data is virtual */
        writemcreg(DMA_CTL, *(volatile uint *)(PHYS_TO_K1(DMA_CTL)) | 0x110);

	/*
	 * Enable graphics interrupt.
	 * Clear pending GDMA int first.
	 */

	*(unchar *)(PHYS_TO_K1(LIO_0_ISR_ADDR)) = 
		*(unchar *)(PHYS_TO_K1(LIO_0_ISR_ADDR)) & ~LIO_GDMA;

	lio_mask = *(unchar *)(PHYS_TO_K1(LIO_0_MASK_ADDR));
	*(unchar *)(PHYS_TO_K1(LIO_0_MASK_ADDR)) = lio_mask | LIO_GDMA_MASK;

	wbflush();

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
	msg_printf (DBG, "LIO_0_ISR %x mask %x CPUCTRL1 %x\n", 
			*(unchar *)(PHYS_TO_K1(LIO_0_ISR_ADDR)),
			*(unchar *)(PHYS_TO_K1(LIO_0_MASK_ADDR)),
			*(uint *)(PHYS_TO_K1(CPUCTRL1)));
	*(unchar *)(PHYS_TO_K1(LIO_0_MASK_ADDR)) = lio_mask;

	if (rtval) {
		msg_printf (ERR, "DMA_CAUSE %x\n", dma_error_handler());
    		sum_error ("DMA virtual-to-physical addr./interrupt test");	
	}
	else
		okydoky ();

	return rtval;
}

