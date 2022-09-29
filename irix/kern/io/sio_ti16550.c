/*
 * Lower level module for the modular serial i/o driver.
 * This module implements the  TI16550C
 *
 * Copyright 1995, Silicon Graphics, Inc.  All rights reserved.
 */

#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/file.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/stream.h"
#include "sys/strids.h"
#include "sys/strmp.h"
#include "sys/stropts.h"
#include "sys/kmem.h"
#include "sys/stty_ld.h"
#include "sys/types.h"
#include "ksys/serialio.h"
#include "sys/termio.h"
#include "sys/stropts.h"
#include "sys/stty_ld.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/termio.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/mips_addrspace.h"
#include "sys/proc.h"
#include "sys/sema.h"
#include "sys/ddi.h"
#include "sys/capability.h"
#include "sys/uart16550.h"
#include "sys/pfdat.h"
#include "sys/invent.h"
#include "sys/PCI/pciio.h"
#include <sys/idbgentry.h>	/* for qprintf(), idbg_addfunc() */

#ifdef DEBUG_RX
#define dr_printf(x)				printf x
#else
#define dr_printf(x)				
#endif

#ifdef DEBUG_TX
#define dt_printf(x)				printf x
#else
#define dt_printf(x)				
#endif

#define	CNTRL_A		'\001'
#ifdef DEBUG
  #define dprintf(x)	printf x
  #ifndef DEBUG_CHAR
  #define DEBUG_CHAR	CNTRL_A
  #endif
#else /* !DEBUG */
  #define DEBUG_CHAR	CNTRL_A
  #define dprintf(x)				
#endif /* DEBUG */

int du_console;
int	zduart_rdebug = 0;
static int def_console_baud  = 9600;   /* default speed for the console */
static int def_pdbxport_baud = 9600;   /* default speed for the pdbxport */

int console_device_attached = 0;
extern console_is_tport;

/*
 *-----------------------------------------------------------------------------
 * sio_drivermask = Those bits in MACE mask register that pertain to driver.
 * sio_currntmask = Those bits in MACE mask register that pertain to driver and
 *		    the values of these bits that the driver expects to see.
 *-----------------------------------------------------------------------------
 * DO NOT enable the ERRORS in the mace mask since the interrupt
 * handler does not take care of the error conditions in this implementation.
 */
_macereg_t	sio_drivermask = (ISA_SERIAL0_DIR |
				  ISA_SERIAL0_Tx_THIR |
				  ISA_SERIAL0_Tx_PREQ |
				  ISA_SERIAL0_Tx_MEMERR |
				  ISA_SERIAL0_Rx_THIR |
				  ISA_SERIAL1_DIR |
				  ISA_SERIAL1_Tx_THIR |
				  ISA_SERIAL1_Tx_PREQ |
				  ISA_SERIAL1_Tx_MEMERR |
				  ISA_SERIAL1_Rx_THIR );
_macereg_t	sio_currntmask = 0x0;

/*
 * UART port  1= back panel port 1      = minor #1
 *   "    "   2=   "    "   port 2      =  ''   #2
 *
 * "Dual console" support - SNCDCPORT is primary (A) non-graphic console, while
 * PDBXPORT is secondary (B) non-graphic console.
 */
#define UARTPORTS	1		/* ports per DUART */
#define NUMUARTS	2
#define SCNDCPORT	0		/* only 1 duart -- no kbd/mouse */
#define PDBXPORT	1		/* port for dbx access to symmon */
#define NUMMINOR	32		/* must be (2**n) */
#define MIN_MINOR   1		/* console needs to be makedev(0,1) */
#define MAX_MINOR   2

#define RAW_CLOCK_SPEED			(22000000)
#define SER_CLK_SPEED(prescalar)	(RAW_CLOCK_SPEED/(prescalar)*2)

#define SER_DIVISOR(baud, clk)          ((((clk) + (baud) * 8) / ((baud) * 16)))
#define DIVISOR_TO_BAUD(div, clk)       ((clk) / 16 / ((div)))

#define BAUD_LO(x)	((uchar_t) (x))
#define BAUD_HI(x)	((uchar_t) ((x) >> 8))

/*
 * type of dma data (pair packing format)
 */
typedef long long dmadata_t;

/*
 * DMA Interface Registers for serial port
 */
typedef struct sport {
	__uint64_t  tx_ctrl;	/* Tx dma channel ring buffer control */
	__uint64_t  tx_rptr;	/* Tx read pointer */
	__uint64_t  tx_wptr;	/* Tx write pointer */
	__uint64_t  tx_depth;	/* Tx depth */
	__uint64_t  rx_ctrl;	/* Rx dma channel ring buffer control */
	__uint64_t  rx_rptr;	/* Rx read pointer */
	__uint64_t  rx_wptr;	/* Rx write pointer */
	__uint64_t  rx_depth;	/* Rx depth */
} sport_t;

/* 
 * XXX:
 * The mysteriously named pciio_pio_* are part of the CRIME 1.1 PIO read
 * bug workaround.  Don't change them unless you understand the implications.
 */
 
#define offset(x)	            ((int) &((sport_t *) 0)->x)
#define WRITE_ISA_IF(reg, val)  pciio_pio_write64((uint64_t)val, (uint64_t *)((ulong)dp->ifp + offset(reg)))
#define READ_ISA_IF(reg)        pciio_pio_read64((uint64_t *)((ulong)dp->ifp + offset(reg)))

/*
 * read from receive dma ring buffer 
 * 
 */
#define RINGBUFF_READ(dp) 		pciio_pio_read64(dp->rx_ptr) 
#define RINGBUFF_WRITE(dp,x) 	pciio_pio_write64(x, dp->tx_ptr) 

#define INC_PTR(ptr) \
	{ __uint64_t x; \
		x  = READ_ISA_IF(ptr); \
		x += 32; \
		WRITE_ISA_IF(ptr, x); \
	}

