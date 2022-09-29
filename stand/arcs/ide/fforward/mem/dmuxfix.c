#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/mc.h"
#include "sys/vdma.h"
#include "libsk.h"

#ident "$Revision: 1.3 $"

/*
 * Minimalist workaround for the DMUX parity bug.  When incorrect parity
 * is forced through DMUX, subsequent GIO DMA operations fail.  Nobody
 * seems quite sure what the problem is, but operations with the virtual
 * DMA engine in MC seem to clear everything out again.  Here, we do a
 * minimal setup of the DMA engine, and clear a little chunk of memory.
 *
 * This is handy after the parity test, so other tests will work correctly.
 */

void DMUXfix ()
{
	int		i;
	char		buff [512];

	/* everything disabled and default; no virtual mapping */
	writemcreg (DMA_CTL, 0);

	/* clear virtual address mask and substitute bits */
        writemcreg (DMA_GIO_MASK, 0);
        writemcreg (DMA_GIO_SUB, 0);

        /* Clear status bits */
        writemcreg (DMA_CAUSE, 0);

	/* set up the transfer */

        writemcreg (DMA_MEMADR, K1_TO_PHYS (buff));
        writemcreg (DMA_MODE, VDMA_M_LBURST | VDMA_M_INCA |
                              VDMA_M_GTOH | VDMA_M_FILL);
        writemcreg (DMA_STRIDE, 0);
        writemcreg (DMA_SIZE, (1 << 16) | sizeof (buff));

        /*  Start the transfer */
        writemcreg (DMA_GIO_ADRS, 0x00000000);

	/* wait a while */

        for (i = 0; i < 1000; i++) {
                if (VDMAREG(DMA_RUN) & VDMA_R_RUNNING)
                        DELAY (1);
                else
                        break;
	}
}
