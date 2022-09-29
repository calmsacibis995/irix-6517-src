#include "sys/sbd.h"
#include "sys/hal2.h"

void
hal2_codecbctrl1()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iar = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    int i, nbad =0;
    unsigned short j;
    unsigned short rb0, rb1;
    int badval[256];
    
    printf("begin test of HAL2 CODECB CTRL1 register\n");
    *isr = 0x8;
    /*
     * clear the badval array
     */
    for (i = 0; i < 256; i++) {
	badval[i] = 0;
    }
    /*
     * the HAL2 ADC CTRL1 reg has 8 significant bits
     * so we just run through all possible values a few thousand
     * times.
     */
    for (i = 0; i < 4000; i++) {
	for (j = 0; j < 256; j++) {
	    *idr0 = j;
	    *iar = HAL2_CODECB_CTRL1_W;    /* write the values */
	    /*
	     * spin on the ISR to make sure the value we 
	     * just wrote is latched in before we read it
	     */
	    while (*isr & HAL2_ISR_TSTATUS);
	    /*
	     * read back twice,  just to make sure
	     * back-to-back reads will agree.
	     */
	    *idr0 = 0xffff;
	    *iar = HAL2_CODECB_CTRL1_R;
	    while (*isr & HAL2_ISR_TSTATUS);
	    rb0 = *idr0;
	    *iar = HAL2_CODECB_CTRL1_R;
	    while (*isr & HAL2_ISR_TSTATUS);
	    rb1 = *idr0;
	    if (rb0 != j) {
		nbad++;
	        printf("ERROR (read 1): wrote 0x%x, got 0x%x\n",
			j, rb0);
		badval[j]++;
	    }
	    if (rb1 != j) {
		nbad++;
	        printf("ERROR (read 2): wrote 0x%x, got 0x%x\n",
			j, rb1);
		badval[j]++;
	    }
	}
    }
    /*
     * now,  if there were any bad readbacks,  print for which
     * values they occurred. If a bit is bad,  we should expect
     * to see 128 bad values...
     */
    if (nbad) {
	printf("bad readbacks: "); 
	for (i = 0; i < 256; i++) {
	    if (badval[i]) {
		printf("0x%x (%d), ", i, badval[i]);
	    }
	}
	printf("\n");
    }
    printf("end test of HAL2 CODECB CTRL1 register\n");
}

main()
{
    hal2_configure_pbus_pio();
    hal2_codecbctrl1();
}
