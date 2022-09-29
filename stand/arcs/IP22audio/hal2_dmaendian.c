/************************************************************************
 *
 *  hal2_dmaendian.c
 *
 *     test DMA_ENDIAN register in the hal2.
 *
 ************************************************************************/

#include "sys/sbd.h"
#include "sys/hpc3.h"
#include "sys/hal2.h"

main()
{
    hal2_unreset();
    hal2_configure_pbus_pio();
    hal2_dma_endian_test();
}

void
hal2_dma_endian_test()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iar  = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr  = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    unsigned short value;
    unsigned short readback[2];
    unsigned int   errors[32], total;
    int            i, j;
    
    printf("Entering HAL2_DMA_ENDIAN test\n");
    
    /* initialize the error counts */
    total = 0;
    for (i = 0; i < 32; i++) {
	errors[i] = 0;
    }
    
    for (j = 0; j < 1000; j++) {
        for (i = 0; i < 32; i++) {
	    value = i;
	    *idr0 = value;
	    *iar  = HAL2_DMA_ENDIAN_W;
	    /* spin */
	    while (*isr & HAL2_ISR_TSTATUS);
	    
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_DMA_ENDIAN_R;
	    /* spin */
	    while (*isr & HAL2_ISR_TSTATUS);
	    readback[0] = *idr0;
	    if (readback[0] != value) {
		errors[value]++;
		total++;
	    }
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_DMA_ENDIAN_R;
	    /* spin */
	    while (*isr & HAL2_ISR_TSTATUS);
	    readback[1] = *idr0;
	    if (readback[1] != value) {
		errors[value]++;
		total++;
	    }
	}
    }
    
    if (total) {
        printf("errors:\n");
	for (i = 0; i < 32; i++) {
	    printf("0x%x : %d\n", i, errors[i]);
	}
    }
    printf("Exiting HAL2_DMA_ENDIAN test\n");
}
