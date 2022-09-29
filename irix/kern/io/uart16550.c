/*
 * streams driver for the 16550 serial communication controller
 *
 */

#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/stream.h"
#include "sys/strids.h"
#include "sys/strmp.h"
#include "sys/stropts.h"
#include "sys/stty_ld.h"
#include "sys/types.h"
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
  static du_debug = 0;
  #define dprintf(x)				printf x
  #ifndef DEBUG_CHAR
  #define DEBUG_CHAR	CNTRL_A
  #endif
#else /* !DEBUG */
  #define DEBUG_CHAR	CNTRL_A
  #define dprintf(x)				
#endif /* DEBUG */

int	zduart_rdebug = 0;

static int def_console_baud = B9600;	/* default speed for the console */
static int def_pdbxport_baud = B9600;   /* default speed for the pdbxport */
static void du_poll();
static int du_open(queue_t *, dev_t *, int, int, struct cred *);

extern console_is_tport;
extern struct stty_ld def_stty_ld;

/* default cflags */
#define DEF_CFLAG 		((tcflag_t)(CREAD | def_stty_ld.st_cflag))

/* stream stuff */
static struct module_info dum_info = {
	STRID_DUART,			/* module ID */
	"DUART",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* maximum packet size--infinite */
	128,				/* hi-water mark */
	16,				/* lo-water mark */
};

#define MIN_RMSG_LEN	4		/* minimum buffer size */
#define MAX_RMSG_LEN	2048	/* largest msg allowed */
#define XOFF_RMSG_LEN	256		/* send XOFF here */
#define MAX_RBUF_LEN	1024

#define MAX_RSRV_CNT	3		/* continue input timer this long */
extern int duart_rsrv_duration;	/* send input this often */

static void du_rsrv(queue_t *);
static int du_close(queue_t *, int, struct cred *);

static struct qinit du_rinit = {
	NULL, (int (*)())du_rsrv, du_open, du_close, NULL, &dum_info, NULL
};

static int du_wput(queue_t *, mblk_t *);
static struct qinit du_winit = {
	du_wput, NULL, NULL, NULL, NULL, &dum_info, NULL
};

int dudevflag = 0;
struct streamtab duinfo = {
	&du_rinit, &du_winit, NULL, NULL
};


#define NUMMINOR	32		/* must be (2**n) */
#define PORT(dev)	port_map[(dev) & (NUMMINOR - 1)]
#define MODEM(dev)	((dev) & (NUMMINOR * 3))
#define FLOW_MODEM(dev)	((dev) & (NUMMINOR * 2))

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

uint du_console;
#define CONSOLE_DMA	1		/* use dma for console */

static u_char port_map[NUMMINOR] = {
	255,   0,   1, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
};


#define ACE_PRESCALER_DIVISOR  (24)
#define BAUD(x)     (SERIAL_CLOCK_FREQ / ((x) * 16)) 
#define BAUDBAD     0xffff

/*
 * Baud rate table using a 1.8432-Mhz Crystal. 
 */

const u_short divisor_table[] = {
    0,  	/* 0 baud rate */
    0x900,	/* 50 baud rate */
    0x600,	/* 75 baud rate */
    0x417,	/* 110 baud rate ( %0.026 ) */
    0x359,	/* 134.5 baud rate ( %0.058 ) */
    0x300,	/* 150 baud rate */
    0x180,	/* 300 baud rate */
    0xc0,	/* 600 baud rate */
    0x60,	/* 1200 baud rate */
    0x40,	/* 1800 baud rate */
    0x3a,	/* 2000 baud rate */
    0x30,	/* 2400 baud rate */
    0x20,	/* 3600 baud rate */
    0x18,	/* 4800 baud rate */
    0x10,	/* 7200 baud rate */
    0x0c,	/* 9600 baud rate */
    0x6,	/* 19200 baud rate */
    0x3,	/* 38400 baud rate */
    0x2,	/* 56000 baud rate */
    0x1 	/* 128000 baud rate */
};

#define MAXBAUDS sizeof(divisor_table) / sizeof(u_short) /* 18 */

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

#define offset(x)	           ((int) &((sport_t *) 0)->x)
#define WRITE_ISA_IF(reg, val) WRITE_REG64(val, (ulong)dp->ifp + offset(reg), long long)
#define READ_ISA_IF(reg)       READ_REG64((ulong)dp->ifp + offset(reg), long long)

/*
 * read from receive dma ring buffer 
 * 
 */
#define RINGBUFF_READ(dp) 		(*dp->rx_ptr) 
#define RINGBUFF_WRITE(dp,x) 	(*dp->tx_ptr = x) 

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
#define INC_XMT_PTR(dp) \
	if (dp->tx_wptr < dp->tx_ringend) \
		INC_PTR(tx_wptr) \
	else { \
		WRITE_ISA_IF(tx_wptr, 0); \
		dp->tx_wptr = dp->tx_ringbase; \
	};

/* disable interrupts for console debug io */
#define Tx_Disabled(dp) \
	(!((READ_ISA_IF(tx_ctrl) & DMA_INT_MASK) \
		&& (READ_ISA_IF(tx_ctrl) & DMA_ENABLE)))
#define Tx_Enable(dp) \
	{ __uint64_t x; \
		dp->du_icr &= ~ICR_TIEN; \
		write_port(REG_ICR, dp->du_icr); \
		x = READ_ISA_IF(tx_ctrl); \
		x |= DMA_ENABLE; \
		WRITE_ISA_IF(tx_ctrl, x); \
	}

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
	volatile u_char	*dp_cntrl;	/* port control	reg */

	u_char	dp_index;		/* port number */

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
	dmadata_t *tx_wptr;			/* transmit ring buffer write pointer */
	dmadata_t *rx_ringbase;		/* receive ring buffer base */
	dmadata_t *rx_ringend;		/* receive ring buffer base */
	dmadata_t *rx_rptr;			/* receive ring buffer read pointer */
	dmadata_t rx_cache;			/* to cache receive data */
	struct rcvbuf rx_rcvbuf;	/* buffer current 32-byte receive data */

	u_char	dp_rsrv_cnt;		/* input timer count */
	int	dp_rsrv_duration;	/* input timer */

	u_int	dp_state;		/* current state */

	struct termio dp_termio;
#define dp_iflag dp_termio.c_iflag	/* use some of the bits (see below) */
#define dp_cflag dp_termio.c_cflag	/* use all of the standard bits */
#define dp_ospeed dp_termio.c_ospeed	/* output speed */
#define dp_ispeed dp_termio.c_ispeed	/* input speed */
#define dp_line  dp_termio.c_line	/* line discipline */
#define dp_cc    dp_termio.c_cc		/* control characters */

	queue_t	*dp_rq, *dp_wq;		/* our queues */
	mblk_t	*dp_rmsg, *dp_rmsge;	/* current input message, head/tail */
	int	dp_rmsg_len;		/* current input message length */
	int	dp_rbsize;		/* recent input message length */
	mblk_t	*dp_rbp;		/* current input buffer */
	mblk_t	*dp_wbp;		/* current output buffer */

	int	(*dp_porthandler)(unsigned char);	/* textport hook */

	int	dp_tid;			/* (recent) output delay timer ID */

	int	dp_fe, dp_over;		/* framing/overrun errors counts */
	int	dp_allocb_fail;		/* losses due to allocb() failures */
	int dp_char_xmitted;	/* chars xmitted since last interrupt */
} dport_t;

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
#define	read_port(num)			(*(dp->dp_cntrl + num * 0x100 + 0x7))
#define	write_port(num, val)	((*(dp->dp_cntrl + num * 0x100 + 0x7)) = val)


#define DUART_DMA_FLUSH	1
#define DUART_DMA_NOFLUSH 0

#define LOCK_PORT(dp)			spltty()
#define UNLOCK_PORT(dp,s)   	splx(s)
#define INITLOCK_PORT(dp)

/* bits in dp_state */
#define DP_ISOPEN		0x00000001	/* device is open */
#define DP_WOPEN		0x00000002	/* waiting for carrier */
#define DP_DCD			0x00000004	/* we have carrier */
#define DP_TIMEOUT		0x00000008	/* delaying */
#define DP_BREAK		0x00000010	/* breaking (dss: i.e sending a break) */
#define DP_BREAK_QUIET	0x00000020	/* finishing break */
#define DP_TXSTOP		0x00000040	/* output stopped by received XOFF */
#define DP_LIT			0x00000080	/* have seen literal character */
#define DP_BLOCK		0x00000100	/* XOFF sent because input full */
#define DP_TX_TXON		0x00000200	/* need to send XON */
#define DP_TX_TXOFF		0x00000400	/* need to send XOFF */
#define	DP_SE_PENDING	0x00000800	/* buffer alloc event pending */
#define DP_DMA			0x00001000	/* port is in dma mode */
#define	DP_BREAK_ON		0x00010000	/* receiving break */
#define	DP_CTS			0x00020000	/* CTS on */
#define	DP_LOOP			0x00040000	/* loopback mode on */
#define	DP_EXTCLK		0x10000000	/* external clock mode */
#define	DP_RS422		0x20000000	/* RS422 compatibility mode */
#define	DP_MODEM		0x80000000	/* modem device */

#define	SEND_BREAK	1			/* set break */
#define	STOP_BREAK	0			/* reset break */
#define	SEND_XON	1			/* input flow control */
#define	SEND_XOFF	0			/* input flow control */
#define	IGNORE_IXOFF	1		/* input fc, ignore IXOFF flag */

static dport_t dports[] = {
	{(u_char *)(SERIAL_PORT0_CNTRL), 0},
	{(u_char *)(SERIAL_PORT1_CNTRL), 1},
};

static int maxport;		/* maximum number of ports */
static fifo_size = 16;		/* on chip transmit FIFO size */


