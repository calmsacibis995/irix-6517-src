/************************************************************************
 *
 ************************************************************************/

#include "sys/sbd.h"
#include "sys/hpc3.h"
#include "sys/hal2.h"

void hal2_setbres();

main()
{
    hal2_configure_pbus_pio();
    hal2_setbres();
}

void
hal2_setbres()
{
    volatile unsigned long *idr0 = (unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *idr1 = (unsigned long *)PHYS_TO_K1(HAL2_IDR1);
    volatile unsigned long *iar  = (unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isr  = (unsigned long *)PHYS_TO_K1(HAL2_ISR);
    char line[200];
    int source, inc, modctrl;
    
again:
    printf("Entering HAL2_SETBRES test\n");

    printf("\nPlease enter clock source 0=48k, 1=44.1k, 2=aes rx, -1=exit: \n");
    gets(line);
    source=atoi(line);
    if(source==-1)goto done;

    printf("Please enter inc: \n");
    gets(line);
    inc=atoi(line);

    printf("Please enter modctrl: \n");
    gets(line);
    modctrl=atoi(line);
    
    printf("source %d, inc %d, modctrl %d\n",source,inc,modctrl);

    *isr=0x8;
    *idr0 = source;
printf("Writing 0x%x to 0x%x\n", source, idr0);
    *iar  = HAL2_BRES1_CTRL1_W;
printf("Writing 0x%x to 0x%x\n", HAL2_BRES1_CTRL1_W, iar);
    *idr0 = inc;
printf("Writing 0x%x to 0x%x\n", inc, idr0);
    *idr1 = modctrl;
printf("Writing 0x%x to 0x%x\n", modctrl, idr1);
    *iar  = HAL2_BRES1_CTRL2_W;
printf("Writing 0x%x to 0x%x\n", HAL2_BRES1_CTRL2_W, iar);

    printf("Exiting HAL2_SETBRES test\n");
    goto again;
done:;
    
}
