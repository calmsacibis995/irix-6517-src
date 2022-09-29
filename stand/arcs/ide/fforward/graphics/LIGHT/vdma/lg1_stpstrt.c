
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
extern void basic_DMA_setup();
extern int dma_error_handler();
extern void rex_clear();
extern void rex_setclt();

/* Since we are testing stop and restart DMA, I would like to turn the
 * snooping off here */
int
lg1_stoprestart_dma(argc, argv)
int argc;
char **argv;
{
	int x, y;
	int linewidth = 12;
	int linecount = 2;
	int i,j;
	unsigned char *wbuf; 


   	msg_printf (VRB, "Virtual DMA stop/restart test\n");

	if (argc > 5) {
	    msg_printf(ERR,"usage: %s [x] [y] [linewidth(in pixels)] [linecount]\n", argv[0]);
	    return(-1);
	}

	x = y = 100;
	switch (argc) {
		case 2:
			atob(argv[1], &x);
			break;
		case 3:
			atob(argv[1], &x);
			atob(argv[2], &y);
			break;
		case 4:
			atob(argv[1], &x);
			atob(argv[2], &y);
			atob(argv[3], &linewidth);
			break;
		case 5:
			atob(argv[1], &x);
			atob(argv[2], &y);
			atob(argv[3], &linewidth);
			atob(argv[4], &linecount);
			break;
		default:
			break;
	}

        if (linewidth % 4) {
                msg_printf(VRB, "linewidth: number of pixels must be even multiple of 4 bytes (quadmode only, none-zero SB)\n");
                return -1;
        }

	if ((linewidth * linecount) < 0 || (linewidth * linecount) > 4096) {
		msg_printf (ERR,"require 0 < linewidth * linecount <= 4096\n");
		return -1;
	}

	basic_DMA_setup(0);

	/* Initialize color map for index 0 to 7 */
	rex_setclt(REX,0,255, 0, 0);

	rex_clear();
	vc1_on();

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys); 

	for (i=0; i<linecount; i++) 
		for (j=0; j<linewidth; j++) 
	    		*wbuf++ = i%4;

        flush_cache();

    	REX->set.xstarti = x; 
	REX->set.xendi = x + linewidth - 1;
    	REX->set.ystarti = y;     
	REX->set.yendi = y + (linecount - 1);
    	REX->go.command = ( 0);     /* NO-OP */

    	REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | QUADMODE | REX_DRAW);

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADR, vdma_wphys); 
	writemcreg(DMA_SIZE, (linecount << 16) | linewidth);

	/* Setup DMA mode: long burst, no snooping, incrementing addressing,
           fill mode disabled, sync disabled, DMA read */
       	writemcreg(DMA_MODE, 0x50);

	/* Start DMA */
	writemcreg(DMA_GIO_ADRS,  (uint)&REX->go.rwaux1);

/* ??? Stop the DMA after 1 micro sec. Might have to fine tune this number */
/* Should have enough time to stop 12-byte pixel DMA */
#define TIME_TO_STOP_DMA	1
	i = 0;
	while (*(volatile uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40) {
		if (i == TIME_TO_STOP_DMA) {
			writemcreg(DMA_STDMA,  0);
			break;
		}
		i++;
		DELAY(1);
	}

	vdma_wait();

	/* If DMA was stopped before completing the pixel data transfer */

	if ( ! (*(volatile uint *)PHYS_TO_K1(DMA_RUN) & 8) ) {
		/* Restart the DMA */
		writemcreg(DMA_STDMA,  1);

		vdma_wait();
	}
	else
		msg_printf (VRB,"Dma completed before we could stop it\n");


	if (dma_error_handler()) {
		sum_error ("Stop/Restart DMA test");
		return -1;
	}
	else
            okydoky ();

	return 0;
}