static void mips_du_break(dport_t *, int);	/* send/stop break processing */
static void mips_du_config(dport_t *, tcflag_t, speed_t, struct termio *); /* config port */
static int mips_du_getchar(dport_t *);	/* get a char from port */
static void mips_du_iflow(dport_t *, int, int);	/* flow control */
static int mips_du_start_rx(dport_t *);
static void mips_du_start_tx(dport_t *);
static void mips_du_stop_rx(dport_t *, int);
static void mips_du_stop_tx(dport_t *);
static int mips_du_txrdy(dport_t *);
static void mips_du_putchar(dport_t *, u_char);
static void dma_tx_flush(dport_t *);
static void dma_rx_interrupt(dport_t *, int);
static void dma_tx_interrupt(dport_t *, int);
static int dma_rx_char(dport_t *);	/* get a char from port */

static int dma_rx_block(dport_t *);	/* get a char from port */

static void du_coff(dport_t *);	/* process carrier-off interrupts */
static void du_con(dport_t *);	/* process carrier-on  interrupts */
static void du_flushw(dport_t *);	/* flush output */
static void du_flushr(struct dport *);	/* flush input */
static mblk_t *du_getbp(dport_t *, u_int);	/* get a buffer */
static int du_loopback(dport_t *, mblk_t *);
static void du_rx(dport_t *);
static void du_tx(dport_t *, int);
static void du_modem(dport_t *);




/*
 * gobble any waiting input, should only be called when safe from interrupts
 */
static void
du_rclr(dport_t *dp)
{
	int c;
	int is_scndcport = ((dp->dp_index == SCNDCPORT) || (dp->dp_index == PDBXPORT));
	/*
	 * above assignment and its use later is not clear to me. It appears to
	 * be redundant as used since the current port has to be one of the two
	 * (scndcport or pdbxport). With current checks, a debug char received 
	 * on any of two ports will cause debug mode to be enetered. XXX
	 */

	dr_printf(("gobbling input\n"));

	if (dp->dp_state & DP_DMA) {
		while ((c = dma_rx_char(dp)) != -1) { 
			if (kdebug && !console_is_tport && 
				is_scndcport && (c & 0xff) == DEBUG_CHAR)
			debug("ring");
		}
	} else {
		while (read_port(REG_LSR) & LSR_RCA) {	/* data is ready */
			c = read_port(REG_DAT);
			if (kdebug && !console_is_tport && 
				is_scndcport && (c & 0xff) == DEBUG_CHAR)
			debug("ring");
		}
	}
}



/*
 * shut down a port, should only be called when safe from interrupts
 */
static void
du_zap(dport_t *dp, int hup)
{
	dprintf(("zapping port %d\n", dp->dp_index));
	du_flushw(dp);			/* forget pending output */

	mips_du_stop_rx(dp, hup);
	mips_du_stop_tx(dp);

	du_rclr(dp);
}



/*
 * finish a delay (started from M_DELAY/TCSBRK) ioctl)
 */
static void
du_delay(dport_t *dp)
{
	int s = LOCK_PORT(dp);

	if ((dp->dp_state & (DP_BREAK | DP_BREAK_QUIET)) != DP_BREAK) {	
		/* finishing break */
		dp->dp_state &= ~(DP_TIMEOUT | DP_BREAK | DP_BREAK_QUIET);
		mips_du_start_tx(dp);	/* resume output */

	} else {			/* unless need to quiet break */
		mips_du_break(dp, STOP_BREAK);
		dp->dp_state |= DP_BREAK_QUIET;
		dp->dp_tid = STREAMS_TIMEOUT(du_delay, (caddr_t)dp, HZ/20);
	}

	UNLOCK_PORT(dp, s);
}



/*
 * heartbeat to send input up stream
 * this is used to reduced the large cost of trying to send each input byte
 * upstream by itself.
 */
static void
du_rsrv_timer(dport_t *dp)
{
	int s = LOCK_PORT(dp);

	if (--dp->dp_rsrv_cnt)
		(void)STREAMS_TIMEOUT(du_rsrv_timer, (caddr_t)dp, dp->dp_rsrv_duration);

	if (dp->dp_state & DP_ISOPEN && canenable(dp->dp_rq))
		qenable(dp->dp_rq);

	UNLOCK_PORT(dp, s);
}



/*
 * flush output, should only be called when safe from interrupts
 */
static void
du_flushw(dport_t *dp)
{
	dt_printf(("flushing output\n"));

	if ((dp->dp_state & (DP_TIMEOUT | DP_BREAK | DP_BREAK_QUIET))
	    == DP_TIMEOUT) { /* must have seen M_DELAY */
		untimeout(dp->dp_tid);		/* forget stray timeout */
		dp->dp_state &= ~DP_TIMEOUT;
	}

	freemsg(dp->dp_wbp);
	dp->dp_wbp = NULL;

	/* 
	 * XXX do we need to flush out ring buffer? 
	 * This can probably be achieved by setting write pointer back to 
	 * read pointer. For example:
	 * dp->tx_wptr = dp->tx_rptr;
	 * May need to disable DMA before the preceeding op.
	 * we may also reset dma channel in the following fashion:
	 * dp->tx_ctrl |= DMA_CHANNEL_RESET;
	 * The second approach sounds better since we may be allowed to change the
	 * write pointer.
	 */
}



/*
 * flush input, should only be called when safe from interrupts
 */
static void
du_flushr(dport_t *dp)
{
	dr_printf(("flushing input\n"));

	freemsg(dp->dp_rmsg);
	dp->dp_rmsg = NULL;
	dp->dp_rmsg_len = 0;
	freemsg(dp->dp_rbp);
	dp->dp_rbp = NULL;

	qenable(dp->dp_rq);		/* turn input back on */

	/* 
	 * XXX do we need to flush receive ring buffer? 
	 * This can probably be achieved by setting read pointer ahead to 
	 * write pointer. For example:
	 * dp->rx_rptr = dp->rx_wptr
	 * May need to disable DMA before the preceeding op.
	 * we may also reset dma channel in the following fashion:
	 * dp->rx_ctrl |= DMA_CHANNEL_RESET;
	 * The second approach sounds better since we may be allowed to change the
	 * read pointer.
	 */
}



/*
 * save a message on our write queue and start the output interrupt, if
 * necessary
 */
static int
du_save(dport_t *dp, queue_t *wq, mblk_t *bp)
{
	putq(wq, bp);			/* save the message */

	/* start Tx only if we must */
	if (Tx_Disabled(dp))
		mips_du_start_tx(dp);	
}



/*
 * get current tty parameters
 */
void
tcgeta(queue_t *wq, mblk_t *bp, struct termio *p)
{
	*STERMIO(bp) = *p;

	bp->b_datap->db_type = M_IOCACK;
	qreply(wq, bp); 
}



/*
 * set tty parameters
 */
static void
du_tcset(dport_t *dp, mblk_t *bp)
{
	tcflag_t cflag;
	speed_t ospeed;
	struct iocblk *iocp;
	struct termio *tp;
	extern int baud_to_cbaud(speed_t);

	iocp = (struct iocblk *)bp->b_rptr; 
	tp = STERMIO(bp);

	if (tp->c_ospeed == __INVALID_BAUD) {
	    tp->c_ospeed = dp->dp_ospeed;	       /* don't change */
	}
	tp->c_ispeed = dp->dp_ospeed;
	cflag = tp->c_cflag;
	ospeed = tp->c_ospeed;
	if ((ospeed == 0) || (BAUD(ospeed) < 1) 
	                      || (BAUD(ospeed) > BAUD(50))) {
	    /*
	     * we support between 50 and ~460K bps;
	     * return error if we can't support the requested baud rate
	     */
	    tp->c_ospeed = dp->dp_ospeed;
	    iocp->ioc_count = 0;
	    iocp->ioc_error = EINVAL;
	    bp->b_datap->db_type = M_IOCACK;
	    return;
	}
	if (dp->dp_state & DP_MODEM) {
		/*
		 * On modem ports, only super-user can change CLOCAL.
		 * Otherwise, fail to do so silently for historic reasons.
		 */
		ASSERT(iocp->ioc_cr);
		if (((dp->dp_cflag & CLOCAL) ^ (cflag & CLOCAL)) &&
		    !_CAP_CRABLE(iocp->ioc_cr, CAP_DEVICE_MGT)) {
			cflag &= ~CLOCAL;
			cflag |= dp->dp_cflag & CLOCAL;
		}
	}
	mips_du_config(dp, cflag, ospeed, tp);
	tp->c_cflag = dp->dp_cflag; /* tell line discipline the results */
	tp->c_ospeed = dp->dp_ospeed;
	tp->c_ispeed = dp->dp_ispeed;

	iocp->ioc_count = 0;
	bp->b_datap->db_type = M_IOCACK;
}



/*
 * interrupt-process an IOCTL. this function processes those IOCTLs that
 * must be done by the output interrupt
 */
static void
du_i_ioctl(dport_t *dp, mblk_t *bp)
{
	struct iocblk *iocp;

	iocp = (struct iocblk *)bp->b_rptr;

	switch (iocp->ioc_cmd) {
	case TCSBRK:
		if (!*(int *)bp->b_cont->b_rptr) {
			dp->dp_state |= DP_TIMEOUT | DP_BREAK;
			mips_du_break(dp, SEND_BREAK);	/* send break to uart */
			dp->dp_tid = STREAMS_TIMEOUT(du_delay, (caddr_t)dp, HZ/4);
		}
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		break;

	case TCSETAF:
		du_tcset(dp, bp);
		du_flushr(dp);
		(void)putctl1(dp->dp_rq->q_next, M_FLUSH, FLUSHR);
		break;

	case TCSETA:
	case TCSETAW:
		du_tcset(dp, bp);
		break;

	case TCGETA:
		tcgeta(dp->dp_wq, bp, &dp->dp_termio);
		return;

	case SIOC_ITIMER:
		dp->dp_rsrv_duration = *(int *)bp->b_cont->b_rptr;
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		break;

	case SIOC_LOOPBACK:
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		iocp->ioc_error = du_loopback(dp, bp);
		break;

	default:
		ASSERT(0);
	}

	putnext(dp->dp_rq, bp);
}


