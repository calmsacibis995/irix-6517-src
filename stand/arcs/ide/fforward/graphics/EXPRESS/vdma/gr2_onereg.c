#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/gr2hw.h"
#include "libsc.h"
#include "uif.h"
#include "dma.h"
#include "diagcmds.h"

/* external variables */
extern struct gr2_hw *base; 

/* external utility routines */
extern int dma_error_handler(void);

int
gr2_onereg_dma(argc, argv)
int argc;
char **argv;
{
	register int i,j;
	int error_count = 0;
	int ntimes = 1;
	unsigned int *wbuf; 
	unsigned int *rbuf; 


   	msg_printf (VRB, "One register virtual DMA test\n");

	if (argc /* 1 */) {
        	msg_printf (VRB, "requires Snoop - skipping test\n");
        	return 0;
	}

	if (argc > 2) {
		msg_printf(ERR, "usage: %s [ntimes]\n", argv[0]);
		return -1;
	}
	
	if (argc == 2)
		atob(argv[1], &ntimes);

	basic_DMA_setup(1);	/* SNOOP ! */


        /* set DMA_SYNC source to 0 to synchronize the transfer with vertical
           retrace. */
        base->hq.dmasync = 0;

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys); 
	*wbuf++ = 0xabcdefab;
	*wbuf++ = 0xcdefabcd;
	*wbuf = 0xefabcdef;

	writemcreg(DMA_GIO_ADRS, KDM_TO_MCPHYS(&(base->fifo[DIAG_SIMV3f])));

	for (i=0; i<ntimes; i++) {

		/* One register to start the DMA */ 
		writemcreg(DMA_MEMADRDS, vdma_wphys); 

		/* Wait for DMA to complete */
		vdma_wait();

		if (dma_error_handler()) {
			msg_printf(ERR,"DMA read (from memory to command FIFO) did not complete\n");
			return -1;
		}

		/* clear the readback buffer */
		rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
		for (j=0; j<3; i++)
			*rbuf++ = 0xdeadbeef;

		/* read back from GE7 RAM0 */ 
		rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
		for (j=0; j<3; j++)
			*rbuf++ = base->ge[0].ram0[j];

		wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);
		rbuf = (unsigned int *) PHYS_TO_K0(vdma_rphys);
        	for (j=0; j<3; j++) {
            		if (*rbuf != *wbuf) {
		        	error_count++;
		        	msg_printf(ERR,
		           	"Expected: 0x%08x, Actual: 0x%08x\n", *wbuf, *rbuf);
	    		}
            		wbuf++; rbuf++;
		}
	}
    
	if (error_count)
            sum_error ("One register DMA test");
    	else
            okydoky ();

	return (error_count);
}
