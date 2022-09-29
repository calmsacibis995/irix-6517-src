#ident	"$Revision: 1.109 $"
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
 * module implements all hardware dependent functions for doing serial
 * i/o on the IOC3 serial ports.
 */

#if DEBUG
/* XXX for LA triggering. If a DMA gets lost, we write to this
 * variable.
 */
int * ioc3_trigger;
#endif

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/iobus.h>
#include <sys/driver.h>

#include <sys/graph.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>

#include <sys/serialio.h>
#include <ksys/serialio.h>
#include <sys/ns16550.h>

#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/ioc3.h>
#include <sys/invent.h>
#if DEBUG
#include <sys/idbgentry.h>
#include <sys/arcs/spb.h>
#endif
#include <sys/arcs/debug_block.h>
#include <sys/kopt.h>
#include <sys/errno.h>

#if IP27	/* XXX- must eliminate this. */
#include <sys/PCI/bridge.h>
#endif

#if HEART_COHERENCY_WAR
#include <sys/cpu.h>
#endif

#define PENDING(port) (PCI_INW(&(port)->ioc3->sio_ir) & port->ienb)

/* default to 4k buffers */

#ifdef IOC3_1K_BUFFERS

#define RING_BUF_SIZE 1024
#define IOC3_BUF_SIZE_BIT 0
#define PROD_CONS_MASK PROD_CONS_PTR_1K

#else

#define RING_BUF_SIZE 4096
#define IOC3_BUF_SIZE_BIT SBBR_L_SIZE
#define PROD_CONS_MASK PROD_CONS_PTR_4K

#endif

#define TOTAL_RING_BUF_SIZE (RING_BUF_SIZE * 4)

#if NBPP < TOTAL_RING_BUF_SIZE
#include <sys/pfdat.h>
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

#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	NEWA(ptr,n)	(ptr = kmem_zalloc((n)*sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

/* local port info for the IOC3 serial ports. This contains as its
 * first member the global sio port private data.
 */
typedef struct ioc3port {
    sioport_t		sioport;	/* must be first struct entry! */

    vertex_hdl_t	conn_vhdl;	/* vhdl to use for pciio requests */
    vertex_hdl_t	port_vhdl;	/* vhdl for the serial port */

    /* base piomap addr of the ioc3 board this port is on
     * and associated serial, uart and superio register maps
     */
    ioc3_mem_t	       *ioc3;
    ioc3_sregs_t       *serial;
    ioc3_uregs_t       *uart;
    ioc3_sioregs_t     *sio;

    /* ring buffer page for this port */
    caddr_t		ring_buf_k0;	/* ring buffer location in K0 space */

    /* rings for this port (port A or port B) */
    struct ring	       *inring;
    struct ring	       *outring;

    /* hook to port specific values for this port (port A or B) */
    struct hooks       *hooks;

    int			flags;

    /* cache of DCD/CTS bits last received */
    char modem_bits;

    /* various rx/tx parameters */
    int baud;
    int tx_lowat;
    int rx_timeout;

    /* copy of notification bits */
    int notify;

    /* shadow copies of various registers so we don't need to PIO
     * read them constantly
     */
    ioc3reg_t ienb; /* enabled interrupts */

    /* When this value is flushed to hardware for the console port, we
     * need to grab the symmon console lock via CONS_HW_LOCK for debug
     * kernels. This is to ensure that the soft copy of the register
     * here and the hardware copy don't get out of sync, as well as to
     * ensure that the kernel doesn't touch the console while symmon
     * is using it.
     */
    ioc3reg_t sscr;

    ioc3reg_t tx_prod;
    ioc3reg_t rx_cons;

    /* back pointer to ioc3 soft area */
    void *ioc3_soft;
} ioc3port_t;

#if DEBUG
int next_saveport;
#define MAXSAVEPORT 256
ioc3port_t *saveport[MAXSAVEPORT];
#endif

#if DEBUG
static int debuglock_ospl; /* For CONS_HW_LOCK macro */
#endif

static dev_t cons_devs[2];

/* TX low water mark. We need to notify the driver whenever TX is getting
 * close to empty so it can refill the TX buffer and keep things going.
 * Let's assume that if we interrupt 1 ms before the TX goes idle, we'll
 * have no trouble getting in more chars in time (I certainly hope so)
 */
#define TX_LOWAT_LATENCY 1000
#define TX_LOWAT_HZ (1000000 / TX_LOWAT_LATENCY)
#define TX_LOWAT_CHARS(baud) (baud / 10 / TX_LOWAT_HZ)

/* flags per port */
#define INPUT_HIGH	0x01
#define DCD_ON		0x02
#define LOWAT_WRITTEN	0x04
#define READ_ABORTED	0x08
#define TX_DISABLED	0x10

/* get local port type from global sio port type */
#define LPORT(port) ((ioc3port_t*)(port))

/* get global port from local port type */
#define GPORT(port) ((sioport_t*)(port))

/* Since ports A and B have different register offsets and bitmasks
 * for everything, we'll store those that we need in tables so we
 * don't have to be constantly checking if we're dealing with port A
 * or B
 */
struct hooks {
    ioc3reg_t intr_delta_dcd;
    ioc3reg_t intr_delta_cts;
    ioc3reg_t intr_tx_mt;
    ioc3reg_t intr_rx_timer;
    ioc3reg_t intr_rx_high;
    ioc3reg_t intr_tx_explicit;
    ioc3reg_t intr_clear;
    ioc3reg_t intr_all;
    char rs422_select_pin;
};

static struct hooks hooks_array[2] =
{
    /* values for port A */
    {
	SIO_IR_SA_DELTA_DCD,
	SIO_IR_SA_DELTA_CTS,
	SIO_IR_SA_TX_MT,
	SIO_IR_SA_RX_TIMER,
	SIO_IR_SA_RX_HIGH,
	SIO_IR_SA_TX_EXPLICIT,
	(SIO_IR_SA_TX_MT | SIO_IR_SA_RX_FULL | SIO_IR_SA_RX_HIGH |
	 SIO_IR_SA_RX_TIMER | SIO_IR_SA_DELTA_DCD | SIO_IR_SA_DELTA_CTS |
	 SIO_IR_SA_INT | SIO_IR_SA_TX_EXPLICIT | SIO_IR_SA_MEMERR),
	SIO_IR_SA,
	GPPR_UARTA_MODESEL_PIN,
    },

    /* values for port B */
    {
	SIO_IR_SB_DELTA_DCD,
	SIO_IR_SB_DELTA_CTS,
	SIO_IR_SB_TX_MT,
	SIO_IR_SB_RX_TIMER,
	SIO_IR_SB_RX_HIGH,
	SIO_IR_SB_TX_EXPLICIT,
	(SIO_IR_SB_TX_MT | SIO_IR_SB_RX_FULL | SIO_IR_SB_RX_HIGH |
	 SIO_IR_SB_RX_TIMER | SIO_IR_SB_DELTA_DCD | SIO_IR_SB_DELTA_CTS |
	 SIO_IR_SB_INT | SIO_IR_SB_TX_EXPLICIT | SIO_IR_SB_MEMERR),
	SIO_IR_SB,
	GPPR_UARTB_MODESEL_PIN,
    }
};

/* macros to get into the port hooks. require a variable called
 * hooks set to port->hooks
 */
#define H_INTR_TX_MT	hooks->intr_tx_mt
#define H_INTR_RX_TIMER hooks->intr_rx_timer
#define H_INTR_RX_HIGH	hooks->intr_rx_high
#define H_INTR_TX_EXPLICIT hooks->intr_tx_explicit
#define H_INTR_CLEAR	hooks->intr_clear
#define H_INTR_DELTA_DCD hooks->intr_delta_dcd
#define H_INTR_DELTA_CTS hooks->intr_delta_cts
#define H_INTR_ALL	hooks->intr_all
#define H_RS422		hooks->rs422_select_pin

/* a ring buffer entry */
struct ring_entry {
    union {
	struct {
	    __uint32_t alldata;
	    __uint32_t allsc;
	} all;
	struct {
	    char data[4];	/* data bytes */
	    char sc[4];		/* status/control */
	} s;
    } u;
};

/* test the valid bits in any of the 4 sc chars using "allsc" member */
#define RING_ANY_VALID \
	((__uint32_t)(RXSB_MODEM_VALID | RXSB_DATA_VALID) * 0x01010101)

#define ring_sc u.s.sc
#define ring_data u.s.data
#define ring_allsc u.all.allsc

/* number of entries per ring buffer. */
#define ENTRIES_PER_RING (RING_BUF_SIZE / (int)sizeof(struct ring_entry))

/* an individual ring */
struct ring {
    struct ring_entry entries[ENTRIES_PER_RING];
};

/* the whole enchilada */
struct ring_buffer {
    struct ring TX_A;
    struct ring RX_A;
    struct ring TX_B;
    struct ring RX_B;
};

/* get a ring from a port struct */
#define RING(port, which) \
    &(((struct ring_buffer *)((port)->ring_buf_k0))->which)

/* local functions: */
static int ioc3_open		(sioport_t *port);
static int ioc3_config		(sioport_t *port, int baud, int byte_size,
				 int stop_bits, int parenb, int parodd);
static int ioc3_enable_hfc	(sioport_t *port, int enable);
static int ioc3_set_extclk	(sioport_t *port, int clock_factor);

    /* data transmission */
static int do_ioc3_write	(sioport_t *port, char *buf, int len);
static int ioc3_write		(sioport_t *port, char *buf, int len);
static int ioc3_sync_write	(sioport_t *port, char *buf, int len);
static void ioc3_wrflush	(sioport_t *port);
static int ioc3_break		(sioport_t *port, int brk);
static int ioc3_enable_tx	(sioport_t *port, int enb);

    /* data reception */
static int ioc3_read		(sioport_t *port, char *buf, int len);

    /* event notification */
static int ioc3_notification	(sioport_t *port, int mask, int on);
static int ioc3_rx_timeout	(sioport_t *port, int timeout);

    /* modem control */