#define INC_RCV_PTR(dp) \
	if (dp->rx_rptr < dp->rx_ringend) \
		INC_PTR(rx_rptr) \
	else { \
		WRITE_ISA_IF(rx_rptr, 0); \
		dp->rx_rptr = dp->rx_ringbase; \
	};

/* buffer to hold 32-byte DMA block */
struct rcvbuf {
	unsigned char ctrl[16];
	unsigned char data[16];
	unsigned int  count;
	unsigned int  next;
};

/*
 * each port has a data structure that looks like this
 */
typedef struct dport {
	sioport_t   sioport;        	/* must be first struct entry! */
	volatile u_char	*dp_cntrl;	/* port control	reg */

	u_char	dp_index;		/* port number */
	u_int	dp_state;		/* current state */

	vertex_hdl_t	conn_vhdl;
	vertex_hdl_t	port_vhdl;

	/* software copy of hardware registers */
	u_char	du_dat;			/* receive/transmit data */
	u_char	du_icr;			/* interrupt control */
	u_char	du_fcr;			/* fifo control */
	u_char	du_lcr;			/* line control */
	u_char	du_mcr;			/* modem control */
	u_char	du_lsr;			/* line status */
	u_char	du_msr;			/* modem status */
	u_char	du_efr;			/* extra functions */

	sport_t *ifp;			/* serial isa interface */
	dmadata_t *tx_ringbase;		/* transmit ring buffer base */
	dmadata_t *tx_ringend;		/* transmit ring buffer base */
	u_int	  stx_wptr;		/* software copy of hdwr TX wptr */
	dmadata_t *rx_ringbase;		/* receive ring buffer base */
	dmadata_t *rx_ringend;		/* receive ring buffer base */
	dmadata_t *rx_rptr;		/* receive ring buffer read pointer */
	u_int	  srx_rptr;		/* software copy of hdwr RX rptr */
	dmadata_t rx_cache;		/* to cache receive data */
	struct rcvbuf *rx_rcvbuf;	/* buffer current 32-byte rcv data */

	int	(*dp_porthandler)(unsigned char);	/* textport hook */

	int old_tx_val; /* copy of transmit control value */
	int last_lsr;
	int notify;	/* copy of notification flags */
} dport_t;

#define isconsole(dp)			(dp->dp_index == du_console)
static dev_t cons_devs[2];

/* 
 * External ISA address map for serial ports
 * the actual uart registers are available at these addresses 
 * 0x1f380000: ISA bus external IO space
 * 0x00010000: offset of serial port com #1
 * 0x00018000: offset of serial port com #2
 */
#define SERIAL_PORT0_CNTRL		PHYS_TO_K1(ISA_SER1_BASE)
#define SERIAL_PORT1_CNTRL		PHYS_TO_K1(ISA_SER2_BASE)

/* 
 * Define the ISA interface registers
 * The registers are defined on 8-byte boundries while functional block
 * of registers start on 16K byte (0x4000) aligned address. Thus the 
 * page 0 is at 0x0, page 1 at 0x4000, page 2 at 0x8000, page 3 at 0xc000
 */

#define PERIPHERAL_STATUS_REGISTER		PHYS_TO_K1(ISA_INT_STS_REG)
#define PERIPHERAL_INTMSK_REGISTER		PHYS_TO_K1(ISA_INT_MSK_REG) 
#define PERIPHERAL_SERIAL0_ISA_BASE		PHYS_TO_K1(MACE_ISA+0x8000)
#define PERIPHERAL_SERIAL1_ISA_BASE		PHYS_TO_K1(MACE_ISA+0xC000)

/* 
 * Macros to read and write from uart port 
 * each register occupies 256 bytes in external ISA space
 * the actual byte is available in low-order byte of 8 byte value
 */
#define	read_port(num)			pciio_pio_read8((uint8_t *)(dp->dp_cntrl + num * 0x100 + 0x7))
#define	write_port(num, val)	pciio_pio_write8((uint8_t)val, (uint8_t *)(dp->dp_cntrl + num * 0x100 + 0x7))
#define RD_REG	read_port
#define WR_REG	write_port

#define Disable_Duart_intrs(dp)		spltty()
#define Enable_Duart_intrs(dp,s)   	splx(s)
#define INITLOCK_PORT(dp)

/* bits in dp_state */
#define DP_TX_DISABLED 0x1

static dport_t dports[NUMUARTS];
static int maxport;		    	/* maximum number of ports */
static fifo_size = 16;			/* on chip transmit FIFO size */

static void dma_tx_flush(dport_t *);
static void dma_rx_interrupt(dport_t *, int);
static void dma_tx_interrupt(dport_t *, int);
static int  dma_rx_char(dport_t *);		/* get a char from port */
static int  dma_rx_look_char(dport_t *);	/* look at a char from port */
static int  dma_rx_block(dport_t *);		/* get a char from port */

/* local port to global port (and vice versa) conversion */
#define LPORT(port) ((dport_t *)(port))
#define GPORT(port) ((sioport_t *)(port))

#define MINOR_TO_PORT(minor) dports[(minor) - MIN_MINOR]
sioport_t *minor_to_port(int);

#if 0
/* mh16550 boot time info so upper layer can find us
 * given only a minor number
 */
static struct sio_lower_info mh16550_info = {
    0,
    MIN_MINOR,
    MAX_MINOR,
    minor_to_port,
    "mh16550 serial"
};
#endif

/* local functions: */
static int  mh16550_open(sioport_t *port);
static int  mh16550_config(sioport_t *port, int baud, int byte_size,
			  int stop_bits, int parenb, int parodd);
static int  mh16550_enable_hfc(sioport_t *port, int enable);
static int  mh16550_set_extclk(sioport_t *port, int clock_factor);

    /* data transmission */
static int  mh16550_write(sioport_t *port, char *buf, int len);
static int  mh16550_break(sioport_t *port, int brk);
static int  mh16550_du_write(sioport_t *port, char *buf, int len);
static void  mh16550_wrflush(sioport_t *port);
static int mh16550_enable_tx(sioport_t *port, int enb);

    /* data reception */