#ifdef DEBUG
static void
dump_dp(int i)
{
	qprintf("Index %d, Port %d, @0x%x\n",i,dports[i].dp_index,dports+i);
	qprintf("State: ");
	if (dports[i].dp_state & DP_ISOPEN)
		qprintf("ISOPEN ");
	if (dports[i].dp_state & DP_WOPEN)
		qprintf("WOPEN ");
	if (dports[i].dp_state & DP_DCD)
		qprintf("DCD ");
	if (dports[i].dp_state & DP_TIMEOUT)
		qprintf("TIMEOUT ");
	if (dports[i].dp_state & DP_BREAK)
		qprintf("BREAK ");
	if (dports[i].dp_state & DP_BREAK_QUIET)
		qprintf("BREAK_QUIET ");
	if (dports[i].dp_state & DP_TXSTOP)
		qprintf("TXSTOP ");
	if (dports[i].dp_state & DP_LIT)
		qprintf("LIT ");
	if (dports[i].dp_state & DP_BLOCK)
		qprintf("BLOCK ");
	if (dports[i].dp_state & DP_TX_TXON)
		qprintf("TX_TXON ");
	if (dports[i].dp_state & DP_TX_TXOFF)
		qprintf("TX_TXOFF ");
	if (dports[i].dp_state & DP_SE_PENDING)
		qprintf("SE_PENDING ");
	if (dports[i].dp_state & DP_EXTCLK)
		qprintf("EXTCLK ");
	if (dports[i].dp_state & DP_RS422)
		qprintf("RS422 ");
	if (dports[i].dp_state & DP_MODEM)
		qprintf("MODEM ");
	if (dports[i].dp_state & DP_BREAK_ON)
		qprintf("BREAK_ON ");
	if (dports[i].dp_state & DP_CTS)
		qprintf("CTS ");
	if (dports[i].dp_state & DP_LOOP)
		qprintf("LOOP ");
	if (dports[i].dp_state & DP_DMA)
		qprintf("DMA ");
	qprintf("\n");

	qprintf("Framing errors: %d, Overruns: %d, Alloc failed: %d\n", 
		dports[i].dp_fe, dports[i].dp_over, dports[i].dp_allocb_fail);
}
#endif
extern char *kopt_find(char *);

/*
 * initialize the DUART ports. this is done to allow debugging soon after
 * booting
 */
void
du_init()
{
	dport_t *dp;
	static duinit_flag = 0;
	char *env;
	int d_baud;
	u_char *console = kopt_find("console");
	__uint64_t dma_ring_base;

	if (duinit_flag++)		/* do it at most once */
		return;

	if (console) {
	     if ((console[0] == 'd') && (console[1] == '2')) {
		  du_console = PDBXPORT;
	     }
	     else {
		  du_console = SCNDCPORT;
	     }
	} else {
		du_console = SCNDCPORT;
	}

    /* check for pdbxport */
	if (kdebug && ((env = kopt_find("rdebug")) != NULL && atoi(env) != 0)) {
		zduart_rdebug = 1;
		env = kopt_find("rbaud");
		if (env) {
		    def_pdbxport_baud = atoi(env);
		}
	}

	/* figure out number of ports to work with */
	maxport = NUMUARTS * UARTPORTS - 1;


	/*
	 * this should be the only place we reset the chips. enable global
	 * interrupt too
	 */
	setcrimevector(MACE_INTR(4), SPL5 /*spltty*/, du_poll /*isr*/, 0 /*arg*/, 0 /*flag*/);

	/* external ISA controller */
	dports[0].dp_cntrl = (u_char *) SERIAL_PORT0_CNTRL;
	dports[1].dp_cntrl = (u_char *) SERIAL_PORT1_CNTRL;

	/* DMA ring base registers */
	dma_ring_base = get_isa_dma_buf_addr();
	dma_ring_base = PHYS_TO_K1(dma_ring_base);

	dports[0].tx_ringbase = (dmadata_t *) (dma_ring_base | (RID_SERIAL0_TX << 12));
	dports[0].rx_ringbase = (dmadata_t *) (dma_ring_base | (RID_SERIAL0_RX << 12));
	dports[1].tx_ringbase = (dmadata_t *) (dma_ring_base | (RID_SERIAL1_TX << 12));
	dports[1].rx_ringbase = (dmadata_t *) (dma_ring_base | (RID_SERIAL1_RX << 12));

	dports[0].tx_ringend = (dmadata_t *) ((long long) dports[0].tx_ringbase + 4096 - 4);
	dports[0].rx_ringend = (dmadata_t *) ((long long) dports[0].rx_ringbase + 4096 - 4);
	dports[1].tx_ringend = (dmadata_t *) ((long long) dports[1].tx_ringbase + 4096 - 4);
	dports[1].rx_ringend = (dmadata_t *) ((long long) dports[1].rx_ringbase + 4096 - 4);

	/* initialize the DMA ring read and write pointers */
	dports[0].tx_wptr = dports[0].tx_ringbase;
	dports[0].rx_rptr = dports[0].rx_ringbase;
	dports[1].tx_wptr = dports[1].tx_ringbase;
	dports[1].rx_rptr = dports[1].rx_ringbase;

	/*
	 * initialize the serial port isa interface register block
	 */
	dports[0].ifp = (sport_t *) PERIPHERAL_SERIAL0_ISA_BASE;
	dports[1].ifp = (sport_t *) PERIPHERAL_SERIAL1_ISA_BASE;

	/*
	 * deassert all output signals, enable 16550 functions, transmitter
	 * interrupt when FIFO is completely empty
	 */
	for (dp = dports; dp <= &dports[maxport]; dp++) {

		/* enable fifos and set receiver fifo trigger to 14 bytes */
		write_port(REG_FCR, FIFOEN);	
		dp->du_fcr = FIFOEN;

		/* Reset all interrupts for soft restart */
		(void)read_port(REG_ISR);
		(void)read_port(REG_LSR);
		(void)read_port(REG_MSR);

		/* 
		 * we are DTE so assert dtr and rts. DCE asserts dsr and cts 
	     * assert rts and we are ready.
		 */
		ASSERT_DTR(dp->du_mcr);	
		ASSERT_RTS(dp->du_mcr);
		write_port(REG_MCR, dp->du_mcr);
	}

	env = kopt_find("dbaud");
	if (env && (d_baud = atoi(env)) > 0)
		def_console_baud = d_baud;

	/* 
	 * initialize secondary console 
	 */
	dp = &dports[du_console];
	mips_du_config(dp, DEF_CFLAG, def_console_baud,
		&def_stty_ld.st_termio);
	du_zap(dp, HUPCL);

#ifdef DEBUG
	if(env && d_baud <= 0)
		cmn_err(CE_WARN, "non-numeric or 0 value for dbaud ignored (%s)", env);

	idbg_addfunc("duart", dump_dp);
#endif
}



/*
 * open a stream DUART port
 */
static int
du_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	dport_t *dp;
	u_short port;
	int s;
	int error;

	if (sflag)			/* only a simple stream driver */
		return(ENXIO);

	port = PORT(*devp);
	if (port > maxport)		/* fail if bad device # */
		return(ENXIO);

	if (port == PDBXPORT && zduart_rdebug)
		return(ENXIO);

	dp = &dports[port];

	s = LOCK_PORT(dp);
	if (!(dp->dp_state & (DP_ISOPEN | DP_WOPEN))) {	/* on the 1st open */
		tcflag_t cflag;
		speed_t ospeed;
		queue_t *wq = WR(rq);

		cflag = def_stty_ld.st_cflag;
		ospeed = def_stty_ld.st_ospeed;
		if (dp->dp_index == du_console)
			ospeed = def_console_baud;
		else if (zduart_rdebug &&
			 dp->dp_index == PDBXPORT)
			ospeed = def_pdbxport_baud;

		dp->dp_rsrv_duration = duart_rsrv_duration;
		dp->dp_state &= ~(DP_TXSTOP | DP_LIT | DP_BLOCK | DP_TX_TXON |
			DP_TX_TXOFF | DP_RS422 | DP_MODEM | DP_EXTCLK);

		if (MODEM(*devp)) {		/* device is a modem */
			dp->dp_state |= DP_MODEM;
			cflag &= ~CLOCAL;
			if (FLOW_MODEM(*devp))	/* h/w flow control supported */
				cflag |= CNEW_RTSCTS;
		}

		/*
		 * open port in DMA mode (default)
		 */
		dp->dp_state |= DP_DMA;
		mips_du_config(dp, cflag, ospeed, &def_stty_ld.st_termio);
		dp->dp_state |= DP_WOPEN; 

		if (!mips_du_start_rx(dp)		/* wait for carrier */
		    && !(dp->dp_cflag & CLOCAL)
		    && !(flag & (FNONBLK | FNDELAY))) {
			do {
				if (sleep((caddr_t)dp, STIPRI)) {
					dp->dp_state &= ~(DP_WOPEN | DP_ISOPEN);
					du_zap(dp, HUPCL);
					UNLOCK_PORT(dp, s);
					return(EINTR);
				}
			} while (!(dp->dp_state & DP_DCD));
		}

		rq->q_ptr = (caddr_t)dp;	/* connect device to stream */
		wq->q_ptr = (caddr_t)dp;
		dp->dp_wq = wq;
		dp->dp_rq = rq;
		dp->dp_state |= DP_ISOPEN;
		dp->dp_state &= ~DP_WOPEN;
		dp->dp_cflag |= CREAD;
		dp->dp_porthandler = NULL;	/* owner gains control */

		du_rclr(dp);		/* discard input */

		if (error = strdrv_push(rq, "stty_ld", devp, crp)) {
			dp->dp_state &= ~(DP_ISOPEN | DP_WOPEN);
			dp->dp_rq = NULL;
			dp->dp_wq = NULL;
			du_zap(dp, HUPCL);
			UNLOCK_PORT(dp, s);
			return(error);
		}
	} else {
		/*
 		 * you cannot open two streams to the same device. the dp
		 * structure can only point to one of them. therefore, you
		 * cannot open two different minor devices that are synonyms
		 * for the same device. that is, you cannot open both ttym1
		 * and ttyd1
 		 */
		if (dp->dp_rq != rq) {
			/* fail if already open */
			UNLOCK_PORT(dp, s);
			return(EBUSY);
		}
	}

	UNLOCK_PORT(dp, s);
	return(0);		/* return successfully */
}



