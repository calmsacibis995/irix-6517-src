/***********************************************************************\
*	File:		ioc3uart.c					*
*									*
*	IOC3 ASIC UART routines.  The IOC3 ASIC resides on the IO6	*
*	card and forms the basis for a system console.			*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/ns16550.h>
#include <sys/SN/agent.h>
#include <sys/SN/klconfig.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/PCI_defs.h>

#include "ip27prom.h"
#include "hub.h"
#include "ioc3uart.h"
#include "libasm.h"
#include "rtc.h"
#include "pod.h"

#define INIT_TIMEOUT	10000	/* microseconds */
#define PUTC_TIMEOUT	10000	/* microseconds */

#if 0
/* Junk bus needs its registers 64-bit aligned. */
typedef __uint64_t	prom_uart_reg_t;

#define PROM_SER_CLK_SPEED 		12000000
#define PROM_SER_DIVISOR(x)     	(PROM_SER_CLK_SPEED / ((x) * 16))

#else

#define PROM_SER_CLK_SPEED	(SER_XIN_CLK / SER_PREDIVISOR)

#define PROM_SER_DIVISOR(y)		SER_DIVISOR(y, PROM_SER_CLK_SPEED)
/* IOC3 likes its registers 32-bit aligned. */
typedef uart_reg_t	prom_uart_reg_t;
#endif

/*
 * ioc3uart_init
 */

static int baud_table[] = {
    /* 110, 300, 1200, 2400, 4800, 9600, 19200, 38400 */
    300, 1200, 2400, 9600, 19200, 38400
};

#define	NBAUD	(sizeof (baud_table) / sizeof (baud_table[0]))

static int
valid_baud(int baud)
{
    int		i;

    for (i = 0; i < NBAUD; i++)
	if (baud == baud_table[i])
	    return 1;

    return 0;
}


static int
next_baud(int baud)
{
    int		i;

    for (i = 1; i < NBAUD; i++)
	if (baud_table[i] > baud)
	    return baud_table[i];

    return baud_table[0];
}

#if 0
#define IOC3DELAY delay(2)
#else
#define IOC3DELAY delay(200)
#endif

static void
configure_port(volatile prom_uart_reg_t *cntrl, int baud)
{
    int			    clkdiv;
    rtc_time_t		    expire;

    clkdiv = PROM_SER_DIVISOR(baud);

    hubprintf("About to read %lx\n", cntrl);

    expire = rtc_time() + INIT_TIMEOUT;

    /* make sure the transmit FIFO is empty */
    while ((RD_REG(cntrl, LINE_STATUS_REG) & LSR_XMIT_EMPTY) == 0 &&
	   rtc_time() < expire)
	IOC3DELAY;

    WR_REG(cntrl, LINE_CNTRL_REG, LCR_DLAB);
	IOC3DELAY;
    WR_REG(cntrl, DIVISOR_LATCH_MSB_REG, (clkdiv >> 8) & 0xff);
	IOC3DELAY;
    WR_REG(cntrl, DIVISOR_LATCH_LSB_REG, clkdiv & 0xff);
	IOC3DELAY;

    /* while dlab is set program the pre-divisor */
    WR_REG(cntrl, SCRATCH_PAD_REG, SER_PREDIVISOR * 2);
    IOC3DELAY;

    /* set operating parameters and set DLAB to 0 */
    WR_REG(cntrl, LINE_CNTRL_REG, LCR_8_BITS_CHAR | LCR_1_STOP_BITS);
	IOC3DELAY;

    /* no interrupt */
    WR_REG(cntrl, INTR_ENABLE_REG, 0x0);
	IOC3DELAY;

    /* enable FIFO mode and reset both FIFOs */
    WR_REG(cntrl, FIFO_CNTRL_REG, FCR_ENABLE_FIFO);
	IOC3DELAY;

    WR_REG(cntrl, FIFO_CNTRL_REG,
	   FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET | FCR_XMIT_FIFO_RESET);

    /* turn on RTS and DTR in case the console terminal expects flow
     * control signals to make sense
     */
    WR_REG(cntrl, MODEM_CNTRL_REG, MCR_DTR | MCR_RTS);
}


void
ioc3_chip_init(console_t *console)
{
    struct ioc3_configregs *cb = (struct ioc3_configregs*)console->config_base;
    PCI_OUTW(&cb->pci_scr,
	     PCI_INW(&cb->pci_scr) |
	     PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE);
    PCI_OUTW(&cb->pci_lat, 0xff00);
}