static int  mh16550_read(sioport_t *port, char *buf, int len);
    
    /* event notification */
static int  mh16550_notification(sioport_t *port, int mask, int on);
static int  mh16550_rx_timeout(sioport_t *port, int timeout);

    /* modem control */
static int  mh16550_set_DTR(sioport_t *port, int dtr);
static int  mh16550_set_RTS(sioport_t *port, int rts);
static int  mh16550_query_DCD(sioport_t *port);
static int  mh16550_query_CTS(sioport_t *port);

static int  mh16550_loopback(sioport_t *port, int enable);

static int  mh16550_set_proto(sioport_t *port, int proto);

static int  mh16550_handle_intr(_macereg_t m_status, sioport_t *port);

void  mh16550_isr(eframe_t *ep, __psint_t m_status);

static struct serial_calldown mh16550_calldown = {
	mh16550_open,
	mh16550_config,
	mh16550_enable_hfc,
	mh16550_set_extclk,
	mh16550_write,
	mh16550_du_write,
	mh16550_wrflush,
	mh16550_break,
	mh16550_enable_tx,
	mh16550_read,
	mh16550_notification,
	mh16550_rx_timeout,
	mh16550_set_DTR,
	mh16550_set_RTS,
	mh16550_query_DCD,
	mh16550_query_CTS,
	mh16550_set_proto,
};

/*
 * ##############################################################################
 * ##                                                                          ##
 * ##                        DMA Suppoort Routines                             ##
 * ##                                                                          ##
 * ##############################################################################
 */

#define	SIONEP	4
#define	SIOCPE	4
#define	SIOCPP	(SIONEP * SIOCPE)

struct payload {
    union element {
	__uint64_t	dw;
	struct {
	    uint8_t	ctrl[SIOCPE];
	    uint8_t	data[SIOCPE];
	} f;
    } e[SIONEP];
};

/*
 * Fill a DMA payload with upto 16 characters
 */
static int
sio_tx_payload(__uint64_t *txwptr, uint8_t *cp, int len)
{
	register int el, ce, cnt;
	struct payload payload;

	/* Initialize with zeros */
#if SIONEP == 4
	payload.e[0].dw = 0LL, payload.e[1].dw = 0LL;
	payload.e[2].dw = 0LL, payload.e[3].dw = 0LL;
#else
	for (el = 0; el < SIONEP; ++el) {
		payload.e[el].dw = 0LL;
	}
#endif

	/* at most we can handle SIOCPP characters */
	if (len > SIOCPP)
		len = SIOCPP;
	cnt = len;

	/* loop */
	for (el = 0; el < SIONEP; ++el) {
		for (ce = 0; ce < SIOCPE; ++ce) {
			if (len-- <= 0)
				goto done;
			payload.e[el].f.ctrl[ce] = DMA_OC_WTHR;
			payload.e[el].f.data[ce] = *cp++;
		}
	}
done:

	/* push new block to memory */
#if SIONEP == 4
	txwptr[0] = payload.e[0].dw, txwptr[1] = payload.e[1].dw;
	txwptr[2] = payload.e[2].dw, txwptr[3] = payload.e[3].dw;
#else
	for (el = 0; el < SIONEP; ++el) {
		*txwptr++ = payload.e[el].dw;
	}
#endif

	return cnt;
}

static void
tx_chars(dport_t *dp, char *buf, int len)
{
	register char *txwptr;
	register int cnt;

	/* write out all the data */
	while (len > 0) {
		/* compute new ring buffer address */
		txwptr = (char *)dp->tx_ringbase + dp->stx_wptr;

		/* write one block */
		cnt = sio_tx_payload((__uint64_t *)txwptr, (uint8_t *)buf, len);
		buf += cnt;
		len -= cnt;

		/* advance ring buffer pointer */
		dp->stx_wptr = (dp->stx_wptr + (SIOCPP*2)) & (4096 - 1);
	}

	/* update hardware write ptr */
	WRITE_ISA_IF(tx_wptr, dp->stx_wptr);
}

dev_t
get_cons_dev(int which)
{
        ASSERT(which == 1 || which == 2);

        return(cons_devs[which - 1]);
}

/* get a block from ring buffer. 
 */
static int
dma_rx_block(dport_t *dp)
{
	__uint64_t rptr, wptr;
	__uint32_t *ringbufp, *ctrlp, *datap;
	int i; 
	unsigned int count = 0;

	rptr = READ_ISA_IF(rx_rptr);
	wptr = READ_ISA_IF(rx_wptr);
	if (rptr == wptr)
		return 0;

	dp->rx_rcvbuf->next = 0;
	dp->rx_rcvbuf->count = 0;

	ringbufp = (__uint32_t *) dp->rx_rptr;	        /* for src */
	ctrlp = (__uint32_t *) &dp->rx_rcvbuf->ctrl;	/* for control */
	datap = (__uint32_t *) &dp->rx_rcvbuf->data;	/* for data */

	for (i=0; i<4; i++) {
		*ctrlp++ = ringbufp[0];
		*datap++ = ringbufp[1];

		/* since DMA engine never writes a zero 64-bit word and we clear 
	 	 * the ring entries we have read, this indicates ring buffer is
	 	 * empty.
	 	 */
		if (*ringbufp == 0) 
			break;

		/* clear ring entry just read; see comment above */
		*ringbufp = 0;

		ringbufp += 2;
		count += 4;
	}

	/* in the last 4 control bytes, it is possible not all are valid */
	while (dp->rx_rcvbuf->ctrl[count-1] == 0)
		count--;
	dp->rx_rcvbuf->count = count;

	dp->rx_rptr += 4;
	INC_RCV_PTR(dp);

	return count;
}

/* Get a character from ring buffer. Return -1 if none available.
 * append status to data.
 */
