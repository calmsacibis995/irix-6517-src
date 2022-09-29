/*
 * Copyright (C) 1986, 1992, 1993, 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/*
 * This is a lower level module for the modular serial i/o driver. This
 * module implements dummy entry points.
 */

#include <ksys/serialio.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/hwgraph.h>
#include "sys/ddi.h"
#include "sys/pda.h"

#ifdef DEBUG
#include <sys/idbgentry.h>
#endif

#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

/* To make the INTR_KTHREADS changes touch fewer lines, just redefine
 * the spinlock routines to be mutex routines instead of changing every call.
 */
#undef mutex_spinlock
#undef mutex_spinunlock
#undef nested_spinlock
#undef nested_spinunlock
#undef spinlock_init

#define mutex_spinlock(x) (mutex_lock(x, PZERO), 0)
#define mutex_spinunlock(x,s) mutex_unlock(x)
#define nested_spinlock(x) mutex_lock(x, PZERO)
#define nested_spinunlock(x) mutex_unlock(x)
#define spinlock_init(x,name) mutex_init(x, MUTEX_DEFAULT, name);

#define FIFO_CHARS 64
#define FIFO_MASK (FIFO_CHARS-1)
struct fifo {
    int fifo[FIFO_CHARS];
    int prod, cons;
    int flags;
};
#define FIFO_BREAK 1

#define FIFO_EMPTY(f) ((f)->prod == (f)->cons && \
		       ((f)->flags & FIFO_BREAK) == 0)
#define FIFO_FULL(f) (((f)->prod - (f)->cons) == FIFO_CHARS)
#define FIFO_SIZE(f) ((f)->prod - (f)->cons)

#define FIFO_PUTC(f, c) ((c) == BREAK_CHAR ? \
			 (((f)->flags |= FIFO_BREAK), 1) : \
			 (FIFO_FULL(f) ? 0 : \
			  ((f)->fifo[(f)->prod++ & FIFO_MASK] = c, 1)))

#define FIFO_GETC(f) (((f)->flags & FIFO_BREAK) ? \
		      ((f)->flags &= ~FIFO_BREAK, BREAK_CHAR) : \
		      (FIFO_EMPTY(f) ? -1 : \
		       ((f)->fifo[(f)->cons++ & FIFO_MASK])))

/* special fifo char flags */
#define BREAK_CHAR 0x100

/* simulated uart */
struct uart {
    mutex_t lock;
    struct fifo rx_fifo;
    struct fifo tx_fifo;
    int intr_pend;
    int intr_enb;
    int rx_timeout;
    int baud;
    int baudcount;
    int outsigs;
    int insigs;

#ifdef DEBUG
    int write_calls;
    int read_calls;
    int tx_chars;
    int rx_chars;
    int rx_timeouts;
    int rx_overruns;
    int rx_break;
    int tx_break;
#endif
};

#define INTR_TX		0x1
#define INTR_RX		0x2
#define INTR_DDCD	0x4
#define INTR_DCTS	0x8

#define F_DCDDTR	0x1
#define F_RTSCTS	0x2

#define HEARTBEAT_EVENTS (N_OUTPUT_LOWAT | N_DATA_READY | \
			  N_BREAK | N_OVERRUN_ERROR | N_DDCD | N_DCTS)

#define BAUD_TRIGGER (HZ * 10)

/* local port info for the DUMMY serial ports. This contains as its
 * first member the global sio port private data.
 */
typedef struct dummyport {
    sioport_t sioport; /* must be first struct entry! */

    struct uart uart;
    int flags;
    int baud;
    int byte_size;
    int notify;
    struct dummyport *twin;
} dummyport_t;

/* number of ports configured */
extern int dummy_loopback_pairs;
#define NUM_PORTS (dummy_loopback_pairs * 2)
struct dummyport **ports;

#define PORT_BREAK 1
#define PORT_PARENB 2
#define PORT_PARODD 4
#define PORT_STOPBITS 8

#define MAX_CHARS 16


/* get local port type from global sio port type */
#define LPORT(port) ((dummyport_t*)(port))

/* get global port from local port type */
#define GPORT(port) ((sioport_t*)(port))

