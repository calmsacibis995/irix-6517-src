/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  VDMA Line Stride Diagnostic
 */

#ident	"$Revision: 1.9 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/lg1hw.h"
#include "uif.h"
#include "dma.h"
#include "light.h"
#include <libsc.h>
#include <libsk.h>

/* external variables */
extern long _dcache_size;
extern long _dcache_linesize;

#define rex_getpixel(x,y) (rex_get4pixel((x),(y)) >> 24)

static int cache_mode = 2;

/*
 * cache_mode =		0 : we use K0, we snoop
 *			1 : we use K0, flush cache by hand, don't snoop
 *			2 : we use K1, don't snoop
 */

/*
 * Test the VDMA line stride feature.
 */

int
lg1_linestride_dma(argc, argv)
int argc;
char **argv;
{
        int dualhead = rex_dualhead();

	int error_count = 0;
	int imgwidth = 12;	/* image width in pixels */
	int stride = 0;		/* stride in pixels */

	int framewidth;		/* imgwidth + stride */
	int imgheight;		/* a reasonable height for the image */
	int npixels;		/* framewidth * imgheight */
	int i, j;
	unsigned char *wbuf, *rbuf; 

	/* 
	 * If snoop is on, use short burst, else long.
	 */
	
	int vdma_modes = VDMA_INCA;

   	msg_printf (VRB, "Virtual DMA line stride test\n");


	if (argc > 4) {
	    msg_printf(ERR,"usage: %s [width [stride [cache_mode]]]\n", 
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

        if ((imgwidth + stride) % 4) {
                msg_printf(ERR, "width + stride must be multiple of 4 (quadmode only)\n");
                return -1;
	}
	if (imgwidth < 1 || imgwidth > 1024 - 100) {
		msg_printf (ERR,"VDMA line stride: require 0 < width < 924");
		return -1;
	}
	if (stride + imgwidth > 4096 || stride + imgwidth < 0) {
		msg_printf (ERR,"VDMA line stride: stride %d is too big\n");
		return -1;
	}

	if (cache_mode < 0 || cache_mode > 2)
		cache_mode = 2;

	if (cache_mode == 0) 
		vdma_modes |= VDMA_SNOOP;
	else
		vdma_modes |= VDMA_LBURST;


	basic_DMA_setup(cache_mode == 0);


	/* Initialize Rex */
	rex_clear();
	vc1_on();

	/*
	 * Initialize data buffer 
	 */
	framewidth = imgwidth + stride;
	imgheight = 0x1000 / imgwidth;
	if (imgheight > 4)
		imgheight = 4;
	npixels = framewidth * imgheight;

	if (cache_mode == 2) {
		wbuf = (unsigned char *) PHYS_TO_K1(vdma_wphys); 
		rbuf = (unsigned char *) PHYS_TO_K1(vdma_rphys); 
	}
	else {
		wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys); 
		rbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys); 
	}

	for (i = 0; i < npixels; i++) 
	    *wbuf++ = i;

	/* Flush the data to RAM */
	if (cache_mode == 1) 
		flush_cache();

    	REX->set.xstarti = 100; 
	REX->set.xendi = 100 + imgwidth - 1;
    	REX->set.ystarti = 100;     
	REX->set.yendi = 100 + imgheight - 1;
    	REX->go.command = ( 0);     /* NO-OP */

    	REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | 
				QUADMODE | REX_DRAW);

	if (stride >= 0)
		dma_go ((caddr_t)&REX->go.rwaux1, vdma_wphys, imgheight,
			imgwidth, stride, 1, vdma_modes | DMA_READ);
	else /* start at last line */
		dma_go ((caddr_t)&REX->go.rwaux1,vdma_wphys+npixels-framewidth, 
			imgheight, imgwidth, stride, 1,
			 vdma_modes | DMA_READ);

	if (dma_error_handler()) {
		sum_error ("VDMA line stride test (HTOG)");
		return ++error_count;
	}

	/*
	 * DMA read the data back.  Have to do 
	 * this a line at a time
	 * because of problem with REX.
	 */ 

	if (stride < 0)
		rbuf += npixels - framewidth;

	/*
	 * Set up REX to give us our pixels back.
	 */
	REX->set.xstarti = 100;
	REX->set.xendi = 100 + imgwidth - 1;
	REX->set.ystarti = 100;
	REX->set.yendi = 100 + imgheight - 1;

	REX->go.command = REX_LDPIXEL|QUADMODE;
	REX->set.command = REX_LDPIXEL|QUADMODE|BLOCK|XYCONTINUE;

	/*
	 * Read the data.
	 */

	for (i = 0; i < imgheight; i++) {
		if (dualhead)  {
			uint *buf = (uint *)rbuf;
                        for (j = 0; j < (imgwidth+3)/4; j++) 
                                buf[j] = REX->go.rwaux1;
		}
		else {
			dma_go ((caddr_t)&REX->go.rwaux1, K1_TO_PHYS(rbuf), 1,
				imgwidth, stride, 1, vdma_modes | DMA_WRITE);

			if (dma_error_handler()) {
				sum_error ("VDMA line stride test (GTOH)");
				return ++error_count;
			}
		}

		rbuf += (imgwidth + stride);
	}

		
	
	/*
	 * Compare what we wrote with what we read back.
	 * If different, look in the refresh buffer to 
	 * find out whether read or write failed
	 * (maybe both !).
	 */
	
	if (cache_mode == 2) {
		wbuf = (unsigned char *) PHYS_TO_K1(vdma_wphys); 
		rbuf = (unsigned char *) PHYS_TO_K1(vdma_rphys); 
	}
	else {
		wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys); 
		rbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys); 
	}

	for (i = 0; i < imgheight; i++) {
		for (j = 0; j < imgwidth; j++)
			if (wbuf[j] != rbuf[j]) {
				msg_printf (ERR,
			"VDMA wrote %x read %x offset %x actual pixel is %x\n",
					wbuf[j], rbuf[j],
					(((int)rbuf) & 0xfff) + j, 
						rex_getpixel(100+j, 100+i));
				error_count++;
			}
		wbuf += (imgwidth + stride);
		rbuf += (imgwidth + stride);
	}

	if (error_count == 0)
		okydoky();
	else
		sum_error ("VDMA line stride test (GTOH)");
    	return (error_count);
}

