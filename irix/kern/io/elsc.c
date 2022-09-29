/***********************************************************************\
*	File:		elsc.c						*
*									*
*	IP27prom System Controller Driver				*
*									*
*	This driver talks to the entry-level system controller over	*
*	the I2C bus and implements the ELSC protocol and command set.	*
*									*
*	NOTE: This module keeps all its global variables in a scratch	*
*	area of memory specified when elsc_init is called.		*
*									*
\***********************************************************************/

/***********************************************************************\
*                                                                       *
* NOTICE!!                                                              *
*                                                                       *
* This file exists both in stand/arcs/lib/libkl/io and in               *
* irix/kern/io.  Please make any changes to both files.                 *
*                                                                       *
\***********************************************************************/


#include <sys/types.h>
#include <sys/nic.h>
#include <sys/SN/addrs.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/SN0/ip27config.h>

#include <ksys/elsc.h>

#if _STANDALONE

#include <libkl.h>
#include "hub.h"
#include "libc.h"
#include "libasm.h"
#include "ip27prom.h"
#include "rtc.h"

#define I2C_TIMEOUT	1000000		/* I2C bus operation timeout (usec) */
#define ELSC_TIMEOUT	1000000		/* ELSC response timeout (usec) */
#define LOCK_TIMEOUT	5000000		/* Hub lock timeout (usec) */

#else /* _STANDALONE */

#include <sys/kmem.h>
#include <sys/pda.h>
#include <sys/ddi.h>
#include <sys/systm.h>

#include <sys/clksupport.h>

#define I2C_TIMEOUT	10000000	/* I2C bus operation timeout (usec) */
#define ELSC_TIMEOUT	10000000	/* ELSC response timeout (usec) */
#define LOCK_TIMEOUT	5000000		/* Hub lock timeout (usec) */

#define LOCAL_HUB	LOCAL_HUB_ADDR
#define LD(x)		(*(volatile __uint64_t *)(x))
#define SD(x, v)	(LD(x) = (__uint64_t) (v))

#define hub_cpu_get()	0
#define hub_slot_get()	1

#define LBYTE(caddr)	(*(char *) caddr)

/* in the kernel the serialio port lock takes care of mutex between
 * the cpus on a hub, so we don't do anything here.
 */
#define hub_lock_timeout(l, t)
#define hub_unlock(l)

#define memcpy(a,b,l) bcopy(b,a,l)

#endif /* _STANDALONE */

#define LDEBUG		0

#if LDEBUG
#include "junkuart.h"
#endif

#define I2C_ADDR_RAM	((i2c_addr_t) 0x50)
#define I2C_ADDR_UST	((i2c_addr_t) 0x20)

/*
 * ELSC data is in NVRAM page 7 at the following offsets.
 */

#define NVRAM_MAGIC_AD	0x700		/* magic number used for init */
#define NVRAM_PASS_WD	0x701		/* password (4 bytes in length) */
#define NVRAM_DBG1	0x705		/* virtual XOR debug switches */
#define NVRAM_DBG2	0x706		/* physical XOR debug switches */
#define NVRAM_CFG	0x707		/* ELSC Configuration info */
#define NVRAM_MODULE	0x708		/* system module number */
#define NVRAM_BIST_FLG	0x709		/* BIST flags (2 bits per nodeboard) */
#define NVRAM_PARTITION 0x70a		/* module's partition id */
#define	NVRAM_DOMAIN	0x70b		/* module's domain id */
#define	NVRAM_CLUSTER	0x70c		/* module's cluster id */
#define	NVRAM_CELL	0x70d		/* module's cellid */

#define NVRAM_MAGIC_NO	0x37		/* value of magic number */

#define NEXT(p)		(((p) + 1) % ELSC_QSIZE)

#define	cq_init(q)	bzero((q), sizeof (*(q)))
#define cq_empty(q)	((q)->ipos == (q)->opos)
#define cq_full(q)	(NEXT((q)->ipos) == (q)->opos)
#define cq_used(q)	((q)->opos <= (q)->ipos ?			\
			 (q)->ipos - (q)->opos :			\
			 ELSC_QSIZE + (q)->ipos - (q)->opos)
#define cq_room(q)	((q)->opos <= (q)->ipos ?			\
			 ELSC_QSIZE - 1 + (q)->opos - (q)->ipos :	\
			 (q)->opos - (q)->ipos - 1)
#define cq_add(q, c)	((q)->buf[(q)->ipos] = (u_char) (c),		\
			 (q)->ipos = NEXT((q)->ipos))
#define cq_rem(q, c)	((c) = (q)->buf[(q)->opos],			\
			 (q)->opos = NEXT((q)->opos))

/*
 * get_myid (internal)
 *
 *   Returns CPU ID from 0 to 7 (slot * 2 + PI_CPU_NUM)
 */

