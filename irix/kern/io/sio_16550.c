#if IP22 || IP30 || IOC3_PIO_MODE
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
 * module implements the ns16550
 */


#ifdef DPRINTF
#define d_printf(x) printf x
#else
#define d_printf(x)
#endif

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

#include <sys/types.h>
#include <ksys/serialio.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/ns16550.h>
#include <sys/cmn_err.h>
#include <sys/mips_addrspace.h>
#include <sys/kmem.h>

#include <sys/iograph.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/invent.h>

#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	NEWA(ptr,n)	(ptr = kmem_zalloc((n)*sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))
#define	DELA(ptr,n)	(kmem_free(ptr, (n)*sizeof (*(ptr))))

#if IOC3_PIO_MODE
#define IS_DU				1
#define NUM_PORTS			2
#endif

/* misc definitions */
#ifdef SERIAL_CONFIG_CNTRL		/* for SuperIO only */
#define CONFIG_LOOPBACK		0x01
#define CONFIG_PORT0_MIDI	0x02
#define CONFIG_PORT1_MIDI	0x04
#define CONFIG_DISABLE_HFC	0x08
#define CONFIG_DISABLE_CTS	0x10
#endif	/* SERIAL_CONFIG_CNTRL */

/* local port info for the 16550 serial ports. This contains as its
 * first member the global sio port private data.
 */
typedef struct ns16550port {
    sioport_t		sioport; 	/* must be first struct entry! */
    vertex_hdl_t	port_vhdl;	/* vertex for this port */
    ioc3_mem_t         *ioc3;
    uart_reg_t	       *port_addr;	/* cached pio address */
    int			txchars;	/* # of available spaces in the xmit FIFO
					   since the last LSR_XMIT_BUF_EMPTY */
    int			notify;		/* copy of notification flags */
    int			last_lsr;	/* copy of last lsr read from hardware */
} ns16550port_t;

static dev_t cons_devs[2];

/* get local port type from global sio port type */
#define LPORT(port) ((ns16550port_t*)(port))

/* get global port from local port type */
#define GPORT(port) ((sioport_t*)(port))

#ifdef IS_DU
extern int kdebugbreak;
#endif

#if DEBUG
void			idbg_prconbuf(void *port);
#endif
static void		ns16550_set550(ns16550port_t *dp);

int			ns16550_attach(vertex_hdl_t conn_vhdl);

static void		ns16550_handle_intr(intr_arg_t arg);
void			ns16550_poll(void);

/* local functions: */
static int		ns16550_open(sioport_t *port);
static int		ns16550_config(sioport_t *port, int baud, int byte_size, int stop_bits, int parenb, int parodd);
static int		ns16550_enable_hfc(sioport_t *port, int enable);
static int		ns16550_set_extclk(sioport_t *port, int clock_factor);

/* data transmission */
static int		ns16550_write(sioport_t *port, char *buf, int len);
static void		ns16550_wrflush(sioport_t *port);
static int		ns16550_break(sioport_t *port, int brk);

/* data reception */
static int		ns16550_read(sioport_t *port, char *buf, int len);

/* event notification */
static int		ns16550_notification(sioport_t *port, int mask, int on);
static int		ns16550_rx_timeout(sioport_t *port, int timeout);

/* modem control */
static void		set_DTR_RTS(sioport_t *port, int val, int mask);
static int		ns16550_set_DTR(sioport_t *port, int dtr);
static int		ns16550_set_RTS(sioport_t *port, int rts);
static int		ns16550_query_DCD(sioport_t *port);
static int		ns16550_query_CTS(sioport_t *port);

    /* output mode */
static int ns16550_set_proto(sioport_t *port, enum sio_proto proto);


static struct serial_calldown ns16550_calldown = {
    ns16550_open,
    ns16550_config,
    ns16550_enable_hfc,
    ns16550_set_extclk,
    ns16550_write,
    ns16550_write,	/* du write, for now same as regular write */
    ns16550_wrflush,
    ns16550_break,
    ns16550_read,
    ns16550_notification,
    ns16550_rx_timeout,
    ns16550_set_DTR,
    ns16550_set_RTS,
    ns16550_query_DCD,
    ns16550_query_CTS,
    ns16550_set_proto,
};

