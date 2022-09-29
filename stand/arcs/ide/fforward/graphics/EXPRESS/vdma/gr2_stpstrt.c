#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "libsk.h"
#include "sys/gr2_if.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern struct gr2_hw *base;

/* external utility routines */
extern int dma_error_handler();

unsigned char redmap[256] = {
  0,255,  0,255,  0,255,  0,255, 85,198,113,142,113,142, 56,170,170, 85,170, 85,
170, 85,170, 85,170, 85,170, 85,170, 85,170, 85, 10, 20, 30, 40, 51, 61, 71, 81,
 91,102,112,122,132,142,153,163,173,183,193,204,214,224,234,244,  0,  0,  0,  0,
  0,  0,  0,  0, 63, 63, 63, 63, 63, 63, 63, 63,127,127,127,127,127,127,127,127,
191,191,191,191,191,191,191,191,255,255,255,255,255,255,255,255,  0,  0,  0,  0,
  0,  0,  0,  0, 63, 63, 63, 63, 63, 63, 63, 63,127,127,127,127,127,127,127,127,
191,191,191,191,191,191,191,191,255,255,255,255,255,255,255,255,  0,  0,  0,  0,
  0,  0,  0,  0, 63, 63, 63, 63, 63, 63, 63, 63,127,127,127,127,127,127,127,127,
191,191,191,191,191,191,191,191,255,255,255,255,255,255,255,255,  0,  0,  0,  0,
  0,  0,  0,  0, 63, 63, 63, 63, 63, 63, 63, 63,127,127,127,127,127,127,127,127,
191,191,191,191,191,191,191,191,255,255,255,255,255,255,255,255,  0,  0,  0,  0,
  0,  0,  0,  0, 63, 63, 63, 63, 63, 63, 63, 63,127,127,127,127,127,127,127,127,
191,191,191,191,191,191,191,191,255,255,255,255,255,255,255,255,
};

unsigned char greenmap[256] = {
  0,  0,255,255,  0,  0,255,255, 85,113,198,142,113, 56,142,170,170, 85,170, 85,
170, 85,170, 85,170, 85,170, 85,170, 85,170, 85, 10, 20, 30, 40, 51, 61, 71, 81,
 91,102,112,122,132,142,153,163,173,183,193,204,214,224,234,244,  0, 36, 72,109,
145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,
  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,
145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,
  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,
145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,
  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,
145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,
  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,
145,182,218,255,  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,
  0, 36, 72,109,145,182,218,255,  0, 36, 72,109,145,182,218,255,
};

unsigned char bluemap[256] = {
  0,  0,  0,  0,255,255,255,255, 85,113,113, 56,198,142,142,170,170, 85,170, 85,
170, 85,170, 85,170, 85,170, 85,170, 85,170, 85, 10, 20, 30, 40, 51, 61, 71, 81,
 91,102,112,122,132,142,153,163,173,183,193,204,214,224,234,244,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 63, 63, 63, 63,
 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,127,127,127,127,
127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,
127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,191,191,191,191,
191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,
191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,191,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

/* Since we are testing stop and restart DMA, I would like to turn the
 * snooping off here */
int
gr2_stoprestart_dma(argc, argv)
int argc;
char **argv;
{
	register int i,j;
	int error_count = 0;
	unsigned char *wbuf, *rbuf; 
	int DMAstop = 0;


   	msg_printf (VRB, "Virtual DMA stop/restart test\n");

	basic_DMA_setup(0);

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys); 

	for (i=0 ; i < GR2_XMAP_NCOLMAP_VR; i++ ) {
    		*wbuf++ = redmap[i] ;
		*wbuf++ = greenmap[i];
		*wbuf++ = bluemap[i];
	}

	flush_cache();

	/* *** HERE, we assume the xmap was initialize to 8-bit color
	   *** index mode .*/

        /* set DMA_SYNC source to 0 to synchronize the transfer with vertical
           retrace. */
        base->hq.dmasync = 0;

	/* Set the starting index */
	base->xmapall.addrhi = (GR2_8BITCMAP_BASE >> 8) & 0xff;
	base->xmapall.addrlo = GR2_8BITCMAP_BASE & 0xff;

#define GR2_VR_TIMEOUT          10000
	for (i = 0; i < GR2_VR_TIMEOUT; i++) {
	   	/* wait for FIFO becomes less than half full */
		/* Asserted low, if fifo more than half full. */
		if (base->xmap[0].fifostatus & GR2_FIFO_HALF_FULL)
			break;
		DELAY(1);
	}
	if (i == GR2_VR_TIMEOUT) {
		msg_printf(VRB, "Xmap FIFO more than half full!\n");
		return -1;
	}

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADR,  vdma_wphys); 

	writemcreg(DMA_SIZE,  (1 << 16) | (3*GR2_XMAP_NCOLMAP_VR));

	/* Setup DMA mode: long burst, no snooping, incrementing addressing,
           fill mode disabled, sync enabled, GTOH */
       	writemcreg(DMA_MODE,  VDMA_LBURST | VDMA_INCA | VDMA_SYNC);

	/* Start DMA */
	writemcreg(DMA_GIO_ADRS,  (uint) &base->xmapall.clut); 

