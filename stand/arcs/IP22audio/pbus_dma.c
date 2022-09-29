/************************************************************************
 *
 * hal2_dma.c -
 *
 *     subroutines for the configuration and control of PBUS DMA.
 *
 ************************************************************************/

#include "sys/sbd.h"
#include "sys/hpc3.h"
#include "sys/hal2.h"
#include "pbus.h"

static int 
pbus_dma_params[11] = {
    PBUS_DMA_RD_D3, 
    PBUS_DMA_RD_D4, 
    PBUS_DMA_RD_D5, 
    PBUS_DMA_WR_D3, 
    PBUS_DMA_WR_D4, 
    PBUS_DMA_WR_D5, 
    PBUS_DMA_DS_16, 
    PBUS_DMA_EVEN_HIGH, 
    PBUS_DMA_REAL_TIME, 
    PBUS_DMA_BURST_COUNT, 
    PBUS_DMA_DRQ_LIVE
};

/************************************************************************
 * 
 * configure the DMA channel
 * 
 ************************************************************************/
int
hal2_configure_dma(int channel)
{
    volatile pbus_cfgdma_t    dma_config;
    volatile pbus_cfgdma_t   *config_ptr = 
        (pbus_cfgdma_t *) PHYS_TO_K1(HPC3_PBUS_CFGDMA(channel));
    volatile pbus_cfgdma_t    readback;
    int                       return_value = 0;

    dma_config.rd_d3       = pbus_dma_params[0];
    dma_config.rd_d4       = pbus_dma_params[1];
    dma_config.rd_d5       = pbus_dma_params[2];
    dma_config.wr_d3       = pbus_dma_params[3];
    dma_config.wr_d4       = pbus_dma_params[4];
    dma_config.wr_d5       = pbus_dma_params[5];
    dma_config.ds_16       = pbus_dma_params[6];
    dma_config.even_high   = pbus_dma_params[7];
    dma_config.real_time   = pbus_dma_params[8];
    dma_config.burst_count = pbus_dma_params[9];
    dma_config.drq_live    = pbus_dma_params[10];
    
    *config_ptr          = dma_config;
    readback             = *config_ptr;
    
    /*
     * should really compare the input config and the
     * config read back from the memory-mapped location.
     */
	
    return(return_value);
}


/************************************************************************
 * 
 * configure all pbus DMA channels that the audio system will utilize:
 * 
 ************************************************************************/
void
hal2_configure_pbus_dma(void)
{
    int result;
    int i;

    for (i = 0; i < 8; i++)
    {
        result = hal2_configure_dma(i);
        if (result == -1)
        {
            printf("pbus configuration failed for channel %d.\n",
                    i);
        }
    }
}

/************************************************************************
 * 
 * control PBUS DMA channel i:
 * 
 ************************************************************************/

void
hal2_control_pbus_dma(int channel)
{
   
}
