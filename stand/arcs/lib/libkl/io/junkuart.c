
/***********************************************************************\
*	File:		junkuart.c					*
*									*
*	IOC3 ASIC UART routines.  The IOC3 ASIC resides on the IO6	*
*	card and forms the basis for a system console.			*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/ns16550.h>
#include <sys/SN/arch.h>
#include <sys/SN/agent.h>
#include <stdarg.h>

#include "hub.h"
#include "junkuart.h"
#include "libasm.h"
#include "rtc.h"

#define LBYTE(caddr) \
	(char) ((*(int *) ((__psunsigned_t) (caddr) & ~3) << \
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)


#if 1
/* Junk bus needs its registers 64-bit aligned. */
typedef __uint64_t	prom_uart_reg_t;

#define PROM_SER_CLK_SPEED		12000000
#define PROM_SER_DIVISOR(x)		(PROM_SER_CLK_SPEED / ((x) * 16))

#else
#define PROM_SER_CLK_SPEED	SER_CLK_SPEED
#define PROM_SER_DIVISOR(y)		SER_DIVISOR(y)
/* IOC3 likes its registers 32-bit aligned. */
typedef __uint32_t	prom_uart_reg_t;
#endif

#define INIT_TIMEOUT		100000		/* microseconds */
#define PUTC_TIMEOUT		10000		/* microseconds */

/*
 * junkuart_init
 */

static int baud_table[] = {
    /* 110, 300, 1200, 2400, 4800, 9600, 19200, 38400 */
    300, 1200, 2400, 9600, 19200, 38400
};

#define	NBAUD	(sizeof (baud_table) / sizeof (baud_table[0]))


ulong get_junkuart_base(void)
{
    return (ulong)LOCAL_HUB(MD_UREG1_8);
}

int get_junkuart_baud(void)
{
    return 9600;
}

/*
 * junkuart_probe
 *
 *   Returns non-zero if a junkuart is present.
 */

int junkuart_probe(void)
{
    volatile prom_uart_reg_t	   *cntrl;

    cntrl  = (volatile prom_uart_reg_t *) get_junkuart_base();

    WR_REG(cntrl, SCRATCH_PAD_REG, (uchar_t) 0x55);
    WR_REG(cntrl, INTR_ENABLE_REG, 0x0);	/* Un-float bus */
    if (RD_REG(cntrl, SCRATCH_PAD_REG) != (uchar_t) 0x55)
	return 0;

    WR_REG(cntrl, SCRATCH_PAD_REG, (uchar_t) 0xaa);
    WR_REG(cntrl, INTR_ENABLE_REG, 0x0);	/* Un-float bus */
    if (RD_REG(cntrl, SCRATCH_PAD_REG) != (uchar_t) 0xaa)
	return 0;

    WR_REG(cntrl, SCRATCH_PAD_REG, (uchar_t) 0);

    return 1;
}

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

#define IOC3DELAY	delay(2)

static void
configure_port(volatile prom_uart_reg_t *cntrl, int baud)
{
    rtc_time_t		expire;
    int			clkdiv;

    clkdiv = PROM_SER_DIVISOR(baud);

    expire = rtc_time() + INIT_TIMEOUT;

    /* make sure the transmit FIFO is empty */
    while (! (RD_REG(cntrl, LINE_STATUS_REG) & LSR_XMIT_EMPTY)) {
	IOC3DELAY;
	if (rtc_time() > expire)
	    break;
    }

    WR_REG(cntrl, LINE_CNTRL_REG, LCR_DLAB);
	IOC3DELAY;
    WR_REG(cntrl, DIVISOR_LATCH_MSB_REG, (clkdiv >> 8) & 0xff);
	IOC3DELAY;
    WR_REG(cntrl, DIVISOR_LATCH_LSB_REG, clkdiv & 0xff);
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
}

/* ARGSUSED */
void junkuart_init(void *init_data)
{
    volatile prom_uart_reg_t	   *cntrl;
    int			    baud;

    cntrl = (volatile prom_uart_reg_t *) get_junkuart_base();
    baud  = get_junkuart_baud();

    configure_port(cntrl, valid_baud(baud) ? baud : 9600);
}