static int ioc3_set_DTR		(sioport_t *port, int dtr);
static int ioc3_set_RTS		(sioport_t *port, int rts);
static int ioc3_query_DCD	(sioport_t *port);
static int ioc3_query_CTS	(sioport_t *port);

    /* output mode */
static int ioc3_set_proto	(sioport_t *port, enum sio_proto proto);

    /* user mapped driver support */
static int ioc3_get_mapid	(sioport_t *port, void *arg);
static int ioc3_map		(sioport_t *port, vhandl_t *vt, off_t off);
static void ioc3_unmap		(sioport_t *port);
static int ioc3_set_sscr	(sioport_t *port, int arg, int flag);

static struct serial_calldown ioc3_calldown = {
    ioc3_open,
    ioc3_config,
    ioc3_enable_hfc,
    ioc3_set_extclk,
    ioc3_write,
    ioc3_sync_write,
    ioc3_wrflush,	/* du flush */
    ioc3_break,
    ioc3_enable_tx,
    ioc3_read,
    ioc3_notification,
    ioc3_rx_timeout,
    ioc3_set_DTR,
    ioc3_set_RTS,
    ioc3_query_DCD,
    ioc3_query_CTS,
    ioc3_set_proto,
    ioc3_get_mapid,
    ioc3_map,
    ioc3_unmap,
    ioc3_set_sscr
};

/* baud rate stuff */
#define SET_BAUD(p, b) set_baud_ti(p, b)
static int set_baud_ti(ioc3port_t *, int);

#ifdef DEBUG
/* performance characterization logging */
#define DEBUGINC(x,i) stats.x += i

static struct {

    /* ports present */
    uint ports;
    /* ports killed */
    uint killed;

    /* interrupt counts */
    uint total_intr;
    uint port_a_intr;
    uint port_b_intr;
    uint ddcd_intr;
    uint dcts_intr;
    uint rx_timer_intr;
    uint rx_high_intr;
    uint explicit_intr;
    uint mt_intr;
    uint mt_lowat_intr;

    /* write characteristics */
    uint write_bytes;
    uint write_cnt;
    uint wrote_bytes;
    uint tx_buf_used;
    uint tx_buf_cnt;
    uint tx_pio_cnt;

    /* read characteristics */
    uint read_bytes;
    uint read_cnt;
    uint drain;
    uint drainwait;
    uint resetdma;
    uint read_ddcd;
    uint rx_overrun;
    uint parity;
    uint framing;
    uint brk;
    uint red_bytes;
    uint rx_buf_used;
    uint rx_buf_cnt;

    /* errors */
    uint dma_lost;
    uint read_aborted;
    uint read_aborted_detected;
} stats;

#else
#define DEBUGINC(x,i)
#endif

/* infinite loop detection.
 */
#define MAXITER 1000000
#define SPIN(cond, success) \
{ \
	 int spiniter = 0; \
	 success = 1; \
	 while(cond) { \
		 spiniter++; \
		 if (spiniter > MAXITER) { \
			 success = 0; \
			 break; \
		 } \
	 } \
}

#ifdef DEBUG
void
ioc3_clear_stats(void)
{
    bzero(&stats, sizeof(stats));
}
void
ioc3_pr_stats(void)
{
    qprintf("%u ports attached, %u detached\n", stats.ports, stats.killed);
    qprintf("%u interrupts: %u port a, %u port b\n",
	    stats.total_intr, stats.port_a_intr, stats.port_b_intr);
    qprintf("%u ddcd, %u dcts, %u rx_timer, %u rx_high\n",
	    stats.ddcd_intr, stats.dcts_intr, stats.rx_timer_intr,
	    stats.rx_high_intr);
    qprintf("%u explicit, %u mt, %u mt_lowat\n",
	    stats.explicit_intr, stats.mt_intr, stats.mt_lowat_intr);

    if (stats.write_cnt > 0) {
	qprintf("average write request %u bytes (%u/%u)\n",
		stats.write_bytes / stats.write_cnt,
		stats.write_bytes, stats.write_cnt);
	qprintf("average bytes actually written %u (%u/%u)\n",
		stats.wrote_bytes / stats.write_cnt,
		stats.wrote_bytes, stats.write_cnt);
    }

    if (stats.tx_buf_cnt > 0) {
	qprintf("average tx ring buffer size used %u (%u/%u)\n",
		stats.tx_buf_used / stats.tx_buf_cnt,
		stats.tx_buf_used, stats.tx_buf_cnt);
    }

    if (stats.explicit_intr + stats.mt_intr > 0) {
	qprintf("bytes written per tx intr: %u (%u/%u)\n",
		stats.wrote_bytes / (stats.explicit_intr + stats.mt_intr),
		stats.wrote_bytes, (stats.explicit_intr + stats.mt_intr));
    }

    if (stats.explicit_intr > 0) {
	qprintf("bytes written per explicit intr: %u (%u/%u)\n",
		stats.wrote_bytes / stats.explicit_intr,
		stats.wrote_bytes, stats.explicit_intr);
    }

    if (stats.read_cnt > 0) {
	qprintf("average read request %u bytes (%u/%u)\n",
		stats.read_bytes / stats.read_cnt,
		stats.read_bytes, stats.read_cnt);
	qprintf("average bytes actually read %u (%u/%u)\n",
		stats.red_bytes / stats.read_cnt,
		stats.red_bytes, stats.read_cnt);
    }

    if (stats.rx_buf_cnt > 0) {
	qprintf("average rx ring buffer size used %u (%u/%u)\n",
		stats.rx_buf_used / stats.rx_buf_cnt,
		stats.rx_buf_used, stats.rx_buf_cnt);
    }

    if (stats.rx_timer_intr + stats.rx_high_intr > 0) {
	qprintf("bytes read per rx intr %u (%u/%u)\n",
		stats.red_bytes / (stats.rx_timer_intr + stats.rx_high_intr),
		stats.red_bytes, (stats.rx_timer_intr + stats.rx_high_intr));
    }
    qprintf("%u rx drains, %u waited for, %u resetdma\n",
	    stats.drain, stats.drainwait, stats.resetdma);
    qprintf("%u rx ddcd, %u overrun, %u parity, %u framing, %u break\n",
	    stats.read_ddcd, stats.rx_overrun, stats.parity,
	    stats.framing, stats.brk);
    qprintf("%d reads aborted, %d detected\n", stats.read_aborted,
	    stats.read_aborted_detected);
    qprintf("%d DMAs lost\n", stats.dma_lost);
}

void
idbg_ioc3dump(void *arg)
{
    if (!arg)
	ioc3_clear_stats();
    else
	ioc3_pr_stats();
}

void
idbg_prconbuf(void *port)
{
    extern void sio_prconbuf(sioport_t*);
    if (port == 0)
	port = the_console_port;
    sio_prconbuf(GPORT(port));
}
#endif

dev_t
get_cons_dev(int which)
{
    ASSERT(which == 1 || which == 2);

    return(cons_devs[which - 1]);
}

iopaddr_t
ring_dmatrans(vertex_hdl_t conn_vhdl, caddr_t vaddr)
{
    iopaddr_t	paddr = K0_TO_PHYS(vaddr);

    if (conn_vhdl != GRAPH_VERTEX_NONE)
	return pciio_dmatrans_addr (conn_vhdl, 0, paddr, TOTAL_RING_BUF_SIZE, PCIIO_DMA_A64);

    return paddr;
}

/* If interrupt routine called enable_intrs, then would need to write
 * mask_enable_intrs() routine.
 */
static void
mask_disable_intrs(ioc3port_t *port, ioc3reg_t mask)
{
    port->ienb &= ~mask;
}

static void
enable_intrs(ioc3port_t *port, ioc3reg_t mask)
{
    if ((port->ienb & mask) != mask) {
	IOC3_WRITE_IES(port->ioc3_soft, mask);
	port->ienb |= mask;
    }
}

static void
disable_intrs(ioc3port_t *port, ioc3reg_t mask)
{
    if (port->ienb & mask) {
	IOC3_WRITE_IEC(port->ioc3_soft, mask);
	port->ienb &= ~mask;
    }
}

