#ident	"$Revision: 1.9 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/lg1hw.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern Rexchip *REX;
extern long _dcache_size;
extern long _dcache_linesize;

/* external utility routines */
extern void basic_DMA_setup();
extern int dma_error_handler();
extern void rex_clear();
extern void flush_cache();

static int cache_mode = 2;

/*
 * cache_mode =        0 : we use K0, we snoop
 *                      1 : we use K0, flush cache by hand, don't snoop
 *                      2 : we use K1, don't snoop
 */

int
lg1_defaultregs_dma(argc, argv)
int argc;
char **argv;
{
        int dualhead = rex_dualhead();

	int error_count = 0;
	int npixels, nwords;
	int rflag;
	int i;
	unsigned char j;
	unsigned int *wbuf; 
	unsigned int *rbuf; 
	unsigned int *gaddr;
	unsigned long gfx_addr;

   	msg_printf (VRB, "Virtual DMA default registers test\n");

	if (argc > 3) {
	    msg_printf(ERR,"usage: %s [npixels] [rtest]\n", 
				argv[0]);
	    return(-1);
	}
	
	npixels = 12;
	rflag = 1;
	if (argc == 2)
		atob(argv[1], &npixels);
	else if (argc == 3) {
		atob(argv[1], &npixels);
		atob(argv[2], &rflag);
	}

	if (npixels % 4) {
		msg_printf(VRB, "Number of pixels must be even multiple of 4 bytes (quadmode only)\n");	
		return -1;
	}
	nwords = npixels / 4;

        basic_DMA_setup (cache_mode == 0);


        /* Initialize Rex */
        rex_clear();
	vc1_on();

        if (cache_mode == 2) {
                wbuf = (unsigned int *) PHYS_TO_K1(vdma_wphys);
                rbuf = (unsigned int *) PHYS_TO_K1(vdma_rphys);
        }
        else {
                wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
                rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
        }

	/*
	 * Initialize data buffer  
	 */

	for (i=0; i<nwords; i++) {
	    j = (i+1) & 0xff;
	    *wbuf++ = (j << 24) | (j << 16) | (j << 8) | j;
	}

        /* Flush the data to RAM */
        if (cache_mode == 1)
                flush_cache();


        REX->set.xstarti = 100;
        REX->set.xendi = 100 + npixels - 1;
        REX->set.ystarti = 100;
        REX->set.yendi = 100;
        REX->go.command = ( 0);     /* NO-OP */

        REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | QUADMODE | REX_DRAW);

	/* DMA read - from memory to gfx */

	msg_printf (DBG, "writing %x to DMA_MEMADRD\n", vdma_wphys);

	writemcreg(DMA_MEMADRD, vdma_wphys); 

	/*
	 * Snoop got turned on by writing DMA_MEMADRD above.
	 * If we're not to user snoop, turn it off and turn on
	 * long burst mode.
	 */

	if (cache_mode != 0) {
		writemcreg(DMA_MODE, i = (*(uint *)(PHYS_TO_K1(DMA_MODE)) &
			 			~VDMA_SNOOP) | VDMA_LBURST);
		msg_printf (DBG,"Wrote %x to DMA_MODE\N", i);
	}
	if (npixels != 12) {
		writemcreg(DMA_SIZE, i = (1 << 16) | (npixels & 0xffff));
		msg_printf (DBG,"Wrote %x to DMA_SIZE\N", i);
	}

	msg_printf (DBG, "writing %x to DMA_GIO_ADRS\n", &REX->go.rwaux1);
	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);


	vdma_wait();	 /* Poll until DMA is stops or times out */

	if (dma_error_handler()) {
		msg_printf(ERR,"DMA read (from memory to gfx) did not complete\n");
		return -1;
	}

	/* DMA write - from gfx to memory */
	if (rflag) {
        	REX->set.xstarti = 100;
        	REX->set.xendi = 100 + npixels - 1;
        	REX->set.ystarti = 100;
        	REX->set.yendi = 100;

		REX->go.command = ( REX_LDPIXEL|QUADMODE);
		REX->set.command = ( REX_LDPIXEL|QUADMODE|BLOCK|XYCONTINUE);

		if (dualhead) {
			for (i = 0; i < nwords; i++) 
				rbuf[i] = REX->go.rwaux1;
		}
		else {
			writemcreg(DMA_MEMADRD, vdma_rphys);
			writemcreg(DMA_MODE, 
			*(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | VDMA_GTOH);
			if (npixels != 12)
				writemcreg(DMA_SIZE, 
					(1 << 16) | (npixels & 0xffff));

			msg_printf (DBG, "writing %x to DMA_GIO_ADRS\n", 
						&REX->go.rwaux1);
			writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

			msg_printf (DBG, "waiting for Graphics to host ... \n");

			vdma_wait();	 /* Poll until DMA stops or times out */

			if (dma_error_handler()) {
				msg_printf(ERR,"DMA write (from gfx to memory) did not complete\n");
				return -1;
			}
		}

        	if (cache_mode == 2) 
                	wbuf = (unsigned int *) PHYS_TO_K1(vdma_wphys);
        	else 
                	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);

        	for (i=0; i<nwords; i++) {
            		if (*rbuf != *wbuf) {
		        	error_count++;
		        	msg_printf(ERR,
		           	"Address: 0x%08x, Expected: 0x%08x, Actual: 0x%08x\n",
		           	(unsigned int) rbuf, *wbuf, *rbuf);
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
