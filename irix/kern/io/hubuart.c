/* Copyright 1986,1992, Silicon Graphics Inc., Mountain View, CA. */
/*
 * streams driver for the Intel 8251 UART on SN0 Hub chip
 *
 * $Revision: 1.25 $
 */

#if defined(SN) && defined(SABLE)

#include "sys/cmn_err.h"

#include "sys/debug.h"

#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/kmem.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/stream.h"
#include "sys/strids.h"
#include "sys/strmp.h"
#include "sys/stropts.h"
#include "sys/strsubr.h"
#include "sys/stty_ld.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/termio.h"
#include "sys/time.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sema.h"
#include "sys/edt.h"
#include "sys/i8251uart.h"
#include "sys/z8530.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/capability.h"
#include "sys/SN/agent.h"
#include "sys/SN/intr.h"
#include "sys/idbgentry.h"

extern cpuid_t master_procid;

#include "sys/mpzduart.h"
#include "sys/idbgactlog.h"

#ifdef DEBUG
static du_debug = 0;
#endif /* DEBUG */

/* #define FAKEKBDMS	*/	/* enable fake keyboard/mouse for fh bringup */


extern int posix_tty_default;
extern int kdebugbreak;

#define	CNTRL_A		'\001'
#ifdef DEBUG
  #ifndef DEBUG_CHAR
  #define DEBUG_CHAR	CNTRL_A
  #endif
#else /* !DEBUG */
  #define DEBUG_CHAR	CNTRL_A
#endif /* DEBUG */

static int def_console_baud = B9600;	/* default speed for the console */

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

#define RBUF_LEN    256			/* minimum buffer size */

#define MAX_RSRV_CNT	3		/* continue input timer this long */
extern int duart_rsrv_duration;		/* send input this often */

static void du_rsrv(queue_t *);
struct cred;
static int du_open(queue_t *, dev_t *, int, int, struct cred *);
static int du_close(queue_t *, int, struct cred *);
static int du_handle_intr(int, int);
int hubuart_handle_intr(int duartnum);

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
#define PORT(dev)	0
#define MODEM(dev)	((dev) & (NUMMINOR * 3))
#define FLOW_MODEM(dev)	((dev) & (NUMMINOR * 2))

#define	SCNDCPORT	0	/* dport index for the console */

/* start and end of array offsets for active lines */
int duartbase = 0;
int maxduart;

/* priority with which to run the low level interrupt (set by hi level). */
static enum {
    NEVER,		/* low level need not be run at this time */
    EVENTUALLY,		/* low level should be run by a timeout */
    NOW,		/* generate low level intr now */

    EXCLUSIVELY		/* generate low level intr and disable hi level
			 * until low level has completed, so low level
			 * is guaranteed to run before next hi level */

} lointr_mustrun;

/* higher priority overrides lower priority */
#define RUNLOW(level) \
{ \
    if (lointr_mustrun < level) \
	lointr_mustrun = level; \
}

static int got_chars_this_intr;
int local_rsrv_duration;

/* TX buffer */
#define CMD_NULL	0
#define CMD_COFF	1
#define CMD_CON		2
#define CMD_ZAP		3

#define EMPTY_TX_BUF(dp) (dp->dp_tx_rind == dp->dp_tx_wind)
#define FULL_TX_BUF(dp)  (dp->dp_tx_wind == dp->dp_tx_rind+TX_BUFSIZE)
#define RESET_TX_BUF(dp) dp->dp_tx_wind = dp->dp_tx_rind = 0

#define LOCK_TX_BUF(dp)		io_splockspl(dp->dp_tx_buflock, spl7)
#define UNLOCK_TX_BUF(dp, s)	io_spunlockspl(dp->dp_tx_buflock, s)
#define INIT_TX_BUFLOCK(dp) \
		init_spinlock(&dp->dp_tx_buflock, "dut", dp->dp_index)

/* RX buffer */

#define LOCK_RX_BUF(dp)		io_splockspl(dp->dp_rx_buflock, spl7)
#define UNLOCK_RX_BUF(dp, s)	io_spunlockspl(dp->dp_rx_buflock, s)

#define EMPTY_RX_BUF(dp) (dp->dp_rx_rind == dp->dp_rx_wind)
#define FULL_RX_BUF(dp)  (dp->dp_rx_wind == dp->dp_rx_rind+RX_BUFSIZE)
#define HIWAT_RX_BUF(dp) (dp->dp_rx_wind >= dp->dp_rx_rind+(RX_BUFSIZE/2))
#define ALMOST_HIWAT_RX_BUF(dp)\
    (dp->dp_rx_wind >= (dp->dp_rx_rind+(RX_BUFSIZE/2)-64))
#define RESET_RX_BUF(dp) dp->dp_rx_rind = dp->dp_rx_wind = 0

#define CLEAR_RX_BUF(dp) \
	{\
		int ospl; \
		ospl = LOCK_RX_BUF(dp); \
		dp->dp_rx_rind = dp->dp_rx_wind; \
		UNLOCK_RX_BUF(dp, ospl); \
	}

#define INIT_RX_BUFLOCK(dp) \
		init_spinlock(&dp->dp_rx_buflock, "dur", dp->dp_index)

#if DEBUG && !SABLE
#define LOCK_DU_ACTLOG(dp)	io_splockspl(dp->dp_al_lock, spl7)
#define UNLOCK_DU_ACTLOG(dp, s)	io_spunlockspl(dp->dp_al_lock, s)
#define INIT_DU_ACTLOG_LOCK(dp) \
		init_spinlock(&dp->dp_al_lock, "dua", dp->dp_index)

#define DU_LOG_ACT(dp, act)		du_log_act(dp, act, 0)
#define DU_LOG_ACT_INFO(dp, act, i)	du_log_act(dp, act, i)
#else /* DEBUG */
#define LOCK_DU_ACTLOG(dp)	1
#define UNLOCK_DU_ACTLOG(dp, s)
#define DU_LOG_ACT(dp, act)
#define DU_LOG_ACT_INFO(dp, act, i)
#define INIT_DU_ACTLOG_LOCK(dp)
#endif /* DEBUG */

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
#define DP_SE_PENDING   0x00000800      /* buffer alloc event pending */

#define DP_TX_EMPTY	0x00001000      /* TX buffer empty */
#define DP_TX_START	0x00002000      /* TX needs to be started */

#define DP_ENABLE_WAIT	0x00004000	/* Hold close until enable complete. */

#define	DP_EXTCLK	0x10000000	/* external clock mode */
#define	DP_RS422	0x20000000	/* RS422 compatibility mode */
#define DP_FLOW		0x40000000	/* do hardware flow control */
#define	DP_MODEM	0x80000000	/* modem device */

#define	DP_BREAK_ON	0x00010000	/* receiving break */
#define	DP_CTS		0x00020000	/* CTS on */

#define	CTS_XOR_MASK	0x00
#define	DCD_XOR_MASK	0x00
#define	DTR_XOR_MASK	0x00
#define	RTS_XOR_MASK	0x00

dport_t dports[2];
static int max_kbdms_port;


static int maxport = 1;		/* maximum number of ports */
static fifo_size = 4;		/* on chip transmit FIFO size */
static unsigned int mouseports;
static unsigned int gfxkeyports;

static int mips_du_allsent_nolock(dport_t *);
static void mips_du_break(dport_t *, int);
static void mips_du_config(dport_t *, tcflag_t, speed_t, struct termio *);
static int mips_du_getchar(dport_t *, int);
static int nolock_du_getchar(dport_t *);
#define	SEND_XON	1
#define	SEND_XOFF	0
#define	IGNORE_IXOFF	1
static void mips_du_iflow(dport_t *, int, int);
static void mips_du_putchar(dport_t *, u_char);
static void mips_du_putchar_nolock(dport_t *, u_char);
static int mips_du_start_rx(dport_t *);
static void mips_du_start_tx(dport_t *);
static void mips_du_stop_rx(dport_t *, int);
static void mips_du_stop_tx(dport_t *);
static int mips_du_txrdy(dport_t *);
static int mips_du_txrdy_nolock(dport_t *);

static void du_coff(dport_t *);
static void du_con(dport_t *);
static int du_extclk(dport_t *, mblk_t *);
static void du_flushw(dport_t *);
static void du_flushr(struct dport *);
static mblk_t *du_getbp(dport_t *, u_int);
static int du_rs422(dport_t *, mblk_t *);
static int du_rx(dport_t *, int);
static int du_tx(dport_t *, int);
static void du_rclr(dport_t *);
static void du_zap(dport_t *dp, int hup);
static int du_process_inchar(dport_t *, int);

#define SCHED_NOTHING	0
#define SCHED_TX	1
#define SCHED_RX	2

#if DEBUG

/* Activity log */
void
du_log_act(dport_t *dp, enum duact act, int info)
{
	register int s, wind;
	timespec_t tv;


	s = LOCK_DU_ACTLOG(dp);

	nanotime(&tv);

	if ((wind = dp->dp_al_wind++) == DU_ACTLOG_SIZE)
		wind = dp->dp_al_wind = 0;

	dp->dp_actlog[wind].dae_act = act;
	dp->dp_actlog[wind].dae_sec = tv.tv_sec;
	dp->dp_actlog[wind].dae_usec = tv.tv_nsec/1000;
	dp->dp_actlog[wind].dae_info = info;
	dp->dp_actlog[wind].dae_cpu = cpuid();

	UNLOCK_DU_ACTLOG(dp, s);
}

#endif /* DEBUG */

/* port locking routines to support multiple lock requests from the
 * same cpu
 */
static int
du_lock_port(dport_t *dp, int try)
{

    int s;
ASSERT_ALWAYS(dp == dports);
ASSERT_ALWAYS(dp->dp_index == 0);
    dp = &(dports[(dp->dp_index/2)*2]);
    if (dp->dp_port_lockcpu == cpuid()) {
	ASSERT(cpuid() != -1);
	ASSERT(dp->dp_port_locktrips > 0);
	dp->dp_port_locktrips++;
	return(0);
    }
    s = io_splockspl(dp->dp_port_lock, spl7);
ASSERT_ALWAYS(dp->dp_port_locktrips == 0);
ASSERT_ALWAYS(dp->dp_port_lockcpu == -1);
    dp->dp_port_locktrips = 1;
    dp->dp_port_lockcpu = cpuid();
    return(s);
}

static void
du_unlock_port(dport_t *dp, int ospl)
{
ASSERT_ALWAYS(dp == dports);
ASSERT_ALWAYS(dp->dp_index == 0);
    dp = &(dports[(dp->dp_index/2)*2]);
    
ASSERT_ALWAYS(dp->dp_port_lockcpu == cpuid());
ASSERT_ALWAYS(dp->dp_port_locktrips > 0);
    if (--dp->dp_port_locktrips == 0) {
	dp->dp_port_lockcpu = -1;
	io_spunlockspl(dp->dp_port_lock, ospl);
    }
}

/* Low-level TX buffer routines. */

static void
du_exec_tx(dport_t *dp)
{
	register int s;
        register u_char cmd;
	register int rind, wind, index, loops;

	DU_LOG_ACT(dp, DU_ACT_EXEC_TX);

ASSERT_ALWAYS(dp == dports);
ASSERT_ALWAYS(dp->dp_index == 0);
ASSERT_ALWAYS(dp->dp_tx_buflock);

        s = LOCK_TX_BUF(dp);

        rind = dp->dp_tx_rind;
        wind = dp->dp_tx_wind;

	UNLOCK_TX_BUF(dp, s);

	loops = 0;
        while (wind != rind) {
		loops++;

                index = rind % TX_BUFSIZE;
                cmd = dp->dp_tx_buf[index].cmd;

		switch(cmd) {

		case CMD_NULL:
			break;

		case CMD_COFF:
			du_coff(dp);
			break;

		case CMD_CON:
			du_con(dp);
			break;

		case CMD_ZAP:
			du_zap(dp, (int)dp->dp_tx_buf[index].arg);
			break;
		}

		s = LOCK_TX_BUF(dp);
#ifdef DEBUG
		dp->dp_tx_buf[index].rseq = dp->dp_tx_seq++;
		dp->dp_tx_buf[index].rcpu = cpuid();
#endif
		rind = ++dp->dp_tx_rind;
		wind = dp->dp_tx_wind;

		if (EMPTY_TX_BUF(dp))
			RESET_TX_BUF(dp);

		UNLOCK_TX_BUF(dp, s);
	}
	s = LOCK_PORT(dp);
	if ((dp->dp_state & DP_TX_EMPTY) || (dp->dp_state & DP_TX_START)) {
		UNLOCK_PORT(dp, s);
		mips_du_start_tx(dp);
	} else {
		UNLOCK_PORT(dp, s);
	}
	DU_LOG_ACT(dp, DU_ACT_EXEC_TX_RET0);
}

void
du_tx_putcmd(dport_t *dp, u_char cmd, u_char arg)
{
	register int s;
	register int index;

ASSERT_ALWAYS(dp == dports);
ASSERT_ALWAYS(dp->dp_index == 0);
	DU_LOG_ACT(dp, DU_ACT_TX_PUTCMD);

	s = LOCK_TX_BUF(dp);

        if (FULL_TX_BUF(dp)) {
                dp->dp_full_tx_buf++;
		UNLOCK_TX_BUF(dp, s);
		DU_LOG_ACT(dp, DU_ACT_TX_PUTCMD_RET0);
                return;
        }

        index = dp->dp_tx_wind % TX_BUFSIZE;
        dp->dp_tx_buf[index].cmd = cmd;

	switch(cmd) {

	case CMD_COFF:
	case CMD_CON:
	case CMD_NULL:
		break;

	case CMD_ZAP:
		dp->dp_tx_buf[index].arg = arg;
		break;
	}

#ifdef DEBUG
	dp->dp_tx_buf[index].wseq = dp->dp_tx_seq++;
	dp->dp_tx_buf[index].wcpu = cpuid();
#endif
	dp->dp_tx_wind++;

	UNLOCK_TX_BUF(dp, s);
	DU_LOG_ACT(dp, DU_ACT_TX_PUTCMD_RET1);
}