static int
dma_rx_char(dport_t *dp)
{
	int next;
	unsigned char ctrl, data;
	

	if (!dp->rx_rcvbuf->count) {
		(void) dma_rx_block(dp);
		if (!dp->rx_rcvbuf->count)
			return -1;	/* ring empty */
	}

	next = dp->rx_rcvbuf->next++;
	dp->rx_rcvbuf->count--;

	ctrl = dp->rx_rcvbuf->ctrl[next];
	data = dp->rx_rcvbuf->data[next];

#ifdef LATER
	if ( ctrl & DMA_IC_FRMERR )
		cmn_err(CE_ALERT, "!Framing error on serial port %d", dp->dp_index);
	if ( ctrl & DMA_IC_OVRRUN )
		cmn_err(CE_ALERT, "!Overrun error on serial port %d", dp->dp_index);
	if ( ctrl & DMA_IC_PARERR )
		cmn_err(CE_ALERT, "!Parity error on serial port %d", dp->dp_index);
	if ( ctrl & DMA_IC_BRKDET )
		cmn_err(CE_ALERT, "!Break signal on serial port %d", dp->dp_index);
#endif

	return ((ctrl << 8) | data);
}

/* Look at a character from ring buffer. Return -1 if none available.
 * This is called only from du_conpoll to see if DEBUG_CHAR has been
 * pressed. We look only at the last character. If interrupts are 
 * enabled, hopefully it will be picked by interrupt handler first.
 * If not, this should look at the charecter first and nothing will
 * done if last character isn't DEBUG_CHAR. if it is, we process it.
 */
static int
dma_rx_look_char(dport_t *dp)
{
	int next;
	unsigned char ctrl, data;
	

	if (!dp->rx_rcvbuf->count) {
		(void) dma_rx_block(dp);
		if (!dp->rx_rcvbuf->count)
			return -1;
	}

	next = dp->rx_rcvbuf->next;
	ctrl = dp->rx_rcvbuf->ctrl[next];
	data = dp->rx_rcvbuf->data[next];
	
	if (ctrl & DMA_IC_VALID)
		return ((ctrl << 8) | data);
	else
		return -1;
}

/*
 * 01 device intr req
 * 02 Tx threshold
 * 04 Tx pair request
 * 08 Tx memory error
 * 10 Rx threshold
 * 20 Rx over-run
 */

static void
dma_rx_interrupt(dport_t *dp, int enable)
{
	int shift_by = 20;

	shift_by += dp->dp_index * 6;	/* there are 6 int bits per port */
	
	/*
	 * Update shadow copy of 'current mask'
	 * Write it out to MACE mask register 
	 */
	if (enable)
	        sio_currntmask |=  (0x10 << shift_by);
	else    sio_currntmask &= ~(0x10 << shift_by);

	mace_mask_update(sio_drivermask, sio_currntmask);
}

static void
dma_tx_interrupt(dport_t *dp, int enable)
{
	int shift_by = 20;

	shift_by += dp->dp_index * 6;	/* there are 6 int bits per port */
	/*
	 * Update shadow copy of 'current mask'
	 * Write it out to MACE mask register 
	 */
	if (enable)
	        sio_currntmask |=  (0x02 << shift_by);
	else    sio_currntmask &= ~(0x02 << shift_by);

	mace_mask_update(sio_drivermask, sio_currntmask);
}

/*
 * ##############################################################################
 * ##                                                                          ##
 * ##                        Debug Suppoort Routines                           ##
 * ##                                                                          ##
 * ##############################################################################
 */
void
mh16550_dport_info(__psint_t n)
{
	dport_t *dp;

	if ((n < 0) || (n > 4))
		dp = &dports[1];
	else 
		dp = &dports[n];

	qprintf("dport 0x%x (%d)\n", dp, dp->dp_index);
	qprintf("dat 0x%x icr 0x%x fcr 0x%x lcr 0x%x\n",
		dp->du_dat, dp->du_icr, dp->du_fcr, dp->du_lcr);
	qprintf("mcr 0x%x lsr 0x%x msr 0x%x efr 0x%x\n",
		dp->du_mcr, dp->du_lsr, dp->du_msr, dp->du_efr);
	qprintf("notify 0x%x\n", dp->notify);
	qprintf("old tx value 0x%x\n", dp->old_tx_val);
	qprintf("cntrl 0x%x ifp 0x%x\n", dp->dp_cntrl, dp->ifp);
	qprintf("transmit ring base 0x%x end 0x%x wptr 0x%x\n",
		dp->tx_ringbase, dp->tx_ringend, dp->stx_wptr);
	qprintf("receive  ring base 0x%x end 0x%x wptr 0x%x\n",
		dp->rx_ringbase, dp->rx_ringend, dp->rx_rptr);
}

/*
 * ##############################################################################
 * ##                                                                          ##
 * ##                        Modular Serial Driver Support                     ##
 * ##                                                                          ##
 * ##############################################################################
 */

/* given a minor number by the upper layer, return the port struct */
sioport_t *
minor_to_port(int minor)
{
    dport_t *dp;

    ASSERT(minor >= MIN_MINOR && minor <= MAX_MINOR);

    dp = &(MINOR_TO_PORT(minor));
    return(GPORT(dp));
}

/*
 * Enable hardware flow control
 */
static int
mh16550_enable_hfc(sioport_t *port, int enable)
{
	dport_t *dp = LPORT(port);

	ASSERT(sio_port_islocked(port));

	if (enable)
		dp->du_mcr |= MCR_AFE;
	else 
		dp->du_mcr &= ~MCR_AFE;
	write_port(REG_MCR, dp->du_mcr);
	return (0);
}

int baud_debug = 0;

/*
 * config hardware
 */