void ioc3uart_init(void *init_data)
{
    volatile prom_uart_reg_t	   *cntrl;
    int			    baud;
    console_t *console = init_data;

    set_ioc3uart_base(console->uart_base);
    cntrl = (volatile prom_uart_reg_t *) (console->uart_base);
    baud  = console->baud ;

    hubprintf("IOC3_SIO_CR address == 0x%lx\n",
		console->memory_base + IOC3_SIO_CR);

    ioc3_chip_init(console);

    SW(console->memory_base + IOC3_SIO_CR,
		((UARTA_BASE >> 3) << SIO_CR_SER_A_BASE_SHIFT) |
		((UARTB_BASE >> 3) << SIO_CR_SER_B_BASE_SHIFT) |
		(4 << SIO_CR_CMD_PULSE_SHIFT));

    hubprintf("IOC3_SIO_CR == 0x%x\n", LW(console->memory_base + IOC3_SIO_CR));

    SW(console->memory_base + IOC3_GPDR, 0);

    SW(console->memory_base + IOC3_GPCR_S,
		GPCR_INT_OUT_EN | GPCR_MLAN_EN |
		GPCR_DIR_SERA_XCVR | GPCR_DIR_SERB_XCVR);

    configure_port(cntrl, valid_baud(baud) ? baud : 9600);
}

/*
 * ioc3uart_poll
 *
 *   Returns 1 if a character is available, 0 if not.
 */

int ioc3uart_poll(void)
{
    volatile prom_uart_reg_t	   *cntrl;
    uchar_t		    status;

    cntrl  = (volatile prom_uart_reg_t *) get_ioc3uart_base();
    status = RD_REG(cntrl, LINE_STATUS_REG);

#if 0
    if (status & LSR_BREAK) {
	int		baud;

	/* pop the NULL char associated with the BREAK off the FIFO */

	RD_REG(cntrl, RCV_BUF_REG);

	/* cycle baud rate */

	baud = next_baud(get_ioc3uart_baud());

	set_ioc3uart_baud(baud);

	configure_port(cntrl, baud);

	ioc3uart_puts("\nBaud changed\n");

	/* XXX set new baud rate in NVRAM variable, see main.c */

	return 0;
    }
#endif

    if (status & (LSR_OVERRUN_ERR | LSR_PARITY_ERR | LSR_FRAMING_ERR)) {
	if (status & LSR_OVERRUN_ERR)
	    ioc3uart_puts("\nNS16550 overrun error\n");
	else if (status & LSR_FRAMING_ERR)
	    ioc3uart_puts("\nNS16550 framing error\n");
	else
	    ioc3uart_puts("\nNS16550 parity error\n");

	return 0;
    }

    return ((status & LSR_DATA_READY) != 0);
}

/*
 * ioc3uart_readc
 *
 *   Reads a character.  May only be called after ioc3uart_poll has
 *   indicated a character is available.
 */

int ioc3uart_readc(void)
{
    volatile prom_uart_reg_t	   *cntrl;

    cntrl  = (volatile prom_uart_reg_t *) get_ioc3uart_base();

    return RD_REG(cntrl, RCV_BUF_REG);
}

/*
 * ioc3uart_getc
 *
 *   Waits for a character and returns it.
 *   Flashes hub LEDs while waiting.
 */

#define FLASH_CYCLES	2000

int ioc3uart_getc(void)
{
    int		f, i, c;
    uchar_t		    status;

    for (f = 0xc0; ; f ^= IOC3UART_FLASH) {
	for (i = 0; i < FLASH_CYCLES; i++)
	    if (ioc3uart_poll()){
		c = ioc3uart_readc();
		return  c;
	    }
	hub_led_set(f);
    }

    /*NOTREACHED*/

    return 0;
}

/*
 * ioc3uart_putc
 *
 *   Writes a single character when ready.
 *   Returns 0 if successful, -1 if timed out.
 *
 *   Converts \n to \r\n (can add 256 to quote a plain \n).
 */

int ioc3uart_putc(int c)
{
    volatile prom_uart_reg_t	   *cntrl;
    rtc_time_t			    expire;

    if (c > 255)
	c -= 256;
    else if (c == '\n' && ioc3uart_putc('\r') < 0)
	return -1;

    cntrl  = (volatile prom_uart_reg_t *) get_ioc3uart_base();

    expire = rtc_time() + PUTC_TIMEOUT;

    while (! (RD_REG(cntrl, LINE_STATUS_REG) & LSR_XMIT_BUF_EMPTY))
	if (rtc_time() > expire)
	    return -1;

    WR_REG(cntrl, XMIT_BUF_REG, c);

    return 0;
}

/*
 * ioc3uart_puts
 *
 *   Writes a null-terminated string.
 *   Returns 0 if successful, -1 if ioc3_putc failed.
 *   Contains special support so the string may reside in the PROM.
 */

int ioc3uart_puts(char *s)
{
    int		c;

    if (s == 0)
	s = "<NULL>";

    while ((c = LBYTE(s)) != 0) {
	if (ioc3uart_putc(c) < 0)
	    return -1;
	s++;
    }

    return 0;
}