/* Low-level RX buffer routines. */

static int
du_rx_buf_getchar(dport_t *dp, int *c)
{
	register int s;

ASSERT_ALWAYS(dp == dports);
ASSERT_ALWAYS(dp->dp_index == 0);
	DU_LOG_ACT(dp, DU_ACT_RX_BUF_GETCHAR);

	s = LOCK_RX_BUF(dp);
	if (EMPTY_RX_BUF(dp)) {
		RESET_RX_BUF(dp);
		UNLOCK_RX_BUF(dp, s);
		DU_LOG_ACT(dp, DU_ACT_RX_BUF_GETCHAR_RET0);
		return 0;
	}
	*c = dp->dp_rx_buf[dp->dp_rx_rind++ % RX_BUFSIZE];
	UNLOCK_RX_BUF(dp, s);

	DU_LOG_ACT_INFO(dp, DU_ACT_RX_BUF_GETCHAR_RET1, *c);
	return(1);
}

/* return 1 if we need to stop receiving because of buffer problems,
 * 0 otherwise
 */
static int
du_rx_buf_putchar(dport_t *dp, int c, int hi)
{

	register int s;
	int hiwat;

	DU_LOG_ACT_INFO(dp, DU_ACT_RX_BUF_PUTCHAR, c);

	s = LOCK_RX_BUF(dp);

	if (FULL_RX_BUF(dp)) {

	    /* The buffer is full. We need to run the low level 
	     * exclusively to get the streams code going to clear out
	     * this data. We'll use an extra flag to ensure that we
	     * only try to activate the streams code once since it will
	     * probably take a little while to get going. This flag will
	     * be cleared by the read service routine when the data has
	     * finally been cleared out.
	     */
	    if (hi && !dp->dp_rx_lointr_running) {
		RUNLOW(EXCLUSIVELY);
		dp->dp_rx_lointr_running = 1;
	    }

	    dp->dp_full_rx_buf++;
#ifdef DU_METERING
	    fullbuf++;
#endif
	    UNLOCK_RX_BUF(dp, s);
	    DU_LOG_ACT(dp, DU_ACT_RX_BUF_PUTCHAR_RET0);
	    return(1);
	}

	dp->dp_rx_buf[dp->dp_rx_wind++ % RX_BUFSIZE] = c;

	/* make low level run if buffer is almost at the HIWAT mark,
	 * so we won't have to flow control if it isn't really necessary
	 */
	if (hi && ALMOST_HIWAT_RX_BUF(dp) && !dp->dp_rx_lointr_running) {
	    RUNLOW(NOW);
#ifdef DU_METERING
	    almostfull++;
#endif
	    dp->dp_rx_lointr_running = 1;
	    UNLOCK_RX_BUF(dp, s);
	    DU_LOG_ACT(dp, DU_ACT_RX_BUF_PUTCHAR_RET1);
	    return(1);
	}

	hiwat = HIWAT_RX_BUF(dp);
	UNLOCK_RX_BUF(dp, s);

	if (hiwat) {
	    if (hi) {
		/* we must flow control *now* to prevent character loss,
		 * so run low exclusively (this can't be done from hi)
		 */
		RUNLOW(EXCLUSIVELY);
		dp->dp_rx_lointr_running = 1;
		DU_LOG_ACT(dp, DU_ACT_RX_BUF_PUTCHAR_RUNLOW);
	    }
	    else {
		dp->dp_hiwat_rx_buf++;
#ifdef DU_METERING
		iflow++;
#endif
		mips_du_iflow(dp, SEND_XOFF, !IGNORE_IXOFF);
	    }
	}

	DU_LOG_ACT(dp, DU_ACT_RX_BUF_PUTCHAR_RET2);
	return(0);
}


#ifdef DEBUG
static int du_max_inchars = 0;
static int du_cur_inchars = 0;
#endif /* DEBUG */

static int
du_get_inchars(dport_t *dp, int hi)
{
	register int c, cnt;
	register int s;
	int is_scndcport = dp->dp_index == SCNDCPORT;
	u_char sr;
	
	DU_LOG_ACT(dp, DU_ACT_GET_INCHARS);

	c = 0;
	cnt = 0;

	/* must block hi level between mips_du_getchar and
	 * du_rx_buf_putchar else a low level intr may get a char
	 * from the hardware, then a hi level intr races in, gets a
	 * char from the hardware, stuffs it in the buffer, then the low
	 * level continues, stuffs its char in the buffer, and the order
	 * of the two chars has been reversed.
	 */
	s = SET_SPL();
	while ((c = mips_du_getchar(dp, 0)) != -1) {
		SYSINFO.rawch++;

		if (kdebug && is_scndcport &&
		    (c & 0xff) == DEBUG_CHAR) {
			debug("ring");
			continue;
		}
#ifdef DEBUG
		if (dp->dp_porthandler) {
			sr = c >> 8;

    			if (sr)
				cmn_err(CE_WARN, "Port %d, sr:0x%x\n",
				dp->dp_index, sr);
			if ((*dp->dp_porthandler)(c & 0xff)) {
				continue;
			}
		}
#else
		if (hi == 0) /* XXX when is this porthandler used anyway? */
		    if (dp->dp_porthandler && (*dp->dp_porthandler)(c & 0xff))
			continue;
#endif /* DEBUG */

		if (du_rx_buf_putchar(dp, c, hi)) {
		    cnt++;
		    break;
		}
		splx(s);
		s = SET_SPL();

		cnt++;
	}
	splx(s);
#ifdef DEBUG
	du_cur_inchars = cnt;
	if (cnt > du_max_inchars)
		du_max_inchars = cnt;
#endif /* DEBUG */
#ifdef DU_METERING
	totalchars += cnt;
#endif
	DU_LOG_ACT(dp, DU_ACT_GET_INCHARS_RET0);
	return(cnt);
}


/*
 * gobble any waiting input, should only be called when safe from interrupts
 */
static void
du_rclr(dport_t *dp)
{
	int c;
	int is_scndcport = dp->dp_index == SCNDCPORT;

	DU_LOG_ACT(dp, DU_ACT_RCLR);

	CLEAR_RX_BUF(dp);
	while ((c = mips_du_getchar(dp, 1)) != -1) {
		if (kdebug && is_scndcport &&
		    (c & 0xff) == DEBUG_CHAR)
			debug("ring");
	}
	DU_LOG_ACT(dp, DU_ACT_RCLR_RET0);
}



/*
 * shut down a port, should only be called when safe from interrupts
 */
static void
du_zap(dport_t *dp, int hup)
{
	int s;

	DU_LOG_ACT(dp, DU_ACT_ZAP);

	du_flushw(dp);			/* forget pending output */

	mips_du_stop_rx(dp, hup);
	s = LOCK_PORT(dp);
	mips_du_stop_tx(dp);
	UNLOCK_PORT(dp, s);

	du_rclr(dp);
	DU_LOG_ACT(dp, DU_ACT_ZAP_RET0);
}



/*
 * finish a delay
 */
static void
du_delay(dport_t *dp)
{
	int s;

	DU_LOG_ACT(dp, DU_ACT_DELAY);

	s = LOCK_PORT(dp);
	if ((dp->dp_state & (DP_BREAK | DP_BREAK_QUIET)) != DP_BREAK) {
		dp->dp_state &= ~(DP_TIMEOUT | DP_BREAK | DP_BREAK_QUIET);
		UNLOCK_PORT(dp, s);
		mips_du_start_tx(dp);	/* resume output */

	} else {			/* unless need to quiet break */
		mips_du_break(dp, 0);
		dp->dp_state |= DP_BREAK_QUIET;
		UNLOCK_PORT(dp, s);
		dp->dp_tid = STREAMS_TIMEOUT((strtimeoutfunc_t)du_delay, dp,
						HZ/20);
	}
	DU_LOG_ACT(dp, DU_ACT_DELAY_RET0);
}





/*
 * flush output, should only be called when safe from interrupts
 */
static void
du_flushw(dport_t *dp)
{
	int s;

	DU_LOG_ACT(dp, DU_ACT_FLUSHW);

	s = LOCK_PORT(dp);
	if ((dp->dp_state & (DP_TIMEOUT | DP_BREAK | DP_BREAK_QUIET))
	    == DP_TIMEOUT) {
		dp->dp_state &= ~DP_TIMEOUT;
		UNLOCK_PORT(dp, s);
		untimeout(dp->dp_tid);		/* forget stray timeout */
	} else {
		UNLOCK_PORT(dp, s);
	}

	freemsg(dp->dp_wbp);
	dp->dp_wbp = NULL;
	DU_LOG_ACT(dp, DU_ACT_FLUSHW_RET0);
}



/*
 * flush input, should only be called when safe from interrupts
 */
static void
du_flushr(dport_t *dp)
{

	DU_LOG_ACT(dp, DU_ACT_FLUSHR);

	CLEAR_RX_BUF(dp);

	if (dp->dp_cur_rbp) {
		freemsg(dp->dp_cur_rbp);
		dp->dp_cur_rbp = NULL;
	}
	if (dp->dp_nxt_rbp) {
		freemsg(dp->dp_nxt_rbp);
		dp->dp_nxt_rbp = NULL;
	}

	if (dp->dp_rq)
		qenable(dp->dp_rq);	/* turn input back on */
	DU_LOG_ACT(dp, DU_ACT_FLUSHR_RET0);
}



/*
 * save a message on our write queue and start the output interrupt.
 */
static void
du_save(dport_t *dp, queue_t *wq, mblk_t *bp)
{
	DU_LOG_ACT(dp, DU_ACT_SAVE);
	putq(wq, bp);		/* save the message */
	mips_du_start_tx(dp);	/* start TX */
	DU_LOG_ACT(dp, DU_ACT_SAVE_RET0);
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
	int s;
	extern int baud_to_cbaud(speed_t);

	DU_LOG_ACT(dp, DU_ACT_TCSET);

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
	    DU_LOG_ACT(dp, DU_ACT_TCSET_RET0);
	    return;
	}
	s = LOCK_PORT(dp);

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

	mips_du_config(dp, cflag, ospeed, tp);
	tp->c_cflag = dp->dp_cflag; /* tell line discipline the results */
	tp->c_ospeed = dp->dp_ospeed;
	tp->c_ispeed = dp->dp_ispeed;

	iocp->ioc_count = 0;
	bp->b_datap->db_type = M_IOCACK;
	DU_LOG_ACT(dp, DU_ACT_TCSET_RET0);
}



/*
 * Interrupt-process an IOCTL. this function processes those IOCTLs that
 * must be done by the output interrupt.
 *
 * Called with the port locked.
 */
