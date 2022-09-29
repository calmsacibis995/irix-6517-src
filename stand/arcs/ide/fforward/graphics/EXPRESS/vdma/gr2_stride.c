#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "dma.h"
#include "diagcmds.h"

/* external variables */
extern struct gr2_hw *base; 

/* external utility routines */
extern int dma_error_handler(void);
static unsigned int color[8] = {0x0, 0xff, 0xff00, 0xff0000,
                          0xffff00, 0xffff, 0xff00ff, 0xffffff};
/* ###
 * We are testing bottom-to-top and top-to-bottom DMA by using positive 
 * and negative stride for EXPRESS.  We all know RE3 can only fill out
 * pixels in top-to-bottom direction, so that we have to use negative
 * stride to implement bottom-to-top case.
 * 32-bit pixels were used, since we only have this pixel type supported
 * in microcode.
 */
int
gr2_stride_dma(int argc, char **argv)
{
	register int i,j;
	int error_count = 0;
	int imgwidth = 12;
	int stride = 0;
	int cache_mode = 1;	/* no snoop */
	int vdma_mode;

	int framewidth;
	int imgheight;
	int npixels;
	unsigned int *wbuf, *rbuf; 


   	msg_printf (VRB, "Virtual DMA positive/negative stride test\n");

	if (argc > 3) {
	    msg_printf(ERR,"usage: %s [pixels][stride(in words)][cache_mode] \n", 
				argv[0]);
	    return(-1);
	}
	
        switch (argc) {
                case 2:
                        atob(argv[1], &imgwidth);
                        break;
                case 3:
                        atob(argv[1], &imgwidth);
                        atob(argv[2], &stride);
                        break;
                case 4:
                        atob(argv[1], &imgwidth);
                        atob(argv[2], &stride);
                        atob(argv[3], &cache_mode);
                        break;
                default:
                        break;
        }

        if (stride < 0) {
                msg_printf (ERR,"VDMA stride: stride %d is negative\n");
                return -1;
        } 
	if (stride > 32767 || -(2*imgwidth+stride) < -32768) {
		msg_printf (ERR,"VDMA stride: stride %d is too big\n");
                return -1;
	}

	basic_DMA_setup(cache_mode == 0);	
	
	/*
	 * Initialize data buffer  
	 */
	framewidth = imgwidth + stride;
	imgheight = 0x400 / imgwidth;
	if (imgheight > 8) imgheight = 8;
	npixels = framewidth * imgheight;

	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 

	for (i=0; i<npixels; i++) 
		*wbuf++ = color[(i+1) & 0x7];

        if (cache_mode) /* writeback-invalidate data cache */
                flush_cache();

	/***** Positive stride case:  pixels were drawn from top to bottom. */

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

	base->hq.fin1 = 0;
       	base->fifo[DIAG_REDMA] = 1;     /* Host to Gfx */
       	base->fifo[DIAG_DATA] = 100; /* x1 */
       	base->fifo[DIAG_DATA] = 100; /* y1 */
       	base->fifo[DIAG_DATA] = 100 + imgwidth - 1; /* x2 */
       	base->fifo[DIAG_DATA] = 100 + imgheight - 1; /* y2 */

	if (cache_mode)
		vdma_mode = VDMA_LBURST | VDMA_SYNC | VDMA_INCA;
	else
		vdma_mode = VDMA_SYNC | VDMA_INCA;

	dma_go ((caddr_t) &base->hq.gedma, vdma_wphys, imgheight, 
			imgwidth*(int)sizeof(int), stride * (int)sizeof(int),
			1, vdma_mode);

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA read (from memory to gfx) did not complete\n");
		return -1;
	}

