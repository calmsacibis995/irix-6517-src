/***********************************************************************\
*       File:           i2c.c                                           *
*                                                                       *
*       This module is a driver for the PCF8584 chip attached to        *
*       the SN0 Hub.  The PCF8584 does communication on the Philips    *
*       I2C (inter-IC communication) bus connecting all Hubs in a SN0  *
*       module as well as the module's entry-level system controller.   *
*                                                                       *
*       This is a limited driver and does not support multi-master      *
*       or slave mode.  It used to, but after we couldn't get I2C to    *
*       to work for the elaborate ideal functionality, we stripped      *
*       it down for simplicity.  We do have multiple masters, but	*
*	the system controller acts as a "super-master" and doles out	*
*	time slices on the bus via a special protocol.			*
*                                                                       *
*       NOTE: No global variables so that this module can be called     *
*       when running out of the PROM before RAM is available.           *
*                                                                       *
*       Related documents (if you can call them that):                  *
*                                                                       *
*       * The Philips I2C bus                                           *
*       * Application Hints for the PCD8584                             *
*       * Application Report PCF8584 I2C-Bus Controller                 *
*       * Philips PCD8584 I2C-bus controller datasheet                  *
*                                                                       *
\***********************************************************************/

/***********************************************************************\
*                                                                       *
* NOTICE!!                                                              *
*                                                                       *
* This file exists both in stand/arcs/lib/libkl/ml and in               *
* irix/kern/io. Please make any changes to both files                   *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/SN/agent.h>
#include <sys/SN/arch.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/SN0/ip27config.h>

#define LD(x)			(*(volatile __uint64_t *)(x))
#define SD(x, v)		(LD(x) = (__uint64_t) (v))

#define SLEEP_ENABLE		0

#ifndef _STANDALONE
#define LOCAL_HUB		LOCAL_HUB_ADDR
#endif /* _STANDALONE */

#if !defined(_STANDALONE) && SLEEP_ENABLE

#define SLEEP_TIME		30
#define SLEEP_DEFS		rtc_time_t sleep_max; int s;
#define SLEEP_INIT		sleep_max = rtc_time() + SLEEP_TIME
#define SLEEP_CHECK		if (rtc_time() > sleep_max) {		\
					go_to_sleep();			\
					sleep_max = rtc_time() + 30;	\
				}

#define RAISE_SPL		s = spl7()
#define LOWER_SPL		splx(s)

#else

#define SLEEP_DEFS
#define SLEEP_INIT
#define SLEEP_CHECK

#define RAISE_SPL
#define LOWER_SPL

#endif

#define I2C_GEN_CALL		1

#define I2C_SLOT		(SLOTNUM_GETSLOT(get_node_slotid(nasid)) - 1)
#define I2C_MY_ADDR		(0x09 + I2C_SLOT)
#define I2C_ARB_ADDR		0x70
#define I2C_MSG_INDICATION	8
#define I2C_SIGNAL_TIME		250	/* usec */
#define I2C_INVALID_ADDR	0x7f

#if _STANDALONE
#define I2C_CPU_CODE		(I2C_SLOT * 2 + \
				 LD(REMOTE_HUB(nasid, PI_CPU_NUM)))
#else
#define I2C_CPU_CODE		0
#endif

#define I2C_DEBUG_UART_IN	0	/* Wait for RETURN key on junkuart */
#define I2C_DEBUG_UART_OUT	0	/* Print on junkuart               */
#define I2C_DEBUG_UART		(I2C_DEBUG_UART_IN || I2C_DEBUG_UART_OUT)
#define I2C_DEBUG_TRACE		0	/* Record trace in memory at $f31  */
#define I2C_DEBUG		(I2C_DEBUG_UART || I2C_DEBUG_TRACE)
#define I2C_TIMEOUT		1	/* Enable timeouts                 */

