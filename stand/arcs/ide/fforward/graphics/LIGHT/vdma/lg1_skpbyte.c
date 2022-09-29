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
 *  VDMA Skip Byte Diagnostic
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

/*
cache_mode =
                0 : we use K0, we snoop
                1 : we use K0, flush cache by hand, don't snoop
                2 : we use K1, don't snoop
*/
static int cache_mode = 2;

/*
 * Test VDMA write with SB != 0.
 *
 * XXX
 * Should take stride (in the GL sense)
 * as a parameter - ditto linestride test.
 */

int
lg1_skipbyte(argc, argv)
int argc;
char **argv;
{
        int dualhead = rex_dualhead();

	int error_count = 0;
	int imgwidth = 12;	/* image width in pixels */
	int stride = 0;		/* stride in pixels */
	int skip_byte = 1;	/* offset into word of first pixel */

	int imgheight;		/* a reasonable height for the image */
	int npixels;		/* imgwidth * imgheight */
	int i, j;
	unsigned char *wbuf, *rbuf; 

	
	int vdma_modes = VDMA_INCA | VDMA_LBURST; /* not snooping */

   	msg_printf (VRB, "Virtual DMA skip byte test\n");

	if (argc > 4) {
	    msg_printf(ERR,"usage: %s [width [skip_byte [cache_mode]]]\n", 
				argv[0]);
	    return(-1);
	}

	switch (argc) {
		case 2:
			atob(argv[1], &imgwidth);
			break;
		case 3:
			atob(argv[1], &imgwidth);
			atob(argv[2], &skip_byte);
			break;
		case 4:
			atob(argv[1], &imgwidth);
			atob(argv[2], &skip_byte);
			atob(argv[3], &cache_mode);
			break;
		default:
			break;
	}

	if (imgwidth < 1 || imgwidth > 1024 - 100) {
		msg_printf (ERR,"VDMA skip byte: require 0 < width < 924");
		return -1;
	}

	if (cache_mode < 0 || cache_mode > 2)
		cache_mode = 2;

	skip_byte &= 3;

	basic_DMA_setup(cache_mode == 0);

	if (cache_mode == 0) {   /* Enable Snooping */
		vdma_modes &= ~VDMA_LBURST;
		vdma_modes |= VDMA_SNOOP;
	}


	/* Initialize Rex */
	rex_clear();
	vc1_on();

	/*
	 * Initialize data buffer 
	 */
	imgheight = 0x1000 / imgwidth;
	if (imgheight > 64)
		imgheight = 64;
	npixels = imgwidth * imgheight;

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

	if (cache_mode == 1) /* writeback-invalidate data cache */
        	flush_cache();

    	REX->set.xstarti = 100 - skip_byte; 	/* fudge xstart */
	REX->set.xendi = 100 + imgwidth - 1;
    	REX->set.ystarti = 100;     
	REX->set.yendi = 100 + imgheight - 1;
    	REX->go.command = ( 0);     /* NO-OP */

    	REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | 
					QUADMODE | REX_DRAW);

	/*
	 * Calculate stride.  We choose stride such that
	 * (stride + imgwidth) % 4 == 0, as required by Rex -
	 * start byte must be same for each line.
	 */

	stride = ((imgwidth ^ 3) + 1) & 3;

	dma_go ((caddr_t)&REX->go.rwaux1, vdma_wphys + skip_byte, imgheight,
		imgwidth, stride, 1, vdma_modes | DMA_READ);

	if (dma_error_handler()) {
		sum_error ("VDMA line stride test (HTOG)");
		return ++error_count;
	}


	/*
	 * Read the data.  REX1 can only read pixels per scanline.
	 */

	for (i = 0; i < imgheight; i++) {
		/*
	 	 * Set up REX to give us our pixels back.
	 	 */
		REX->set.xstarti = 100;
		REX->set.xendi = 100 + imgwidth - 1;
		REX->set.ystarti = 100 + i;
		REX->set.yendi = 100 + i;

		REX->go.command = REX_LDPIXEL|QUADMODE;
		REX->set.command = REX_LDPIXEL|QUADMODE|BLOCK|XYCONTINUE;

		if (dualhead) {
			uint *buf = (uint *)rbuf;

                        for (j = 0; j < (imgwidth+3)/4; j++) 
                                buf[j] = REX->go.rwaux1;
		}
 		else {
			dma_go ((caddr_t)&REX->go.rwaux1, KDM_TO_PHYS(rbuf), 1, 
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
	wbuf += skip_byte;

	for (i = 0; i < imgheight; i++) {
		for (j = 0; j < imgwidth; j++) {
			if (wbuf[j] != rbuf[j]) {
				msg_printf (ERR,
			"VDMA wrote %x read %x offset %x actual pixel is %x\n",
					wbuf[j], rbuf[j], ((uint)rbuf) & 0xfff, 
						rex_getpixel(100+j, 100+i));
				error_count++;
			}
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
