#ident	"$Revision: 1.11 $"
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

int
lg1_cacheflush_dma(int argc, char **argv)
{
	int error_count = 0;
	int npixels, nwords;
	int rflag;
	int i;
	int vdma_modes = 0x50;
	unsigned char j;
	unsigned int *wbuf; 
	unsigned int *rbuf; 

	int dualhead = rex_dualhead();

   	msg_printf (VRB, "Virtual DMA cache flushing test\n");
   	msg_printf (DBG, "data cache size %x line size %x\n", 
			_dcache_size, _dcache_linesize);

	if (argc > 3) {
		msg_printf(ERR,"usage: %s [npixels] [rtest]\n", argv[0]); 
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

	basic_DMA_setup(0);	/* no snoop */

        /* Initialize Rex */
        rex_clear();
	vc1_on();

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 
	for (i=0; i<nwords; i++) {
	    j = (i+1) & 0xff;
	    *wbuf++ = (j << 24) | (j << 16) | (j << 8) | j;
	}

	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 

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

	dma_go ((caddr_t)&REX->go.rwaux1, vdma_wphys, 1, npixels, 0, 1,
		vdma_modes);

	if (dma_error_handler()) return -1;

	/* DMA write */
	if (rflag) {
		rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
                REX->set.xstarti = 100;
                REX->set.xendi = 100 + npixels - 1;
                REX->set.ystarti = 100;
                REX->set.yendi = 100;

                REX->go.command = ( REX_LDPIXEL|QUADMODE);
                REX->set.command = ( REX_LDPIXEL|QUADMODE|BLOCK|XYCONTINUE);

		if (!dualhead) {
			dma_go ((caddr_t)&REX->go.rwaux1, vdma_rphys, 1,
				npixels, 0, 1, vdma_modes | VDMA_GTOH);
			if (dma_error_handler()) 
				return -1;
		}
		else  {
			for (i = 0; i < npixels/4; i++)
				rbuf[i] = REX->go.rwaux1;
		}

		wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
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
            sum_error ("DMA cache flushing test");
    	else
            okydoky ();

    	return (error_count);
}
