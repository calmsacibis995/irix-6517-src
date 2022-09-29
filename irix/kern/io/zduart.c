/* Copyright 1986,1992, Silicon Graphics Inc., Mountain View, CA. */
/*
 * streams driver for the Zilog Z85130 serial communication controller
 *
 * $Revision: 1.150 $
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
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/termio.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sema.h"
#include "ksys/vproc.h"
#include "sys/z8530.h"
#include "sys/ddi.h"
#include "sys/capability.h"
#if IPMHSIM || MHSIM
#include "sys/kopt.h"
#endif /* IPMHSIM */

#include <sys/idbgentry.h>
#include <sys/kmem.h>
#include <sys/hwgraph.h>

#ifdef DEBUG
static volatile du_debug = 0;		/* for link compatability */
#endif /* DEBUG */

#if defined(IP20)
#define ENETCOUNT	1
/* for the ethernet driver */
static int 	enet_carrier;
int 		enet_carrier_on(void);
int 		enet_collision;
void 		reset_enet_carrier(void);
void		heart_led_control(int);
#endif	/* IP20 */

#if IP22 || IP26 || IP28 || IP32 || IPMHSIM
#define	CONSOLE_D2	1		/* support alternate console=d2 */
uint du_console;			/* default console (d1) */
#else
#define du_console SCNDCPORT		/* only have console on d1 */
#endif


extern int posix_tty_default;
extern console_is_tport;
extern int kdebugbreak;
#define	CNTRL_A		'\001'
#ifdef DEBUG
  #ifndef DEBUG_CHAR
  #define DEBUG_CHAR	CNTRL_A
  #endif
#else /* !DEBUG */
  #define DEBUG_CHAR	CNTRL_A
#endif /* DEBUG */
int	zduart_rdebug = 0;

static int def_console_baud = B9600;	/* default speed for the console */
#if IP20 || IP22 || IP26 || IP28 || IP32 || IPMHSIM
static int def_pdbxport_baud = B9600;   /* default speed for the pdbxport */
#endif

extern struct stty_ld def_stty_ld;

/* default cflags */
#define DEF_CFLAG 		((tcflag_t)(CREAD | def_stty_ld.st_cflag))
#define DEF_GFXKEY_CFLAG 	(CS8 | PARODD | PARENB | CREAD | CLOCAL)
#define DEF_GFXKEY_OSPEED	(B600)


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
#define MAX_RMSG_LEN	2048		/* largest msg allowed */
#define XOFF_RMSG_LEN	256		/* send XOFF here */
#define MAX_RBUF_LEN	1024

#define MAX_RSRV_CNT	3		/* continue input timer this long */
extern int duart_rsrv_duration;		/* send input this often */

static void du_rsrv(queue_t *);
struct cred;
static int du_open(queue_t *, dev_t *, int, int, struct cred *);
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

#define DUARTPORTS	2	/* ports per DUART */

#define NUMMINOR	32		/* must be (2**n) */
#define PORT(dev)	port_map[(dev) & (NUMMINOR - 1)]

#if defined(IP20)
/*
 * DUART port 1= back panel keybd port  = minor #30
 *   "    "   0=   "    "   mouse port  =  "    #31
 *   "    "   2=   "    "   port 1      =  "    #1
 *   "    "   3=   "    "    "   2      =  "    #2
 *   "    "   4=   "    "    "   3      =  "    #3
 *   "    "   5=   "    "    "   4      =  "    #4
 */


#define	SCNDCPORT	2		/* port for the secondary console */
#define	MOUSEPORT	0		/* mouse */
#define	GFXKEYPORT	1		/* graphics keyboard */
#define PDBXPORT	3		/* port for dbx access to symmon */
#define	NUMDUARTS	2		/* IP20 - max number of DUARTs */

static u_char port_map[NUMMINOR] = {
	255,   2,   3,   4,    5, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255,   1,   0,
};
#elif IP22 || IP26 || IP28 || IP32 || IPMHSIM
/*
 * DUART port 1= back panel port 1      = minor #1
 *   "    "   2=   "    "   port 2      =  ''   #2
 */
/*
 * "Dual console" support - SNCDCPORT is primary (A) non-graphic console, while
 * PDBXPORT is secondary (B) non-graphic console.
 */
#define NUMDUARTS	1
#define SCNDCPORT	0		/* only 1 duart -- no kbd/mouse */
#define PDBXPORT	1		/* port for dbx access to symmon */
#if IP22 || IP26 || IP28
extern uint _hpc3_write2;		/* software copy of write2 reg */
#endif /* IP22 || IP26 || IP28 */

static u_char port_map[NUMMINOR] = {
	255,   0,   1, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
	255, 255, 255, 255,  255, 255, 255, 255,
};
#endif	/* IPXX */

#if IPMHSIM || MHSIM
#define BAUD(x) 	(x)
#else
#define	BAUD(x)		((CLK_SPEED + (x) * CLK_FACTOR) /		\
			 ((x) * CLK_FACTOR * 2) - 2)
#endif /* IPMHSIM */

#define BAUDBAD		0xffff


/*
 * each port has a data structure that looks like this
 */
typedef struct dport {
	/*
	 * Add a sleep lock (mutex_t) to protect access to the following:
	 *
	 *	dp_porthandler
	 * 	dp_state
	 *	dp_rq, dp_wq
	 *	dp_wr{1,15}
	 */
	/* hardware address */
	volatile u_char	*dp_cntrl;	/* port control	reg */
	volatile u_char	*dp_data;	/* port data reg */

	u_char	dp_index;		/* port number */

	/* z8530 return 1's in unused bits, so, we need a bit mask */
	u_char	dp_bitmask;

	/* software copy of hardware registers */
	u_char	dp_wr1;			/* individual interrupt mask */
	u_char	dp_wr3;			/* receiver parameters */
	u_char	dp_wr4;			/* transmitter/receiver parameters */
	u_char	dp_wr5;			/* transmitter parameters */
	u_char	dp_wr15;		/* external events interrupt mask */

	u_char	dp_rsrv_cnt;		/* input timer count */
	int	dp_rsrv_duration;	/* input timer */

	u_int	dp_state;		/* current state */

	struct termio dp_termio;
#define dp_iflag dp_termio.c_iflag	/* use some of the bits (see below) */
#define dp_cflag dp_termio.c_cflag	/* use all of the standard bits */
#define dp_line  dp_termio.c_line	/* line discipline */
#define dp_ospeed dp_termio.c_ospeed	/* output speed */
#define dp_ispeed dp_termio.c_ispeed	/* input speed */
#define dp_cc    dp_termio.c_cc		/* control characters */

	queue_t	*dp_rq, *dp_wq;		/* our queues */
	mblk_t	*dp_rmsg, *dp_rmsge;	/* current input message, head/tail */
	int	dp_rmsg_len;		/* current input message length */
	int	dp_rbsize;		/* recent input message length */
	mblk_t	*dp_rbp;		/* current input buffer */
	mblk_t	*dp_wbp;		/* current output buffer */

	int	(*dp_porthandler)(unsigned char);	/* textport hook */

	toid_t	dp_tid;			/* (recent) output delay timer ID */

	int	dp_fe, dp_over;		/* framing/overrun errors counts */
	int	dp_allocb_fail;		/* losses due to allocb() failures */
	lock_t	dp_lock;		/* sleep lock protecting port */
	sv_t	dp_sv;			/* sync var for use in du_open */
} dport_t;

#define	SIOTYPE_PORT	0
#define	SIOTYPE_MODEM	1
#define	SIOTYPE_FLOW	2
#define SIOTYPE_KBDMS	3

typedef struct siotype_s {
    int			type;
    dport_t	       *port;
} siotype_t;

/* bits in dp_state */
#define DP_ISOPEN	0x00000001	/* device is open */
#define DP_WOPEN	0x00000002	/* waiting for carrier */
#define DP_DCD		0x00000004	/* we have carrier */
#define DP_TIMEOUT	0x00000008	/* delaying */
#define DP_BREAK	0x00000010	/* breaking */
#define DP_BREAK_QUIET	0x00000020	/* finishing break */
#define DP_TXSTOP	0x00000040	/* output stopped by received XOFF */
#define DP_LIT		0x00000080	/* have seen literal character */
#define DP_BLOCK	0x00000100	/* XOFF sent because input full */
#define DP_TX_TXON	0x00000200	/* need to send XON */
#define DP_TX_TXOFF	0x00000400	/* need to send XOFF */
#define	DP_SE_PENDING	0x00000800	/* buffer alloc event pending */

#define	DP_EXTCLK	0x10000000	/* external clock mode */
#define	DP_RS422	0x20000000	/* RS422 compatibility mode */
#define	DP_MODEM	0x80000000	/* modem device */

#define	DP_BREAK_ON	0x00010000	/* receiving break */
#define	DP_CTS		0x00020000	/* CTS on */

#if IP20
#define	CTS_XOR_MASK	RR0_CTS
#define	DCD_XOR_MASK	RR0_DCD
#define	DTR_XOR_MASK	WR5_DTR
#define	RTS_XOR_MASK	WR5_RTS
#endif	/* IP20 */
#if IP22 || IP26 || IP28 || IP32 || IPMHSIM
#define	CTS_XOR_MASK	0x00
#define	DCD_XOR_MASK	0x00
#define	DTR_XOR_MASK	0x00
#define	RTS_XOR_MASK	0x00
#endif	/* IP22 || IP26 || IP28 || IP32 || IPMHSIM */


static dport_t dports[] = {
#if defined(IP20)
	{(u_char *)(DUART0B_CNTRL), (u_char *)(DUART0B_DATA), 0, 0xff},
	{(u_char *)(DUART0A_CNTRL), (u_char *)(DUART0A_DATA), 1, 0xff},
#endif
#if IP22 || IP26 || IP28 || (defined(IP32) && !defined(MHSIM))
	{(u_char *)(DUART1B_CNTRL), (u_char *)(DUART1B_DATA), 0, 0xff},
	{(u_char *)(DUART1A_CNTRL), (u_char *)(DUART1A_DATA), 1, 0xff},
#endif
#if defined(IPMHSIM) || (defined(IP32) && defined(MHSIM))
	{(u_char *)(0), (u_char *)(MHSIM_DUART1B_DATA), 0, 0xff},
	{0, 0, 0, 0xff},
#endif /* IPMHSIM */
#if defined(IP20)
	{(u_char *)(DUART1B_CNTRL), (u_char *)(DUART1B_DATA), 2, 0xff},
	{(u_char *)(DUART1A_CNTRL), (u_char *)(DUART1A_DATA), 3, 0xff},
	{(u_char *)(DUART2B_CNTRL), (u_char *)(DUART2B_DATA), 4, 0xff},
	{(u_char *)(DUART2A_CNTRL), (u_char *)(DUART2A_DATA), 5, 0xff},
#endif
};

#if IPMHSIM || MHSIM
struct scons_device {
	volatile unsigned long	sc_status;	/* status register */
	volatile unsigned long	sc_command;	/* command register */
	volatile unsigned long	sc_rx;		/* receiver data register */
	volatile unsigned long	sc_tx;		/* transmitter data register */
	volatile unsigned long	sc_txbuf;	/* transmitter data buffer */
};

#define	SC_STAT_RXRDY	1		/* receiver has data available */
#define	SC_CMD_RXIE	1		/* receiver interrupt enable */
#define	SC_CMD_TXFLUSH	2		/* flush any buffered output */

#define SCONS0_BASE 0x1b000000
#define SCONS1_BASE 0x1b001000

static int is_sableio = 0;

#define DCL_SCONS_P(scp,duartnum) \
	volatile struct scons_device *scp = \
		(volatile struct scons_device *) \
			PHYS_TO_K1(((duartnum == 0) \
				    ? SCONS0_BASE : SCONS1_BASE))
#endif /* IPMHSIM */