static void
du_i_ioctl(dport_t *dp, mblk_t *bp, int *sp)
{
	struct iocblk *iocp;
	int s;

	DU_LOG_ACT(dp, DU_ACT_I_IOCTL);

	iocp = (struct iocblk *)bp->b_rptr;

	switch (iocp->ioc_cmd) {
	case TCSBRK:
		if (!*(int *)bp->b_cont->b_rptr) {
			dp->dp_state |= DP_TIMEOUT | DP_BREAK;
			mips_du_break(dp, 1);
			dp->dp_tid = STREAMS_TIMEOUT((strtimeoutfunc_t)du_delay,
							(caddr_t)dp, HZ/4);
		} else {	/* tcdrain */
			/*
			 * Wait until the transmitter is empty.
			 */
			while (posix_tty_default &&
			       !mips_du_allsent_nolock(dp)) {
				UNLOCK_PORT(dp, *sp);
				*sp = LOCK_PORT(dp);
			}
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
		DU_LOG_ACT(dp, DU_ACT_I_IOCTL_RET0);
		return;

	case SIOC_ITIMER:
		local_rsrv_duration = *(int *)bp->b_cont->b_rptr;
		DU_LOG_ACT_INFO(dp, DU_ACT_I_IOCTL_ITIMER,
				local_rsrv_duration);
		if (local_rsrv_duration < 0)
		    local_rsrv_duration = 0;
		else if (local_rsrv_duration > 10)
		    local_rsrv_duration = 10;
		iocp->ioc_count = 0;
		bp->b_datap->db_type = M_IOCACK;
		break;


	default:
		ASSERT_ALWAYS(0);
	}

	putnext(dp->dp_rq, bp);
	DU_LOG_ACT(dp, DU_ACT_I_IOCTL_RET1);
}


int spurious_kbdms_intr = 0;
int spurious_1_2_intr = 0;

static void
enable_intrs(void)
{
    int duartnum;
    nasid_t nasid;

    for(duartnum = duartbase; duartnum < maxduart; duartnum += 4) {
	ASSERT_ALWAYS(duartnum != 0);

#if SABLE
	nasid = COMPACT_TO_NASID_NODEID(cputocnode(master_procid));

	/* XXX - Can't do this.  Hook it up via the nice interfaces. */
	REMOTE_HUB_S(nasid, PI_INT_MASK0_A, REMOTE_HUB_L(nasid, PI_INT_MASK0_A)
			 |= UART_INTR);
#else
	<<< Bomb! >>>
#endif
    }
}

static toid_t toid;
static void du_lointr(void*);

/*
 * Dynamically configure the port_map and dports arrays by probing
 * for the EPC's in the system.  Currently, this function only supports
 * one working IO4.
 */

#define UNUSED 255

#if 0 /* I'll get this working later */
int
duedtinit(struct edt *ep)
{
}
#endif

void
hub_du_portinit(void)
{
    int i, s;			/* Index registers */
    int slot;			/* Slot of IO4 for given unit # */
    int adap;			/* IO adapter of Io4 for given unit # */
    int unit;			/* Unit currently being configured */
    int numspaces;
    unsigned padap;             /* index of current padapter */
    __psunsigned_t d_base;       /* Base address for EPC DUART values */
    dport_t *dp;		/* Pointer to current dport */
    unsigned base;		/* Current dports index */
    __psunsigned_t swin;	/* Swin value */
    int cpudest;		/* destination cpu for interrupts */

    extern epc_xadap2slot(int, int*, int*);

    /* XXX - Uhhh...  is this right? */
    duartbase = 0;
    maxduart = base = 2; 
    ASSERT_ALWAYS(base > 0);
 
    max_kbdms_port = 0;

    cpudest = master_procid;

#if 0 /* I'll get this working later */
    for(i = 0; i < nedt; i++) {
	if (edt[i].e_init == duedtinit) {
	    if (edt[i].v_setcpuintr)
		cpudest = edt[i].v_cpuintr;
	    break;
	}
    }
#endif

    maxport = 1;
}

/*
 * initialize the DUART ports. this is done to allow debugging soon after
 * booting
 */

void
du_init(void)
{
	dport_t *dp;
	static duinit_flag = 0;
	char *env;
	int s;
	u_char du_cntrl;	/* XXXrs - for RD_CNTRL */
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	int port;

	if (duinit_flag++)		/* do it at most once */
		return;

	master_nasid = COMPACT_TO_NASID_NODEID(cputocnode(master_procid));

	/* figure out number of ports to work with */
	maxport = 1;
	mouseports = 0;
	gfxkeyports = 0;
	local_rsrv_duration = duart_rsrv_duration;
	if (local_rsrv_duration < 0)
	    local_rsrv_duration = 0;
	else if (local_rsrv_duration > 10)
	    local_rsrv_duration = 10;

	for (dp = dports; dp < &dports[maxport]; dp++) {
#ifndef SABLE
		if (!dp->dp_cntrl)
			continue;
#endif

ASSERT_ALWAYS(dp == dports);
ASSERT_ALWAYS(dp->dp_index == 0);
		INIT_RX_BUFLOCK(dp);
		dp->dp_rx_rind = dp->dp_rx_wind = 0;
		INIT_TX_BUFLOCK(dp);
		dp->dp_tx_rind = dp->dp_tx_wind = 0;
#ifdef DEBUG
		dp->dp_tx_seq = 1;
		INIT_DU_ACTLOG_LOCK(dp);
		dp->dp_al_wind = 0;
		bzero((void*)dp->dp_actlog, sizeof(dp->dp_actlog));
#endif /* DEBUG */
        }

	/*
	 * this should be the only place we reset the chips. enable global
	 * interrupt too
	 */
	for (dp = dports; dp < &dports[maxport]; dp += 2) {
		/* If a port wasn't configured, ignore it */
#ifndef SABLE
		if (!dp->dp_cntrl)
			continue;
#endif
		INITLOCK_PORT(dp);
	}
}

struct dport *
du_minordev_to_dp(dev_t minordev)
{
    return(&dports[PORT(minordev)]);
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
	short *rxbuf;

	if (sflag) 			/* only a simple stream driver */
	    return(ENXIO);

	port = PORT(*devp);
	if (port > maxport) 		/* fail if bad device # */
	    return(ENXIO);

	dp = &dports[port];
ASSERT_ALWAYS(dp == dports);
ASSERT_ALWAYS(dp->dp_index == 0);
	DU_LOG_ACT_INFO(dp, DU_ACT_OPEN, *devp);
#ifndef SABLE
	if (!dp->dp_cntrl) {
	    DU_LOG_ACT(dp, DU_ACT_OPEN_RET0);
	    return(ENXIO);
	}
#endif

	s = LOCK_PORT(dp);
	if (!(dp->dp_state & (DP_ISOPEN | DP_WOPEN))) {	/* on the 1st open */
		tcflag_t cflag;
		speed_t ospeed;
		queue_t *wq = WR(rq);

		if ((rxbuf = (short*)kmem_alloc(RXBUFSIZE, KM_SLEEP)) == 0) {
			UNLOCK_PORT(dp, s);
			return(ENOMEM);
		}
		dp->dp_rx_buf = rxbuf;

		dp->dp_state |= DP_WOPEN;

		cflag = def_stty_ld.st_cflag;
		ospeed = def_stty_ld.st_ospeed;
		if (dp->dp_index == SCNDCPORT)
			ospeed = def_console_baud;

		dp->dp_state &= ~(DP_TXSTOP | DP_LIT | DP_BLOCK | DP_TX_TXON |
			          DP_TX_TXOFF | DP_RS422 | DP_MODEM |
				  DP_EXTCLK);

		if (MODEM(*devp)) {
			DU_LOG_ACT(dp, DU_ACT_OPEN_MODEM);
			dp->dp_state |= DP_MODEM;
			cflag &= ~CLOCAL;
			if (FLOW_MODEM(*devp)) {
			    DU_LOG_ACT(dp, DU_ACT_OPEN_FLOW_MODEM);
			    cflag |= CNEW_RTSCTS;
			}
		}
		UNLOCK_PORT(dp, s);


		mips_du_config(dp, cflag, ospeed, &def_stty_ld.st_termio);

		if (!mips_du_start_rx(dp)		/* wait for carrier */
		    && !(dp->dp_cflag & CLOCAL)
		    && !(flag & (FNONBLK | FNDELAY))) {

			int done;	
			do {
				if (sleep((caddr_t)dp, STIPRI)) {
					s = LOCK_PORT(dp);
					dp->dp_state &= ~(DP_WOPEN | DP_ISOPEN);
					UNLOCK_PORT(dp, s);
					du_zap(dp, HUPCL);
					kmem_free(rxbuf, RXBUFSIZE);
					DU_LOG_ACT(dp, DU_ACT_OPEN_RET1);
					return(EINTR);
				}
				s = LOCK_PORT(dp);
				done = (dp->dp_state & DP_DCD);
				UNLOCK_PORT(dp, s);
			} while (!done);
		}

		rq->q_ptr = (caddr_t)dp;	/* connect device to stream */
		wq->q_ptr = (caddr_t)dp;
		s = LOCK_PORT(dp);
		dp->dp_porthandler = NULL;	/* owner gains control */
		dp->dp_wq = wq;
		dp->dp_rq = rq;
		UNLOCK_PORT(dp, s);
		dp->dp_cflag |= CREAD;

		du_rclr(dp);		/* discard input */

		error = strdrv_push(rq, "stty_ld", devp, crp);
		if (error) {
			s = LOCK_PORT(dp);
			dp->dp_state &= ~(DP_ISOPEN | DP_WOPEN);
			dp->dp_rq = NULL;
			dp->dp_wq = NULL;
			UNLOCK_PORT(dp, s);
			du_zap(dp, HUPCL);
			kmem_free(rxbuf, RXBUFSIZE);
			DU_LOG_ACT(dp, DU_ACT_OPEN_RET2);
			return(error);
		}
		s = LOCK_PORT(dp);
		dp->dp_state |= DP_ISOPEN;
		dp->dp_state &= ~DP_WOPEN;
		UNLOCK_PORT(dp, s);
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
		    DU_LOG_ACT(dp, DU_ACT_OPEN_RET3);
		    return(EBUSY);
		}
	}

	DU_LOG_ACT(dp, DU_ACT_OPEN_RET4);
	return(0);		/* return successfully */
}



/*
 * close a port
 */
static int
du_close(queue_t *rq, int flag, struct cred *crp)
{
	dport_t *dp = (dport_t *)rq->q_ptr;
	int s;
	short *rxbuf;

	DU_LOG_ACT(dp, DU_ACT_CLOSE);
	
	s = LOCK_PORT(dp);
	if (dp->dp_state & DP_SE_PENDING) {
		dp->dp_state &= ~DP_SE_PENDING;
		UNLOCK_PORT(dp, s);
		str_unbcall(rq);
	} else {
		UNLOCK_PORT(dp, s);
	}

	du_flushr(dp);
	du_flushw(dp);
	s = LOCK_PORT(dp);
	dp->dp_rq = NULL;
	dp->dp_wq = NULL;
	UNLOCK_PORT(dp, s);
	du_zap(dp, dp->dp_cflag & HUPCL);
	s = LOCK_PORT(dp);
	dp->dp_state &= ~(DP_ISOPEN | DP_WOPEN);
	rxbuf = dp->dp_rx_buf;
	dp->dp_rx_buf = 0;
	UNLOCK_PORT(dp, s);
	kmem_free(rxbuf, RXBUFSIZE);
	DU_LOG_ACT(dp, DU_ACT_CLOSE_RET0);
	return 0;
}

/*
 * send a bunch of 1 or more characters up the stream
 */
