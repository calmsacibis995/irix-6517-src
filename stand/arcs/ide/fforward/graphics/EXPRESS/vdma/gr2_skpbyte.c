#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "libsk.h"
#include "sys/gr2_if.h"
#include "libsc.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern struct gr2_hw *base;

/* external utility routines */
extern int dma_error_handler();

extern unsigned char redmap[256];
extern unsigned char greenmap[256];
extern unsigned char bluemap[256];

#define NCOLORS	8

int
gr2_skipbyte(argc, argv)
int argc;
char **argv;
{
	register int i;
	int error_count = 0;
	int skip_byte = 1;
	int cache_mode = 1;	/* No snoop */
	int vdma_mode;
	int totalbytes;
	unsigned char *wbuf, *startbuf, *rbuf; 


   	msg_printf (VRB, "Virtual DMA skip-byte test\n");

	if (argc > 3) {
		msg_printf(ERR, "usage: %s [skip_byte][cache_mode]\n", 
			argv[0]);
		return -1;
	}

	if (argc == 2)
		atob(argv[1], &skip_byte);
	else if (argc == 3) {
		atob(argv[1], &skip_byte);
		atob(argv[2], &cache_mode);
	}
		
	skip_byte &= 3;

	basic_DMA_setup(cache_mode == 0); 
	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys); 
	wbuf += skip_byte;
	startbuf = wbuf;

	for (i=0 ; i < NCOLORS; i++ ) {
    		*wbuf++ = redmap[i] ;
		*wbuf++ = greenmap[i];
		*wbuf++ = bluemap[i];
	}

	totalbytes = skip_byte + 3 * NCOLORS;

	/* writeback-invalidate data cache */
	if (cache_mode)
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


	if (cache_mode)
		vdma_mode = VDMA_SYNC | VDMA_LBURST | VDMA_INCA;
	else
		vdma_mode = VDMA_SYNC | VDMA_SNOOP | VDMA_INCA;

	/* DMA read - from memory to gfx */
	dma_go ((caddr_t) &base->xmapall.clut,K0_TO_PHYS((uint) startbuf),
		 1, totalbytes, 0, 1, vdma_mode);
		
	if (dma_error_handler()) {
		msg_printf (ERR, "skip-byte DMA test failed");
		return -1;
	}

	/* Read back and compare */
	
	/* Set the starting index */
	base->xmapall.addrhi = (GR2_8BITCMAP_BASE >> 8) & 0xff;
	base->xmapall.addrlo = GR2_8BITCMAP_BASE & 0xff;

	rbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys);	

	/* Wait for XMAP external FIFO to be empty */
	while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY)
		;
	/* Wait until it gets into vertical blanking state */
	while (!(*(volatile unchar *)PHYS_TO_K1(VRSTAT_ADDR) & VRSTAT_MASK))
		;

	for (i=0; i < NCOLORS; i++) {
		*rbuf++ = base->xmap[0].clut;
		*rbuf++ = base->xmap[0].clut;
		*rbuf++ = base->xmap[0].clut;
	}

	rbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys);
        	
	for (i=0; i < NCOLORS; i++) {
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
		sum_error("VDMA skip-byte test");
	else
            okydoky ();

	return error_count;
}