#ifdef IP22
static int ip24;		/* machine is an IP24 */
#endif

static int maxport;		/* maximum number of ports */
static fifo_size = 4;		/* on chip transmit FIFO size */


static void mips_du_break(dport_t *, int);
static void mips_du_config(dport_t *, tcflag_t, speed_t, struct termio *, int);
static int mips_du_getchar(dport_t *, int);
#define	SEND_XON	1
#define	SEND_XOFF	0
#define	IGNORE_IXOFF	1
static void mips_du_iflow(dport_t *, int, int, int);
static void mips_du_putchar(dport_t *, u_char);
static int mips_du_allsent(dport_t *);
static int mips_du_start_rx(dport_t *);
static void mips_du_start_tx(dport_t *, int);
static void mips_du_stop_rx(dport_t *, int);
static void mips_du_stop_tx(dport_t *);
static int mips_du_txrdy(dport_t *);

static void du_coff(dport_t *, int);
static void du_con(dport_t *);
static int du_extclk(dport_t *, mblk_t *);
static void du_flushw(dport_t *);
static void du_flushr(struct dport *);
static mblk_t *du_getbp(dport_t *, u_int, int);
static int du_rs422(dport_t *, mblk_t *);
static void du_rx(dport_t *);
static void du_tx(dport_t *, int, int);

#ifdef DEBUG
static int du_loopback(dport_t *, mblk_t *);
#endif

static void du_initport(dport_t *);
static int du_lock(dport_t *);
static void du_unlock(dport_t *, int);
static lcl_intr_func_t du_poll;

#ifdef MOUSEPORT
/*
 * return the DUART port number of the system mouse port
 */
int
du_mouse_port()
{
	return(MOUSEPORT);
}
#endif



#ifdef GFXKEYPORT
/*
 * return the DUART port number of the system keyboard port
 */
int
du_keyboard_port()
{
	return(GFXKEYPORT);
}
#endif



/* 
 * DO NOT REPLACE THE NEXT 2 ROUTINES WITH MACROS SINCE THE CODE MAY GET
 * PREEMPTED BETWEEN splx() AND THE RETURN, THUS CAUSE WRONG DATA TO GET
 * RETURNED
 */

/* quick way to read uart register 0 and data register. */
static u_char
rd_uart_reg0_data(volatile u_char *p)
{
	u_char	data;
	int	s = spl7();

	Z8530_DELAY;
	data = *p;
	splx(s);

	return data;
}

/* read uart registers using indirect addressing. */
static u_char
rd_uart_reg(volatile u_char *p, int r)
{
	u_char	data;
	int	s = spl7();

	Z8530_DELAY;
	*p = r;
	Z8530_FBDELAY;
	data = *p;
	splx(s);

	return data;
}

/*
 * gobble any waiting input, should only be called when safe from interrupts
 */
static void
du_rclr(dport_t *dp)
{
	int c;
	int is_scndcport = (dp->dp_index == du_console);

	while ((c = mips_du_getchar(dp, 0)) != -1) {
		if (kdebug && !console_is_tport && is_scndcport && (c & 0xff) == DEBUG_CHAR)
			debug("ring");
	}
}


/*
 * shut down a port, should only be called when safe from interrupts
 */
/*
 * LOCKS HELD ON ENTRY: None.
 */ 
static void
du_zap(dport_t *dp, int hup)
{
	int s;

	du_flushw(dp);			/* forget pending output */

	mips_du_stop_rx(dp, hup);

	LOCK_PORT(dp, s);
	mips_du_stop_tx(dp);
	UNLOCK_PORT(dp, s);
	du_rclr(dp);
}



/*
 * finish a delay
 */
static void
du_delay(dport_t *dp)
{
	int s;

	LOCK_PORT(dp, s);

	if ((dp->dp_state & (DP_BREAK | DP_BREAK_QUIET)) != DP_BREAK) {
		dp->dp_state &= ~(DP_TIMEOUT | DP_BREAK | DP_BREAK_QUIET);
		UNLOCK_PORT(dp, s);
		mips_du_start_tx(dp, 1);/* resume output */

	} else {			/* unless need to quiet break */
		mips_du_break(dp, 0);
		dp->dp_state |= DP_BREAK_QUIET;
		UNLOCK_PORT(dp, s);
		dp->dp_tid = STREAMS_TIMEOUT(du_delay, (caddr_t)dp, HZ/20);
	}
}



/*
 * heartbeat to send input up stream
 * this is used to reduced the large cost of trying to send each input byte
 * upstream by itself.
 */
static void
du_rsrv_timer(dport_t *dp)
{
	int s;

	LOCK_PORT(dp, s);

	if (--dp->dp_rsrv_cnt)
		(void)STREAMS_TIMEOUT(du_rsrv_timer, (caddr_t)dp, dp->dp_rsrv_duration);

	if (dp->dp_state & DP_ISOPEN && canenable(dp->dp_rq))
		qenable(dp->dp_rq);

	UNLOCK_PORT(dp, s);
}



/*
 * flush output, should only be called when safe from interrupts
 */
/*
 * LOCKS HELD ON ENTRY: None.
 */
static void
du_flushw(dport_t *dp)
{
	int s;

	LOCK_PORT(dp, s);
	if ((dp->dp_state & (DP_TIMEOUT | DP_BREAK | DP_BREAK_QUIET))
	    == DP_TIMEOUT) {
		dp->dp_state &= ~DP_TIMEOUT;
		untimeout(dp->dp_tid);		/* forget stray timeout */
	}

	freemsg(dp->dp_wbp);
	dp->dp_wbp = NULL;
	UNLOCK_PORT(dp, s);
}

/*
 * flush input, should only be called when safe from interrupts
 *
 * called from du_i_ioctl with streams locked
 * called from du_close with streams locked.
 * called from du_wput with streams locked.
 * called from du_rx with streams locked.
 */
static void
du_flushr(dport_t *dp)
{
	freemsg(dp->dp_rmsg);
	dp->dp_rmsg = NULL;
	dp->dp_rmsg_len = 0;
	freemsg(dp->dp_rbp);
	dp->dp_rbp = NULL;

	if (dp->dp_rq) {
		/* turn input back on */
		ASSERT(STREAM_LOCKED(dp->dp_rq));
		qenable(dp->dp_rq);
	}
}

/*
 * save a message on our write queue and start the output interrupt, if
 * necessary
 */
static void
du_save(dport_t *dp, queue_t *wq, mblk_t *bp)
{
	int s;

	putq(wq, bp);			/* save the message */

	LOCK_PORT(dp, s);
	if (!(dp->dp_wr1 & WR1_TX_INT_ENBL)) {
		UNLOCK_PORT(dp, s);
		mips_du_start_tx(dp, 1);/* start TX only if we must */
	} else
		UNLOCK_PORT(dp, s);
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
 *
 * called from du_i_ioctl with streams locked.
 * called from du_wput with streams locked.
 */
static void
du_tcset(dport_t *dp, mblk_t *bp)
{
	tcflag_t cflag;
	speed_t ospeed;
	struct iocblk *iocp;
	struct termio *tp;
	extern int baud_to_cbaud(speed_t);
	int s;
	iocp = (struct iocblk *)bp->b_rptr;
	tp = STERMIO(bp);

	if (tp->c_ospeed == __INVALID_BAUD) {
	    tp->c_ospeed = dp->dp_ospeed;	       /* don't change */
	}
	tp->c_ispeed = dp->dp_ospeed;
	cflag = tp->c_cflag;
	ospeed = tp->c_ospeed;
	/*
	 * we only support up to 38400, so return error
	 * if we can't support the requested baud rate
	 */
	if (baud_to_cbaud(ospeed) == __OLD_INVALID_BAUD) {
	    tp->c_ospeed = dp->dp_ospeed;
	    iocp->ioc_count = 0;
	    iocp->ioc_error = EINVAL;
	    bp->b_datap->db_type = M_IOCACK;
	    return;
	}

	LOCK_PORT(dp, s);
        if (dp->dp_state & DP_MODEM) {
		/*
		 * On modem ports, only super-user can change CLOCAL.
		 * Otherwise, fail to do so silently for historic reasons.
		 */
		if (((dp->dp_cflag & CLOCAL) ^ (cflag & CLOCAL)) &&
		    !_CAP_CRABLE(iocp->ioc_cr, CAP_DEVICE_MGT)) {
			cflag &= ~CLOCAL;
			cflag |= dp->dp_cflag & CLOCAL;
		}
	}
	UNLOCK_PORT(dp, s);

	mips_du_config(dp, cflag, ospeed, tp, 1);

	tp->c_cflag = dp->dp_cflag; /* tell line discipline the results */
	tp->c_ospeed = dp->dp_ospeed;
	tp->c_ispeed = dp->dp_ispeed;

	iocp->ioc_count = 0;
	bp->b_datap->db_type = M_IOCACK;
}



/*
 * interrupt-process an IOCTL. this function processes those IOCTLs that
 * must be done by the output interrupt
 *
 * called from du_tx with streams locked.
 */
static void
du_i_ioctl(dport_t *dp, mblk_t *bp)
{
	struct iocblk *iocp;
	int s;
	iocp = (struct iocblk *)bp->b_rptr;

	switch (iocp->ioc_cmd) {
	case TCSBRK:
		if (!*(int *)bp->b_cont->b_rptr) {
			LOCK_PORT(dp, s);
			dp->dp_state |= DP_TIMEOUT | DP_BREAK;
			mips_du_break(dp, 1);
			UNLOCK_PORT(dp, s);
			dp->dp_tid = STREAMS_TIMEOUT(du_delay, (caddr_t)dp, HZ/4);
		} else {	/* tcdrain */

			/*
	 		 * Wait until the transmitter is empty.
			 * XXXrs should unmask interrupts while spinning,
			 * but we're too deep here.
			 */
			LOCK_PORT(dp, s);
			while (posix_tty_default && !mips_du_allsent(dp)) {

				UNLOCK_PORT(dp, s);
				LOCK_PORT(dp, s);
			}
			UNLOCK_PORT(dp, s);
		}
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		break;

	case TCSETAF:
		du_tcset(dp, bp);
		du_flushr(dp);
		streams_interrupt((strintrfunc_t)putctl1,
				    (void *)dp->dp_rq->q_next,
				    (void *)(__psint_t)M_FLUSH,
				    (void *)(__psint_t)FLUSHR);
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

	case SIOC_EXTCLK:
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		iocp->ioc_error = du_extclk(dp, bp);
		break;

	case SIOC_RS422:
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		iocp->ioc_error = du_rs422(dp, bp);
		break;

#ifdef DEBUG
	case SIOC_LOOPBACK:
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		iocp->ioc_error = du_loopback(dp, bp);
		break;
#endif

	default:
		ASSERT(0);
	}

	streams_interrupt((strintrfunc_t)putnext, dp->dp_rq, bp, NULL);
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
#ifdef CONSOLE_D2
	char *console = kopt_find("console");
#endif

	if (duinit_flag++)	/* do it at most once */
		return;

#if IPMHSIM || MHSIM
	is_sableio = is_enabled(arg_sableio);
#endif /* IPMHSIM */
#ifdef IP22
	/* Some bits on IP24 duarts need to be inverted or otherwise
	 * handled differently.  (IP24 code is compiled with IP22 defined;
	 * they share a binary.)
	 */
	ip24 = !is_fullhouse ();
#endif

#ifdef CONSOLE_D2
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
#endif

#if defined(PDBXPORT)
        /* check for pdbxport */
	if (kdebug &&
	    ((env = kopt_find("rdebug")) != NULL &&
	       atoi(env) != 0))
		zduart_rdebug = 1;

	if (zduart_rdebug) {
		env = kopt_find("rbaud");
		if (env) {
		    def_pdbxport_baud = atoi(env);
		}
	}
#endif

	/* figure out number of ports to work with */
	maxport = NUMDUARTS * DUARTPORTS - 1;

	/* disable RS422 compatibility mode */
#if defined(IP20)
	*(u_char *)PHYS_TO_K1(PORT_CONFIG) |= PCON_SER0RTS | PCON_SER1RTS;
#elif defined(IP22) || defined(IP26) || defined(IP28)
	_hpc3_write2 |= UART0_ARC_MODE | UART1_ARC_MODE;
	*(volatile int *)PHYS_TO_COMPATK1(HPC3_WRITE2) = _hpc3_write2;
#endif

	/*
	 * this should be the only place we reset the chips. enable global
	 * interrupt too
	 */
#if !IP32
	setlclvector(VECTOR_DUART,du_poll,0);
#else
	setcrimevector(MACE_INTR(4),3,du_poll,0,0);
#endif
	for (dp = dports; dp < &dports[maxport]; dp += 2) {
		INITLOCK_PORT(dp);
		INITLOCK_PORT(dp+1);
#ifdef PDBXPORT
		/* if debug mode, skip debug duart */
		if (zduart_rdebug &&
		    ((dp - dports) & ~1) == (PDBXPORT & ~1)) {
			WR_CNTRL(dp->dp_cntrl, WR9, WR9_MIE);
			continue;
		}
#endif
		WR_CNTRL(dp->dp_cntrl, WR9, WR9_HW_RST | WR9_MIE)
	}

	/*
	 * deassert all output signals, enable Z85130 functions, transmitter
	 * interrupt when FIFO is completely empty
	 */
	for (dp = dports; dp <= &dports[maxport]; dp++) {
		dp->dp_wr15 = WR15_Z85130_EXTENDED;
		WR_CNTRL(dp->dp_cntrl, WR15, dp->dp_wr15)

		WR_CNTRL(dp->dp_cntrl, WR7, WR7_TX_FIFO_INT | WR7_EXTENDED_READ);
		/*
		 * cover up for a mistake made by the manufacturing people,
		 * they mixed the Z85130 with some Z8530
		 */
		if (RD_CNTRL(dp->dp_cntrl, RR14) 
		    != (WR7_TX_FIFO_INT | WR7_EXTENDED_READ))
			fifo_size = 1;

#ifdef PDBXPORT
		/* if debug mode, leave debug duart running */
		if (zduart_rdebug &&
		    ((dp - dports) == SCNDCPORT ||
		     (dp - dports) == PDBXPORT)) {
			dp->dp_wr5 = WR5_TX_8BIT | WR5_TX_ENBL;
			DU_DTR_ASSERT(dp->dp_wr5);
			DU_RTS_ASSERT(dp->dp_wr5);
			WR_CNTRL(dp->dp_cntrl,WR5,dp->dp_wr5);
			continue;
		}
#endif
#if IP20
		if (dp->dp_index == GFXKEYPORT) {
			dp->dp_wr5 = 0;
			DU_DTR_DEASSERT(dp->dp_wr5); /* leave green LED on */
		} else
#endif
		{
			DU_DTR_DEASSERT(dp->dp_wr5);
			DU_RTS_DEASSERT(dp->dp_wr5);
		}
		WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5);
	}

	env = kopt_find("dbaud");
	if (env && (d_baud = atoi(env)) > 0)
		def_console_baud = d_baud;

        /* initialize the secondary console */
	dp = &dports[du_console];
	mips_du_config(dp, DEF_CFLAG, def_console_baud,
	    &def_stty_ld.st_termio, 0);
	du_zap(dp, HUPCL);

#ifdef DEBUG
	if(env && d_baud <= 0)
		cmn_err(CE_WARN, "non-numeric or 0 value for dbaud ignored (%s)", env);
#endif


#ifdef GFXKEYPORT
	/* initialize the graphics keyboard */
	dp = &dports[GFXKEYPORT];
	dp->dp_iflag = IGNBRK;
	mips_du_config(dp, DEF_GFXKEY_CFLAG, DEF_GFXKEY_OSPEED, (struct termio *)0, 0);
	du_zap(dp, HUPCL);
#endif

#ifdef ENETCOUNT
	/* enable etherent collision interrupt */
	dp = &dports[MOUSEPORT];
	dp->dp_wr15 |= WR15_CTS_IE;
	WR_CNTRL(dp->dp_cntrl, WR15, dp->dp_wr15)
	dp->dp_wr1 |= WR1_EXT_INT_ENBL;
	WR_CNTRL(dp->dp_cntrl, WR1, dp->dp_wr1)
#endif

#ifdef DEBUG
	idbg_addfunc("duart", dump_dp);
#endif
#if IPMHSIM || MHSIM
	if (is_sableio)
		fifo_size = 0x7fffffff;
#endif /* IPMHSIM */
}

