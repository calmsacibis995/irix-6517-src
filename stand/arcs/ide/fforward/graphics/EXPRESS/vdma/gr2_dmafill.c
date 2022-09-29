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
 *  VDMA Memory Fill Diagnostic 
 */

#ident	"$Revision: 1.7 $"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "dma.h"

/* external utility routines */
extern int dma_error_handler();
extern void flush_cache();

static int dma_memfill(unsigned int, int, int, int, int, int, int);
static void verify_buf(unsigned int *, int, int, char *);
static void clearbuf (unsigned int *, int, int);

static int error_count = 0;

/*
 * Test the VDMA memory fill feature.
 *
 * XXX Only the likely uses of this
 * feature are tested here as yet,
 * namely filling a rectangle (stride >= 0).
 *
 */


static int cache_mode = 2;	

/*
cache_mode =	
		0 : we use K0, we snoop
		1 : we use K0, flush cache by hand, don't snoop
		2 : we use K1, don't snoop
*/
	
gr2_dmafill(argc, argv)
int argc;
char **argv;
{
	int nwords = 100;		/* words per row */
	int nrows = 1;			/* what you think */
	int fillpat = 0x48454c50;	/* the word value to write */
	unsigned int *wbuf; 
	int i;
	int silent = 0;
	int patn1, patn2;
	int vdma_modes = VDMA_GTOH | VDMA_LBURST | VDMA_FILL | VDMA_INCA;


	/* If silent then don't show any msgs */
	if (argc == 2)
	  if (!(strcmp (argv[1], "-s"))) {
		silent = 1;
		argc = 1;
	}
	if (!silent) msg_printf (VRB, "Virtual DMA memory fill test\n");

	if (argc > 4) {
	    msg_printf(ERR,"usage: %s [nwords [nrows [cache_policy]]]\n", 
				argv[0]);
	    return(-1);
	}

	switch (argc) {
		case 2:
			atob(argv[1], &nwords);
			break;
		case 3:
			atob(argv[1], &nwords);
			atob(argv[2], &nrows);
			break;
		case 4:
			atob(argv[1], &nwords);
			atob(argv[2], &nrows);
			atob(argv[3], &cache_mode);
			break;
		default:
			break;
	}

	if (nrows * nwords <= 0 || nrows * nwords > 1024) {
		msg_printf (ERR, " require 0 < nrows * nwords <= 1024\n");
		return -1;
	}
		
	if (cache_mode < 0 || cache_mode > 2)
		cache_mode = 2;

	basic_DMA_setup (cache_mode == 0);

	if (cache_mode) { /* Disable Snooping */

		msg_printf (DBG, "gr2_memfill: disabling snoop\n");
	}
	else {
		msg_printf (DBG, "gr2_memfill: enabling snoop\n");
		vdma_modes |= VDMA_SNOOP;
		vdma_modes &= ~VDMA_LBURST;
	}
	

	if (cache_mode == 2)  /* k1seg kernel text/data uncached */
		wbuf = (unsigned int *) PHYS_TO_K1(vdma_wphys); 
	else
		wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 

	/*
	 * Fill nrows * nwords words.
	 * Quit if VDMA error occurs.
	 */

	clearbuf (wbuf, nrows * nwords, ~fillpat);

	if (cache_mode == 1)
		flush_cache();

	if (dma_memfill (vdma_wphys, nrows, nwords << 2, 0, 1, 
				vdma_modes, fillpat))
		return error_count;

	verify_buf (wbuf, nrows * nwords, fillpat, "dmafill: stride 0");

	
	/*
	 * Like before, only this time the last 
	 * word in each row is skipped.
	 * Quit if VDMA error occurs.
	 */

	if (nwords < 2)
		nwords = 2;

	clearbuf (wbuf, nrows * nwords, ~fillpat);
	if (cache_mode == 1)
		flush_cache();

	if (dma_memfill (vdma_wphys, nrows, (nwords - 1) << 2, 4, 1, 
			vdma_modes, fillpat))
		return error_count;

	for (i = 0; i < nrows; i++) {
		verify_buf (wbuf + nwords * i, nwords - 1, fillpat,
				"dmafill: stride 4");
		verify_buf (wbuf - 1 + nwords * (i + 1), 1, ~fillpat,
				"dmafill: stride 4");
		if (error_count)
			break;
	}
	
	/* 
	 * spec says addressed WORDS are written - setup a 2 byte
	 * transfer across a word boundary and verify both
	 * words are written.
	 * Quit if VDMA error occurs.
	 */

	wbuf[0] = wbuf[1] = ~fillpat;
	if (cache_mode == 1)
		flush_cache();

	if (dma_memfill (vdma_wphys + 3, 1, 2, 0, 1, vdma_modes, fillpat))
		return error_count;
	
	patn1 = ((~fillpat & ~0xff) | (fillpat & 0xff));
	patn2 = ((~fillpat & ~0xff000000) | (fillpat & 0xff000000));

	if (wbuf[0] != patn1 || wbuf[1] != patn2) {
		msg_printf (ERR,"dmafill: SB 3 BC 2 read %x %x expect %x %x\n",
			wbuf[0], wbuf[1], patn1, patn2);
		error_count++;
	}
	
	if ((error_count == 0) && (!silent))
		okydoky();

    	return (error_count);
}



/*
 * Write fillval into memory.
 *
 * XXX We would use dma_go(), but it converts our
 * fillval from KDM to physical.
 */

static int
dma_memfill (unsigned int buf,
	int h, int w, int s, int z, int modes, int fillval)
{
	int i;

	/* Set DMA memory address */
	writemcreg(DMA_MEMADR, i = buf); /* physical address */

msg_printf (DBG, "dma_memfill: wrote %x to DMA_MEMADR\n", i);

	/* Set transfer size height and width */
	writemcreg(DMA_SIZE, i = ((h << 16) | w));

msg_printf (DBG, "dma_memfill: wrote %x to DMA_SIZE\n", i);

        /* Set zoom factor and stride */
        writemcreg(DMA_STRIDE, i = (z << 16) | (s & 0xffff));

msg_printf (DBG, "dma_memfill: wrote %x to DMA_STRIDE\n", i);

	/* Set DMA mode */
        writemcreg(DMA_MODE, i = modes);

msg_printf (DBG, "dma_memfill: wrote %x to DMA_MODE\n", i);

	/* Start DMA */
	writemcreg(DMA_GIO_ADRS, i = fillval);

msg_printf (DBG, "dma_memfill: wrote %x to DMA_GIO_ADRS\n", i);

	vdma_wait();	 /* Poll until DMA stops or times out */

	if (dma_error_handler()) {
		sum_error ("VDMA memory fill test");
		return 1;
	}
	return 0;
}

static void
verify_buf (unsigned int *buf, int nwords, int val, char *errmsg )
{
	int i;

	for (i = 0; i < nwords; i++)
		if (*buf++ != val) {
			msg_printf (ERR, "%s word %d read %x expect %x\n",
				errmsg, i, buf[-1], val); 
			error_count++;
		}
}

static void
clearbuf (unsigned int *buf, int nwords, int val )
{
	while (nwords-- > 0)
		*buf++ = val;
}