/*
 * PCF8584 register definitions
 *
 * Access conditions		Name	Purpose
 * ----------------------	----	-----------------------------
 * ESO=0 A0=1 ES1=X ES2=X	S1	Control register (W/O)
 * ESO=1 A0=1 ES1=X ES2=X	S1	Control/status register (R/W)
 * ESO=1 A0=0 ES1=X ES2=0	S0	Data register (R/W)
 * ESO=0 A0=0 ES1=0 ES2=0	S0'	Own address (R/W)
 * ESO=0 A0=0 ES1=1 ES2=0 	S2	Clock control (R/W)
 * ESO=0 A0=0 ES1=0 ES2=1 	S3	Interrupt vector (R/W)
 * ESO=1 A0=0 ES1=X ES2=1 	S3	Interrupt vector (R/W)
 */

/*
 * Control register write-only bits
 */

#define CTL_PIN			0x80	/* Reset function/repeated starts   */
#define CTL_ESO			0x40	/* Enable serial output             */
#define CTL_ES1			0x20	/* Select current register for A0=0 */
#define CTL_ES2			0x10	/* Select current register for A0=0 */
#define CTL_ENI			0x08	/* Enable interrupt output          */
#define CTL_STA			0x04	/* Start/stop condition generation  */
#define CTL_STO			0x02	/* Start/stop condition generation  */
#define CTL_ACK			0x01	/* Send positive ACKs (master recv) */

/*
 * Status register read-only bits
 */

#define STA_PIN			0x80	/* Pending interrupt NOT 	    */
#define STA_STS			0x20	/* Stop detected (slave recv mode)  */
#define STA_BER			0x10	/* Bus error 			    */
#define STA_LRB			0x08	/* Last received (ACK) bit 	    */
#define STA_AD0			0x08	/* General call 		    */
#define STA_AAS			0x04	/* Addressed as slave 		    */
#define STA_LAB			0x02	/* Lost arbitration 		    */
#define STA_BNB			0x01	/* Bus not busy 		    */

/*
 * Clock control register values for main clock and SCL clock frequencies
 */

#define CLK_IC_3M		0x00
#define CLK_IC_4430K		0x10
#define CLK_IC_6M		0x14
#define CLK_IC_8M		0x18
#define CLK_IC_12M		0x1c

#define CLK_SCL_90K		0x00
#define CLK_SCL_45K		0x01
#define CLK_SCL_11K		0x02
#define CLK_SCL_1500		0x03

#include "ksys/i2c.h"

#if I2C_DEBUG_UART
#include <stdlib.h>
#endif

#if _STANDALONE
# include "junkuart.h"		/* For debugging */
# define PRINTF			if (I2C_DEBUG_UART_OUT) junkuart_printf
#else
# include <sys/SN/addrs.h>
# include "sys/systm.h"
# define PRINTF			if (I2C_DEBUG_UART_OUT) printf
#endif

/*
 * Reads or writes with A0=1 access the CSR, which contains bits to select
 * the current PCF8584 register.  Reads or writes with A0=0 access the
 * currently selected PCF8584 register.  This module will leave the Data
 * Register selected after the chip is initialized.
 */

#if I2C_DEBUG_TRACE
extern __uint64_t get_cop1(int);
extern void set_cop1(int, __uint64_t);
#define SH(addr, value)		(*(volatile unsigned short *) (addr) = (value))
#endif

#if I2C_DEBUG

static __uint64_t debug_read(int reg)
{
    __uint64_t	value;
#if I2C_DEBUG_UART_IN
    char	buf[16];
#endif
#if I2C_DEBUG_TRACE
    __uint64_t	ptr;
#endif

#if I2C_DEBUG_UART_IN
    junkuart_printf("READ A%d pending: ", reg);
    junkuart_gets(buf, sizeof (buf));
#endif

    value = (u_char) LD(LOCAL_HUB(KL_I2C_REG) + reg);

#if I2C_DEBUG_UART_OUT
    junkuart_printf("READ A%d got 0x%02x: ", reg, value);
#endif

#if I2C_DEBUG_UART_IN
    junkuart_gets(buf, sizeof (buf));

    if (buf[0] >= '0' && buf[0] <= '9')
	value = strtoull(buf, 0, 0);
#endif

#if I2C_DEBUG_TRACE
    ptr = get_cop1(31);
    if (ptr >> 56 == 0x96) {
	SH(ptr, reg << 8 | value);
	ptr += 2;
	set_cop1(31, ptr);
	SH(ptr, 0xffff);
    }
#endif

    return value;
}