static int
mh16550_config(sioport_t *port,
	int baud,
	int byte_size,
	int stop_bits,
	int parenb,
	int parodd)
{
	int lcr;
	int diff, divisor, prescalar, actual_baud;
	int min_divisor, min_scalar, min_diff;
	dport_t *dp = LPORT(port);
	
	ASSERT(sio_port_islocked(port));
	lcr = dp->du_lcr;

	/* set number of data bits */
	lcr &= ~LCR_MASK_BITS_CHAR;
	lcr |= byte_size - 5;

	/* set number of stop bits */
	lcr &= ~LCR_MASK_STOP_BITS;
	lcr |= stop_bits ? LCR_STOP2 : LCR_STOP1;

	/* set parity */
	lcr &= ~LCR_MASK_PARITY_BITS;
	if (parenb) {
		lcr |= LCR_PAREN;
		if (!parodd)
			lcr |= LCR_PAREVN;
	}

	/* 
	 *  Loop through the possible prescalars, searching
	 *  for the one that gives us the least difference.
	 */
	min_diff = baud;
	for (prescalar = 6; prescalar < 30; prescalar++){
		divisor = SER_DIVISOR(baud, SER_CLK_SPEED(prescalar));
		if (divisor == 0) divisor++; 	/* Don't divide by zero */
		actual_baud = DIVISOR_TO_BAUD(divisor, SER_CLK_SPEED(prescalar));

		diff = actual_baud - baud;
		if (diff < 0)
			diff = -diff;

		if (diff < min_diff){
			min_diff = diff;
			min_scalar = prescalar;
			min_divisor = divisor;
		}
	}

	/* 
	 * If we can't come within 1% of the desired baud rate then
	 * reject this.
	 */
	if (min_diff * 100 > baud){
#ifdef DEBUG
		printf("SIO: Can't get good divisor for %d\n", baud);
#endif
		return(1);
	} 

#ifdef DEBUG
	if (baud_debug && baud){
	/* 
	 *The deviation percentage would be (float)min_diff/baud*100
	 */
	actual_baud = DIVISOR_TO_BAUD(min_divisor, SER_CLK_SPEED(min_scalar));
	printf("prescalar (%d) diff (%d) baud (%d) actual baud (%d) divisor (%d)\n",
		min_scalar, min_diff, baud, actual_baud, min_divisor);
	}
#endif

	/*
	* First, enable read/write of baud rate
	* divisor latches then write out LS byte,
	* write out MS byte,
	* then disable read/write of baud rate
	* divisor latches
	*/
	write_port(REG_LCR, LCR_DLAB);
	write_port(REG_SCR, min_scalar);
	write_port(REG_DLH, BAUD_HI(min_divisor));
	write_port(REG_DLL, BAUD_LO(min_divisor));
	
	dp->du_lcr = lcr;
	 write_port(REG_LCR, dp->du_lcr); /* and clear latch */

	/* XXX muck with interrupt bits here? */
	/* XXX mips_du_start_rx here like we use use to? */

	return (0);
}

/*
 * write bytes to the hardware. Returns the number of bytes
 * actually written
 */
mh16550_write(sioport_t *port, char *buf, int len)
{
	dport_t *dp = LPORT(port);
	int count, newLen, olen = len;
	int s = Disable_Duart_intrs(dp);

	ASSERT(L_LOCKED(port, L_WRITE));

	if (dp->dp_state & DP_TX_DISABLED)
		return (0);
	
	/* 
	 * calculate free blocks available to write.
	 * each block can store 16 data bytes.
	 */
	count = MAX_RING_SIZE - READ_ISA_IF(tx_depth) - 32;
	count >>= 1;

	while (count && len) {
		newLen = min(count, len);
		tx_chars(dp, buf, newLen);
		buf += newLen;
		len -= newLen;
		count -= newLen;
	}

	Enable_Duart_intrs(dp, s);
	return olen - len;
}

/*
 * write bytes to the hardware. Returns the number of bytes
 * actually written
 */
mh16550_du_write(sioport_t *port, char *buf, int len)
{
	dport_t *dp = LPORT(port);
	int old_val, i;
	int s = Disable_Duart_intrs(dp);

	/* Disable DMA */
	old_val = READ_ISA_IF(tx_ctrl);
	WRITE_ISA_IF(tx_ctrl, old_val & ~(DMA_ENABLE|DMA_INT_EMPTY));

        for (i = 0; i < len; i++) {
                while (!(read_port(REG_LSR) & LSR_XHRE)) /* wait until ready */
                        ;
		write_port(REG_DAT, *buf);
                buf++;
        }
	WRITE_ISA_IF(tx_ctrl, old_val);
	
	Enable_Duart_intrs(dp, s);
	return len;
}

/*
 * early console output before the driver has been initialized.
 */
int
mh16550_console_write(char *buf, unsigned long len, unsigned long *cnt)
{
	*cnt = mh16550_du_write(GPORT(&dports[du_console]), buf, len);
	return 0;
}

/*
 * flush previous down_du_write calls
 */
static void
mh16550_wrflush(sioport_t *port)
{
	/*
	 * XXX for now, just return. Comments in io/sio_16550.c
	 * indicate we have a small fifo so there's no need to flush
	 */
	return;
}

#if 0
/* XXX
 * This doesn't seem to be used anywhere -wsm4/11/97.
 */
emergency_write(char *buf, int len)
{
	mh16550_du_write(GPORT(&dports[0]), buf, len);
}
#endif

/*
 * Set or clear break condition on output
 */
static int
mh16550_break(sioport_t *port, int brk)
{
    dport_t *dp = LPORT(port);

	ASSERT(sio_port_islocked(port));
	dprintf(("break %s\n", (brk ? "start" : "stop")));

	if (brk)
		dp->du_lcr |= LCR_SETBREAK;
	else
		dp->du_lcr &= ~LCR_SETBREAK;
	write_port(REG_LCR, dp->du_lcr);
	return (0);
}

/*
 * read in bytes from the hardware. Return the number of bytes
 * actually read
 */