/*
 * close a port
 */
static int
du_close(queue_t *rq, int flag, struct cred *crp)
{
	dport_t *dp = (dport_t *)rq->q_ptr;
	int s = LOCK_PORT(dp);

	if (dp->dp_state & DP_SE_PENDING) {	/* allocating buffer */
		dp->dp_state &= ~DP_SE_PENDING;	/* not anymore! */
		str_unbcall(rq);
	}

	du_flushr(dp);
	du_flushw(dp);
	dp->dp_state &= ~(DP_ISOPEN | DP_WOPEN);
	dp->dp_rq = NULL;
	dp->dp_wq = NULL;
	du_zap(dp, dp->dp_cflag & HUPCL);

	UNLOCK_PORT(dp, s);
}



/*
 * send a bunch of 1 or more characters up the stream
 */
static void
du_rsrv(queue_t *rq)
{
	mblk_t *bp;
	dport_t *dp = (dport_t *)rq->q_ptr;
	int s = LOCK_PORT(dp);

	dr_printf(("in du_rsrv\n"));

	/* dsetia: maybe use cannextput? see page 91 of dd ref guide */
	if (!canput(rq->q_next)) {	/* quit if upstream congested */
		noenable(rq);	/* do not schedule this service routine */
		UNLOCK_PORT(dp, s);
		return;
	}
	enableok(rq);	/* no congestion, so enable service */
	if (dp->dp_state & DP_SE_PENDING) {	/* buffer alloc event pending */
		dp->dp_state &= ~DP_SE_PENDING;
		str_unbcall(rq);
	}

	/* 
	 * when we do not have an old buffer to send up, or when we are
	 * timing things, send the current buffer
	 */
	dr_printf(("service: rbp 0x%lx rmsg 0x%lx rsrv_cnt %d\n", 
		dp->dp_rbp, dp->dp_rmsg, dp->dp_rsrv_cnt));

	bp = dp->dp_rbp;	/* current input buffer */
	if (bp && bp->b_wptr > bp->b_rptr
	    && (!dp->dp_rmsg || !dp->dp_rsrv_cnt)) {
		str_conmsg(&dp->dp_rmsg, &dp->dp_rmsge, bp);
		dp->dp_rmsg_len += (bp->b_wptr - bp->b_rptr);
		dp->dp_rbp = NULL;
	}

	bp = dp->dp_rmsg;
	if (bp) {
		/* update "recent" msg len */
		if (dp->dp_rmsg_len > dp->dp_rbsize)
			dp->dp_rbsize = dp->dp_rmsg_len;	
		else
			dp->dp_rbsize = (dp->dp_rmsg_len + dp->dp_rbsize) / 2;
		dp->dp_rmsg_len = 0;
		dp->dp_rmsg = NULL;
		UNLOCK_PORT(dp, s);	/* without too much blocking, */
		putnext(rq, bp);	/* send the message */
		s = LOCK_PORT(dp);
	}

	/* unblock if currently blocked */
	mips_du_iflow(dp, SEND_XON, !IGNORE_IXOFF);

	/* get a buffer now, rather than waiting for an interrupt */
	if (!dp->dp_rbp)
		(void)du_getbp(dp, BPRI_LO);

	UNLOCK_PORT(dp, s);
}



/*
 * 'put' function, just start the output if we like the message.
 */
static int
du_wput(queue_t *wq, mblk_t *bp)
{
	dport_t *dp = (dport_t *)wq->q_ptr;
	struct iocblk *iocp;
	unchar c;
	int s = LOCK_PORT(dp);
	int i;

	dt_printf(("wput received mblk, type:0x%x\n", bp->b_datap->db_type));

	switch (bp->b_datap->db_type) {

	/* streams, pg b-16. flush message queues */
	case M_FLUSH:	/* XXX may not want to restart output since flow
			   control may be messed up */
		c = *bp->b_rptr;	/* first byte of message has flags */
		sdrv_flush(wq, bp);
		if (c & FLUSHW) {
			du_flushw(dp);
			dp->dp_state &= ~DP_TXSTOP;
			mips_du_start_tx(dp);	/* restart output */
		}
		if (c & FLUSHR)
			du_flushr(dp);
		break;

	case M_DATA:
	case M_DELAY:
		du_save(dp, wq, bp);
		break;

	case M_IOCTL:
		iocp = (struct iocblk *)bp->b_rptr;
		switch (iocp->ioc_cmd) {
		case TCXONC:
			switch (*(int *)(bp->b_cont->b_rptr)) {
			case 0:			/* TCOOFF? stop output */
				dp->dp_state |= DP_TXSTOP;
				mips_du_stop_tx(dp);
				break;
			case 1:			/* TCOON? resume output */
				dp->dp_state &= ~DP_TXSTOP;
				mips_du_start_tx(dp);
				break;
			case 2:			/* TCIOFF? suspend input */
				mips_du_iflow(dp, SEND_XOFF, IGNORE_IXOFF);
				break;
			case 3:			/* TCION? restart suspended input */
				mips_du_iflow(dp, SEND_XON, IGNORE_IXOFF);
				break;
			default:
				iocp->ioc_error = EINVAL;
				break;
			}
			iocp->ioc_count = 0;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq, bp);
			break;

		case TIOCMGET:	/* get all modem bits */
			if (dp->du_mcr & MCR_RTS) 
				i |= TIOCM_RTS;
			if (dp->du_mcr & MCR_DTR) 
				i |= TIOCM_DTR;
			dp->du_msr = c = read_port(REG_MSR);
			if (c & MSR_DCD)
				i |= TIOCM_CD;
			if (c & MSR_CTS)
				i |= TIOCM_CTS;
			if (c & MSR_DSR)
				i |= TIOCM_DSR;
			if (c & MSR_RI)
				i |= TIOCM_RI;
			*(int*)(bp->b_cont->b_rptr) = i;	/* return value */
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			break;

		case TCGETA: /* XXXrs - Don't wait until output? */
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			tcgeta(dp->dp_wq,bp, &dp->dp_termio);
			break;

		case TCSETA: /* XXXrs - Don't wait until output? */
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			(void)du_tcset(dp,bp);
			qreply(wq,bp);
			break;

		case SIOC_ITIMER:	/* set input timer */
		case TCSBRK:	/* send BREAK */
		case TCSETAF:	/* drain output, flush input, set parameters */
		case TCSETAW:	/* drain output, set paramaters */
			du_save(dp, wq, bp);
			break;

		case SIOC_RS422:	/* select/de-select RS422 protocol */
		case SIOC_EXTCLK:	/* select/de-select external clock */
		case SIOC_LOOPBACK:	/* select/de-select loop test mode */
			if ((dp->dp_index != maxport)
			    && (dp->dp_index != maxport - 1)) {
				iocp->ioc_error = EINVAL;
				iocp->ioc_count = 0;
				bp->b_datap->db_type = M_IOCACK;
				qreply(wq, bp);
				break;
			}

			du_save(dp, wq, bp);
			break;

		case TCBLKMD:
			dp->dp_iflag |= IBLKMD;
			iocp->ioc_count = 0;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq, bp);
			break;

		default:
			bp->b_datap->db_type = M_IOCNAK;
			qreply(wq, bp);
			break;
		}
		break;


	default:
		sdrv_error(wq, bp);
	}

	UNLOCK_PORT(dp, s);
}

/*
 * Handle a dma interrupt from the specified channel.
 * Returns:
 *	0 if nothing to do on the specified duart
 *	1 if something was done
 *
 * I am not sure if I need to register seperate handler for each
 * port. For now, I assume I have a common handler and will try
 * to identify the intr source. 
 */
static int
du_handle_dma_intr(int duartnum)
{
	int s;
	long long pisr;
	dport_t *dp;

	/* read the peripheral interrupt status and mask register */
	pisr = READ_REG64(PERIPHERAL_STATUS_REGISTER, long long);

	if (pisr & ISA_SERIAL0_MASK) {	/* intr on serial port 0 */

		dp = &dports[0];
		s = LOCK_PORT(dp); 

		if (pisr & ISA_SERIAL0_Tx_THIR)	{ /* ThresHold intr request */
			du_tx(dp, 0);
		} else 
		if (pisr & ISA_SERIAL0_Rx_THIR)	/* ThresHold intr request */
			du_rx(dp);
		else
		if (pisr & ISA_SERIAL0_Tx_MEMERR)	/* transmit DMA memory error */
			cmn_err(CE_WARN, "!DMA memory error on serial port %d", dp->dp_index);
		else
		if (pisr & ISA_SERIAL0_Rx_OVERRUN) /* receive DMA overrun */
			cmn_err(CE_WARN, "!DMA Overrun error on serial port %d", dp->dp_index);
		else
		if (pisr & ISA_SERIAL0_DIR) {	/* Device Interrupt Request */
			/* should only get modem interrupts directly */
			du_modem(dp);
		}
		UNLOCK_PORT(dp, s);
	}
	else if (pisr & ISA_SERIAL1_MASK) { /* port 1 interrupt */

		dp = &dports[1];
		s = LOCK_PORT(dp); 

		if (pisr & ISA_SERIAL1_Tx_THIR)	/* ThresHold intr request */
			du_tx(dp, 0);
		else 
		if (pisr & ISA_SERIAL1_Rx_THIR)	/* ThresHold intr request */
			du_rx(dp);
		else
		if (pisr & ISA_SERIAL1_Tx_MEMERR)	/* transmit DMA memory error */
			cmn_err(CE_WARN, "!DMA memory error on serial port %d", dp->dp_index);
		else
		if (pisr & ISA_SERIAL1_Rx_OVERRUN) /* receive DMA overrun */
			cmn_err(CE_WARN, "!DMA Overrun error on serial port %d", dp->dp_index);
		else
		if (pisr & ISA_SERIAL1_DIR) {	/* Device Interrupt Request */
			/* should only get modem interrupts directly */
			du_modem(dp);
		}
		UNLOCK_PORT(dp, s);
	}
	else 
		return 0;

	return 1; 
}