static int get_myid(void)
{
    int			slot;

    slot = hub_slot_get() - 1;

    return (slot * 2 + LD(LOCAL_HUB(PI_CPU_NUM)));
}

/*
 * elsc_init
 *
 *   Initialize ELSC structure
 */

void elsc_init(elsc_t *e, nasid_t nasid)
{
    bzero(e, sizeof (*e));

    e->nasid	= nasid;

    e->sel_cpu	= -1;
    e->sol	= 1;
}

/*
 * dequeue_msg
 *
 *   Internal routine to dequeue a message.
 */

static int dequeue_msg(elsc_t *e, char *msg, int msg_max)
{
    int			i, len;
    u_char		c;

    if (cq_empty(&e->imq))
	return -1;

    cq_rem(&e->imq, len);

    i = 0;

    while (len--) {
	if (cq_empty(&e->imq))
	    return -1;
	cq_rem(&e->imq, c);
	if (i < msg_max - 1)
	    msg[i++] = c;
    }

    msg[i] = 0;

#if LDEBUG
    junkuart_printf("dequeue_msg: msg=<%s>\n", msg);
#endif

    return 0;
}

/*
 * elsc_msg_check
 *
 *   This may be used only if no callback is in place.
 *
 *   Polls to see if elsc_process has queued any messages from the
 *   ELSC.  If so, it dequeus the first one, copies it into
 *   the supplied buffer, and returns 1.  If not, it returns 0.  On
 *   error, it returns a negative error code.
 */

int elsc_msg_check(elsc_t *e, char *msg, int msg_max)
{
    return (dequeue_msg(e, msg, msg_max) < 0) ? 0 : 1;
}

/*
 * elsc_msg_callback
 *
 *   Set up a callback routine for when elsc_process receives messages
 *   (other than acp) instead of queueing them.  Any messages currently
 *   queued are passed to the callback as soon as it's set.
 *
 *   Use NULL to turn off the callback and start queueing again.
 */

int elsc_msg_callback(elsc_t *e,
		      void (*callback)(void *callback_data, char *msg),
		      void *callback_data)
{
    char		msg[ELSC_PACKET_MAX];

    e->msg_callback	 = callback;
    e->msg_callback_data = callback_data;

    if (callback)
	while (dequeue_msg(e, msg, sizeof (msg)) >= 0)
	    (*callback)(callback_data, msg);

    return 0;
}

/*
 * elsc_errmsg
 *
 *   Given a negative error code,
 *   returns a corresponding static error string.
 */

char *elsc_errmsg(int code)
{
    switch (code) {
    case ELSC_ERROR_CMD_SEND:
	return "Command send error";
    case ELSC_ERROR_CMD_CHECKSUM:
	return "Command packet checksum error";
    case ELSC_ERROR_CMD_UNKNOWN:
	return "Unknown command";
    case ELSC_ERROR_CMD_ARGS:
	return "Invalid command argument(s)";
    case ELSC_ERROR_CMD_PERM:
	return "Permission denied";
    case ELSC_ERROR_RESP_TIMEOUT:
	return "System controller response timeout";
    case ELSC_ERROR_RESP_CHECKSUM:
	return "Response packet checksum error";
    case ELSC_ERROR_RESP_FORMAT:
	return "Response format error";
    case ELSC_ERROR_RESP_DIR:
	return "Response direction error";
    case ELSC_ERROR_MSG_LOST:
	return "Message lost because queue is full";
    case ELSC_ERROR_LOCK_TIMEOUT:
	return "Timed out getting ELSC lock";
    case ELSC_ERROR_DATA_SEND:
	return "Error sending data";
    case ELSC_ERROR_NIC:
	return "NIC protocol error";
    case ELSC_ERROR_NVMAGIC:
	return "Bad magic number in NVRAM";
    default:
	return i2c_errmsg(code);
    }
}

/*
 * elsc_nvram_write
 *
 *   Copies bytes from 'buf' into NVRAM, starting at NVRAM address
 *   'addr' which must be between 0 and 2047.
 *
 *   If 'len' is non-negative, the routine copies 'len' bytes.
 *
 *   If 'len' is negative, the routine treats the data as a string and
 *   copies bytes up to and including a NUL-terminating zero, but not
 *   to exceed '-len' bytes.
 */

int elsc_nvram_write(elsc_t *e, int addr, char *buf, int len)
{
    uchar_t		data[2];
    int			i, r, xlen;

    if (len < 0) {
	i = strlen(buf) + 1;
	len = (i > -len) ? -len : i;
    }

    for (i = 0; i < len; i++, addr++) {
	data[0] = (uchar_t) (addr & 0xff);
	data[1] = (uchar_t) buf[i];

	hub_lock_timeout(HUB_LOCK_I2C, LOCK_TIMEOUT);

	r = i2c_master_xmit(e->nasid, I2C_ADDR_RAM | addr >> 8 & 7,
			    data, 2, &xlen, I2C_TIMEOUT, 0);

	hub_unlock(HUB_LOCK_I2C);

	if (r < 0)
	    break;
    }

    return r;
}