static int
mh16550_read(sioport_t *port, char *buf, int len)
{
    dport_t *dp = LPORT(port);
    int c, ch, sr, total;

	ASSERT(sio_port_islocked(port));

       /* In case the upper level ring buffer is filled up, no
	  choice but to drop on the floor.  This can easily happen
	  if there is nobody doing reads on this port. */
       if (len == 0){
               dma_rx_char(dp);
               return(0);
       }


	if (dp->last_lsr != -1) {
		c = dp->last_lsr;
		dp->last_lsr = -1;
	} else
		c = dma_rx_char(dp);

	total = 0;
	while (c != -1) {
		sr = c >> 8;
		ch = c & 0xff;

		/* if we get a control-A, debug on this port */
		if (kdebug && !console_is_tport &&
		     isconsole(dp) && ch == DEBUG_CHAR) {
			SIO_UNLOCK_PORT(port);
			debug("ring");
			SIO_LOCK_PORT(port);
			c = dma_rx_char(dp);
			continue;
		}
		
		if ((dp->notify & (N_ALL_ERRORS | N_BREAK)) &&
			(sr & (DMA_IC_BRKDET|DMA_IC_FRMERR|DMA_IC_OVRRUN|DMA_IC_PARERR))) {
		/* if we have already copied some bytes, stop here
		 * Otherwise, just copy one more byte and that's it
		 */
		if (total > 0) {
			dp->last_lsr = c;
			break;
		} else {
			len = 1;
			if ((dp->notify & N_BREAK) && (sr & DMA_IC_BRKDET))
				UP_NCS(port, NCS_BREAK);
			if ((dp->notify & N_FRAMING_ERROR) && (sr & DMA_IC_FRMERR))
				UP_NCS(port, NCS_FRAMING);
			if ((dp->notify & N_OVERRUN_ERROR) && (sr & DMA_IC_OVRRUN))
				UP_NCS(port, NCS_OVERRUN);
			if ((dp->notify & N_PARITY_ERROR) && (sr & DMA_IC_PARERR))
				UP_NCS(port, NCS_PARITY);
		  }
		}

		*buf++ = ch;
		len--;
		total++;
		if (len <=0 )
			break;
		c = dma_rx_char(dp);
	}

	/* dprintf(("rd p 0x%x len %d(%d)\n", port, len, total)); */
	return(total);
}

