#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/lg1hw.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern Rexchip *REX;

/* external utility routines */
extern void basic_dma_setup();
extern int dma_error_handler();
extern void rex_clear();

int
lg1_onereg_dma(argc, argv)
int argc;
char **argv;
{
        int dualhead = rex_dualhead();

	int error_count = 0;
	int i;
	unsigned char j;
	unsigned int *wbuf; 
	unsigned int *rbuf; 


   	msg_printf (VRB, "One register virtual DMA test\n");

	if (argc >= 0) { /* if 1 */
   		msg_printf (VRB, "requires Snoop - skipping test\n");
		return;
	}

	basic_DMA_setup(1); 	/* Snoop required here */

        /* Initialize Rex */
        rex_clear();
	vc1_on();

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 

	/* 12 bytes */
	for (i=0; i<3; i++) {
	    j = (i+1) & 0xff;
	    *wbuf++ = (j << 24) | (j << 16) | (j << 8) | j;
	}

        REX->set.xstarti = 100;
        REX->set.xendi = 100 + 12 - 1;
        REX->set.ystarti = 100;
        REX->set.yendi = 100;
        REX->go.command = ( 0);     /* NO-OP */

        REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | QUADMODE | REX_DRAW);

	*(uint *)(PHYS_TO_K1(DMA_GIO_ADR)) = (uint)&REX->go.rwaux1;

	/* One register to start the DMA */ 
	*(uint *)(PHYS_TO_K1(DMA_MEMADRDS)) = vdma_wphys; 

	/* Poll until DMA is stoped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
		;

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA read (from memory to gfx) did not complete\n");
		return -1;
	}

	/* DMA write - from gfx to memory */
	rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);

       	REX->set.xstarti = 100;
       	REX->set.xendi = 100 + 12 - 1;
       	REX->set.ystarti = 100;
       	REX->set.yendi = 100;

	REX->go.command = ( REX_LDPIXEL|QUADMODE);
	REX->set.command = ( REX_LDPIXEL|QUADMODE|BLOCK|XYCONTINUE);

	if (dualhead) 
		for (i = 0; i < 12/4; i++)
			rbuf[i] = REX->go.rwaux1;
	else {

		*(uint *)(PHYS_TO_K1(DMA_MEMADRD)) = vdma_rphys;
		msg_printf (DBG, "wrote %x to MEMADRD\n", vdma_rphys);
		*(uint *)(PHYS_TO_K1(DMA_MODE)) = i =
			*(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | DMA_WRITE;
		msg_printf (DBG, "wrote %x to DMA_MODES\n", i);
		*(uint *)(PHYS_TO_K1(DMA_SIZE)) =  i =
					(1 << 16) | 0xc;
		msg_printf (DBG, "wrote %x to DMA_SIZE\n", i);
		*(uint *)(PHYS_TO_K1(DMA_GIO_ADRS)) = (uint)&REX->go.rwaux1;
		msg_printf (DBG, "wrote %x to DMA_GIO_ADRS\n", &REX->go.rwaux1);
	
		/* Poll until DMA is stoped */
#define VDMA_TIMEOUT 50000
		for (i = 0; i < VDMA_TIMEOUT; i++) {
			if ( ! (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40) )
				break;
			*(uint *)(PHYS_TO_K1(DMA_STDMA)) = 0;
		}
		if (i == VDMA_TIMEOUT) {
			msg_printf (DBG, "dma timeout\n");
		}

		if (dma_error_handler()) {
			msg_printf(ERR,"DMA write (from gfx to memory) did not complete\n");
			return -1;
		}
	}
			
	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
        for (i=0; i<3; i++) {
            if (*rbuf != *wbuf) {
		        error_count++;
		        msg_printf(ERR,
		           "Address: 0x%08x, Expected: 0x%08x, Actual: 0x%08x\n",
		           (unsigned int) wbuf, *wbuf, *rbuf);
	    }
            wbuf++; rbuf++;
	}
    
	if (error_count)
            sum_error ("One register DMA test");
    	else
            okydoky ();

	return (error_count);
}