/*
 * Handle an interrupt from the specified duart.
 * Returns:
 *	0 if nothing to do on the specified duart
 *	1 if something was done
 *
 * This routine allows for code sharing between Everest, IP20, and
 * IP12 platforms.  The latter uses a single interrupt source
 * for multiple duarts, and therefore needs to poll each duart
 * to find which one actually needs servicing.  The latter
 * platform uses separate interrupts for each duart.
 */

static int
du_handle_intr(int duartnum)
{
	dport_t *dp = &dports[duartnum];

	int i;
	u_char rr0;
	u_char isr, lsr, intr_id;
	int s = LOCK_PORT(dp); 		/* (and dp1) */

	isr = read_port(REG_ISR);
	lsr = read_port(REG_LSR);
	intr_id = isr & 0x0e;

	dprintf(("int isr 0x%x, lsr 0x%x, id 0x%x\n", isr, lsr, intr_id));

	if ((isr & ISR_MSTATUS) || !intr_id) {	/* no intr */
		dprintf(("spurious intr\n"));
		UNLOCK_PORT(dp, s);
		return(0);
	}

	/* receive character/error interrupt */
	if (intr_id == ISR_RxRDY || intr_id == ISR_FFTMOUT 
		|| intr_id == ISR_RSTATUS || lsr == LSR_RCA) {
		SYSINFO.rcvint++;
		du_rx(dp);
	} else 

	/* transmit buffer empty interrupt */
	if (intr_id == ISR_TxRDY) {
		SYSINFO.xmtint++;
		dp->dp_char_xmitted = 0;
		du_tx(dp, 0);
	} else

	/* modem interrupt */
	if (intr_id == ISR_MSTATUS)
		du_modem(dp);

	UNLOCK_PORT(dp, s);
	return(1);
}


/*
 * Examine each duart to see if it is the one that needs servicing.
 */
static void
du_poll()
{
	int i;
	int worked;

	do {
		worked = 0;
		for (i = 0; i < maxport; i++)
			worked = du_handle_dma_intr(i);
	} while (worked);
}




/*
 * get a new buffer, should only be called when safe from interrupts
 */
static mblk_t*				/* return NULL or the new buffer */
du_getbp(dport_t *dp, u_int pri)
{
	mblk_t *bp;
	mblk_t *rbp;
	int size;
	int size0;

	rbp = dp->dp_rbp;
	/* if overflowing or current buffer empty, do not need another buffer */
	if (dp->dp_rmsg_len >= MAX_RMSG_LEN
	    || rbp && rbp->b_rptr >= rbp->b_wptr) {
		bp = NULL;

	} else {
		/*
		 * get another buffer, but always keep room to grow.
		 * this helps prevent deadlock
	 	 */
		size0 = (dp->dp_rbsize += dp->dp_rbsize / 4);
		if (size0 > MAX_RBUF_LEN)
			size0 = dp->dp_rbsize = MAX_RBUF_LEN;
		else if (size0 < MIN_RMSG_LEN)
			size0 = MIN_RMSG_LEN;
		size = size0;
		for ( ; ; ) {
			bp = allocb(size, pri);
			if (bp)
				break;
			if (pri == BPRI_HI && (size >>= 2) >= MIN_RMSG_LEN)
				continue;
			if (!(dp->dp_state & DP_SE_PENDING)) {
				bp = str_allocb(size0, dp->dp_rq, BPRI_HI);
				if (!bp)
					dp->dp_state |= DP_SE_PENDING;
			}
			break;
		}
	}

	if (!rbp)
		dp->dp_rbp = bp;
	else if (bp || rbp->b_wptr >= rbp->b_datap->db_lim) {
		/*
		 * we have an old buffer and a new buffer, or old buffer is
		 * full 
		 */
		str_conmsg(&dp->dp_rmsg, &dp->dp_rmsge, rbp);
		dp->dp_rmsg_len += (rbp->b_wptr - rbp->b_rptr);
		dp->dp_rbp = bp;
	}

	if (dp->dp_rmsg_len >= XOFF_RMSG_LEN || !dp->dp_rbp)
		mips_du_iflow(dp, SEND_XOFF, !IGNORE_IXOFF);

	return(bp);
}



/*
 * slow and hopefully infrequently used function to put characters somewhere
 * where they will go up stream
 */
static void
du_slowr(dport_t *dp, u_char c)
{
	mblk_t *bp;

	if (dp->dp_iflag & IBLKMD)	/* this kludge apes the old */
		return;			/* block mode hack */

	if (!(bp = dp->dp_rbp)		/* get buffer if have none */
	    && !(bp = du_getbp(dp, BPRI_HI))) {
		dp->dp_allocb_fail++;
		return;
	}
	*bp->b_wptr = c;
	if (++bp->b_wptr >= bp->b_datap->db_lim) {
		(void)du_getbp(dp, BPRI_LO);	/* send buffer when full */
	}
}



/*
 * input interrupt
 */
#ifdef _INTR_TIME_TRACING
#define DU_RX_TRACE_MAX 500
struct du_rx_trace_s {
	time_t	timestamp;
	unsigned char	ichar;
	unsigned char	sr;
} du_rx_trace[DU_RX_TRACE_MAX];
int	du_rx_trace_next = 0;
#endif

static void
du_rx(dport_t *dp)
{
	int c;
	u_char sr;
	volatile u_char *cntrl = dp->dp_cntrl;
	mblk_t *bp;

	/* must be open to input */
	if ((dp->dp_state & (DP_ISOPEN | DP_WOPEN)) != DP_ISOPEN
	    && dp->dp_porthandler == NULL) {
		if (!(dp->dp_state & (DP_ISOPEN | DP_WOPEN)))
			du_zap(dp, dp->dp_cflag & HUPCL);
		else
			du_rclr(dp);	/* just forget it if partly open */
		return;
	}

	/* must be reading, to read.
	if (!(dp->dp_cflag & CREAD)) { 
		du_rclr(dp);
		return;
	}

	dr_printf(("in du_rx\n"));

	/* process all available characters */
	while ((c = dma_rx_char(dp)) != -1 ) {
		SYSINFO.rawch++;

		sr = c >> 8;
		c &= 0xff;

		/* if we get a control-A, debug on this port */
		if (kdebug && !console_is_tport &&
		     c == DEBUG_CHAR) {
			debug("ring");
			continue;
		}

#ifdef DEBUG
		if (dp->dp_porthandler) {
	    		if (sr)
				cmn_err(CE_WARN, "Port %d, sr:0x%x\n",
					dp->dp_index, sr);
			if ((*dp->dp_porthandler)(c))
				continue;
		}
#else
		if (dp->dp_porthandler && (*dp->dp_porthandler)(c))
			continue;
#endif /* DEBUG */

		/* start/stop output (if permitted) when we get XOFF or XON */
		if (dp->dp_iflag & IXON) {
			u_char cs = c;

			if (dp->dp_iflag & ISTRIP) 		/* strip to 7 bits */
				cs &= 0x7f;

			if ( dp->dp_state & DP_TXSTOP ) {
			    if ( cs == dp->dp_cc[VSTART] || dp->dp_iflag & IXANY ) {
					dp->dp_state &= ~DP_TXSTOP; 
					dr_printf(("received a start char\n"));
					mips_du_start_tx(dp);	/* restart output */
					if (cs == dp->dp_cc[VSTART]) /* eat it */
						continue;
				}
			} else if ( cs == dp->dp_cc[VSTOP] ) {
				dp->dp_state |= DP_TXSTOP;
				dr_printf(("received a stop char\n"));
				mips_du_stop_tx(dp);	/* stop output */
			} else { 
				if ( cs == dp->dp_cc[VLNEXT] && dp->dp_line != LDISC0 )
					dp->dp_state |= DP_LIT;	/* just note escape */
				else if (DP_LIT & dp->dp_state)
					dp->dp_state &= ~DP_LIT;
			}
			if ( cs == dp->dp_cc[VSTART] || cs == dp->dp_cc[VSTOP] )
				continue;	/* ignore extra start-stop */
		}


		if ( sr & DMA_IC_PARERR && !( dp->dp_iflag & INPCK)) {
			dr_printf(("ignore PARERR (INPCK not set)\n"));
			sr &= ~DMA_IC_PARERR;
		}

		if ( sr & (DMA_IC_BRKDET | DMA_IC_FRMERR | DMA_IC_OVRRUN | DMA_IC_PARERR)) {
			if (sr & DMA_IC_OVRRUN)
				dp->dp_over++;
			if (sr & DMA_IC_FRMERR)
				dp->dp_fe++;

            if (( c & 0377) == 0) {
				dr_printf(("received BREAK\n"));
                if ( dp->dp_iflag & IGNBRK) {
					dr_printf(("ignore BRKEAK (IGNBRK set)\n"));
                    continue;
				}

                putctl( dp->dp_rq->q_next, M_BREAK);
				if ( dp->dp_iflag & BRKINT ) {
					du_flushr(dp);
					(void)putctl1(dp->dp_rq->q_next, M_FLUSH, FLUSHRW);
					(void)putctl1(dp->dp_rq->q_next, M_PCSIG, SIGINT);
                	continue;
				}
            } else {
                if ( dp->dp_iflag & IGNPAR ) {
					dr_printf(("ignore parity; IGNPAR set\n"));
                    continue;
				}
            }

			if (dp->dp_iflag & PARMRK) {
				du_slowr(dp, '\377');
				du_slowr(dp, '\000');
			} else {
				c = '\000';
			}
		} else {
			if ( dp->dp_iflag & ISTRIP )
				c &= 0x7f;
			else {
				c &= 0377;
				if ( c == '\377' && dp->dp_iflag & PARMRK )
					du_slowr(dp, '\377');
			}
		}


		if (!(bp = dp->dp_rbp)	/* get a buffer if we have none */
		    && !(bp = du_getbp(dp, BPRI_HI))) {
			dp->dp_allocb_fail++;
			continue;	/* forget it no buffer available */
		}
		*bp->b_wptr = c;
		if (++bp->b_wptr >= bp->b_datap->db_lim) {
			(void)du_getbp(dp, BPRI_LO);	/* send when full */
		}
	}


	if (!dp->dp_rsrv_cnt && dp->dp_rbp) {
		if (dp->dp_rsrv_duration <= 0
		    || dp->dp_rsrv_duration > HZ / 10) {
			if (dp->dp_state & DP_ISOPEN && canenable(dp->dp_rq))
				qenable(dp->dp_rq);
		} else {
			(void)STREAMS_TIMEOUT(du_rsrv_timer,
			    (caddr_t)dp, dp->dp_rsrv_duration);
			dp->dp_rsrv_cnt = MAX_RSRV_CNT;
		}
	}
}


