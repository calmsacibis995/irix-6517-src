#include "sys/sbd.h"
#include "sys/hal2.h"

hal2_isrgreset()
{
    volatile unsigned long *isrptr=(unsigned long *)PHYS_TO_K1(HAL2_ISR);
    int i;
    unsigned short isr0, isr1, isr2, isr3;
    int nbad = 0;
    
    printf("HAL2 ISR global reset bit test entered\n");

    for (i = 0; i < 500000; i++) {
	/*
	 * unreset the HAL2
	 */
	*isrptr = *isrptr & ~(HAL2_ISR_GLOBAL_RESET_N);
	
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if (isr0 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}
	if (isr1 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}
	if (isr2 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}
	if (isr3 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}

	/*
	 * put the HAL2 in global reset mode.
	 */
	*isrptr = *isrptr | HAL2_ISR_GLOBAL_RESET_N;
		
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if ((isr0 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr1 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr2 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr3 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
    }
    if (nbad) {
	printf("Error: readback failed  from ISR %d times\n", 
	    nbad);
    }
    printf("HAL2 ISR global reset bit bit test done\n");
}

main()
{
    hal2_configure_pbus_pio();
    hal2_isrgreset();
}
