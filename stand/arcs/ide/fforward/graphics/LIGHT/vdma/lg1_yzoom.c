#ident	"$Revision: 1.10 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/lg1hw.h"
#include "uif.h"
#include "light.h"
#include "dma.h"
#include <libsc.h>
#include <libsk.h>

static int cache_mode = 2;

/*
 * cache_mode =        0 : we use K0, we snoop
 *                      1 : we use K0, flush cache by hand, don't snoop
 *                      2 : we use K1, don't snoop
 */

int
lg1_yzoom_dma(argc, argv)
int argc;
char **argv;
{
	int npixels = 12;
	int yzoom = 2;
	int vdma_modes = VDMA_INCA;
	int nwords;
	int i;
	unsigned char j;
	unsigned int *wbuf; 

   	msg_printf (VRB, "Virtual DMA y zoom test\n");

	if (argc > 4) {
	    msg_printf(ERR,"usage: %s [npixels] [yzoom] [cache_mode]\n", 
				argv[0]);
	    return(-1);
	}

	switch (argc) {
		case 2:
			atob(argv[1], &npixels);
			break;
		case 3:
			atob(argv[1], &npixels);
			atob(argv[2], &yzoom);
			break;
		case 4:
			atob(argv[1], &npixels);
			atob(argv[2], &yzoom);
			atob(argv[3], &cache_mode);
			break;
		default:
			break;
	}

        if (npixels % 4) {
                msg_printf(VRB, "Number of pixels must be even multiple of 4 bytes (quadmode only)\n");
                return -1;
        }

	if (npixels > 1024)
		npixels = 1024;
	nwords = npixels / 4;

	if (cache_mode < 0 || cache_mode > 2)
		cache_mode = 2;
	
	basic_DMA_setup(cache_mode == 0);

	if (cache_mode == 0) 
		vdma_modes |= VDMA_SNOOP;
	else
		vdma_modes |= VDMA_LBURST;
	

	msg_printf (DBG, "yzoom: Init color map 1-4\n");

	/* Initialize color map */
	rex_setclt();

	msg_printf (DBG, "yzoom: rex_clear()\n");
	vc1_on();

	rex_clear();

	/*
	 * Initialize data buffer  
	 */
	if (cache_mode == 2)
		wbuf = (unsigned int *) PHYS_TO_K1(vdma_wphys); 
	else
		wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 

	msg_printf (DBG, "yzoom: Init buf phys %x K1 %x\n", vdma_wphys, wbuf);

	for (i=0; i<nwords; i++) {
	    *wbuf++ = (1 << 24) | (2 << 16) | (3 << 8) | 4;
	}
	wbuf -= nwords;

	/* Flush data to RAM */
	if (cache_mode == 1)
		clear_cache (wbuf, npixels);

	msg_printf (DBG, "yzoom: start REX\n");

    	REX->set.xstarti = 100; 
	REX->set.xendi = 100 + npixels - 1;
    	REX->set.ystarti = 100;     
	REX->set.yendi = 100 + (yzoom - 1);
    	REX->go.command = ( 0);     /* NO-OP */

    	REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | 
				QUADMODE | REX_DRAW);

	/* DMA read - from memory to gfx */
	dma_go ((caddr_t)&REX->go.rwaux1, vdma_wphys, 1, npixels, 0, yzoom,
		vdma_modes);

	if (dma_error_handler()) {
		sum_error ("DMA y zoom test");
		return -1;
	}
	else
            okydoky ();

	return 0;

}