static void
du_rsrv(queue_t *rq)
{
	int s, s1;
	mblk_t *bp;
	dport_t *dp = (dport_t *)rq->q_ptr;
	register int more;
	int c, size, i;

	DU_LOG_ACT(dp, DU_ACT_RSRV);

	if (!canput(rq->q_next)) {	/* quit if upstream congested */
		noenable(rq);
		dp->dp_rsrv_blocked++;
		dp->dp_rsrv_blk_consec++;
		if (dp->dp_rsrv_blk_consec > dp->dp_rsrv_blk_consec_max)
			dp->dp_rsrv_blk_consec_max = dp->dp_rsrv_blk_consec;
		DU_LOG_ACT(dp, DU_ACT_RSRV_RET0);
		return;
	}
	enableok(rq);
	dp->dp_rsrv_blk_consec = 0;

	s = LOCK_PORT(dp);
	if (dp->dp_state & DP_SE_PENDING) {
		dp->dp_state &= ~DP_SE_PENDING;
		UNLOCK_PORT(dp, s);
		str_unbcall(rq);
	} else {
		UNLOCK_PORT(dp, s);
	}

	/* get input characters from low level */
	more = 0;
	while(du_rx_buf_getchar(dp, &c) && !(more = du_process_inchar(dp, c)))
		;

	/* indicate to du_rx_buf_putchar() that the rx buffer has been
	 * emptied out
	 */
	dp->dp_rx_lointr_running = 0;

	/* 
	 * Send any current buffer upstream, unless empty.
	 */
	bp = dp->dp_cur_rbp;
	if (bp && bp->b_wptr > bp->b_rptr) {
#ifdef DEBUG
		size = bp->b_wptr - bp->b_rptr;
		for (i = 0, c = 1; i < MAX_CHARCNT-1; i++, c *= 2) {
			if (size <= c) {
				dp->dp_rx_charcnt[i]++;
				break;
			}
		}
		if (i == MAX_CHARCNT-1)
			dp->dp_rx_charcnt[i]++;
		if (size > dp->dp_rx_max_charcnt)
			dp->dp_rx_max_charcnt = size;
#endif /* DEBUG */
		dp->dp_cur_rbp = dp->dp_nxt_rbp;
		dp->dp_nxt_rbp = NULL;
		putnext(rq, bp);	/* send the message */
		DU_LOG_ACT(dp, DU_ACT_RSRV_PUTNEXT);
	}

	mips_du_iflow(dp, SEND_XON, !IGNORE_IXOFF);

	if (dp->dp_cur_rbp == NULL || dp->dp_nxt_rbp == NULL)
		(void)du_getbp(dp, BPRI_LO);

	if (more)
		qenable(dp->dp_rq);

	DU_LOG_ACT(dp, DU_ACT_RSRV_RET1);
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
	int s, s1;
	int i;

	s1 = SET_SPL();
	DU_LOG_ACT(dp, DU_ACT_WPUT);
	switch (bp->b_datap->db_type) {

	case M_FLUSH:	/* XXX may not want to restart output since flow
			   control may be messed up */
		c = *bp->b_rptr;
		sdrv_flush(wq, bp);
		if (c & FLUSHW) {
			du_flushw(dp);
			s = LOCK_PORT(dp);
			dp->dp_state &= ~DP_TXSTOP;
			UNLOCK_PORT(dp, s);
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
			case 0:			/* stop output */
				s = LOCK_PORT(dp);
				dp->dp_state |= DP_TXSTOP;
				mips_du_stop_tx(dp);
				UNLOCK_PORT(dp, s);
				break;
			case 1:			/* resume output */
				s = LOCK_PORT(dp);
				dp->dp_state &= ~DP_TXSTOP;
				UNLOCK_PORT(dp, s);
				mips_du_start_tx(dp);
				break;
			case 2:
				mips_du_iflow(dp, SEND_XOFF, IGNORE_IXOFF);
				break;
			case 3:
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

		case TIOCMGET:
			/* DTR is true when open and CBAUD != 0 */
			s = LOCK_PORT(dp);
			i = (dp->dp_ospeed) ? TIOCM_DTR : 0;
			if ((dp->dp_wr5 & WR5_RTS) ^ RTS_XOR_MASK)
				i |= TIOCM_RTS;
			c = RD_RR0(dp->dp_cntrl);
			UNLOCK_PORT(dp, s);
			if ((c & RR0_DCD) ^ DCD_XOR_MASK)
				i |= TIOCM_CD;
			if ((c & RR0_CTS) ^ CTS_XOR_MASK)
				i |= TIOCM_CTS;
			*(int*)(bp->b_cont->b_rptr) = i;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			break;

		case TCGETA: /* XXXrs - Don't wait until output? */
			ASSERT_ALWAYS(iocp->ioc_count == sizeof(struct termio));
			tcgeta(dp->dp_wq,bp, &dp->dp_termio);
			break;

		case TCSETA: /* XXXrs - Don't wait until output? */
			ASSERT_ALWAYS(iocp->ioc_count == sizeof(struct termio));
			(void)du_tcset(dp,bp);
			qreply(wq,bp);
			break;

		case SIOC_ITIMER:	/* set input timer */
		case TCSBRK:	/* send BREAK */
		case TCSETAF:	/* drain output, flush input, set parameters */
		case TCSETAW:	/* drain output, set paramaters */
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

	DU_LOG_ACT(dp, DU_ACT_WPUT_RET0);
	SPLX(s1);
	return 0;
}

#if DEBUG || SABLE
#include <sys/i8251uart.h>

#define HUBUART_INPUT_ENABLE	REMOTE_HUB_S(master_nasid, MD_UREG0_0, I8251_TXENB|I8251_RXENB)
#define HUBUART_INPUT_DISABLE	REMOTE_HUB_S(master_nasid, MD_UREG0_0, I8251_TXENB)
#define HUBUART_INPUT_RDY	(REMOTE_HUB_L(master_nasid, MD_UREG0_0) & I8251_RXRDY)
#define HUBUART_INPUT		((char)REMOTE_HUB_L(master_nasid, MD_UREG0_1))
#define HUBUART_OUTPUT_RDY	(REMOTE_HUB_L(master_nasid, MD_UREG0_0) & I8251_TXRDY)


/*
 * The PROM will have initialized all HUB UARTs properly
 * by the time we get here.
 */

/*
 * Print a single character to the caller's CC uart.
 * SPINS until the hardware is ready to accept a new character.
 */
void
hubuart_output(char c)
{
	int timeleft = 100000;

	if (c == '\n')
		hubuart_output('\r');

	while (timeleft-- &&
		!(REMOTE_HUB_L(master_nasid, MD_UREG0_0) & I8251_TXRDY))
		/* spin */ ;

	REMOTE_HUB_S(master_nasid, MD_UREG0_1, c);
}

/*
 * put string to caller's hubuart.  Machines in the field will 
 * not have anything attached to this UART, but putstr to this 
 * port is convenient during initial bringup of SN0 systems.  
 * It's one step above the LED display!
 */
void
hubuart_putstr(char *string)
{
	char c;
	char *bufptr = string;
#if defined (NOUART_SABLE)
	extern print_trap(char *);

	print_trap(string);
	return;
#endif
/* NOTREACHED */
	while ((c = *bufptr++) != '\0')
		hubuart_output(c);
}


#endif

#if SABLE
void
hubuartintr(struct eframe_s *ep)
{
    int state;

	/* Turn the interrupt back off. */
	REMOTE_HUB_CLR_INTR(master_nasid, UART_INTR);

#if !IP27_NO_HUBUART_INT

	hubuart_handle_intr(0);

#endif /* !IP27_NO_HUBUART_INT */


	return;
}

void
hubuart_service()
{
  	/* NOTE: The only reason we disable duart input and enable it
	 * are because the SABLE simulation slows down by a factor of 5
	 * when input is allowed (I think it does a "select" on every
	 * instruction simuoated).  So only enable it when service routine
	 * is called.
	 */
  	HUBUART_INPUT_ENABLE;		/* Enable receiver, SABLE slows down*/
#if IP27_NO_HUBUART_INT
	/* If not using HUBUART interrupts (i.e. masked off in SR) then we
	 * need to poll the uart for characters.
	 */
	if (HUBUART_INPUT_RDY) {
		hubuart_handle_intr(0);
	}
#endif
	HUBUART_INPUT_DISABLE;		/* Disable receive, SABLE speeds up */
}

/*
 * Handle an interrupt from the CC Uart
 * Returns:
 *	0 if nothing to do on the specified duart
 *	1 if something was done
 *
 */

int
hubuart_handle_intr(int duartnum)
{
	u_char du_cntrl;	/* XXXrs - for RD_CNTRL */
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */

	dport_t *dp0 = &dports[duartnum];

	int i;
	int dp0txsched;
	int dp0rxsched;
	u_char rr0;
	u_char rr3;

	queue_t *rq;
	u_int state;

	int s, s1, hwstate;

	s = SET_SPL();

	dp0txsched = 0;
	dp0rxsched = 0;

	s1 = LOCK_PORT(dp0);

	hwstate = REMOTE_HUB_L(master_nasid, MD_UREG0_0);

	/* receive character/error interrupt */
	if (hwstate & I8251_RXRDY) {
		SYSINFO.rcvint++;
		if ((i = du_rx(dp0, 0)) == SCHED_RX)
			dp0rxsched++;
		else if (i == SCHED_TX)
			dp0txsched++;
	}

	/* transmit buffer empty interrupt */

	if (hwstate & I8251_TXRDY) {
		/* Actually, this is transmit buffer ready.  But SABLE
		 * does not support the buffer empty interrupt, so
		 * we'll just pretend we received buffer empty.
		 */
		SYSINFO.xmtint++;
		mips_du_stop_tx(dp0);	/* we'll restart when ready to send */
		dp0->dp_state |= DP_TX_EMPTY|DP_TX_START;
		dp0txsched++;
	}

	state = dp0->dp_state;
	rq = dp0->dp_rq;

	if (dp0txsched) {
		streams_interrupt((strintrfunc_t)du_exec_tx, dp0, NULL, NULL);
	}
	if (dp0rxsched && state & DP_ISOPEN && rq && canenable(rq)) {
		streams_interrupt((strintrfunc_t)qenable, rq, NULL, NULL);
	}

	UNLOCK_PORT(dp0, s1);

	SPLX(s);

	return(1);
}
#endif	/* SABLE */

#ifndef SABLE
/*
 * Handle an interrupt from the specified duart.
 * Returns:
 *	0 if nothing to do on the specified duart
 *	1 if something was done
 */

static int
du_handle_intr(int duartnum, int hi)
{
	u_char du_cntrl;	/* XXXrs - for RD_CNTRL */
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */

	dport_t *dp0 = &dports[duartnum];	/* port B on DUART */
	dport_t *dp1 = &dports[duartnum+1];	/* port A on DUART */

	int i;
	int dp0txsched, dp1txsched;
	int dp0rxsched, dp1rxsched;
	u_char rr0;
	u_char rr3;
#ifdef DEBUG
	int logdp0 = 0, logdp1 = 0;
#endif

	queue_t *rq;
	u_int state;

	int s, s1;

	/* If dp_cntrl is zero, it is likely that this port isn't
	 * properly initialized, so we just ignore it.
	 */
	if (!dp0->dp_cntrl || !dp1->dp_cntrl)
	    return 0;
    
	dp0txsched = dp1txsched = 0;
	dp0rxsched = dp1rxsched = 0;

	s1 = LOCK_PORT(dp1);
	rr3 = RD_CNTRL(dp1->dp_cntrl, RR3);

	/* tack on any saved flags from the hi intr. rr3 may be cleared
	 * between reads, and IP flags read at the hi level may be needed
	 * by the lo level. Hence the hi level saves some of its IP flags
	 * and passes them down to the lo level.
	 */
	if (!hi) {
	    rr3 |= dp1->dp_last_rr3;
	    dp1->dp_last_rr3 = 0;
	}

	UNLOCK_PORT(dp1, s1);

	if (!rr3)
	    return(0);

#if DEBUG
	if (rr3 & (RR3_CHNA_RX_IP|RR3_CHNA_TX_IP|RR3_CHNA_EXT_IP)) {
	    DU_LOG_ACT_INFO(dp1, hi ? DU_ACT_HANDLE_INTR_HI :
			    DU_ACT_HANDLE_INTR_LO, rr3);
	    logdp1 = 1;
	}
	if (rr3 & (RR3_CHNB_RX_IP|RR3_CHNB_TX_IP|RR3_CHNB_EXT_IP)) {
	    DU_LOG_ACT_INFO(dp0, hi ? DU_ACT_HANDLE_INTR_HI :
			    DU_ACT_HANDLE_INTR_LO, rr3);
	    logdp0 = 1;
	}
#endif

	/* receive character/error interrupt */
	if (rr3 & RR3_CHNB_RX_IP) {
		SYSINFO.rcvint++;

		if ((i = du_rx(dp0, hi)) == SCHED_RX) {
		    if (hi)
			/* got some chars, eventually we'll need to
			 * send them upstream
			 */
			RUNLOW(EVENTUALLY);
		    dp0rxsched++;
		}
		else if (i == SCHED_TX)
			dp0txsched++;
	}
	else if (!hi && HIWAT_RX_BUF(dp0) && !(dp0->dp_state & DP_BLOCK)) {
#ifdef DU_METERING
	    iflow_deferred++;
#endif
	    mips_du_iflow(dp0, SEND_XOFF, !IGNORE_IXOFF);
	}
	    

	if (rr3 & RR3_CHNA_RX_IP) {
		SYSINFO.rcvint++;

		if ((i = du_rx(dp1, hi)) == SCHED_RX) {
		    if (hi)
			RUNLOW(EVENTUALLY);
		    dp1rxsched++;
		}
		else if (i == SCHED_TX)
			dp1txsched++;
	}
	else if (!hi && HIWAT_RX_BUF(dp1) && !(dp1->dp_state & DP_BLOCK)) {
#ifdef DU_METERING
	    iflow_deferred++;
#endif
	    mips_du_iflow(dp1, SEND_XOFF, !IGNORE_IXOFF);
	}

	if (rr3 & RR3_CHNB_TX_IP) {
	    /* if we can service the write request without any
	     * streams overhead, do it now
	     */
	    if (du_tx(dp0, 1))
		/* if we sent any characters, clear the TX_IP flag since
		 * the output fifo is no longer empty and it would be
		 * an error to behave as if we had just received a TX intr.
		 * Instead we'll just wait for the next TX intr when the chars
		 * we just sent have drained
		 */
		rr3 &= (~RR3_CHNB_TX_IP);
	}	

	if (rr3 & RR3_CHNA_TX_IP) {
	    if (du_tx(dp1, 1))
		rr3 &= (~RR3_CHNA_TX_IP);
	}

	if (hi) {
	    /* store any holdover IP bits for the low level */
	    dp1->dp_last_rr3 |= rr3;

	    /* if there is still an unsatisfied TX condition or any
	     * exceptions, the low level is required immediately
	     */
	    if (rr3 & (RR3_CHNA_EXT_IP | RR3_CHNB_EXT_IP |
		       RR3_CHNA_TX_IP | RR3_CHNB_TX_IP))
		RUNLOW(EXCLUSIVELY);

	    /* no need to spl since we're already hi. Hold over the sched
	     * flags for the low level
	     */
	    dp0->dp_rxsched |= dp0rxsched;
	    dp1->dp_rxsched |= dp1rxsched;
#ifdef DEBUG
	    if (logdp0)
		DU_LOG_ACT(dp0, DU_ACT_HANDLE_INTR_RET0);
	    if (logdp1)
		DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_RET0);
#endif
	    return(1);
	}

	s = SET_SPL();

	/* merge in the sched flags from the high level */
	dp0rxsched |= dp0->dp_rxsched;
	dp1rxsched |= dp1->dp_rxsched;
	dp0->dp_rxsched = 0;
	dp1->dp_rxsched = 0;

	splx(s);

	/* notice if we got any chars this intr so we will know to
	 * reenable the heartbeat timeout later
	 */
	if (dp0rxsched) {
	    got_chars_this_intr = 1;
	    dp0->dp_saw_rx = 1;
	}
	if (dp1rxsched) {
	    got_chars_this_intr = 1;
	    dp1->dp_saw_rx = 1;
	}

	/* external status interrupt */
	if (rr3 & RR3_CHNB_EXT_IP) {
#ifdef DU_METERING
	    extint++;
#endif
		s1 = LOCK_PORT(dp0);
		rr0 = RD_RR0(dp0->dp_cntrl);

		/* handle CTS interrupt only if we care */
		if (dp0->dp_wr15 & WR15_CTS_IE) {
			if (dp0->dp_state & DP_CTS
			    && !(DU_CTS_ASSERTED(rr0))) {
				SYSINFO.mdmint++;
				dp0->dp_state &= ~DP_CTS;
				DU_LOG_ACT(dp0, DU_ACT_HANDLE_INTR_CTS_OFF);
			} else if (!(dp0->dp_state & DP_CTS)
				   && DU_CTS_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp0->dp_state |= DP_CTS;
				DU_LOG_ACT(dp0, DU_ACT_HANDLE_INTR_CTS_ON);
					dp0->dp_state |= DP_TX_START;
					dp0txsched++;
			}
		}

		/* handle DCD interrupt only if we care */
		if (dp0->dp_wr15 & WR15_DCD_IE) {
			if (dp0->dp_state & DP_DCD
			    && !DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp0->dp_state &= ~DP_DCD;
				DU_LOG_ACT(dp0, DU_ACT_HANDLE_INTR_DCD_OFF);
				UNLOCK_PORT(dp0, s1);
				du_tx_putcmd(dp0, CMD_COFF, 0);
				dp0txsched++;
				s1 = LOCK_PORT(dp0);
			} else if (!(dp0->dp_state & DP_DCD)
				   && DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp0->dp_state |= DP_DCD;
				DU_LOG_ACT(dp0, DU_ACT_HANDLE_INTR_DCD_ON);
					UNLOCK_PORT(dp0, s1);
					du_tx_putcmd(dp0, CMD_CON, 0);
					dp0txsched++;
					s1 = LOCK_PORT(dp0);
			}
		}

		/* handle BREAK interrupt */
		if (rr0 & RR0_BRK)
			dp0->dp_state |= DP_BREAK_ON;
		else {
			dp0->dp_state &= ~DP_BREAK_ON;
			if (kdebug && kdebugbreak &&
			    dp0->dp_index == SCNDCPORT) {
				UNLOCK_PORT(dp0, s1);
				debug("ring");
				s1 = LOCK_PORT(dp0);
			}
		}

		/* reset latches */
		WR_WR0(dp0->dp_cntrl, WR0_RST_EXT_INT)
		UNLOCK_PORT(dp0, s1);
	}

	/* external status interrupt */
	if (rr3 & RR3_CHNA_EXT_IP) {
#ifdef DU_METERING
	    extint++;
#endif

		DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_ESTAT_INTR);

		s1 = LOCK_PORT(dp1);
		rr0 = RD_RR0(dp1->dp_cntrl);

		/* handle CTS interrupt only if we care */
		if (dp1->dp_wr15 & WR15_CTS_IE) {
			if (dp1->dp_state & DP_CTS
			    && !DU_CTS_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state &= ~DP_CTS;
				DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_CTS_OFF);
			} else if (!(dp1->dp_state & DP_CTS)
				   && DU_CTS_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state |= DP_CTS;
				DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_CTS_ON);
				dp1->dp_state |= DP_TX_START;
				dp1txsched++;
			}
		}

		/* handle DCD interrupt only if we care */
		if (dp1->dp_wr15 & WR15_DCD_IE) {
			if (dp1->dp_state & DP_DCD
			    && !DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state &= ~DP_DCD;
				DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_DCD_OFF);
				UNLOCK_PORT(dp1, s1);
				du_tx_putcmd(dp1, CMD_COFF, 0);
				dp1txsched++;
				s1 = LOCK_PORT(dp1);
			} else if (!(dp1->dp_state & DP_DCD)
				   && DU_DCD_ASSERTED(rr0)) {
				SYSINFO.mdmint++;
				dp1->dp_state |= DP_DCD;
				DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_DCD_ON);
				UNLOCK_PORT(dp1, s1);
				du_tx_putcmd(dp1, CMD_CON, 0);
				dp1txsched++;
				s1 = LOCK_PORT(dp1);
			}
		}

		/* handle BREAK interrupt */
		if (rr0 & RR0_BRK)
			dp1->dp_state |= DP_BREAK_ON;
		else
			dp1->dp_state &= ~DP_BREAK_ON;

		/* reset latches */
		WR_WR0(dp1->dp_cntrl, WR0_RST_EXT_INT)
		UNLOCK_PORT(dp1, s1);
	}

	/* transmit buffer empty interrupt */

	if (rr3 & RR3_CHNB_TX_IP) {
		DU_LOG_ACT(dp0, DU_ACT_HANDLE_INTR_TXE_INTR);
		SYSINFO.xmtint++;
		s1 = LOCK_PORT(dp0);
		mips_du_stop_tx(dp0);	/* we'll restart when ready to send */
		dp0->dp_state |= DP_TX_EMPTY|DP_TX_START;
		UNLOCK_PORT(dp0, s1);
		dp0txsched++;
	}

	if (rr3 & RR3_CHNA_TX_IP) {
		DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_TXE_INTR);
		SYSINFO.xmtint++;
		s1 = LOCK_PORT(dp1);
		mips_du_stop_tx(dp1);	/* we'll restart when ready to send */
		dp1->dp_state |= DP_TX_EMPTY|DP_TX_START;
		UNLOCK_PORT(dp1, s1);
		dp1txsched++;
	}

	/*
	 * Hold the port lock across the calls to streams_interrupt(qenable)
	 * so that the queue can't get freed before it is actually linked to
	 * the enabled list.  Otherwise, we could end up trying to call the
	 * service procedure of a "dead" queue.
	 */

	if (dp0rxsched) {
	    s1 = LOCK_PORT(dp0);
	    state = dp0->dp_state;
	    rq = dp0->dp_rq;
	    if (state & DP_ISOPEN && rq && canenable(rq)) {
		streams_interrupt((strintrfunc_t)qenable, rq,
				  NULL, NULL);
	    }
	    UNLOCK_PORT(dp0, s1);
#ifdef DU_METERING
	    streams_int++;
#endif
	}

	if (dp0txsched)
		streams_interrupt((strintrfunc_t)du_exec_tx, dp0, NULL, NULL);

	if (dp1rxsched) {
	    s1 = LOCK_PORT(dp1);
	    state = dp1->dp_state;
	    rq = dp1->dp_rq;
	    if (state & DP_ISOPEN && rq && canenable(rq)) {
		streams_interrupt((strintrfunc_t)qenable, rq,
				  NULL, NULL);
	    }
	    UNLOCK_PORT(dp1, s1);
#ifdef DU_METERING
	    streams_int++;
#endif
	}

	if (dp1txsched)
		streams_interrupt((strintrfunc_t)du_exec_tx, dp1, NULL, NULL);

