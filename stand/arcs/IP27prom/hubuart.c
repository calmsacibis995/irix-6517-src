/***********************************************************************\
*	File:		hubuart.c					*
*									*
*	Hub UART routines.  This is not actually a UART per se.		*
*	It's a debugging port for which Sable and RTL simulators	*
*	have support.  Also, there may be support for connectivity	*
*	on the MD junk bus.						*
*									*
\***********************************************************************/

#ident "$Revision: 1.10 $"

#ifdef SABLE

#include <sys/types.h>
#include <sys/i8251uart.h>
#include <sys/SN/agent.h>

#include "ip27prom.h"

#include "hubuart.h"
#include "hub.h"

/*
 * hubuart_init
 */

#define RESET0	(I8251_RESET)
#define RESET1	(I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | I8251_STOPB2)
#define RESET2	(I8251_TXENB | I8251_RXENB | I8251_RESETERR)

/* ARGSUSED */
void hubuart_init(void *init_data)
{
    SD(KL_UART_CMD, 0);		/* Clear state by writing 3 zeros */
    SD(KL_UART_CMD, 0);
    SD(KL_UART_CMD, 0);
    SD(KL_UART_CMD, RESET0);	/* Soft reset and configure ASYNC mode */
    SD(KL_UART_CMD, RESET1);
    SD(KL_UART_CMD, RESET2);
}

/*
 * hubuart_poll
 *
 *   Returns 1 if a character is available, 0 if not.
 */

int hubuart_poll(void)
{
    return ((LD(KL_UART_CMD) & I8251_RXRDY) != 0);
}

/*
 * hubuart_readc
 *
 *   Reads a character.  May only be called after hubuart_poll has
 *   indicated a character is available.
 */

int hubuart_readc(void)
{
    return (int) LD(KL_UART_DATA);
}

/*
 * hubuart_getc
 *
 *   Waits for a character and returns it.
 *   Flashes hub LEDs while waiting.
 */

#define FLASH_CYCLES	200000

int hubuart_getc(void)
{
    int		f, i;

    for (f = 0; ; f ^= HUBUART_FLASH) {
	for (i = 0; i < FLASH_CYCLES; i++)
	    if (hubuart_poll())
		return hubuart_readc();

	hub_led_set(f);
    }

    /*NOTREACHED*/
}

/*
 * hubuart_putc
 *
 *   Writes a single character when ready.
 *   Returns 0 if successful, -1 if timed out.
 *
 *   Converts \n to \r\n (can add 256 to quote a plain \n).
 */

#define PUTC_TIMEOUT	2000000

int hubuart_putc(int c)
{
    int		i;

    if (c > 255)
	c -= 256;
    else if (c == '\n' && hubuart_putc('\r') < 0)
	return -1;

    for (i = 0; i < PUTC_TIMEOUT; i++)
	if (LD(KL_UART_CMD) & I8251_TXRDY) {
	    SD(KL_UART_DATA, c);
	    return 0;
	}

    return -1;
}

/*
 * hubuart_puts
 *
 *   Writes a null-terminated string.
 *   Returns 0 if successful, -1 if hubuart_putc failed.
 *   Contains special support so the string may reside in the PROM.
 */

int hubuart_puts(char *s)
{
    int		c;

    if (s == 0)
	s = "<NULL>";

    while ((c = LBYTE(s)) != 0) {
	if (hubuart_putc(c) < 0)
	    return -1;
	s++;
    }

    return 0;
}

#endif /* SABLE */