/*
 * process Modem Status register changes
 */
void
du_modem(dport_t *dp)
{
	u_char msr;

	msr = read_port(REG_MSR);	/* and reset the interrupt */

	/* handle CTS intr only if we care */
	if (dp->dp_cflag & CNEW_RTSCTS)	{
		if (msr & MSR_CTS) {
			dp->dp_cflag |= DP_CTS;
			mips_du_start_tx(dp);
		}
		else {
			/* no xmit fifo intr causes DMA to stop automaticlly */
			SYSINFO.mdmint++;
			dp->dp_cflag &= ~DP_CTS;
			mips_du_stop_tx(dp);
		}
	}

	/* handle DCD intr only if we care */
	if (!(dp->dp_cflag & CLOCAL)) {
		if (msr & MSR_DCD) { /* carrier on */
			dp->dp_state |= DP_DCD;
			du_con(dp);
		} 
		else { /* carrier off */
			if (dp->dp_state & DP_ISOPEN && dp->dp_state & DP_DCD)
				du_coff(dp);
			dp->dp_state &= ~DP_DCD;
		}
	}
}

/*
 * process carrier-on interrupt
 */
static void
du_con(dport_t *dp)
{
	if (dp->dp_state & DP_WOPEN)
		wakeup((caddr_t)dp);	/* awaken open() requests */
}



/*
 * process carrier-off interrupt
 */
static void
du_coff(dport_t *dp)
{
	du_zap(dp, HUPCL);	/* kill the modem */
	flushq(dp->dp_wq, FLUSHDATA);
	(void)putctl1(dp->dp_rq->q_next, M_FLUSH, FLUSHW);
	(void)putctl(dp->dp_rq->q_next, M_HANGUP);
}



/*
 * output something
 */
static void
du_tx(dport_t *dp, int prime)
{
	u_char c;
	mblk_t *wbp;
	int count, rdepth, len, local_fifo_size;
	volatile u_char *cntrl = dp->dp_cntrl;
	int xmitted = 0, towrite;

	/* if we can't transmit now, just return */
	if (cant_xmit(dp)) {
		dt_printf(("tx disabled, can't start tx now\n"));
		mips_du_stop_tx(dp);
		return;
	}

	/* if not clear to send, just return */
	if (dp->dp_cflag & CNEW_RTSCTS && !(dp->dp_state & DP_CTS)) {
		mips_du_stop_tx(dp);
		return;
	}

	/* calculate free blocks available to write and
	 * if ring buffer is full, just return
	 */
	count = MAX_RING_BLOCKS - READ_ISA_IF(tx_depth); 
	if (!count)
		return;

	count <<= 4;	/* each block can store 16 data bytes */
	local_fifo_size = count;

	dt_printf(("du_tx: send %d (max) bytes\n", count));

	if (dp->dp_state & (DP_TX_TXON | DP_TX_TXOFF)) {
		if (dp->dp_state & DP_TX_TXON) {	/* send XON or XOFF */
			if (dp->dp_cflag & CNEW_RTSCTS
		    	&& !(dp->dp_state & DP_CTS)) {
				mips_du_stop_tx(dp);
				return;
			}
			c = dp->dp_cc[VSTART];
			dp->dp_state &= ~(DP_TX_TXON | DP_TX_TXOFF | DP_BLOCK);
		} else if (dp->dp_state & DP_TX_TXOFF) {
			if (dp->dp_cflag & CNEW_RTSCTS
		    	&& !(dp->dp_state & DP_CTS)) {
				mips_du_stop_tx(dp);
				return;
			}
			c = dp->dp_cc[VSTOP];
			dp->dp_state &= ~DP_TX_TXOFF;
			dp->dp_state |= DP_BLOCK;
		} 
		du_tx_chars(dp, &c, 1);
		dp->dp_char_xmitted += 1;	/* in blocks */
		return;
	}

	/* send all we can */
	while (count) {
		SYSINFO.outch++;
		if (!(wbp = dp->dp_wbp)) {
			wbp = getq(dp->dp_wq);
			if (!wbp) {
				/* if we didn't xmit anything this time, we want to stop
				 * transmission. This involved disabling Tx DMA operation
				 * as well as interrupts on this port.
				 */
				if (!xmitted)
					mips_du_stop_tx(dp);
				return;
			}

			switch (wbp->b_datap->db_type) {
			case M_DATA:
				dp->dp_wbp = wbp;
				break;

				/* must wait until output drained */
			case M_DELAY:	/* start output delay */
				if (count == local_fifo_size) { /* fifo full now */
					dp->dp_state |= DP_TIMEOUT;
					dp->dp_tid = STREAMS_TIMEOUT(du_delay,
			    			(caddr_t)dp,
			    			*(int *)wbp->b_rptr); /* num ticks */
					freemsg(wbp);
					continue;
				} else {
					/* m_delay is an ordinary message, not high priority. */
					putbq(dp->dp_wq, wbp);
					return;
				}

				/* must wait until output drained */
			case M_IOCTL:
				if (count == local_fifo_size) {
					du_i_ioctl(dp, wbp);
					continue;
				} else {
					putbq(dp->dp_wq, wbp);
					return;
				}

			default:
				panic("bad duart msg");
			}
		}

		if (wbp->b_rptr >= wbp->b_wptr) {
			dp->dp_wbp = rmvb(wbp, wbp);
			freeb(wbp);
			continue;
		}

		len = min(wbp->b_wptr - wbp->b_rptr, count);

		du_tx_chars(dp, wbp->b_rptr, len);
		wbp->b_rptr += len;
		count -= len;
		xmitted++;

		/* even though we could complete transmitting count bytes
		 * and then flush the DMA channel before returning, it is
		 * probably safe to flush whatever we have transmitted in
		 * each iteration. If this turns out to be a performance
		 * issue, I may do just one flush at the end
		 */
	}


	/*
	 * If the queue is now empty, put an empty data block on it
	 * to prevent a close from finishing prematurely.
	 */
	if (!dp->dp_wq->q_first && dp->dp_wbp && (wbp = allocb(0, BPRI_HI)))
		putbq(dp->dp_wq, wbp);
}



/*
 * read up to `len' characters from the secondary console port.
 */
int
ducons_read(u_char *buf, int len)
{
	int c;
	dport_t *dp = &dports[du_console];
	int i = 0;
	int s;

	s = LOCK_PORT(dp);

	while (i < len) {
		if ((c = mips_du_getchar(dp)) == -1)
			break;
		*buf++ = c & 0xff;		/* ignore status */
		i++;
	}

	UNLOCK_PORT(dp, s);

	return(i);
}

/*
 * write `len' characters to the secondary console port, should only be
 * called when safe from interrupt.  in most cases, use the kernel printf()
 * instead of calling this routine directly
 */
int
ducons_write(u_char *buf, int len)
{
	dport_t *dp = &dports[du_console];
	int i;
	int s;

	s = LOCK_PORT(dp);
	for (i = 0; i < len; i++) {
		while (!mips_du_txrdy(dp))	/* poll until ready */
			;
		mips_du_putchar(dp, *buf);
		if (*buf == '\n') {
			while (!mips_du_txrdy(dp))
				;
			mips_du_putchar(dp, '\r');
		}
		buf++;
	}
	UNLOCK_PORT(dp, s);
	return(len);
}
void ducons_flush(void) {}



/*
 * du_conpoll - see if a receive char - used for debugging ONLY
 * NOTE: since we toss anything but ^A, this can lose chars on the
 * console (small price to pay for being able to get into debugger)
 */
void
du_conpoll(void)
{
	int c;
	dport_t *dp = &dports[du_console];
	int s;

	if (!kdebug || console_is_tport)
		return;

	s = LOCK_PORT(dp);

	if ((c = mips_du_getchar(dp)) != -1) {
		if ((c & 0xff) == DEBUG_CHAR)
			debug("ring");
	}

	UNLOCK_PORT(dp, s);
}



int
du_getchar(int port)
{
	int c;
	int s;

	s = spltty();
	if ((c = mips_du_getchar(&dports[port])) != -1)
		c &= 0xff;
	splx(s);

	return(c);
}



void
du_putchar(int port, unsigned char c)
{
	dport_t *dp = &dports[port];
	int s;

	s = splhi();
	while (!mips_du_txrdy(dp))
		;
	mips_du_putchar(dp, c);
	splx(s);
}


/*
 * determine whether the DUART port is ready to transmit a character or not.
 * 0 => not ready
 */
static int
mips_du_txrdy(dport_t *dp)
{
	return (read_port(REG_LSR) & LSR_XHRE);
}


/*
 * transmit a character. du_txrdy must be called first to make sure the
 * transmitter is ready
 */
static void
mips_du_putchar(dport_t *dp, u_char c)
{
	write_port(REG_DAT, c);
}