/*
 * elsc_nvram_read
 *
 *   Copies bytes from NVRAM into 'buf', starting at NVRAM address
 *   'addr' which must be between 0 and 2047.
 *
 *   If 'len' is non-negative, the routine copies 'len' bytes.
 *
 *   If 'len' is negative, the routine treats the data as a string and
 *   copies bytes up to and including a NUL-terminating zero, but not
 *   to exceed '-len' bytes.
 */

int elsc_nvram_read(elsc_t *e, int addr, char *buf, int len)
{
    uchar_t		xdata[1];
    uchar_t		rdata[1];
    int			i, r, xlen, rlen;

    for (i = 0; ; addr++) {
	xdata[0] = (uchar_t) (addr & 0xff);

	hub_lock_timeout(HUB_LOCK_I2C, LOCK_TIMEOUT);

	r = i2c_master_xmit_recv(e->nasid,
				 I2C_ADDR_RAM | addr >> 8 & 7,
				 xdata, 1, &xlen,
				 rdata, 1, &rlen,
				 -1, I2C_TIMEOUT, 0);

	hub_unlock(HUB_LOCK_I2C);

	if (r < 0)
	    break;

	buf[i++] = (char) rdata[0];

	if (len < 0 && (buf[i] == 0 || i == -len) || i == len)
	    break;
    }

    return r;
}

/*
 * elsc_command
 *
 *   Generates a command packet from a command string, sends it, and
 *   receives the response.
 *
 *   The command string is taken from e->cmd and is an ordinary NUL-
 *   terminated string, as is the response which is placed in e->resp.
 *
 *   If the flag 'only_if_message' is true, the command will be executed
 *   only if a message is waiting (as indicated by the arbitration token) --
 *   otherwise it'll return I2C_ERROR_NO_MESSAGE.
 */

int elsc_command(elsc_t *e, int only_if_message)
{
    u_char		xbuf[ELSC_PACKET_MAX], rbuf[ELSC_PACKET_MAX];
    int			xlen, xaccept, rrecv;
    int			i, r, len, sum;

#if LDEBUG
    junkuart_printf("elsc_command: command(%s)\n", e->cmd);
#endif

    /*
     * Construct command packet in the following format, where len is the
     * length of the command/parameters text:
     *
     *	Byte 0: len (number of bytes of command/parameters text)
     *	Byte 1 to len: 'len' bytes of command/parameters text
     *	Byte 1+len: checksum for bytes 1 through 2+len
     */

    sum = 0;

    for (len = 0; e->cmd[len]; len++)
	sum += (xbuf[1 + len] = e->cmd[len]);

    sum += (xbuf[0] = len);

    xbuf[1 + len] = (unsigned int) (-sum) & 0xff;

    xlen = 2 + len;

    /*
     * Transmit command xbuf to ELSC and receive response.
     */

#if LDEBUG
    junkuart_printf("elsc_command: transmit %d\n", xlen);
#endif

    hub_lock_timeout(HUB_LOCK_I2C, LOCK_TIMEOUT);

    r = i2c_master_xmit_recv(e->nasid,
			     ELSC_I2C_ADDR,
			     xbuf, xlen, &xaccept,
			     rbuf, sizeof (rbuf), &rrecv,
			     2,		/* Embedded length + 2 */
			     ELSC_TIMEOUT,
			     only_if_message);

    hub_unlock(HUB_LOCK_I2C);

    if (r < 0)
	goto done;

    if (xaccept != xlen) {
	r = ELSC_ERROR_CMD_SEND;
	goto done;
    }

    /*
     * Parse response packet, which is in the following format, where len
     * is the length of the response/parameters text:
     *
     *	Byte 0: len (number of bytes of response/parameters text)
     *	Byte 1 to len: 'len' bytes of response/parameters text
     *	Byte 1+len: checksum so adding bytes 0 through 1+len is 0 mod 256.
     */

    if (rrecv < 2 || rrecv != 2 + (len = sum = rbuf[0])) {
	r = ELSC_ERROR_RESP_FORMAT;
	goto done;
    }

    if(len >= ELSC_PACKET_MAX) {
        r = ELSC_ERROR_RESP_FORMAT;
        goto done;
    }

    for (i = 0; i < len; i++)
	sum += (e->resp[i] = rbuf[1 + i]);

    e->resp[i] = 0;

    if (((sum + rbuf[1 + len]) & 0xff) != 0) {
	r = ELSC_ERROR_RESP_CHECKSUM;
	goto done;
    }

#if LDEBUG
    junkuart_printf("elsc_command: response(%s)\n", e->resp);
#endif

    r = 0;

 done:

#if LDEBUG
    if (r)
	junkuart_printf("elsc_command: r=%d\n", r);
#endif

    return r;
}