#ifdef DEBUG	
	if (logdp0)
	    DU_LOG_ACT(dp0, DU_ACT_HANDLE_INTR_RET1);
	if (logdp1)
	    DU_LOG_ACT(dp1, DU_ACT_HANDLE_INTR_RET1);
#endif
	return(1);
}

#endif


/*
 * Get a new buffer.  Must have the streams monitor.
 */
static mblk_t*				/* return NULL or the new buffer */
du_getbp(dport_t *dp, u_int pri)
{
	mblk_t *bp;
	int s;

	DU_LOG_ACT(dp, DU_ACT_GETBP);

	if (!dp->dp_cur_rbp && !dp->dp_nxt_rbp) {
		dp->dp_cur_rbp = dp->dp_nxt_rbp;
		dp->dp_nxt_rbp = NULL;
	}
	if (dp->dp_nxt_rbp) {
	    DU_LOG_ACT(dp, DU_ACT_GETBP_RET0);
	    return(dp->dp_cur_rbp);
	}

	bp = allocb(RBUF_LEN, pri);

	s = LOCK_PORT(dp);
	if (!bp && !(dp->dp_state & DP_SE_PENDING)) {
		UNLOCK_PORT(dp, s);
		bp = str_allocb(RBUF_LEN, dp->dp_rq, BPRI_HI);
		s = LOCK_PORT(dp);
		if (!bp)
			dp->dp_state |= DP_SE_PENDING;
	}
	UNLOCK_PORT(dp, s);

	if (!bp) {
		mips_du_iflow(dp, SEND_XOFF, !IGNORE_IXOFF);
		DU_LOG_ACT(dp, DU_ACT_GETBP_RET1);
		return(dp->dp_cur_rbp);
	}

	if (dp->dp_cur_rbp) {
		dp->dp_nxt_rbp = bp;
		DU_LOG_ACT(dp, DU_ACT_GETBP_RET2);
		return(dp->dp_cur_rbp);
	}

	dp->dp_cur_rbp = bp;

	bp = allocb(RBUF_LEN, pri);

	s = LOCK_PORT(dp);
	if (!bp && !(dp->dp_state & DP_SE_PENDING)) {
		UNLOCK_PORT(dp, s);
		bp = str_allocb(RBUF_LEN, dp->dp_rq, BPRI_HI);
		s = LOCK_PORT(dp);
		if (!bp)
			dp->dp_state |= DP_SE_PENDING;
	}
	UNLOCK_PORT(dp, s);

	if (!bp) {
		mips_du_iflow(dp, SEND_XOFF, !IGNORE_IXOFF);
		DU_LOG_ACT(dp, DU_ACT_GETBP_RET3);
		return(dp->dp_cur_rbp);
	}

	dp->dp_nxt_rbp = bp;
	DU_LOG_ACT(dp, DU_ACT_GETBP_RET4);
	return(dp->dp_cur_rbp);
}



/*
 * slow and hopefully infrequently used function to put characters somewhere
 * where they will go up stream
 */
static void
du_slowr(dport_t *dp, u_char c)
{
	mblk_t *bp;

	DU_LOG_ACT(dp, DU_ACT_SLOWR);

	if (dp->dp_iflag & IBLKMD) {	/* this kludge apes the old */
	    DU_LOG_ACT(dp, DU_ACT_SLOWR_RET0);
	    return;			/* block mode hack */
	}

	if (!(bp = dp->dp_cur_rbp)	/* get a buffer if we have none */
	    && !(bp = du_getbp(dp, BPRI_HI))) {
		dp->dp_allocb_fail++;
		DU_LOG_ACT(dp, DU_ACT_SLOWR_RET1);
		return;			/* YIKES */
	}
	if (bp->b_wptr < bp->b_datap->db_lim)
		*bp->b_wptr++ = c;
	else
		dp->dp_rb_over++;	/* YIKES */
	DU_LOG_ACT(dp, DU_ACT_SLOWR_RET2);
}

static int
du_process_inchar(dport_t *dp, int c)
{

	u_char sr;
	mblk_t *bp;
	register int s;

	DU_LOG_ACT_INFO(dp, DU_ACT_PROCESS_INCHAR, c);

	sr = c >> 8;
	c &= 0xff;

	/* start/stop output (if permitted) when we get XOFF or XON */
	if (dp->dp_iflag & IXON) {
		u_char cs = c;

		if (dp->dp_iflag & ISTRIP)
			cs &= 0x7f;
		s = LOCK_PORT(dp);
		if (dp->dp_state & DP_TXSTOP
		    && (cs == dp->dp_cc[VSTART]
		    || (dp->dp_iflag & IXANY
		    && (cs != dp->dp_cc[VSTOP]
		    || dp->dp_line == LDISC0)))) {
			dp->dp_state &= ~DP_TXSTOP;
			UNLOCK_PORT(dp, s);
			mips_du_start_tx(dp);
			if (cs == dp->dp_cc[VSTART]) {
				DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET0);
				return 0;
			}
		} else if (DP_LIT & dp->dp_state) {
			dp->dp_state &= ~DP_LIT;
			UNLOCK_PORT(dp, s);
		} else if (cs == dp->dp_cc[VSTOP]) {
			dp->dp_state |= DP_TXSTOP;
			DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RX_XOFF);
			mips_du_stop_tx(dp);	/* stop output */
			UNLOCK_PORT(dp, s);
			DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET1);
			return 0;
		} else if (cs == dp->dp_cc[VSTART]) {
			UNLOCK_PORT(dp, s);
			DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET2);
			return 0;
		} else if (cs == dp->dp_cc[VLNEXT]
		    && dp->dp_line != LDISC0) {
			dp->dp_state |= DP_LIT;	/* just note escape */
			UNLOCK_PORT(dp, s);
		} else {
			UNLOCK_PORT(dp, s);
		}
	}

	if (sr & RR0_BRK) {
		if (dp->dp_iflag & IGNBRK) {
		    DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET3);
			return 0;
		}
		if (dp->dp_iflag & BRKINT) {
			du_flushr(dp);
			(void)putctl1(dp->dp_rq->q_next, M_FLUSH, FLUSHRW);
			(void)putctl1(dp->dp_rq->q_next, M_PCSIG, SIGINT);
			DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET4);
			return 0;
		}
		if (dp->dp_iflag & PARMRK) {
			du_slowr(dp, '\377');
			du_slowr(dp, '\000');
		}
		c = '\000';
	} else if (sr & (RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR)) {
		if (sr & RR1_RX_ORUN_ERR)
			dp->dp_over++;		/* count overrun */
		if (sr & RR1_PARITY_ERR)
			dp->dp_parity++;	/* count parity */
		if (sr & RR1_FRAMING_ERR)
			dp->dp_fe++;		/* count framing */

		if (dp->dp_iflag & IGNPAR) {
		    DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET5);
		    return 0;

		} else if (!(dp->dp_iflag & INPCK)) {
			/* ignore input parity errors if asked */

		} else if (sr & (RR1_PARITY_ERR | RR1_FRAMING_ERR)) {
			if (dp->dp_iflag & PARMRK) {
				du_slowr(dp, '\377');
				du_slowr(dp, '\000');
			} else {
				c = '\000';
			}
		}
	} else if (dp->dp_iflag & ISTRIP) {
		c &= 0x7f;
	} else if (c == '\377' && dp->dp_iflag & PARMRK) {
		du_slowr(dp, '\377');
	}

	if (!(bp = dp->dp_cur_rbp)	/* get a buffer if we have none */
	    && !(bp = du_getbp(dp, BPRI_HI))) {
		dp->dp_allocb_fail++;
		DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET6);
		return -1;	/* YIKES */
	}
	if (bp->b_wptr < (bp->b_datap->db_lim - 1)) {
		*bp->b_wptr++ = c;
		DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET7);
		return 0;
	}
	if (bp->b_wptr == (bp->b_datap->db_lim - 1)) {
		*bp->b_wptr++ = c;
		DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET8);
		return -1;	/* Nearly YIKES */
	}
	dp->dp_rb_over++;	/* YIKES */
	DU_LOG_ACT(dp, DU_ACT_PROCESS_INCHAR_RET9);
	return -1;
}

static void
du_process_rx(dport_t *dp)
{
	char c;
	register int cnt;
	
}


/*
 * input interrupt
 */
static int
du_rx(dport_t *dp, int hi)
{
	volatile unsigned char *cntrl = dp->dp_cntrl;
	volatile unsigned char *data = dp->dp_data;
	int s, err;

	DU_LOG_ACT(dp, DU_ACT_RX);

	s = LOCK_PORT(dp);
	/* must be open to input */
	if ((dp->dp_state & (DP_ISOPEN | DP_WOPEN)) != DP_ISOPEN
	    && dp->dp_porthandler == NULL) {
		if (!(dp->dp_state & (DP_ISOPEN | DP_WOPEN))) {
		    if (hi) {
			UNLOCK_PORT(dp, s);
			/* can't do anything on this port until this is fixed,
			 * so run low exclusively
			 */
			RUNLOW(EXCLUSIVELY);
			DU_LOG_ACT(dp, DU_ACT_RX_RET0);
			return(SCHED_NOTHING); /* defer this to the lo intr */
		    }

		    UNLOCK_PORT(dp, s);
		    mips_du_stop_rx(dp, (dp->dp_cflag & HUPCL) ? 1 : 0);
		    du_rclr(dp);
		    du_tx_putcmd(dp, CMD_ZAP,
				 (dp->dp_cflag & HUPCL) ? 1 : 0);
		    DU_LOG_ACT(dp, DU_ACT_RX_RET1);
		    return(SCHED_TX);
		}
		UNLOCK_PORT(dp, s);
		du_rclr(dp);	/* just forget it if partly open */
		DU_LOG_ACT(dp, DU_ACT_RX_RET2);
		return(SCHED_NOTHING);
	}
	UNLOCK_PORT(dp, s);

	/* must be reading, to read.  DCD must be on for modem port to read */
	if (!(dp->dp_cflag & CREAD)) { 
	    du_rclr(dp);
	    DU_LOG_ACT(dp, DU_ACT_RX_RET3);
	    return(SCHED_NOTHING);
	}

	/* Suck in all avaliable characters. */
	err = du_get_inchars(dp, hi) ? SCHED_RX : SCHED_NOTHING;
	DU_LOG_ACT(dp, DU_ACT_RX_RET4);
	return(err);
}


/*
 * process carrier-on interrupt
 */
static void
du_con(dport_t *dp)
{
	int s;
	u_int state;

	DU_LOG_ACT(dp, DU_ACT_CON);

	s = LOCK_PORT(dp);
	state = dp->dp_state;
	UNLOCK_PORT(dp, s);

	if (state & DP_WOPEN)
		wakeup((caddr_t)dp);	/* awaken open() requests */
	DU_LOG_ACT(dp, DU_ACT_CON_RET0);
}



/*
 * process carrier-off interrupt
 */