/*
 * junkuart_poll
 *
 *   Returns 1 if a character is available, 0 if not.
 */

int junkuart_poll(void)
{
    volatile prom_uart_reg_t	   *cntrl;
    uchar_t		    status;

    cntrl  = (volatile prom_uart_reg_t *) get_junkuart_base();
    status = RD_REG(cntrl, LINE_STATUS_REG);

    if (status & (LSR_OVERRUN_ERR | LSR_PARITY_ERR | LSR_FRAMING_ERR)) {
	if (status & LSR_OVERRUN_ERR)
	    junkuart_puts("\nNS16550 overrun error\n");
	else if (status & LSR_FRAMING_ERR)
	    junkuart_puts("\nNS16550 framing error\n");
	else
	    junkuart_puts("\nNS16550 parity error\n");

	return 0;
    }

    return ((status & LSR_DATA_READY) != 0);
}

/*
 * junkuart_readc
 *
 *   Reads a character.  May only be called after junkuart_poll has
 *   indicated a character is available.
 */

int junkuart_readc(void)
{
    volatile prom_uart_reg_t	   *cntrl;

    cntrl  = (volatile prom_uart_reg_t *) get_junkuart_base();

    return RD_REG(cntrl, RCV_BUF_REG);
}

/*
 * junkuart_getc
 *
 *   Waits for a character and returns it.
 *   Flashes hub LEDs while waiting.
 */

#define FLASH_CYCLES	2000

int junkuart_getc(void)
{
    int		f, i, c;
    uchar_t		    status;

    for (f = 0xc0; ; f ^= JUNKUART_FLASH) {
	for (i = 0; i < FLASH_CYCLES; i++)
	    if (junkuart_poll()){
		c = junkuart_readc();
		return	c;
	    }
	hub_led_set(f);
    }

    /*NOTREACHED*/

    return 0;
}

/*
 * junkuart_putc
 *
 *   Writes a single character when ready.
 *   Returns 0 if successful, -1 if timed out.
 *
 *   Converts \n to \r\n (can add 256 to quote a plain \n).
 */

int junkuart_putc(int c)
{
    volatile prom_uart_reg_t	   *cntrl;
    rtc_time_t		    expire;

    if (c > 255)
	c -= 256;
    else if (c == '\n' && junkuart_putc('\r') < 0)
	return -1;

    cntrl  = (volatile prom_uart_reg_t *) get_junkuart_base();

    expire = rtc_time() + PUTC_TIMEOUT;

    while (! (RD_REG(cntrl, LINE_STATUS_REG) & LSR_XMIT_BUF_EMPTY))
	if (rtc_time() > expire) {
	    return -1;
	}

    WR_REG(cntrl, XMIT_BUF_REG, c);

    return 0;
}

/*
 * junkuart_puts
 *
 *   Writes a null-terminated string.
 *   Returns 0 if successful, -1 if junk_putc failed.
 *   Contains special support so the string may reside in the PROM.
 */

int junkuart_puts(char *s)
{
    int			c;

    if (s == 0)
	s = "<NULL>";

    while ((c = LBYTE(s)) != 0) {
	if (junkuart_putc(c) < 0)
	    return -1;
	s++;
    }

    return 0;
}

/*
 * junkuart_gets
 *
 *   Mini version of gets for debugging.
 */

char *junkuart_gets(char *buf, int max_length)
{
    int			c;
    char	       *bufp;

    bufp = buf;

    while ((c = junkuart_getc()) != '\r') {
	junkuart_putc(c);
	*bufp++ = c;
    }

    junkuart_putc(c);
    junkuart_putc('\n');

    *bufp = 0;

    return buf;
}

/*
 * junkuart_printf
 *
 *   Bonus routine useful for debugging.
 */

void junkuart_printf(const char *fmt, ...)
{
    va_list	ap;
    char	buf[80];
    extern int	vsprintf(char *buf, const char *fmt, va_list ap);

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    junkuart_puts(buf);
}
