#ident	"$Revision: 1.3 $"
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
extern Rexchip *REX;
extern long _dcache_size;
extern long _dcache_linesize;

/* external utility routines */
extern void basic_dma_setup();
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
lg1_decreadr_dma(argc, argv)
int argc;
char **argv;
{
	int dualhead = rex_dualhead();
	int error_count = 0;
	int npixels, nwords;
	int rflag;
	int i;
	int vdma_modes = VDMA_LBURST;
	unsigned char j;
	unsigned int *wbuf; 
	unsigned int *rbuf; 

   	msg_printf (VRB, "VDMA GIO64 decrement address test\n");

	if (argc > 4) {
		msg_printf(ERR,"usage: %s [npixels] [rtest] [cache_mode]\n", 
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
        else if (argc == 4) {
                atob(argv[1], &npixels);
                atob(argv[2], &rflag);
                atob(argv[3], &cache_mode);
        }

        if (npixels % 4) {
                msg_printf(VRB, "Number of pixels must be even multiple of 4 bytes (quadmode only)\n");
                return -1;
        }
	nwords = npixels / 4;

	basic_DMA_setup(cache_mode == 0);

        /* Initialize Rex */
        rex_clear();
	vc1_on();

	/*
	 * Initialize data buffer  
	 */
	
	if (cache_mode == 2)
		wbuf = (unsigned int *) PHYS_TO_K1(vdma_wphys); 
	else
		wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 

	for (i=0; i<npixels; i++) 
		((char *)wbuf)[i] = i;


	if (cache_mode == 1)
		flush_cache();


	msg_printf(DBG, "setup REX");

        REX->set.xstarti = 100;
        REX->set.xendi = 100 + npixels - 1;
        REX->set.ystarti = 100;
        REX->set.yendi = 100;
        REX->go.command = REX_NOP;

        REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | 
				XYCONTINUE | QUADMODE | REX_DRAW);


	/* Setup DMA mode: long burst, no snooping, incrementing addressing,
	   fill mode disabled, sync disabled, DMA read */

	dma_go ((caddr_t)&REX->go.rwaux1, vdma_wphys + npixels - 1, 1, 
		npixels, 0, 1, vdma_modes);

	if (dma_error_handler()) return -1;



	/* DMA write */
	if (rflag) {
		if (cache_mode == 2) {
			rbuf = (unsigned int *) PHYS_TO_K1(vdma_rphys);
			wbuf = (unsigned int *) PHYS_TO_K1(vdma_wphys);
		}
		else {
			rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
			wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
		}
                REX->set.xstarti = 100;
                REX->set.xendi = 100 + npixels - 1;
                REX->set.ystarti = 100;
                REX->set.yendi = 100;

                REX->go.command = REX_LDPIXEL|QUADMODE;
                REX->set.command =REX_LDPIXEL|QUADMODE|BLOCK|XYCONTINUE;

                if (!dualhead) {
                        dma_go ((caddr_t)&REX->go.rwaux1, vdma_rphys+npixels-1,
				1, npixels, 0, 1, vdma_modes | VDMA_GTOH);
			if (dma_error_handler()) 
				return -1;
		}
                else  {
			for (i = nwords; i > 0; )
				rbuf[--i] = REX->go.rwaux1;
		}


        	for (i=0; i<nwords; i++) {
            		if (*rbuf != *wbuf) {
		        	error_count++;
		        	msg_printf(ERR,
		           	"Address: 0x%08x, Expected: 0x%08x, Actual: 0x%08x\n",
		           	(unsigned int) wbuf, *wbuf, *rbuf);
	    		}
            		wbuf++; rbuf++;
		}
	}
    
	if (error_count)
            sum_error ("DMA GIO64 decrementing test");
    	else
            okydoky ();

    	return (error_count);
}