static void
du_coff(dport_t *dp)
{
	int s;
	u_int state;

	DU_LOG_ACT(dp, DU_ACT_COFF);
	s = LOCK_PORT(dp);
	state = dp->dp_state;
	UNLOCK_PORT(dp, s);

	if (!(dp->dp_cflag & CLOCAL)	/* worry only for an open modem */
	    && state & DP_ISOPEN) {
		du_zap(dp, HUPCL);	/* kill the modem */
		flushq(dp->dp_wq, FLUSHDATA);
		(void)putctl1(dp->dp_rq->q_next, M_FLUSH, FLUSHW);
		(void)putctl(dp->dp_rq->q_next, M_HANGUP);
	}
	DU_LOG_ACT(dp, DU_ACT_COFF_RET0);
}

/*
 * output something.
 * this routine can be called either from a streams interrupt or from
 * a serial interrupt. The parameter nostreams is true when called from
 * a serial interrupt. In this case, no streams calls may be made, since
 * the monitor is not held. The only action we can take is to pass bytes
 * to the hardware from the current streams buffer.
 */
static int
du_tx(dport_t *dp, int nostreams)
{
	u_char c;
	volatile unsigned char *cntrl = dp->dp_cntrl;
	mblk_t *wbp;
	int local_fifo_size, count, empty;
	int s, sentone = 0;

	DU_LOG_ACT_INFO(dp, DU_ACT_TX, nostreams);

	/* The z85230 can only send one byte unless we just got a fifo
	 * drained interrupt.
	 */
	s = LOCK_PORT(dp);
#if SABLE && IP27_NO_HUBUART_INT
	dp->dp_state |= DP_TX_EMPTY;
	empty = 1;
	fifo_size = 32000;
#else
	empty = (dp->dp_state & DP_TX_EMPTY) || nostreams;
	dp->dp_state &= ~DP_TX_EMPTY;
#endif

	local_fifo_size = count = empty ? fifo_size : 1;

	/* send all we can */


	/* we have to protect from the hi level interrupt between getting
	 * the char from the streams buffer and sending it to the hardware
	 * otherwise output chars may appear on the output in reverse order
	 * if the high level races in
	 */
	while (count && mips_du_txrdy(dp)) {

		if (!(dp->dp_state & DP_ISOPEN)
		    || dp->dp_state & (DP_BREAK | DP_BREAK_QUIET)
		    || dp->dp_state & (DP_TXSTOP | DP_TIMEOUT)
			&& !(dp->dp_state & (DP_TX_TXON | DP_TX_TXOFF))) {

		    if (nostreams) {
			UNLOCK_PORT(dp, s);
			DU_LOG_ACT(dp, DU_ACT_TX_RET0);
			return(sentone);
		    }

		    mips_du_stop_tx(dp);
		    UNLOCK_PORT(dp, s);
		    DU_LOG_ACT(dp, DU_ACT_TX_RET1);
		    return(0);
		}

		SYSINFO.outch++;
		if (dp->dp_state & DP_TX_TXON) {	/* send XON or XOFF */
		    if (nostreams) {
			UNLOCK_PORT(dp, s);
			DU_LOG_ACT(dp, DU_ACT_TX_RET2);
			return(sentone);
		    }

		    if (dp->dp_cflag & CNEW_RTSCTS
			&& !(dp->dp_state & DP_CTS)) {
			mips_du_stop_tx(dp);
			UNLOCK_PORT(dp, s);
			DU_LOG_ACT(dp, DU_ACT_TX_RET3);
			return(0);
		    }
		    c = dp->dp_cc[VSTART];
		    dp->dp_state &= ~(DP_TX_TXON | DP_TX_TXOFF | DP_BLOCK);
		    DU_LOG_ACT(dp, DU_ACT_TX_XON);
		} else if (dp->dp_state & DP_TX_TXOFF) {
		    if (nostreams) {
			UNLOCK_PORT(dp, s);
			DU_LOG_ACT(dp, DU_ACT_TX_RET4);
			return(sentone);
		    }

		    if (dp->dp_cflag & CNEW_RTSCTS
			&& !(dp->dp_state & DP_CTS)) {
			mips_du_stop_tx(dp);
			UNLOCK_PORT(dp, s);
			DU_LOG_ACT(dp, DU_ACT_TX_RET5);
			return(0);
		    }
		    c = dp->dp_cc[VSTOP];
		    dp->dp_state &= ~DP_TX_TXOFF;
		    dp->dp_state |= DP_BLOCK;
		    DU_LOG_ACT(dp, DU_ACT_TX_XOFF);
		} else {
			if (!(wbp = dp->dp_wbp)) {	/* get another msg */
				if (nostreams) {
				    UNLOCK_PORT(dp, s);
				    DU_LOG_ACT(dp, DU_ACT_TX_RET6);
				    return(sentone);
				}

				wbp = getq(dp->dp_wq);
				if (!wbp) {
					mips_du_stop_tx(dp);
					UNLOCK_PORT(dp, s);
					DU_LOG_ACT(dp, DU_ACT_TX_RET7);
					return(0);
				}

				switch (wbp->b_datap->db_type) {
				case M_DATA:
					dp->dp_wbp = wbp;

					break;

				/* must wait until output drained */
				case M_DELAY:	/* start output delay */
					if (count == local_fifo_size) {
						dp->dp_state |= DP_TIMEOUT;
						dp->dp_tid = STREAMS_TIMEOUT(
						(strtimeoutfunc_t)du_delay,
					    		(caddr_t)dp,
					    		*(int *)wbp->b_rptr);
						freemsg(wbp);
						continue;
					} else {
						putbq(dp->dp_wq, wbp);
						UNLOCK_PORT(dp, s);
						DU_LOG_ACT(dp, DU_ACT_TX_RET8);
						return(0);
					}

				/* must wait until output drained */
				case M_IOCTL:
					if (count == local_fifo_size) {
						du_i_ioctl(dp, wbp, &s);
						continue;
					} else {
						putbq(dp->dp_wq, wbp);
						UNLOCK_PORT(dp, s);
						DU_LOG_ACT(dp, DU_ACT_TX_RET9);
						return(0);
					}

				default:
					panic("bad duart msg");
				}
			}
			if (dp->dp_cflag & CNEW_RTSCTS
			    && !(dp->dp_state & DP_CTS)) {
			    if (nostreams) {
				UNLOCK_PORT(dp, s);
				DU_LOG_ACT(dp, DU_ACT_TX_RET10);
				return(sentone);
			    }

			    mips_du_stop_tx(dp);
			    UNLOCK_PORT(dp, s);
			    DU_LOG_ACT(dp, DU_ACT_TX_RET11);
			    return(0);
			}

			if (wbp->b_rptr >= wbp->b_wptr) {
			    if (nostreams) {
				UNLOCK_PORT(dp, s);
				DU_LOG_ACT(dp, DU_ACT_TX_RET12);
				return(sentone);
			    }

			    dp->dp_wbp = rmvb(wbp, wbp);
			    freeb(wbp);
			    continue;
			}

			c = *wbp->b_rptr++;
		}

		mips_du_putchar(dp, c);

		/* give others a chance */
		UNLOCK_PORT(dp, s);
		s = LOCK_PORT(dp);

		sentone = 1;
		count--;
	}

	UNLOCK_PORT(dp, s);
	if (nostreams) {
	    DU_LOG_ACT(dp, DU_ACT_TX_RET13);
	    return(sentone);
	}

	/*
	 * If the queue is now empty, put an empty data block on it
	 * to prevent a close from finishing prematurely.
	 */
	if (!dp->dp_wq->q_first && dp->dp_wbp && (wbp = allocb(0, BPRI_HI)))
		putbq(dp->dp_wq, wbp);

	DU_LOG_ACT(dp, DU_ACT_TX_RET14);
	return(0);
}


/*
 * set to give some characters away
 */
void
gl_setporthandler(int port, int (*fp)(unsigned char))
{
	int s;
	dport_t *dp;

	dp = &dports[port];
	DU_LOG_ACT(dp, DU_ACT_GL_SPH);

	s = LOCK_PORT(dp);
	dp->dp_porthandler = fp;
	UNLOCK_PORT(dp, s);
	DU_LOG_ACT(dp, DU_ACT_GL_SPH_RET0);
}


/*
 * read up to `len' characters from the secondary console port.
 */
/* XXXrs - this isn't used anywhere in the kernel */
int
ducons_read(u_char *buf, int len)
{
	int c;
	dport_t *dp = &dports[SCNDCPORT];
	int i = 0;
	int s;

	DU_LOG_ACT(dp, DU_ACT_CONS_READ);
	s = SET_SPL();

	while (i < len) {
		if ((c = mips_du_getchar(dp, 1)) == -1)
			break;
		*buf++ = c & 0xff;		/* ignore status */
		i++;
	}

	SPLX(s);

	DU_LOG_ACT(dp, DU_ACT_CONS_READ_RET0);
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
	dport_t *dp = &dports[SCNDCPORT];
	int i;
	int s;
#if defined (NOUART_SABLE)
	buf[len] = 0;
	hubuart_putstr((char *)buf);
	return len;
#endif
/* NOTREACHED */

	DU_LOG_ACT(dp, DU_ACT_CONS_WRITE);
#if SABLE
	for (i=0; i<len; i++) {
		hubuart_output(*buf);
		buf++;
	}
	DU_LOG_ACT(dp, DU_ACT_CONS_WRITE_RET0);
	return(len);
#else
        if (cpuid() != 0) {
                /* We are on the wrong CPU, cpuaction() over */
                cpuaction(0, (void (*)())ducons_write, A_NOW, buf, len);
		DU_LOG_ACT(dp, DU_ACT_CONS_WRITE_RET0);
                return(len);
        }

	/*
	 * Lock the port down to avoid racing with "regular" transmit
	 * code.  The "regular" transmit logic does not expect to have
	 * characters stuffed into the output buffer between seeing the
	 * transmit empty interrupt and restarting tx.
	 */
	s = LOCK_PORT(dp);

	for (i = 0; i < len; i++) {
		while (!mips_du_txrdy_nolock(dp)) /* poll until ready */
			;
		mips_du_putchar_nolock(dp, *buf);
		if (*buf == '\n') {
			while (!mips_du_txrdy_nolock(dp))
				;
			mips_du_putchar_nolock(dp, '\r');
		}
		buf++;
	}
	while (!mips_du_txrdy_nolock(dp))
		;

	UNLOCK_PORT(dp, s);

	DU_LOG_ACT(dp, DU_ACT_CONS_WRITE_RET1);
	return(len);
#endif	/* SABLE */
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
	dport_t *dp = &dports[SCNDCPORT];
	int s, locked;

	DU_LOG_ACT(dp, DU_ACT_CONPOLL);
	/*
	 * If cpu 0 is currently stopped in the debugger,
	 * it's watching the console port for cntrl-A.
	 * If cpu 0 is not currently stopped, it's executing
	 * in the kernel and will also see a cntrl-A.  No
	 * need for any other processor to interfere.
	 */
	if (cpuid() != 0) {
	    DU_LOG_ACT(dp, DU_ACT_CONPOLL_RET0);
	    return;
	}

	if (!kdebug) {
	    DU_LOG_ACT(dp, DU_ACT_CONPOLL_RET1);
	    return;
	}

	locked = 0;
	if (dp->dp_port_lock) {
		if (!(s = _trylock(dp->dp_port_lock, spl7))) {
		    DU_LOG_ACT(dp, DU_ACT_CONPOLL_RET2);
		    return;	/* Bail if have lock but can't get it. */
		}
		locked++;	/* Got the lock */
	}

	c = nolock_du_getchar(dp);

	if (locked)
		io_spunlockspl(dp->dp_port_lock, s);

	if (c != -1 && (c & 0xff) == DEBUG_CHAR)
		debug("ring");
	DU_LOG_ACT(dp, DU_ACT_CONPOLL_RET3);
}

int
du_getchar(int port)
{
	int c;
	int s;
	volatile unsigned char *cntrl = dports[port].dp_cntrl;
	dport_t *dp = &dports[port];

	DU_LOG_ACT(dp, DU_ACT_GETCHAR);

	if (cntrl == 0) {
		/* Not yet ready */
	    DU_LOG_ACT(dp, DU_ACT_GETCHAR_RET0);
	    return(-1);
	}

	if ((c = mips_du_getchar(dp, 0)) != -1)
		c &= 0xff;

	DU_LOG_ACT(dp, DU_ACT_GETCHAR_RET1);
	return(c);
}

void
du_putchar(int port, unsigned char c)
{
    dport_t *dp = &dports[port];
    int s;
    
    while (!mips_du_txrdy(dp));
    mips_du_putchar(dp, c);
}

/*
 * Determine whether or not the DUART port has completely transmitted all
 * output characters.
 * 0 => not done transmitting
 * Port lock MUST be held by caller.
 */
static int
mips_du_allsent_nolock(dport_t *dp)
{
	u_char du_cntrl;	/* XXXrs - for RD_CNTRL */
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	volatile unsigned char *cntrl = dp->dp_cntrl;

	return(RD_CNTRL(cntrl, RR1) & RR1_ALL_SENT);
}

/*
 * determine whether the DUART port is ready to transmit a character or not.
 * 0 => not ready
 */
static int
mips_du_txrdy(dport_t *dp)
{
	register int s, rtn;
	volatile unsigned char *cntrl = dp->dp_cntrl;

	DU_LOG_ACT(dp, DU_ACT_TXRDY);
#if SABLE
	/* For now we return "ready".  Low level routines will not let us
	 * output next character until it's "really" ready.
	 */
	DU_LOG_ACT(dp, DU_ACT_TXRDY_RET0);
	return( RR0_TX_EMPTY );
#else
	<<< This only compiles for sable >>>
#endif
}

/*
 * Same as above, but port lock held by caller.
 */
static int
mips_du_txrdy_nolock(dport_t *dp)
{
	volatile unsigned char *cntrl = dp->dp_cntrl;

	DU_LOG_ACT(dp, DU_ACT_TXRDY_NOLOCK);
#if SABLE
	/* For now we return "ready".  Low level routines will not let us
	 * output next character until it's "really" ready.
	 */
	DU_LOG_ACT(dp, DU_ACT_TXRDY_NOLOCK_RET0);
	return( HUBUART_OUTPUT_RDY );
#else
	<<< This only compiles for sable >>>
#endif
}


