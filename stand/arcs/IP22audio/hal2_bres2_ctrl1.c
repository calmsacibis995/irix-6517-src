/************************************************************************
 *
 ************************************************************************/

#include "sys/sbd.h"
#include "sys/hpc3.h"
#include "sys/hal2.h"

main()
{
    hal2_configure_pbus_pio();
    hal2_bres2_ctrl1();
}

void
hal2_bres2_ctrl1()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iar  = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr  = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    unsigned short value;
    unsigned short readback[2];
    unsigned int   errors[3], total;
    int            i, j;
    
    printf("Entering HAL2_BRES2_CTRL1 test\n");
    
    /* initialize the counts */
    total = 0;
    for (i = 0; i < 3; i++) {
	errors[i] = 0;
    }
    
    for (j = 0; j < 1000000; j++) {
        for (i = 0; i < 3; i++) {
	    value = i;
	    *idr0 = value;
	    *iar  = HAL2_BRES2_CTRL1_W;
	    
	    /* spin */
	    while (*isr & HAL2_ISR_TSTATUS);
	    
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_BRES2_CTRL1_R;
	    while (*isr & HAL2_ISR_TSTATUS);
	    readback[0] = *idr0;
	    if (readback[0] != value) {
		errors[value]++;
		total++;
	    }
	}
    }
    
    if (total) {
        printf("errors:\n");
	for (i = 0; i < 3; i++) {
	    printf("0x%x : %d\n", i, errors[i]);
	}
    }
    printf("Exiting HAL2_BRES2_CTRL1 test\n");
}
