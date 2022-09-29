

#ident	"$Revision: 1.2 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/lg1hw.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern Rexchip *REX;

/* external utility routines */
extern void basic_dma_setup();
extern int dma_error_handler();
extern void rex_clear();
extern void rex_setclt();

static int cache_mode = 2;

static int xpage = 1;


/*
 * cache_mode =        0 : we use K0, we snoop
 *                      1 : we use K0, flush cache by hand, don't snoop
 *                      2 : we use K1, don't snoop
 */


#define DTLB_VPNHIMASK          0xffe00000      /* upper 11 bits */
#define DTLB_VALID              0x2             /* bit 1 */

#define TLB_REG_STEP    (DMA_TLB_HI_1 - DMA_TLB_HI_0)
#define uTLB_HI(i) (*(uint *)(PHYS_TO_K1(DMA_TLB_HI_0 + (i) * TLB_REG_STEP)))
#define uTLB_LO(i) (*(uint *)(PHYS_TO_K1(DMA_TLB_LO_0 + (i) * TLB_REG_STEP)))


int
lg1_virtual_dma(argc, argv)
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
	    msg_printf(ERR,"usage: %s [npixels] [yzoom] [xpage]\n", 
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
			atob(argv[3], &xpage);
			break;
		default:
			break;
	}

        if (npixels % 4) {
                msg_printf(VRB, "Number of pixels must be even multiple of 4 bytes (quadmode only)\n");
                return -1;
        }

	nwords = npixels / 4;

	if (cache_mode < 0 || cache_mode > 2)
		cache_mode = 2;
	
	basic_DMA_setup (cache_mode == 0);

	/* Now set for virtual address translation */

        writemcreg(DMA_CTL, *(uint *)(PHYS_TO_K1(DMA_CTL)) | (1 << 8));


	if (cache_mode == 0) 
		vdma_modes |= VDMA_SNOOP;
	else
		vdma_modes |= VDMA_LBURST;
	

	msg_printf (DBG, "virtual: Init color map 1-4\n");

	/* Initialize color map */
	rex_setclt();

	msg_printf (DBG, "virtual: rex_clear()\n");
	vc1_on();

	rex_clear();

	/*
	 * Initialize data buffer  
	 */
	if (cache_mode == 2)
		wbuf = (unsigned int *) 
			PHYS_TO_K1(vdma_wphys + (xpage == 0 ? 0 :
					(4096 - (nwords/2)*4))); 
	else
		wbuf = (unsigned int *) 
			PHYS_TO_K0(vdma_wphys + (xpage == 0 ? 0 :
					(4096 - (nwords/2)*4))); 

	msg_printf (DBG, "virtual: Init buf phys %x K1 %x\n", vdma_wphys, wbuf);

	for (i=0; i<nwords; i++) {
	    *wbuf++ = (1 << 24) | (2 << 16) | (3 << 8) | 4;
	}
	wbuf -= nwords;

	if (cache_mode == 1)
		flush_cache();

    	REX->set.xstarti = 100; 
	REX->set.xendi = 100 + npixels - 1;
    	REX->set.ystarti = 100;     
	REX->set.yendi = 100 + (yzoom - 1);
    	REX->go.command = ( 0);     /* NO-OP */

    	REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | 
				QUADMODE | REX_DRAW);

	
	/*
	 * Make a page table entry in vdma_rphys
	 * that points vdma_phys to vdma_phys.
	 */

	clean_dmatlb();
	set_dmatlb (0, vdma_wphys, vdma_rphys, 1);
	set_dmapte (PHYS_TO_K1(vdma_rphys), vdma_wphys);
	set_dmapte (PHYS_TO_K1(vdma_rphys), vdma_wphys + 4096);

	/* DMA read - from memory to gfx */

	dma_go (&REX->go.rwaux1, KDM_TO_PHYS(wbuf), 1, npixels, 0, 
			yzoom, vdma_modes);

	if (dma_error_handler()) {
		sum_error ("DMA y zoom test");
		return -1;
	}
	else
            okydoky ();

	return 0;

}