/* service any pending interrupts on the given port */
static void
ioc3_serial_intr(intr_arg_t arg, ioc3reg_t sio_ir)
{
    ioc3port_t *port = (ioc3port_t *)arg;
    sioport_t *gp = GPORT(port);
    struct hooks *hooks = port->hooks;

    ASSERT(sio_port_islocked(gp) == 0);

    /* Possible race condition here: The TX_MT interrupt bit may be
     * cleared without the intervention of the interrupt handler,
     * e.g. by a write. If the top level interrupt handler reads a
     * TX_MT, then some other processor does a write, starting up
     * output, then we come in here, see the TX_MT and stop DMA, the
     * output started by the other processor will hang. Thus we can
     * only rely on TX_MT being legitimate if it is read while the
     * port lock is held. Therefore this bit must be ignored in the
     * passed in interrupt mask which was read by the top level
     * interrupt handler since the port lock was not held at the time
     * it was read. We can only rely on this bit being accurate if it
     * is read while the port lock is held. So we'll clear it for now,
     * and reload it later once we have the port lock.
     */
    sio_ir &= ~(H_INTR_TX_MT);

    SIO_LOCK_PORT(gp);

    dprintf(("interrupt: ir 0x%x\n", sio_ir));

    do {
	ioc3reg_t shadow;

	/* handle a DCD change */
	if (sio_ir & H_INTR_DELTA_DCD) {
	    DEBUGINC(ddcd_intr, 1);

	    /* ACK the interrupt */
	    PCI_OUTW(&port->ioc3->sio_ir, H_INTR_DELTA_DCD);

	    /* If DCD has raised, notify upper layer. Otherwise
	     * wait for a record to be posted to notify of a dropped DCD
	     */
	    CONS_HW_LOCK(IS_CONSOLE(port));
	    shadow = PCI_INW(&port->serial->shadow);
	    CONS_HW_UNLOCK(IS_CONSOLE(port));
	    
            if (port->notify & N_DDCD) {
                if (shadow & SHADOW_DCD)        /* notify upper layer of DCD */
                        UP_DDCD(gp, 1);
                else
                        port->flags |= DCD_ON;  /* flag delta DCD/no DCD */
	    }
	}

	/* handle a CTS change */
	if (sio_ir & H_INTR_DELTA_CTS) {
	    DEBUGINC(dcts_intr, 1);

	    /* ACK the interrupt */
	    PCI_OUTW(&port->ioc3->sio_ir, H_INTR_DELTA_CTS);

	    CONS_HW_LOCK(IS_CONSOLE(port));
	    shadow = PCI_INW(&port->serial->shadow);
	    CONS_HW_UNLOCK(IS_CONSOLE(port));

	    /* notify upper layer */
	    if (port->notify & N_DCTS) {
		if (shadow & SHADOW_CTS)
		    UP_DCTS(gp, 1);
		else
		    UP_DCTS(gp, 0);
	    }
	}

	/* RX timeout interrupt. Must be some data available. Put this
	 * before the check for RX_HIGH since servicing this condition
	 * may cause that condition to clear
	 */
	if (sio_ir & H_INTR_RX_TIMER) {
	    DEBUGINC(rx_timer_intr, 1);

	    /* ACK the interrupt */
	    PCI_OUTW(&port->ioc3->sio_ir, H_INTR_RX_TIMER);

	    if (port->notify & N_DATA_READY)
		UP_DATA_READY(gp);
	}

	/* RX high interrupt. Must be after RX_TIMER.
	 */
	else if (sio_ir & H_INTR_RX_HIGH) {
	    DEBUGINC(rx_high_intr, 1);

	    /* data available, notify upper layer */
	    if (port->notify & N_DATA_READY)
		UP_DATA_READY(gp);

	    /* We can't ACK this interrupt. If up_data_ready didn't
	     * cause the condition to clear, we'll have to disable
	     * the interrupt until the data is drained by the upper layer.
	     * If the read was aborted, don't disable the interrupt as
	     * this may cause us to hang indefinitely. An aborted read
	     * generally means that this interrupt hasn't been delivered
	     * to the cpu yet anyway, even though we see it as asserted 
	     * when we read the sio_ir
	     */
	    if ((sio_ir = PENDING(port)) & H_INTR_RX_HIGH) {
		if ((port->flags & READ_ABORTED) == 0) {
		    mask_disable_intrs(port, H_INTR_RX_HIGH);
		    port->flags |= INPUT_HIGH;
		}
		else
		    DEBUGINC(read_aborted_detected, 1);
	    }
	}

	/* we got a low water interrupt: notify upper layer to
	 * send more data. Must come before TX_MT since servicing
	 * this condition may cause that condition to clear.
	 */
	if (sio_ir & H_INTR_TX_EXPLICIT) {
	    DEBUGINC(explicit_intr, 1);

	    port->flags &= ~LOWAT_WRITTEN;

	    /* ACK the interrupt */
	    PCI_OUTW(&port->ioc3->sio_ir, H_INTR_TX_EXPLICIT);

	    if (port->notify & N_OUTPUT_LOWAT)
		UP_OUTPUT_LOWAT(gp);
	}

	/* handle TX_MT. Must come after TX_EXPLICIT
	 */
	else if (sio_ir & H_INTR_TX_MT) {
	    DEBUGINC(mt_intr, 1);

	    /* if the upper layer is expecting a lowat notification
	     * and we get to this point it probably means that for
	     * some reason the TX_EXPLICIT didn't work as expected
	     * (that can legitimately happen if the output buffer is
	     * filled up in just the right way). So sent the notification
	     * now.
	     */
	    if (port->notify & N_OUTPUT_LOWAT) {
		DEBUGINC(mt_lowat_intr, 1);

		if (port->notify & N_OUTPUT_LOWAT)
		    UP_OUTPUT_LOWAT(gp);

		/* we need to reload the sio_ir since the upcall may
		 * have caused another write to occur, clearing
		 * the TX_MT condition
		 */
		sio_ir = PENDING(port);
	    }

	    /* If the TX_MT condition still persists even after the upcall,
	     * we've got some work to do
	     */
	    if (sio_ir & H_INTR_TX_MT) {

		/* if we are not currently expecting DMA input, and the
		 * transmitter has just gone idle, there is no longer any
		 * reason for DMA, so disable it
		 */
		if (!(port->notify & (N_DATA_READY | N_DDCD))) {
		    ASSERT(port->sscr & SSCR_DMA_EN);
		    port->sscr &= ~SSCR_DMA_EN;
		    CONS_HW_LOCK(IS_CONSOLE(port));
		    PCI_OUTW(&port->serial->sscr, port->sscr);
		    CONS_HW_UNLOCK(IS_CONSOLE(port));
		}

		/* prevent infinite TX_MT interrupt */
		mask_disable_intrs(port, H_INTR_TX_MT);
	    }
	}

	sio_ir = PENDING(port);
    } while (sio_ir & H_INTR_ALL);

    SIO_UNLOCK_PORT(gp);

    /* Re-enable interrupts before returning from interrupt handler
     * Getting interrupted here is okay.  It'll just v() our semaphore, and
     * we'll come through the loop again.
     */
    IOC3_WRITE_IES(port->ioc3_soft, port->ienb);
}

/* baud rate setting code */
static int
set_baud_ti(ioc3port_t *port, int baud)
{
    int divisor;
    int actual_baud;
    int diff;
    int lcr, prediv;

    for (prediv = 6; prediv < 64; prediv++) {
	divisor = SER_DIVISOR(baud, SER_CLK_SPEED(prediv));
	actual_baud =
	    DIVISOR_TO_BAUD(divisor, SER_CLK_SPEED(prediv));

	diff = actual_baud - baud;
	if (diff < 0)
	    diff = -diff;

	/* if we're within 1% we've found a match
	 */
	if (diff * 100 <= actual_baud) 
	    break;
    }

    /* if the above loop completed, we didn't match
     * the baud rate.  give up.
     */
    if (prediv == 64) return 1;

    lcr = PCI_INB(&port->uart->iu_lcr);
    PCI_OUTB(&port->uart->iu_lcr, lcr | LCR_DLAB);
    PCI_OUTB(&port->uart->iu_dll, (char)divisor);
    PCI_OUTB(&port->uart->iu_dlm, (char)(divisor >> 8));
    PCI_OUTB(&port->uart->iu_scr, prediv);
    PCI_OUTB(&port->uart->iu_lcr, lcr);

    return(0);
}

/* initialize the sio and ioc3 hardware for a given port */
static int
hardware_init(ioc3port_t *port, int isconsole)
{
    ioc3reg_t sio_cr;
    struct hooks *hooks = port->hooks;

    DEBUGINC(ports, 1);

    /* Wait for any output to drain on the console port */
    if (isconsole) {
	int max = (16 * 1050)/4;	/* ~1ms/char, 16B fifo, 4 us/loop */

	while (max--) {
	    if (port->uart->iu_lsr & LSR_XMIT_BUF_EMPTY)
		break;
	    us_delay(3);
	}
    }

    /* Idle the IOC3 serial interface */
    PCI_OUTW(&port->serial->sscr, SSCR_RESET);

    /* wait until any pending bus activity for this port has
     * ceased
     */
#ifdef	OLD_NETLIST

    while(  (SSCR_RESET & PCI_INW(&port->serial->sscr)) );

#else
    do sio_cr = PCI_INW(&port->ioc3->sio_cr);
    while(!(sio_cr & SIO_CR_ARB_DIAG_IDLE) &&
	  (((sio_cr &= SIO_CR_ARB_DIAG) == SIO_CR_ARB_DIAG_TXA) ||
	   sio_cr == SIO_CR_ARB_DIAG_TXB ||
	   sio_cr == SIO_CR_ARB_DIAG_RXA ||
	   sio_cr == SIO_CR_ARB_DIAG_RXB));
#endif

    /* finish reset sequence */
    PCI_OUTW(&port->serial->sscr, 0);

    /* once RESET is done, reload cached tx_prod and rx_cons values
     * and set rings to empty by making prod == cons
     */
    port->tx_prod = PCI_INW(&port->serial->stcir) & PROD_CONS_MASK;
    PCI_OUTW(&port->serial->stpir, port->tx_prod);

    port->rx_cons = PCI_INW(&port->serial->srpir) & PROD_CONS_MASK;
    PCI_OUTW(&port->serial->srcir, port->rx_cons);

    /* disable interrupts for this 16550 */
    PCI_OUTB(&port->uart->iu_lcr, 0); /* clear DLAB */
    PCI_OUTB(&port->uart->iu_ier, 0);

    /* set the default baud */
    SET_BAUD(port, port->baud);

    /* set line control to 8 bits no parity
     */
    PCI_OUTB(&port->uart->iu_lcr, LCR_8_BITS_CHAR | LCR_1_STOP_BITS);
    
    /* enable the fifos */
    PCI_OUTB(&port->uart->iu_fcr, FCR_ENABLE_FIFO);
    /* then reset 16550 fifos */
    PCI_OUTB(&port->uart->iu_fcr,
	     FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET | FCR_XMIT_FIFO_RESET);

    /* clear modem control register */
    PCI_OUTB(&port->uart->iu_mcr, 0);

    /* echo modem control settings in shadow reg */
    PCI_OUTW(&port->serial->shadow, 0);

    /* only do this once per A/B pair */
    if (port->hooks == &hooks_array[0]) {
	__psint_t ring_pci_addr;

	/* set the DMA address */

	ring_pci_addr = ring_dmatrans(port->conn_vhdl,
				      port->ring_buf_k0);
#if _MIPS_SZPTR == 64
	PCI_OUTW(&port->ioc3->sbbr_h,
		 (ioc3reg_t)((__psunsigned_t)ring_pci_addr >> 32));
#else
	PCI_OUTW(&port->ioc3->sbbr_h, 0);
#endif
	PCI_OUTW(&port->ioc3->sbbr_l,
		 ((ioc3reg_t)(__psint_t)ring_pci_addr |
		  IOC3_BUF_SIZE_BIT));
    }

    /* set the receive timeout value to 10 msec */
    PCI_OUTW(&port->serial->srtr, SRTR_HZ / 100);

    /* set RX threshold, enable DMA */
    /* set high water mark at 3/4 of full ring */
    port->sscr = (ENTRIES_PER_RING * 3 / 4);

    /* uart experiences pauses at high baud rate reducing actual
     * throughput by 10% or so unless we enable high speed polling
     * XXX when this hardware bug is resolved we should revert to
     * normal polling speed
     */
    port->sscr |= SSCR_HIGH_SPD;

    CONS_HW_LOCK(IS_CONSOLE(port));
    PCI_OUTW(&port->serial->sscr, port->sscr);
    CONS_HW_UNLOCK(IS_CONSOLE(port));

    /* disable and clear all serial related interrupt bits */
    IOC3_WRITE_IEC(port->ioc3_soft, H_INTR_CLEAR);
    port->ienb &= ~H_INTR_CLEAR;
    PCI_OUTW(&port->ioc3->sio_ir, H_INTR_CLEAR);

    return(0);
}