/* ??? Stop the DMA after 1 micro sec. Might have to fine tune this number */
#define TIME_TO_STOP_DMA	1

	i = 0;
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40) {
		if (i == TIME_TO_STOP_DMA) {
			writemcreg(DMA_STDMA,  0);
			break;
		}
		i++;
		DELAY(1);
	}

	/* Wait for DMA to stop */
	vdma_wait();

	/* If DMA was stopped before completing the pixel data transfer */
	if (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 8)
		msg_printf (VRB,"DMA complete before we could stop it !\n");
	else {
		/* Restart the DMA */
		writemcreg(DMA_STDMA,  1);

		/* Poll until DMA is stopped */
		vdma_wait();
	}

	if (dma_error_handler()) {
		sum_error ("Stop/Restart DMA test");
		return -1;
	}

	/* Read back and compare */
	
	/* Set the starting index */
	base->xmapall.addrhi = (GR2_8BITCMAP_BASE >> 8) & 0xff;
	base->xmapall.addrlo = GR2_8BITCMAP_BASE & 0xff;

	rbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys);	
	for (i=0; i < GR2_XMAP_NCOLMAP_VR; i+=8) {
		for (j=0; j<8; j++) {
			/* Wait for XMAP external FIFO to be empty */
			while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY)
				;

			/* Wait until it gets into vertical blanking state */
			while (!(*(volatile unchar *)PHYS_TO_K1(VRSTAT_ADDR) & VRSTAT_MASK))
				;

			*rbuf++ = base->xmap[0].clut;
			*rbuf++ = base->xmap[0].clut;
			*rbuf++ = base->xmap[0].clut;
		}
	}

	rbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys);

	for (i=0; i < GR2_XMAP_NCOLMAP_VR; i++) {
		if (*rbuf != redmap[i]) {
			msg_printf(ERR, "Red component of color %d incorrect Expected 0x%x, Actual 0x%x\n", i, redmap[i], *rbuf);
			error_count++;
		}
		rbuf++;
		if (*rbuf != greenmap[i]) {
			msg_printf(ERR, "Green component of color %d incorrect Expected 0x%x, Actual 0x%x\n", i, greenmap[i], *rbuf);
			error_count++;
		}
		rbuf++;
		if (*rbuf != bluemap[i]) {
			msg_printf(ERR, "Blue component of color %d incorrect Expected 0x%x, Actual 0x%x\n", i, bluemap[i], *rbuf);
			error_count++;
		}
		rbuf++;
	}

	if (error_count)
		sum_error("VDMA stop/restart test");
	else
            okydoky ();

	return error_count;
}
