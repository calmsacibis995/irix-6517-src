#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "libsk.h"
#include "diagcmds.h"
#include "libsc.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern struct gr2_hw *base; 

/* external utility routines */
extern int dma_error_handler();

static unsigned long color[8] = {0x0, 0xff, 0xff00, 0xff0000,
			  0xffff00, 0xffff, 0xff00ff, 0xffffff};
int
gr2_masksubst_dma(int argc, char **argv)
{
	int npixels = 32;
	int cache_mode = 1;	/* no snoop */
	int vdma_mode;
	int giomask;
	int i;
	unsigned long *wbuf; 



	switch (argc) {
	case 1:
		giomask = 0xffffffff;
		break;
	case 2:
		atob (argv[1], &giomask);
		break;
	case 3:
		atob (argv[1], &giomask);
		atob (argv[2], &cache_mode);
		break;
	default :
		msg_printf (ERR, "Usage : gr2_mask_dma [mask [cachemode]]\n");
		return -1;
	}

   	msg_printf (DBG, "Virtual DMA giomask test mask %x\n", giomask);

	basic_DMA_setup(cache_mode == 0);

	/*
	 * Set mask and sub so that the 'giomask' bits of the 
	 * programmed GIO address is replaced with the
	 * 'giomask' bits of &base->hq.gedma
	 */
        writemcreg(DMA_GIO_MASK,  giomask);
        writemcreg(DMA_GIO_SUB,  ((uint) (KDM_TO_PHYS(&base->hq.gedma))) & giomask);


	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned long *) PHYS_TO_K0(vdma_wphys); 

	for (i=0; i<npixels; i++) 
	    *wbuf++ = color[(i+1) & 0x7];

	if (cache_mode) /* writeback-invalidate data cache */
        	flush_cache();

	if (cache_mode)
		vdma_mode = VDMA_LBURST | VDMA_INCA | VDMA_SYNC;
	else
		vdma_mode = VDMA_SNOOP | VDMA_INCA | VDMA_SYNC;


        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

	base->hq.fin1 = 0;
	base->fifo[DIAG_REDMA] = 1;	/* Host to Gfx */
	base->fifo[DIAG_DATA] = 100; /* x1 */
	base->fifo[DIAG_DATA] = 100; /* y1 */
	base->fifo[DIAG_DATA] = 100 + npixels - 1;
	base->fifo[DIAG_DATA] = 100;

	/* Start the dma, with a hamburgered GIO address */

	dma_go (((caddr_t) &base->hq.gedma) & ~giomask, vdma_wphys, 1, 
			npixels*sizeof(int), 0, 1, vdma_mode);

	if (dma_error_handler()) {
		msg_printf (ERR, "DMA giomask test failed\n");
		return -1;
	}
	
#define TIMEOUT_VDMA    100000
	for (i=0; 1; i++) {
		if (base->hq.version & HQ2_FINISH1) 
			break;

		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			sum_error("VDMA giomask test");
			return -1;
		}
		DELAY(1);
	}
        okydoky ();
	base->hq.fin1 = 0;
	return 0;

}