/*
 * get a character from the receiver. if none received, return -1.
 * append status to data
 */
static int
mips_du_getchar(dport_t *dp)
{
	int c;

	if (read_port(REG_LSR) & LSR_RCA) { 	/* data ready */
		return (read_port(REG_DAT));
	}
	else 
		return -1;
}


/* disable transmit dma and transmitt threshold interrupt */
static void
mips_du_stop_tx(dport_t *dp)
{
	long long x;
	dt_printf(("stop tx\n"));

	if (dp->dp_state & DP_DMA) {
		x = READ_ISA_IF(tx_ctrl);
		x &= ~DMA_INT_MASK;
		WRITE_ISA_IF(tx_ctrl, x);
		dma_tx_interrupt(dp, 0);
	} else {
		dp->du_icr &= ~ICR_TIEN;
		write_port(REG_ICR, dp->du_icr);
	}
}




/*  start the output
 *  Call here only with interrupts safe.
 */
static void
mips_du_start_tx(dport_t *dp)
{
	long long x;

	if (cant_xmit(dp)) {	/* Xmission not possible for now */
		dt_printf(("tx disabled, can't start tx now\n"));
		return;
	}

	dt_printf(("start tx\n"));

	/*
	 * if in dma mode, disable uart transmit intrs 
	 */
	if (dp->dp_state & DP_DMA) {
		dp->du_icr &= ~ICR_TIEN;
		write_port(REG_ICR, dp->du_icr);
		x = (DMA_ENABLE | DMA_INT_EMPTY); 
		WRITE_ISA_IF(tx_ctrl, x);
		dma_tx_interrupt(dp, 1);
	}
	else {
		dp->du_icr |= ICR_TIEN;
		write_port(REG_ICR, dp->du_icr);
	}

	/* Transmitter empty interrupt will start xmission */
}



static void
mips_du_stop_rx(dport_t *dp, int hup)
{
	long long x;
	u_char *console = kopt_find("console");

	dr_printf(("stop rx\n"));

	if (!(kdebug &&
	      (dp->dp_index == SCNDCPORT || dp->dp_index == PDBXPORT))) {

		/* disable rx dma */
		x = READ_ISA_IF(rx_ctrl);
		x &= ~(DMA_ENABLE | DMA_INT_MASK); 
		WRITE_ISA_IF(rx_ctrl, x);
		dma_rx_interrupt(dp, 0);

		dp->du_icr &= ~(ICR_RIEN | ICR_SIEN);
		write_port(REG_ICR, dp->du_icr);
	}

	/* deassert RTS, DTR if hup != 0 */
	if (hup && !(console && *console == 'd' &&
		 ((dp->dp_index == SCNDCPORT) || (dp->dp_index == PDBXPORT)))) {

		/* disable modem status interrupts */
		dp->du_icr &= ~ICR_MIEN;
		write_port(REG_ICR, dp->du_icr);

		/* deassert output handshake signals on tty port */
		dp->du_mcr &= ~(MCR_DTR | MCR_RTS);
		write_port(REG_MCR, dp->du_mcr);
		/* XXX do we need to write above bytes into ring buffer with MCR bit set */
	}
}

/*
 * return !0 if carrier detected
 */
static int
mips_du_start_rx(dport_t *dp)
{
	u_char o_icr;
	u_char lsr, msr;

	dr_printf(("start rx\n"));

	/* assert dtr, rts */
	dp->du_mcr |= (MCR_DTR | MCR_RTS);
	write_port(REG_MCR, dp->du_mcr);
	/* XXX do we need to write above bytes into ring buffer with MCR bit set */


	/* 
	 * When in DMA mode, we must disable the receive and line status intrs
	 * DMA will automatically manage them. Otherwise enable data-ready and 
	 * receiver line-status intrs.
	 */
	o_icr = dp->du_icr;
	if (dp->dp_state & DP_DMA) {
		dp->du_icr &= ~(ICR_RIEN | ICR_SIEN);
		write_port(REG_ICR, dp->du_icr);
	}
	else {
		dp->du_icr |= (ICR_RIEN | ICR_SIEN);
		write_port(REG_ICR, dp->du_icr);
	}

	/* watch carrier-detect only if this is an open modem port */
	if (!(dp->dp_cflag & CLOCAL)
	    && dp->dp_state & (DP_ISOPEN | DP_WOPEN)) {
		/* all modem status interrupts including CD, CTS, RI */
		dp->du_icr |= ICR_MIEN;	
		write_port(REG_ICR, dp->du_icr);
	}


	/*
	 * XXX
	 * Not sure how to handle the code below. If port is not in DMA mode,
	 * it is easy to check if previously read was enabled (by looking at
	 * o_icr. However if DMA is enabled, ICR_RIEN would not be set. How
	 * do I determine if DMA reads were previously not enabled?
	 * For now, I am taking this code out.
	 */
#ifdef not_used_code
	/* if now enabling input, gobble old stuff */
	if (!(o_icr & ICR_RIEN))
		du_rclr(dp);
#endif

	msr = read_port(REG_MSR);

	if (msr & MSR_CTS)
		dp->dp_state |= DP_CTS;
	else
		dp->dp_state &= ~DP_CTS;

	if (msr & MSR_DCD)
		dp->dp_state |= DP_DCD;
	else
		dp->dp_state &= ~DP_DCD;

	/* enable rx dma */
	if (dp->dp_state & DP_DMA) {
		long long x;

		x = (DMA_ENABLE | DMA_INT_NEMPTY); 
		WRITE_ISA_IF(rx_ctrl, x);
		dma_rx_interrupt(dp, 1);
	}

	return dp->dp_state & DP_DCD;
}



static void
mips_du_config(dport_t *dp, tcflag_t cflag, speed_t ospeed, struct termio *tp)
{
	volatile u_char *cntrl = dp->dp_cntrl;
	tcflag_t delta_cflag;
	int delta_ospeed;
	u_short time_constant;
	extern int baud_to_cbaud(speed_t);	       /* in streamio.c */

	delta_cflag = (cflag ^ dp->dp_cflag)
		& (CLOCAL|CSIZE|CSTOPB|PARENB|PARODD|CNEW_RTSCTS);
	if (ospeed == __INVALID_BAUD)
		ospeed = dp->dp_ospeed;
	delta_ospeed = ospeed != dp->dp_ospeed;

	if (tp)
		dp->dp_termio = *tp;
	dp->dp_cflag = cflag;
	dp->dp_ospeed = dp->dp_ispeed = ospeed;

	if (delta_cflag || delta_ospeed) {
		if (ospeed == B0) {	/* hang up line if asked */
			du_coff(dp);
			du_zap(dp, HUPCL);

		} else {
			unsigned short time_constant;
			/* set number of stop bits */
			dp->du_lcr |= (cflag & CSTOPB) ? LCR_STOP2 : LCR_STOP1;

			/* set number of data bits */
			dp->du_lcr |= (cflag & CSIZE) >> 4;

			/* set parity */
			if (cflag & PARENB) {
				dp->du_lcr |= LCR_PAREN;	/* enable parity */
				if (!(cflag & PARODD))
					dp->du_lcr |= LCR_PAREVN;	/* even parity */
			}

			/* set time constant according to baud rate */
			write_port(REG_LCR, LCR_DLAB);	/* enable data latch */
			write_port(REG_SCR, ACE_PRESCALER_DIVISOR);
			time_constant = BAUD(ospeed);
			write_port(REG_DLH, time_constant >> 8);
			write_port(REG_DLL, time_constant);

			write_port(REG_LCR, dp->du_lcr); /* and clear latch */

			/* XXX du_start will not flush rceive fifo becuz RIEN is set above */
			(void)mips_du_start_rx(dp);
		}
	}
}


#define send_xoff !send_xon

/* perform input flow control */
static void
mips_du_iflow(dport_t *dp, int send_xon, int ignore_ixoff)
{
	dr_printf(("perform inout flow control\n"));

	/* assert/deassert RTS for h/w flow control */
	if (dp->dp_cflag & CNEW_RTSCTS) {
		if (send_xon && !DU_RTS_ASSERTED(dp->du_mcr)) {
			DU_RTS_ASSERT(dp->du_mcr);
			write_port(REG_MCR, dp->du_mcr);
		} else if (send_xoff && DU_RTS_ASSERTED(dp->du_mcr)) {
			DU_RTS_DEASSERT(dp->du_mcr);
			write_port(REG_MCR, dp->du_mcr);
		}
	}

	/* 
	 if IXOFF is not be ignored (ignore_ixoff is not set) and
     the inout flow control is not set, we don't have much
     to do.
	*/
	if (ignore_ixoff || dp->dp_iflag & IXOFF) {
		if (send_xon && dp->dp_state & DP_BLOCK) {
			dp->dp_state |= DP_TX_TXON;
			mips_du_start_tx(dp);
		}
		else if (send_xoff && !(dp->dp_state & DP_BLOCK)) {
			dp->dp_state |= DP_TX_TXOFF;
			dp->dp_state &= ~DP_TX_TXON;
			mips_du_start_tx(dp);
		}
	}
}
#undef send_xoff



/* 
 * send break. write directly to isa port irrespective of dma mode.
 */
static void
mips_du_break(dport_t *dp, int start_break)
{
	dprintf(("break %s\n", (start_break ? "start" : "stop")));

	if (start_break)
		dp->du_lcr |= LCR_SETBREAK;
	else
		dp->du_lcr &= ~LCR_SETBREAK;
	write_port(REG_LCR, dp->du_lcr);
}


static int
du_loopback(dport_t *dp, mblk_t *bp)
{
	u_int enable = *(u_int *)(bp->b_cont->b_rptr);

	dprintf(("entering loopback mode\n"));
	/* disable transmitter/receiver automatically done by uart */

	/* set loopback mode */
	if (enable) {
		dp->du_mcr |= MCR_LOOP;
		dp->dp_state |= DP_LOOP;
	} else {
		dp->du_mcr &= ~MCR_LOOP;
		dp->dp_state &= ~DP_LOOP;
	}
	write_port(REG_MCR, dp->du_mcr);

	return 0;
}