/*
 * transmit a character. du_txrdy must be called first to make sure the
 * transmitter is ready
 */
static void
mips_du_putchar(dport_t *dp, u_char c)
{
	register int s;

	DU_LOG_ACT_INFO(dp, DU_ACT_MIPS_PUTCHAR, c);

#if SABLE
	REMOTE_HUB_S(master_nasid, MD_UREG0_1, c);
	DU_LOG_ACT(dp, DU_ACT_MIPS_PUTCHAR_RET0);
	return;
#else
	<<< This only compiles for sable >>>
#endif
}

/* 
 * Same as above, but port lock held by caller.
 */
static void
mips_du_putchar_nolock(dport_t *dp, u_char c)
{
	DU_LOG_ACT_INFO(dp, DU_ACT_MIPS_PUTCHAR_NOLOCK, c);
#if SABLE
	hubuart_output( c );
	DU_LOG_ACT(dp, DU_ACT_MIPS_PUTCHAR_NOLOCK_RET0);
	return;
#else
	<<< This only compiles for sable >>>
#endif
}


static void
dsp_du_putchar(dport_t *dp, u_char c)
{
}


/********** du_getchar **********/

/*
 * get a character from the receiver. if none received, return -1.
 * append status to data
 */
static int
mips_du_getchar(dport_t *dp, int ignerr)
{
	int c, rtn;
	volatile unsigned char *cntrl = dp->dp_cntrl;
	int s;
	u_char du_cntrl;	/* XXXrs - for RD_CNTRL */
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */

	DU_LOG_ACT(dp, DU_ACT_MIPS_GETCHAR);
#if SABLE
	DU_LOG_ACT(dp, DU_ACT_MIPS_GETCHAR_RET0);
	if (HUBUART_INPUT_RDY) {
		return(HUBUART_INPUT);
	} else
		return(-1);
#else
	<<< This only compiles for sable >>>
#endif
}


static int
nolock_du_getchar(dport_t *dp)
{
	int c, rtn;
	u_char du_cntrl;	/* XXXrs - for RD_CNTRL */
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	volatile unsigned char *cntrl = dp->dp_cntrl;

	DU_LOG_ACT(dp, DU_ACT_NOLOCK_GETCHAR);
	if (RD_RR0(cntrl) & RR0_RX_CHR) {
		c = RD_CNTRL(cntrl, RR1);
		if (c &= (RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR))
			WR_WR0(cntrl, WR0_RST_ERR)

		if (dp->dp_state & DP_BREAK_ON)
			c |= RR0_BRK;

		DU_LOG_ACT(dp, DU_ACT_NOLOCK_GETCHAR_RET0);
		return((c << 8) | (RD_DATA(dp->dp_data) & dp->dp_bitmask));
	}
	DU_LOG_ACT(dp, DU_ACT_NOLOCK_GETCHAR_RET1);
	return(-1);
}

/*
 * Port MUST be locked here.
 */
static void
mips_du_stop_tx(dport_t *dp)
{
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	volatile unsigned char *cntrl = dp->dp_cntrl;

	DU_LOG_ACT(dp, DU_ACT_STOP_TX);
	dp->dp_wr1 &= ~WR1_TX_INT_ENBL;
#ifndef SABLE
	WR_CNTRL(cntrl, WR1, dp->dp_wr1)
#endif
	dp->dp_state &= ~(DP_TX_EMPTY|DP_TX_START);
#ifndef SABLE
	WR_WR0(cntrl, WR0_RST_TX_INT)
#endif
	DU_LOG_ACT(dp, DU_ACT_STOP_TX_RET0);
}



static void
dsp_du_stop_tx(dport_t *dp)
{
}


/********** du_start_tx **********/
static void
mips_du_start_tx(dport_t *dp)
{
	int s, s1;
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	volatile unsigned char *cntrl = dp->dp_cntrl;

	DU_LOG_ACT(dp, DU_ACT_START_TX);
	s1 = SET_SPL();
	s = LOCK_PORT(dp);
	dp->dp_state &= ~DP_TX_START;
	if (!(dp->dp_wr1 & WR1_TX_INT_ENBL)) {
		dp->dp_wr1 |= WR1_TX_INT_ENBL;
#ifndef SABLE
		WR_CNTRL(cntrl, WR1, dp->dp_wr1)
#endif
		DU_LOG_ACT(dp, DU_ACT_START_TX_ENABLE);
		UNLOCK_PORT(dp, s);
		du_tx(dp, 0);	/* send something */
#if SABLE && IP27_NO_HUBUART_INT
		/* Simulate receipt of transmit buffer empty interrupt
		 * following transmission of all queued output characters.
		 */
		mips_du_stop_tx(dp);
		dp->dp_state |= DP_TX_EMPTY|DP_TX_START;
#endif
		SPLX(s1);
		DU_LOG_ACT(dp, DU_ACT_START_TX_RET0);
		return;
	}
	UNLOCK_PORT(dp, s);
	SPLX(s1);
	DU_LOG_ACT(dp, DU_ACT_START_TX_RET1);
}


static void
mips_du_stop_rx(dport_t *dp, int hup)
{
	int s;
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	volatile unsigned char *cntrl = dp->dp_cntrl;
	char *console = kopt_find("console");

	DU_LOG_ACT(dp, DU_ACT_STOP_RX);
#if SABLE
	DU_LOG_ACT(dp, DU_ACT_STOP_RX_RET0);
	return;
#else
	s = LOCK_PORT(dp);
	if (!(kdebug && dp->dp_index == SCNDCPORT)) {
		dp->dp_wr1 &= ~WR1_RX_INT_MSK;	/* disable receiver interrupt */
		dp->dp_wr15 &= ~WR15_BRK_IE;	/* disable BREAK interrupt */
	}

	/* deassert RTS, DTR if hup != 0 */
	if (hup 
	    && !(console && *console == 'd' && dp->dp_index == SCNDCPORT)) {
		
		dp->dp_wr15 &= ~(WR15_DCD_IE | WR15_CTS_IE);

		/* deassert output handshake signals on tty port */
		DU_DTR_DEASSERT(dp->dp_wr5);
		DU_RTS_DEASSERT(dp->dp_wr5);
		DU_LOG_ACT(dp, DU_ACT_STOP_RX_RTS_DRT_DEASSERT);
	}

	/* update hardware registers */
	UNLOCK_PORT(dp, s);
	DU_LOG_ACT(dp, DU_ACT_STOP_RX_RET0);
#endif	/* SABLE */
}

/*
 * return !0 if carrier detected
 */
static int
mips_du_start_rx(dport_t *dp)
{
	volatile unsigned char *cntrl = dp->dp_cntrl;
	u_char o_imr;
	u_char stat;
	int s;
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	u_int state;

	DU_LOG_ACT(dp, DU_ACT_START_RX);
#if SABLE
	dp->dp_state |= DP_DCD;
	DU_LOG_ACT(dp, DU_ACT_START_RX_RET0);
	return( DP_DCD );
#else
	s = LOCK_PORT(dp);
	if (dp->dp_state & DP_RS422) {
		DU_DTR_ASSERT(dp->dp_wr5);
		DU_RTS_DEASSERT(dp->dp_wr5);
	} else {
		DU_DTR_ASSERT(dp->dp_wr5);
		DU_RTS_ASSERT(dp->dp_wr5);
		DU_LOG_ACT(dp, DU_ACT_START_RX_RTS_DTR_ASSERT);
	}
	WR_CNTRL(cntrl, WR5, dp->dp_wr5)

	o_imr = dp->dp_wr1;
	dp->dp_wr1 |=  WR1_RX_INT_ERR_ALL_CHR;

	/* watch carrier-detect only if this is an open modem port */
	if (!(dp->dp_cflag & CLOCAL)
	    && dp->dp_state & (DP_ISOPEN | DP_WOPEN)) 
		dp->dp_wr15 |= WR15_DCD_IE;

	/* Enable CTS only if CNEW_RTSCTS is set on an open modem port*/
	if ((dp->dp_cflag & CNEW_RTSCTS)
	    && dp->dp_state & (DP_ISOPEN | DP_WOPEN))
		dp->dp_wr15 |= WR15_CTS_IE;

	dp->dp_wr15 |= WR15_BRK_IE;
	WR_CNTRL(cntrl, WR15, dp->dp_wr15)

	if (dp->dp_wr15 & (WR15_DCD_IE | WR15_CTS_IE | WR15_BRK_IE))
		dp->dp_wr1 |= WR1_EXT_INT_ENBL;
	else
		dp->dp_wr1 &= ~WR1_EXT_INT_ENBL;
	WR_CNTRL(cntrl, WR1, dp->dp_wr1)
	UNLOCK_PORT(dp, s);

	/* if now enabling input, gobble old stuff */
	if (!(o_imr & WR1_RX_INT_MSK))
		du_rclr(dp);

	s = LOCK_PORT(dp);
	/* make sure the external status bits are current */
	WR_WR0(cntrl, WR0_RST_EXT_INT);
	stat = RD_RR0(cntrl);

	if (stat & RR0_BRK)		/* receiving break ? */
		dp->dp_state |= DP_BREAK_ON;
	else
		dp->dp_state &= ~DP_BREAK_ON;

	if (DU_CTS_ASSERTED(stat)) {
		dp->dp_state |= DP_CTS;
		DU_LOG_ACT(dp, DU_ACT_START_RX_CTS_ON);
	}
	else {
		dp->dp_state &= ~DP_CTS;
		DU_LOG_ACT(dp, DU_ACT_START_RX_CTS_OFF);
	}

	if (DU_DCD_ASSERTED(stat)) {
		dp->dp_state |= DP_DCD;
		DU_LOG_ACT(dp, DU_ACT_START_RX_DCD_ON);
	}
	else {
		dp->dp_state &= ~DP_DCD;
		DU_LOG_ACT(dp, DU_ACT_START_RX_DCD_OFF);
	}

	state = dp->dp_state & DP_DCD;
	UNLOCK_PORT(dp, s);

	DU_LOG_ACT(dp, DU_ACT_START_RX_RET0);
	return state;
#endif
}



static void
mips_du_config(dport_t *dp, tcflag_t cflag, speed_t ospeed, struct termio *tp)
{
	volatile unsigned char *cntrl = dp->dp_cntrl;
	tcflag_t delta_cflag;
	int delta_ospeed;
	u_short time_constant;
	int s;
	int du_s;		/* XXXrs - for RD_CNTRL/WR_CNTRL */
	extern int baud_to_cbaud(speed_t);	       /* in streamio.c */

	DU_LOG_ACT(dp, DU_ACT_CONFIG);
	delta_cflag = (cflag ^ dp->dp_cflag)
		& (CLOCAL|CSIZE|CSTOPB|PARENB|PARODD|CNEW_RTSCTS);
	if (ospeed == __INVALID_BAUD)
		ospeed = dp->dp_ospeed;
	delta_ospeed = ospeed != dp->dp_ospeed;

	if (tp)
		dp->dp_termio = *tp;
	dp->dp_cflag = cflag;
	dp->dp_ospeed = dp->dp_ispeed = ospeed;

#if SABLE
	DU_LOG_ACT(dp, DU_ACT_CONFIG_RET0);
	return;
#else
	/* 
	 * always configures during open in case the last process set up 
	 * the port to use the external clock and did not set it back to use
	 * the internal BRG
	 */
	s = LOCK_PORT(dp);
	if (delta_cflag || delta_ospeed || !(dp->dp_state & DP_ISOPEN)) {
		if (ospeed == B0) {	/* hang up line if asked */
			UNLOCK_PORT(dp, s);
			du_coff(dp);
			du_zap(dp, HUPCL);
			DU_LOG_ACT(dp, DU_ACT_CONFIG_RET0);
			return;
		}
		dp->dp_wr3 = WR3_RX_ENBL;
		if (dp->dp_state & DP_EXTCLK)
			dp->dp_wr4 &= WR4_CLK_MSK;
		else
			dp->dp_wr4 = WR4_X16_CLK;

		dp->dp_wr5 = dp->dp_wr5 & (WR5_RTS | WR5_DTR) | WR5_TX_ENBL;

		/* set number of stop bits */
		dp->dp_wr4 |= cflag & CSTOPB ? WR4_2_STOP : WR4_1_STOP;

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
		UNLOCK_PORT(dp, s);

		(void)mips_du_start_rx(dp);
		DU_LOG_ACT(dp, DU_ACT_CONFIG_RET1);
		return;
	}
	UNLOCK_PORT(dp, s);
	DU_LOG_ACT(dp, DU_ACT_CONFIG_RET2);
#endif /* !SABLE */
}

static void
mips_du_iflow(dport_t *dp, int send_xon, int ignore_ixoff)
{
	int s;
	int du_s;	  /* XXXrs - for RD_CNTRL/WR_CNTRL */
	volatile unsigned char *cntrl = dp->dp_cntrl;

	DU_LOG_ACT_INFO(dp, DU_ACT_IFLOW, send_xon << 4 | ignore_ixoff);
	s = LOCK_PORT(dp);
	if (dp->dp_cflag & CNEW_RTSCTS) {	/* hardware flow control */
		if (send_xon && !DU_RTS_ASSERTED(dp->dp_wr5)) {
			DU_RTS_ASSERT(dp->dp_wr5);
			DU_LOG_ACT(dp, DU_ACT_IFLOW_RTS_ASSERT);
		} else if (!send_xon && DU_RTS_ASSERTED(dp->dp_wr5)) {
			DU_RTS_DEASSERT(dp->dp_wr5);
			DU_LOG_ACT(dp, DU_ACT_IFLOW_RTS_DEASSERT);
		}
	}

	if (!ignore_ixoff && !(dp->dp_iflag & IXOFF)) {
		UNLOCK_PORT(dp, s);
		DU_LOG_ACT(dp, DU_ACT_IFLOW_RET0);
		return;
	}

	if (send_xon && dp->dp_state & DP_BLOCK) {
		dp->dp_state |= DP_TX_TXON;
		UNLOCK_PORT(dp, s);
		mips_du_start_tx(dp);
		DU_LOG_ACT(dp, DU_ACT_IFLOW_RET1);
		return;
	}
	if (!send_xon && !(dp->dp_state & DP_BLOCK)) {
		dp->dp_state |= DP_TX_TXOFF;
		dp->dp_state &= ~DP_TX_TXON;
		UNLOCK_PORT(dp, s);
		mips_du_start_tx(dp);
		DU_LOG_ACT(dp, DU_ACT_IFLOW_RET2);
		return;
	}
	UNLOCK_PORT(dp, s);
	DU_LOG_ACT(dp, DU_ACT_IFLOW_RET3);
}



