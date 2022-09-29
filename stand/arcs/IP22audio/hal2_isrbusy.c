#include "sys/sbd.h"
#include "sys/hal2.h"

hal2_isrbusy()
{
    volatile unsigned long *isrptr=(unsigned long *)PHYS_TO_K1(HAL2_ISR);
    int i;
    unsigned short isr0, isr1, isr2, isr3;
    int nbad = 0;
    
    printf("HAL2 ISR busy/done bit test entered\n");
    printf("Initial value: 0x%x\n", *isrptr);
    
    /*
     * read the ISR 2 million times; verify that it does
     * not change,  and that all the reads are correct. With
     * the HAL quiescent, the busy bit should be 0 ("transaction
     * done").
     */
    for (i = 0; i < 500000; i++) {
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if (isr0 & HAL2_ISR_TSTATUS) {
	    nbad++;
	}
	if (isr1 & HAL2_ISR_TSTATUS) {
	    nbad++;
	}
	if (isr2 & HAL2_ISR_TSTATUS) {
	    nbad++;
	}
	if (isr3 & HAL2_ISR_TSTATUS) {
	    nbad++;
	}
    }
    if (nbad) {
	printf("Error: number of 'busy' values from ISR:%d\n", 
	    nbad);
    }
    printf("HAL2 ISR busy/done bit test done\n");
}

main()
{
    hal2_configure_pbus_pio();
    hal2_isrbusy();
}
