/************************************************************************
 *
 * pbus_pio.c -
 *
 *    configuration routines for pio for pbus devices (hal2, volume
 *    control, synth, etc.)
 *
 ************************************************************************/
#include "sys/sbd.h"
#include <sys/hpc3.h>
#include <sys/hal2.h>
#include "pbus.h"

static int 
pbus_pio_params[][8] = {
    HAL2_PIO_RD_P2,
    HAL2_PIO_RD_P3,
    HAL2_PIO_RD_P4,
    HAL2_PIO_WR_P2,
    HAL2_PIO_WR_P3,
    HAL2_PIO_WR_P4,
    HAL2_PIO_DS_16,
    HAL2_PIO_EVEN_HIGH,
    AES_PIO_RD_P2,
    AES_PIO_RD_P3,
    AES_PIO_RD_P4,
    AES_PIO_WR_P2,
    AES_PIO_WR_P3,
    AES_PIO_WR_P4,
    AES_PIO_DS_16,
    AES_PIO_EVEN_HIGH,
    VOLUME_PIO_RD_P2,
    VOLUME_PIO_RD_P3,
    VOLUME_PIO_RD_P4,
    VOLUME_PIO_WR_P2,
    VOLUME_PIO_WR_P3,
    VOLUME_PIO_WR_P4,
    VOLUME_PIO_DS_16,
    VOLUME_PIO_EVEN_HIGH,
    SYNTH_PIO_RD_P2,
    SYNTH_PIO_RD_P3,
    SYNTH_PIO_RD_P4,
    SYNTH_PIO_WR_P2,
    SYNTH_PIO_WR_P3,
    SYNTH_PIO_WR_P4,
    SYNTH_PIO_DS_16,
    SYNTH_PIO_EVEN_HIGH
};

/************************************************************************
 * configure the PIO channel
 ************************************************************************/
int
hal2_configure_pio(int channel)
{
    volatile pbus_cfgpio_t    pio_config;
    volatile pbus_cfgpio_t   *config_ptr = 
        (pbus_cfgpio_t *) PHYS_TO_K1(HPC3_PBUS_CFG(channel));
    volatile pbus_cfgpio_t    readback;
    int              return_value = 0;

    pio_config.rd_p2     = pbus_pio_params[channel][0];
    pio_config.rd_p3     = pbus_pio_params[channel][1];
    pio_config.rd_p4     = pbus_pio_params[channel][2];
    pio_config.wr_p2     = pbus_pio_params[channel][3];
    pio_config.wr_p3     = pbus_pio_params[channel][4];
    pio_config.wr_p4     = pbus_pio_params[channel][5];
    pio_config.ds_16     = pbus_pio_params[channel][6];
    pio_config.even_high = pbus_pio_params[channel][7];

    *config_ptr          = pio_config;
    readback             = *config_ptr;
    
    /*
     * should really compare the input config and the
     * config read back from the memory-mapped location.
     */
    if (readback.rd_p2 != pio_config.rd_p2)
        return_value = -1;
    else if (readback.rd_p3 != pio_config.rd_p3)
        return_value = -1;
    else if (readback.rd_p4 != pio_config.rd_p4)
        return_value = -1;
    else if (readback.wr_p2 != pio_config.wr_p2)
        return_value = -1;
    else if (readback.wr_p3 != pio_config.wr_p3)
        return_value = -1;
    else if (readback.wr_p4 != pio_config.wr_p4)
        return_value = -1;
    else if (readback.ds_16 != pio_config.ds_16)
        return_value = -1;
    else if (readback.even_high != pio_config.even_high)
        return_value = -1;
	
    return(return_value);
}


/************************************************************************
 * 
 * configure all pbus PIO channels that the audio system will utilize:
 *    channel 0 = hal2
 *    channel 1 = aestx/aesrx
 *    channel 2 = volume control
 *    channel 3 = synth
 * 
 ************************************************************************/
void
hal2_configure_pbus_pio(void)
{
    int result;
    int i;

    for (i = 0; i < 4; i++)
    {
        result = hal2_configure_pio(i);
        if (result == -1)
        {
            printf("pbus configuration failed for channel %d.\n",
                    i);
        }
    }
}
