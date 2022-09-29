/*
 *   hal2_initaes.c -
 * 
 *      Note:
 *          In order to perform this test under emulation, hardware
 *          rework is required. This rework will impact the software
 *          setup of the CS8411 (AES RX) so that it can accept signal
 *          on MCK from an external source.
 *
 *          From the application note, "CS8411, CS8412 Operation with
 *          an External VCO":
 *              1. Write to control register 1 (CR1) and set bit 5 high.
 *              2. Write to interrupt enable register 2 (IER2) and set 
 *                 bit 7 high (TEST1).
 *              Pin 19 of the CS8411 will now accept inputs.
 *
 */

#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/hpc3.h"

void hal2_loopaes();
void init_aestx();
void init_aesrx();

main()
{
    hal2_unreset();
    hal2_loopaes();
}

static int mode;

void
hal2_loopaes()
{
    volatile int i;
    char str[256];

    printf("HAL2 AES loop thru test entered\n");
    hal2_configure_pbus_pio();
    hal2_configure_pbus_dma();    
    while (1) {
        printf("Enter 1 for external VCO, 0 for internal: \n");
        gets(str);
        mode = atoi(str);

        init_aesrx();
        /*init_aestx();   XXX don't init the TX just yet */
    }
}

