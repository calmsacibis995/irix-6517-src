#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "libsk.h"
#include "libsc.h"
#include "gr2.h"
#include "uif.h"
#include "dma.h"
#include "diagcmds.h"

/* external variables */
extern struct gr2_hw *base; 


/* external utility routines */
extern int dma_error_handler();
extern int GBL_VMAVers;
extern int GBL_VMBVers;
extern int GBL_VMCVers;
static unsigned long color[8] = {0x0, 0xff, 0xff00, 0xff0000,
                          0xffff00, 0xffff, 0xff00ff, 0xffffff};

int
gr2_defaultregs_dma(argc, argv)
int argc;
char **argv;
{
	int error_count = 0;
	int npixels;
	int rflag;
	int i;
	unsigned int *wbuf; 
	unsigned int *rbuf; 
	int cache_mode = 1;	/* We don't snoop, we are responsible
				   for cache coherency */
 	long pixmask;


   	msg_printf (VRB, "Virtual DMA default registers test\n");

	if (argc > 3) {
	    msg_printf(ERR,"usage: %s [npixels] [rtest]\n", 
				argv[0]);
	    return(-1);
	}
	/* check bit-plane configuration */
        gr2_boardvers();	
	if (GBL_VMAVers != 3) pixmask = 0xff;
	if (GBL_VMBVers != 3) pixmask |= 0xff00;
	if (GBL_VMCVers != 3) pixmask |= 0xff0000;


	npixels = 3;
	rflag = 1;
	if (argc == 2)
		atob(argv[1], &npixels);
	else if (argc == 3) {
		atob(argv[1], &npixels);
		atob(argv[2], &rflag);
	}

	/* Max. number of pixels */
	if (npixels > 1000) npixels = 1000;

	basic_DMA_setup(0);

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 

	for (i=0; i<npixels; i++) 
		*wbuf++ = color[(i+1) & 0x7] & pixmask;
	wbuf -= npixels;

        /* Flush the data to RAM */
        if (cache_mode)
		flush_cache();

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

	base->hq.fin1 = 0;
        base->fifo[DIAG_REDMA] = 1;     /* Host to Gfx */
        base->fifo[DIAG_DATA] = 100; /* x1 */
        base->fifo[DIAG_DATA] = 100; /* y1 */
        base->fifo[DIAG_DATA] = 100 + npixels - 1;
        base->fifo[DIAG_DATA] = 100;

	dma_go ((caddr_t) &base->hq.gedma, vdma_wphys, 1, 
		 npixels*sizeof(int), 0, 1, 0x50 | VDMA_SYNC);

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA read (from memory to gfx) did not complete\n");
		return -1;
	}

#define TIMEOUT_VDMA    100000
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

	/* DMA write - from gfx to memory */
	if (rflag) {
		rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);

        	/* Inactivate the DMA sync source */
        	base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

		base->hq.fin1 = 0;
		base->fifo[DIAG_REDMA] = 0; /* GFX_TO_HOST */
		base->fifo[DIAG_DATA] = 100; /* x1 */
		base->fifo[DIAG_DATA] = 100; /* y1 */
		base->fifo[DIAG_DATA] = 100 + npixels - 1;
		base->fifo[DIAG_DATA] = 100;

		dma_go ((caddr_t) &base->hq.gedma, vdma_rphys, 1, 
		 	npixels*sizeof(int), 0, 1, 0x50 | VDMA_GTOH|VDMA_SYNC);

		if (dma_error_handler()) {
			msg_printf(ERR,"DMA write (from gfx to memory) did not complete\n");
			return -1;
		}

		/* Polling finish flag */
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

        	for (i=0; i<npixels; i++) {
            		if ((*rbuf & pixmask) != (*wbuf & pixmask)) {
		        	error_count++;
		        	msg_printf(ERR,
		           	"Address: 0x%08x, Expected: 0x%08x, Actual: 0x%08x\n",
		           	(unsigned int) rbuf, *wbuf & pixmask, *rbuf & pixmask);
	    		}
            		wbuf++; rbuf++;
		}
	}
    
	if (error_count)
            sum_error ("DMA default registers test");
    	else
            okydoky ();

    	return (error_count);
}
