/***********************************************************************\
*	File:		netuart.c					*
*									*
*	Pseudo UART routines that operate using SN0net vectors.	*
*	The POD command "talk" accesses this node's NI_SCRATCH_REG1	*
*	to perform the communication protocol.  This node doesn't	*
*	have to know anything.						*
*									*
\***********************************************************************/

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/cpu.h>

#include "ip27prom.h"
#include "hub.h"

#include <libkl.h>
#include <rtc.h>

#include "netuart.h"

#define TX(nasid, slice) \
	TO_NODE((nasid), (IP27PROM_NETUART + 0 + (slice) * 4))

#define RX(nasid, slice) \
	TO_NODE((nasid), (IP27PROM_NETUART + 8 + (slice) * 4))

#define TX_INT(slice)	((IP27PROM_INT_NETUART + 0) + (slice))
#define RX_INT(slice)	((IP27PROM_INT_NETUART + 2) + (slice))

#define EN_INT(slice)	((IP27PROM_INT_NETUART + 4) + slice)

#define TIMEOUT		100000

void netuart_init(void *init_data)
{
    int			slice = hub_cpu_get();

    hub_int_clear(TX_INT(slice));
    hub_int_clear(RX_INT(slice));
}

int netuart_poll(void)
{
    int			slice = hub_cpu_get();

    return hub_int_test(EN_INT(slice)) && hub_int_test(RX_INT(slice));
}

int netuart_readc(void)
{
    int			slice = hub_cpu_get();
    int			c;

    c = LW(RX(get_nasid(), slice));

    hub_int_clear(RX_INT(slice));

    return c;
}

#define FLASH_CYCLES	2000

int netuart_getc(void)
{
    int		f, i;

    for (f = 0xc0; ; f ^= NETUART_FLASH) {
	for (i = 0; i < FLASH_CYCLES; i++)
	    if (netuart_poll())
		return netuart_readc();

	hub_led_set(f);
    }

    /*NOTREACHED*/

    return 0;
}

int netuart_putc(int c)
{
    int			slice = hub_cpu_get();
    rtc_time_t		expire;

    if (! hub_int_test(EN_INT(slice)))
	return 0;

    if (c > 255)
	c -= 256;
    else if (c == '\n' && netuart_putc('\r') < 0)
	return -1;

    expire = rtc_time() + TIMEOUT;

    while (hub_int_test(TX_INT(slice)) && rtc_time() < expire)
	;

    SW(TX(get_nasid(), slice), c);

    hub_int_set(TX_INT(slice));

    return 0;
}

int netuart_puts(char *s)
{
    int		c;

    if (s == 0)
	s = "<NULL>";

    while ((c = LBYTE(s)) != 0) {
	if (netuart_putc(c) < 0)
	    return -1;
	s++;
    }

    return 0;
}

void netuart_enable(nasid_t nasid, int slice, int enable)
{
    if (enable) {
	hub_int_set_remote(nasid, EN_INT(slice));
	/* Discard spurious output */
	hub_int_clear_remote(nasid, TX_INT(slice));
    } else
	hub_int_clear_remote(nasid, EN_INT(slice));
}

int netuart_recv(nasid_t nasid, int slice)
{
    int			c;

    if (hub_int_test_remote(nasid, TX_INT(slice))) {
	c = LW(TX(nasid, slice));
	hub_int_clear_remote(nasid, TX_INT(slice));
    } else
	c = -1;

    return c;
}

int netuart_send(nasid_t nasid, int slice, int c)
{
    rtc_time_t		expire;

    expire = rtc_time() + TIMEOUT;

    while (hub_int_test_remote(nasid, RX_INT(slice)))
	if (rtc_time() > expire)
	    break;

    SW(RX(nasid, slice), c);

    hub_int_set_remote(nasid, RX_INT(slice));

    return 0;
}