#if DEBUG
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

static void 
ns16550_set550(ns16550port_t *dp) 
{
    volatile uart_reg_t *addr;
    uart_reg_t lcr;

    addr = dp->port_addr;

    /*
    * Reset all interrupts for soft restart.
    */
    RD_REG(addr, INTR_ID_REG);
    RD_REG(addr, LINE_STATUS_REG);
    RD_REG(addr, MODEM_STATUS_REG);

    WR_REG(addr, FIFO_CNTRL_REG, FCR_ENABLE_FIFO);        /* enable FIFOs */
    WR_REG(addr, FIFO_CNTRL_REG,
	FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET | FCR_XMIT_FIFO_RESET);

    /* on TI part, set clock predivisor */
    lcr = RD_REG(addr, LINE_CNTRL_REG);
    WR_REG(addr, LINE_CNTRL_REG, lcr | LCR_DLAB);
    
    /* enable interrupt pin */
    WR_REG(addr, MODEM_CNTRL_REG, MCR_ENABLE_IRQ);
    
    WR_REG(addr, SCRATCH_PAD_REG, SER_PREDIVISOR * 2);
    WR_REG(addr, LINE_CNTRL_REG, lcr);
}

/*
 * Boot time initialization of this module
 */
int
ns16550_attach(vertex_hdl_t conn_vhdl)
{
    /*REFERENCED*/
    graph_error_t	rc;
    vertex_hdl_t	ioc3_vhdl;
    void	       *ioc3_soft;
    ioc3_mem_t	       *ioc3_mem;
    vertex_hdl_t	port_vhdl;
    vertex_hdl_t	intr_dev_vhdl;
    ns16550port_t      *ports[NUM_PORTS];
    ns16550port_t      *port;
    char		name[32];
    int			x;
    pciio_piomap_t	map;
#if IS_DU
    int			isconsole;
#endif

#if IOC3_PIO_MODE
    if (!ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_ttya) &&
	!ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_ttyb))
	return -1;	/* not brought out to connectors */
#endif

    /*
     * get a pio mapping to the chip
     */
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    ioc3_soft = (void*)hwgraph_fastinfo_get(ioc3_vhdl);
    ASSERT(ioc3_soft != NULL);
    ioc3_mem = ioc3_mem_ptr(ioc3_soft);
    ASSERT(ioc3_mem != NULL);

#if IS_DU
    /*
     * If we are attached to the console ioc3,
     * then we must be the console port.
     */
    isconsole = ioc3_is_console(conn_vhdl);
#endif

    /*
     * if the ioc3 is in dualslot mode, we want to use
     * the second slot. if there is no GUEST link, this
     * leaves conn_vhdl unchaged which is what we want.
     */
    (void)hwgraph_traverse(conn_vhdl, EDGE_LBL_GUEST, &conn_vhdl);

#if DEBUG
    if (&(((ns16550port_t *)0)->sioport) != 0)
	panic("sio_16550: sioport must be first element of ns16550port_t");
#endif

    /*
     * create port structures for each port
     */
    NEWA(port, NUM_PORTS);
    for (x=0; x<NUM_PORTS; ++x)
	ports[x] = port++;

    /*
     * find the port addresses.
     */
#ifdef IOC3_PIO_MODE
    ports[0]->port_addr = (uart_reg_t *) &(ioc3_mem->sregs.uarta);
#else
    ports[0]->port_addr = (uart_reg_t *)SERIAL_PORT0_CNTRL;
#endif
    ports[0]->ioc3 = ioc3_mem;

#if NUM_PORTS > 1
#ifdef IOC3_PIO_MODE
    ports[1]->port_addr = (uart_reg_t *) &(ioc3_mem->sregs.uartb);
#else
#ifdef SERIAL_PORT1_CNTRL
    ports[1]->port_addr = (uart_reg_t *)SERIAL_PORT1_CNTRL;
#endif
#endif
    ports[1]->ioc3 = ioc3_mem;