/*
 * Port MUST be locked here.
 */
static void
mips_du_break(dport_t *dp, int start_break)
{
	register int s;
	int du_s;	  /* XXXrs - for RD_CNTRL/WR_CNTRL */
	volatile unsigned char *cntrl = dp->dp_cntrl;

	DU_LOG_ACT(dp, DU_ACT_BREAK);
	cntrl = dp->dp_cntrl;
	if (start_break)
		dp->dp_wr5 |= WR5_SEND_BRK;
	else
		dp->dp_wr5 &= ~WR5_SEND_BRK;
	DU_LOG_ACT(dp, DU_ACT_BREAK_RET0);
}



/* -------------------- DEBUGGING SUPPORT -------------------- */

char *padding = "                          ";

/* any additions to this array must be reflected by the enum
 * in mpzduart.h as well
 */
char *duact_strings[] =
{
    "EMPTY",

    "EXEC_TX",
    "EXEC_TX_RET0",

    "TX_PUTCMD",
    "TX_PUTCMD_RET0",
    "TX_PUTCMD_RET1",

    "RX_BUF_GETCHAR",
    "RX_BUF_GETCHAR_RET0",
    "RX_BUF_GETCHAR_RET1",

    "RX_BUF_PUTCHAR",
    "RX_BUF_PUTCHAR_RET0",
    "RX_BUF_PUTCHAR_RET1",
    "RX_BUF_PUTCHAR_RET2",
    "RX_BUF_PUTCHAR_RUNLOW",
    
    "GET_INCHARS",
    "GET_INCHARS_RET0",

    "RCLR",
    "RCLR_RET0",

    "ZAP",
    "ZAP_RET0",

    "DELAY",
    "DELAY_RET0",

    "FLUSHW",
    "FLUSHW_RET0",

    "FLUSHR",
    "FLUSHR_RET0",

    "SAVE",
    "SAVE_RET0",

    "TCSET",
    "TCSET_RET0",

    "I_IOCTL",
    "I_IOCTL_RET0",
    "I_IOCTL_RET1",
    "I_IOCTL_ITIMER",

    "OPEN",
    "OPEN_RET0",
    "OPEN_RET1",
    "OPEN_RET2",
    "OPEN_RET3",
    "OPEN_RET4",
    "OPEN_MODEM",
    "OPEN_FLOW_MODEM",

    "CLOSE",
    "CLOSE_RET0",

    "RSRV",
    "RSRV_RET0",
    "RSRV_RET1",
    "RSRV_PUTNEXT",

    "WPUT",
    "WPUT_RET0",

    "HANDLE_INTR_HI",
    "HANDLE_INTR_LO",
    "HANDLE_INTR_RET0",
    "HANDLE_INTR_RET1",
    "HANDLE_INTR_CTS_ON",
    "HANDLE_INTR_CTS_OFF",
    "HANDLE_INTR_DCD_ON",
    "HANDLE_INTR_DCD_OFF",
    "HANDLE_INTR_ESTAT_INTR",
    "HANDLE_INTR_TXE_INTR",

    "GETBP",
    "GETBP_RET0",
    "GETBP_RET1",
    "GETBP_RET2",
    "GETBP_RET3",
    "GETBP_RET4",

    "SLOWR",
    "SLOWR_RET0",
    "SLOWR_RET1",
    "SLOWR_RET2",

    "PROCESS_INCHAR",
    "PROCESS_INCHAR_RET0",
    "PROCESS_INCHAR_RET1",
    "PROCESS_INCHAR_RET2",
    "PROCESS_INCHAR_RET3",
    "PROCESS_INCHAR_RET4",
    "PROCESS_INCHAR_RET5",
    "PROCESS_INCHAR_RET6",
    "PROCESS_INCHAR_RET7",
    "PROCESS_INCHAR_RET8",
    "PROCESS_INCHAR_RET9",
    "PROCESS_INCHAR_RX_XOFF",

    "RX",
    "RX_RET0",
    "RX_RET1",
    "RX_RET2",
    "RX_RET3",
    "RX_RET4",

    "CON",
    "CON_RET0",

    "COFF",
    "COFF_RET0",

    "TX",
    "TX_RET0",
    "TX_RET1",
    "TX_RET2",
    "TX_RET3",
    "TX_RET4",
    "TX_RET5",
    "TX_RET6",
    "TX_RET7",
    "TX_RET8",
    "TX_RET9",
    "TX_RET10",
    "TX_RET11",
    "TX_RET12",
    "TX_RET13",
    "TX_RET14",
    "TX_XON",
    "TX_XOFF",
    "TX_ENABLE",

    "GL_SPH",
    "GL_SPH_RET0",

    "CONS_READ",
    "CONS_READ_RET0",

    "CONS_WRITE",
    "CONS_WRITE_RET0",
    "CONS_WRITE_RET1",

    "CONPOLL",
    "CONPOLL_RET0",
    "CONPOLL_RET1",
    "CONPOLL_RET2",
    "CONPOLL_RET3",

    "GETCHAR",
    "GETCHAR_RET0",
    "GETCHAR_RET1",

    "PUTCHAR",
    "PUTCHAR_RET0",

    "TXRDY",
    "TXRDY_RET0",

    "TXRDY_NOLOCK",
    "TXRDY_NOLOCK_RET0",

    "MIPS_PUTCHAR",
    "MIPS_PUTCHAR_RET0",
    "MIPS_PUTCHAR_ENABLE",

    "MIPS_PUTCHAR_NOLOCK",
    "MIPS_PUTCHAR_NOLOCK_RET0",

    "MIPS_GETCHAR",
    "MIPS_GETCHAR_RET0",
    "MIPS_GETCHAR_RET1",
    "MIPS_GETCHAR_FRAME_ERR",
    "MIPS_GETCHAR_OVERRN_ERR",
    "MIPS_GETCHAR_PARITY_ERR",

    "NOLOCK_GETCHAR",
    "NOLOCK_GETCHAR_RET0",
    "NOLOCK_GETCHAR_RET1",

    "STOP_TX",
    "STOP_TX_RET0",

    "START_TX",
    "START_TX_RET0",
    "START_TX_RET1",
    "START_TX_ENABLE",
    
    "STOP_RX",
    "STOP_RX_RET0",
    "STOP_RX_RTS_DRT_DEASSERT",

    "START_RX",
    "START_RX_RET0",
    "START_RX_CTS_ON",
    "START_RX_CTS_OFF",
    "START_RX_DCD_ON",
    "START_RX_DCD_OFF",
    "START_RX_RTS_DTR_ASSERT",

    "CONFIG",
    "CONFIG_RET0",
    "CONFIG_RET1",
    "CONFIG_RET2",

    "IFLOW",
    "IFLOW_RET0",
    "IFLOW_RET1",
    "IFLOW_RET2",
    "IFLOW_RET3",
    "IFLOW_RTS_ASSERT",
    "IFLOW_RTS_DEASSERT",

    "BREAK",
    "BREAK_RET0"
};

void
dump_dport_info(__psint_t p1)
{
	struct dport *dp;
	int i;

	if (p1 < 0) {
		dp = (struct dport *)p1;
	} else {
		dp = du_minordev_to_dp((dev_t)p1);
	}
	qprintf("dport 0x%x (%d)\n", dp, dp->dp_index);
	qprintf("  wr1 0x%x wr3 0x%x wr4 0x%x wr5 0x%x wr15 0x%x\n",
		dp->dp_wr1, dp->dp_wr3, dp->dp_wr4,
		dp->dp_wr5, dp->dp_wr15);
	qprintf("  state 0x%x bitmask 0x%xn", dp->dp_state, dp->dp_bitmask);

	qprintf("  termio i=0%o, o=0%o, c=0%o, l=0%o, speed=%d\n",
		dp->dp_termio.c_iflag,
		dp->dp_termio.c_oflag,
		dp->dp_termio.c_cflag,
		dp->dp_termio.c_lflag,
		dp->dp_termio.c_ospeed);
	qprintf("  line %d ", dp->dp_line);
	qprintf("rq 0x%x wq 0x%x porthandler 0x%x\n",
		dp->dp_rq, dp->dp_wq, dp->dp_porthandler);
	qprintf("  cur_rbp 0x%x nxt_rbp 0x%x wbp 0x%x\n",
		dp->dp_cur_rbp, dp->dp_nxt_rbp, dp->dp_wbp);
	qprintf("  rsrv_blocked %d rsrv_blocked_consec_max %d\n",
		dp->dp_rsrv_blocked, dp->dp_rsrv_blk_consec_max);
	qprintf("  errors: framing %d parity %d overrun %d\n",
		dp->dp_fe, dp->dp_parity, dp->dp_over);
	qprintf(
"          rb_over %d tx_full %d rx_full %d rx_hiwat %d allocb %d\n",
		dp->dp_rb_over, dp->dp_full_tx_buf,
		dp->dp_full_rx_buf, dp->dp_hiwat_rx_buf,
		dp->dp_allocb_fail);
	qprintf("  port_lock 0x%x port_locktrips %d port_lockcpu %d\n",
		dports[(dp->dp_index/2)*2].dp_port_lock,
		dports[(dp->dp_index/2)*2].dp_port_locktrips,
		dports[(dp->dp_index/2)*2].dp_port_lockcpu);
	qprintf("  tx_buflock 0x%x rx_buflock 0x%x\n",
		dp->dp_tx_buflock, dp->dp_rx_buflock);
	qprintf("  rx_buf 0x%x rx_buf_rind %d (0x%x) rx_buf_wind %d (0x%x)\n",
		dp->dp_rx_buf,
		dp->dp_rx_rind, &dp->dp_rx_buf[dp->dp_rx_rind % RX_BUFSIZE],
		dp->dp_rx_wind, &dp->dp_rx_buf[dp->dp_rx_wind % RX_BUFSIZE]);
	qprintf("  tx_buf 0x%x tx_buf_rind %d (%d) tx_buf_wind %d (%d)\n",
		dp->dp_tx_buf,
		dp->dp_tx_rind, dp->dp_tx_rind % TX_BUFSIZE,
		dp->dp_tx_wind, dp->dp_tx_wind % TX_BUFSIZE);
	for (i = 0; i < TX_BUFSIZE; i++) {
		qprintf("  tx_buf[%d]: cmd %d arg %d ",
			i, dp->dp_tx_buf[i].cmd, dp->dp_tx_buf[i].arg);

#ifdef DEBUG
		qprintf(
"wseq %d wcpu %d rseq %d rcpu %d",
			dp->dp_tx_buf[i].wseq,
			dp->dp_tx_buf[i].wcpu,
			dp->dp_tx_buf[i].rseq,
			dp->dp_tx_buf[i].rcpu);
#endif /* DEBUG */
		qprintf("\n");
	}
#ifdef DEBUG
	for (i = 0; i < MAX_CHARCNT; i++) {
		qprintf(
"  charcnt[%d] %d\n",
			i, dp->dp_rx_charcnt[i]);
	}
	qprintf(
"  max_charcnt %d\n",
		dp->dp_rx_max_charcnt);
#endif /* DEBUG */
}

void
reset_dport_info(__psint_t p1)
{
	struct dport *dp;
	int i;

	if (p1 < 0) {
		dp = (struct dport *)p1;
	} else {
		dp = du_minordev_to_dp(p1);
	}

	dp->dp_fe = 0;
	dp->dp_parity = 0;
	dp->dp_over = 0;
	dp->dp_rb_over = 0;
	dp->dp_full_rx_buf = 0;
	dp->dp_full_tx_buf = 0;
	dp->dp_hiwat_rx_buf = 0;
	dp->dp_allocb_fail = 0;
	dp->dp_rsrv_blocked = 0;
#ifdef DEBUG
	for (i = 0; i < MAX_CHARCNT; i++)
		dp->dp_rx_charcnt[i] = 0;
	dp->dp_rx_max_charcnt = 0;
#endif /* DEBUG */
	qprintf("done\n");
}

#if DEBUG == 1

void
dump_dport_actlog(__psint_t p1)
{
	struct dport *dp;
	int i, j, act;
	static int longest_string, padding_len, shortest_string;

	if (longest_string == 0) {
	    padding_len = strlen(padding);
	    for(i = 0; i < DU_ACT_MAX; i++) {
		if ((j = strlen(duact_strings[i])) > longest_string)
		    longest_string = j;
		if (shortest_string == 0 || j < shortest_string)
		    shortest_string = j;
	    }
	    longest_string -= shortest_string;
	}
		
	if (p1 < 0) {
		dp = (struct dport *)p1;
	} else {
	    dp = du_minordev_to_dp(p1);
	}
	for (i = 0, j = dp->dp_al_wind % DU_ACTLOG_SIZE;
	     i < DU_ACTLOG_SIZE;
	     i++, j = ++j % DU_ACTLOG_SIZE) {
		act = dp->dp_actlog[j].dae_act;
		qprintf(
"duactlog[%d]: %s%s sec %d usec %d cpu %d info 0x%x\n",
			j,
			duact_strings[act],
			padding + padding_len - longest_string +
			strlen(duact_strings[act]),
			dp->dp_actlog[j].dae_sec,
			dp->dp_actlog[j].dae_usec,
			dp->dp_actlog[j].dae_cpu,
			dp->dp_actlog[j].dae_info);
	}
}

#endif /* DEBUG */

get_cons_dev(int which)
{
	printf("FIXME: hubuart.c get_cons_dev\n");
	return 0;
}

#endif /* SN && SABLE */