/*
 * can't transmit when:
 *
 * 1) device is not opened yet
 * 2) we are sending a break char
 * 3) transmission is stopped (with VSTOP cc)
 */

int cant_xmit(dport_t *dp)
{
	return (!(dp->dp_state & DP_ISOPEN)
	    || dp->dp_state & (DP_BREAK | DP_BREAK_QUIET)
	    || dp->dp_state & (DP_TXSTOP | DP_TIMEOUT)
		&& !(dp->dp_state & (DP_TX_TXON | DP_TX_TXOFF)));
}

/*
 * write a character into the ring buffer. Caller assumes space is
 * available in ring buffer.
 *
 * The smallest unit for DMA transfer is a 64-bit (8-byte) chunk which
 * consist of 4 data bytes and 4 control bytes. DMA engine transfers 4
 * of these 8-bytes chunks at a time.
 *
 * To xfer data to DMA, we first pack incoming bytes into 64-bit chunks.
 * When it is filled up, we write it to ring buffer. When we have written
 * 4 of these (i.e transferred 32 bytes) we increment the ring buffer ptr
 * which is aligned on 32-byte boundry in the ring buffers.
 * 
 */

/* transmit a 4 byte data block */
dma_tx_block(dport_t *dp, unsigned char *buf)
{
	int i; 
	__uint32_t *wp;
	unsigned char *cp;

	wp = (__uint32_t *) dp->tx_wptr;
	*wp++ = 0x40404040;	/* DMA_OC_WTHR x 4 */

	/* since the buf may not be aligned on a word boundry, I cannot simply
	 * cast it and read off a word. Thus the following will not work.
	 *
	 * *cp++ = *(__uint32_t *) buf;
	 *
	 */
	cp = (unsigned char *) wp;
	*cp++ = *buf++; *cp++ = *buf++; *cp++ = *buf++; *cp++ = *buf++;

	if (((__uint64_t) ++dp->tx_wptr % 32) == 0)
		INC_XMT_PTR(dp);
}

/* transmit a few bytes (less than 4) */
dma_tx_bytes(dport_t *dp, char *buf, int len)
{
	int i;
	unsigned char *p, *q;

	p = (unsigned char *) dp->tx_wptr;
	q = p + 4;

	/* fill real data bytes first */
	for (i=0; i<len; i++) {
		*p++ = DMA_OC_WTHR;
		*q++ = *buf++;
	}

	/* now pad with invalid data bytes */
	for (; i<4; i++)
		*p++ = DMA_OC_INVALID;

	if (((__uint64_t) ++dp->tx_wptr % 32) == 0)
		INC_XMT_PTR(dp);
}

static void
dma_tx_flush(dport_t *dp)
{
	int blocks;

	/* numbers of 64-bit blocks remaining is calulated by
	 * dividing number of bytes we are from next 64-bit 
	 * boundry by 8
	 */
	if (!(blocks = (__uint64_t)dp->tx_wptr % 32))
		return;
	blocks = (32 - blocks) >> 3;
	while (blocks--)
		*dp->tx_wptr++ = 0x0;	/* invalid bytes */
	INC_XMT_PTR(dp);
}

du_tx_chars(dport_t *dp, char *buf, int len)
{
	/* transmit as many full blocks as possible */
	while (len >= 4) {
		dma_tx_block(dp, buf);
		buf += 4;
		len -= 4;
	};

	/* less than a block remaining.
	 * pad the rest with invalid bytes.
	 */
	if (len)
		dma_tx_bytes(dp, buf, len);

	dma_tx_flush(dp);
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

	dp->rx_rcvbuf.next = 0;
	dp->rx_rcvbuf.count = 0;

	ringbufp = (__uint32_t *) dp->rx_rptr;	        /* for src */
	ctrlp = (__uint32_t *) &dp->rx_rcvbuf.ctrl;	/* for control */
	datap = (__uint32_t *) &dp->rx_rcvbuf.data;	/* for data */

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
	while (dp->rx_rcvbuf.ctrl[count-1] == 0)
		count--;
	dp->rx_rcvbuf.count = count;

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
	unsigned char ctrl, data, *cp;
	

	if (!dp->rx_rcvbuf.count) {
		(void) dma_rx_block(dp);
		if (!dp->rx_rcvbuf.count)
			return -1;	/* ring empty */
	}

	next = dp->rx_rcvbuf.next++;
	dp->rx_rcvbuf.count--;

	ctrl = dp->rx_rcvbuf.ctrl[next];
	data = dp->rx_rcvbuf.data[next];

#ifdef DEBUG
	if ( ctrl & DMA_IC_FRMERR )
		cmn_err(CE_WARN, "!Framing error on serial port %d", dp->dp_index);
	if ( ctrl & DMA_IC_OVRRUN )
		cmn_err(CE_WARN, "!Overrun error on serial port %d", dp->dp_index);
	if ( ctrl & DMA_IC_PARERR )
		cmn_err(CE_WARN, "!Parity error on serial port %d", dp->dp_index);
	if ( ctrl & DMA_IC_BRKDET )
		cmn_err(CE_WARN, "!Break signal on serial port %d", dp->dp_index);
	/* dprintf(("getchar: ctrl 0x%x data %c\n", ctrl, data)); */
#endif

	return ((ctrl << 8) | data);
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
	__uint64_t pier;
	int shift_by = 20;

	shift_by += dp->dp_index * 6;	/* there are 6 int bits per port */
	
	pier = READ_REG64(PERIPHERAL_INTMSK_REGISTER, long long);
	if (enable)
		pier |= (0x10 << shift_by);
	else
		pier &= ~(0x10 << shift_by);
	WRITE_REG64(pier, PERIPHERAL_INTMSK_REGISTER, long long);
}

static void
dma_tx_interrupt(dport_t *dp, int enable)
{
	__uint64_t pier;
	int shift_by = 20;

	shift_by += dp->dp_index * 6;	/* there are 6 int bits per port */
	
	pier = READ_REG64(PERIPHERAL_INTMSK_REGISTER, long long);
	if (enable)
		pier |= (0x02 << shift_by);
	else
		pier &= ~(0x02 << shift_by);
	WRITE_REG64(pier, PERIPHERAL_INTMSK_REGISTER, long long);
}

/* -------------------- DEBUGGING SUPPORT -------------------- */
void
dump_dport_info(__psint_t n)
{
	dport_t *dp;
	int i;
	unsigned char status;

	if (n < 0)
		dp = (dport_t *) n;
	else 
		dp = &dports[n];

	qprintf("dport 0x%x (%d)\n", dp, dp->dp_index);
	qprintf("dat 0x%x icr 0x%x fcr 0x%x lcr 0x%x\n",
		dp->du_dat, dp->du_icr, dp->du_fcr, dp->du_lcr);
	qprintf("mcr 0x%x lsr 0x%x msr 0x%x efr 0x%x\n",
		dp->du_mcr, dp->du_lsr, dp->du_msr, dp->du_efr);
	qprintf("cntrl 0x%x ifp 0x%x\n", dp->dp_cntrl, dp->ifp);
	qprintf("transmit ring base 0x%x end 0x%x wptr 0x%x\n",
		dp->tx_ringbase, dp->tx_ringend, dp->tx_wptr);
	qprintf("receive  ring base 0x%x end 0x%x wptr 0x%x\n",
		dp->rx_ringbase, dp->rx_ringend, dp->rx_rptr);
	qprintf("rsrv_cnt %d rmsg 0x%x rmsge 0x%x\n",
		dp->dp_rsrv_cnt, dp->dp_rmsg, dp->dp_rmsge);
	qprintf("rmsg_len %d rbsize %d rbp 0x%x\n",
		dp->dp_rmsg_len, dp->dp_rbsize, dp->dp_rbp);
	qprintf("rq 0x%x wq 0x%x rbp 0x%x wbp 0x%x\n",
		dp->dp_rq, dp->dp_wq, dp->dp_rbp, dp->dp_wbp);
	qprintf("&termio 0x%x iflag 0x%x cflag 0x%x line 0x%x speed %d\n",
		&(dp->dp_termio), dp->dp_iflag, dp->dp_cflag, dp->dp_line,
		dp->dp_ospeed);
	qprintf("state 0x%x %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		dp->dp_state,
		dp->dp_state & DP_ISOPEN ?	"ISOPEN " : "",
		dp->dp_state & DP_WOPEN ?	"WOPEN " : "",
		dp->dp_state & DP_DCD ?		"DCD " : "",
		dp->dp_state & DP_TIMEOUT ?	"TIMEOUT " : "",
		dp->dp_state & DP_BREAK ?	"BREAK " : "",
		dp->dp_state & DP_BREAK_QUIET ?	"BREAK_QUIET " : "",
		dp->dp_state & DP_TXSTOP ?	"TXSTOP " : "",
		dp->dp_state & DP_LIT ?		"LIT " : "",
		dp->dp_state & DP_BLOCK ?	"BLOCK " : "",
		dp->dp_state & DP_TX_TXON ?	"TX_TXON " : "",
		dp->dp_state & DP_TX_TXOFF ?	"TX_TXOFF " : "",
		dp->dp_state & DP_SE_PENDING ?	"SE_PENDING " : "",
		dp->dp_state & DP_MODEM ?	"MODEM " : "");
	qprintf("Errors: frame %d overrun %d allocb_fail %d\n",
		dp->dp_fe, dp->dp_over, dp->dp_allocb_fail);
	qprintf("porthandler 0x%x tid 0x%x\n",
		dp->dp_porthandler, dp->dp_tid);
}
	

void
reset_dport_info(__psint_t n)
{
	qprintf("Not supported.\n");
}

#ifdef DEBUG
/*ARGSUSED*/
void
dump_dport_actlog(__psint_t n)
{
	qprintf("Not supported.\n");
}
#endif /* DEBUG */