/*
 * elsc_parse
 *
 *   Parses a response in e->resp.  If the response is an error
 *   response, returns the corresponding error code.  If not, the
 *   parameters are parsed and stored into successive parameter arguments.
 *
 *   If argument parameter p1, p2, or p3 is NULL, parsing stops at that
 *   point.
 *
 *   Returns:
 *	On success, the number of arguments parsed.
 *	On failure, a negative error code.
 */

int elsc_parse(elsc_t *e, char *p1, char *p2, char *p3)
{
    char	       *s	= e->resp;

    if (p1) *p1 = 0;
    if (p2) *p2 = 0;
    if (p3) *p3 = 0;

    if (strncmp(s, "err", 3) == 0) {
	s += 4;
	if (strncmp(s, "csum", 4) == 0)
	    return ELSC_ERROR_CMD_CHECKSUM;
	if (strncmp(s, "cmd", 3) == 0)
	    return ELSC_ERROR_CMD_UNKNOWN;
	if (strncmp(s, "arg", 3) == 0)
	    return ELSC_ERROR_CMD_ARGS;
	if (strncmp(s, "perm", 4) == 0)
	    return ELSC_ERROR_CMD_PERM;
	if (strncmp(s, "pwr_state", 9) == 0) /* New return value with MSC 4.x */
	    return ELSC_ERROR_CMD_STATE;
	return ELSC_ERROR_RESP_FORMAT;
    }

    if (strncmp(s, "ok", 2) != 0)
	return ELSC_ERROR_RESP_FORMAT;

    s += 2;

    if (*s == 0)
	return 0;

    if (*s++ != ' ')
	return ELSC_ERROR_RESP_FORMAT;

    if (*s == 0 || p1 == 0)
	return 0;

    while (*s && *s != ' ')
	*p1++ = *s++;
    *p1 = 0;

    if (*s == ' ')
	s++;

    if (*s == 0 || p2 == 0)
	return 1;

    while (*s && *s != ' ')
	*p2++ = *s++;
    *p2 = 0;

    if (*s == ' ')
	s++;

    if (*s == 0 || p3 == 0)
	return 2;

    while (*s && *s != ' ')
	*p3++ = *s++;
    *p3 = 0;

    return 3;
}

/*
 * elsc_process
 *
 *   This routine checks for messages from the ELSC.
 *
 *   If an "acp" (alternate console port) message is received, TTY input
 *   is queued.  TTY data can be polled and read via the elscuart routines.
 *
 *   If any other message is received, it is either passed to the
 *   message callback routine (if one has been configured via
 *   elsc_msg_callback) or queued.  Queued messages can be polled and
 *   read via the elsc_msg_check routine.
 *
 *   elsc_process should be called fairly often from each loop in
 *   the PROM to prevent from missing ELSC messages, which time out
 *   after a while.
 *
 *   Returns I2C_ERROR_NO_MESSAGE if no message was received, 0 otherwise.
 */

int elsc_process(elsc_t *e)
{
    char	       *s;
    int			i, r;

    sprintf(e->cmd, "get");

    if ((r = elsc_command(e, 1)) < 0)
	goto done;

#if LDEBUG
    junkuart_printf("elsc_process: received %s\n", e->resp);
#endif

    if (strncmp(e->resp, "acp ", 4) == 0) {
	/*
	 * If the message is TTY input, put the TTY data on the input queue.
	 */

#if LDEBUG
	junkuart_printf("elsc_process: acp\n");
#endif

	for (s = e->resp + 6; *s; s++)
	    cq_add(&e->tiq, *s);	/* Overflow drops characters */

#if 0
     } else if (strncmp(e->resp, "sel ", 4) == 0) {
	/*
	 * If message is a hub selection, record the selected hub.
	 */

	e->sel_cpu = (e->resp[4] - '0');

	cq_init(&e->tiq);
	cq_init(&e->toq);

#if LDEBUG
	junkuart_printf("elsc_process: sel %d\n", e->sel_cpu);
#endif
#endif
    } else if (e->msg_callback) {
#if LDEBUG
	junkuart_printf("elsc_process: callback <%s>\n", e->resp);
#endif

	(*e->msg_callback)(e->msg_callback_data, e->resp);
    } else {
	/*
	 * Queue message (length + string)
	 */

#if LDEBUG
	junkuart_printf("elsc_process: enqueue\n");
#endif

	i = strlen(e->resp);

	if (cq_room(&e->imq) < 1 + i) {
	    r = ELSC_ERROR_MSG_LOST;
	    goto done;
	}

	cq_add(&e->imq, i);

	for (s = e->resp; *s; s++)
	    cq_add(&e->imq, *s);
    }

#if LDEBUG
    if (r)
	junkuart_printf("elsc_process: r=%d\n", r);
#endif

    r = 0;

 done:

    return r;
}