#define TIMEOUT_VDMA    100000
	for (i=0; !(base->hq.version & HQ2_FINISH1); i++) {

		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			return -1;
		}
		DELAY(1);
	}

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

	base->hq.fin1 = 0;
	base->fifo[DIAG_REDMA] = 0; /* GFX_TO_HOST */
	base->fifo[DIAG_DATA] = 100; /* x1 */
	base->fifo[DIAG_DATA] = 100; /* y1 */
	base->fifo[DIAG_DATA] = 100 + imgwidth - 1;
	base->fifo[DIAG_DATA] = 100 + imgheight - 1;

	dma_go ((caddr_t) &base->hq.gedma, vdma_rphys, imgheight, 
			imgwidth*(int)sizeof(int), stride*(int)sizeof(int),
			1, vdma_mode | VDMA_GTOH);

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA write (from gfx to memory) did not complete\n");
		return -1;
	}

	/* Polling finish flag */
	for (i=0; !(base->hq.version & HQ2_FINISH1); i++) {
		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			return -1;
		}
		DELAY(1);
	}

	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
	rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);

        for (i=0; i<imgheight; i++) {
		for (j=0; j<imgwidth; j++) {
            		if (*rbuf != *wbuf) {
		        	error_count++;
		        	msg_printf(ERR,
		           	"Positive stride DMA Expected: 0x%08x, Actual: 0x%08x\n", 
				*wbuf, *rbuf);
	    		}
            		wbuf++; rbuf++;
		}
		wbuf += stride;
		rbuf += stride;
   	} 

	if (argc > 0) {	/* Negative stride doesn't work ?!?! */
		msg_printf (DBG,"\nSkipping negative stride test\n\n");
		okydoky ();
		return 0;
	}

	/***** Negative stride case: pixels drawn from bottom to top */

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;
	base->hq.fin1 = 0;
       	base->fifo[DIAG_REDMA] = 1;     /* Host to Gfx */
       	base->fifo[DIAG_DATA] = 100; /* x1 */
       	base->fifo[DIAG_DATA] = 100; /* y1 */
       	base->fifo[DIAG_DATA] = 100 + imgwidth - 1; /* x2 */
       	base->fifo[DIAG_DATA] = 100 + imgheight - 1;

	dma_go ((caddr_t) &base->hq.gedma, vdma_wphys + 
		(npixels - framewidth)*(int)sizeof (int), 
		imgheight,imgwidth*(int)sizeof(int), 
		-(imgwidth+framewidth)*(int)sizeof(int), 1, vdma_mode);

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA read (from memory to gfx) did not complete\n");
		return -1;
	}

	for (i=0; !(base->hq.version & HQ2_FINISH1); i++) {
		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			return -1;
		}
		DELAY(1);
	}

	/* Read back and compare for negative stride DMA */
	rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);

	/* Invalidate data cache */
	if (cache_mode)
		flush_cache();

        /* Inactivate the DMA sync source */
        base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

	base->hq.fin1 = 0;
	base->fifo[DIAG_REDMA] = 0; /* GFX_TO_HOST */
	base->fifo[DIAG_DATA] = 100; /* x1 */
	base->fifo[DIAG_DATA] = 100; /* y1 */
	base->fifo[DIAG_DATA] = 100 + imgwidth - 1;
	base->fifo[DIAG_DATA] = 100 + imgheight - 1;

	dma_go ((caddr_t) &base->hq.gedma, vdma_rphys + 
			(npixels - framewidth)*(int)sizeof (int), 
			imgheight,imgwidth*(int)sizeof(int), 
			-(imgwidth+framewidth)*(int)sizeof(int), 1, vdma_mode);

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA write (from gfx to memory) did not complete\n");
		return -1;
	}

	/* Polling finish flag */
	for (i=0; !(base->hq.version & HQ2_FINISH1); i++) {
		if (i > TIMEOUT_VDMA) {
			msg_printf(ERR,"TIMEOUT gfx DMA did not complete (finishflag not set)\n");
			base->hq.fin1 = 0;
			return -1;
		}
		DELAY(1);
	}


	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
	rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
        for (i=0; i<imgheight; i++) {
		for (j=0; j<imgwidth; j++) {
            		if (*rbuf != *wbuf) {
		        	error_count++;
		        	msg_printf(ERR,
		           	"Negative stride DMA Expected: 0x%08x, Actual: 0x%08x\n", 
				*wbuf, *rbuf);
	    		}
            		wbuf++; rbuf++;
		}
		wbuf += stride;
		rbuf += stride;
   	} 

	if (error_count)
            sum_error ("DMA: positive/negative stride test");
    	else
            okydoky ();

    	return (error_count);
}
