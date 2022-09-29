#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "diagcmds.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern struct gr2_hw *base; 

/* external utility routines */
extern int dma_error_handler(void);

static unsigned long color[8] = {0x0, 0xff, 0xff00, 0xff0000,
			  0xffff00, 0xffff, 0xff00ff, 0xffffff};
int
gr2_yzoom_dma(argc, argv)
int argc;
char **argv;
{
	int npixels = 3;
	int yzoom = 2;
	int cache_mode = 1;	/* no snoop */
	int vdma_mode;
	int i;
	unsigned long *wbuf; 


   	msg_printf (VRB, "Virtual DMA y zoom test\n");

	if (argc > 4) {
	    msg_printf(ERR,"usage: %s [npixels] [yzoom] [cache_mode]\n", 
				argv[0]);
	    return(-1);
	}

	switch (argc) {
		case 2:
			atob(argv[1], &npixels);
			break;
		case 3:
			atob(argv[1], &npixels);
			atob(argv[2], &yzoom);
			break;
		case 4:
			atob(argv[1], &npixels);
			atob(argv[2], &yzoom);
			atob(argv[3], &cache_mode);
			break;
		default:
			break;
	}

	/* Max. number of pixels per scan line */
	if (npixels > 1000) npixels = 1000;

	basic_DMA_setup(cache_mode == 0);

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
	base->fifo[DIAG_DATA] = 100 + yzoom - 1;


	dma_go ((caddr_t) &base->hq.gedma,vdma_wphys, 1,
			npixels*(int)sizeof(int), 0, yzoom, vdma_mode);

	if (dma_error_handler()) {
		msg_printf (ERR, "DMA y zoom test failed\n");
		return -1;
	}
	
#define TIMEOUT_VDMA    100000
	for (i=0; !(base->hq.version & HQ2_FINISH1); i++) {

		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			sum_error("VDMA yzoomed test");
			return -1;
		}
		DELAY(1);
	}
        okydoky ();
	base->hq.fin1 = 0;
	return 0;

}