/* export console device numbers */
static dev_t cons_devs[2];
dev_t
get_cons_dev(int which)
{
    ASSERT(which == 1 || which == 2);

    return(cons_devs[which - 1]);
}

/* late init stuff that must be done after the hwgraph is inited 
 * (du_init is called before hwgraph init time)
 */
void
du_lateinit()
{
    char name[12];
    vertex_hdl_t ttydir_vhdl, inputdir_vhdl;
    graph_error_t rc;
    int x, total;
    siotype_t *siotype;
#if DEBUG
    siotype_t *last_siotype;
#endif

    /* create containing directory /hw/ttys and /hw/input */
    rc = hwgraph_path_add(hwgraph_root, "ttys", &ttydir_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    rc = hwgraph_path_add(hwgraph_root, "input", &inputdir_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    total = 0;

    /* ordinary ttys have 3 types, d m and f */
    for(x = 1; x <= 16 ; x++)
	if (port_map[x] != 255 && dports[port_map[x]].dp_cntrl)
	    total += 3;

    /* keyboard/mouse have 1 type */
    if (port_map[31] != 255 && dports[port_map[31]].dp_cntrl)
	total++;
    if (port_map[30] != 255 && dports[port_map[30]].dp_cntrl)
	total++;

    siotype = (siotype_t*)
	kmem_alloc(sizeof(siotype_t) * total, KM_NOSLEEP);
#if DEBUG
    last_siotype = siotype + total;
#endif

    /* look for ordinary serial ports and create the nodes.
     */
    for(x = 1; x <= 16; x++) {
	static char types[] = {SIOTYPE_PORT, SIOTYPE_MODEM, SIOTYPE_FLOW};
	vertex_hdl_t dev_vhdl;
	int type;

	if (port_map[x] == 255 ||
	    dports[port_map[x]].dp_cntrl == 0) {
	    ASSERT(x != 1 && x != 2); /* console must exist! */
	    continue;
	}

	/* create the device nodes pointing to this port */
	for(type = 0; type < 3; type++) {
	    siotype->type = types[type];
	    siotype->port = &dports[port_map[x]];
	    sprintf(name, "tty%c%d", "dmf"[type], x);
	    rc = hwgraph_char_device_add(ttydir_vhdl, name, "du", &dev_vhdl);
	    ASSERT(rc == GRAPH_SUCCESS);
	    hwgraph_chmod(dev_vhdl, HWGRAPH_PERM_TTY);
	    hwgraph_fastinfo_set(dev_vhdl, (arbitrary_info_t)siotype);

	    /* save console devs and create dials and tablet synonyms */
	    if ((x == 1 || x == 2) && type == 0) {
		static char *names[] = {"tablet", "dials"};
		static int perms[] = {HWGRAPH_PERM_TABLET, HWGRAPH_PERM_DIALS};
		
		cons_devs[x-1] = vhdl_to_dev(dev_vhdl);
		
		/* must be separate node, not just an extra edge to the same node */
		rc = hwgraph_char_device_add(inputdir_vhdl, names[x-1], "du", &dev_vhdl);
		ASSERT(rc == GRAPH_SUCCESS);
		hwgraph_chmod(dev_vhdl, perms[x-1]);
		hwgraph_fastinfo_set(dev_vhdl, (arbitrary_info_t)siotype);
	    }
	    siotype++;
	}
    }
    
    /* look for kbd/mouse ports and create the nodes.
     */
    {
	vertex_hdl_t dev_vhdl;

	/* check for a mouse */
	if (port_map[31] != 255 &&
	    dports[port_map[31]].dp_cntrl) {

	    siotype->type = SIOTYPE_KBDMS;
	    siotype->port = &dports[port_map[31]];

	    /* create a node in /hw/inputX */
	    rc = hwgraph_char_device_add(hwgraph_root, "input/mouse", "du", &dev_vhdl);
	    ASSERT(rc == GRAPH_SUCCESS);
	    hwgraph_chmod(dev_vhdl, HWGRAPH_PERM_MOUSE);
	    hwgraph_fastinfo_set(dev_vhdl, (arbitrary_info_t)siotype);
	    
	    siotype++;
	}

	/* check for a keyboard */
	if (port_map[30] != 255 &&
	    dports[port_map[30]].dp_cntrl) {

	    siotype->type = SIOTYPE_KBDMS;
	    siotype->port = &dports[port_map[30]];

	    /* create a node in /hw/inputX */
	    rc = hwgraph_char_device_add(hwgraph_root, "input/keyboard", "du", &dev_vhdl);
	    hwgraph_chmod(dev_vhdl, HWGRAPH_PERM_KBD);
	    ASSERT(rc == GRAPH_SUCCESS);
	    hwgraph_fastinfo_set(dev_vhdl, (arbitrary_info_t)siotype);

	    siotype++;
	}
    }
    ASSERT(siotype == last_siotype);
}

/*
 * open a stream DUART port
 */
static int
du_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	dport_t *dp;
	u_short port;
	int s, modem, flow_modem;
	int error;
	siotype_t *siotype;
	int done;

	if (sflag)			/* only a simple stream driver */
		return(ENXIO);

	if (!dev_is_vertex(*devp)) {
	    if ((*devp & (NUMMINOR - 1)) == 1) {
		cmn_err(CE_WARN, "opening console with old style "
			"device node. Run MAKEDEV.\n");
		port = PORT(*devp);
		dp = &dports[port];
		modem = flow_modem = 0;
	    }
	    else
		return(ENODEV);
	}
	else {
	    siotype = (siotype_t*)device_info_get(*devp);
	    dp = siotype->port;
	    modem = (siotype->type == SIOTYPE_MODEM || siotype->type == SIOTYPE_FLOW);
	    flow_modem = (siotype->type == SIOTYPE_FLOW);
	    port = dp->dp_index;
	}

#ifdef PDBXPORT
	if (port == PDBXPORT &&
	    zduart_rdebug)
		return(ENXIO);
#endif

	LOCK_PORT(dp, s);
	if (!(dp->dp_state & (DP_ISOPEN | DP_WOPEN))) {	/* on the 1st open */
		tcflag_t cflag;
		speed_t ospeed;
		queue_t *wq = WR(rq);

		cflag = def_stty_ld.st_cflag;
		ospeed = def_stty_ld.st_ospeed;
		if (dp->dp_index == du_console)
			ospeed = def_console_baud;
#ifdef PDBXPORT
		else if (zduart_rdebug &&
			 dp->dp_index == PDBXPORT)
			ospeed = def_pdbxport_baud;
#endif
#ifdef GFXKEYPORT
		else if (dp->dp_index == GFXKEYPORT) {
			cflag = DEF_GFXKEY_CFLAG;
			ospeed = DEF_GFXKEY_OSPEED;
		}
#endif

		dp->dp_rsrv_duration = duart_rsrv_duration;
		dp->dp_state &= ~(DP_TXSTOP | DP_LIT | DP_BLOCK | DP_TX_TXON |
			DP_TX_TXOFF | DP_RS422 | DP_MODEM | DP_EXTCLK);

		if (modem) {
			dp->dp_state |= DP_MODEM;
			cflag &= ~CLOCAL;
			if (flow_modem)
				cflag |= CNEW_RTSCTS;
		}

#if IP20
		if (port == maxport)
			*(volatile u_char *)PHYS_TO_K1(PORT_CONFIG) |=
				PCON_SER0RTS;
		else if (port == maxport - 1)
			*(volatile u_char *)PHYS_TO_K1(PORT_CONFIG) |=
				PCON_SER1RTS;
#elif IP22 || IP26 || IP28
		if (port == maxport)
			_hpc3_write2 |= UART0_ARC_MODE;
		else if (port == maxport - 1)
			_hpc3_write2 |= UART1_ARC_MODE;
		*(volatile uint *)PHYS_TO_COMPATK1(HPC3_WRITE2) = _hpc3_write2;
#endif

		dp->dp_state |= DP_WOPEN;
		UNLOCK_PORT(dp, s);
		mips_du_config(dp, cflag, ospeed, &def_stty_ld.st_termio, 1);
		if (!mips_du_start_rx(dp)		/* wait for carrier */
		    && !(dp->dp_cflag & CLOCAL)
		    && !(flag & (FNONBLK | FNDELAY))) {
			do {
				LOCK_PORT(dp, s);
				if (sv_wait_sig(&dp->dp_sv, PZERO+1,
						&dp->dp_lock, s)) {
					LOCK_PORT(dp, s);
					dp->dp_state &= ~(DP_WOPEN|DP_ISOPEN);
					UNLOCK_PORT(dp, s);
					du_zap(dp, HUPCL);
					return (EINTR);
				}
				LOCK_PORT(dp, s);
				done = (dp->dp_state & DP_DCD);
				UNLOCK_PORT(dp, s);
			} while (!done);
		}

		rq->q_ptr = (caddr_t)dp;	/* connect device to stream */
		wq->q_ptr = (caddr_t)dp;
		LOCK_PORT(dp, s);
		dp->dp_wq = wq;
		dp->dp_rq = rq;
		dp->dp_state |= DP_ISOPEN;
		dp->dp_state &= ~DP_WOPEN;
		dp->dp_porthandler = NULL;	/* owner gains control */
		UNLOCK_PORT(dp, s);
		dp->dp_cflag |= CREAD;

		du_rclr(dp);		/* discard input */

		if (error = strdrv_push(rq, "stty_ld", devp, crp)) {
			LOCK_PORT(dp, s);
			dp->dp_state &= ~(DP_ISOPEN | DP_WOPEN);
			dp->dp_rq = NULL;
			dp->dp_wq = NULL;
			UNLOCK_PORT(dp, s);
			du_zap(dp, HUPCL);
			return(error);
		}
	} else {
		UNLOCK_PORT(dp, s);
		/*
 		 * you cannot open two streams to the same device. the dp
		 * structure can only point to one of them. therefore, you
		 * cannot open two different minor devices that are synonyms
		 * for the same device. that is, you cannot open both ttym1
		 * and ttyd1
 		 */
		if (dp->dp_rq != rq) {
			/* fail if already open */
			return(EBUSY);
		}
	}

	return(0);		/* return successfully */
}



/*
 * close a port
 */
/*ARGSUSED3*/
static int
du_close(queue_t *rq, int flag, struct cred *crp)
{
	dport_t *dp = (dport_t *)rq->q_ptr;
	int s;

	LOCK_PORT(dp, s);
	if (dp->dp_state & DP_SE_PENDING) {
		dp->dp_state &= ~DP_SE_PENDING;
		UNLOCK_PORT(dp, s);
		str_unbcall(rq);
	} else {
		UNLOCK_PORT(dp, s);
	}

	du_flushr(dp);
	du_flushw(dp);
	LOCK_PORT(dp, s);
	dp->dp_state &= ~(DP_ISOPEN | DP_WOPEN);
	dp->dp_rq = NULL;
	dp->dp_wq = NULL;
	UNLOCK_PORT(dp, s);
	du_zap(dp, dp->dp_cflag & HUPCL);

	return 0;
}



/*
 * send a bunch of 1 or more characters up the stream
 */
static void
du_rsrv(queue_t *rq)
{
	mblk_t *bp;
	dport_t *dp = (dport_t *)rq->q_ptr;
	int s;

	if (!canput(rq->q_next)) {	/* quit if upstream congested */
		noenable(rq);
		return;
	}
	enableok(rq);
	LOCK_PORT(dp, s);
	if (dp->dp_state & DP_SE_PENDING) {
		dp->dp_state &= ~DP_SE_PENDING;
		UNLOCK_PORT(dp, s);
		str_unbcall(rq);
		LOCK_PORT(dp, s);
	}

	/* 
	 * when we do not have an old buffer to send up, or when we are
	 * timing things, send the current buffer
	 */
	bp = dp->dp_rbp;
	if (bp && bp->b_wptr > bp->b_rptr
	    && (!dp->dp_rmsg || !dp->dp_rsrv_cnt)) {
		str_conmsg(&dp->dp_rmsg, &dp->dp_rmsge, bp);
		dp->dp_rmsg_len += (bp->b_wptr - bp->b_rptr);
		dp->dp_rbp = NULL;
	}

	bp = dp->dp_rmsg;
	if (bp) {
		if (dp->dp_rmsg_len > dp->dp_rbsize)
			dp->dp_rbsize = dp->dp_rmsg_len;
		else
			dp->dp_rbsize = (dp->dp_rmsg_len + dp->dp_rbsize) / 2;
		dp->dp_rmsg_len = 0;
		dp->dp_rmsg = NULL;
		UNLOCK_PORT(dp, s);
		putnext(rq, bp);	/* send the message */
	} else
		UNLOCK_PORT(dp, s);

	mips_du_iflow(dp, SEND_XON, !IGNORE_IXOFF, 1);

	/* get a buffer now, rather than waiting for an interrupt */
	LOCK_PORT(dp, s);
	if (!dp->dp_rbp)
		(void)du_getbp(dp, BPRI_LO, 1);
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
	int s;
	int i;
#if IP22 || IP26 || IP28
	volatile uint *hpc3_read = (volatile uint *)PHYS_TO_COMPATK1(HPC3_READ);
#endif

	switch (bp->b_datap->db_type) {

	case M_FLUSH:	/* XXX may not want to restart output since flow
			   control may be messed up */
		c = *bp->b_rptr;
		sdrv_flush(wq, bp);
		if (c & FLUSHW) {
			du_flushw(dp);
			LOCK_PORT(dp, s);
			dp->dp_state &= ~DP_TXSTOP;
			UNLOCK_PORT(dp, s);
			mips_du_start_tx(dp, 1);/* restart output */
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
			case 0:			/* stop output */
				LOCK_PORT(dp, s);
				dp->dp_state |= DP_TXSTOP;
				mips_du_stop_tx(dp);
				UNLOCK_PORT(dp, s);
				break;
			case 1:			/* resume output */
				LOCK_PORT(dp, s);
				dp->dp_state &= ~DP_TXSTOP;
				UNLOCK_PORT(dp, s);
				mips_du_start_tx(dp, 1);
				break;
			case 2:
				mips_du_iflow(dp, SEND_XOFF, IGNORE_IXOFF, 1);
				break;
			case 3:
				mips_du_iflow(dp, SEND_XON, IGNORE_IXOFF, 1);
				break;
			default:
				iocp->ioc_error = EINVAL;
				break;
			}
			iocp->ioc_count = 0;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq, bp);
			break;

		case TIOCMGET:
			/* DTR is true when open and CBAUD != 0 */
			i = (dp->dp_ospeed) ? TIOCM_DTR : 0;
			LOCK_PORT(dp, s);
			if ((dp->dp_wr5 & WR5_RTS) ^ RTS_XOR_MASK)
				i |= TIOCM_RTS;
			c = RD_RR0(dp->dp_cntrl);
			UNLOCK_PORT(dp, s);
			if ((c & RR0_DCD) ^ DCD_XOR_MASK)
				i |= TIOCM_CD;
			if ((c & RR0_CTS) ^ CTS_XOR_MASK)
				i |= TIOCM_CTS;
#if IP22 || IP26 || IP28
			if (dp->dp_index == maxport) {
				if (*hpc3_read & UART0_DSR)
					i |= TIOCM_DSR;
				if (*hpc3_read & UART0_RI)
					i |= TIOCM_RI;
			} else {
				if (*hpc3_read & UART1_DSR)
					i |= TIOCM_DSR;
				if (*hpc3_read & UART1_RI)
					i |= TIOCM_RI;
			}
#endif	/* IP22 || IP26 || IP28 */
			*(int*)(bp->b_cont->b_rptr) = i;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			break;

		case TCGETA: /* XXXrs - Don't wait until output? */
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			tcgeta(dp->dp_wq,bp, &dp->dp_termio);
			break;

		case TCSETA: /* XXXrs - Don't wait until output? */
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			(void)du_tcset(dp, bp);
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
#ifdef DEBUG
		case SIOC_LOOPBACK:	/* select/de-select loop test mode */
#endif
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

	return 0;
}

/*
 * Handle an interrupt from the specified duart.
 * Returns:
 *	0 if nothing to do on the specified duart
 *	1 if something was done
 *
 * This routine allows for code sharing between Everest, IP20
 * platforms.  The latter uses a single interrupt source
 * for multiple duarts, and therefore needs to poll each duart
 * to find which one actually needs servicing.  The latter
 * platform uses separate interrupts for each duart.
 */

static int
du_handle_intr(int duartnum)
{
	dport_t *dp0 = &dports[duartnum];	/* port B on DUART */
	dport_t *dp1 = &dports[duartnum+1];	/* port A on DUART */
#if IPMHSIM || MHSIM
	int i;
#endif /* IPHSIM || MHSIM */
	u_char rr0;
	u_char rr3;
	int s;

	LOCK_PORT(dp1, s);
#if IPMHSIM || MHSIM
	if (is_sableio) {
		DCL_SCONS_P(scp,duartnum);

		rr3 = ((duartnum == 0 &&
			(scp->sc_status & SC_STAT_RXRDY))
		       ? RR3_CHNB_RX_IP : 0);
	} else {
		if (duartnum == 0) {
			MHSIM_DUART_READ(MHSIM_DUART_INTR_STATE_ADDR, i);
		} else {
			i = 0;
		}
		rr3 = 0;
		if (i & MHSIM_DUART_RX_INTR_STATE) rr3 |= RR3_CHNB_RX_IP;
		if (i & MHSIM_DUART_TX_INTR_STATE) rr3 |= RR3_CHNB_TX_IP;
	}
	if (!rr3) {
#else /* IPMHSIM */
	if (!(rr3 = RD_CNTRL(dp1->dp_cntrl, RR3))) {
#endif /* IPMHSIM */
		UNLOCK_PORT(dp1, s);
		return(0);
	}

	UNLOCK_PORT(dp1, s);

	/* receive character/error interrupt */
	if (rr3 & RR3_CHNB_RX_IP) {
		SYSINFO.rcvint++;
		du_rx(dp0);
	}

	if (rr3 & RR3_CHNA_RX_IP) {
		SYSINFO.rcvint++;
		du_rx(dp1);
	}

	/* external status interrupt */
	if (rr3 & RR3_CHNB_EXT_IP) {
		LOCK_PORT(dp0, s);
		rr0 = RD_RR0(dp0->dp_cntrl);
#ifdef IP22
		if (ip24 && (dp0->dp_state & DP_RS422)) {
			DU_CTS_INVERT(rr0);
		}
#endif

		/* handle CTS interrupt only if we care */
		if (dp0->dp_wr15 & WR15_CTS_IE) {
			if (dp0->dp_state & DP_CTS
			    && !(DU_CTS_ASSERTED(rr0))) {
				SYSINFO.mdmint++;
				dp0->dp_state &= ~DP_CTS;
			} else if (!(dp0->dp_state & DP_CTS)
				   && DU_CTS_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp0->dp_state |= DP_CTS;
#ifdef ENETCOUNT
				if (dp0->dp_index != MOUSEPORT) {
#endif
					UNLOCK_PORT(dp0, s);
					mips_du_start_tx(dp0, 0);
					LOCK_PORT(dp0, s);
#ifdef ENETCOUNT
				} else {
					enet_collision++;
					enet_carrier = 1;
					dp0->dp_wr15 &= 
					    ~WR15_DCD_IE;
					WR_CNTRL(dp0->dp_cntrl,
					    WR15, dp0->dp_wr15)
				}
#endif
			}
		}

		/* handle DCD interrupt only if we care */
		if (dp0->dp_wr15 & WR15_DCD_IE) {
			if (dp0->dp_state & DP_DCD
			    && !DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp0->dp_state &= ~DP_DCD;
				UNLOCK_PORT(dp0, s);
				du_coff(dp0, 0);
				LOCK_PORT(dp0, s);
			} else if (!(dp0->dp_state & DP_DCD)
				   && DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp0->dp_state |= DP_DCD;
#ifdef ENETCOUNT
				if (dp0->dp_index != MOUSEPORT) {
#endif
					UNLOCK_PORT(dp0, s);
					du_con(dp0);
					LOCK_PORT(dp0, s);
#ifdef ENETCOUNT
				} else {
					enet_carrier = 1;
					dp0->dp_wr15 &= 
					    ~WR15_DCD_IE;
					WR_CNTRL(dp0->dp_cntrl,
					    WR15, dp0->dp_wr15)
				}
#endif
			}
		}

		/* handle BREAK interrupt */
		if (rr0 & RR0_BRK)
			dp0->dp_state |= DP_BREAK_ON;
		else {
			dp0->dp_state &= ~DP_BREAK_ON;
			if (kdebug && kdebugbreak && !console_is_tport
			    && (dp0->dp_index == du_console))
				debug("ring");
		}

		/* reset latches */
		WR_WR0(dp0->dp_cntrl, WR0_RST_EXT_INT)
		UNLOCK_PORT(dp0, s);
	}

	/* external status interrupt */
	if (rr3 & RR3_CHNA_EXT_IP) {
		LOCK_PORT(dp1, s);
		rr0 = RD_RR0(dp1->dp_cntrl);
#ifdef IP22
		if (ip24 && (dp1->dp_state & DP_RS422)) {
			DU_CTS_INVERT(rr0);
		}
#endif /* IP22 */

		/* handle CTS interrupt only if we care */
		if (dp1->dp_wr15 & WR15_CTS_IE) {
			if (dp1->dp_state & DP_CTS
			    && !DU_CTS_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state &= ~DP_CTS;
			} else if (!(dp1->dp_state & DP_CTS)
				   && DU_CTS_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state |= DP_CTS;
				UNLOCK_PORT(dp1, s);
				mips_du_start_tx(dp1, 0);
				LOCK_PORT(dp1, s);
			}
		}

		/* handle DCD interrupt only if we care */
		if (dp1->dp_wr15 & WR15_DCD_IE) {
			if (dp1->dp_state & DP_DCD
			    && !DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state &= ~DP_DCD;
				UNLOCK_PORT(dp1, s);
				du_coff(dp1, 0);
				LOCK_PORT(dp1, s);
			} else if (!(dp1->dp_state & DP_DCD)
				   && DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state |= DP_DCD;
				UNLOCK_PORT(dp1, s);
				du_con(dp1);
				LOCK_PORT(dp1, s);
			}
		}

		/* handle BREAK interrupt */
		if (rr0 & RR0_BRK)
			dp1->dp_state |= DP_BREAK_ON;
		else
			dp1->dp_state &= ~DP_BREAK_ON;

		/* reset latches */
		WR_WR0(dp1->dp_cntrl, WR0_RST_EXT_INT)
		UNLOCK_PORT(dp1, s);
	}


	if (rr3 & RR3_CHNB_TX_IP) {
		SYSINFO.xmtint++;
		du_tx(dp0, 0, 0);
	}

	/* transmit buffer empty interrupt */
	if (rr3 & RR3_CHNA_TX_IP) {
		SYSINFO.xmtint++;
		du_tx(dp1, 0, 0);
	}
	return(1);
}


/*
 * Examine each duart to see if it is the one that needs servicing.
 */
/*ARGSUSED*/
static void
du_poll(__psint_t arg, struct eframe_s *ep)
{
	int i;
	int worked;

	do {
		worked = 0;
		for (i = 0; i <= maxport; i += 2)
			worked = du_handle_intr(i);
	} while (worked);
}

/*
 * get a new buffer, should only be called when safe from interrupts
 *
 * called from du_slowr with streams locked.
 * called from du_rx with streams not locked.
 * called from du_rsrv with streams locked.
 */
static mblk_t*				/* return NULL or the new buffer */
du_getbp(dport_t *dp, u_int pri, int strlock)
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
		mips_du_iflow(dp, SEND_XOFF, !IGNORE_IXOFF, strlock);

	return(bp);
}

/*
 * slow and hopefully infrequently used function to put characters somewhere
 * where they will go up stream
 * 
 * called from du_rx with streams locked.
 */
static void
du_slowr(dport_t *dp, u_char c)
{
	mblk_t *bp;

	if (dp->dp_iflag & IBLKMD)	/* this kludge apes the old */
		return;			/* block mode hack */

	if (!(bp = dp->dp_rbp)		/* get buffer if have none */
	    && !(bp = du_getbp(dp, BPRI_HI, 1))) {
		dp->dp_allocb_fail++;
		return;
	}
	*bp->b_wptr = c;
	if (++bp->b_wptr >= bp->b_datap->db_lim) {
		(void)du_getbp(dp, BPRI_LO, 1);	/* send buffer when full */
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
	mblk_t *bp;
	int s, state;

	LOCK_PORT(dp, s);
	/* must be open to input */
	if ((dp->dp_state & (DP_ISOPEN | DP_WOPEN)) != DP_ISOPEN
	    && dp->dp_porthandler == NULL) {
		if (!(dp->dp_state & (DP_ISOPEN | DP_WOPEN))) {
			UNLOCK_PORT(dp, s);
			du_zap(dp, dp->dp_cflag & HUPCL);
		} else {
			UNLOCK_PORT(dp, s);
			du_rclr(dp);	/* just forget it if partly open */
		}
		return;
	}
	UNLOCK_PORT(dp, s);

	/* must be reading, to read.  DCD must be on for modem port to read */
	if (!(dp->dp_cflag & CREAD)) { 
		du_rclr(dp);
		return;
	}

	/*
	 * XXX There may a race here (see du_get_inchars() in mpzduart.c).
	 */
	/* process all available characters */
	while ((c = mips_du_getchar(dp, 0)) != -1 ) {
		SYSINFO.rawch++;

		sr = c >> 8;
		c &= 0xff;

		/* if we get a control-A, debug on this port */
		if (kdebug && !console_is_tport &&
		    (dp->dp_index == du_console) && c == DEBUG_CHAR) {
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

			if (dp->dp_iflag & ISTRIP)
				cs &= 0x7f;
			LOCK_PORT(dp, s);
			if (dp->dp_state & DP_TXSTOP
			    && (cs == dp->dp_cc[VSTART]
			    || (dp->dp_iflag & IXANY
			    && (cs != dp->dp_cc[VSTOP]
			    || dp->dp_line == LDISC0)))) {
				dp->dp_state &= ~DP_TXSTOP;
				UNLOCK_PORT(dp, s);
				mips_du_start_tx(dp, 0);/* restart output */
				if (cs == dp->dp_cc[VSTART])
					continue;
			} else if (DP_LIT & dp->dp_state) {
				dp->dp_state &= ~DP_LIT;
				UNLOCK_PORT(dp, s);
			} else if (cs == dp->dp_cc[VSTOP]) {
				dp->dp_state |= DP_TXSTOP;
				mips_du_stop_tx(dp);	/* stop output */
				UNLOCK_PORT(dp, s);
				continue;
			} else if (cs == dp->dp_cc[VSTART]) {
				UNLOCK_PORT(dp, s);
				continue;	/* ignore extra control-Qs */
			} else if (cs == dp->dp_cc[VLNEXT]
			    && dp->dp_line != LDISC0) {
				dp->dp_state |= DP_LIT;	/* just note escape */
				UNLOCK_PORT(dp, s);
			} else {
				UNLOCK_PORT(dp, s);
			}
		}

		if (sr & RR0_BRK) {
			if (dp->dp_iflag & IGNBRK)
				continue;	/* ignore it if ok */
			if (dp->dp_iflag & BRKINT) {
				STREAMS_LOCK();
				du_flushr(dp);
				(void)putctl1(dp->dp_rq->q_next,
				    M_FLUSH, FLUSHRW);
				(void)putctl1(dp->dp_rq->q_next,
				    M_PCSIG, SIGINT);
				STREAMS_UNLOCK();
				continue;
			}
			if (dp->dp_iflag & PARMRK) {
				STREAMS_LOCK();
				du_slowr(dp, '\377');
				du_slowr(dp, '\000');
				STREAMS_UNLOCK();
			}
			c = '\000';
		} else if (sr &
		    (RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR)) {
			if (sr & RR1_RX_ORUN_ERR)
				dp->dp_over++;	/* count overrun */

			if (dp->dp_iflag & IGNPAR) {
				continue;

			} else if (!(dp->dp_iflag & INPCK)) {
				/* ignore input parity errors if asked */

			} else if (sr & (RR1_PARITY_ERR | RR1_FRAMING_ERR)) {
				if (sr & RR1_FRAMING_ERR)
					dp->dp_fe++;
				if (dp->dp_iflag & PARMRK) {
					STREAMS_LOCK();
					du_slowr(dp, '\377');
					du_slowr(dp, '\000');
					STREAMS_UNLOCK();
				} else {
					c = '\000';
				}
			}


		} else if (dp->dp_iflag & ISTRIP) {
			c &= 0x7f;
		} else if (c == '\377' && dp->dp_iflag & PARMRK) {
			STREAMS_LOCK();
			du_slowr(dp, '\377');
			STREAMS_UNLOCK();
		}


		if (!(bp = dp->dp_rbp)	/* get a buffer if we have none */
		    && !(bp = du_getbp(dp, BPRI_HI, 0))) {
			dp->dp_allocb_fail++;
			continue;	/* forget it no buffer available */
		}
		*bp->b_wptr = c;
		if (++bp->b_wptr >= bp->b_datap->db_lim) {
			(void)du_getbp(dp, BPRI_LO, 0);	/* send when full */
		}
	}


	if (!dp->dp_rsrv_cnt && dp->dp_rbp) {
		if (dp->dp_rsrv_duration <= 0
		    || dp->dp_rsrv_duration > HZ / 10) {
			LOCK_PORT(dp, s);	
			state = dp->dp_state;
			UNLOCK_PORT(dp, s);
			if (state & DP_ISOPEN && canenable(dp->dp_rq))
				streams_interrupt((strintrfunc_t)qenable,
						(void *)dp->dp_rq,
						0, 0);
		} else {
			(void)STREAMS_TIMEOUT(du_rsrv_timer,
			    (caddr_t)dp, dp->dp_rsrv_duration);
			dp->dp_rsrv_cnt = MAX_RSRV_CNT;
		}
	}
}



/*
 * process carrier-on interrupt
 */
static void
du_con(dport_t *dp)
{
	u_int state;
	int s;

	LOCK_PORT(dp, s);
	state = dp->dp_state;
	UNLOCK_PORT(dp, s);

	if (state & DP_WOPEN)
		sv_broadcast(&dp->dp_sv);
}



/*
 * process carrier-off interrupt
 *
 * called from mips_du_config with streams locked/unlocked.
 * called from du_handle_intr with streams not locked.
 */
static void
du_coff(dport_t *dp, int strlock)
{
	int s;
	u_int state;

	LOCK_PORT(dp, s);
	state = dp->dp_state;
	UNLOCK_PORT(dp, s);
	
	if (!(dp->dp_cflag & CLOCAL)	/* worry only for an open modem */
	    && state & DP_ISOPEN) {
		du_zap(dp, HUPCL);	/* kill the modem */
		if (!strlock)
			STREAMS_LOCK();
		flushq(dp->dp_wq, FLUSHDATA);
		(void)putctl1(dp->dp_rq->q_next, M_FLUSH, FLUSHW);
		(void)putctl(dp->dp_rq->q_next, M_HANGUP);
		if (!strlock)
			STREAMS_UNLOCK();
	}
}



/*
 * output something
 *
 * called from mips_du_start_tx with streams locked/unlocked.
 * called from du_handle_intr with streams unlocked.
 */
static void
du_tx(dport_t *dp, int prime, int strlock)
{
	u_char c;
	mblk_t *wbp;
	int local_fifo_size, count;
	int s;

	if (!mips_du_txrdy(dp)) {
		return;
	}

	/* The z85230 can only send one byte unless we just got a fifo
	 * drained interrupt.
	 */
#if IPMHSIM || MHSIM
	if (is_sableio) 
		local_fifo_size = count = fifo_size;
	else
#endif /* IPMHSIM */
	local_fifo_size = count = prime ? 1 : fifo_size;

	if (!strlock)
		STREAMS_LOCK();

	LOCK_PORT(dp, s);

	/* send all we can */
	while (count) {
		if (!(dp->dp_state & DP_ISOPEN)
		    || dp->dp_state & (DP_BREAK | DP_BREAK_QUIET)
		    || dp->dp_state & (DP_TXSTOP | DP_TIMEOUT)
			&& !(dp->dp_state & (DP_TX_TXON | DP_TX_TXOFF))) {
			mips_du_stop_tx(dp);
			UNLOCK_PORT(dp ,s);
			if (!strlock)
				STREAMS_UNLOCK();
			return;
		}

		SYSINFO.outch++;
		if (dp->dp_state & DP_TX_TXON) {	/* send XON or XOFF */
			if (dp->dp_cflag & CNEW_RTSCTS
			    && !(dp->dp_state & DP_CTS)) {
				mips_du_stop_tx(dp);
				UNLOCK_PORT(dp, s);
				if (!strlock)
					STREAMS_UNLOCK();
				return;
			}
			c = dp->dp_cc[VSTART];
			dp->dp_state &= ~(DP_TX_TXON | DP_TX_TXOFF | DP_BLOCK);
			UNLOCK_PORT(dp, s);
		} else if (dp->dp_state & DP_TX_TXOFF) {
			if (dp->dp_cflag & CNEW_RTSCTS
			    && !(dp->dp_state & DP_CTS)) {
				mips_du_stop_tx(dp);
				UNLOCK_PORT(dp, s);
				if (!strlock)
					STREAMS_UNLOCK();
				return;
			}
			c = dp->dp_cc[VSTOP];
			dp->dp_state &= ~DP_TX_TXOFF;
			dp->dp_state |= DP_BLOCK;
			UNLOCK_PORT(dp, s);
		} else {
			/*
			 * possible race condition here.  if thread A
			 * gets preempted after the first conditional is
			 * determined to be true, and thread B
			 * closes the port, getq(dp->dp_wq) will panic the
			 * system when thread A is resumed. thus, need to
			 * verify dp->dp_wq is not null after STREAMS_LOCK()
			 */
			if (!(wbp = dp->dp_wbp)) {	/* get another msg */
				if (!dp->dp_wq) {
					continue;
				} else {
					wbp = getq(dp->dp_wq);
				}
				if (!wbp) {
					mips_du_stop_tx(dp);
					UNLOCK_PORT(dp, s);
					if (!strlock)
						STREAMS_UNLOCK();
					return;
				}

				switch (wbp->b_datap->db_type) {
				case M_DATA:
					dp->dp_wbp = wbp;
					break;

				/* must wait until output drained */
				case M_DELAY:	/* start output delay */
					if (count == local_fifo_size) {
						dp->dp_state |= DP_TIMEOUT;
						dp->dp_tid = STREAMS_TIMEOUT(du_delay,
					    		(caddr_t)dp,
					    		*(int *)wbp->b_rptr);
						freemsg(wbp);
						continue;
					} else {
						putbq(dp->dp_wq, wbp);
						UNLOCK_PORT(dp, s);
						if (!strlock)
							STREAMS_UNLOCK();
						return;
					}

				/* must wait until output drained */
				case M_IOCTL:
					if (count == local_fifo_size) {
						UNLOCK_PORT(dp, s);
						du_i_ioctl(dp, wbp);
						LOCK_PORT(dp, s);
						continue;
					} else {
						putbq(dp->dp_wq, wbp);
						UNLOCK_PORT(dp, s);
						if (!strlock)
							STREAMS_UNLOCK();
						return;
					}

				default:
					panic("bad duart msg");
				}
			}
			if (dp->dp_cflag & CNEW_RTSCTS
			    && !(dp->dp_state & DP_CTS)) {
				mips_du_stop_tx(dp);
				UNLOCK_PORT(dp, s);
				if (!strlock)
					STREAMS_UNLOCK();
				return;
			}
			if (wbp->b_rptr >= wbp->b_wptr) {
				dp->dp_wbp = rmvb(wbp, wbp);
				freeb(wbp);
				continue;
			}

			c = *wbp->b_rptr++;
		}
		mips_du_putchar(dp, c);
		count--;
	}

	/*
	 * If the queue is now empty, put an empty data block on it
	 * to prevent a close from finishing prematurely.
	 */
	if (!dp->dp_wq->q_first && dp->dp_wbp && (wbp = allocb(0, BPRI_HI))) {
		UNLOCK_PORT(dp, s);
		putbq(dp->dp_wq, wbp);
	} else
		UNLOCK_PORT(dp, s);
	if (!strlock)
		STREAMS_UNLOCK();
}



#ifdef GFXKEYPORT
/*
 * set to give some characters away
 */
void
gl_setporthandler(int port, int (*fp)(unsigned char))
{
	dports[port].dp_porthandler = fp;
}
#endif	/* GFXKEYPORT */



/*
 * read up to `len' characters from the secondary console port.
 */
int
ducons_read(u_char *buf, int len)
{
	dport_t *dp = &dports[du_console];
	int i = 0;
	int c;

	while (i < len) {
		if ((c = mips_du_getchar(dp, 0)) == -1)
			break;
		*buf++ = c & 0xff;		/* ignore status */
		i++;
	}

	return(i);
}



/*
 * write `len' characters to the secondary console port, should only be
 * called when safe from interrupt.  in most cases, use the kernel printf()
 * instead of calling this routine directly
 */
/*
 * This routine is a bit of a problem since it can be called from
 * interrupt context, as well as from a thread context.  It uses
 * SPLs for now until we can get the other stuff worked out.
 */
int
ducons_write(u_char *buf, int len)
{
	dport_t *dp = &dports[du_console];
	int i;
	int s;

	int onintstk = ((private.p_kstackflag == PDA_CURINTSTK) ||
			(private.p_kstackflag == PDA_CURIDLSTK && !curvprocp));
	if (onintstk)
		s = spltty();
	else
		LOCK_PORT(dp, s);

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

	if (onintstk)
		splx(s);
	else
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

	if (!kdebug || console_is_tport)
		return;

	if ((c = mips_du_getchar(dp, 1)) != -1) {
		if ((c & 0xff) == DEBUG_CHAR)
			debug("ring");
	}
}


int
du_getchar(int port)
{
	int c;

	/* no locking needed here since mips_du_getchar() will do it */
	if ((c = mips_du_getchar(&dports[port], 0)) != -1)
		c &= 0xff;
	return(c);
}



void
du_putchar(int port, unsigned char c)
{
	dport_t *dp = &dports[port];
	int s;

	LOCK_PORT(dp, s);

	while (!mips_du_txrdy(dp))
		;
	mips_du_putchar(dp, c);
	UNLOCK_PORT(dp, s);
}


/*
 * determine whether the DUART port is ready to transmit a character or not.
 * 0 => not ready
 */
static int
mips_du_txrdy(dport_t *dp)
{
#if IPMHSIM || MHSIM
        /*
         * Simulator should always be ready to xmit a character
         */
        return 1;
#else /* !IPMHSIM */
	volatile u_char *cntrl = dp->dp_cntrl;

	return(RD_RR0(cntrl) & RR0_TX_EMPTY);
#endif /* IPMHSIM */
}

/*
 * determine whether or not the DUART port has completely transmitted all
 * output characters.
 * 0 => not done transmitting
 */
static int
mips_du_allsent(dport_t *dp)
{
#if IPMHSIM || MHSIM
	return(1);
#else
	volatile u_char *cntrl = dp->dp_cntrl;

	return(RD_CNTRL(cntrl, RR1) & RR1_ALL_SENT);
#endif
}

/*
 * transmit a character. du_txrdy must be called first to make sure the
 * transmitter is ready
 */
static void
mips_du_putchar(dport_t *dp, u_char c)
{
	c &= dp->dp_bitmask;
#if IPMHSIM || MHSIM
	if (is_sableio) {
		DCL_SCONS_P(scp,dp->dp_index);

		scp->sc_txbuf = c;
		scp->sc_command |= SC_CMD_TXFLUSH;
	} else
		MHSIM_DUART_WRITE(dp->dp_data, c);
#else /* IPMHSIM */
	WR_DATA(dp->dp_data, c)
#endif /* IPHMSIM */
}



/********** du_getchar **********/
/*
 * get a character from the receiver. if none received, return -1.
 * append status to data
 */
/*
 * The frompoll argument is used to denote that this routine *may*
 * have been called from interrupt context.  We don't bother with
 * the sleep locks if so.
 */

static int
mips_du_getchar(dport_t *dp, int frompoll)
{
	int c, ch;
	volatile u_char *cntrl = dp->dp_cntrl;
	int s;
#if IPMHSIM || MHSIM

	c = 0;
	if (is_sableio) {
		DCL_SCONS_P(scp,dp->dp_index);

		if (dp->dp_index == 0 &&
		    (scp->sc_status & SC_STAT_RXRDY))
			ch = scp->sc_rx;
		else
			ch = -1;
	} else
		MHSIM_DUART_READ(dp->dp_data, ch);
	if (ch < 0) {
	   return -1;
	} else {
	   return (ch & dp->dp_bitmask);
	}
#else /* IPMHSIM */
	if (!frompoll)
		LOCK_PORT(dp, s);
	if (RD_RR0(cntrl) & RR0_RX_CHR) {
		c = RD_CNTRL(cntrl, RR1);	/* read possible err status */
		if (c &= (RR1_FRAMING_ERR|RR1_RX_ORUN_ERR|RR1_PARITY_ERR)) {
			WR_WR0(cntrl, WR0_RST_ERR)
#if DEBUG && !MFG
			if ( c & RR1_FRAMING_ERR ) {
				if (!frompoll)
					UNLOCK_PORT(dp, s);
				cmn_err(CE_WARN,
					"!Framing error on serial port %d",
					dp->dp_index);
				if (!frompoll)
					LOCK_PORT(dp, s);
			}
			if ( c & RR1_RX_ORUN_ERR ) {
				if (!frompoll)
					UNLOCK_PORT(dp, s);
				cmn_err(CE_WARN,
					"!Overrun error on serial port %d",
					dp->dp_index);
				if (!frompoll)
					LOCK_PORT(dp, s);
			}
			if ( c & RR1_PARITY_ERR ) {
				if (!frompoll)
					UNLOCK_PORT(dp, s);
				cmn_err(CE_WARN,
					"!Parity error on serial port %d",
					dp->dp_index);
				if (!frompoll)
					LOCK_PORT(dp, s);
			}
#endif	/* DEBUG */
		}

		if (dp->dp_state & DP_BREAK_ON)
			c |= RR0_BRK;

#ifdef _INTR_TIME_TRACING
		{
			struct du_rx_trace_s *drp;
			unsigned char tc;

			tc = (RD_DATA(dp->dp_data) & dp->dp_bitmask);
			if (!frompoll)
				UNLOCK_PORT(dp, s);
			drp = du_rx_trace + du_rx_trace_next;
			drp->timestamp = get_timestamp();
			drp->ichar = tc;
			drp->sr = c;
			if (du_rx_trace_next == (DU_RX_TRACE_MAX - 1))
				du_rx_trace_next = 0;
			else
				du_rx_trace_next++;
			return((c << 8) | tc);
		}
#else
		ch = ((c << 8) | (RD_DATA(dp->dp_data) & dp->dp_bitmask));
		if (!frompoll)
			UNLOCK_PORT(dp, s);
		return (ch);
#endif
	} else {
		if (!frompoll)
			UNLOCK_PORT(dp, s);
		return(-1);
	}
#endif /* IPMHSIM */
}

static void
mips_du_stop_tx(dport_t *dp)
{
	volatile u_char *cntrl = dp->dp_cntrl;

	dp->dp_wr1 &= ~WR1_TX_INT_ENBL;

#if IPMHSIM || MHSIM
	if (! is_sableio)
#endif /* IPMHSIM */
	WR_CNTRL(cntrl, WR1, dp->dp_wr1)
	WR_WR0(cntrl, WR0_RST_TX_INT)
}


#if 0
static void
dsp_du_stop_tx(dport_t *dp)
{
}
#endif


/********** du_start_tx **********/
/*
 * called from mips_du_iflow with streams locked/unlocked.
 * called from du_delay with streams locked.
 * called from du_save with streams locked.
 * called from du_wput with streams locked.
 * called from du_handle_intr with streams not locked.
 * called from du_rx with streams not locked.
 */
static void
mips_du_start_tx(dport_t *dp, int strlock)
{
	volatile u_char *cntrl = dp->dp_cntrl;
	int s;

	LOCK_PORT(dp, s);
	if (!(dp->dp_wr1 & WR1_TX_INT_ENBL)) {
		dp->dp_wr1 |= WR1_TX_INT_ENBL;
#if IPMHSIM || MHSIM
		if (! is_sableio)
#endif /* IPMHSIM */
		WR_CNTRL(cntrl, WR1, dp->dp_wr1)
		UNLOCK_PORT(dp, s);
		du_tx(dp, 1, strlock);		/* send something */
	} 
	else {
		UNLOCK_PORT(dp, s);
	}
}



static void
mips_du_stop_rx(dport_t *dp, int hup)
{
	volatile u_char *cntrl = dp->dp_cntrl;
	char *console = kopt_find("console");
	int s;

	LOCK_PORT(dp, s);
	if (!(kdebug && (dp->dp_index == du_console))
#ifdef GFXKEYPORT
	    && dp != &dports[GFXKEYPORT]
#endif
	    ) {
		dp->dp_wr1 &= ~WR1_RX_INT_MSK;	/* disable receiver interrupt */
		dp->dp_wr15 &= ~WR15_BRK_IE;	/* disable BREAK interrupt */
	}

	/* deassert RTS, DTR if hup != 0 */
	if (hup && !(console && *console == 'd' &&
	    (dp->dp_index == du_console))) {
#ifdef ENETCOUNT
		if (dp->dp_index != MOUSEPORT)
#endif
			dp->dp_wr15 &= ~(WR15_DCD_IE | WR15_CTS_IE);

		/* deassert output handshake signals on tty port */
#ifdef GFXKEYPORT
		if (dp->dp_index != GFXKEYPORT && dp->dp_index != MOUSEPORT)
#endif
		{
			DU_DTR_DEASSERT(dp->dp_wr5);
#ifdef IP22
			if (ip24 && (dp->dp_state & DP_RS422))
				DU_DTR_INVERT (dp->dp_wr5);
#endif
			DU_RTS_DEASSERT(dp->dp_wr5);
			WR_CNTRL(cntrl, WR5, dp->dp_wr5)
		}
	}

	/*
	 * disable global external interrupt if none of the individual external
	 * interrupt is enabled
	 */
	if (!(dp->dp_wr15 & (WR15_BRK_IE | WR15_CTS_IE | WR15_DCD_IE)))
		dp->dp_wr1 &= ~WR1_EXT_INT_ENBL;

	/* update hardware registers */
#if IPMHSIM || MHSIM
	if (is_sableio) {
		DCL_SCONS_P(scp,dp->dp_index);

		if (dp->dp_wr1 & WR1_RX_INT_ERR_ALL_CHR) {
			if (! (scp->sc_command & SC_CMD_RXIE))
				scp->sc_command |= SC_CMD_RXIE;
		} else {
			if (scp->sc_command & SC_CMD_RXIE)
				scp->sc_command &= ~SC_CMD_RXIE;
		}
	} else
#endif /* IPMHSIM */
	WR_CNTRL(cntrl, WR1, dp->dp_wr1)
	WR_CNTRL(cntrl, WR15, dp->dp_wr15)
	UNLOCK_PORT(dp, s);
}

/*
 * return !0 if carrier detected
 */
static int
mips_du_start_rx(dport_t *dp)
{
	volatile u_char *cntrl = dp->dp_cntrl;
	u_char o_imr;
	u_char stat;
	u_int state;
	int s;

	LOCK_PORT(dp, s);
	if (dp->dp_state & DP_RS422) {
		DU_DTR_ASSERT(dp->dp_wr5);
#if defined(IP20)
		DU_RTS_DEASSERT(dp->dp_wr5);
#endif	/* IP20 */
#if IP22 || IP26 || IP28
#if IP22
		if (ip24)
			DU_DTR_INVERT(dp->dp_wr5);
#endif
		DU_RTS_ASSERT(dp->dp_wr5);
#endif	/* IP22 || IP26 || IP28 */
	} else
#ifdef GFXKEYPORT
	if (dp->dp_index != GFXKEYPORT &&  dp->dp_index != MOUSEPORT)
#endif
	{
		DU_DTR_ASSERT(dp->dp_wr5);
		DU_RTS_ASSERT(dp->dp_wr5);
	}
	WR_CNTRL(cntrl, WR5, dp->dp_wr5)

	o_imr = dp->dp_wr1;
	dp->dp_wr1 |=  WR1_RX_INT_ERR_ALL_CHR;

	/* watch carrier-detect only if this is an open modem port */
	if (!(dp->dp_cflag & CLOCAL)
#ifdef GFXKEYPORT
	    && dp->dp_index != GFXKEYPORT && dp->dp_index != MOUSEPORT
#endif
	    && dp->dp_state & (DP_ISOPEN | DP_WOPEN))
		dp->dp_wr15 |= WR15_DCD_IE;

	/* Enable CTS only if CNEW_RTSCTS is set on an open modem port*/
	if ((dp->dp_cflag & CNEW_RTSCTS)
#ifdef GFXKEYPORT
	    && dp->dp_index != GFXKEYPORT && dp->dp_index != MOUSEPORT
#endif
	    && dp->dp_state & (DP_ISOPEN | DP_WOPEN))
		dp->dp_wr15 |= WR15_CTS_IE;


	dp->dp_wr15 |= WR15_BRK_IE;
	WR_CNTRL(cntrl, WR15, dp->dp_wr15)

	if (dp->dp_wr15 & (WR15_DCD_IE | WR15_CTS_IE | WR15_BRK_IE))
		dp->dp_wr1 |= WR1_EXT_INT_ENBL;
	else
		dp->dp_wr1 &= ~WR1_EXT_INT_ENBL;

#if IPMHSIM || MHSIM
	if (is_sableio) {
		DCL_SCONS_P(scp,dp->dp_index);

		if (dp->dp_wr1 & WR1_RX_INT_ERR_ALL_CHR) {
			if (! (scp->sc_command & SC_CMD_RXIE))
				scp->sc_command |= SC_CMD_RXIE;
		} else {
			if (scp->sc_command & SC_CMD_RXIE)
				scp->sc_command &= ~SC_CMD_RXIE;
		}
	} else
#endif /* IPMHSIM */
	WR_CNTRL(cntrl, WR1, dp->dp_wr1)
	UNLOCK_PORT(dp, s);
	/* if now enabling input, gobble old stuff */
	if (!(o_imr & WR1_RX_INT_MSK))
		du_rclr(dp);

	LOCK_PORT(dp, s);
	/* make sure the external status bits are current */
	WR_WR0(cntrl, WR0_RST_EXT_INT)
	stat = RD_RR0(cntrl);
#ifdef IP22
	if (ip24 && (dp->dp_state & DP_RS422)) {
		DU_CTS_INVERT(stat);
	}
#endif /* IP22 */

	if (stat & RR0_BRK)		/* receiving break ? */
		dp->dp_state |= DP_BREAK_ON;
	else
		dp->dp_state &= ~DP_BREAK_ON;

	if (DU_CTS_ASSERTED(stat))
		dp->dp_state |= DP_CTS;
	else
		dp->dp_state &= ~DP_CTS;

	if (DU_DCD_ASSERTED(stat))
		dp->dp_state |= DP_DCD;
	else
		dp->dp_state &= ~DP_DCD;

	state = dp->dp_state & DP_DCD;
	UNLOCK_PORT(dp, s);
	return (state);
}



/*
 * called from du_tcset with streams locked.
 * called from du_init with streams not locked.
 * called from du_open with streams locked.
 */
static void
mips_du_config(dport_t *dp, tcflag_t cflag, speed_t ospeed, struct termio *tp,
		int strlock)
{
	volatile u_char *cntrl = dp->dp_cntrl;
	tcflag_t delta_cflag;
	int delta_ospeed;
	u_short time_constant;
	extern int baud_to_cbaud(speed_t);	       /* in streamio.c */
	int s;

	delta_cflag = (cflag ^ dp->dp_cflag)
		& (CLOCAL|CSIZE|CSTOPB|PARENB|PARODD|CNEW_RTSCTS);
	if (ospeed == __INVALID_BAUD)
		ospeed = dp->dp_ospeed;
	delta_ospeed = ospeed != dp->dp_ospeed;

	if (tp)
		dp->dp_termio = *tp;
	dp->dp_cflag = cflag;
	dp->dp_ospeed = dp->dp_ispeed = ospeed;

	/* 
	 * always configures during open in case the last process set up 
	 * the port to use the external clock and did not set it back to use
	 * the internal BRG
	 */
	LOCK_PORT(dp, s);
	if (delta_cflag || delta_ospeed || !(dp->dp_state & DP_ISOPEN)) {
		if (ospeed == B0) {	/* hang up line if asked */
			UNLOCK_PORT(dp, s);
			du_coff(dp, strlock);
			du_zap(dp, HUPCL);
		} else {
			dp->dp_wr3 = WR3_RX_ENBL;
			if (dp->dp_state & DP_EXTCLK)
				dp->dp_wr4 &= WR4_CLK_MSK;
			else
				dp->dp_wr4 = WR4_X16_CLK;
			dp->dp_wr5 = (dp->dp_wr5 & (WR5_RTS | WR5_DTR)) | WR5_TX_ENBL;

			/* set number of stop bits */
			dp->dp_wr4 |= (cflag & CSTOPB) ? WR4_2_STOP : WR4_1_STOP;

			/* set number of data bits */
			switch (cflag & CSIZE) {
			case CS5:
				dp->dp_bitmask = 0x1f;
				break;
			case CS6:
				dp->dp_bitmask = 0x3f;
				dp->dp_wr3 |= WR3_RX_6BIT;
				dp->dp_wr5 |= WR5_TX_6BIT;
				break;
			case CS7:
				dp->dp_bitmask = 0x7f;
				dp->dp_wr3 |= WR3_RX_7BIT;
				dp->dp_wr5 |= WR5_TX_7BIT;
				break;
			case CS8:
				dp->dp_bitmask = 0xff;
				dp->dp_wr3 |= WR3_RX_8BIT;
				dp->dp_wr5 |= WR5_TX_8BIT;
				break;
			}

			/* set parity */
			if (cflag & PARENB) {
				dp->dp_wr4 |= WR4_PARITY_ENBL;
				if (!(cflag & PARODD))
					dp->dp_wr4 |= WR4_EVEN_PARITY;
			}

			/* set transmitter/receiver control */
			WR_CNTRL(cntrl, WR4, dp->dp_wr4)
			WR_CNTRL(cntrl, WR3, dp->dp_wr3 & ~WR3_RX_ENBL)
			WR_CNTRL(cntrl, WR5, dp->dp_wr5 & ~WR5_TX_ENBL)

			/* use NRZ encoding format*/
			WR_CNTRL(cntrl, WR10, 0x0)

			/* use internal BRG if is not using external clock */
			if (!(dp->dp_state & DP_EXTCLK))
				WR_CNTRL(cntrl, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG)

			/* set time constant according to baud rate */
			WR_CNTRL(cntrl, WR14, 0x0)
			time_constant = BAUD(ospeed);
			WR_CNTRL(cntrl, WR12, time_constant & 0xff)
			WR_CNTRL(cntrl, WR13, time_constant >> 8)

			/* enable internal BRG if is not using external clock */
			if (!(dp->dp_state & DP_EXTCLK))
				WR_CNTRL(cntrl, WR14, WR14_BRG_ENBL)

			/* enable transmitter/receiver */
			WR_CNTRL(cntrl, WR3, dp->dp_wr3)
			WR_CNTRL(cntrl, WR5, dp->dp_wr5)

#if IPMHSIM || MHSIM
			/*
			 * This tells the simulator to configure the terminal.
			 */
			if (is_sableio) {
				DCL_SCONS_P(scp,dp->dp_index);
				
				if (! (scp->sc_command & SC_CMD_RXIE))
					scp->sc_command = SC_CMD_RXIE;
			} else
				MHSIM_DUART_WRITE(MHSIM_DUART_INTR_ENBL_ADDR, MHSIM_DUART_RX_INTR_STATE);
#endif /* IPMHSIM */
			UNLOCK_PORT(dp, s);
			(void)mips_du_start_rx(dp);
		}
	} else
		UNLOCK_PORT(dp, s);
}


/*
 * called from du_rsrv with streams locked.
 * called from du_wput with streams locked.
 * called from du_getbp with streams locked/unlocked.
 */
static void
mips_du_iflow(dport_t *dp, int send_xon, int ignore_ixoff, int strlock)
{
	volatile u_char *cntrl = dp->dp_cntrl;
	int s;

	LOCK_PORT(dp, s);
	if (dp->dp_cflag & CNEW_RTSCTS) { /* hardware flow control */
		if (send_xon && !DU_RTS_ASSERTED(dp->dp_wr5)) {
			DU_RTS_ASSERT(dp->dp_wr5);
			WR_CNTRL(cntrl, WR5, dp->dp_wr5)
		} else if (!send_xon && DU_RTS_ASSERTED(dp->dp_wr5)) {
			DU_RTS_DEASSERT(dp->dp_wr5);
			WR_CNTRL(cntrl, WR5, dp->dp_wr5)
		}
	}

	if (!ignore_ixoff && !(dp->dp_iflag & IXOFF)) {
		UNLOCK_PORT(dp, s);
		return;
	} else {
		if (send_xon && dp->dp_state & DP_BLOCK) {
			dp->dp_state |= DP_TX_TXON;
			UNLOCK_PORT(dp, s);
			mips_du_start_tx(dp, strlock);
		}
		else if (!send_xon && !(dp->dp_state & DP_BLOCK)) {
			dp->dp_state |= DP_TX_TXOFF;
			dp->dp_state &= ~DP_TX_TXON;
			UNLOCK_PORT(dp, s);
			mips_du_start_tx(dp, strlock);
		}
	}
	UNLOCK_PORT(dp, s);
}



static void
mips_du_break(dport_t *dp, int start_break)
{
	volatile u_char *cntrl = dp->dp_cntrl;

	if (start_break)
		dp->dp_wr5 |= WR5_SEND_BRK;
	else
		dp->dp_wr5 &= ~WR5_SEND_BRK;
	WR_CNTRL(cntrl, WR5, dp->dp_wr5)
}



#if ENETCOUNT
/* set up to do ethernet carrier detection */
void
reset_enet_carrier()
{
	dport_t	*dp = &dports[MOUSEPORT];

	WR_WR0(dp->dp_cntrl, WR0_RST_EXT_INT)
	enet_carrier = DU_DCD_ASSERTED((RD_RR0(dp->dp_cntrl)));

	if (!enet_carrier) {
		dp->dp_wr15 |= WR15_DCD_IE;
		WR_CNTRL(dp->dp_cntrl, WR15, dp->dp_wr15)

		dp->dp_state &= ~DP_DCD;
	}
}



/*
 * return ethernet carrier status, reset_enet_carrier() must be called before
 * calling this routine
 */
int
enet_carrier_on()
{
	dport_t	*dp = &dports[GFXKEYPORT];

	if (!enet_carrier)
		enet_carrier = RD_CNTRL(dp->dp_cntrl, RR3) & RR3_CHNB_EXT_IP;

	return(enet_carrier);
}



/*
 * turn heart beat LED on/off for IP20
 * control color of LED on Hollywood.  0 -> green LED, !0 -> amber led
 */
void
heart_led_control(int on)
{
	dport_t *dp = &dports[GFXKEYPORT];

	if (on)
		DU_RTS_DEASSERT(dp->dp_wr5);
	else
		DU_RTS_ASSERT(dp->dp_wr5);

	WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5)
}
#endif	/* IP20 */


#ifdef DEBUG
static int
du_loopback(dport_t *dp, mblk_t *bp)
{
	register u_int enable = *(u_int *)(bp->b_cont->b_rptr);
	register u_int loopback = enable ? WR14_LCL_LPBK : 0;
	volatile u_char *cntrl = dp->dp_cntrl;

	/* disable transmitter/receiver first */
	WR_CNTRL(cntrl, WR3, dp->dp_wr3 & ~WR3_RX_ENBL)
	WR_CNTRL(cntrl, WR5, dp->dp_wr5 & ~WR5_TX_ENBL)

	/* set loopback mode - must be done after setting EXTCLK */
	if (dp->dp_state & DP_EXTCLK) {
		WR_CNTRL(cntrl, WR14, loopback)
	} else {
		WR_CNTRL(cntrl, WR14, WR14_BRG_ENBL | loopback)
	}

	/* re-enable transmitter/receiver */
	WR_CNTRL(cntrl, WR3, dp->dp_wr3)
	WR_CNTRL(cntrl, WR5, dp->dp_wr5)

	return 0;
}
#endif

static int
du_rs422(dport_t *dp, mblk_t *bp)
{
#if defined(IP20)
	volatile u_char *port_config = (u_char *)PHYS_TO_K1(PORT_CONFIG);
#elif defined(IP22) || defined(IP26) || defined(IP28)
	volatile uint *port_config = (uint *)PHYS_TO_COMPATK1(HPC3_WRITE2);
#endif
	int s;

	LOCK_PORT(dp, s);
	if (*(u_int *)(bp->b_cont->b_rptr) == RS422_OFF) {
		dp->dp_state &= ~DP_RS422;

#if defined(IP20)
		if (dp->dp_index == maxport)
			*port_config |= PCON_SER0RTS;
		else
			*port_config |= PCON_SER1RTS;
		DU_RTS_ASSERT(dp->dp_wr5);
#elif defined(IP22) || defined(IP26) || defined(IP28)
		if (dp->dp_index == maxport)
			_hpc3_write2 |= UART0_ARC_MODE;
		else
			_hpc3_write2 |= UART1_ARC_MODE;
		*port_config = _hpc3_write2;
		DU_RTS_DEASSERT(dp->dp_wr5);
#endif

		WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5)
		UNLOCK_PORT(dp, s);
		return 0;

	} else if (*(u_int *)(bp->b_cont->b_rptr) == RS422_ON) {
		dp->dp_state |= DP_RS422;

#if defined(IP20)
		if (dp->dp_index == maxport)
			*port_config &= ~PCON_SER0RTS;
		else
			*port_config &= ~PCON_SER1RTS;
		DU_RTS_DEASSERT(dp->dp_wr5);
#elif defined(IP22) || defined(IP26) || defined(IP28)
		if (dp->dp_index == maxport)
			_hpc3_write2 &= ~UART0_ARC_MODE;
		else
			_hpc3_write2 &= ~UART1_ARC_MODE;
		*port_config = _hpc3_write2;
		DU_RTS_ASSERT(dp->dp_wr5);
#endif

		WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5)
		UNLOCK_PORT(dp, s);
		return 0;

	} else {
		UNLOCK_PORT(dp, s);
		return EINVAL;
	}
}