/*
 * Command Set
 */

int elsc_version(elsc_t *e, char *result)
{
    int			r;

    sprintf(e->cmd, "ver");

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, result, 0, 0)) < 0)
	return r;

    return 0;
}

int elsc_nvram_magic(elsc_t *e)
{
    int			r;
    u_char		magic;

    if ((r = elsc_nvram_read(e, NVRAM_MAGIC_AD, (char *) &magic, 1)) < 0)
	return r;

    return (magic == NVRAM_MAGIC_NO) ? 0 : ELSC_ERROR_NVMAGIC;
}

int elsc_debug_set(elsc_t *e, u_char byte1, u_char byte2)
{
    int			r;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_DBG1, (char *) &byte1, 1)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_DBG2, (char *) &byte2, 1)) < 0)
	return r;

    return 0;
}

int elsc_debug_get(elsc_t *e, u_char *byte1, u_char *byte2)
{
    int			r;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_DBG1, (char *) byte1, 1)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_DBG2, (char *) byte2, 1)) < 0)
	return r;

    return 0;
}

int elsc_module_set(elsc_t *e, int module)
{
    int			r;
    char		mod	= (char) module;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_MODULE, &mod, 1)) < 0)
	return r;

    return 0;
}

int elsc_module_get(elsc_t *e)
{
    int			r;
    char		mod;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_MODULE, &mod, 1)) < 0)
	return r;

    return (int) (u_char) mod;
}

int elsc_partition_set(elsc_t *e, int partition)
{
    int			r;
    char		par	= (char) partition;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_PARTITION, &par, 1)) < 0)
	return r;

    return 0;
}

int elsc_partition_get(elsc_t *e)
{
    int			r;
    char		par;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_PARTITION, &par, 1)) < 0)
	return r;

    return (int) (u_char) par;
}

int elsc_domain_set(elsc_t *e, int domain)
{
    int			r;
    char		dom	= (char) domain;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_DOMAIN, &dom, 1)) < 0)
	return r;

    return 0;
}

int elsc_domain_get(elsc_t *e)
{
    int			r;
    char		dom;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_DOMAIN, &dom, 1)) < 0)
	return r;

    return (int) (u_char) dom;
}

int elsc_cluster_set(elsc_t *e, int cluster)
{
    int			r;
    char		clu	= (char) cluster;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_CLUSTER, &clu, 1)) < 0)
	return r;

    return 0;
}

int elsc_cluster_get(elsc_t *e)
{
    int			r;
    char		clu;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_CLUSTER, &clu, 1)) < 0)
	return r;

    return (int) (u_char) clu;
}

int elsc_cell_set(elsc_t *e, int cell)
{
    int			r;
    char		cel	= (char) cell;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_CELL, &cel, 1)) < 0)
	return r;

    return 0;
}

int elsc_cell_get(elsc_t *e)
{
    int			r;
    char		cel;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_CELL, &cel, 1)) < 0)
	return r;

    return (int) (u_char) cel;
}

int elsc_bist_set(elsc_t *e, char bist_status)
{
    int		r;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_write(e, NVRAM_BIST_FLG, &bist_status, 1)) < 0)
	return r;

    return 0;
}

char elsc_bist_get(elsc_t *e)
{
    int		r;
    char	bist_status;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    if ((r = elsc_nvram_read(e, NVRAM_BIST_FLG, &bist_status, 1)) < 0)
	return r;

    return bist_status;
}

int elsc_lock(elsc_t *e, int retry_interval_usec,
	      int timeout_usec, u_char lock_val)
{
    char		p1[ELSC_PACKET_MAX];
    int			r;
    rtc_time_t		expiry;

    expiry = rtc_time() + timeout_usec;

    sprintf(e->cmd, "tas %c", (int) lock_val);

    do {
	if ((r = elsc_command(e, 0)) < 0)
	    goto done;

	if ((r = elsc_parse(e, p1, 0, 0)) < 0)
	    goto done;

	if (p1[0] == '0' || p1[0] == lock_val) {
	    r = 0;
	    goto done;
	}

	rtc_sleep(retry_interval_usec);
    } while (rtc_time() < expiry);

    r = ELSC_ERROR_LOCK_TIMEOUT;

 done:

    return r;
}

int elsc_unlock(elsc_t *e)
{
    int			r;

    sprintf(e->cmd, "tas 0");

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, 0, 0, 0)) < 0)
	return r;

    return 0;
}

int elsc_display_char(elsc_t *e, int led, int chr)
{
    int			r;

    sprintf(e->cmd, "dsc %x %c", led, chr);

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, 0, 0, 0)) < 0)
	return r;

    return 0;
}

int elsc_display_digit(elsc_t *e, int led, int num, int l_case)
{
    return elsc_display_char(e,
			     led,
			     num < 10 ? num + '0' :
			     num - 10 + (l_case ? 'a' : 'A'));
}

