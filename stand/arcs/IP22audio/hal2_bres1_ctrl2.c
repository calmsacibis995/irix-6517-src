/************************************************************************
 *
 * hal2_bres1_ctrl2.c -
 *
 *    control parameter 2 of the bresenham clock generator:
 *       IDR0 = INC (16 bits): rate generator increment
 *       IDR1 = MODCTRL (16 bits): modulus control
 *
 *    note on use of these paramters in pmod.
 *
 *    Fout = Fs * (INC/MOD)             [INC < MOD < 65535]
 *     
 *    MODCTRL = 0x00FFFF & (MOD - INC - 1)
 *
 ************************************************************************/

#include "sys/sbd.h"
#include "sys/hpc3.h"
#include "sys/hal2.h"

main()
{
    hal2_unreset();
    hal2_configure_pbus_pio();
    hal2_bres1_ctrl2();
}

void
hal2_bres1_ctrl2()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *idr1 = (unsigned long *)PHYS_TO_K1(HAL2_IDR1);
    volatile unsigned long *iar  = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr  = (unsigned long *)PHYS_TO_K1(HAL2_ISR);

    unsigned short inc;
    unsigned short mod;
    unsigned short modctrl;

    unsigned short modctrl_rb, inc_rb;
    unsigned int   modctrl_errors = 0, inc_errors = 0; 

    unsigned int   total = 0;
    int            i, j, k;
    
    printf("Entering HAL2_BRES1_CTRL2 test\n");
    
    for (j = 0; j < 65535; j+=32) {               /* loop thru mod vals */
    
	mod = j;
	for (i = 0; i < mod; i+=32) {             /* loop inc up to mod */
	    
	    inc = i;
	    
	    /*
	     * original formula for modctrl in pmod is incorrect.
	     * modctrl should be inc - mod - 1, making it less
	     * that zero.
	     */
	    modctrl = 0x00ffff & (inc - mod - 1);
	    
	    *idr0 = inc;
	    *idr1 = modctrl;
	    
	    *iar  = HAL2_BRES1_CTRL2_W;
	    
	    /* spin */
	    while (*isr & HAL2_ISR_TSTATUS);
	    
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_BRES1_CTRL2_R0;
	    while (*isr & HAL2_ISR_TSTATUS);   /*spin*/
	    inc_rb = *idr0;
	    if (inc_rb != inc) {
		inc_errors++;
		total++;
	    }
	    
	    *idr0 = 0xbadbad;
	    *iar  = HAL2_BRES1_CTRL2_R1;
	    while (*isr & HAL2_ISR_TSTATUS);
	    modctrl_rb = *idr0;
	    if (modctrl_rb != modctrl) {
		modctrl_errors++;
		total++;
	    }
	}
    }
        
    
    if (total) {
        printf("errors:\n");
	printf("modctl errors = %d\n", modctrl_errors);
	printf("inc    errors = %d\n", inc_errors);
    }
    printf("Exiting HAL2_BRES1_CTRL2 test\n");
}
