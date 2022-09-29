/*
 * hal2_unreset.c -
 *  code to unreset the hal2 chip
 */

#include <sys/sbd.h>
#include <sys/hal2.h>

void
hal2_unreset()
{
    volatile __uint32_t *isrp = (__uint32_t *) PHYS_TO_K1(HAL2_ISR);

    /* Set up the ISR */

    *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N;
}