int elsc_display_mesg(elsc_t *e, char *chr)
{
    int			r, c;

    sprintf(e->cmd, "dsp ");

    for (r = 0; r < 8; r++) {
	c = LBYTE(chr);
	if (c) {
	    e->cmd[r + 4] = c;
	    chr++;
	} else
	    e->cmd[r + 4] = ' ';
    }

    e->cmd[12] = 0;

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, 0, 0, 0)) < 0)
	return r;

    return 0;
}

int elsc_password_set(elsc_t *e, char *password)
{
    int			r;
    char		pw[4];

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    for (r = 0; r < 4; r++)
	if ((pw[r] = LBYTE(password)) == 0)
	    pw[r] = ' ';
	else
	    password++;

    return elsc_nvram_write(e, NVRAM_PASS_WD, pw, 4);
}

int elsc_password_get(elsc_t *e, char *password)
{
    int			r;

    if ((r = elsc_nvram_magic(e)) < 0)
	return r;

    password[4] = 0;

    return elsc_nvram_read(e, NVRAM_PASS_WD, password, 4);
}

/*
 * elsc_power_query
 *
 *   To be used after system reset, this command returns 1 if the reset
 *   was the result of a power-on, 0 otherwise.
 *
 *   The power query status is cleared to 0 after it is read.
 */

int elsc_power_query(elsc_t *e)
{
    char		p1[ELSC_PACKET_MAX];
    int			r;

    sprintf(e->cmd, "pwr q");

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, p1, 0, 0)) < 0)
	return r;

    return (int) strtoull(p1, 0, 16);
}

int elsc_rpwr_query(elsc_t *e, int is_master)
{
    char		p1[ELSC_PACKET_MAX];
    char		p2[ELSC_PACKET_MAX];
    char		*s;
    int			r;

    sprintf(e->cmd, "pwr r");

    if ((r = elsc_command(e, 0)) < 0)
	return r;

#ifdef SIMULATE_RPS
    strcpy(e->resp, "ok m:fl s:ok");
#endif

    if ((r = elsc_parse(e, p1, p2, 0)) < 0)
	return r;
    
#if _STANDALONE
    if( SN00 && !is_master) {
#else
    if( private.p_sn00 && !is_master) {
#endif
	s = p2;
    }
    else
	s = p1;

    if(s[0] == 'n')
	return -1;

    if(strstr(s,"fl") != NULL)
	return 0;

    if(strstr(s, "ok") != NULL)
	return 1;

    return -1;
}

/*
 * elsc_power_down
 *
 *   Sets up system to shut down in "sec" seconds (or modifies the
 *   shutdown time if one is already in effect).  Use 0 to power
 *   down immediately.
 */

int elsc_power_down(elsc_t *e, int sec)
{
    int			r;

    sprintf(e->cmd, "pwr d %x", sec);

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, 0, 0, 0)) < 0)
	return r;

    return 0;
}

int elsc_system_reset(elsc_t *e)
{
    int			r;

    sprintf(e->cmd, "rst");

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, 0, 0, 0)) < 0)
	return r;

    return 0;
}

int elsc_power_cycle(elsc_t *e)
{
    int			r;

    sprintf(e->cmd, "pwr c");

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, 0, 0, 0)) < 0)
	return r;

    return 0;
}

int elsc_dip_switches(elsc_t *e)
{
    char		p1[ELSC_PACKET_MAX];
    int			r;

    sprintf(e->cmd, "rsw");

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, p1, 0, 0)) < 0)
	return r;

    return 0xff ^ (int) strtoull(p1, 0, 16);
}

/*
 * ELSC Support for reading midplane NIC
 */

/*ARGSUSED*/
static int nic_access(nic_data_t data,
		      int pulse, int sample, int delay)
{
    char		p1[ELSC_PACKET_MAX];
    int			r;
    elsc_t	       *e	= (elsc_t *) data;

    sprintf(e->cmd, "nic %x %x %x", pulse, sample, delay);

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, p1, 0, 0)) < 0)
	return r;

    return (int) strtoull(p1, 0, 16);
}

int elsc_nic_get(elsc_t *e, __uint64_t *nic, int verbose)
{
    nic_state_t		ns;
    int			r;

    verbose = verbose;	/* XXX implement verbose */

    if ((r = nic_setup(&ns,
		       nic_access,
		       (nic_data_t) e)) != 0)
	return (r > 0) ? ELSC_ERROR_NIC : r;

    *nic = 0;				/* Clear two MS bytes */

    if ((r = nic_next(&ns, (char *) nic + 2, (char *) 0, (char *) 0)) != 0)
	return (r > 0) ? ELSC_ERROR_NIC : r;

    return 0;
}

