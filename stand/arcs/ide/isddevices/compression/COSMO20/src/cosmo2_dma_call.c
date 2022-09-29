
#include "cosmo2_defs.h"
#include "cosmo2_mcu_def.h"

extern __uint32_t dma_setup(UBYTE,UBYTE,__uint64_t *, __uint32_t, __uint32_t);

void
cosmo2_dma_flush(UBYTE chnl) 
{
   msg_printf(SUM, " dma flush in %x \n", chnl);
   switch (chnl) {
    case CHANNEL_0:
        mcu_WRITE(DMA_FLUSH0, 0xf);
        break;
    case CHANNEL_1:
        mcu_WRITE(DMA_FLUSH1, 0xf);
        break;
    case CHANNEL_2:
        mcu_WRITE(DMA_FLUSH2, 0xf);
        break;
    case CHANNEL_3:
        mcu_WRITE(DMA_FLUSH3, 0xf);
        break;
    default:
        break;
    }
}