/*
 * Device initialization.
 * Called at *_attach() time for each
 * IOC3 with serial ports in the system.
 * If vhdl is GRAPH_VERTEX_NONE, do not do
 * any graph related work; otherwise, it
 * is the IOC3 vertex that should be used
 * for requesting pciio services.
 */
int
ioc3_serial_attach(vertex_hdl_t conn_vhdl)
{
    /*REFERENCED*/
    graph_error_t	rc;
    ioc3_mem_t	       *ioc3_mem;
    vertex_hdl_t	port_vhdl, ioc3_vhdl;
    vertex_hdl_t	intr_dev_vhdl;
    int			isconsole;
    ioc3port_t	       *port;
    ioc3port_t	       *ports[2];
    static char	       *names[] = { "tty/1", "tty/2" };
    int			x;
    void	       *ioc3_soft;
    struct sio_lock    *lockp;
    extern void	       *the_kdbx_port;

    if (!ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_ttya) &&
	!ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_ttyb)) {
#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "%v has no serial connectors\n", conn_vhdl);
#endif
	return -1;	/* not brought out to connectors */
    }

    /*
     * get a pio mapping to the chip
     */
    ioc3_mem = (ioc3_mem_t *)pciio_piotrans_addr
	(conn_vhdl, 0, PCIIO_SPACE_WIN(0), 0, sizeof (*ioc3_mem), 0);
    ASSERT(ioc3_mem != NULL);

    /*
     * If we are attached to the console ioc3,
     * then we must be the console port.
     */
    isconsole = ioc3_is_console(conn_vhdl);
#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "%v is the console\n", conn_vhdl);
#endif

    /*
     * if the ioc3 is in dualslot mode, we want to use
     * the second slot. if there is no GUEST link, this
     * leaves conn_vhdl unchaged which is what we want.
     */
    (void)hwgraph_traverse(conn_vhdl, EDGE_LBL_GUEST, &conn_vhdl);

    /* get back pointer to the ioc3 soft area */
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    ioc3_soft = (void*)hwgraph_fastinfo_get(ioc3_vhdl);

    /*
     * create port structures for each port
     */
    NEWA(port, 2);
    ports[0] = port++;
    ports[1] = port++;

#if DEBUG
    {
	int slot = atomicAddInt(&next_saveport, 2) - 2;
	saveport[slot] = ports[0];
	saveport[slot+1] = ports[1];
	ASSERT(slot < MAXSAVEPORT);
    }
#endif

#ifdef DEBUG
    if ((caddr_t)port != (caddr_t)&(port->sioport))
	panic("sioport is not first member of ioc3port struct\n");