int _elsc_hbt(elsc_t *e, int ival, int rdly)
{
    int			r;

    if (ival)
	sprintf(e->cmd,
		"hbt %d %d",		/* Enable heartbeat monitoring */
		ival, rdly);
    else if (rdly)
	sprintf(e->cmd, "hbt");		/* Send heartbeat */
    else
	sprintf(e->cmd, "hbt 0");	/* Disable heartbeat monitoring */

    if ((r = elsc_command(e, 0)) < 0)
	return r;

    if ((r = elsc_parse(e, 0, 0, 0)) < 0)
	return r;

    return 0;
}

/*
 * _ELSCUART Routines
 *
 *   These routines provide a TTY interface for pass-through terminal I/O
 *   to and from the UART on the ELSC (Alternate Console Port, or ACP).
 */

int _elscuart_probe(elsc_t *e)
{
    char                ver[ELSC_PACKET_MAX];
    return (elsc_version(e, ver) >= 0);
}

/* ARGSUSED */
void _elscuart_init(elsc_t *e, void *init_data)
{
    cq_init(&e->tiq);
    cq_init(&e->toq);
    cq_init(&e->imq);
}

int _elscuart_poll(elsc_t *e)
{
#if 0
    if (e->sel_cpu != get_myid())
	return 0;
#endif

    if (! cq_empty(&e->tiq)) {
#if LDEBUG
	junkuart_printf("elscuart_poll: using queued data\n");
#endif
	return 1;
    }

    elsc_process(e);

    if (cq_empty(&e->tiq))
	return 0;

#if LDEBUG
    junkuart_printf("elscuart_poll: new input\n");
#endif

    return 1;
}

/*
 * _elscuart_readc
 *
 *   It's illegal to call this routine unless elscuart_poll has indicated
 *   a character is waiting.
 */

int _elscuart_readc(elsc_t *e)
{
    int			c;

    if (cq_empty(&e->tiq)) {
#if LDEBUG
	junkuart_printf("elscuart_readc: empty error\n");
#endif
	return -1;
    }

    cq_rem(&e->tiq, c);

#if LDEBUG
    junkuart_printf("elscuart_readc: return %d\n", c);
#endif

    return c;
}

/*
 * _elscuart_getc
 *
 *   This routine blocks, so it is necessary to use elsc_msg_callback
 *   to set up for processing ELSC messages during this routine
 *   or other routines that call it (e.g., gets).
 */

int _elscuart_getc(elsc_t *e)
{
    int			r, c;

    while (/* e->sel_cpu != get_myid() || */ cq_empty(&e->tiq))
	if ((r = elsc_process(e)) < 0 && r != I2C_ERROR_NO_MESSAGE)
	    return -1;

    cq_rem(&e->tiq, c);

#if LDEBUG
    junkuart_printf("elscuart_getc: return %d\n", c);
#endif

    return c;
}

/*
 * _elscuart_puts
 */

int _elscuart_puts(elsc_t *e, char *s)
{
    int			c;

#if LDEBUG
    junkuart_printf("elscuart_puts: (%s)\n", s);
#endif

#if 0
    if (e->sel_cpu != get_myid())
	return 0;	/* Discard output if not selected */
#endif

    if (s == 0)
	s = "<NULL>";

    while ((c = LBYTE(s)) != 0) {
	if (_elscuart_putc(e, c) < 0)
	    return -1;
	s++;
    }

    return 0;
}

/*
 * _elscuart_putc
 */

int _elscuart_putc(elsc_t *e, int c)
{
#if LDEBUG
    junkuart_printf("elscuart_putc: %d\n", c);
#endif

#if 0
    if (e->sel_cpu != get_myid())
	return 0;	/* Discard output if not selected */
#endif

    if (c != '\n' && c != '\r' && cq_used(&e->toq) >= ELSC_LINE_MAX) {
	cq_add(&e->toq, '\r');
	cq_add(&e->toq, '\n');
	_elscuart_flush(e);
	e->sol = 1;
    }

    if (e->sol && c != '\r') {
	char		prefix[16], *s;

	if (cq_room(&e->toq) < 8 && _elscuart_flush(e) < 0)
	    return -1;

#if _STANDALONE
	sprintf(prefix,
		"%c%c %03d: ",
		'0' + (int) hub_slot_get(),
		'A' + (int)  hub_cpu_get(),
		e->nasid);
#else /* _STANDALONE */
	sprintf(prefix,			/* kernel printf is evil */
		"%c%c %d%d%d: ",
		'0' + (int) hub_slot_get(),
		'A' + (int)  hub_cpu_get(),
		e->nasid / 100,
		(e->nasid / 10) % 10,
		e->nasid % 10);
#endif /* _STANDALONE */

	for (s = prefix; *s; s++)
	    cq_add(&e->toq, *s);

	e->sol = 0;
    }

    if (cq_room(&e->toq) < 2 && _elscuart_flush(e) < 0)
	return -1;

    if (c == '\n') {
	cq_add(&e->toq, '\r');
	e->sol = 1;
    }

    cq_add(&e->toq, (u_char) c);

    if (c == '\n') {
	/* Implement line buffering */

	if (_elscuart_flush(e) < 0)
	    return -1;
    }

    if (c == '\r')
	e->sol = 1;

    return 0;
}

