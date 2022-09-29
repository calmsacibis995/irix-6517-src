/************************************************************************
 *
 ************************************************************************/

#include "sys/sbd.h"
#include "sys/hpc3.h"
#include "sys/hal2.h"

main()
{
    hal2_unreset();
    hal2_configure_pbus_pio();
    hal2_synth_ctrl();
}

void
hal2_synth_ctrl()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iar  = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr  = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    unsigned short value;
    unsigned short readback[2];
    unsigned int   errors[8], total, count;
    int            i, j;
    
    printf("Entering HAL2_SYNTH_CTRL test\n");
    
    /* initialize the counts */
    count = 0;
    total = 0;
    for (i = 0; i < 8; i++) {
	errors[i] = 0;
    }
    
    for (j = 0; j < 100; j++) {
        for (i = 0; i < 8; i++) {
            count++;
	    value = i;
	    *idr0 = value;
	    *iar  = HAL2_SYNTH_CTRL_W;
	    /* spin */
	    while (*isr & HAL2_ISR_TSTATUS);
	    
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_SYNTH_CTRL_R;
	    while (*isr & HAL2_ISR_TSTATUS);
	    readback[0] = *idr0;

	    if (readback[0] != value) {
                /*printf("value = %x readback = %x\n", value, readback[0]);*/
		errors[value]++;
		total++;
	    }
/*
	    while (*isr & HAL2_ISR_TSTATUS);
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_AESRX_CTRL_R;
	    readback[1] = *idr0;
	    if (readback[1] != value) {
                printf("value = %x readback = %x\n", value, readback[1]);
		errors[value]++;
		total++;
	    }
*/
	}
    }
    
    printf("iterations: %d\n", count);
    if (total) {
        printf("errors: %d\n", total);
	for (i = 0; i < 8; i++) {
	    printf("0x%x : %d\n", i, errors[i]);
	}
    }
    printf("Exiting HAL2_SYNTH_CTRL test\n");
}
