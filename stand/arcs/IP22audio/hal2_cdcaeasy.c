#include "sys/sbd.h"
#include "sys/hal2.h"

void
hal2_codecactrl1()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iar = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    unsigned short val;
    unsigned short rb0;
    
    printf("begin test of HAL2 CODECA CTRL1 register\n");
	    /*
	     * loop through all values for bits 0-4; let 
	     * bits 8 and 9 be the same as bits 0-1.
	     */
	    val = 0;
	    *idr0 = val;
	    *iar = HAL2_CODECA_CTRL1_W;    /* write the values */
	    /*
	     * spin on the ISR to make sure the value we 
	     * just wrote is latched in before we read it
	     */
	    while (*isr & HAL2_ISR_TSTATUS);
	    *idr0= 0xffff;		/* write a bogus value */

	    *iar = HAL2_CODECA_CTRL1_R;
	    while (*isr & HAL2_ISR_TSTATUS);
	    rb0 = *idr0;
	    printf("[read 1]: wrote 0x%x, got 0x%x\n",val,rb0);
    printf("end test of HAL2 CODECA CTRL1 register\n");
}

main()
{
    hal2_configure_pbus_pio();
    hal2_codecactrl1();
}