static int
du_extclk(dport_t *dp, mblk_t *bp)
{
	u_int clock_factor = *(u_int *)(bp->b_cont->b_rptr);
	volatile u_char *cntrl = dp->dp_cntrl;
	int s;

	if (clock_factor != EXTCLK_OFF && clock_factor != EXTCLK_1X
	    && clock_factor != EXTCLK_16X && clock_factor != EXTCLK_32X
	    && clock_factor != EXTCLK_64X)
		return EINVAL;

	LOCK_PORT(dp, s);
	/* disable transmitter/receiver first */
	WR_CNTRL(cntrl, WR3, dp->dp_wr3 & ~WR3_RX_ENBL)
	WR_CNTRL(cntrl, WR5, dp->dp_wr5 & ~WR5_TX_ENBL)

	WR_CNTRL(cntrl, WR14, 0x0)	/* disable BRG */

	dp->dp_wr4 &= ~WR4_CLK_MSK;
	if (clock_factor == EXTCLK_OFF) {
		dp->dp_state &= ~DP_EXTCLK;
		dp->dp_wr4 |= WR4_X16_CLK;
		WR_CNTRL(cntrl, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG)
		WR_CNTRL(cntrl, WR14, WR14_BRG_ENBL)

	} else {
		dp->dp_state |= DP_EXTCLK;
		dp->dp_wr4 |= clock_factor;
		WR_CNTRL(cntrl, WR11, WR11_RCLK_TRXC | WR11_TCLK_TRXC)
	}

	/* enable transmitter/receiver and the clock */
	WR_CNTRL(cntrl, WR4, dp->dp_wr4)
	WR_CNTRL(cntrl, WR3, dp->dp_wr3)
	WR_CNTRL(cntrl, WR5, dp->dp_wr5)

	UNLOCK_PORT(dp, s);

	return 0;
}

/* -------------------- DEBUGGING SUPPORT -------------------- */

/*ARGSUSED*/
void
dump_dport_info(__psint_t n)
{
	qprintf("Not supported.\n");
}

/*ARGSUSED*/
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

/*
 * Locking wrappers.
 *
 * Note that the DUARTS come in pairs, and so the lock at dports[0]
 * also locks dports[1], and so on.
 * dwong: since the shared registers are either read only or not being
 *	used other than in the the init routine, each port can have it's
 *	own mutex
 *
 * Note the gross test for whether or not we're being called
 * on an interrupt stack (instead of thread stack).  We have to
 * fiddle the spl if we can't risk sleeping on the mutex.
 */
static void
du_initport(dport_t *dp)
{
	spinlock_init(&dp->dp_lock, "dp");
	sv_init(&dp->dp_sv, 0, "dp_sv");
}

static int
du_lock(dport_t *dp)
{
	return (mutex_spinlock(&dp->dp_lock));
}

static void
du_unlock(dport_t *dp, int s)
{
	mutex_spinunlock(&dp->dp_lock, s);
}
