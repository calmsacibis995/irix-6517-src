#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/hpc3.h"

/*#define EMULATION  1*/

void
init_aesrx()
/*
 *  init_aesrx() configures the AES receiver that hangs off the PBUS.
 *  It assumes that the call to configure the PBUS for PIO has already
 *  been made.
 *
 *  This function does, among other things:
 *  1. brings the chip out of reset.
 *  2. selects the memory buffer mode (mode 0).
 *
 */
{
    unsigned long *aesrx_cr1 = (unsigned long *) PHYS_TO_K1(AESRX_CR1);
    unsigned long *aesrx_cr2 = (unsigned long *) PHYS_TO_K1(AESRX_CR2);
    unsigned long *aesrx_ier2 = (unsigned long *) PHYS_TO_K1(AESRX_SR2);
    unsigned long readback;
   
    /*
     * AESRX_CR1:
     *
     *  bit   function
     *   0    : ~RST (set to 1 to bring out of reset)
     *   1    : UNUSED
     *   2    : B0 (selects buffer memory mode; 0 for buffer mode 0)
     *   3    : B1 (selects buffer memory mode; 0 for buffer mode 0)
     *   4    : CS2/~CS1 (selects channel status subframe; set to 0)
     *   5    : IER/~SR (set to 0 for status register enable)
     *   6    : FCEN (freq. comp. enable; set to 0)
     *   7    : FPLL (FSYNC from PLL; set to 0...get FSYNC from RXP/RXN)
     *
     *   result = 0000 0001 = 0x1
     */

#ifdef EMULATION
    *aesrx_cr1 = 0x0;     /* specifically reset the chip */
    us_delay(1);
    readback = 0xff & *aesrx_cr1;
    printf("aesrx_cr1 after resetting chip: %x\n", readback);
    *aesrx_cr1  = 0x21;
    us_delay(1);
    readback = 0xff & *aesrx_cr1;
    if (readback != 0x21) {
        printf("aesrx_cr1 didn't accept write value: %x\n", readback);
    }
    *aesrx_ier2 = 0x80;
    us_delay(1);
    readback = 0xff & *aesrx_ier2;
    if (readback != 0x80) {
        printf("aesrx_ier2 didn't accept write value: %x\n", readback);
    }
#else
    readback = 0xff & *aesrx_cr1;
    printf("BEFORE: aesrx_cr1 = %x\n", readback);
    *aesrx_cr1 = 0x1;
    us_delay(50);
    readback = 0xff & *aesrx_cr1;
    if (readback != 0x1) {
        printf("aesrx_cr1 didn't accept write value: %x\n", readback);
    }
    readback = 0xff & *aesrx_cr1;
    printf("AFTER: aesrx_cr1 = %x\n", readback);
#endif /*EMULATION*/

    /*
     * AESRX_CR2:
     *
     *  bit   function
     *   0    : SCED (1=data on falling edge of SCK; 0=rising edge)
     *   1    : MSTR (1=SCK & FSYNC are outputs)
     *   2    : FSF0 (=0)
     *   3    : FSF1 (=1)
     *   4    : SDF0 (=0)
     *   5    : SDF1 (=1)
     *   6    : SDF2 (=0)
     *   7    : ROER (1=repeat audio data on error)
     *
     *   result = 1010 1011 = 0xab
     */
    readback = 0xff & *aesrx_cr2;
    printf("BEFORE: aesrx_cr2 = %x\n", readback);
    *aesrx_cr2 = 0xab;
    us_delay(50);
    readback = 0xff & *aesrx_cr2;
    if (readback != 0xab) {
        printf("aesrx_cr2 didn't accept write value: %x\n", readback);
    }
    readback = 0xff & *aesrx_cr2;
    printf("AFTER: aesrx_cr2 = %x\n", readback);
}


void
init_aestx()
/*
 *  init_aestx() configures the AES tranmitter that hangs off the PBUS.
 *  It assumes that the call to configure the PBUS for PIO has already
 *  been made.
 *
 *  This function does, among other things:
 *  1. brings the chip out of reset.
 *  2. unmutes the chip so that audio will be transmitted.
 *  3. places the memory in the correct mode for our application.
 *
 *  Note: nothing is done with the AES TX status register as it
 *  contains only flags that indicate position into buffer memory
 *  for channel status and user status bytes.
 */
{
    unsigned long *aestx_cr1 = (unsigned long *) PHYS_TO_K1(AESTX_CR1);
    unsigned long *aestx_cr2 = (unsigned long *) PHYS_TO_K1(AESTX_CR2);
    unsigned long *aestx_cr3 = (unsigned long *) PHYS_TO_K1(AESTX_CR3);
    unsigned long readback;
   
    /*
     * AESTX_CR2:
     *
     *  bit   function
     *   0    : ~RST (set to 1 to bring out of reset)
     *   1    : ~MUTE (must be set to 1 for data tranmission)
     *   2    : CRCE (channel status CRC enable...professional mode only)
     *   3    : B0 (selects buffer memory mode; 0 for buffer mode 0)
     *   4    : B1 (selects buffer memory mode; 0 for buffer mode 0)
     *   5    : V (validity bit fo current sample; set to 1?)
     *   6    : M0 (selects MCK freq.; = 0)
     *   7    : M1 (selects MCK freq.; = 1)
     *
     *   result = 1000 0111 = 0x87
     *
     *   go with this for the time being. XXX
     *   1000 0111 = 0x87
     */
    *aestx_cr2 = 0x0;   /* explicitly reset the TX */
    us_delay(50);
    readback = 0xff & *aestx_cr2;
    if (readback != 0x0) printf("aestx_cr2 reset failed.\n");

    *aestx_cr2 = 0x87;
    us_delay(1);
    readback = 0xff & *aestx_cr2;
    if (readback != 0x87) printf("aestx_cr2 setup failed.\n");

    /*
     * AESTX_CR1:
     *
     *  bit   function
     *   0    : MASK0 (set to 0)
     *   1    : MASK1 (set to 0)
     *   2    : MASK2 (set to 0)
     *   3    : xxx
     *   4    : xxx
     *   5    : xxx
     *   6    : TRNPT (transparent; for syncing output to input)
     *   7    : BKST (block start; for syncing output to input)
     *
     *   result = 0000 0000 = 0x00
     */
    *aestx_cr1 = 0x00;
    us_delay(50);
    readback = 0xff & *aestx_cr1;
    if (readback != 0x00) printf("aestx_cr1 setup failed.\n");

    /*
     * AESTX_CR3:
     *
     *  bit   function
     *   0    : SCED = 1
     *   1    : MSTR = 0
     *   2    : FSF0 = 0
     *   3    : FSF1 = 1
     *   4    : SDF0 = 0
     *   5    : SDF1 = 0
     *   6    : SDF2 = 0
     *   7    : xxx
     *
     *   result = 0000 1001 = 0x9
     */
    *aestx_cr3 = 0x9;
    us_delay(50);
    readback = 0xff & *aestx_cr3;
    if (readback != 0x9) printf("aestx_cr3 setup failed.\n");


}