static __uint64_t debug_write(int reg, __uint64_t value)
{
#if I2C_DEBUG_UART_IN
    char	buf[16];
#endif
#if I2C_DEBUG_TRACE
    __uint64_t	ptr;
#endif

#if I2C_DEBUG_UART_OUT
    junkuart_printf("WRITE S%d with 0x%02x: ", reg, value);
#endif

#if I2C_DEBUG_UART_IN
    junkuart_gets(buf, sizeof (buf));

    if (buf[0] >= '0' && buf[0] <= '9')
	value = strtoull(buf, 0, 0);
#endif

#if I2C_DEBUG_TRACE
    ptr = get_cop1(31);
    if (ptr >> 56 == 0x96) {
	SH(ptr, (reg + 2) << 8 | value);
	ptr += 2;
	set_cop1(31, ptr);
	SH(ptr, 0xffff);
    }
#endif

    SD(LOCAL_HUB(KL_I2C_REG) + reg, value);

    return value;
}

#define READ_A0()		debug_read(0)
#define READ_A1()		debug_read(1)

#define WRITE_A0(v)		debug_write(0, v)
#define WRITE_A1(v)		debug_write(1, v)

#else /* I2C_DEBUG */

#define READ_A0()	((u_char) \
			 REMOTE_HUB_L(nasid, KL_I2C_REG))

#define READ_A1()	((u_char) \
			 REMOTE_HUB_L(nasid, (KL_I2C_REG + 8)))

#define WRITE_A0(v)	REMOTE_HUB_S(nasid, KL_I2C_REG, v)

#define WRITE_A1(v)	REMOTE_HUB_S(nasid, (KL_I2C_REG + 8), v)

#endif /* I2C_DEBUG */

/*
 * i2c_init
 *
 *   Execute PCF8584 initialization sequence
 *
 *   This driver expects to get to the PCF8584 data/shift register by
 *   using double-word reads and writes to the base address, and to
 *   get to the status/command register by using double-word reads/writes
 *   to the base address plus 8.
 *
 *   This routine puts the stupid chip into monitor mode.
 *
 *   Returns:
 *	0 on success
 *	A negative error code on failure
 */

int i2c_init(nasid_t nasid)
{
    int			r = 0;

#if I2C_DEBUG
    printf("i2c_init: entered\n");
#endif

    /* Reset and write Own Address as 0x80 (temporarily) */
    WRITE_A1(CTL_PIN);
    WRITE_A0(0x7f);

    /* Set clock control */
    WRITE_A1(CTL_PIN | CTL_ES1);
    WRITE_A0(CLK_IC_12M | CLK_SCL_90K);

    /* Go back and read Own Address to verify chip is responsive */
    WRITE_A1(CTL_PIN);
    if (READ_A0() != 0x7f) {
        r = I2C_ERROR_INIT;
        goto done;
    }

    /* Write our true address */
    WRITE_A1(CTL_PIN);
    WRITE_A0(I2C_MY_ADDR);

    /* Turn on the I2C bus interface -- also set the PIN bit */
    WRITE_A1(CTL_PIN | CTL_ESO);

#if I2C_DEBUG
    printf("i2c_init: re-initialization done\n");
#endif

done:

#if I2C_DEBUG
    printf("i2c_init: return %d\n", r);
#endif
    return r;
}

/*
 * i2c_probe
 *
 *   Checks to see if the ELSC is sending tokens.  Can also be used to
 *   wait for the ELSC to start sending tokens.  Returns 1 if so, 0 if not.
 */

int i2c_probe(nasid_t nasid, rtc_time_t timeout)
{
    rtc_time_t		expiry;
    __uint64_t		token;

    expiry = rtc_time() + timeout;

    token = READ_A0();

    while (rtc_time() < expiry)
	if (READ_A0() != token)
	    return 1;

    return 0;
}

/*
 * reset (INTERNAL)
 *
 *   Attempt to reset chip on error.
 */

