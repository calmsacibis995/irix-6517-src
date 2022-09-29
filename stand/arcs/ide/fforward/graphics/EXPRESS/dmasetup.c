/*
 *  Graphics DMA Diagnostics
 */
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "ide_msg.h"
#include "sys/gr2hw.h"
#include "diagcmds.h"
#include "libsk.h"
#include "dma.h"
#include "uif.h"

extern void basic_DMA_setup(int);
char dirstr[][20] = {"GFX to CPU:", "CPU to GFX:"};

/* count is in the (lines << 16) | width format */
void
mk_dmada(char *hostbufp, int count, int flags, __psunsigned_t gfxaddr)
{

	if (count < (1 << 16)) {	/* not in lines:width format */
		msg_printf (VRB, "Bad count in mk_dmada\n");
		return;
	}

    basic_DMA_setup(0);	 /* Snooping is disabled here. */

    writemcreg(DMA_MEMADR, KDM_TO_MCPHYS(hostbufp));

    /* always enable sync bit for EXPRESS */
    writemcreg(DMA_MODE, VDMA_INCA | VDMA_LBURST | VDMA_SYNC | flags);

    writemcreg(DMA_SIZE, count); 

    writemcreg(DMA_STRIDE, 1<<16); /* zoom=1, stride = 0 */

    writemcreg(DMA_MODE,
	*(volatile uint *)(PHYS_TO_K1(DMA_MODE)) | flags);

    writemcreg(DMA_GIO_ADR, KDM_TO_MCPHYS(gfxaddr));

}

int
do_dma(void)
{
    /* Start the DMA */
    writemcreg(DMA_STDMA, 1);

    /* Poll until DMA is stoped */
        (void)vdma_wait();

    if (dma_error_handler()) {
	msg_printf(ERR, "DMA still not completed.\n");
	return -1;
    }

    return 0;
}

#define GFX_TO_HOST     0
#define HOST_TO_GFX     1

/* dir is VDMA_HTOG or VDMA_GTOH */
long
pix_dma(struct gr2_hw *hw, int dir, int x1, int y1, int x2, int y2, int *buf)
{
    int	count, i;
    int	direction;

    /* send command to GE7 */
    direction = (dir == (VDMA_HTOG)) ? HOST_TO_GFX : GFX_TO_HOST;

    count = (x2 - x1 + 1) * (int)sizeof (int) | ((y2 - y1 + 1) << 16);
    mk_dmada((char *)buf, count, dir, (__psunsigned_t)&hw->hq.gedma);

    /* Send down the dma CPU/Bitplanes test token */
    hw->hq.fin1 = 0;
    hw->fifo[DIAG_REDMA] = direction;
    hw->fifo[DIAG_DATA] = x1;
    hw->fifo[DIAG_DATA] = y1;
    hw->fifo[DIAG_DATA] = x2;
    hw->fifo[DIAG_DATA] = y2;

    if (do_dma() < 0) return(-1);

    i = 20;
    while (i) {
       if ((hw->hq.version & 0x4)== 0) {
          DELAY(1000);
       } else break;

       i--;
    }

    if ( i <= 0 ) {
       msg_printf(DBG, "Waiting for finish flag 1 in %s ...\n",dirstr[direction]);
       msg_printf(ERR, "GE7 not responding.\n");
       return(-1);
    }


    hw->hq.fin1 = 0;

    return(0);
}
