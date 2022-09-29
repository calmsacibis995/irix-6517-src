#include "sys/sbd.h"
#include "sys/hal2.h"

void
hal2_codecbctrl2()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *idr1 = (unsigned long *)PHYS_TO_K1(HAL2_IDR1);
    volatile unsigned long *iar = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    int i, nbad =0;
    int val0,  val1;
    unsigned short j;
    unsigned short rb0, rb1;
    int badval0=0, badval1=0;
    
    printf("begin test of HAL2 CODECB CTRL2 register\n");
    *isr = 0x8;		/* out of reset */
    /*
     * the HAL2 ADC CTRL2 reg has 12 significant bits in IDR1
     * and 11 in IDR0,  so we just run through all possible values
     * a few times. We avoid writing the same values into IDR0
     * and IDR1 at the same time by adding 255 to the IDR0 value.
     */
    for (i = 0; i < 1000; i++) {
	for (j = 0; j < 4096; j++) {
	    val0 = (j + 255) & 2047;
	    val1 = j;
	    *idr0 = val0;
	    *idr1 = val1;
	    *iar = HAL2_CODECB_CTRL2_W;    /* write the values */
	    /*
	     * spin on the ISR to make sure the value we 
	     * just wrote is latched in before we read it
	     */
	    while (*isr & HAL2_ISR_TSTATUS);
	    *idr0 = 0xffff;
	    *idr1 = 0xffff;
	    /*
	     * read the value back
	     * the CTRL2 regs have separate addresses for the two
	     * words.
	     */
	    *iar = HAL2_CODECB_CTRL2_R0;
	    while (*isr & HAL2_ISR_TSTATUS);
	    rb0 = *idr0;
	    *iar = HAL2_CODECB_CTRL2_R1;
	    while (*isr & HAL2_ISR_TSTATUS);
	    rb1 = *idr0;
	    if (rb0 != val0) {
		printf("ERROR (word 0): wrote 0x%x, got 0x%x\n", val0, rb0);
		badval0++;
	    }
	    if (rb1 != val1) {
		printf("ERROR (word 1): wrote 0x%x, got 0x%x\n", val1, rb1);
		badval1++;
	    }
	}
    }
    if (badval0 || badval1) {
	printf("FAILED: badval0 = %d,  badval1 = %d\n", badval0, badval1);
    }
    printf("end test of HAL2 CODECB CTRL2 register\n");
}

main()
{
    hal2_configure_pbus_pio();
    hal2_codecbctrl2();
}
