#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/sema.h"
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

#define NUM_SHORT_WORDS	24
#define VSRAM_START     0x7000 

/* ### We are not testing true GIO64 decrementing addressing here! */
/* ### But word-aligned address decrementing is tested. */
int
gr2_decreadr_dma(argc, argv)
int argc;
char **argv;
{
	int error_count = 0;
	int cache_mode = 1;
	int vdma_mode;
	int i;
	unsigned short *wbuf, *rbuf; 
	unsigned int endadr;
 

   	msg_printf (VRB, "Virtual DMA decrement addressing test\n");

	if (argc > 2) {
		msg_printf(ERR, "usage: %s [cache_mode]\n", argv[0]);
		return -1;
	}
	
	if (argc == 2)
		atob(argv[1], &cache_mode);

	basic_DMA_setup(0);

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned short *) PHYS_TO_K0(vdma_wphys); 

	for (i=0; i<NUM_SHORT_WORDS; i++) {
   		*wbuf++ = (i+1) & 0xff;
	}

	endadr = ((uint) wbuf) - 1;

	/* writeback-invalidate data cache */
	if (cache_mode)
		flush_cache();

        /* set DMA_SYNC source to 0 to synchronize the transfer with vertical
           retrace. */
        base->hq.dmasync = 0;

        base->vc1.addrhi = VSRAM_START >> 8;
        base->vc1.addrlo = VSRAM_START & 0xff;

	/* DMA read - from memory to vc1 */

	if (cache_mode)
		/* Setup DMA mode: long burst, no snooping, 
		 * decrementing addressing, fill mode disabled, 
		 * sync disabled, DMA read */
       		vdma_mode = VDMA_LBURST | VDMA_SYNC;
	else
		/* Setup DMA mode: short burst, snooping,
		 * decrementing addressing, fill mode disabled,
		 * sync disabled, DMA read */
		vdma_mode = VDMA_SNOOP | VDMA_SYNC;

	dma_go ((caddr_t) &base->vc1.sram, K0_TO_PHYS(endadr), 1, 
		NUM_SHORT_WORDS*2, 0, 1, vdma_mode);

	if (dma_error_handler()) {
		msg_printf(ERR, "VC1 SRAM DMA failed\n");
		return -1;
	}

	/* Read back and compare */
	rbuf = (unsigned short *) PHYS_TO_K0(vdma_rphys); 

        base->vc1.addrhi = (VSRAM_START) >> 8;
        base->vc1.addrlo = (VSRAM_START) & 0xff;

	for (i=NUM_SHORT_WORDS; i>0; i-=2) {
		*rbuf = base->vc1.sram;
		if (*rbuf != (i-1)) {
	        	error_count++;
	        	msg_printf(ERR,
	           	"Expected: 0x%08x, Actual: 0x%08x\n",
		           	(i-1), *rbuf);
		}
		rbuf++;
		*rbuf = base->vc1.sram;
		if (*rbuf != i) {
	        	error_count++;
	        	msg_printf(ERR,
	           	"Expected: 0x%08x, Actual: 0x%08x\n",
		           	i, *rbuf);
		}
		rbuf++;
	}
		
	if (error_count)
		sum_error("VDMA decrementing addressing test");
	else
            okydoky ();

	return (error_count);
}