/*
 * _elscuart_gets
 *
 *   Mini version of gets for debugging.
 */

/*ARGSUSED*/
char *_elscuart_gets(elsc_t *e, char *buf, int max_length)
{
    int			c;
    char	       *bufp;

    bufp = buf;

    while ((c = _elscuart_getc(e)) != '\r') {
	_elscuart_putc(e, c);
	_elscuart_flush(e);
	*bufp++ = c;
    }

    _elscuart_putc(e, c);
    _elscuart_putc(e, '\n');

    *bufp = 0;

    return buf;
}

/*
 * _elscuart_flush
 *
 *   This routine flushes queued TTY output to the ELSC.  It's called
 *   automatically if the output queue runs out of room, or if a newline
 *   is written (line buffered output).  It may be called at other
 *   appropriate times, such as at the beginning of gets.
 */

int _elscuart_flush(elsc_t *e)
{
    char		p1[ELSC_PACKET_MAX];
    int			n, r;

#if LDEBUG
    junkuart_printf("elscuart_flush\n");
#endif

    while ((n = cq_used(&e->toq)) > 0) {
	if (n > ELSC_ACP_MAX)
	    n = ELSC_ACP_MAX;

	sprintf(e->cmd, "acp %d ", get_myid());

	/* Handle circular queue wrap-around */

	r = ELSC_QSIZE - e->toq.opos;

	if (n > r) {
	    memcpy(e->cmd + 6, &e->toq.buf[e->toq.opos], r);
	    memcpy(e->cmd + 6 + r, &e->toq.buf[0], n - r);
	} else
	    memcpy(e->cmd + 6, &e->toq.buf[e->toq.opos], n);

	e->cmd[6 + n] = 0;

#if LDEBUG
	junkuart_printf("elscuart_flush: n=%d\n", n);
#endif

	if ((r = elsc_command(e, 0)) < 0)
	    goto done;

	if ((r = elsc_parse(e, p1, 0, 0)) < 0)
	    goto done;

	r = (int) strtoull(p1, 0, 16);	/* Number of characters accepted */

	if (r >= 0 && r <= n)
	    e->toq.opos = (e->toq.opos + r) % ELSC_QSIZE;
    }

    r = 0;

 done:

    return r;
}

/*
 * ELSCUART Routines
 *
 *   The following routines are similar to their counterparts above,
 *   except instead of taking an elsc_t pointer directly, they call
 *   a user-supplied global routine "get_elsc" to obtain the pointer.
 *   This is useful when the elsc is employed for stdio.
 */

int elscuart_probe(void)
{
    return _elscuart_probe(get_elsc());
}

void elscuart_init(void *init_data)
{
    _elscuart_init(get_elsc(), init_data);
}

int elscuart_poll(void)
{
    return _elscuart_poll(get_elsc());
}

int elscuart_readc(void)
{
    return _elscuart_readc(get_elsc());
}

int elscuart_getc(void)
{
    return _elscuart_getc(get_elsc());
}

int elscuart_puts(char *s)
{
    return _elscuart_puts(get_elsc(), s);
}

int elscuart_putc(int c)
{
    return _elscuart_putc(get_elsc(), c);
}

char *elscuart_gets(char *buf, int max_length)
{
    return _elscuart_gets(get_elsc(), buf, max_length);
}

int elscuart_flush(void)
{
    return _elscuart_flush(get_elsc());
}


int elsc_mfg_nic_get(elsc_t *e, char *nic_info, int verbose)
{
      int status;
      status = nic_info_get(nic_access, (nic_data_t)e , nic_info);
      return status;
}

int elsc_ust_write(elsc_t *e, uchar_t c)
{
    int			r, xlen;


    hub_lock_timeout(HUB_LOCK_I2C, LOCK_TIMEOUT);

    r = i2c_master_xmit(e->nasid, I2C_ADDR_UST ,
			    &c, 1, &xlen, I2C_TIMEOUT, 0);

    hub_unlock(HUB_LOCK_I2C);
    return r;
}

int elsc_ust_read(elsc_t *e, char *c)
{
    uchar_t		rdata[1];
    uchar_t		xdata[1];
    int			i, r, xlen, rlen;


	hub_lock_timeout(HUB_LOCK_I2C, LOCK_TIMEOUT);

	r = i2c_master_recv(e->nasid,
				 I2C_ADDR_UST,
				 rdata, 1, &rlen,
				 -1, I2C_TIMEOUT, 0);

	hub_unlock(HUB_LOCK_I2C);


	*c = (char) rdata[0];


    return r;
}
