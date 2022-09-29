/********************************************************************
 *
 * aestx.c -
 *    test out those pesky aes transmittter registers which are 
 *    memory mapped into the MIPS land.
 *    
 *    there are 4 control registers for the Crystal AES/IEC
 *    digital tranmitter, 4 user status registers, and 24 channel
 *    status registers. each are a byte wide. thus, we can fully
 *    support the IEC spec with respect to the channel status
 *    bits, but we are still weak on the user status bits.
 *
 ********************************************************************/

#include <sys/hal2.h>
#include <sys/sbd.h>

#define HAL2_NUM_AESTX_REGISTERS 28

extern void hal2_configure_pbus_pio();
static void init_reg_addr_table(void);
void        hal2_test_aes_tx(void);
void        init_aestx();

unsigned long aes_tx_register[HAL2_NUM_AESTX_REGISTERS];

main()
{
    init_reg_addr_table();
    hal2_unreset();
    hal2_configure_pbus_pio();
    init_aestx();
    hal2_test_aes_tx();
}

static void
init_reg_addr_table()
{
    aes_tx_register[0]  = AESTX_UD0;
    aes_tx_register[1]  = AESTX_UD1;
    aes_tx_register[2]  = AESTX_UD2;
    aes_tx_register[3]  = AESTX_UD3;
    aes_tx_register[4]  = AESTX_CS0;
    aes_tx_register[5]  = AESTX_CS1;
    aes_tx_register[6]  = AESTX_CS2;
    aes_tx_register[7]  = AESTX_CS3;
    aes_tx_register[8]  = AESTX_CS4;
    aes_tx_register[9]  = AESTX_CS5;
    aes_tx_register[10] = AESTX_CS6;
    aes_tx_register[11] = AESTX_CS7;
    aes_tx_register[12] = AESTX_CS8;
    aes_tx_register[13] = AESTX_CS9;
    aes_tx_register[14] = AESTX_CS10;
    aes_tx_register[15] = AESTX_CS11;
    aes_tx_register[16] = AESTX_CS12;
    aes_tx_register[17] = AESTX_CS13;
    aes_tx_register[18] = AESTX_CS14;
    aes_tx_register[19] = AESTX_CS15;
    aes_tx_register[20] = AESTX_CS16;
    aes_tx_register[21] = AESTX_CS17;
    aes_tx_register[22] = AESTX_CS18;
    aes_tx_register[23] = AESTX_CS19;
    aes_tx_register[24] = AESTX_CS20;
    aes_tx_register[25] = AESTX_CS21;
    aes_tx_register[26] = AESTX_CS22;
    aes_tx_register[27] = AESTX_CS23;
}

void
hal2_test_aes_tx(void)
/*
 * non-audio bytes (viz., channel status and user status bytes)
 * should have all 256 values run thru them.
 */
{
    int            i, j;
    int            error_count = 0;
    
    volatile unsigned long *tx_register, readback;

    for (i = 0; i < 28; i++) {
        tx_register = (unsigned long *)PHYS_TO_K1(aes_tx_register[i]);

        for (j = 0; j < 256; j++) {
             *tx_register = j;
             us_delay(500);
             readback     = *tx_register;
             if (readback != j) {
                 /*printf("%x w = %x r = %x\n", tx_register, j, readback);*/
                 error_count++;
             }
        }
    } 
    printf("cumulative error count = %d\n", error_count);
}

void
init_aestx()
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
     *   6    : M0 (selects MCK freq.; assume 0 for now)
     *   7    : M1 (selects MCK freq.; assume 0 for now)
     *
     *   result = 0010 0111 = 0x27
     */
    *aestx_cr2 = 0x27;
    us_delay(50);
    readback = *aestx_cr2;
    if (readback != 0x27)
        printf("readback of control reg. 2 failed\n");

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
    readback = *aestx_cr1;
    if (readback != 0x00)
        printf("readback of control reg. 1 failed\n");

    /*
     * AESTX_CR3:
     *
     *  bit   function
     *   0    : SCED
     *   1    : MSTR
     *   2    : FSF0
     *   3    : FSF1
     *   4    : SDF0
     *   5    : SDF1
     *   6    : SDF2
     *   7    : xxx
     *
     *   result = 0000 0000 = 0x00
     */
    *aestx_cr3 = 0x00;
    us_delay(50);
    readback = *aestx_cr3;
    if (readback != 0x00)
        printf("readback of control reg. 3 failed\n");
}