#endif

    /*
     * build port verticies, and hook up port pointers.
     */
    for (x=0; x<NUM_PORTS; ++x) {
	sprintf(name, "tty/%d", x+1);
	rc = hwgraph_path_add(conn_vhdl, name, &port_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	ports[x]->port_vhdl = port_vhdl;
	hwgraph_fastinfo_set(port_vhdl, (arbitrary_info_t)ports[x]);

	/* create inventory entries */
	device_inventory_add(port_vhdl,
			     INV_SERIAL,
			     INV_IOC3_PIO,
			     -1,0,0);
    }

    /*
     * arrange for us to get handed our interrupts.
     */
    if (hwgraph_edge_get(ports[0]->port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	intr_dev_vhdl = ports[0]->port_vhdl;
    }
    ioc3_intr_connect(conn_vhdl,
		      SIO_IR_SA_INT,
		      (ioc3_intr_func_t)ns16550_handle_intr,
		      ports[0],
		      ports[0]->port_vhdl,
		      intr_dev_vhdl,
		      0);
#if NUM_PORTS > 1
    if (hwgraph_edge_get(ports[1]->port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	intr_dev_vhdl = ports[1]->port_vhdl;
    }
    ioc3_intr_connect(conn_vhdl,
		      SIO_IR_SB_INT,
		      (ioc3_intr_func_t)ns16550_handle_intr,
		      ports[1],
		      ports[1]->port_vhdl,
		      intr_dev_vhdl,
		      0);
#endif

#if IP22 && !IOC3_PIO_MODE
    setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, ns16550_poll, 0);
#endif

#ifdef SERIAL_CONFIG_CNTRL	/* for IP22 SuperIO only */
    /* program uart configuration register */
    *(volatile uart_reg_t *)SERIAL_CONFIG_CNTRL =
	CONFIG_DISABLE_HFC | CONFIG_DISABLE_CTS;
#endif

#ifdef IOC3_PIO_MODE
    {
	ioc3_mem_t     *ioc3 = (ioc3_mem_t *)ioc3_mem;
	ioc3reg_t	sio_cr;
	
	/* reset the IOC3 */
	PCI_OUTW(&ioc3->port_a.sscr, SSCR_RESET);
	PCI_OUTW(&ioc3->port_b.sscr, SSCR_RESET);
	
	/* wait until any pending bus activity for this port has
	 * ceased
	 */

	do sio_cr = PCI_INW(&ioc3->sio_cr);
	while(!(sio_cr & SIO_CR_ARB_DIAG_IDLE) &&
	      (((sio_cr &= SIO_CR_ARB_DIAG) == SIO_CR_ARB_DIAG_TXA) ||
	       sio_cr == SIO_CR_ARB_DIAG_TXB ||
	       sio_cr == SIO_CR_ARB_DIAG_RXA ||
	       sio_cr == SIO_CR_ARB_DIAG_RXB));
	
	/* finish reset sequence */
	PCI_OUTW(&ioc3->port_a.sscr, 0);
	PCI_OUTW(&ioc3->port_b.sscr, 0);

	/* enable superio pass-thru interrupts */
	PCI_OUTW(&ioc3->sio_ies, (SIO_IR_SA_INT | SIO_IR_SB_INT));
    }
#endif

    for(x = 0; x < NUM_PORTS; x++) {

	port = ports[x];
	/* perform upper layer initialization */
	sio_initport(GPORT(port), port->port_vhdl, NODE_TYPE_ALL,
		     isconsole && x == 0);
	
	/* store the console devices for {1,2}/d if this is the console ioc3 */
	if (isconsole) {
	    vertex_hdl_t cons_vhdl;

	    rc = hwgraph_edge_get(port->port_vhdl, "d", &cons_vhdl);
	    ASSERT(rc == GRAPH_SUCCESS);
	    cons_devs[x] = vhdl_to_dev(cons_vhdl);
	}

	/* attach the calldown hooks so upper layer can call our
	 * routines.
	 */
	port->sioport.sio_calldown = &ns16550_calldown;
	port->last_lsr = -1;
	ns16550_set550(port);
    }

#if IS_DU
    if (isconsole) {
	
	/* set up hwgraph nodes for console port */
	device_controller_num_set(ports[0]->port_vhdl, 1);
	device_controller_num_set(ports[1]->port_vhdl, 2);
	
	sio_make_hwgraph_nodes(GPORT(ports[0]));
	sio_make_hwgraph_nodes(GPORT(ports[1]));

	the_console_port = ports[0];
    }
#endif

    return 0;
}

static void
ns16550_handle_intr(intr_arg_t arg)
{
    ns16550port_t	*dp = (ns16550port_t *)arg;
    volatile uart_reg_t *addr;
    uchar_t  	 	iir_val;
    uchar_t		msr_val, lsr_val;
    sioport_t		*port = GPORT(dp);
    
    ASSERT(sio_port_islocked(port) == 0);
    LOCK_PORT(port);

    addr = dp->port_addr;

    iir_val = RD_REG(addr, INTR_ID_REG);
#ifdef INTCLR_CNTRL && !IOC3_PIO_MODE
    /* clear int at 0x1f490000 (special PAL) */
    *(volatile uchar_t *)INTCLR_CNTRL;
#endif
    
    /* no interrupt pending */
    if (iir_val & IIR_NO_INTR) {
	UNLOCK_PORT(port);
    /* enable superio pass-thru interrupts */
    PCI_OUTW(&dp->ioc3->sio_ies, (SIO_IR_SA_INT | SIO_IR_SB_INT));
	return;
    }

    lsr_val = RD_REG(addr, LINE_STATUS_REG);
    msr_val = RD_REG(addr, MODEM_STATUS_REG);

#ifdef  DEBUG2
    cmn_err(CE_WARN, "intr: Port = %#x, IIR = %#x, LSR = %#x, MSR = %#x",
	port, iir_val, lsr_val, msr_val);
#endif
    
    if ((lsr_val & LSR_DATA_READY) && (dp->notify & N_DATA_READY))
	UP_DATA_READY(port);

    if ((lsr_val & LSR_XMIT_BUF_EMPTY) && (dp->notify & N_OUTPUT_LOWAT))
	UP_OUTPUT_LOWAT(port);
	
    /* These upcalls share an interrupt, so we need to check dp->notify
     * to see if they have been requested
     */
    if ((dp->notify & N_DCTS) && (msr_val & MSR_DELTA_CTS))
	UP_DCTS(port, msr_val & MSR_CTS);

    if ((dp->notify & N_DDCD) && (msr_val & MSR_DELTA_DCD))
	UP_DDCD(port, msr_val & MSR_DCD);
    UNLOCK_PORT(port);

    /* enable superio pass-thru interrupts */
    PCI_OUTW(&dp->ioc3->sio_ies, (SIO_IR_SA_INT | SIO_IR_SB_INT));

}

void
ns16550_poll(void)
{
    if (the_console_port)
	ns16550_handle_intr(the_console_port);
}

/*
 * open a port
 */
/* ARGSUSED */
static int
ns16550_open(sioport_t *port)
{
    ASSERT(L_LOCKED(port, L_OPEN));

    return(ns16550_config(port, 9600, 8, 0, 0, 0));
}

/*
 * config hardware
 */
static int
ns16550_config(sioport_t *port,
	     int baud,
	     int byte_size,
	     int stop_bits,
	     int parenb,
	     int parodd)
{
    ushort_t temp;
    volatile uart_reg_t *addr;
    ns16550port_t *dp = LPORT(port);
    int actual_baud, diff, divisor;

    addr = dp->port_addr;

    ASSERT(L_LOCKED(port, L_CONFIG));
    
    /* Verify hardware can support requested baud rate.  We must be
     * within 5% of the requested baud to make the cut. (1/2 bit time
     * out of 10 data bits) Probably we should cut it down to 2.5% just
     * to be safe
     */
    divisor = SER_DIVISOR(baud, SER_CLK_SPEED(SER_PREDIVISOR));
    actual_baud = DIVISOR_TO_BAUD(divisor, SER_CLK_SPEED(SER_PREDIVISOR));
    
    diff = actual_baud - baud;
    if (diff < 0)
	diff = -diff;
    if (diff * 200 / 5 > actual_baud)
	return(1);

    /* set number of data bits */
    temp = RD_REG(addr, LINE_CNTRL_REG);
    temp &= ~LCR_MASK_BITS_CHAR;
    switch (byte_size) {
      case 5:
	temp |= LCR_5_BITS_CHAR;
	break;
	
      case 6:
	temp |= LCR_6_BITS_CHAR;
	break;
	
      case 7:
	temp |= LCR_7_BITS_CHAR;
	break;
	
      case 8:
	temp |= LCR_8_BITS_CHAR;
	break;
    }
    
    temp &= ~LCR_MASK_STOP_BITS;
    if (stop_bits)
	temp |= LCR_2_STOP_BITS;
    else
	temp |= LCR_1_STOP_BITS;
    
    /* set parity and type (odd/even) */
    if (parenb) {
	temp |= LCR_ENABLE_PARITY;
	if (!parodd)
	    temp |= LCR_EVEN_PARITY;
	else
	    temp &= ~LCR_EVEN_PARITY;
    }
    else
	temp &= ~LCR_ENABLE_PARITY;
    
    /*
     * set baud rate divisor registers based 
     * on value in baud rate table 
     *
     * First, enable read/write of baud rate 
     * divisor latches then write out LS byte,
     * write out MS byte,
     * then disable read/write of baud rate 
     * divisor latches 
     */
    
    WR_REG(addr, LINE_CNTRL_REG, temp | LCR_DLAB); 
    
    WR_REG(addr, DIVISOR_LATCH_LSB_REG, (char)divisor);
    WR_REG(addr, DIVISOR_LATCH_MSB_REG, (char)(divisor >> 8));

    WR_REG(addr, LINE_CNTRL_REG, temp);
    
    /*
     * set temp to value of Interrupt Enable 
     * register with the first four
     * bits set to zero (i.e. disabled) 
     */
    temp = RD_REG(addr, INTR_ENABLE_REG);
    temp &= ~(IER_ENABLE_LINE_STATUS_INTR | IER_ENABLE_RCV_INTR | 
	      IER_ENABLE_XMIT_INTR | IIR_MODEM_STATUS_INTR);
    
    /* output this info to the IER */
    WR_REG(addr, INTR_ENABLE_REG, temp);
    return(0);
}

/*
 * Enable hardware flow control
 */
/* ARGSUSED */
static int
ns16550_enable_hfc(sioport_t *port, int enable)
{
    ASSERT(L_LOCKED(port, L_ENABLE_HFC));
    return(1);
}

/*
 * set external clock mode
 */
/*ARGSUSED*/
static int
ns16550_set_extclk(sioport_t *port, int clock_factor)
{
    ASSERT(L_LOCKED(port, L_SET_EXTCLK));
    /* only allow 0 (no external clock) */
    return(clock_factor);
}


/*
 * write bytes to the hardware. Returns the number of bytes
 * actually written
 */
static int
ns16550_write(sioport_t *port, char *buf, int len)
{
    ns16550port_t *dp = LPORT(port);
    uchar_t lsr;
    volatile uart_reg_t *addr;
    int olen = len;

    ASSERT(L_LOCKED(port, L_WRITE));
    addr = dp->port_addr;

    lsr = RD_REG(addr, LINE_STATUS_REG);

    /*
     * dp->txchars should only be reset right after we detect a
     * LSR_XMIT_BUF_EMPTY
     */
    if (lsr & LSR_XMIT_BUF_EMPTY)
	dp->txchars = XMIT_FIFO_SIZE;

    while(dp->txchars && len) {
	dp->txchars--;
	len--;
	WR_REG(addr, XMIT_BUF_REG, *buf++);
    }

    if (!IS_CONSOLE(dp))
    	d_printf(("wr p 0x%x len %d(%d)\n", port, olen, olen - len));

    return olen - len;
}

/*ARGSUSED*/
static void
ns16550_wrflush(sioport_t *port)
{
	return;			/* small fifo, no need to flush */
}

/*
 * Set or clear break condition on output
 */
static int
ns16550_break(sioport_t *port, int brk)
{
    ns16550port_t *dp = LPORT(port);
    volatile uart_reg_t *addr;
    uchar_t lcr_val;
    
    ASSERT(L_LOCKED(port, L_BREAK));

    addr = dp->port_addr;
    lcr_val = RD_REG(addr, LINE_CNTRL_REG);
    
#ifdef  DEBUG2
    cmn_err(CE_WARN,
	    "break: LCR = %x, start_break = %x", lcr_val, start_break);
#endif
    
    if (brk)
	lcr_val |= LCR_SET_BREAK;
    else
	lcr_val &= ~LCR_SET_BREAK;
    
    WR_REG(addr, LINE_CNTRL_REG, lcr_val);
    return(0);
}

/*
 * read in bytes from the hardware. Return the number of bytes
 * actually read
 */
static int
ns16550_read(sioport_t *port, char *buf, int len)
{
    ns16550port_t *dp = LPORT(port);
    int total;
    volatile uart_reg_t *addr;
    uchar_t           lsr_val, err_bits;

    ASSERT(L_LOCKED(port, L_READ));

    addr = dp->port_addr;

    /* If we saw an error condition or break on the previous read but
     * we temporarily ignored it because we had already copied some data
     * back to the caller, we'll need to return that error or break byte
     * now. Only problem is the lsr gets cleared when it is read. So
     * if we read the lsr again it will not show an error or break. So
     * we'll need to look at the saved value of the lsr instead.
     */
    lsr_val = RD_REG(addr, LINE_STATUS_REG);
    if (dp->last_lsr != -1) {
	err_bits = dp->last_lsr;
	dp->last_lsr = -1;
    }
    else
	err_bits = lsr_val;
	
    total = 0;
    while((lsr_val & LSR_DATA_READY) && len > 0) {
	/* read possible error status */
	if ((dp->notify & (N_ALL_ERRORS | N_BREAK)) &&
	    (err_bits & (LSR_FRAMING_ERR | LSR_OVERRUN_ERR |
			LSR_PARITY_ERR | LSR_BREAK)))
	    {
	    
	    /* if we've already copied some bytes, stop here.
	     * Otherwise, just copy one more byte and that's it
	     */
	    if (total > 0) {
		dp->last_lsr = lsr_val;
		break;
	    }
	    else {
		len = 1;
		if ((dp->notify & N_BREAK) &&
		    (err_bits & LSR_BREAK)) {
#ifdef IS_DU
		    if (kdebug && kdebugbreak && !console_is_tport &&
			IS_CONSOLE(dp)) {
			UNLOCK_PORT(port);
			debug("ring");
			LOCK_PORT(port);
		    }
		    else
#endif
		    UP_NCS(port, NCS_BREAK);
		}
		
		if ((dp->notify & N_FRAMING_ERROR) &&
		    (err_bits & LSR_FRAMING_ERR))
		    UP_NCS(port, NCS_FRAMING);
		
		if ((dp->notify & N_OVERRUN_ERROR) &&
		    (err_bits & LSR_OVERRUN_ERR))
		    UP_NCS(port, NCS_OVERRUN);
		
		if ((dp->notify & N_PARITY_ERROR) &&
		    (err_bits & LSR_PARITY_ERR))
		    UP_NCS(port, NCS_PARITY);

		if ( err_bits & LSR_RCV_FIFO_ERR )
		    cmn_err(CE_WARN, "!Error in FIFO on serial port 0x%x",
			    dp);
	    }
	}
	
	/* get the char */
	*buf = RD_REG(addr, RCV_BUF_REG);

	/* check for debugger */
	if (kdebug && !console_is_tport && IS_CONSOLE(dp) &&
	    *buf == DEBUG_CHAR) {
		UNLOCK_PORT(port);
		debug("ring");
		LOCK_PORT(port);
	}
	else {
	    buf++;
	    len--;
	    total++;
	}
	err_bits = lsr_val = RD_REG(addr, LINE_STATUS_REG);
    }

    d_printf(("rd p 0x%x len %d(%d)\n", port, len, total));
    return(total);
}

static int
ns16550_notification(sioport_t *port, int mask, int on)
{
    ns16550port_t *dp = LPORT(port);
    volatile uart_reg_t *addr;
    uchar_t ier_val;
    int intrbits;

    ASSERT(L_LOCKED(port, L_NOTIFICATION));
    ASSERT(mask);

    intrbits = 0;
    if (mask & N_DATA_READY)
	intrbits |= IER_ENABLE_RCV_INTR;
    if (mask & N_OUTPUT_LOWAT)
	intrbits |= IER_ENABLE_XMIT_INTR;

    addr = dp->port_addr;

    ier_val = RD_REG(addr, INTR_ENABLE_REG);
    if (on) {
	dp->notify |= mask;
	ier_val |= intrbits;

	/* If any of the line status notifications was requested,
	 * we must turn on the line status interrupt
	 */
	if (dp->notify & (N_ALL_ERRORS | N_BREAK))
	    ier_val |= IER_ENABLE_LINE_STATUS_INTR;

	/* If any of the modem status notifications was requested,
	 * we must turn on the modem status interrupt
	 */
	if (dp->notify & (N_DDCD | N_DCTS))
	    ier_val |= IER_ENABLE_MODEM_STATUS_INTR;
    }
    else {
	dp->notify &= ~mask;
	ier_val &= ~intrbits;

	/* If none of the line status notifications is requested,
	 * turn off the line status interrupt
	 */
	if (!(dp->notify & (N_ALL_ERRORS | N_BREAK)))
	    ier_val &= ~IER_ENABLE_LINE_STATUS_INTR;

	/* If none of the modem status notifications is requested,
	 * turn off the modem status interrupt
	 */
	if (!(dp->notify & (N_DDCD | N_DCTS)))
	    ier_val &= ~IER_ENABLE_MODEM_STATUS_INTR;
    }

    WR_REG(addr, INTR_ENABLE_REG, ier_val);

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
ns16550_rx_timeout(sioport_t *port, int timeout)
{
    ASSERT(L_LOCKED(port, L_RX_TIMEOUT));

    return(1);
}

static void
set_DTR_RTS(sioport_t *port, int val, int mask)
{
    ns16550port_t *dp = LPORT(port);
    volatile uart_reg_t *addr;
    uchar_t mcr_val;

    addr = dp->port_addr;

    mcr_val = RD_REG(addr, MODEM_CNTRL_REG);
    if (val)
	mcr_val |= mask;
    else
	mcr_val &= ~mask;
    WR_REG(addr, MODEM_CNTRL_REG, mcr_val);
}

static int
ns16550_set_DTR(sioport_t *port, int dtr)
{
    ASSERT(L_LOCKED(port, L_SET_DTR));
    set_DTR_RTS(port, dtr, MCR_DTR);
    return(0);
}

static int
ns16550_set_RTS(sioport_t *port, int rts)
{
    ASSERT(L_LOCKED(port, L_SET_RTS));
    set_DTR_RTS(port, rts, MCR_RTS);
    return(0);
}

static int
ns16550_query_DCD(sioport_t *port)
{
    ns16550port_t *dp = LPORT(port);
    volatile uart_reg_t *addr;

    ASSERT(L_LOCKED(port, L_QUERY_DCD));
    addr = dp->port_addr;

    return(RD_REG(addr, MODEM_STATUS_REG) & MSR_DCD);
}

static int
ns16550_query_CTS(sioport_t *port)
{
    ns16550port_t *dp = LPORT(port);
    volatile uart_reg_t *addr;

    ASSERT(L_LOCKED(port, L_QUERY_CTS));
    addr = dp->port_addr;

    return(RD_REG(addr, MODEM_STATUS_REG) & MSR_CTS);
}

/* ARGSUSED */
static int
ns16550_set_proto(sioport_t *port, enum sio_proto proto)
{
    ASSERT(L_LOCKED(port, L_SET_PROTOCOL));
    /* only support rs232 */
    return(proto != PROTO_RS232);
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
        sscr = PCI_INW(&port_a->sscr);
        if ((sscr & SSCR_DMA_EN) && !(sscr & SSCR_PAUSE_STATE)) {
                PCI_OUTW(&port_a->sscr, sscr | SSCR_DMA_PAUSE);
                while ((PCI_INW(&port_a->sscr) & SSCR_PAUSE_STATE) == 0)
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

#endif /* IP22 || IP30 || IOC3_PIO_MODE */