/*REFERENCED*/
static void reset(nasid_t nasid, int fail)
{
    if (fail < 0)
	WRITE_A1(CTL_PIN);

    WRITE_A1(CTL_PIN | CTL_ESO);
}

/*
 * i2c_arb
 *
 *   Monitors the bus until the system controller indicates it's okay
 *   to become the bus master for a time slice.  Once this routine
 *   returns successfully, the caller should immediately begin an I2C
 *   operation.  The I2C operation may take as long as required since
 *   the system controller will hold off as long as the bus is busy.
 *
 *   Returns:
 *	0 if arbitrated and no message is waiting in the system controller
 *	1 if arbitrated and a message is waiting in the system controller
 *	Negative error code on failure
 */

int i2c_arb(nasid_t nasid, rtc_time_t timeout, rtc_time_t *token_start)
{
    int			r;
    rtc_time_t		expiry;
    rtc_time_t		xtime, now;
    __uint64_t		sig1, sig2, sig, data;

#if I2C_TIMEOUT
    expiry = rtc_time() + timeout;
#else
    expiry = RTC_TIME_MAX;
#endif

    /* In the 12 p 4io config prevent the nodes in slots 5 & 6
     * from talking to the elsc over i2c.
     */
    if (!node_can_talk_to_elsc()) {
	return I2C_ERROR_NO_ELSC;
    }

    sig1 = I2C_ARB_ADDR * 2 + I2C_CPU_CODE;
    sig2 = I2C_ARB_ADDR * 2 + I2C_CPU_CODE + I2C_MSG_INDICATION;

    /*
     * If signal is already on, have to wait until it goes away
     * because we missed the time slice.
     */

    while (1) {
	if (READ_A1() & STA_BNB) {
	    data = READ_A0();

	    if (data != sig1 && data != sig2)
		break;
	}

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_ARB;
	    goto done;
	}
    }

    /*
     * Wait for signal to occur for at least I2C_SIGNAL_TIME microseconds.
     */

 nope:

    if (token_start)
	*token_start = rtc_time();

    while (1) {
	if (READ_A1() & STA_BNB) {
	    data = READ_A0();

	    if (data == sig1) {
		sig = sig1;
		break;
	    }

	    if (data == sig2) {
		sig = sig2;
		break;
	    }
	}

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_ARB;
	    goto done;
	}
    }

    xtime = rtc_time() + I2C_SIGNAL_TIME;

    while (1) {
	if ((READ_A1() & STA_BNB) == 0)
	    goto nope;

	data = READ_A0();

	if (data != sig)
	    goto nope;

	now = rtc_time();

	if (now > expiry) {
	    r = I2C_ERROR_TO_ARB;
	    goto done;
	}

	if (now > xtime)
	    break;
    }

    /*
     * Success
     */

    r = (sig & 8) ? 1 : 0;

 done:

    return r;
}

/*
 * i2c_master_xmit
 *
 *   Becomes I2C bus master and transmit data (without arbitration).
 *   Fails on lost arbitration, bus error, or addressed as slave.
 *
 *   After transmitting, this routine does a repeated start to an invalid
 *   I2C address to put a known fixed value into the data registers of
 *   all PCF8584 chips on the bus (system controller arbitration hack).
 *
 *   addr: slave device to be written
 *   buf: array of bytes to be written
 *   len_max: maximum number of bytes to be written
 *   len_ptr: if successful, returns number of bytes accepted
 *   timeout: timeout of complete operation in microseconds
 *   only_if_message: do transaction only if a message is waiting
 *
 *   Returns:
 *	0 on success
 *	Negative error code on failure
 */