static void enable_heartbeat(void);
static void disable_heartbeat(void);
static void heartbeat(void);
static void dummy_intr(dummyport_t*);
static int heartbeat_refcnt;
static toid_t heartbeat_toid;
static mutex_t heartbeat_lock;

/* local functions: */
static int dummy_open		(sioport_t *port);
static int dummy_config		(sioport_t *port, int baud, int byte_size,
				 int stop_bits, int parenb, int parodd);
static int dummy_enable_hfc	(sioport_t *port, int enable);
static int dummy_set_extclk	(sioport_t *port, int clock_factor);

    /* data transmission */
static int dummy_write		(sioport_t *port, char *buf, int len);
static void dummy_wrflush	(sioport_t *port);
static int dummy_break		(sioport_t *port, int brk);

    /* data reception */
static int dummy_read		(sioport_t *port, char *buf, int len);
    
    /* event notification */
static int dummy_notification		(sioport_t *port, int mask, int on);
static int dummy_rx_timeout		(sioport_t *port, int timeout);

    /* modem control */
static int dummy_set_DTR	(sioport_t *port, int dtr);
static int dummy_set_RTS	(sioport_t *port, int rts);
static int dummy_query_DCD	(sioport_t *port);
static int dummy_query_CTS	(sioport_t *port);

    /* output control */
static int dummy_set_proto	(sioport_t *port, enum sio_proto proto);

static struct serial_calldown dummy_calldown = {
    dummy_open,
    dummy_config,
    dummy_enable_hfc,
    dummy_set_extclk,
    dummy_write,
    dummy_write,	/* du write, for now same as regular write */
    dummy_wrflush,
    dummy_break,
    dummy_read,
    dummy_notification,
    dummy_rx_timeout,
    dummy_set_DTR,
    dummy_set_RTS,
    dummy_query_DCD,
    dummy_query_CTS,
    dummy_set_proto,
};

#if 0
void
dump_dummyport_act(__psint_t p)
{
    dummyport_t *port;
    struct uart *u;

    if (p < MIN_MINOR || p > MAX_MINOR) {
	qprintf("minor must be in range [%d-%d]\n", MIN_MINOR, MAX_MINOR);
	return;
    }
    port = &MINOR_TO_PORT(p);
    u = &port->uart;

    qprintf("\ndummy port %d\n", p);
    qprintf("write calls %d\n", u->write_calls);
    qprintf("read calls %d\n", u->read_calls);
    qprintf("rx chars %d\n", u->rx_chars);
    qprintf("rx timeouts %d\n", u->rx_timeouts);
    qprintf("rx overruns %d\n", u->rx_overruns);
    qprintf("tx chars %d\n", u->tx_chars);
    qprintf("rx break %d\n", u->rx_break);
    qprintf("tx break %d\n", u->tx_break);
    qprintf("baud %d bpc %d stopbits %d parenb %d parodd %d\n",
	    port->baud, port->byte_size,
	    (port->flags & PORT_STOPBITS) && 1,
	    (port->flags & PORT_PARENB) && 1,
	    (port->flags & PORT_PARODD) && 1);
}
#endif

/* uart routines */
static int
uart_putc(struct uart *uart, int c)
{
    /*REFERENCED*/
    int s, sent;
    struct fifo *tx_fifo = &uart->tx_fifo;

    s = mutex_spinlock(&uart->lock);
    
    /* If tx fifo is currently empty, we'll need to clear the intr condition
     */
    if (FIFO_EMPTY(tx_fifo))
	uart->intr_pend &= ~INTR_TX;

    ASSERT((uart->intr_pend & INTR_TX) == 0);

    /* stuff the char if there's room for it */
    sent = FIFO_PUTC(tx_fifo, c);

#ifdef DEBUG
    if (c == BREAK_CHAR)
	uart->tx_break++;
#endif

    mutex_spinunlock(&uart->lock, s);

    return(sent);
}

