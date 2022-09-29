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
extern void basic_dma_setup();
extern int dma_error_handler();
extern void rex_clear();
extern void flush_cache();

static int cache_mode = 2;

/*
cache_mode =
                0 : we use K0, we snoop
                1 : we use K0, flush cache by hand, don't snoop
                2 : we use K1, don't snoop
*/


int
lg1_masksubst_dma(argc, argv)
int argc;
char **argv;
{
	int error_count = 0;
	int i;
	unsigned char j;
	int vdma_modes = VDMA_INCA;
	unsigned int *wbuf; 
	unsigned int *rbuf; 

   	msg_printf (VRB, "VDMA GIO mask/substitute test\n");

	if (argc > 3) {
		msg_printf(ERR,"usage: %s [cache_mode]\n", argv[0]);
		return -1;
	}

	if (argc == 2)
		atob(argv[1], &cache_mode);

	if (cache_mode < 0 || cache_mode > 2) 
		cache_mode = 2;

	if (cache_mode == 0)
		vdma_modes |= VDMA_SNOOP;
	else
		vdma_modes |= VDMA_LBURST;
	

	basic_DMA_setup(cache_mode == 0);

	/* set up DMA_GIO_MASK and DMA_GIO_SUB */
	writemcreg(DMA_GIO_MASK, 0xf00);
	msg_printf (DBG,"Wrote 0xf00 to DMA_GIO_MASK\n");
	writemcreg(DMA_GIO_SUB, 0);
	msg_printf (DBG,"Wrote 0 to DMA_GIO_SUB\n");

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

	/* 12 bytes */
	for (i=0; i<3; i++) {
	    j = (i+1) & 0xff;
	    *wbuf++ = (j << 24) | (j << 16) | (j << 8) | j;
	}

	if (cache_mode == 1) 
                flush_cache();

        REX->set.xstarti = 100;
        REX->set.xendi = 100 + 12 - 1;
        REX->set.ystarti = 100;
        REX->set.yendi = 100;
        REX->go.command = ( 0);     /* NO-OP */

        REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | 
				QUADMODE | REX_DRAW);

	dma_go (&REX->go.rwaux1, vdma_wphys, 1, 12, 0, 1, vdma_modes);

	/* DMA write - from gfx to memory */

	if (cache_mode == 2)
		rbuf = (unsigned int *) PHYS_TO_K1(vdma_rphys);
	else
		rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);

	for (i=0; i<3; i++)
		*rbuf++ = 0xdeadbeef;

	/* Invalidate data cache */
	if (cache_mode == 1)
		flush_cache();

       	REX->set.xstarti = 100;
       	REX->set.xendi = 100 + 12 - 1;
       	REX->set.ystarti = 100;
       	REX->set.yendi = 100;

	REX->go.command = ( REX_LDPIXEL|QUADMODE);
	REX->set.command = ( REX_LDPIXEL|QUADMODE|BLOCK|XYCONTINUE);

	/* Change the DMA_GIO_MASK value */
	writemcreg(DMA_GIO_MASK, 0);

	dma_go (&REX->go.rwaux1, vdma_rphys, 1, 12, 0, 1, vdma_modes | DMA_WRITE);
	
	if (dma_error_handler()) {
		msg_printf(ERR,"DMA write (from gfx to memory) did not complete\n");
		return -1;
	}
			
	if (cache_mode == 2) {
		wbuf = (unsigned int *) PHYS_TO_K1(vdma_wphys);
		rbuf = (unsigned int *) PHYS_TO_K1(vdma_rphys);
	}
	else {
		wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
		rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
	}

        for (i=0; i<3; i++) {
            if (*rbuf == *wbuf) {
		        error_count++;
		        msg_printf(ERR,
		           "Address: 0x%08x, data should not be 0x%08x\n",
		           (unsigned int) wbuf, *wbuf);
	    }
            wbuf++; rbuf++;
	}
    
	if (error_count)
            sum_error ("VDMA mask/substitute test");
    	else
            okydoky ();

	return (error_count);
}