int i2c_master_xmit(nasid_t nasid,
		    i2c_addr_t addr,
		    u_char *buf,
		    int len_max,
		    int *len_ptr,
		    rtc_time_t timeout,
		    int only_if_message)
{
    rtc_time_t		expiry;
    SLEEP_DEFS
    __uint64_t		status;
    int			i, r;

    PRINTF("i2c_master_xmit: entered, addr=0x%02x\n", addr);

#if I2C_TIMEOUT
    expiry = rtc_time() + timeout;
#else
    expiry = RTC_TIME_MAX;
#endif

    *len_ptr = 0;

    RAISE_SPL;

#if I2C_GEN_CALL
    /*
     * WARNING: Don't put anything between the bus arbitrate and the
     * transmit, as then it'll miss its window when running in DEX
     * mode (unbelievably slow).
     */

    if ((r = i2c_arb(nasid, timeout, 0)) < 0) {
	LOWER_SPL;
	return r;
    }
#endif

    if (only_if_message && r == 0) {
	LOWER_SPL;
	r = I2C_ERROR_NO_MESSAGE;
	goto done;
    }

    status = READ_A1();

    if (status & (STA_BER | STA_AAS | STA_LAB)) {
	LOWER_SPL;
	r = I2C_ERROR_STATE;
	goto done;
    }

    /*
     * Write slave address/direction and generate a start condition.
     * Wait for the address to be sent or an error condition to occur.
     */

    WRITE_A0(addr << 1 | 0);
    WRITE_A1(CTL_PIN | CTL_ESO | CTL_STA | CTL_ACK);

    LOWER_SPL;
    SLEEP_INIT;

    while (1) {
	status = READ_A1();

	if (status & (STA_BER | STA_AAS | STA_LAB)) {
	    r = I2C_ERROR_STATE;
	    goto stop;
	}

	if (~status & STA_PIN)
	    break;

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_SENDA;
	    goto stop;
	}

	SLEEP_CHECK;
    }

    if (status & STA_LRB) {
	r = I2C_ERROR_NAK;
	goto stop;
    }

    for (i = 0; i < len_max; i++) {
	/*
	 * Send data bytes until all of them have been sent, or
	 * the slave NAKs a byte, or until an error.
	 */

	WRITE_A0(buf[i]);

	SLEEP_INIT;

	while (1) {
	    status = READ_A1();

	    if (status & (STA_BER | STA_AAS | STA_LAB)) {
		r = I2C_ERROR_STATE;
		goto stop;
	    }

	    if (~status & STA_PIN)
		break;

	    if (rtc_time() > expiry) {
		r = I2C_ERROR_TO_SENDD;
		goto stop;
	    }

	    SLEEP_CHECK;
	}

	(*len_ptr)++;

	if (status & STA_LRB)
	    break;
    }

    /*
     * Send a repeated start to an invalid address to "clear" the bus.
     * A NAK is expected (see comment at top of routine).
     */

    WRITE_A1(CTL_ESO | CTL_STA | CTL_ACK);
    WRITE_A0(I2C_INVALID_ADDR << 1);

    r = 0;

    SLEEP_INIT;

    while (1) {
	status = READ_A1();

	if (status & (STA_BER | STA_AAS | STA_LAB)) {
	    r = I2C_ERROR_STATE;
	    goto stop;
	}

	if (~status & STA_PIN)
	    break;

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_SENDA;
	    goto stop;
	}

	SLEEP_CHECK;
    }

 stop:

    /* Write a stop condition */

    WRITE_A1(CTL_PIN | CTL_ESO | CTL_STO);

 done:

#if 0
    /* Revert to default operating condition */

    reset(r);
#endif

    PRINTF("i2c_master_xmit: return %d\n", r);

    return r;
}


/*
 * i2c_master_recv
 *
 *   Becomes I2C bus master and receive data (without arbitration).
 *   Fails on lost arbitration, bus error, or addressed as slave.
 *
 *   After receiving, this routine does a repeated start to an invalid
 *   I2C address to put a known fixed value into the data registers of
 *   all PCF8584 chips on the bus (system controller arbitration hack).
 *
 *   addr: slave device to be read
 *   buf: array to hold bytes that are read
 *   len_max: maximum number of bytes to read, must be positive
 *   len_ptr: if successful, returns number of bytes received
 *   emblen: If not -1, indicates the receive packet length will
 *	     be embedded as the first byte in the packet.  The
 *	     value of emblen is added to the first byte to determine
 *	     the expected full byte count (and so when to NAK).
 *   timeout: timeout of complete operation in microseconds
 *   only_if_message: do transaction only if a message is waiting
 *
 *   Returns:
 *	0 on success
 *	Negative error code on failure
 */