#endif

    /* allocate buffers and jumpstart the hardware.
     */
    for (x = 0; x < 2; x++) {
	port = ports[x];
	port->ioc3_soft = ioc3_soft;
	rc = hwgraph_path_add(conn_vhdl, names[x], &port_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	port->conn_vhdl = conn_vhdl;
	port->port_vhdl = port_vhdl;
	port->ienb	= 0;
	hwgraph_fastinfo_set(port_vhdl, (arbitrary_info_t)port);

	/* perform upper layer initialization. Create all device node
	 * types including rs422 ports
	 */
	sio_initport(GPORT(port),
		     port->port_vhdl,
		     NODE_TYPE_ALL_RS232 | NODE_TYPE_ALL_RS422 |
		     NODE_TYPE_USER,
		     isconsole && (x == 0));

	lockp = (struct sio_lock*)kmem_zalloc(sizeof(struct sio_lock), 0);
	ASSERT(lockp);

	/* XXX streams problem prevents using mutexes for the port
	 * lock. If this is fixed in the future SIO_LOCK_MUTEX should
	 * be used for the non-console case
	 */
	lockp->lock_levels[0] = 
	    (isconsole && x == 0) ? SIO_LOCK_SPL7 : SIO_LOCK_SPLHI;
	sio_init_locks(lockp);
	GPORT(port)->sio_lock = lockp;

	/* store the console devices for {1,2}/d if this is the console ioc3 */
	port->baud = 9600;
	if (isconsole) {
	    vertex_hdl_t cons_vhdl;

	    rc = hwgraph_edge_get(port_vhdl, "d", &cons_vhdl);
	    ASSERT(rc == GRAPH_SUCCESS);
	    cons_devs[x] = vhdl_to_dev(cons_vhdl);

	    if (x == 0 || x == 1) {
		int baud;
		char *env = kopt_find(x == 0 ? "dbaud" : "rbaud");
		if (env && (baud = atoi(env)) > 0)
		    port->baud = baud;
		if (x == 1) {
			if (baud)
				printf("Setting rbaud to %d\n", baud);
			the_kdbx_port = port;
		}
		
	    }
	}

	/* create inventory entries */
	device_inventory_add(port_vhdl,
			     INV_SERIAL,
			     INV_IOC3_DMA,
			     -1,0,0);
	
	/* attach the calldown hooks so upper layer can call our
	 * routines.
	 */
	port->sioport.sio_calldown = &ioc3_calldown;

	/* map in the IOC3 and SUPERIO register areas */
	port->ioc3 = ioc3_mem;
	port->sio = &ioc3_mem->sregs;
    }

    port = ports[0];
    /* port A */
    port->hooks = &hooks_array[0];

    /* get direct hooks to the serial regs and uart regs
     * for this port
     */
    port->serial = &(port->ioc3->port_a);
    port->uart = &(port->sio->uarta);

    /* If we don't already have a ring buffer,
     * set one up.
     */
    if (port->ring_buf_k0 == 0) {

#if NBPP >= TOTAL_RING_BUF_SIZE
	if ((port->ring_buf_k0 = kvpalloc(1, VM_DIRECT, 0)) == 0)
	    panic("ioc3_uart driver cannot allocate page\n");
#else
	/* we need to allocate a chunk of memory on a
	 * TOTAL_RING_BUF_SIZE boundary.
	 */
	{
	    pgno_t pfn;
	    caddr_t vaddr;
	    if ((pfn = contig_memalloc(TOTAL_RING_BUF_SIZE / NBPP,
				       TOTAL_RING_BUF_SIZE / NBPP,
				       VM_DIRECT)) == 0)
		panic("ioc3_uart driver cannot allocate page\n");
	    ASSERT(small_pfn(pfn));
	    vaddr = small_pfntova_K0(pfn);
	    (void) COLOR_VALIDATION(pfdat + pfn,
				    colorof(vaddr),
				    0, VM_DIRECT);
	    port->ring_buf_k0 = vaddr;
	}
#endif

    }
    ASSERT((((__psint_t)port->ring_buf_k0) &
	    (TOTAL_RING_BUF_SIZE-1)) == 0);
    bzero(port->ring_buf_k0, TOTAL_RING_BUF_SIZE);
    port->inring = RING(port, RX_A);
    port->outring = RING(port, TX_A);


    /* initialize the hardware for IOC3 and SIO */
    hardware_init(port,isconsole);

    /* port B */
    port = ports[1];
    port->hooks = &hooks_array[1];

    port->serial = &(port->ioc3->port_b);
    port->uart = &(port->sio->uartb);

    port->ring_buf_k0 = ports[0]->ring_buf_k0;
    port->inring = RING(port, RX_B);
    port->outring = RING(port, TX_B);

    /* initialize the hardware for IOC3 and SIO */
    hardware_init(port,0);

    if (hwgraph_edge_get(ports[0]->port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	intr_dev_vhdl = ports[0]->port_vhdl;
    }
    /* attach interrupt handler */
    ioc3_intr_connect(conn_vhdl,
		      SIO_IR_SA,
		      ioc3_serial_intr,
		      ports[0],
		      ports[0]->port_vhdl,
		      intr_dev_vhdl,
		      0);

    if (hwgraph_edge_get(ports[1]->port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	intr_dev_vhdl = ports[1]->port_vhdl;
    }
    ioc3_intr_connect(conn_vhdl,
		      SIO_IR_SB,
		      ioc3_serial_intr,
		      ports[1],
		      ports[1]->port_vhdl,
		      intr_dev_vhdl,
		      0);

    if (isconsole) {
	
	/* set up hwgraph nodes for console port */
	device_controller_num_set(ports[0]->port_vhdl, 1);
	device_controller_num_set(ports[1]->port_vhdl, 2);
	
	sio_make_hwgraph_nodes(GPORT(ports[0]));
	sio_make_hwgraph_nodes(GPORT(ports[1]));

	the_console_port = ports[0];
    }

#ifdef	DEBUG
    idbg_addfunc( "ioc3dump", idbg_ioc3dump );
#endif

    return 0;
}

/* shut down an IOC3 */
/* ARGSUSED1 */
void
ioc3_serial_kill(ioc3port_t *port)
{
    DEBUGINC(killed, 1);

    /* notify upper layer that this port is no longer usable */
    UP_DETACH(GPORT(port));

    /* clear everything in the sscr */
    CONS_HW_LOCK(IS_CONSOLE(port));
    PCI_OUTW(&port->serial->sscr, 0);
    CONS_HW_UNLOCK(IS_CONSOLE(port));
    port->sscr = 0;

#ifdef DEBUG
    /* make sure nobody gets past the lock and accesses the hardware */
    port->ioc3 = 0;
    port->sio = 0;
    port->serial = 0;
    port->uart = 0;
#endif
}

#ifdef DEBUG
int no_high_speed;
#endif

/*
 * open a port
 */
static int
ioc3_open(sioport_t *port)
{
    ioc3port_t *p = LPORT(port);
    int spin_success;

    ASSERT(L_LOCKED(port, L_OPEN));

    p->flags = 0;
    p->modem_bits = 0;

    /* pause the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN) {
	CONS_HW_LOCK(IS_CONSOLE(p));
	PCI_OUTW(&p->serial->sscr, p->sscr | SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->serial->sscr) & SSCR_PAUSE_STATE) == 0, spin_success);
	CONS_HW_UNLOCK(IS_CONSOLE(p));
	if (!spin_success)
		return(-1);
    }

    /* Reset the input fifo. If the uart received chars while the port
     * was closed and DMA is not enabled, the uart may have a bunch of
     * chars hanging around in its RX fifo which will not be discarded
     * by rclr in the upper layer. We must get rid of them here.
     */
    PCI_OUTB(&p->uart->iu_fcr,
	     FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET);


    /* set defaults */
    SET_BAUD(p, 9600);
    PCI_OUTB(&p->uart->iu_lcr, LCR_8_BITS_CHAR | LCR_1_STOP_BITS);

    /* re-enable DMA, set default threshold to intr whenever there is
     * data available.
     */
    p->sscr &= ~SSCR_RX_THRESHOLD;
    p->sscr |= 1; /* default threshold */

#ifdef DEBUG
    /* allow turning on or off of high speed from the debugger */
    if (no_high_speed)
	p->sscr &= ~SSCR_HIGH_SPD;
    else
	p->sscr |= SSCR_HIGH_SPD;
#endif

    /* plug in the new sscr. This implicitly clears the DMA_PAUSE
     * flag if it was set above
     */
    CONS_HW_LOCK(IS_CONSOLE(p));
    PCI_OUTW(&p->serial->sscr, p->sscr);
    CONS_HW_UNLOCK(IS_CONSOLE(p));

    PCI_OUTW(&p->serial->srtr, 0);

    p->tx_lowat = 1;

    dprintf(("ioc3 open successful\n"));

    return(0);
}

/*
 * config hardware
 */
static int
ioc3_config(sioport_t *port,
	    int baud,
	    int byte_size,
	    int stop_bits,
	    int parenb,
	    int parodd)
{
    ioc3port_t *p = LPORT(port);
    char lcr, sizebits;
    int spin_success;

    ASSERT(L_LOCKED(port, L_CONFIG));

    if (SET_BAUD(p, baud))
	return(1);

    switch(byte_size) {
      case 5:
	sizebits = LCR_5_BITS_CHAR;
	break;
      case 6:
	sizebits = LCR_6_BITS_CHAR;
	break;
      case 7:
	sizebits = LCR_7_BITS_CHAR;
	break;
      case 8:
	sizebits = LCR_8_BITS_CHAR;
	break;
      default:
	eprintf(("invalid byte size port 0x%x size %d\n", port, byte_size));
	return(1);
    }

    /* pause the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN) {
	CONS_HW_LOCK(IS_CONSOLE(p));
	PCI_OUTW(&p->serial->sscr, p->sscr | SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->serial->sscr) & SSCR_PAUSE_STATE) == 0, spin_success);
	CONS_HW_UNLOCK(IS_CONSOLE(p));
	if (!spin_success)
		return(-1);
    }

    /* clear relevant fields in lcr */
    lcr = PCI_INB(&p->uart->iu_lcr);
    lcr &= ~(LCR_MASK_BITS_CHAR | LCR_EVEN_PARITY | LCR_ENABLE_PARITY |
	     LCR_MASK_STOP_BITS);

    /* set byte size in lcr */
    lcr |= sizebits;

    /* set parity */
    if (parenb) {
	lcr |= LCR_ENABLE_PARITY;
	if (!parodd)
	    lcr |= LCR_EVEN_PARITY;
    }

    /* set stop bits */
    if (stop_bits)
	lcr |= LCR_2_STOP_BITS;

    PCI_OUTB(&p->uart->iu_lcr, lcr);

    dprintf(("ioc3_config: lcr bits 0x%x\n", lcr));

    /* re-enable the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN) {
	CONS_HW_LOCK(IS_CONSOLE(p));
	PCI_OUTW(&p->serial->sscr, p->sscr);
	CONS_HW_UNLOCK(IS_CONSOLE(p));
    }

    p->baud = baud;

    /* When we get within this number of ring entries of filling the
     * entire ring on TX, place an EXPLICIT intr to generate a lowat
     * notification when output has drained
     */
    p->tx_lowat = (TX_LOWAT_CHARS(baud) + 3) / 4;
    if (p->tx_lowat == 0)
	p->tx_lowat = 1;

    ioc3_rx_timeout(port, p->rx_timeout);

    return(0);
}

/*
 * Enable hardware flow control
 */
static int
ioc3_enable_hfc(sioport_t *port, int enable)
{
    ioc3port_t *p = LPORT(port);

    ASSERT(L_LOCKED(port, L_ENABLE_HFC));

    dprintf(("enable hfc port 0x%x, enb %d\n", port, enable));

    if (enable)
	p->sscr |= SSCR_HFC_EN;
    else
	p->sscr &= ~SSCR_HFC_EN;

    CONS_HW_LOCK(IS_CONSOLE(p));
    PCI_OUTW(&p->serial->sscr, p->sscr);
    CONS_HW_UNLOCK(IS_CONSOLE(p));

    return(0);
}

/*
 * set external clock
 */
/*ARGSUSED*/
static int
ioc3_set_extclk(sioport_t *port, int clock_factor)
{
    ASSERT(L_LOCKED(port, L_SET_EXTCLK));
    /* XXX still todo */

    /* only support 0 (no external clock) */
    return(clock_factor);
}

/*
 * write bytes to the hardware. Returns the number of bytes
 * actually written
 */
static int
do_ioc3_write(sioport_t *port, char *buf, int len)
{
    int prod_ptr, cons_ptr, total;
    struct ring *outring;
    struct ring_entry *entry;
    ioc3port_t *p = LPORT(port);
    struct hooks *hooks = p->hooks;
#if HEART_COHERENCY_WAR
    int old_prod_ptr;
#endif	/* HEART_COHERENCY_WAR */

    DEBUGINC(write_bytes, len);
    DEBUGINC(write_cnt, 1);

    dprintf(("write port 0x%x, len %d\n", port, len));

    ASSERT(len >= 0);

    prod_ptr = p->tx_prod;
#if HEART_COHERENCY_WAR
    old_prod_ptr = prod_ptr;
#endif	/* HEART_COHERENCY_WAR */
    cons_ptr = PCI_INW(&p->serial->stcir) & PROD_CONS_MASK;
    outring = p->outring;

    /* maintain a 1-entry red-zone. The ring buffer is full when
     * (cons - prod) % ring_size is 1. Rather than do this subtraction
     * in the body of the loop, I'll do it now.
     */
    cons_ptr =
	(cons_ptr - (int)sizeof(struct ring_entry)) & PROD_CONS_MASK;

    total = 0;
    /* stuff the bytes into the output */
    while ((prod_ptr != cons_ptr) && (len > 0)) {
	int x;

	/* go 4 bytes (one ring entry) at a time */
	entry = (struct ring_entry*) ((caddr_t)outring + prod_ptr);

	/* invalidate all entries */
	entry->ring_allsc = 0;

	/* copy in some bytes */
	for(x = 0; (x < 4) && (len > 0); x++) {
	    entry->ring_data[x] = *buf++;
	    entry->ring_sc[x] = TXCB_VALID;
	    len--;
	    total++;
	}

	DEBUGINC(tx_buf_used, x);
	DEBUGINC(tx_buf_cnt, 1);

	/* If we are within some small threshold of filling up the entire
	 * ring buffer, we must place an EXPLICIT intr here to generate
	 * a lowat interrupt in case we subsequently really do fill up
	 * the ring and the caller goes to sleep. No need to place
	 * more than one though
	 */
	if (!(p->flags & LOWAT_WRITTEN) &&
	    ((cons_ptr - prod_ptr) & PROD_CONS_MASK) <=
	    p->tx_lowat * (int)sizeof(struct ring_entry)) {
	    p->flags |= LOWAT_WRITTEN;
	    entry->ring_sc[0] |= TXCB_INT_WHEN_DONE;
	    dprintf(("write placing TX_EXPLICIT\n"));
	}

	/* go on to next entry */
	prod_ptr = (prod_ptr + (int)sizeof(struct ring_entry)) & PROD_CONS_MASK;
    }

    /* If we sent something, start DMA if necessary */
    if (total > 0 && !(p->sscr & SSCR_DMA_EN)) {
	p->sscr |= SSCR_DMA_EN;
	CONS_HW_LOCK(IS_CONSOLE(p));
	PCI_OUTW(&p->serial->sscr, p->sscr);
	CONS_HW_UNLOCK(IS_CONSOLE(p));
    }

#if HEART_COHERENCY_WAR
    if (prod_ptr >= old_prod_ptr)
	heart_dcache_wb_inval((caddr_t)outring + old_prod_ptr,
			      prod_ptr - old_prod_ptr);
    else {
	heart_dcache_wb_inval((caddr_t)outring + old_prod_ptr,
	    		      RING_BUF_SIZE - old_prod_ptr);
	heart_dcache_wb_inval((caddr_t)outring,prod_ptr);
    }
#endif	/* HEART_COHERENCY_WAR */

    /* store the new producer pointer. If TX is disabled, we stuff the
     * data into the ring buffer, but we don't actually start TX.
     */
    if (!(p->flags & TX_DISABLED)) {
	PCI_OUTW(&p->serial->stpir, prod_ptr);
	/* If we are now transmitting, enable TX_MT interrupt so we
	 * can disable DMA if necessary when the TX finishes
	 */
	if (total > 0)
	    enable_intrs(p, H_INTR_TX_MT);
    }
    p->tx_prod = prod_ptr;

    DEBUGINC(wrote_bytes, total);
    return(total);
}

/* asynchronous write */
static int
ioc3_write(sioport_t *port, char *buf, int len)
{
    ASSERT(L_LOCKED(port, L_WRITE));
    return(do_ioc3_write(port, buf, len));
}

/* synchronous write */
static int
ioc3_sync_write(sioport_t *port, char *buf, int len)
{
    int bytes;

    ASSERT(sio_port_islocked(port));
    bytes = do_ioc3_write(port, buf, len);

    /* don't allow the system to hang if XOFF is in force */
    if (len > 0 && bytes == 0 && (LPORT(port)->flags & TX_DISABLED))
	ioc3_enable_tx(port, 1);

    return(bytes);
}

/* write flush */
static void
ioc3_wrflush(sioport_t *port)
{
    ioc3port_t *p = LPORT(port);

    ASSERT(sio_port_islocked(port));

    /* We can't flush if TX is disabled due to XOFF. */
    if (!(PCI_INW(&p->ioc3->sio_ir) & SIO_IR_SA_TX_MT) &&
	(p->flags & TX_DISABLED))
	ioc3_enable_tx(port, 1);

    /* spin waiting for TX_MT to assert only if DMA is enabled.  If we
     * are panicking and one of the other processors is already in
     * symmon, DMA will be disabled and TX_MT will never be asserted.
     * There may also be legitimate cases in the kernel where DMA is
     * disabled and we won't flush correctly here.
     */

    while ((PCI_INW(&p->serial->sscr) & (SSCR_DMA_EN | SSCR_PAUSE_STATE)) == SSCR_DMA_EN &&
	   !(PCI_INW(&p->ioc3->sio_ir) & SIO_IR_SA_TX_MT)) {
	DELAY(5);
    }
}

/*
 * Set or clear break condition on output
 */
static int
ioc3_break(sioport_t *port, int brk)
{
    ioc3port_t *p = LPORT(port);
    char lcr;
    int spin_success;

    ASSERT(L_LOCKED(port, L_BREAK));

    /* pause the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN) {
	CONS_HW_LOCK(IS_CONSOLE(p));
	PCI_OUTW(&p->serial->sscr, p->sscr | SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->serial->sscr) & SSCR_PAUSE_STATE) == 0, spin_success);
	CONS_HW_UNLOCK(IS_CONSOLE(p));
	if (!spin_success)
		return(-1);
    }

    lcr = PCI_INB(&p->uart->iu_lcr);
    if (brk)
	/* set break */
	PCI_OUTB(&p->uart->iu_lcr, lcr | LCR_SET_BREAK);
    else
	/* clear break */
	PCI_OUTB(&p->uart->iu_lcr, lcr & ~LCR_SET_BREAK);

    /* re-enable the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN) {
	CONS_HW_LOCK(IS_CONSOLE(p));
	PCI_OUTW(&p->serial->sscr, p->sscr);
	CONS_HW_UNLOCK(IS_CONSOLE(p));
    }

    dprintf(("break port 0x%x, brk %d\n", port, brk));

    return(0);
}

static int
ioc3_enable_tx(sioport_t *port, int enb)
{
    ioc3port_t *p = LPORT(port);
    struct hooks *hooks = p->hooks;
    int spin_success;

    ASSERT(L_LOCKED(port, L_ENABLE_TX));

    /* if we're already in the desired state, we're done */
    if ((enb && !(p->flags & TX_DISABLED)) ||
	(!enb && (p->flags & TX_DISABLED)))
	return(0);

    /* pause DMA */
    CONS_HW_LOCK(IS_CONSOLE(p));
    if (p->sscr & SSCR_DMA_EN) {
	PCI_OUTW(&p->serial->sscr, p->sscr | SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->serial->sscr) & SSCR_PAUSE_STATE) == 0, spin_success);
	if (!spin_success)
		return(-1);
    }

    if (enb) {
	p->flags &= ~TX_DISABLED;
	PCI_OUTW(&p->serial->stpir, p->tx_prod);
	enable_intrs(p, H_INTR_TX_MT);
    }
    else {
	ioc3reg_t txcons = PCI_INW(&p->serial->stcir) & PROD_CONS_MASK;
	p->flags |= TX_DISABLED;
	disable_intrs(p, H_INTR_TX_MT);

	/* Only move the transmit producer pointer back if the
	 * transmitter is not already empty, otherwise we'll be
	 * generating a bogus entry
	 */
	if (txcons != p->tx_prod)
	    PCI_OUTW(&p->serial->stpir,
		     (txcons + (int)sizeof(struct ring_entry)) & PROD_CONS_MASK);
    }

    /* re-enable the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN)
	PCI_OUTW(&p->serial->sscr, p->sscr);

    CONS_HW_UNLOCK(IS_CONSOLE(p));
    return(0);
}

/*
 * read in bytes from the hardware. Return the number of bytes
 * actually read
 */
static int
ioc3_read(sioport_t *port, char *buf, int len)
{
    int prod_ptr, cons_ptr, total, x, spin_success;
    struct ring *inring;
    ioc3port_t *p = LPORT(port);
    struct hooks *hooks = p->hooks;
#ifdef	HEART_COHERENCY_WAR
    int old_cons_ptr;
#endif	/* HEART_COHERENCY_WAR */

    ASSERT(L_LOCKED(port, L_READ));

    dprintf(("read port 0x%x, len %d\n", port, len));

    DEBUGINC(read_bytes, len);
    DEBUGINC(read_cnt, 1);

    ASSERT(len >= 0);

    /* There is a nasty timing issue in the IOC3. When the RX_TIMER
     * expires or the RX_HIGH condition arises, we take an interrupt.
     * At some point while servicing the interrupt, we read bytes from
     * the ring buffer and re-arm the RX_TIMER. However the RX_TIMER is
     * not started until the first byte is received *after* it is armed,
     * and any bytes pending in the RX construction buffers are not drained
     * to memory until either there are 4 bytes available or the RX_TIMER
     * expires. This leads to a potential situation where data is left
     * in the construction buffers forever because 1 to 3 bytes were received
     * after the interrupt was generated but before the RX_TIMER was re-armed.
     * At that point as long as no subsequent bytes are received the
     * timer will never be started and the bytes will remain in the
     * construction buffer forever. The solution is to execute a DRAIN
     * command after rearming the timer. This way any bytes received before
     * the DRAIN will be drained to memory, and any bytes received after
     * the DRAIN will start the TIMER and be drained when it expires.
     * Luckily, this only needs to be done when the DMA buffer is empty
     * since there is no requirement that this function return all
     * available data as long as it returns some.
     */
    /* rearm the timer */
    PCI_OUTW(&p->serial->srcir, p->rx_cons | SRCIR_ARM);

    prod_ptr = PCI_INW(&p->serial->srpir) & PROD_CONS_MASK;
    cons_ptr = p->rx_cons;
#ifdef	HEART_COHERENCY_WAR
    old_cons_ptr = cons_ptr;
#endif	/* HEART_COHERENCY_WAR */

    if (prod_ptr == cons_ptr) {
	int reset_dma = 0;

	/* input buffer appears empty, do a flush. */

	/* DMA must be enabled for this to work. */
	if (!(p->sscr & SSCR_DMA_EN)) {
	    p->sscr |= SSCR_DMA_EN;
	    reset_dma = 1;
	}

	/* potential race condition: we must reload the srpir after
	 * issuing the drain command, otherwise we could think the RX
	 * buffer is empty, then take a very long interrupt, and when
	 * we come back it's full and we wait forever for the drain to
	 * complete.
	 */
	CONS_HW_LOCK(IS_CONSOLE(p));
	PCI_OUTW(&p->serial->sscr, p->sscr | SSCR_RX_DRAIN);
	CONS_HW_UNLOCK(IS_CONSOLE(p));
	prod_ptr = PCI_INW(&p->serial->srpir) & PROD_CONS_MASK;

	DEBUGINC(drain, 1);

	/* We must not wait for the DRAIN to complete unless there are
	 * at least 8 bytes (2 ring entries) available to receive the data
	 * otherwise the DRAIN will never complete and we'll deadlock here.
	 * In fact, to make things easier, I'll just ignore the flush if
	 * there is any data at all now available
	 */
	if (prod_ptr == cons_ptr) {
	    DEBUGINC(drainwait, 1);
	    CONS_HW_LOCK(IS_CONSOLE(p));
	    SPIN(PCI_INW(&p->serial->sscr) & SSCR_RX_DRAIN, spin_success);
	    CONS_HW_UNLOCK(IS_CONSOLE(p));
	    if (!spin_success)
		    return(-1);

	    /* SIGH. We have to reload the prod_ptr *again* since
	     * the drain may have caused it to change
	     */
	    prod_ptr = PCI_INW(&p->serial->srpir) & PROD_CONS_MASK;
	}

	if (reset_dma) {
	    DEBUGINC(resetdma, 1);
	    p->sscr &= ~SSCR_DMA_EN;
	    CONS_HW_LOCK(IS_CONSOLE(p));
	    PCI_OUTW(&p->serial->sscr, p->sscr);
	    CONS_HW_UNLOCK(IS_CONSOLE(p));
	}
    }
    inring = p->inring;

    p->flags &= ~READ_ABORTED;

    total = 0;
    /* grab bytes from the hardware */
    while(prod_ptr != cons_ptr && len > 0) {
	struct ring_entry *entry;

	entry = (struct ring_entry*) ((caddr_t)inring + cons_ptr);

	/* according to the producer pointer, this ring entry
	 * must contain some data. But if the PIO happened faster
	 * than the DMA, the data may not be available yet, so let's
	 * wait until it arrives
	 */
	if ((((volatile struct ring_entry*)entry)->ring_allsc &
	     RING_ANY_VALID) == 0) {

	    struct ring_entry *entry1;
	    int cons_ptr1 = (cons_ptr +
			     (int)sizeof(struct ring_entry)) %
				 PROD_CONS_MASK;

	    /* if this entry has no valid bits but there is a valid
	     * next entry and it contains valid bits, the DMA
	     * transaction for this entry has been lost. We have hit a
	     * hardware bug, so log an error and skip this entry.
	     */
	    entry1 = (struct ring_entry*) ((caddr_t)inring + cons_ptr1);
	    
	    if ((PCI_INW(&p->serial->srpir) & PROD_CONS_MASK) != cons_ptr1 &&
		(((volatile struct ring_entry*)entry1)->ring_allsc &
		 RING_ANY_VALID)) {
		DEBUGINC(dma_lost, 1);
#if DEBUG
		ioc3_trigger = (int *)&entry1;
		debug("ioc3 dropped dma\n");
#endif
	    }
	    else {

		/* indicate the read is aborted so we don't disable
		 * the interrupt thinking that the consumer is
		 * congested
		 */
		p->flags |= READ_ABORTED;
		
		DEBUGINC(read_aborted, 1);
		len = 0;
		break;
	    }
	}

	/* load the bytes/status out of the ring entry */
	for(x = 0; x < 4 && len > 0; x++) {
	    char *sc = &(entry->ring_sc[x]);

	    /* check for change in modem state or overrun */
	    if (*sc & RXSB_MODEM_VALID) {
		if (p->notify & N_DDCD) {

		    /* notify upper layer if DCD dropped */
		    if ((p->flags & DCD_ON) && !(*sc & RXSB_DCD)) {

			/* if we have already copied some data, return
			 * it. We'll pick up the carrier drop on the next
			 * pass. That way we don't throw away the data
			 * that has already been copied back to the caller's
			 * buffer
			 */
			if (total > 0) {
			    len = 0;
			    break;
			}

			p->flags &= ~DCD_ON;

			/* turn off this notification so the carrier
			 * drop protocol won't see it again when it
			 * does a read
			 */
			*sc &= ~RXSB_MODEM_VALID;

#ifdef	HEART_COHERENCY_WAR
		 	if ((cons_ptr & ~0x7f) > old_cons_ptr) {
			    heart_write_dcache_wb_inval(
				(caddr_t)inring + old_cons_ptr,
			    	(cons_ptr & ~0x7f) - (old_cons_ptr));
    			} 
			else if (cons_ptr < old_cons_ptr) {
			    heart_write_dcache_wb_inval(
				(caddr_t)inring + old_cons_ptr,
	    			RING_BUF_SIZE - old_cons_ptr);
			    heart_write_dcache_wb_inval(
				(caddr_t)inring,(cons_ptr & ~0x7f));
    			}
#endif	/* HEART_COHERENCY_WAR */

			/* to keep things consistent, we need to update
			 * the consumer pointer so the next reader won't
			 * come in and try to read the same ring entries
			 * again. This must be done here before the call
			 * to UP_DDCD since UP_DDCD may do a recursive
			 * read!
			 */
			if ((entry->ring_allsc & RING_ANY_VALID) == 0)
			    cons_ptr =
				(cons_ptr + (int)sizeof(struct ring_entry)) &
				    PROD_CONS_MASK;

			PCI_OUTW(&p->serial->srcir, cons_ptr);
			p->rx_cons = cons_ptr;

			/* notify upper layer of carrier drop */
			if (p->notify & N_DDCD)
			    UP_DDCD(port, 0);

			DEBUGINC(read_ddcd, 1);

			/* If we had any data to return, we would have
			 * returned it above
			 */
			return(0);
		    }
		}

		/* notify upper layer that an input overrun occurred */
		if ((*sc & RXSB_OVERRUN) && (p->notify & N_OVERRUN_ERROR)) {
		    DEBUGINC(rx_overrun, 1);
		    UP_NCS(port, NCS_OVERRUN);
		}

		/* Don't look at this byte again */
		*sc &= ~RXSB_MODEM_VALID;
	    }

	    /* check for valid data or RX errors */
	    if (*sc & RXSB_DATA_VALID) {
		if ((*sc & (RXSB_PAR_ERR | RXSB_FRAME_ERR | RXSB_BREAK)) &&
		    (p->notify &
		     (N_PARITY_ERROR | N_FRAMING_ERROR | N_BREAK))) {

		    /* There is an error condition on the next byte. If
		     * we have already transferred some bytes, we'll stop
		     * here. Otherwise if this is the first byte to be read,
		     * we'll just transfer it alone after notifying the
		     * upper layer of its status
		     */
		    if (total > 0) {
			len = 0;
			break;
		    }
		    else {
			if ((*sc & RXSB_PAR_ERR) &&
			    (p->notify & N_PARITY_ERROR)) {
			    DEBUGINC(parity, 1);
			    UP_NCS(port, NCS_PARITY);
			}

			if ((*sc & RXSB_FRAME_ERR) &&
			    (p->notify & N_FRAMING_ERROR)) {
			    DEBUGINC(framing, 1);
			    UP_NCS(port, NCS_FRAMING);
			}

			if ((*sc & RXSB_BREAK) &&
			    (p->notify & N_BREAK)) {
			    DEBUGINC(brk, 1);
			    UP_NCS(port, NCS_BREAK);
			}
			len = 1;
		    }
		}

		*sc &= ~RXSB_DATA_VALID;
		*buf++ = entry->ring_data[x];
		len--;
		total++;
	    }
	}

	DEBUGINC(rx_buf_used, x);
	DEBUGINC(rx_buf_cnt, 1);

	/* if we used up this entry entirely, go on to the next one,
	 * otherwise we must have run out of buffer space, so
	 * leave the consumer pointer here for the next read in case
	 * there are still unread bytes in this entry
	 */
	if ((entry->ring_allsc & RING_ANY_VALID) == 0)
	    cons_ptr = (cons_ptr + (int)sizeof(struct ring_entry)) &
		PROD_CONS_MASK;
    }

#ifdef	HEART_COHERENCY_WAR
    if ((cons_ptr & ~0x7f) > old_cons_ptr) {
	heart_write_dcache_wb_inval((caddr_t)inring + old_cons_ptr,
			      (cons_ptr & ~0x7f) - old_cons_ptr);
    }
    else if (cons_ptr < old_cons_ptr) {
	heart_write_dcache_wb_inval((caddr_t)inring + old_cons_ptr,
	    		      RING_BUF_SIZE - old_cons_ptr);
	heart_write_dcache_wb_inval((caddr_t)inring,(cons_ptr & ~0x7f));
    }
#endif	/* HEART_COHERENCY_WAR */

    /* update consumer pointer and rearm RX timer interrupt */
    PCI_OUTW(&p->serial->srcir, cons_ptr);
    p->rx_cons = cons_ptr;

    /* If we have now dipped below the RX high water mark and we have
     * RX_HIGH interrupt turned off, we can now turn it back on again
     */
    if ((p->flags & INPUT_HIGH) &&
	(((prod_ptr - cons_ptr) & PROD_CONS_MASK) <
	 ((p->sscr & SSCR_RX_THRESHOLD) << PROD_CONS_PTR_OFF))) {
	p->flags &= ~INPUT_HIGH;
	enable_intrs(p, H_INTR_RX_HIGH);
    }

    DEBUGINC(red_bytes, total);

    return(total);
}

/*
 * modify event notification
 */
static int
ioc3_notification(sioport_t *port, int mask, int on)
{
    ioc3port_t *p = LPORT(port);
    struct hooks *hooks = p->hooks;
    ioc3reg_t intrbits, sscrbits;

    ASSERT(L_LOCKED(port, L_NOTIFICATION));
    ASSERT(mask);

    intrbits = sscrbits = 0;

    if (mask & N_DATA_READY)
	intrbits |= (H_INTR_RX_TIMER | H_INTR_RX_HIGH);
    if (mask & N_OUTPUT_LOWAT)
	intrbits |= H_INTR_TX_EXPLICIT;
    if (mask & N_DDCD) {
	intrbits |= H_INTR_DELTA_DCD;
	sscrbits |= SSCR_RX_RING_DCD;
    }
    if (mask & N_DCTS)
	intrbits |= H_INTR_DELTA_CTS;

    if (on) {
	enable_intrs(p, intrbits);
	p->notify |= mask;
	p->sscr |= sscrbits;
    }
    else {
	disable_intrs(p, intrbits);
	p->notify &= ~mask;
	p->sscr &= ~sscrbits;
    }

    /* We require DMA if either DATA_READY or DDCD notification is
     * currently requested. If neither of these is requested and
     * there is currently no TX in progress, DMA may be disabled
     */
    if (p->notify & (N_DATA_READY | N_DDCD))
	p->sscr |= SSCR_DMA_EN;
    else if (!(p->ienb & H_INTR_TX_MT))
	p->sscr &= ~SSCR_DMA_EN;

    CONS_HW_LOCK(IS_CONSOLE(p));
    PCI_OUTW(&p->serial->sscr, p->sscr);
    CONS_HW_UNLOCK(IS_CONSOLE(p));

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
static int
ioc3_rx_timeout(sioport_t *port, int timeout)
{
    int threshold;
    ioc3port_t *p = LPORT(port);

    ASSERT(L_LOCKED(port, L_RX_TIMEOUT));

    p->rx_timeout = timeout;

    /* timeout is in ticks. Let's figure out how many chars we
     * can receive at the current baud rate in that interval
     * and set the RX threshold to that amount. There are 4 chars
     * per ring entry, so we'll divide the number of chars that will
     * arrive in timeout by 4.
     */
    threshold = timeout * p->baud / 10 / HZ / 4;
    if (threshold == 0)
	threshold = 1; /* otherwise we'll intr all the time! */

    if ((unsigned)threshold > (unsigned)SSCR_RX_THRESHOLD)
	    return(1);

    p->sscr &= ~SSCR_RX_THRESHOLD;
    p->sscr |= threshold;

    CONS_HW_LOCK(IS_CONSOLE(p));
    PCI_OUTW(&p->serial->sscr, p->sscr);
    CONS_HW_UNLOCK(IS_CONSOLE(p));

    /* Now set the RX timeout to the given value */
    timeout = timeout * SRTR_HZ / HZ;
    if (timeout > SRTR_CNT)
	timeout = SRTR_CNT;

    PCI_OUTW(&p->serial->srtr, timeout);

    return(0);
}

static int
set_DTRRTS(sioport_t *port, int val, int mask1, int mask2)
{
    ioc3port_t *p = LPORT(port);
    char mcr;
    ioc3reg_t shadow;
    int spin_success;

    /* XXX need lock for pretty much this entire routine. Makes
     * me nervous to hold it for so long. IF we crash or hit
     * a breakpoint in here, we're hosed.
     */
    CONS_HW_LOCK(IS_CONSOLE(p));

    /* pause the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN) {
	PCI_OUTW(&p->serial->sscr, p->sscr | SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->serial->sscr) & SSCR_PAUSE_STATE) == 0, spin_success);
	if (!spin_success)
		return(-1);
    }

    shadow = PCI_INW(&p->serial->shadow);

    /* set new value */
    mcr = PCI_INB(&p->uart->iu_mcr);
    if (val) {
	mcr |= mask1;
	shadow |= mask2;
    }
    else {
	mcr &= ~mask1;
	shadow &= ~mask2;
    }

    PCI_OUTB(&p->uart->iu_mcr, mcr);

    PCI_OUTW(&p->serial->shadow, shadow);

    /* re-enable the DMA interface if necessary */
    if (p->sscr & SSCR_DMA_EN)
	PCI_OUTW(&p->serial->sscr, p->sscr);

    CONS_HW_UNLOCK(IS_CONSOLE(p));
    return(0);
}

static int
ioc3_set_DTR(sioport_t *port, int dtr)
{
    ASSERT(L_LOCKED(port, L_SET_DTR));

    dprintf(("set dtr port 0x%x, dtr %d\n", port, dtr));
    return(set_DTRRTS(port, dtr, MCR_DTR, SHADOW_DTR));
}

static int
ioc3_set_RTS(sioport_t *port, int rts)
{
    ASSERT(L_LOCKED(port, L_SET_RTS));

    dprintf(("set rts port 0x%x, rts %d\n", port, rts));
    return(set_DTRRTS(port, rts, MCR_RTS, SHADOW_RTS));
}

static int
ioc3_query_DCD(sioport_t *port)
{
    ioc3port_t *p = LPORT(port);
    ioc3reg_t shadow;

    ASSERT(L_LOCKED(port, L_QUERY_DCD));

    dprintf(("get dcd port 0x%x\n", port));

    CONS_HW_LOCK(IS_CONSOLE(p));
    shadow = PCI_INW(&p->serial->shadow);
    CONS_HW_UNLOCK(IS_CONSOLE(p));

    return(shadow & SHADOW_DCD);
}

static int
ioc3_query_CTS(sioport_t *port)
{
    ioc3port_t *p = LPORT(port);
    ioc3reg_t shadow;

    ASSERT(L_LOCKED(port, L_QUERY_CTS));

    dprintf(("get cts port 0x%x\n", port));

    CONS_HW_LOCK(IS_CONSOLE(p));
    shadow = PCI_INW(&p->serial->shadow);
    CONS_HW_UNLOCK(IS_CONSOLE(p));

    return(shadow & SHADOW_CTS);
}

static int
ioc3_set_proto(sioport_t *port, enum sio_proto proto)
{
    ioc3port_t *p = LPORT(port);
    struct hooks *hooks = p->hooks;

    ASSERT(L_LOCKED(port, L_SET_PROTOCOL));

    switch(proto) {
      case PROTO_RS232:
	/* clear the appropriate GIO pin */
	PCI_OUTW((&p->ioc3->gppr_0 + H_RS422), 0);
	break;

      case PROTO_RS422:
	/* set the appropriate GIO pin */
	PCI_OUTW((&p->ioc3->gppr_0 + H_RS422), 1);
	break;

      default:
	return(1);
    }

    return(0);
}

#define IS_PORT_A(p) ((p)->hooks == &hooks_array[0])

static int
ioc3_get_mapid(sioport_t *port, void *arg)
{
    struct ioc3_mapid mapid;

    ASSERT(sio_port_islocked(port));

    if (arg) {
	mapid.port = 1 - IS_PORT_A(LPORT(port));
#ifdef IOC3_1K_BUFFERS
	mapid.size = 1024;
#else
	mapid.size = 4096;
#endif
	if (copyout(&mapid, arg, sizeof(mapid)))
	    return(EFAULT);
	return(0);
    }
    else
	return(USIO_MAP_IOC3);
}

static int
ioc3_map(sioport_t *port, vhandl_t *vt, off_t off)
{
    caddr_t mapaddr;
    size_t len;
    ioc3port_t *p = LPORT(port);

    ASSERT(sio_port_islocked(port));

    switch(off) {
      case USIO_MAP_IOC3_DMABUF:
	mapaddr = p->ring_buf_k0;
	len = TOTAL_RING_BUF_SIZE;
	break;
      case USIO_MAP_IOC3_REG:
	mapaddr = (caddr_t)p->ioc3 + 
	    (IS_PORT_A(p) ? IOC3_SSCR_A_P : IOC3_SSCR_B_P);
	len = IOC3_ALIAS_PAGE_SIZE;
	break;
      default:
	return(EINVAL);
    }

    /* make sure *all* interrupts are disabled */
    disable_intrs(p, ~0);

    return(v_mapphys(vt, mapaddr, len));
}

static void
ioc3_unmap(sioport_t *port)
{
    ioc3port_t *p = LPORT(port);

    ASSERT(sio_port_islocked(port));

    /* disable DMA and resync soft copy of sscr with hardware */
    p->sscr &= ~SSCR_DMA_EN;
    CONS_HW_LOCK(IS_CONSOLE(p));
    PCI_OUTW(&p->serial->sscr, p->sscr);
    CONS_HW_UNLOCK(IS_CONSOLE(p));

    /* usio can modify the hardware, so resync hardware state with
     * soft copies of registers.
     */
    p->tx_prod = PCI_INW(&p->serial->stcir) & PROD_CONS_MASK;
    PCI_OUTW(&p->serial->stpir, p->tx_prod);
    p->rx_cons = PCI_INW(&p->serial->srpir) & PROD_CONS_MASK;
    PCI_OUTW(&p->serial->srcir, p->rx_cons);
}

static int
ioc3_set_sscr(sioport_t *port, int arg, int flag)
{
    ioc3port_t *p = LPORT(port);

    if ( flag ) { /* reset arg bits in p->sscr */
	p->sscr &= ~arg;
    } else { /* set bits in p->sscr */
	p->sscr |= arg;
    }
    CONS_HW_LOCK(IS_CONSOLE(p));
    PCI_OUTW(&p->serial->sscr, p->sscr);
    CONS_HW_UNLOCK(IS_CONSOLE(p));
    return(p->sscr);
}

extern ioc3_mem_t *console_mem;		/* in ioc3.c */

/*
 * allow write to the console early, i.e. before hwgraph is setup
 */
int
ioc3_console_write(char *buf, unsigned long n, unsigned long *cnt)
{
        struct ioc3_serialregs  *port_a;
        struct ioc3_uartregs    *uarta;
        ioc3reg_t               sscr;

        port_a = (struct ioc3_serialregs *)&console_mem->port_a;

	/* we have no lock here, which means another cpu could be
	 * fiddling with the sscr at the same time. So repeat until we
	 * get the desired result. We want to ensure that either
	 * SSCR_DMA_EN is clear or if it is set, SSCR_PAUSE_STATE is
	 * set as well.
	 */
	sscr = PCI_INW(&port_a->sscr);
        while ((sscr & (SSCR_DMA_EN | SSCR_PAUSE_STATE)) == SSCR_DMA_EN) {
                PCI_OUTW(&port_a->sscr, sscr | SSCR_DMA_PAUSE);

		/* Break out of this loop if SSCR_PAUSE_STATE comes
		 * on, or if someone else turns off SSCR_DMA_EN or
		 * SSCR_DMA_PAUSE.  In the latter case, we loop around
		 * again in the outer loop and try the pause again.
		 */
                while (((sscr = PCI_INW(&port_a->sscr))
			& (SSCR_DMA_EN | SSCR_DMA_PAUSE | SSCR_PAUSE_STATE))
		       == (SSCR_DMA_EN | SSCR_DMA_PAUSE))
                        ;
        }

        *cnt = n;
        uarta = (struct ioc3_uartregs *)&console_mem->sregs.uarta;
        while (n > 0) {
                while ((PCI_INB(&uarta->iu_lsr) & LSR_XMIT_BUF_EMPTY) == 0)
                        ;
                PCI_OUTB(&uarta->iu_thr, *buf);
                n--;
                buf++;
        }

        return 0;
}

/*
 * poll for control-A from the console before hwgraph is setup, may cause
 * lose of character.  this routine should only be defined in the UART
 * driver that controls the console
 */
void
console_debug_poll(void)
{
        struct ioc3_serialregs  *port_a;
        struct ioc3_uartregs    *uarta;
        ioc3reg_t               sscr;

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
	if (console_mem == NULL)
	    return;
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */

        port_a = (struct ioc3_serialregs *)&console_mem->port_a;
        sscr = PCI_INW(&port_a->sscr);
        if ((sscr & SSCR_DMA_EN) && !(sscr & SSCR_PAUSE_STATE)) {
                PCI_OUTW(&port_a->sscr, sscr | SSCR_DMA_PAUSE);
                while ((PCI_INW(&port_a->sscr) & SSCR_PAUSE_STATE) == 0)
                        ;
        }

        uarta = (struct ioc3_uartregs *)&console_mem->sregs.uarta;
        if (PCI_INB(&uarta->iu_lsr) & LSR_DATA_READY) {
		if (PCI_INB(&uarta->iu_rbr) == '\001')	/* control-A */
		debug("ring");
	}
}
