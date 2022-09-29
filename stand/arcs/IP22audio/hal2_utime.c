#include "sys/sbd.h"
#include "sys/hal2.h"

void
hal2_utime()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *idr1 = (unsigned long *)PHYS_TO_K1(HAL2_IDR1);
    volatile unsigned long *idr2 = (unsigned long *)PHYS_TO_K1(HAL2_IDR2);
    volatile unsigned long *idr3 = (unsigned long *)PHYS_TO_K1(HAL2_IDR3);
    volatile unsigned long *iar = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    int i, j;
    unsigned short pattern[4];
    unsigned short readback[4];
    int max = 0,  maxval;

    volatile unsigned short k;	/* volatile just to prevent optimization */
    unsigned short rb0, rb1;
    int badval=0;
    int average;
    
    printf("begin test of HAL2 UTIME register\n");
    hal2_unreset();
    pattern[0] = pattern[1] = pattern[2] = pattern[3] = 0;
    for (i = 0; i < 100000; i++) {
	int pSecs, pUsecs;
	int rSecs, rUsecs;
	int deltaUsecs;
	/*
	 * make sure usecs and 0.1 usecs are in range
	 * 0xf4240 == 1000000
	 */
	if (((pattern[1] & 0xf) == 0xf) && (pattern[0] > 0x4240)) {
	    printf("usecs out of range err; idr1=0x%x,  idr0=0x%x\n", pattern[1], pattern[0]);
	    pattern[0] = 0x4240;	/* limit usecs */
	}
	if ((pattern[1] & 0xf0) > 0xa0) {
	    printf("0.1 usecs out of range err; idr1=0x%x\n", pattern[1]);
	    pattern[1] = (pattern[1] & 0xf) | 0xa0; /* limit 0.1usecs */
	}
	*idr0 = pattern[0];
	*idr1 = pattern[1];
	*idr2 = pattern[2];
	*idr3 = pattern[3];
	
	*iar = HAL2_UTIME_W;    /* write the values */
	while (*isr & HAL2_ISR_TSTATUS);	/* OK to here */

	*idr0 = 0xffff;		/* clobber the IDR on purpose */
	*idr1 = 0xffff;
	*idr2 = 0xffff;
	*idr3 = 0xffff;
	
	/*
	 * spin for a while to make sure that the value 
	 * is latched in before we read it. Utime values 
	 * get latched in on a fast timer interrupt,  so if
	 * we wait a couple msecs,  we should be ok.
	 */
	while (*isr & HAL2_ISR_USTATUS);

	for (k = 0; k < 10000; k++);		/* spin */
	
	/*
	 * read the value back
	 */
	*iar = HAL2_UTIME_R;
	while (*isr & HAL2_ISR_TSTATUS);
	readback[0] = *idr0;
	readback[1] = *idr1;
	readback[2] = *idr2;
	readback[3] = *idr3;
	
	pSecs = pattern[3] << 16 | pattern[2];
	pUsecs = ((pattern[1] & 0xf) << 16) | pattern[0];
	rSecs = readback[3] << 16 | readback[2];
	rUsecs = ((readback[1] & 0xf) << 16) | readback[0];
	deltaUsecs = (rSecs - pSecs) * 1000000 + (rUsecs - pUsecs);
	if (deltaUsecs < 0) {
	    deltaUsecs += 1000000;
	    if (deltaUsecs < 0) {
		printf("range error\n");
	    }
	}
	/*
	 * determine the (weighted) average delay,  using fixed-point
	 * arithmetic.
	 */
	average = ((9 * average) + (deltaUsecs << 8)) / 10;
	if (i > 100) {
	    j = (deltaUsecs << 8) - average;
	    if ( j < 0 ) j = -j;
	    if (j > max) {
		max = j; 
		maxval = deltaUsecs << 8;
	    }
	}
	pattern[0] = readback[0];
	pattern[1] = readback[1];
	pattern[2] = readback[2];
	pattern[3] = readback[3];
    }
    printf("stats: average delta = %d, max var= %d at %d usecs\n", 
	average, max, maxval);
    printf("end test of HAL2 UTIME register\n");
}

main()
{
    hal2_configure_pbus_pio();
    hal2_utime();
}
