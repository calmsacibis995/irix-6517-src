/***********************************************************************\
*       File:           init_epcuart.c                                  *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/nvram.h>
#include <sys/z8530.h>

#include "ip25prom.h"
#include "prom_externs.h"

static void configure_port(uint);
static unsigned int get_baudrate(uint);

/*
 * init_epc_uart
 *	Initializes the first uart port on the Master IO4.
 */

void
init_epc_uart(void)
{
	__scunsigned_t	baseaddr;	/* Base address of EPC DUART1 */

	sysctlr_message("Initing UART Chan A");
	configure_port(CHN_A);

	sysctlr_message("Initing UART Chan B");
	configure_port(CHN_B);

	baseaddr = SWIN_BASE(EPC_REGION, master_epc_adap())  + 
			(7 * (!getendian()));
	set_epcuart_base(baseaddr);

	epcuart_puts("Testing Channel B on the EPC UART", CHN_B);
}

static void 
configure_port(uint chan)
{
    register volatile u_char	*cntrl;
    uint baud;			/* Computed baud rate */
    __scunsigned_t padap = master_epc_adap();

    /* Setup the control pointer */
    cntrl = (volatile u_char*) (SWIN_BASE(EPC_REGION, padap) +
		(7 * !getendian()) + chan);

    if (chan == CHN_A) {
        WR_CNTRL(cntrl, WR9, WR9_RST_CHNA);
    } else {
        WR_CNTRL(cntrl, WR9, WR9_RST_CHNB);
    }

    /* Allow extended mode commands; among other things, this allows us
     * to read some of the write registers using read register aliases
     */
    WR_CNTRL(cntrl, WR15, WR15_Z85130_EXTENDED);
    WR_CNTRL(cntrl, WR7, WR7_EXTENDED_READ);

    /* Set clock factor, parity, and stop bits */
    WR_CNTRL(cntrl, WR4, WR4_X16_CLK | WR4_1_STOP);

    /* Set up receiver/transmitter operating parameters */
    WR_CNTRL(cntrl, WR3, WR3_RX_8BIT);
    WR_CNTRL(cntrl, WR5, WR5_TX_8BIT);

    /* Set encoding to NRZ mode */
    WR_CNTRL(cntrl, WR10, 0);

    /* receiver/transmitter clock source is baud rate generator */
    WR_CNTRL(cntrl, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG);

    /* Figure out what the baud rate is and write it into the
    * BRG count down registers.
    */
    baud = get_baudrate(chan);
    baud = ((CLK_SPEED + baud * CLK_FACTOR) / (baud * CLK_FACTOR * 2) - 2 ); 

    /* Set the baud rate, safer to disable BRG first */
    WR_CNTRL(cntrl, WR14, 0x00);
    WR_CNTRL(cntrl, WR12, baud & 0xff);
    WR_CNTRL(cntrl, WR13, (baud >> 8) & 0xff);
    WR_CNTRL(cntrl, WR14, WR14_BRG_ENBL);

    /* Enable receiver and transmitter */
    WR_CNTRL(cntrl, WR3, WR3_RX_8BIT | WR3_RX_ENBL);
    WR_CNTRL(cntrl, WR5, WR5_TX_8BIT | WR5_TX_ENBL);

    /* Try to avoid the "mysterious" BREAK signal */
    WR_WR0(cntrl, WR0_RST_EXT_INT);
    if (RD_RR0(cntrl) & RR0_BRK) {
	while (RD_RR0(cntrl) & RR0_RX_CHR) /* LOOP */ ;
	WR_WR0(cntrl, WR0_RST_EXT_INT);
    }

    /* Clear FIFO and any outstanding errors */
    while (RD_RR0(cntrl) & RR0_RX_CHR) {
	RD_CNTRL(cntrl, RR8);
	WR_WR0(cntrl, WR0_RST_ERR);
    }
}

static unsigned int
get_baudrate(uint chan)
{
    char baudstr[NVLEN_LBAUD + 1];
    unsigned i;
    unsigned baud_rate = 0;
    unsigned default_baud;

    if (chan == CHN_A)
	default_baud = 9600;
    else
	default_baud = 19200;

    /* Don't try to read the baud rate out of the NVRAM if the defaults
     * switch is set 
     */
    if (sysctlr_getdebug() & VDS_DEFAULTS)
	return default_baud;

    if (nvram_okay()) {

	/* Read the baud rate out of the prom and convert it to an
	 * integer
	 */
	if (chan == CHN_A) 
	    get_nvram(NVOFF_LBAUD, NVLEN_LBAUD, baudstr);
	else
	    get_nvram(NVOFF_RBAUD, NVLEN_RBAUD, baudstr);

	for (i = 0; baudstr[i] >= '0' && baudstr[i] <= '9';  i++)
	    baud_rate = baud_rate*10 + baudstr[i] - '0';

	if (baud_rate < 50 || baud_rate > 38400 || ((baud_rate % 300) != 0))
	    baud_rate = default_baud;
    } else {
	baud_rate = default_baud;
    }

    return baud_rate;
} 