static int
uart_getc(struct uart *uart)
{
    /*REFERENCED*/
    int s, c;
    struct fifo *rx_fifo = &uart->rx_fifo;

    s = mutex_spinlock(&uart->lock);

    uart->rx_timeout = 0;

    c = FIFO_GETC(rx_fifo);

    /* If the fifo is now below the high water mark, clear the intr */
    if (FIFO_SIZE(rx_fifo) < FIFO_CHARS / 2)
	uart->intr_pend &= ~INTR_RX;

#ifdef DEBUG
    if (c == BREAK_CHAR)
	uart->rx_break++;
#endif

    mutex_spinunlock(&uart->lock, s);

    return(c);
}

/*
 * Boot time initialization of this module
 */
void
dummy_init()
{
    int portnum;
    dummyport_t *port;
    char name[16];
    vertex_hdl_t pvhdl;
    graph_error_t rc;

    ports = (dummyport_t**)kmem_zalloc(sizeof(dummyport_t*) * NUM_PORTS,
				       KM_NOSLEEP);
#ifdef DEBUG
    if ((caddr_t)ports[0] != (caddr_t)&(ports[0]->sioport))
	panic("sioport is not first member of dummyport struct\n");
#endif

    port = (dummyport_t*)kmem_zalloc(sizeof(dummyport_t) * NUM_PORTS,
				     KM_NOSLEEP);

    for(portnum = 0; portnum < NUM_PORTS; portnum++, port++) {

	ports[portnum] = port;
	
	/* attach the calldown hooks so upper layer can call our
	 * routines.
	 */
	port->sioport.sio_calldown = &dummy_calldown;
	spinlock_init(&port->uart.lock, "uart");
	
	sprintf(name, "dummy/%d", portnum+1);
	rc = hwgraph_path_add(hwgraph_root, name, &pvhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_fastinfo_set(pvhdl, (arbitrary_info_t)port);

	/* perform upper layer initialization */
	sio_initport(GPORT(port), pvhdl, NODE_TYPE_ALL, 0);
    }

    for(portnum = 0; portnum < NUM_PORTS; portnum += 2) {
	ports[portnum]->twin = ports[portnum + 1];
	ports[portnum + 1]->twin = ports[portnum];
    }

    spinlock_init(&heartbeat_lock, "heartbeat");
}

static int
dummy_xfer(struct uart *from, struct uart *to)
{
    int c;

    /* send modem/flow control signals */
    if ((from->outsigs ^ to->insigs) & F_DCDDTR)
	to->intr_pend |= INTR_DDCD;

    if ((from->outsigs ^ to->insigs) & F_RTSCTS)
	to->intr_pend |= INTR_DCTS;

    to->insigs = from->outsigs;

    if ((c = FIFO_GETC(&from->tx_fifo)) == -1) {

	/* If there are bytes hanging around in the receiver,
	 * interrupt if they have been waiting there for a long time
	 */
	if (!FIFO_EMPTY(&to->rx_fifo)) {
	    if (to->rx_timeout++ > FIFO_CHARS / 2 ||
		(to->rx_fifo.flags & FIFO_BREAK)) {
		to->rx_timeout = 0;
		to->intr_pend |= INTR_RX;
#ifdef DEBUG
		to->rx_timeouts++;
#endif
	    }
	}
	    
	return(0);
    }
    
    /* if we just emptied the tx fifo, signal a TX done interrupt
     */
    if (FIFO_EMPTY(&from->tx_fifo))
	from->intr_pend |= INTR_TX;
    
#ifdef DEBUG
    from->tx_chars++;
#endif

    to->rx_timeout = 0;

    /* If there is a baudrate mismatch, simulate garbled data */
    if (from->baud != to->baud && c != BREAK_CHAR)
	c = 'X';

    c = FIFO_PUTC(&to->rx_fifo, c);
#ifdef DEBUG
    if (c)
	to->rx_chars++;
    else
	to->rx_overruns++;
#endif

    /* If we just crossed the threshold, signal an RX interrupt */
    if (FIFO_SIZE(&to->rx_fifo) == FIFO_CHARS / 2)
	to->intr_pend |= INTR_RX;

    return(1);
}

/* Start heartbeat if needed
 */
static void
enable_heartbeat(void)
{
    /*REFERENCED*/
    int s = mutex_spinlock(&heartbeat_lock);

    if (heartbeat_toid == 0)
	/* start heartbeat */
        heartbeat_toid = dtimeout(heartbeat, 0, 1, plbase, cpuid());

    heartbeat_refcnt++;

    mutex_spinunlock(&heartbeat_lock, s);
}

/* disable heartbeat */
static void
disable_heartbeat(void)
{
    /*REFERENCED*/
    int s = mutex_spinlock(&heartbeat_lock);

    heartbeat_refcnt--;
    ASSERT(heartbeat_refcnt >= 0);

    mutex_spinunlock(&heartbeat_lock, s);
}

static void
heartbeat(void)
{
    /*REFERENCED*/
    int s, x, didsomething = 0;

    /* simulate the loopback connection. transfer one char
     * in each direction
     */
    for(x = 0; x < NUM_PORTS; x += 2) {
	struct uart *u1 = &ports[x]->uart;
	struct uart *u2 = &ports[x+1]->uart;

	s = mutex_spinlock(&u1->lock);
	nested_spinlock(&u2->lock);

	/* write from u1 to u2 at the requested baud */
	u1->baudcount += u1->baud;
	while(u1->baudcount > BAUD_TRIGGER) {
	    if (dummy_xfer(u1, u2)) {
		didsomething = 1;
		u1->baudcount -= BAUD_TRIGGER;
	    }
	    else
		break;
	}

	/* write from u2 to u1 at the requested baud */
	u2->baudcount += u2->baud;
	while(u2->baudcount > BAUD_TRIGGER) {
	    if (dummy_xfer(u2, u1)) {
		didsomething = 1;
		u2->baudcount -= BAUD_TRIGGER;
	    }
	    else
		break;
	}

	nested_spinunlock(&u2->lock);
	mutex_spinunlock(&u1->lock, s);
    }

    /* If any enabled interrupts are present, service them */
    for(x = 0; x < NUM_PORTS; x++)
	while (ports[x]->uart.intr_pend & ports[x]->uart.intr_enb)
	    dummy_intr(ports[x]);

    s = mutex_spinlock(&heartbeat_lock);
    if (heartbeat_refcnt > 0 || didsomething)
        heartbeat_toid = dtimeout(heartbeat, 0, 1, plbase, cpuid());
    else
	heartbeat_toid = 0;
    mutex_spinunlock(&heartbeat_lock, s);
}

static void
dummy_intr(dummyport_t *port)
{
    sioport_t *gp = GPORT(port);
    struct uart *uart = &port->uart;

    ASSERT(sio_port_islocked(gp) == 0);
    LOCK_PORT(gp);

    dprintf(("timeout port 0x%x\n", port));

    if ((uart->intr_pend & INTR_RX) && (port->notify & N_DATA_READY))
	UP_DATA_READY(gp);

    if ((uart->intr_pend & INTR_TX) && (port->notify & N_OUTPUT_LOWAT))
	UP_OUTPUT_LOWAT(gp);

    if ((uart->intr_pend & INTR_DDCD) && (port->notify & N_DDCD)) {
	/*REFERENCED*/
	int s = mutex_spinlock(&uart->lock);
	uart->intr_pend &= ~INTR_DDCD;
	mutex_spinunlock(&uart->lock, s);
	UP_DDCD(gp, uart->insigs & F_DCDDTR);
    }

    if ((uart->intr_pend & INTR_DCTS) && (port->notify & N_DCTS)) {
	/*REFERENCED*/
	int s = mutex_spinlock(&uart->lock);
	uart->intr_pend &= ~INTR_DCTS;
	mutex_spinunlock(&uart->lock, s);
	UP_DCTS(gp, uart->insigs & F_RTSCTS);
    }

    UNLOCK_PORT(gp);
}

/*
 * open a port
 */
/* ARGSUSED */
static int
dummy_open(sioport_t *port)
{
    dummyport_t *p = LPORT(port);

    ASSERT(L_LOCKED(port, L_OPEN));

    p->flags = 0;

    dprintf(("port 0x%x opened\n", port));
    
    return(0);
}

/*
 * config hardware
 */
static int
dummy_config(sioport_t *port,
	     int baud,
	     int byte_size,
	     int stop_bits,
	     int parenb,
	     int parodd)
{
    dummyport_t *p = LPORT(port);
    ASSERT(L_LOCKED(port, L_CONFIG));

    dprintf(("port 0x%x baud %d byte size %d stop bits %d parenb %d parodd %d\n",
	   port, baud, byte_size, stop_bits, parenb, parodd));

    p->baud = baud;
    p->byte_size = byte_size;

    if (stop_bits)
	p->flags |= PORT_STOPBITS;
    else
	p->flags &= ~PORT_STOPBITS;

    if (parenb)
	p->flags |= PORT_PARENB;
    else
	p->flags &= ~PORT_PARENB;

    if (parodd)
	p->flags |= PORT_PARODD;
    else
	p->flags &= ~PORT_PARODD;
    
    p->uart.baud = baud;
    p->uart.baudcount = 0;

    return(0);
}

/*
 * Enable hardware flow control
 */
/* ARGSUSED */
static int
dummy_enable_hfc(sioport_t *port, int enable)
{
    ASSERT(L_LOCKED(port, L_ENABLE_HFC));
    dprintf(("port 0x%x hfc %d\n", port, enable));
    return(1);
}

/*
 * set external clock
 */
/*ARGSUSED*/
static int
dummy_set_extclk(sioport_t *port, int clock_factor)
{
    ASSERT(L_LOCKED(port, L_SET_EXTCLK));
    /* clearly we don't support this */
    return(clock_factor);
}

/*
 * write bytes to the hardware. Returns the number of bytes
 * actually written
 */
static int
dummy_write(sioport_t *port, char *buf, int len)
{
    dummyport_t *p = LPORT(port);
    int total;
    ASSERT(L_LOCKED(port, L_WRITE));

#ifdef DEBUG
    p->uart.write_calls++;
#endif
    total = 0;
    while(len--) {
	if (uart_putc(&p->uart, *buf) == 0)
	    break;
	buf++;
	total++;
    }
    dprintf(("port 0x%x write %d\n", port, total));

    return(total);
}

/*ARGSUSED*/
static void
dummy_wrflush(sioport_t *port)
{
}


/*
 * Set or clear break condition on output
 */
static int
dummy_break(sioport_t *port, int brk)
{
    dummyport_t *p = LPORT(port);
    ASSERT(L_LOCKED(port, L_BREAK));

    if (brk)
	uart_putc(&p->uart, BREAK_CHAR);

    dprintf(("port 0x%x break %d\n", port, brk));
    return(0);
}

/*
 * read in bytes from the hardware. Return the number of bytes
 * actually read
 */
static int
dummy_read(sioport_t *port, char *buf, int len)
{
    dummyport_t *p = LPORT(port);
    int c, total;

    ASSERT(L_LOCKED(port, L_READ));

#ifdef DEBUG
    p->uart.read_calls++;
#endif
    total = 0;
    if (p->flags & PORT_BREAK) {
	p->flags &= ~PORT_BREAK;
	c = BREAK_CHAR;
    }
    else
	c = uart_getc(&p->uart);

    while(c != -1 && len > 0) {

	if (c == BREAK_CHAR) {
	    if (total > 0) {
		p->flags |= PORT_BREAK;
		break;
	    }
	    else {
		len = 1;
		if (p->notify & N_BREAK)
		    UP_NCS(port, NCS_BREAK);
	    }
	}
	*buf++ = c;
	total++;
	len--;

	if (len > 0)
	    c = uart_getc(&p->uart);
    }
    dprintf(("port 0x%x read %d\n", port, total));
    return(total);
}

/*
 * set event notification
 */
static int
dummy_notification(sioport_t *port, int mask, int on)
{
    dummyport_t *p = LPORT(port);
    /*REFERENCED*/
    int s, bits;

    ASSERT(L_LOCKED(port, L_NOTIFICATION));

    ASSERT(mask);

    bits = 0;
    dprintf(("port 0x%x turn %s notification on", port, on ? "on" : "off"));
    if (mask & N_DATA_READY) {
	dprintf((" input,"));
	bits |= INTR_RX;
    }

    if (mask & N_OUTPUT_LOWAT) {
	dprintf((" output lowat,"));
	bits |= INTR_TX;
    }

    if (mask & N_DDCD)
	bits |= INTR_DDCD;
    if (mask & N_DCTS)
	bits |= INTR_DCTS;

    dprintf(("\n"));

    s = mutex_spinlock(&p->uart.lock);

    if (on)
	p->uart.intr_enb |= bits;
    else
	p->uart.intr_enb &= ~bits;

    mutex_spinunlock(&p->uart.lock, s);

    if (on) {
	if ((p->notify & (HEARTBEAT_EVENTS)) == 0 &&
	    (mask & (HEARTBEAT_EVENTS)))
	    enable_heartbeat();
	p->notify |= mask;
    }
    else {
	if ((p->notify & (HEARTBEAT_EVENTS)) &&
	    ((p->notify & ~mask) & (HEARTBEAT_EVENTS)) == 0)
	    disable_heartbeat();
	p->notify &= ~mask;
    }

    return(0);
}

/*
 * Set RX timeout and threshold values. The upper layer passes in a
 * timeout value. In all cases it would like to be notified at least this
 * often when there are RX chars coming in. We set the RX timeout and
 * RX threshold (based on baud) to ensure that the upper layer is called
 * at roughly this interval during normal RX
 * The input timeout value is in ticks.
 */
/* ARGSUSED */
static int
dummy_rx_timeout(sioport_t *port, int timeout)
{
    ASSERT(L_LOCKED(port, L_RX_TIMEOUT));

    dprintf(("port 0x%x timeout %d\n", port, timeout));
    return(1);
}

/* ARGSUSED */
static int
dummy_set_DTR(sioport_t *port, int dtr)
{
    /*REFERENCED*/
    int s;
    dummyport_t *p = LPORT(port);
    ASSERT(L_LOCKED(port, L_SET_DTR));

    dprintf(("port 0x%x DTR %d\n", port, dtr));

    s = mutex_spinlock(&p->uart.lock);
    if (dtr)
	p->uart.outsigs |= F_DCDDTR;
    else
	p->uart.outsigs &= ~F_DCDDTR;
    mutex_spinunlock(&p->uart.lock, s);

    return(0);
}

/* ARGSUSED */
static int
dummy_set_RTS(sioport_t *port, int rts)
{
    /*REFERENCED*/
    int s;
    dummyport_t *p = LPORT(port);
    ASSERT(L_LOCKED(port, L_SET_RTS));

    dprintf(("port 0x%x RTS %d\n", port, rts));
    s = mutex_spinlock(&p->uart.lock);
    if (rts)
	p->uart.outsigs |= F_RTSCTS;
    else
	p->uart.outsigs &= ~F_RTSCTS;
    mutex_spinunlock(&p->uart.lock, s);
	
    return(0);
}

/* ARGSUSED */
static int
dummy_query_DCD(sioport_t *port)
{
    /*REFERENCED*/
    int s, ret;
    dummyport_t *p = LPORT(port);
    ASSERT(L_LOCKED(port, L_QUERY_DCD));

    dprintf(("port 0x%x query DCD\n", port));

    s = mutex_spinlock(&p->twin->uart.lock);
    ret = p->twin->uart.outsigs & F_DCDDTR;
    mutex_spinunlock(&p->twin->uart.lock, s);
    return(ret);
}

/* ARGSUSED */
static int
dummy_query_CTS(sioport_t *port)
{
    /*REFERENCED*/
    int s, ret;
    dummyport_t *p = LPORT(port);
    ASSERT(L_LOCKED(port, L_QUERY_CTS));

    dprintf(("port 0x%x query CTS\n", port));

    s = mutex_spinlock(&p->twin->uart.lock);
    ret = p->twin->uart.outsigs & F_RTSCTS;
    mutex_spinunlock(&p->twin->uart.lock, s);
    return(ret);
}

/* ARGSUSED */
static int
dummy_set_proto(sioport_t *port, enum sio_proto proto)
{
    ASSERT(L_LOCKED(port, L_SET_PROTOCOL));
    /* support only rs232 */
    return(proto != PROTO_RS232);
}