static int
mh16550_notification(sioport_t *port, int mask, int on)
{
    dport_t *dp = LPORT(port);
    int rx, tx;
	

    ASSERT(sio_port_islocked(port));
    ASSERT(mask);

	/* in DMA, we have no control over line status interrupts (or
	 * any uart intr for that matter). We can only monitor modem
	 * status change intrs. Even if we disbale LSR interrupts, it
	 * will be enabled by DMA engine.
	 */
	rx = tx = DMA_ENABLE;
	if (mask & N_DATA_READY) {
		if (on)
			rx |= DMA_INT_NEMPTY;
		WRITE_ISA_IF(rx_ctrl, rx);
	}
	if (mask & N_OUTPUT_LOWAT) {
		if (on)
			tx |= DMA_INT_EMPTY;
		WRITE_ISA_IF(tx_ctrl, tx);
	}

	if (on) {
		dp->notify |= mask;
		/* If any of the modem status notifications was requested,
		 * we must turn on the modem status interrupt
		 */
		dp->du_icr &= ~ICR_MIEN;
		if (dp->notify & (N_DDCD | N_DCTS)) {
			dp->du_icr |= ICR_MIEN;
			write_port(REG_ICR, dp->du_icr);
		}
	} else {
		dp->notify &= ~mask;
		/* If none of the modem status notifications is requested,
		 * turn off the modem status interrupt
		 */
		if (!(dp->notify & (N_DDCD | N_DCTS))) {
			dp->du_icr &= ~ICR_MIEN;
			write_port(REG_ICR, dp->du_icr);
		}
	}
		
	return (0);
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
mh16550_rx_timeout(sioport_t *port, int timeout)
{
    ASSERT(sio_port_islocked(port));

    return(1);
}

static void
set_DTR_RTS(sioport_t *port, int val, int mask)
{
    dport_t *dp = LPORT(port);

    ASSERT(sio_port_islocked(port));

	if (val)
		dp->du_mcr |= mask;
	else
		dp->du_mcr &= ~mask;
	WR_REG(REG_MCR, dp->du_mcr);
}

static int
mh16550_set_DTR(sioport_t *port, int dtr)
{
	set_DTR_RTS(port, dtr, MCR_DTR);
	return (0);
}

static int
mh16550_set_RTS(sioport_t *port, int rts)
{
	set_DTR_RTS(port, rts, MCR_RTS);
	return (0);
}

static int
mh16550_query_DCD(sioport_t *port)
{
	dport_t *dp = LPORT(port);

	ASSERT(sio_port_islocked(port));
	return (RD_REG(REG_MSR) & MSR_DCD);
}

static int
mh16550_query_CTS(sioport_t *port)
{
	dport_t *dp = LPORT(port);

	ASSERT(sio_port_islocked(port));
	return (RD_REG(REG_MSR) & MSR_CTS);
}

extern char *kopt_find(char *);
/*
 * Boot time initialization of this module, before the hwgraph
 * has been setup (just enough setup to get early console output).
 */
void
mh16550_earlyinit(void)
{
	u_char *console = (u_char *)kopt_find("console");

	du_console = SCNDCPORT;
	if (console && (console[0] == 'd') && (console[1] == '2'))
		du_console = PDBXPORT;

	/* external ISA controller */
	dports[0].dp_cntrl = (u_char *) SERIAL_PORT0_CNTRL;
	dports[1].dp_cntrl = (u_char *) SERIAL_PORT1_CNTRL;

	dports[0].ifp = (sport_t *) PERIPHERAL_SERIAL0_ISA_BASE;
	dports[1].ifp = (sport_t *) PERIPHERAL_SERIAL1_ISA_BASE;
}

/*
 * Initialization of this module after the hwgraph has been
 * setup. Create the tty nodes and link into the sio layer.
 */
void
mh16550_attach(vertex_hdl_t conn_vhdl)
{
	int x;
	graph_error_t rc;
	dport_t *dp;
	char *env;
	__uint64_t dma_ring_base;
	vertex_hdl_t port_vhdl;
        static char *names[] = {"tty/1", "tty/2"};
	static duinit_flag = 0;
	struct sio_lock *lockp;

	if (duinit_flag++)		/* do it at most once */
		return;

	/* figure out number of ports to work with */
	maxport = NUMUARTS * UARTPORTS - 1;

	/*
	 * this should be the only place we reset the chips. enable global
	 * interrupt too
	 */
	setmaceisavector(MACE_INTR(4), sio_drivermask, mh16550_isr);

	dports[0].dp_index = 0;
	dports[1].dp_index = 1;

	/* DMA ring base registers */
	dma_ring_base = get_isa_dma_buf_addr();
	dma_ring_base = PHYS_TO_K1(dma_ring_base);

	dports[0].tx_ringbase =
		(dmadata_t *)(dma_ring_base | (RID_SERIAL0_TX << 12));
	dports[0].rx_ringbase =
		(dmadata_t *)(dma_ring_base | (RID_SERIAL0_RX << 12));
	dports[1].tx_ringbase =
		(dmadata_t *)(dma_ring_base | (RID_SERIAL1_TX << 12));
	dports[1].rx_ringbase =
		(dmadata_t *)(dma_ring_base | (RID_SERIAL1_RX << 12));

	dports[0].tx_ringend =
		(dmadata_t *)((long long) dports[0].tx_ringbase + 4096 - 4);
	dports[0].rx_ringend =
		(dmadata_t *)((long long) dports[0].rx_ringbase + 4096 - 4);
	dports[1].tx_ringend =
		(dmadata_t *)((long long) dports[1].tx_ringbase + 4096 - 4);
	dports[1].rx_ringend =
		(dmadata_t *)((long long) dports[1].rx_ringbase + 4096 - 4);

	/* initialize the DMA ring read and write pointers */
	dports[0].rx_rptr = dports[0].rx_ringbase;
	dports[1].rx_rptr = dports[1].rx_ringbase;

        /* get rcv ring buffer address from K0 space */
        if ( dports[0].rx_rcvbuf == 0 )
                dports[0].rx_rcvbuf = (struct rcvbuf *)kmem_zalloc(sizeof(struct rcvbuf), KM_SLEEP);
	if ( dports[1].rx_rcvbuf == 0 )
                dports[1].rx_rcvbuf = (struct rcvbuf *)kmem_zalloc(sizeof(struct rcvbuf), KM_SLEEP); 
	
	ASSERT(dports[0].rx_rcvbuf != NULL);
	ASSERT(dports[1].rx_rcvbuf != NULL);

	/*
	 * deassert all output signals, enable 16550 functions, transmitter
	 * interrupt when FIFO is completely empty
	 */
	for (dp = dports, x = 0; dp <= &dports[maxport]; dp++, x++) {

		rc = hwgraph_path_add(conn_vhdl, names[x], &port_vhdl);
		ASSERT(rc == GRAPH_SUCCESS);

		dp->conn_vhdl = conn_vhdl;
		dp->port_vhdl = port_vhdl;
		hwgraph_fastinfo_set(port_vhdl, (arbitrary_info_t)&dports[x]);

		sio_initport(GPORT(dp),
			port_vhdl,
			NODE_TYPE_ALL_RS232 | NODE_TYPE_ALL_RS422 |
			NODE_TYPE_USER,
			isconsole(dp));

		lockp = (struct sio_lock*)kmem_zalloc(sizeof(struct sio_lock), 0);
		ASSERT(lockp);

		/* XXX streams problem prevents using mutexes for the
		 * port lock. If this is fixed in the future
		 * SIO_LOCK_MUTEX should be used for the non-console
		 * case
		 */
		lockp->lock_levels[0] = 
		    isconsole(dp) ? SIO_LOCK_SPL7 : SIO_LOCK_SPLHI;
		sio_init_locks(lockp);
		GPORT(dp)->sio_lock = lockp;

		if (isconsole(dp)) {
			vertex_hdl_t cons_vhdl;

			rc = hwgraph_edge_get(port_vhdl, "d", &cons_vhdl);
			ASSERT(rc == GRAPH_SUCCESS);
			cons_devs[x] = vhdl_to_dev(cons_vhdl);
		}

		/* attach calldown hooks so upper layer can call our routines.
		 */
		dp->sioport.sio_calldown = &mh16550_calldown;
		dp->last_lsr = -1;
		dp->old_tx_val = -1;

		/* enable fifos and set receiver fifo trigger to 14 bytes */
		write_port(REG_FCR, FIFOEN);	
		dp->du_fcr = FIFOEN;

		/* Reset all interrupts for soft restart */
		(void)read_port(REG_ISR);
		(void)read_port(REG_LSR);
		(void)read_port(REG_MSR);

		/* enable interrupts */
		dma_rx_interrupt(dp, 1);
		dma_tx_interrupt(dp, 1);

		/* create inventory entries */
		device_inventory_add(port_vhdl, INV_SERIAL,
			INV_ONBOARD, -1, 0, 0);
	}

	env = kopt_find("dbaud");
	if (env)
	    def_console_baud = atoi(env);

	device_controller_num_set(dports[0].port_vhdl, 1);
	device_controller_num_set(dports[1].port_vhdl, 2);

	/* create /hw/ttys edges to sio_initport() devices */
	sio_make_hwgraph_nodes(GPORT(&dports[0]));
	sio_make_hwgraph_nodes(GPORT(&dports[1]));

	the_console_port = &dports[du_console];

	/* reset dp for WRITE_ISA_IF() call */
	dp = &dports[du_console];
	WRITE_ISA_IF(tx_ctrl, DMA_ENABLE);

#ifdef DEBUG
	idbg_addfunc("duart", mh16550_dport_info);
#endif
	console_device_attached = 1;
}

/*
 * Set up an array of masks, so we can check for various ports.  This
 * could go in the per port data struct, but since this is the only
 * place its used....
 */
static struct {
        long long rx_thir;
        long long tx_thir;
        long long dir;
} masks[2] = {
        {ISA_SERIAL0_Rx_THIR, ISA_SERIAL0_Tx_THIR, ISA_SERIAL0_DIR},
        {ISA_SERIAL1_Rx_THIR, ISA_SERIAL1_Tx_THIR, ISA_SERIAL1_DIR}
};


static int
mh16550_handle_intr(_macereg_t m_status, sioport_t *port)
{
	long long pisr;
	dport_t *dp = LPORT(port);

	ASSERT(sio_port_islocked(port) == 0);
	SIO_LOCK_PORT(port);

	/* read the peripheral interrupt status mask register */
	pisr = m_status;

	port = GPORT(dp);
	if ((pisr & masks[dp->dp_index].rx_thir) && (dp->notify & N_DATA_READY))
		UP_DATA_READY(port);
	if ((pisr & masks[dp->dp_index].tx_thir) && (dp->notify & N_OUTPUT_LOWAT)){
		WRITE_ISA_IF(tx_ctrl, DMA_ENABLE|DMA_INT_EMPTY);
		UP_OUTPUT_LOWAT(port);
	}

	/* should only get modem interrupts directly and those too if they
	 * have been requested
	 * Device-interrupt-request bit in MACE status register doesn't
         * reflect the state of modem signal (DCD).   4/30/1997
         * So the check for it is removed --
         * if (pisr & masks[dp->dp_index].dir) {}.
         * Get modem status from TI serial chip's modem status register.
	 */
	{
		u_char msr_val = RD_REG(REG_MSR);

		if ((dp->notify & N_DCTS) && (msr_val & MSR_DCTS))
			UP_DCTS(port, msr_val & MSR_CTS);

		if (dp->notify & N_DDCD) {
                        if (msr_val & MSR_DCD)
                                UP_DDCD(port, 1);
                        else {
                                /* "delta DCD & no DCD" means carrier drop */
                                if (msr_val & MSR_DDCD)
                                    UP_DDCD(port, 0); /* notify carrier drop */
                        }
                }

	}

	SIO_UNLOCK_PORT(port);

	return 0;
}

void
mh16550_isr(eframe_t *ep, __psint_t status)
{
	int i;

	for (i=0; i<=maxport; i++)
		mh16550_handle_intr(status, GPORT(&dports[i]));

	/*
	 * We're done servicing both serial #0, #1
	 * interrupts. We can safely re-enable these
	 * bits in the MACE-ISA mask register.
	 */

	mace_mask_enable(sio_drivermask);
	return;
}

static	int
mh16550_enable_tx(sioport_t *port, int enb)
{
      dport_t *dp = LPORT(port);

      ASSERT(sio_port_islocked(port));

      /* if we're already in the desired state, we're done */
      if ((enb && !(dp->dp_state & DP_TX_DISABLED)) ||
        (!enb && (dp->dp_state & DP_TX_DISABLED)))
        return(0);
      
      if (enb) {
        dp->dp_state &= ~DP_TX_DISABLED;
          if ( dp->old_tx_val != -1)
            WRITE_ISA_IF(tx_ctrl, dp->old_tx_val | DMA_ENABLE);
      }
      else {
        dp->dp_state |= DP_TX_DISABLED;
	dp->old_tx_val = READ_ISA_IF(tx_ctrl);

	WRITE_ISA_IF(tx_ctrl, dp->old_tx_val & ~(DMA_ENABLE|DMA_INT_EMPTY));
      }
      return(0);
}

/*
 * open a port
 */
static int
mh16550_open(sioport_t *port)
{
	ASSERT(sio_port_islocked(port));
	return(mh16550_config(port, 9600, 8, 0, 0, 0));
}

static int
mh16550_loopback(sioport_t *port, int enable)
{
	dport_t *dp = LPORT(port);

	printf("%s loopback\n", enable ? "enable" : "disable");

	ASSERT(sio_port_islocked(port));
	if (enable)
		dp->du_mcr |= MCR_LOOP;
	else
		dp->du_mcr &= ~MCR_LOOP;
    	write_port(REG_MCR, dp->du_mcr);
	return (0);
}

static int
mh16550_set_proto(sioport_t *port, int proto)
{
	dport_t *dp = LPORT(port);
	ASSERT(sio_port_islocked(port));
        /* only support rs232, otherwise return 1 (failure) */
        return(proto != PROTO_RS232);
}

static int
mh16550_set_extclk(sioport_t *port, int clock_factor)
{
	dport_t *dp = LPORT(port);
	ASSERT(sio_port_islocked(port));
        /* only allow 0 (no external clock) */
        return(clock_factor ? 1 : 0);
}

/*
 * poll for control-A from the console before hwgraph is setup, may cause
 * lose of character.  this routine should only be defined in the UART
 * driver that controls the console
 */
void
console_debug_poll(void)
{
	dport_t *dp;
	int i, c, ch;

	for (i = 0; i < NUMUARTS; i++) {
		dp = &dports[i];
		if (isconsole(dp))
			break;
	}

	if (!isconsole(dp) || !console_device_attached ||
		SIO_TRY_LOCK_PORT(GPORT(dp)) == 0) {
		return;
	}

        if (dp->last_lsr != -1) {
		c = dp->last_lsr;
		dp->last_lsr = -1;
        } else
                c = dma_rx_char(dp);

        while (c != -1) {
		ch = c & 0xff;

                /* if we get a control-A, debug on this port */
                if (kdebug && !console_is_tport && ch == DEBUG_CHAR) {
                        SIO_UNLOCK_PORT(GPORT(dp));

			debug("quiet");
                        debug("ring");

                        SIO_LOCK_PORT(GPORT(dp));
                        c = dma_rx_char(dp);
                        continue;
                }
	}
}