int i2c_master_recv(nasid_t nasid,
		    i2c_addr_t addr,
		    u_char *buf,
		    int len_max,
		    int *len_ptr,
		    int emblen,
		    rtc_time_t timeout,
		    int only_if_message)
{
    rtc_time_t		expiry;
    SLEEP_DEFS
    __uint64_t		status;
    int			i, r;

    PRINTF("i2c_master_recv: entered, addr=0x%02x\n", addr);

#if I2C_TIMEOUT
    expiry = rtc_time() + timeout;
#else
    expiry = RTC_TIME_MAX;
#endif

    *len_ptr = 0;

    RAISE_SPL;

#if I2C_GEN_CALL
    /*
     * WARNING: Don't put anything between the bus arbitrate and the
     * receive, as then it'll miss its window when running in DEX
     * mode (unbelievably slow).
     */

    if ((r = i2c_arb(nasid, timeout, 0)) < 0) {
	LOWER_SPL;
	return r;
    }

    if (only_if_message && r == 0) {
	LOWER_SPL;
	r = I2C_ERROR_NO_MESSAGE;
	goto done;
    }
#endif

    status = READ_A1();

    if (status & (STA_BER | STA_AAS | STA_LAB)) {
	LOWER_SPL;
	r = I2C_ERROR_STATE;
	goto done;
    }

    /*
     * Write slave address/direction and generate a start condition.
     * Wait for the address to be sent or an error condition to occur.
     */

    WRITE_A0(addr << 1 | 1);
    WRITE_A1(CTL_PIN | CTL_ESO | CTL_STA | CTL_ACK);

    LOWER_SPL;
    SLEEP_INIT;

    while (1) {
	status = READ_A1();

	if (status & (STA_BER | STA_AAS | STA_LAB)) {
	    r = I2C_ERROR_STATE;
	    goto stop;
	}

	if (~status & STA_PIN)
	    break;

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_SENDA;
	    goto stop;
	}

	SLEEP_CHECK;
    }

    if (status & STA_LRB) {
	r = I2C_ERROR_NAK;
	goto stop;
    }

    /*
     * If reading only one byte, set up for NAKing it.
     */

    if (len_max == 1)
	WRITE_A1(CTL_ESO);

    /*
     * Read and discard the slave address.
     */

    READ_A0();

    for (i = 0; i < len_max; i++) {
	SLEEP_INIT;

	while (1) {
	    status = READ_A1();

	    if (status & (STA_BER | STA_AAS | STA_LAB)) {
		r = I2C_ERROR_STATE;
		goto stop;
	    }

	    if (~status & STA_PIN)
		break;

	    if (rtc_time() > expiry) {
		r = I2C_ERROR_TO_RECVD;
		goto stop;
	    }

	    SLEEP_CHECK;
	}

	/*
	 * If the next byte is the last, set up for NAKing it.
	 */

	if (i == len_max - 2)
	    WRITE_A1(CTL_ESO);

	buf[i] = (u_char) READ_A0();

	/*
	 * If the length is sent as the first byte in the packet,
	 * adjust it by emblen to determine new max. byte count.
	 */

	if (i == 0 && emblen >= 0)
	    len_max = buf[0] + emblen;

	(*len_ptr)++;
    }

    /*
     * Send a repeated start to an invalid address to "clear" the bus.
     * A NAK is expected (see comment at top of routine).
     */

    WRITE_A1(CTL_ESO | CTL_STA | CTL_ACK);
    WRITE_A0(I2C_INVALID_ADDR << 1);

    r = 0;

    SLEEP_INIT;

    while (1) {
	status = READ_A1();

	if (status & (STA_BER | STA_AAS | STA_LAB)) {
	    r = I2C_ERROR_STATE;
	    goto stop;
	}

	if (~status & STA_PIN)
	    break;

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_SENDA;
	    goto stop;
	}

	SLEEP_CHECK;
    }

 stop:

    /* Write a stop condition */

    WRITE_A1(CTL_PIN | CTL_ESO | CTL_STO);

 done:

#if 0
    /* Revert to default operating condition */

    reset(r);
#endif

    PRINTF("i2c_master_recv: return %d\n", r);

    return r;
}


