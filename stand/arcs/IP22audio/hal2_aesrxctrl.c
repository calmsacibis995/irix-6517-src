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
    hal2_aesrx_ctrl();
}

void
hal2_aesrx_ctrl()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iar  = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr  = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    unsigned short value;
    unsigned short readback[2];
    unsigned int   errors[256], total;
    int            i, j;
    
    printf("Entering AESRX_CTRL test\n");
    
    /* initialize the counts */
    total = 0;
    for (i = 0; i < 256; i++) {
	errors[i] = 0;
    }
    
    /*
     * none of the receiver modes under control are mutually 
     * exclusive, so let's just spin thru them.
     * 
     * Q: is there any problem with reading/writing the register
     * with sequential data?
     * 
     * note: there are 2 bits <4:3> that are not used in this 
     * word. thus, i choose to set them to zero in every word
     * that is sent to the hal2. this results in the 'anding'
     * with 0xffe8.
     */
     
        for (i = 0; i < 256; i++) {
	    value = 0xffe8 & i;
	    *idr0 = value;
	    *iar  = HAL2_AESRX_CTRL_W;
	    
	    /* spin */
            /*
             * need to spin on all writes to the iar (especially
             * with emulation of the hal2 chip).
             */
	    while (*isr & HAL2_ISR_TSTATUS);
	    
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_AESRX_CTRL_R;
	    while (*isr & HAL2_ISR_TSTATUS);     /* spin */
	    readback[0] = *idr0;
	    if (readback[0] != value) {
		errors[value]++;
		total++;
	    }
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_AESRX_CTRL_R;
	    while (*isr & HAL2_ISR_TSTATUS);     /* spin */
	    readback[1] = *idr0;
	    if (readback[1] != value) {
		errors[value]++;
		total++;
	    }
	}
    
    if (total) {
        printf("errors:\n");
	for (i = 0; i < 256; i++) {
	    printf("0x%x : %d\n", i, errors[i]);
	}
    }
    printf("Exiting AESRX_CTRL test\n");
}
