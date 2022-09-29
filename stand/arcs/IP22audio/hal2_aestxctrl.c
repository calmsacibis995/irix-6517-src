#include "sys/sbd.h"
#include "sys/hal2.h"

void
hal2_aestx_ctrl()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iar = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    int i, nbad =0;
    unsigned short j;
    unsigned short val;
    unsigned short rb0, rb1;
    int badval[1024];
    
    printf("begin test of HAL2 AESTX CTRL register\n");
    /*
     * clear the badval array
     */
    for (i = 0; i < 1024; i++) {
	badval[i] = 0;
    }
    for (i = 0; i < 32000; i++) {
	for (j = 0; j < 32; j++) {
	    /*
	     * loop through all values for bits 0-4; let 
	     * bits 8 and 9 be the same as bits 0-1.
	     */
	    val = j | ((j & 0x3) << 8);
	    *idr0 = val;
	    *iar = HAL2_AESTX_CTRL_W;    /* write the values */
	    /*
	     * spin on the ISR to make sure the value we 
	     * just wrote is latched in before we read it
	     */
	    while (*isr & HAL2_ISR_TSTATUS);
	    /*
	     * read back twice,  just to make sure
	     * back-to-back reads will agree.
	     */
	    *idr0 = 0xbadbad;
	    *iar = HAL2_AESTX_CTRL_R;
	    while (*isr & HAL2_ISR_TSTATUS);    /* spin */
	    rb0 = *idr0;
	    *iar = HAL2_AESTX_CTRL_R;
	    while (*isr & HAL2_ISR_TSTATUS);    /* spin */
	    rb1 = *idr0;
	    if (rb0 != val) {
		nbad++;
		badval[val]++;
	    }
	    if (rb1 != val) {
		nbad++;
		badval[val]++;
	    }
	}
    }
    /*
     * now,  if there were any bad readbacks,  print for which
     * values they occurred. 
     */
    if (nbad) {
	printf("bad readbacks: "); 
	for (i = 0; i < 1024; i++) {
	    if (badval[i]) {
		printf("0x%x (%d), ", i, badval[i]);
	    }
	}
	printf("\n");
    }
    printf("end test of HAL2 AESTX CTRL register\n");
}

main()
{
    hal2_unreset();
    hal2_configure_pbus_pio();
    hal2_aestx_ctrl();
}