/*
 * i2c_master_xmit_recv
 *
 *   Becomes I2C bus master and transmits data (without arbitration).
 *   Uses a repeat start to switch direction, and receive response data.
 *   Fails on lost arbitration, bus error, or addressed as slave.
 *
 *   After receiving, this routine does a SECOND repeated start to an
 *   invalid I2C address to put a known fixed value into the data
 *   registers of all PCF8584 chips on the bus (system controller
 *   arbitration hack).
 *
 *   addr: slave device to be written/read
 *   xbuf: array of bytes to be written
 *   xlen_max: maximum number of bytes to be written
 *   xlen_ptr: if successful, returns number of bytes accepted
 *   rbuf: array to hold bytes that are read
 *   rlen_max: maximum number of bytes to read, must be positive
 *   rlen_ptr: if successful, returns number of bytes received
 *   emblen: If not -1, indicates the receive packet length will
 *	     be embedded as the first byte in the packet.  The
 *	     value of emblen is added to the first byte to determine
 *	     the expected full byte count (and so when to NAK).
 *   timeout: timeout of complete operation in microseconds
 *   only_if_message: do transaction only if a message is waiting
 *
 *   Returns:
 *	0 on success
 * Negative error code on failure
 */

int i2c_master_xmit_recv(nasid_t nasid,
			 i2c_addr_t addr,
			 u_char *xbuf,
			 int xlen_max,
			 int *xlen_ptr,
			 u_char *rbuf,
			 int rlen_max,
			 int *rlen_ptr,
			 int emblen,
			 rtc_time_t timeout,
			 int only_if_message)
{
    rtc_time_t		expiry;
    SLEEP_DEFS
    __uint64_t		status;
    int			i, r;

    PRINTF("i2c_master_xmit_recv: entered, addr=0x%02x\n", addr);

#if I2C_TIMEOUT
    expiry = rtc_time() + timeout;
#else
    expiry = RTC_TIME_MAX;
#endif

    *xlen_ptr = 0;
    *rlen_ptr = 0;

    RAISE_SPL;

#if I2C_GEN_CALL
    /*
     * WARNING: Don't put anything between the bus arbitrate and the
     * transmit/receive, as then it'll miss its window when running in
     * DEX mode (unbelievably slow).
     */

    if ((r = i2c_arb(nasid, timeout, 0)) < 0) {
	LOWER_SPL;
	return r;
    }

    if (only_if_message && r == 0) {
	LOWER_SPL;
	r = I2C_ERROR_NO_MESSAGE;
	goto done;
    }
#endif

    status = READ_A1();

    if (status & (STA_BER | STA_AAS | STA_LAB)) {
	LOWER_SPL;
	r = I2C_ERROR_STATE;
	goto done;
    }

    /*
     * Write slave address/direction and generate a start condition.
     * Wait for the address to be sent or an error condition to occur.
     */

    WRITE_A0(addr << 1 | 0);
    WRITE_A1(CTL_PIN | CTL_ESO | CTL_STA | CTL_ACK);

    LOWER_SPL;
    SLEEP_INIT;

    while (1) {
	status = READ_A1();

	if (status & (STA_BER | STA_AAS | STA_LAB)) {
	    r = I2C_ERROR_STATE;
	    goto stop;
	}

	if (~status & STA_PIN)
	    break;

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_SENDA;
	    goto stop;
	}

	SLEEP_CHECK;
    }

    if (status & STA_LRB) {
	r = I2C_ERROR_NAK;
	goto stop;
    }

    for (i = 0; i < xlen_max; i++) {
	/*
	 * Send data bytes until all of them have been sent, or
	 * the slave NAKs a byte, or until an error.
	 */

	WRITE_A0(xbuf[i]);

	SLEEP_INIT;

	while (1) {
	    status = READ_A1();

	    if (status & (STA_BER | STA_AAS | STA_LAB)) {
		r = I2C_ERROR_STATE;
		goto stop;
	    }

	    if (~status & STA_PIN)
		break;

	    if (rtc_time() > expiry) {
		r = I2C_ERROR_TO_SENDD;
		goto stop;
	    }

	    SLEEP_CHECK;
	}

	(*xlen_ptr)++;

	if (status & STA_LRB)
	    break;
    }

    /*
     * Send a repeated start, followed by the slave address/direction.
     */

    WRITE_A1(CTL_ESO | CTL_STA | CTL_ACK);
    WRITE_A0(addr << 1 | 1);

    r = 0;

    SLEEP_INIT;

    while (1) {
	status = READ_A1();

	if (status & (STA_BER | STA_AAS | STA_LAB)) {
	    r = I2C_ERROR_STATE;
	    goto stop;
	}

	if (~status & STA_PIN)
	    break;

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_SENDA;
	    goto stop;
	}

	SLEEP_CHECK;
    }

    if (status & STA_LRB) {
	r = I2C_ERROR_NAK;
	goto stop;
    }

    /*
     * If reading only one byte, set up for NAKing it.
     */

    if (rlen_max == 1)
	WRITE_A1(CTL_ESO);

    /*
     * Read and discard the slave address.
     */

    READ_A0();

    for (i = 0; i < rlen_max; i++) {
	SLEEP_INIT;

	while (1) {
	    status = READ_A1();

	    if (status & (STA_BER | STA_AAS | STA_LAB)) {
		r = I2C_ERROR_STATE;
		goto stop;
	    }

	    if (~status & STA_PIN)
		break;

	    if (rtc_time() > expiry) {
		r = I2C_ERROR_TO_RECVD;
		goto stop;
	    }

	    SLEEP_CHECK;
	}

	/*
	 * If the next byte is the last, set up for NAKing it.
	 */

	if (i == rlen_max - 2)
	    WRITE_A1(CTL_ESO);

	rbuf[i] = (u_char) READ_A0();

	/*
	 * If the length is sent as the first byte in the packet,
	 * adjust it by emblen to determine new max. byte count.
	 */

	if (i == 0 && emblen >= 0)
	    rlen_max = rbuf[0] + emblen;

	(*rlen_ptr)++;
    }

    /*
     * Send a repeated start to an invalid address to "clear" the bus.
     * A NAK is expected (see comment at top of routine).
     */

    WRITE_A1(CTL_ESO | CTL_STA | CTL_ACK);
    WRITE_A0(I2C_INVALID_ADDR << 1);

    r = 0;

    SLEEP_INIT;

    while (1) {
	status = READ_A1();

	if (status & (STA_BER | STA_AAS | STA_LAB)) {
	    r = I2C_ERROR_STATE;
	    goto stop;
	}

	if (~status & STA_PIN)
	    break;

	if (rtc_time() > expiry) {
	    r = I2C_ERROR_TO_SENDA;
	    goto stop;
	}

	SLEEP_CHECK;
    }

 stop:

    /* Write a stop condition */

    WRITE_A1(CTL_PIN | CTL_ESO | CTL_STO);

 done:

#if 0
    /* Revert to default operating condition */

    reset(r);
#endif

    PRINTF("i2c_master_xmit_recv: return %d\n", r);

    return r;
}

/*
 * i2c_errmsg
 *
 *   Given a negative error code,
 *   returns a corresponding static error string.
 */

char *i2c_errmsg(int code)
{
    switch (code) {
    case I2C_ERROR_NONE:
	return "No error";
    case I2C_ERROR_INIT:
	return "PCF8584 initialization error";
    case I2C_ERROR_STATE:
	return "Unexpected chip state";
    case I2C_ERROR_NAK:
	return "Addressed slave not responding";
    case I2C_ERROR_TO_ARB:
	return "Timeout waiting for arbitration signal";
    case I2C_ERROR_TO_BUSY:
	return "Timeout on busy bus";
    case I2C_ERROR_TO_SENDA:
	return "Timed out sending address byte";
    case I2C_ERROR_TO_SENDD:
	return "Timed out sending data byte";
    case I2C_ERROR_TO_RECVA:
	return "Timed out receiving address byte";
    case I2C_ERROR_TO_RECVD:
	return "Timed out receiving data byte";
    case I2C_ERROR_NO_MESSAGE:
	return "No message waiting";
    default:
	return "Unknown error";
    }
}
