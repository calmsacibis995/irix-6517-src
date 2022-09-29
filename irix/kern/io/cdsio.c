/* Copyright 1986-1991 Silicon Graphics Inc., Mountain View, CA.
 *
 * Streams driver for the Central Data serial board
 *
 * $Revision: 3.76 $
 */


#define NCD 4				/* total boards configured */
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/sysinfo.h"
#include "sys/edt.h"
#include "sys/vmereg.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/signal.h"
#include "sys/stream.h"
#include "sys/strids.h"
#include "sys/strmp.h"
#include "sys/stropts.h"
#include "sys/termio.h"
#include "sys/stty_ld.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/pio.h"
#include "sys/strsubr.h"
#include "sys/ddi.h"
#include "sys/capability.h"

extern time_t lbolt;

extern char cdnewcable;			/* !=0 if have good internal cables */
extern int cdsio_int_lim;
extern int cdsio_max_burst;
extern void tcgeta(queue_t *, mblk_t *, struct termio *);

#define CDNUMPORTS (NCD*CDUPC)		/* total ports possible */

#define CDUPC_LN 3
#define	CDUPC	(1<<CDUPC_LN)		/* 8 units per controller */

/* always meter on fast machines */
#if !defined(OS_METER)
#define OS_METER
#undef METER
#define METER(x) x
#endif

#ifdef OS_METER
static struct {
	int	retried_iacks;		/* retried interrupt ACKs */
	int	lost_mp_ints;		/* lost MP interrupts */
	int	delays;
	int	delay_lineno;
	int	retries;		/* slow board counts */
	int	over_delays;
	int	delay_ack, wait_ack;
	int	overlap_out;
	int	multi_cmds[CDUPC];	/* multiple bits in command byte */
	int	max_timo;		/* slowest command */
	int	max_timo_lineno;
	int	max_timo_cmd;
} cdsiometers;
#endif /* OS_METER */



/*
 * This structure is used to manage the software state of each
 * serial port.
 */
typedef struct cdport {
	int	cd_framingErrors;	/* # of framing errors */
	int	cd_overruns;		/* # of overrun errors */
	int	cd_allocb_fail;		/* losses due to allocb() failures */

	volatile struct device *cd_device;  /* port device address */
	volatile struct iodevice *cd_iodevice;	/* pointer to i/o port */

	struct	termio cd_termio;
#define cd_iflag cd_termio.c_iflag
#define cd_cflag cd_termio.c_cflag
#define cd_lflag cd_termio.c_lflag
#define cd_line cd_termio.c_line	/* 'line discipline' */
#define cd_cc cd_termio.c_cc

	tcflag_t cd_oldcflag;		/* previous mode bits */
	unchar	cd_mask0, cd_mask1;
	unchar	cd_cmask;		/* trim characters <8 bits wide */
	unchar	cd_rts_dtr;

	ushort	cd_rev;			/* firmware revision */

	unchar	cd_badint;		/* missed MP interrupt scheduling */

	unchar	cd_bit;			/* 1 << board port number */
	int	cd_minor;		/* minor device number */

	int	cd_empty;		/* input buffer emptying pointer */

	int	cd_xmitlimit;		/* output burst size */
	int	cd_xmit;		/* bytes waiting to be sent */

	ulong	cd_late_lbolt;		/* it started being late then */
	ulong	cd_state;		/* current state */
	int	cd_lineno;		/* line # of most recent command */

	queue_t	*cd_rq, *cd_wq;		/* our queues */
	mblk_t	*cd_rmsg, *cd_rmsge;	/* current input message */
	int	cd_rmsg_len;
	int	cd_rbsize;		/* recent message length */
	mblk_t	*cd_rbp;		/* current input buffer */

	toid_t	cd_tid;			/* (recent) output delay timer ID */
} CDPORT;

#define	SIOTYPE_PORT	0
#define	SIOTYPE_MODEM	1
#define	SIOTYPE_FLOW	2

typedef struct siotype_s {
    int			type;
    CDPORT	       *port;
} siotype_t;

/* bits in cd_state */
#define CD_ISOPEN	0x0000001	/* device is open */
#define CD_WOPEN	0x0000002	/* waiting for carrier */
#define CD_TIMEOUT	0x0000004	/* delaying */
#define CD_BREAK	0x0000008	/* breaking */
#define CD_TIMERS	(CD_TIMEOUT|CD_BREAK)
#define CD_LATER	0x0000010	/* retry-timer active */
#define CD_RETRY	0x0000020	/* waiting for the device */
#define CD_IACK_RETRY	0x0000040	/* delayed interrupt ACK */
#define CD_PARM_RETRY	0x0000080	/* retry parameter change */
#define CD_RETRIES	(CD_PARM_RETRY|CD_IACK_RETRY|CD_RETRY)
#define CD_BUF1		0x0000100	/* using output buffer #1 */
#define CD_TXSTOP	0x0000200	/* output stopped by received XOFF */
#define CD_LIT		0x0000400	/* have seen literal character */
#define CD_BLOCK	0x0000800	/* XOFF sent because input full */
#define CD_TX_TXON	0x0001000	/* need to send XON */
#define CD_TX_TXOFF	0x0002000	/* need to send XOFF */
#define CD_RTS		0x0004000	/* set RTS=1 */
#define CD_OFF_INT	0x0008000	/* expect DCD-off interrupt */
#define CD_ON_INT	0x0010000	/* expect DCD-on interrupt */
#define CD_MODEM	0x0040000	/* modem device */
#define CD_PROBED	0x0080000	/* device exists */

#define CD_REPRO_MASK	0xf000000	/* steps in reprograming */
#define CD_REPRO_INC	0x1000000
#define CD_REPRO_STEP(cdp) ((cdp)->cd_state & CD_REPRO_MASK)
#define CD_REPRO_ADV(cdp) ((cdp)->cd_state += CD_REPRO_INC)
#define CD_REPRO(n)	((n)*CD_REPRO_INC)


static CDPORT cdport[CDNUMPORTS];	/* state of each port */


#define MAX_CPS 3840			/* max port speed */

#define INT_LIM_SEC 1000		/* counts/second  (true value for
					 * the Central Data firmware) */
#define MIN_INT_SEC (MAX_CPS/(INBUF_LEN-MAX_CPS/20)) /* min interrupts/sec */
#define MAX_INT_DLY (INT_LIM_SEC/MIN_INT_SEC)
#define MIN_INT_DLY (INT_LIM_SEC/500)

#define HWM	(INBUF_LEN/2)		/* set high water mark to this */

#define B2W(hi,lo)	(((hi)<<8) + (lo))

#define DELAY_CHECK 20			/* check board after this many usec */
#define DELAY_CMD 2000			/* delay this long for commands */
#define DELAY_ACK 100			/* wait this long to ack interrupts */
#define PROBE_TIME (5000*1000)		/* 5 sec to probe the board */
#define DEAD_TIME 5			/* seconds before giving up command */


#define SGI_FIRMWARE 20000		/* versions > this are SGI */

#define OUTBUF_LEN 3056
#define INBUF_LEN 1024			/* assumed to be a power of 2 */

/*
 * Layout of the dual port ram
 */
struct	device {
	union {
	    struct {			/* old firmware */
		unchar  outBuf0[OUTBUF_LEN]; /* 0000-0BEF: out buffer 0 */
		unchar  outBuf1[OUTBUF_LEN]; /* 0BF0-17DF: out buffer 1 */
		unchar  inBuf[INBUF_LEN];    /* 17E0-1BDF: in buffer */
		unchar  errBuf[INBUF_LEN];   /* 1BE0-1FDF: error buffer */
	    } cd;
	    struct {			/* new firmware */
		ushort  inBuf[INBUF_LEN];    /* 0000-07ff: in buffer */
		unchar  outBuf0[OUTBUF_LEN]; /* 0800-13df: out buffer 0 */
		unchar  outBuf1[OUTBUF_LEN]; /* 14f0-1fdf: out buffer 1 */
	    } sg;
	} dat;
	unchar	pad0;			/* 1FE0: not used */
	unchar	cmd;			/* 1FE1: command code */
	ushort	cmdd;			/* 1FE2: command data word */
	unchar	intr;			/* 1FE4: !=0 for interrupt at end */
	unchar	cmdStatus;		/* 1FE5: command status */
	unchar	pad2;			/* 1FE6: */
	unchar	usartStatus;		/* 1FE7: status from uart */
	ushort	inputFillPtr;		/* 1FE8-1FE9: input filling pointer */
	ushort	inputEmptyPtr;		/* 1FEA-1FEB: input emptying ptr */
	ushort	inputOverflows;		/* 1FEC-1FED: input buf overflows */
	unchar	pad3;			/* 1FEE: */
	unchar	isf;			/* 1FEF: interrupt source flag */
	unchar	pad4;			/* 1FF0: */
	unchar	outputBusy;		/* 1FF1: output busy flag */
	unchar	pad5;			/* 1FF2: */
	unchar	outputStopped;		/* 1FF3: output stopped */
	unchar	pad6;			/* 1FF4: */
	unchar	parityError;		/* 1FF5: parity error */
	unchar	pad7;			/* 1FF6: */
	unchar	portIntStatusFlag;	/* 1FF7: port interrupt status flag */
	unchar	pad8[6];		/* 1FF8-1FFD: */
	ushort	firmwareRev;		/* 1FFE-1FFF: firmware revision */
};


/*
 * The command port and hostStatusPort overlay the memory space for port 7.
 */
struct	iodevice {
	char	padports[sizeof(struct device)*7];	/* ports 0-6 */
	char	pad[0x1FFC];		/* E000-FFFB: buffer for port 7 */
	unchar	pad0;			/* FFFC: */
	unchar	hostStatusPort;		/* FFFD: status port */
	unchar	pad1;			/* FFFE: */
	unchar	cmdPort;		/* FFFF: command port */
};


/* bits in the per-byte status.
 *	The boards do not like to talk when DCD is low, so we use DSR
 *	instead.
 */
#define	DCD_BIT		(cdnewcable ? SCC_DSR : SCC_DCD)
/* #define DSR_BIT		(cdnewcable ? SCC_DCD : SCC_DSR) */

/* bits in the errorBuf */
#define	ERRB_FRAMING	0x40		/* framing error */
#define	ERRB_OVERRUN	0x20		/* overrun error */
#define	ERRB_PARITY	0x10		/* parity error */
#define ERRB_NEW_BREAK	0x80
#define	ERRB_OLD_BREAK	0x01		/* BREAK input */
#define ERRB_BREAK	(ERRB_NEW_BREAK	| ERRB_OLD_BREAK)

/* bits in the usartStatus */
#define	SCC_DCD		0x04		/* DCD active */
#define	SCC_DSR		0x08		/* DSR active */
#define SCC_CTS		0x10		/* CTS active--new firmware only */


/* bits in isf */
#define	ISF_STATUS	0x04		/* status change interrupt */
#define	ISF_INPUT	0x02		/* input interrupt */
#define	ISF_OUTPUT	0x01		/* output done interrupt */
#define ISF_CMD		0x80		/* command finished */

/* bits in hostStatusPort */
#define	HSP_READY	0x01		/* board is ready for a command */

/* commands */
#define	CMD_SET_SCC	0x00		/* set scc register */
#define	CMD_SET_BAUD	0x01		/* set baud rate registers */
#define CMD_OFLUSH	0x02		/* flush output */
#define	CMD_SEND_0	0x04		/* send block 0 */
#define	CMD_SEND_1	0x05		/* send block 1 */
#define	CMD_SEND_BREAK	0x07		/* send break */
#define	CMD_SET_IDT	0x08		/* set input delay time */
#define	CMD_SET_HWM	0x09		/* set high water mark */
#define	CMD_SET_ISB0	0x0B		/* set interrupt on status bits 0 */
#define	CMD_SET_ISB1	0x0C		/* set interrupt on status bits 1 */
#define	CMD_SET_INPUT	0x0D		/* set input interrupt */
#define	CMD_SET_OUTPUT	0x0E		/* set output interrupt */
#define	CMD_SET_STATUS	0x0F		/* set status interrupt */
#define	CMD_SET_PARITY	0x10		/* set parity interrupt */
#define CMD_NUM		(CMD_SET_PARITY+1)

/* high bits in ipl for CMD_SET_{INPUT/OUTPUT/STATUS/PARITY} */
#define	INT_ENABLE	0x08		/* enable interrupts */
#define	INT_ROAK	0x80		/* reset on ack */

/* values for outputBusy */
#define OPB_IDLE	0		/* output idle */
#define OPB_BUSY	0xff		/* output busy */
#define OPB_VERY_BUSY	0xfe		/* output busy & 1 command pending */


/* bits in SCC write register 3 (used by CMD_SET_SCC) */
#define	WR3_BPC_5	0x00		/* 5 bits per character */
#define	WR3_BPC_7	0x40		/* 7 bits per character */
#define	WR3_BPC_6	0x80		/* 6 bits per character */
#define	WR3_BPC_8	0xC0		/* 8 bits per character */
#define	WR3_DCD		0x20		/* DCD & CTS control enable */
#define	WR3_RCV		0x01		/* receiver enabled */

/* bits in SCC write register 4 */
#define	WR4_CLK_1	0x00		/* clock rate x1 */
#define	WR4_CLK_16	0x40		/* clock rate x16 */
#define	WR4_CLK_32	0x80		/* clock rate x32 */
#define	WR4_CLK_64	0xC0		/* clock rate x64 */
#define	WR4_STOP_1	0x04		/* 1 stop bits */
#define	WR4_STOP_15	0x08		/* 1.5 stop bits */
#define	WR4_STOP_2	0x0C		/* 2 stop bits */
#define	WR4_EVEN	0x02		/* even parity */
#define	WR4_PARITY	0x01		/* enable parity */

/* bits in SCC write register 5 */
#define	WR5_DTR		0x80		/* DTR active */
#define	WR5_BPC_5	0x00		/* 5 bits per character */
#define	WR5_BPC_7	0x20		/* 7 bits per character */
#define	WR5_BPC_6	0x40		/* 6 bits per character */
#define	WR5_BPC_8	0x60		/* 8 bits per character */
#define	WR5_TXEN	0x08		/* TX enable */
#define	WR5_RTS		0x02		/* RTS enable */


/* decide when to turn on RTS or DTR */
#define	RTSBIT(cdp,b)
#define DTRTEST(cdp)	(B0 != ((cdp)->cd_cflag & CBAUD) \
			 && (0 != ((cdp)->cd_state & (CD_ISOPEN|CD_WOPEN))))
#define	DTRBIT(cdp,b)	(DTRTEST(cdp) ? (b) : 0)

/*
 * convert CFLAG's to write register bits
 */
static unchar wr3BitsPerChar[] = {WR3_BPC_5, WR3_BPC_6, WR3_BPC_7, WR3_BPC_8};
static unchar wr5BitsPerChar[] = {WR5_BPC_5, WR5_BPC_6, WR5_BPC_7, WR5_BPC_8};

/* macros to use lookup tables and whatnot */
#define	WR3BPC(cf)	wr3BitsPerChar[((cf) & CSIZE) >> 4]
#define	WR5BPC(cf)	wr5BitsPerChar[((cf) & CSIZE) >> 4]
#define	STOPBITS(cf)	(((cf) & CSTOPB) ? WR4_STOP_2 : WR4_STOP_1)
#define	PARITY(cf) \
	(((cf) & PARENB) \
	 ? (((cf) & PARODD) ? WR4_PARITY : (WR4_EVEN|WR4_PARITY)) \
	 : 0)

#define WR3FLOW(cdp)	(((cdp)->cd_cflag & CNEW_RTSCTS) ? WR3_DCD : 0)

#define	WR4BITS(cf)	(WR4_CLK_16 | STOPBITS(cf) | PARITY(cf))
#define	WR5BITS(cdp)	(DTRBIT(cdp,WR5_DTR) | WR5BPC((cdp)->cd_cflag)	\
			 | ((!((cdp)->cd_state & CD_TXSTOP)		\
			     || (cdp)->cd_rev<SGI_FIRMWARE) ? WR5_TXEN : 0) \
			 | (((cdp)->cd_state & CD_RTS) ? WR5_RTS : 0))

/* CBAUD to WR13 and WR12 values */
int baud_tbl[CBAUD+1] = {
	0x0006,				/* B0 --> hangup port */
	0x05FE,				/* B50 */
	0x03FE,				/* B75 */
	0x02B8,				/* B110 */
	0x0239,				/* B134.5 */
	0x01FE,				/* B150 */
	0x017E,				/* B200 */
	0x00FE,				/* B300 */
	0x007E,				/* B600 */
	0x003E,				/* B1200 */
	0x0029,				/* B1800, approximately */
	0x001E,				/* B2400 */
	0x000E,				/* B4800 */
	0x0006,				/* B9600 */
	0x0002,				/* B19200 */
	0x0000,				/* B38400 */
};


/* if any of these change in cflag, then the chip must be reprogrammed */
#define RE_PROG	(CREAD|CBAUD|CSIZE|CSTOPB|PARENB|PARODD|CNEW_RTSCTS)


/* stream stuff */
extern	struct stty_ld def_stty_ld;
static	struct module_info cdsio_info = {
	STRID_CD3608,			/* module ID */
	"CD3608",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* maximum packet size--infinite */
	128,				/* hi-water mark */
	16,				/* lo-water mark */
};

#define MIN_RMSG_LEN 4			/* minimum buffer size */
#define MAX_RMSG_LEN (INBUF_LEN*2)	/* largest msg allowed */
#define XOFF_RMSG_LEN 256		/* send XOFF here */
#define MAX_RBUF_LEN 4096

#define	CARRPRI	STIPRI			/* sleep for carrier at this */



int cdsiointr(int);
static int low_cdsiointr(CDPORT*);
static void cdCmdOut(CDPORT*);
static int cdLater(CDPORT*, int, int);
static void cdRetry(CDPORT*);
static void cdOut(CDPORT *cdp, unchar);
static void cdReceiveClear(CDPORT*);

static int cdRsrv(queue_t*);
static int cdOpen(queue_t*, dev_t *, int, int, struct cred *);
static void cdClose(queue_t*, int, struct cred *);


static void cdTimeout(CDPORT *, unchar, int);

static struct qinit cdrinit = {
	NULL, cdRsrv, cdOpen, (int (*)())cdClose, NULL, &cdsio_info, NULL
};

static void cdWput(queue_t*, mblk_t*);

static struct qinit cdwinit = {
	(int (*)())cdWput, NULL, NULL, NULL, NULL, &cdsio_info, NULL
};

int cdsiodevflag = 0;
struct streamtab cdsioinfo = {&cdrinit, &cdwinit, NULL, NULL};


/* macros to get at bits in dev
 *	They know that CDUPC and NUMMINOR are powers of 2,
 *	and that CDUPC<NUMMINOR.
 */
#define PORT_KLUDGE	2		/* Clover ports are misnumbered */
#define NUMMINOR	32		/* must be (2**n) */
#define	DEV_OFFSET	5
#define EXPORT_TO_BDPORT(p) (((p) + PORT_KLUDGE) & (CDUPC-1))
#define DEV_TO_EXPORT(dev)  (((dev) - DEV_OFFSET) & (NUMMINOR-1))
#define EXPORT_TO_DEV(ex)   ((ex) + DEV_OFFSET)
#define DEV_TO_BDPORT(dev)  EXPORT_TO_BDPORT(DEV_TO_EXPORT(dev))
#define DEV_TO_BOARD(dev)   ((((dev)-DEV_OFFSET) & (NUMMINOR-1)) >> CDUPC_LN)
#define CDP_TO_BOARD(cdp)   (DEV_TO_BOARD((cdp)->cd_minor))
#define BOARD_TO_EXPORT(b)  ((b) << CDUPC_LN)
#define BOARD_TO_MINOR(b,n) (BOARD_TO_EXPORT(b) + DEV_OFFSET + (n))

#define	MODEM(stype)		((stype)->type == SIOTYPE_MODEM || \
				 (stype)->type == SIOTYPE_FLOW)
#define FLOW_MODEM(stype)	((stype)->type == SIOTYPE_FLOW)

static	char *cderrors[] = {
	"no error",
	"bad eprom checksum",
	"bad scratchpad ram",
	"bad dual-port ram",
	"bad scc addressing",
	"bad usart test",
	"receiver interrupt failed",
};
#define	CDERRS		(sizeof(cderrors) / sizeof(char *))
#define cdErrorMsg(c)	((c) >= CDERRS ? "Unknown error" : cderrors[c])



#define BDBUSY(cdp) (0 == ((cdp)->cd_iodevice->hostStatusPort & HSP_READY))

/* see if the port is too busy to do something new */
#define BUSY_WAIT(cdp,r) (BDBUSY(cdp) && cdLater(cdp,r,__LINE__))



/* wait until the board is ready
 */
static int				/* 1=ready for work */
cdWait(CDPORT *cdp,
       int delay,
       int lineno)			/* delay this long */
{
	int timo;
	int res;

	res = 0;
	timo = 0;
	for (;;) {
		if (!BDBUSY(cdp)) {
			res = 1;
			break;
		}
		if (timo >= delay)
			break;
		timo += DELAY_CHECK;
		DELAY(DELAY_CHECK);
	}
#ifdef OS_METER
	if (timo > cdsiometers.max_timo) {
		cdsiometers.max_timo = timo;
		cdsiometers.max_timo_lineno= lineno;
		cdsiometers.max_timo_cmd = cdp->cd_device->cmd;
	}
#endif
	return res;
}


/* Start board on a new command
 *	Interrupts must be off.
 */
static void
cdCmd(CDPORT *cdp,
      unchar cmd,
      int cmdd,
      int lineno)
{
	int s;

	STR_LOCK_SPL(s);
	if (!cdWait(cdp,DELAY_CMD,lineno))	/* wait for previous command */
#ifdef DEBUG
     printf("ttyd%d: cmd 0x%x failed: data=%x cmdStatus=%d line=%d oline=%d\n",
		cdp->cd_minor, cdp->cd_device->cmd,
		cdp->cd_device->cmdd, cdp->cd_device->cmdStatus,
		lineno, cdp->cd_lineno)
#endif
		;

	cdp->cd_device->cmd= cmd;
	cdp->cd_device->cmdd = cmdd;
	cdp->cd_iodevice->cmdPort = cdp->cd_bit;
	cdp->cd_lineno = lineno;
	STR_UNLOCK_SPL(s);
}

/* Return 1 if the given port has carrier or does not care.
 */
static int
cdIsOn(CDPORT *cdp)
{
	return (0 != (cdp->cd_cflag & CLOCAL)
		|| 0 != (cdp->cd_device->usartStatus & DCD_BIT));
}


/* finish a BREAK or time-out
 */
static void
cdDelay(CDPORT *cdp)
{
	int s;

	STR_LOCK_SPL(s);
	cdp->cd_state &= ~CD_TIMERS;
	cdOut(cdp,1);
	STR_UNLOCK_SPL(s);
}


/* start a timer
 *	Interrupts must be off.
 *	bit == this kind of timer
 *	ticks == for this long
 */
static void
cdTimeout(CDPORT * cdp, unchar bit, int ticks)
{
	cdp->cd_state |= bit;
	cdp->cd_tid = STREAMS_TIMEOUT((strtimeoutfunc_t)cdDelay, cdp, ticks);
}


/* wake up the board after it has been sluggish
 */
static void
cdRetry(CDPORT *cdp)
{
	CDPORT *cdp0;
	int board, i;
	int s;

	STR_LOCK_SPL(s);
	METER(cdsiometers.retries++);

	board = CDP_TO_BOARD(cdp);
	cdp0 = &cdport[BOARD_TO_EXPORT(board)];

	/* the timer has fired */
	cdp0->cd_state &= ~CD_LATER;

	/* Give oldest delaying port 1st service by servicing the port
	 * that called timeout() first.
	 */
	for (i = 0; i < CDUPC; i++) {
		if (0 != (cdp->cd_state & CD_RETRIES)) {
			cdp->cd_state &= ~CD_RETRY;
			cdOut(cdp,0);
		}
		if (cdp == cdp0)
			cdp = cdp + CDUPC-1;
		else
			cdp--;
	}

	/* Try again to acknowledge an interrupt which we could not clear
	 * when it happened.
	 */
	if (cdp0->cd_badint || 0 != (cdp0->cd_state & CD_IACK_RETRY)) {
		METER(cdsiometers.retried_iacks++);
		cdp0->cd_badint = 0;
		(void)low_cdsiointr(cdp0);
	}
	else
		cdCmdOut(cdp0);
	STR_UNLOCK_SPL(s);
}


/* wait until the board can talk to us
 *	Interrupts must be off.
 */
static int				/* 0=have waited long enough */
cdLater(CDPORT *cdp,
	int rbit,
	int lineno)
{
	CDPORT *cdp0;


	cdp0 = &cdport[BOARD_TO_EXPORT(CDP_TO_BOARD(cdp))];

	/* request end-of-command interrupt from new firmware
	 */
	if (cdp0->cd_rev >= SGI_FIRMWARE) {
		cdp0->cd_device->intr = ISF_CMD;
		if (BDBUSY(cdp)) {
			cdp->cd_state |= rbit;
			METER(cdsiometers.delays++);
			METER(cdsiometers.delay_lineno = lineno);
			return 1;
		}
		cdp0->cd_device->intr = 0;
		return 0;
	}

	if (0 != (cdp->cd_state & CD_RETRIES)) {
		/* Give up if we have already waited long enough
		 */
		if (cdp->cd_late_lbolt < lbolt) {
			METER(cdsiometers.over_delays++);
			cdp->cd_late_lbolt = lbolt + DEAD_TIME*HZ;
			cdp->cd_state |= (CD_RETRY | rbit);
#ifdef DEBUG
	    printf("ttyd%d: timer for line=%d cmd=0x%x data=%x cmdStatus=%d\n",
				cdp->cd_minor, cdp->cd_lineno,
				cdp->cd_device->cmd, cdp->cd_device->cmdd,
				cdp->cd_device->cmdStatus);
#endif
			cdp0->cd_state |= CD_LATER;
			(void)STREAMS_TIMEOUT((strtimeoutfunc_t)cdRetry,
							cdp0, 1);
			METER(cdsiometers.delays++);
			return 0;
		}
	} else {
		cdp->cd_late_lbolt = lbolt + DEAD_TIME*HZ;
	}
	cdp->cd_state |= (CD_RETRY | rbit);

	/* start a timeout for this board if not already running */
	if (!(cdp0->cd_state & CD_LATER)) {
		cdp0->cd_state |= CD_LATER;
		(void)STREAMS_TIMEOUT((strtimeoutfunc_t)cdRetry, cdp0, 1);
		METER(cdsiometers.delays++);
	}
	return BDBUSY(cdp);
}


/* Turn DCD interrupt on or off
 *	Interrupts must be off.
 */
static int			/* 1=board was too busy */
cdSetDCDint(CDPORT *cdp)
{
	unchar mask0, mask1;

	/* new firmware is always correct
	 */
	if (cdp->cd_rev >= SGI_FIRMWARE)
		return 0;


	mask0 = (cdp->cd_state & CD_OFF_INT) ? DCD_BIT : 0;

	/* turn interrupts off before turning anything on
	 */
	if (mask0 == 0 && 0 != cdp->cd_mask0) {
		if (BUSY_WAIT(cdp,CD_PARM_RETRY))
			return 1;
		cdCmd(cdp, CMD_SET_ISB0, 0, __LINE__);
		cdp->cd_mask0 = 0;
	}

	mask1 = (cdp->cd_state & CD_ON_INT) ? DCD_BIT : 0;
	if (mask1 != cdp->cd_mask1) {
		if (BUSY_WAIT(cdp,CD_PARM_RETRY))
			return 1;
		cdCmd(cdp, CMD_SET_ISB1, mask1, __LINE__);
		cdp->cd_mask1 = mask1;
	}

	if (mask0 != cdp->cd_mask0) {
		if (BUSY_WAIT(cdp,CD_PARM_RETRY))
			return 1;
		cdCmd(cdp, CMD_SET_ISB0, mask0, __LINE__);
		cdp->cd_mask0 = mask0;
	}

	return 0;
}


/* tell board what to do about RTS, DTR and TX-enable
 */
static int				/* 1=board was too busy */
cdRTS_DTR(CDPORT *cdp)
{
	int rts_dtr;

	rts_dtr = WR5BITS(cdp);
	if (rts_dtr != cdp->cd_rts_dtr) {
		if (BUSY_WAIT(cdp,CD_PARM_RETRY))
			return 1;
		if (!(rts_dtr & WR5_DTR)) {
			cdp->cd_xmit = 0;	/* flush output at DTR off */
			if (cdp->cd_rev >= SGI_FIRMWARE
			    && OPB_IDLE != cdp->cd_device->outputBusy) {
				cdCmd(cdp, CMD_OFLUSH, 0, __LINE__);
				if (BUSY_WAIT(cdp,CD_PARM_RETRY))
					return 1;
			}
		}
		cdCmd(cdp, CMD_SET_SCC, B2W(rts_dtr,5), __LINE__);
		cdp->cd_rts_dtr = rts_dtr;
	}
	return 0;
}


/* Turn off RTS and send XOFF
 */
static void
cdRTSXoff(CDPORT *cdp,
	  int doit)			/* 1=start output if possible */
{
	if (cdp->cd_cflag & CNEW_RTSCTS) {
		cdp->cd_state &= ~CD_RTS;
		(void)cdRTS_DTR(cdp);
	}

	if ((cdp->cd_iflag & IXOFF)
	    && !(cdp->cd_state & CD_BLOCK)) {
		cdp->cd_state |= CD_TX_TXOFF;
		cdp->cd_state &= ~CD_TX_TXON;
		cdOut(cdp,doit);	/*  send XOFF */
	}
}


/* if ready to receive, do XON */
static void
cdRTSXon(CDPORT *cdp)
{
	if (!(cdp->cd_state & CD_RTS)) {
		cdp->cd_state |= CD_RTS;
		(void)cdRTS_DTR(cdp);
	}
	if (cdp->cd_state & CD_BLOCK) {
		cdp->cd_state |= CD_TX_TXON;
		cdOut(cdp,1);
	}
}


/* Clear receiver input buffer
 *	Interrupts must be off here.
 */
static void
cdReceiveClear(CDPORT *cdp)
{
	uint fillPtr, n;

	/* get consistent pointer value since new firmware does not
	 * lock the dual-port RAM
	 */
	do {
		n = cdp->cd_device->inputFillPtr;
		fillPtr = cdp->cd_device->inputFillPtr;
	} while (n != fillPtr);

	if (cdp->cd_rev >= SGI_FIRMWARE) {
		cdp->cd_empty = ((fillPtr<<7)+(fillPtr>>9)) % INBUF_LEN;
	} else {
		cdp->cd_device->inputEmptyPtr = fillPtr;
	}
}

/*
 * flush output timers
 *	Interrupts must have been made safe here.
 */
static int				/* 1=too busy */
cdFlushOutput(CDPORT *cdp,
	      int force)
{
	if (cdp->cd_state & CD_TIMEOUT) {   /* forget stray timeout */
		untimeout(cdp->cd_tid);
		cdp->cd_state &= ~CD_TIMEOUT;
	}

	cdp->cd_xmit = 0;
	if (cdp->cd_rev >= SGI_FIRMWARE
	    && OPB_IDLE != cdp->cd_device->outputBusy) {
		if (!force && BUSY_WAIT(cdp,CD_PARM_RETRY))
			return 1;
		cdp->cd_device->outputBusy = OPB_VERY_BUSY;
		cdCmd(cdp, CMD_OFLUSH, 0, __LINE__);

		/* XXX  The board should generate an interrupt at the end
		 * of the flush command to restart output.  If the
		 * firmware is changed again before release, this code
		 * should be removed.
		 */
		BUSY_WAIT(cdp,CD_PARM_RETRY);
	}
	return 0;
}

/*
 * flush input
 *	interrupts must be safe here
 */
static void
cdFlushInput(CDPORT *cdp)
{
	freemsg(cdp->cd_rmsg);
	cdp->cd_rmsg = 0;
	cdp->cd_rmsg_len = 0;
	freemsg(cdp->cd_rbp);
	cdp->cd_rbp = 0;
	cdReceiveClear(cdp);

	cdRTSXon(cdp);
}



#define HANGUP(cdp) (void)cdSetParams(cdp, cdp->cd_cflag & ~CBAUD, 0)

/* Set the port parameters
 *	Interrupts must be off.
 */
static int				/* 1=board was too busy */
cdSetParams(CDPORT *cdp,
	    tcflag_t cf,			/* new control flags */
	    struct termio *tp)
{
	int baud;
	int max;

	baud = cf & CBAUD;
	if (0 != tp)
		cdp->cd_termio = *tp;
	cdp->cd_cflag = cf;

	/* If killing the port, we must wait until output is finished.
	 *   This is so that we do not drop DTR or RTS before we have
	 *   done all output.  A common cabling convention is to connect
	 *   CTS to either RTS or DTR.  If CTS drops before we are finished,
	 *   we will not be able to reopen the port, because the open
	 *   function will be unable to reprogram the UART.
	 *
	 * It is enough to drop DTR.  We do not have to change the speed of
	 *	the port.
	 */
	if (!DTRTEST(cdp)) {		/* if killing the port, */
		cdp->cd_state &= ~(CD_TX_TXON|CD_TX_TXOFF|CD_BLOCK|CD_RTS
				   | CD_ON_INT | CD_REPRO_MASK);

		if (cdRTS_DTR(cdp)
		    || cdSetDCDint(cdp)
		    || cdFlushOutput(cdp,0))
			return 1;

		cdp->cd_state &= ~CD_PARM_RETRY;
		return 0;
	}

	/* Bang on the chip. If the board is busy, wait to set it up.
	 */
	while (0 != ((cf ^ cdp->cd_oldcflag) & RE_PROG)) {
		if (BUSY_WAIT(cdp,CD_PARM_RETRY))
			return 1;

		/* Disable the receiver.  Set transmitter state.
		 * Then enable the receiver.
		 */
		switch (CD_REPRO_STEP(cdp)) {
		case CD_REPRO(0):
			cdCmd(cdp,CMD_SET_SCC,
			      B2W(WR3BPC(cf)|WR3FLOW(cdp),3),
			      __LINE__);
			CD_REPRO_ADV(cdp);
			continue;
		case CD_REPRO(1):
			cdCmd(cdp, CMD_SET_SCC, B2W(WR4BITS(cf),4), __LINE__);
			CD_REPRO_ADV(cdp);
			continue;
		case CD_REPRO(2):
			if (0 != ((cf ^ cdp->cd_oldcflag) & CBAUD)) {
				cdCmd(cdp, CMD_SET_BAUD, baud_tbl[baud],
				      __LINE__);
				cdp->cd_mask0 = ~DCD_BIT;
				cdp->cd_mask1 = ~DCD_BIT;
			}
			CD_REPRO_ADV(cdp);
			continue;
		case CD_REPRO(3):
			if (0 != (cf & CREAD)
			    && B0 != (cf & CBAUD)
			    && 0 != (cdp->cd_state & (CD_ISOPEN|CD_WOPEN)))
				cdCmd(cdp,CMD_SET_SCC,
				      B2W(WR3BPC(cf)|WR3FLOW(cdp)|WR3_RCV,3),
				      __LINE__);
			CD_REPRO_ADV(cdp);
			continue;
		case CD_REPRO(4):
			if (cdp->cd_rev < SGI_FIRMWARE)
				cdCmd(cdp, CMD_SET_HWM, HWM, __LINE__);
			CD_REPRO_ADV(cdp);
			continue;
		default:
			if (cdsio_int_lim <= 1)		/* no divide by 0 */
				max = MAX_INT_DLY;
			else
				max = INT_LIM_SEC/cdsio_int_lim;
			if (max > MAX_INT_DLY)
				max = MAX_INT_DLY;
			if (max < MIN_INT_DLY)
				max = MIN_INT_DLY;
			cdCmd(cdp, CMD_SET_IDT, max, __LINE__);
			cdReceiveClear(cdp);
			cdp->cd_state &= ~CD_REPRO_MASK;
			cdp->cd_oldcflag = cf;	/* port is now programmed */
		}
	}

	/* Compute a good output burst size.  If we do not need to
	 *	stop output quickly, we can use most of the buffer.
	 *
	 *	These values should be about 0.1 seconds when we might
	 *	have to stop and 2 seconds othewise.  The latter is to
	 *	ensure that all of the data gets out before closing the port.
	 *
	 *	We can disable the transmitter with the SGI firmware, and
	 *	so can always use big buffers.
	 */
	max = (MAX_CPS*2*2)/(baud_tbl[baud]+2);
	if (cdp->cd_rev<SGI_FIRMWARE
	    && ((cdp->cd_iflag & IXON) || (cdp->cd_lflag & ISIG)))
		max /= 10;
	if (cdsio_max_burst != 0
	    && max > cdsio_max_burst)
		max = cdsio_max_burst;
	max /= 2;
	if (max >= OUTBUF_LEN)
		max = OUTBUF_LEN-1;
	if (max == 0)
		max = 1;
	cdp->cd_xmitlimit = max;

	cdp->cd_cmask = (0x3f << (((cf & CSIZE) >> 4) + (0 != (cf & PARENB)))
			 | 0xf);

	if (0 != cdp->cd_rbp)
		cdp->cd_state |= CD_RTS;
	if (cdRTS_DTR(cdp) || cdSetDCDint(cdp))
		return 1;
	cdp->cd_state &= ~CD_PARM_RETRY;
	return 0;
}

/*
 * open a serial port
 */
static int
cdOpen(queue_t *rq,
       dev_t *devp,
       int flag,
       int sflag,
       struct cred *crp)
{
	CDPORT *cdp;
	queue_t *wq = WR(rq);
	int s;
	int error;
	siotype_t *siotype;

	if (sflag)			/* only a simple stream driver */
		return (ENXIO);

	/* validate device */
	if (!dev_is_vertex(*devp))
	    return(ENXIO);

	siotype = (siotype_t*)device_info_get(*devp);
	cdp = siotype->port;
	
	STR_LOCK_SPL(s);
	if (!(cdp->cd_state & (CD_ISOPEN|CD_WOPEN))) {	/* on the 1st open */
		tcflag_t cflag;

		cflag = def_stty_ld.st_cflag;
		cdp->cd_oldcflag = 0;	/* be sure to set CTS */
		cdp->cd_state &= ~(CD_TXSTOP|CD_LIT|CD_BLOCK
				      |CD_TX_TXON|CD_TX_TXOFF
				      |CD_MODEM);
		if (MODEM(siotype)) {
			cdp->cd_state |= CD_MODEM;
			cflag &= ~CLOCAL;
			if (FLOW_MODEM(siotype))
				cflag |= CNEW_RTSCTS;
		}

		for (;;) {
			cdp->cd_state &= ~(CD_OFF_INT|CD_ON_INT);
			if (!(cflag & CLOCAL))
				cdp->cd_state |= (CD_WOPEN|CD_RTS|CD_ON_INT);
			else
				cdp->cd_state |= (CD_WOPEN|CD_RTS);
			(void)cdSetParams(cdp,cflag,&def_stty_ld.st_termio);

			/* finish if we already have carrier */
			if (cdIsOn(cdp)) {
				cdp->cd_state &= ~CD_ON_INT;
				if (!(cflag & CLOCAL))
					cdp->cd_state |= CD_OFF_INT;
				(void)cdSetParams(cdp,cflag,0);
				break;
			}

			if (0 != (flag & (FNONBLK|FNDELAY)))
				break;

			if (sleep((caddr_t)cdp, CARRPRI)) {
				cdp->cd_state &= ~(CD_ISOPEN|CD_WOPEN|CD_RTS
						   |CD_ON_INT|CD_OFF_INT
						   |CD_MODEM);
				HANGUP(cdp);
				STR_UNLOCK_SPL(s);
				return (EINTR);
			}
			/* loop on DCD glitch */
		}

		rq->q_ptr = (caddr_t)cdp;   /* connect device to stream */
		wq->q_ptr = (caddr_t)cdp;
		cdp->cd_wq = wq;
		cdp->cd_rq = rq;
		cdp->cd_state &= ~CD_WOPEN;
		cdp->cd_state |= (CD_ISOPEN|CD_RTS);
		(void)cdSetParams(cdp,cflag|CREAD,0);
		cdReceiveClear(cdp);

		if (error = strdrv_push(rq, "stty_ld", devp, crp)) {
			cdp->cd_state &= ~(CD_ISOPEN|CD_WOPEN|CD_RTS
					   |CD_ON_INT|CD_OFF_INT
					   |CD_MODEM);
			HANGUP(cdp);
			STR_UNLOCK_SPL(s);
			return (error);
		}

	} else {
		/*
		 * You cannot open two streams to the same device.  The
		 * structure can only point to one of them.  Therefore, you
		 * cannot open two different minor devices that are synonyms
		 * for the same device.
		 * XXX eliminate this restriction?
		 */
		if (cdp->cd_rq == rq) {
			ASSERT(cdp->cd_wq == wq);
			ASSERT(cdp->cd_rq->q_ptr == (void *)cdp);
			ASSERT(cdp->cd_wq->q_ptr == (void *)cdp);
		} else {
			STR_UNLOCK_SPL(s);
			return (EBUSY);
		}
	}
	STR_UNLOCK_SPL(s);
	return (0);		/* return successfully */
}


/* close a port
 *	This will shut the port down only after any current output is
 *	finished.  The stream head will have waited at least a while for
 *	any streams messages to be finished.  If the board refused to
 *	do an output request for an earlier buffer, the hangup request
 *	will wait for the board to accept the request.  The hangup
 *	request will also wait for the board to finish an output request
 *	which it did accept.
 */
/* ARGSUSED */
static void
cdClose(queue_t *rq, int flag, struct cred *crp)
{
	register CDPORT *cdp;
	register int s;

	cdp = (CDPORT *)rq->q_ptr;
	if (0 == cdp)
		return;			/* port was not really open */

	STR_LOCK_SPL(s);
	ASSERT(cdp >= &cdport[0] && cdp->cd_rq == rq);

	(void)cdFlushOutput(cdp,0);
	cdFlushInput(cdp);
	cdp->cd_rq = 0;
	cdp->cd_wq = 0;
	cdp->cd_state &= ~(CD_ISOPEN|CD_WOPEN|CD_RTS|CD_ON_INT|CD_OFF_INT
			   |CD_MODEM);
	if (0 != (cdp->cd_cflag & HUPCL)) {
		HANGUP(cdp);
	} else {
		(void)cdSetDCDint(cdp);
	}
	STR_UNLOCK_SPL(s);
}

/*
 * get a new buffer
 */
static mblk_t *				/* current input buffer */
cdGetNewbp(CDPORT *cdp,
	   uint pri,
	   unchar mrg)			/* 1=merge all buffers we have */
{
	int size;
	mblk_t *bp;
	mblk_t *rbp;

	/* if current buffer is has lots of room,
	 *	or if upstream is constipated and we are not merging,
	 * then we do not need a new buffer.
	 */
	rbp = cdp->cd_rbp;
	if ((rbp != 0
	     && rbp->b_rptr >= rbp->b_wptr)
	    || (!mrg			/* or upstream constipated */
		&& cdp->cd_rmsg_len > MAX_RMSG_LEN)) {
		bp = 0;			/* do not need another buffer */

	} else {
		/* get another buffer, but always keep room to grow
		 *	This helps prevent deadlock.
		 */
		size = (cdp->cd_rbsize += (cdp->cd_rbsize+3)/4);
		if (size > MAX_RBUF_LEN) {
			size = cdp->cd_rbsize = MAX_RBUF_LEN;
		} else if (size < MIN_RMSG_LEN) {
			size = MIN_RMSG_LEN;
		}
		do {
			bp = allocb(size, pri);
		} while (bp == 0
			 && BPRI_HI == pri
			 && (size >>= 2) >= MIN_RMSG_LEN);
	}

	/* if we have no old buffer, take the new one, if any.
	 * if we have a new buffer, then combine old buffer.
	 */
	if (rbp == 0) {
		cdp->cd_rbp = rbp = bp;
	} else if (0 != bp
		   || rbp->b_wptr >= rbp->b_datap->db_lim) {
		str_conmsg(&cdp->cd_rmsg, &cdp->cd_rmsge, rbp);
		cdp->cd_rmsg_len += (rbp->b_wptr - rbp->b_rptr);
		cdp->cd_rbp = rbp = bp;
	}

	if (0 == rbp)			/* stop input if overflowing */
		cdRTSXoff(cdp,0);
	return rbp;
}


/* send a bunch of 1 or more characters up the stream
 */
static int				/* 0=cannot send */
cdSendUp(queue_t *rq,
	 CDPORT *cdp)
{
	mblk_t *bp;

	if (!canput(rq->q_next)) {	/* quit if upstream congested */
		if (cdp->cd_rmsg_len >= XOFF_RMSG_LEN)
			cdRTSXoff(cdp,1);
		return 0;
	}

	/* merge current buffer with accumulated message to send as one
	 * gulp up stream.
	 */
	(void)cdGetNewbp(cdp, BPRI_LO, 1);

	bp = cdp->cd_rmsg;
	if (bp != 0) {
		cdp->cd_rmsg = 0;
		if (cdp->cd_rmsg_len > cdp->cd_rbsize)
			cdp->cd_rbsize = cdp->cd_rmsg_len;
		else
			cdp->cd_rbsize = (cdp->cd_rmsg_len+cdp->cd_rbsize)/2;
		cdp->cd_rmsg_len = 0;
		putnext(rq, bp);	/* send the message */
	}
	return 1;
}


/* send a bunch of 1 or more characters up the stream
 *	This should be invoked only because a message could not be sent
 *	upwards by the interrupt, and things have now drained.
 */
static int
cdRsrv(queue_t *rq)
{
	int s;
	CDPORT *cdp;

	STR_LOCK_SPL(s);
	cdp = (CDPORT *)rq->q_ptr;
	ASSERT(cdp->cd_rq == rq);
	ASSERT(cdp >= &cdport[0] && cdp <= &cdport[CDNUMPORTS-1]);
	ASSERT(cdp->cd_state & CD_ISOPEN);

	if (cdSendUp(rq,cdp)
	    && 0 != cdp->cd_rbp) {
		cdRTSXon(cdp);		/* if ready to receive, do XON */
	}
	STR_UNLOCK_SPL(s);
	return 0;
}

static int cdInterruptIoctl(CDPORT *cdp, mblk_t *bp);

/*
 * output 'put' function
 *	Start the output if we like the message.
 */
static void
cdWput(queue_t *wq,
       mblk_t *bp)
{
	CDPORT *cdp;
	struct iocblk *iocp;
	unchar c;
	int i;
	int board;
	int s;

	cdp = (CDPORT *)wq->q_ptr;
	if (!cdp) {
		sdrv_error(wq,bp);	/* quit now if not open */
		return;
	}

	STR_LOCK_SPL(s);
	ASSERT(cdp->cd_wq == wq);
	ASSERT(cdp >= &cdport[0] && cdp <= &cdport[CDNUMPORTS-1]);
	ASSERT(cdp->cd_state & CD_ISOPEN);

	switch (bp->b_datap->db_type) {

	case M_FLUSH:
		c = *bp->b_rptr;
		sdrv_flush(wq,bp);
		if (c & FLUSHW) {
			cdp->cd_state &= ~CD_TXSTOP;
			(void)cdFlushOutput(cdp,1);
			(void)cdRTS_DTR(cdp);
			cdOut(cdp,1);	/* restart output */
		}
		if (c & FLUSHR)
			cdFlushInput(cdp);
		break;

	case M_DATA:
	case M_DELAY:
		putq(wq, bp);
		cdOut(cdp,1);
		break;

	case M_IOCTL:
		iocp = (struct iocblk*)bp->b_rptr;
		switch (iocp->ioc_cmd) {
		case TCXONC:
			ASSERT(iocp->ioc_count == sizeof(int));
			switch (*(int*)(bp->b_cont->b_rptr)) {
			case 0:		/* stop output */
				cdp->cd_state |= CD_TXSTOP;
				(void)cdRTS_DTR(cdp);
				break;
			case 1:		/* resume output */
				cdp->cd_state &= ~CD_TXSTOP;
				(void)cdRTS_DTR(cdp);
				cdOut(cdp,1);
				break;
			case 2:
				cdp->cd_state &= ~CD_RTS;
				(void)cdRTS_DTR(cdp);
				if (!(cdp->cd_state & CD_BLOCK)) {
					cdp->cd_state |= CD_TX_TXOFF;
					cdp->cd_state &= ~CD_TX_TXON;
					cdOut(cdp,1);
				}
				break;
			case 3:
				cdRTSXon(cdp);
				break;
			default:
				iocp->ioc_error = EINVAL;
				break;
			}
			bp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = 0;
			qreply(wq, bp);
			break;

		case TCGETA: /* XXXrs - don't wait until output? */
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			cdInterruptIoctl(cdp, bp);
			break;

		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			/*
			 * On modem ports, only the super-user can change
			 * CLOCAL.  Otherwise, fail silently.
			 */
			if (cdp->cd_state & CD_MODEM) {
				struct termio *tp = STERMIO(bp);
				tcflag_t cflag = tp->c_cflag;
				if (0 != ((cdp->cd_cflag ^ cflag) & CLOCAL) &&
				    !_CAP_CRABLE(iocp->ioc_cr,
						 CAP_DEVICE_MGT)) {
					cflag &= ~CLOCAL;
					cflag |= cdp->cd_cflag & CLOCAL;
					tp->c_cflag = cflag;
				}
			}

			if (iocp->ioc_cmd == TCSETA) {
				cdInterruptIoctl(cdp, bp);
			} else {
				putq(wq, bp);
				cdOut(cdp,1);
			}
			break;

		case TIOCMGET:
			/* DTR is true when open and CBAUD != 0 */
			i = (cdp->cd_cflag & CBAUD) ? TIOCM_DTR : 0;
			if (cdp->cd_state & CD_RTS)
				i |= TIOCM_RTS;
			c = cdp->cd_device->usartStatus;
			if (c & DCD_BIT)
				i |= TIOCM_CD;
		/* since only this board can support CTS & DSR, don't 
		 *	if (c & DSR_BIT)
		 *		i |= TIOCM_DSR;
		 */
			if (cdp->cd_rev >= SGI_FIRMWARE && (c & SCC_CTS))
				i |= TIOCM_CTS;
			*(int*)(bp->b_cont->b_rptr) = i;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			break;

		case TCSBRK:
			putq(wq, bp);
			cdOut(cdp,1);
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

	board = CDP_TO_BOARD(cdp);
	cdp = &cdport[BOARD_TO_EXPORT(board)];
	if (cdp->cd_state & CD_IACK_RETRY)
		(void)low_cdsiointr(cdp);
	STR_UNLOCK_SPL(s);
}


/*
 * interrupt-process an IOCTL
 *	This function processes those IOCTLs that must be done by the output
 *	interrupt.  The board should be known to be quiet.
 */
static int				/* 1=board is now busy */
cdInterruptIoctl(CDPORT *cdp, mblk_t *bp)
{
	struct iocblk *iocp;
	int isbusy;
	struct termio *tp;
	tcflag_t cflag;

	isbusy = 0;
	iocp = (struct iocblk *)bp->b_rptr;
	switch (iocp->ioc_cmd) {
	case TCSBRK:
		if (*(int *)bp->b_cont->b_rptr == 0) {
			/* tell board to send a break */
			if (cdp->cd_rev < SGI_FIRMWARE) {
				/* The manual says BREAK may last 350 msec, but
				 * some versions are slower.  Delay to avoid
				 * talking to the board until it is finished.
				 */
				cdTimeout(cdp, CD_BREAK, (HZ*500)/1000+1);
			}
			cdCmd(cdp,CMD_SEND_BREAK,0, __LINE__);
			isbusy = 1;
		}
		bp->b_datap->db_type = M_IOCACK;
		break;

	case TCSETAF:
	case TCSETA:
	case TCSETAW:
		tp = STERMIO(bp);
		cflag = tp->c_cflag;
		if (!(cflag & CLOCAL))
			cdp->cd_state |= CD_ON_INT;
		(void)cdSetParams(cdp, cflag, tp);
		if (cdIsOn(cdp)) {
			cdp->cd_state &= ~CD_ON_INT;
			if (!(cflag & CLOCAL))
				cdp->cd_state |= CD_OFF_INT;
			(void)cdSetParams(cdp, cflag, 0);
		}

		if (iocp->ioc_cmd == TCSETAF) {
			cdFlushInput(cdp);
			(void)putctl1(cdp->cd_rq->q_next, M_FLUSH, FLUSHR);
		}
		bp->b_datap->db_type = M_IOCACK;
		break;


	case TCGETA:
		tcgeta(cdp->cd_wq, bp, &cdp->cd_termio);
		return (isbusy);

#if defined(DEBUG) && !defined(lint)
	default:
		ASSERT(0);
#endif
	}

	iocp->ioc_count = 0;
	putnext(cdp->cd_rq, bp);

	return (isbusy);
}


/* Output Interrupt Service Routine
 */
static void
cdOut(CDPORT *cdp,
      unchar doit)			/* 1=start output if possible */
{
	mblk_t *wbp, *next_wbp;

	volatile unchar *obuf;
	unchar *p;
	int w, i, mask;
	int xmitlimit, xmit;
	unchar ostat;


	/* finish any postponed modem control work
	 */
	if ((cdp->cd_state & CD_PARM_RETRY)
	    && cdSetParams(cdp,cdp->cd_cflag,0))
		return;
	if (!(cdp->cd_state & CD_ISOPEN))
		return;

	/* quit if too busy for more
	 */
	ostat = cdp->cd_device->outputBusy;
	if (0 != (cdp->cd_state & CD_TIMERS))
		return;

again:;
	if (cdp->cd_rev < SGI_FIRMWARE) {
		if (cdp->cd_state & CD_BUF1)
			obuf = &cdp->cd_device->dat.cd.outBuf1[cdp->cd_xmit];
		else
			obuf = &cdp->cd_device->dat.cd.outBuf0[cdp->cd_xmit];
	} else {
		if (cdp->cd_state & CD_BUF1)
			obuf = &cdp->cd_device->dat.sg.outBuf1[cdp->cd_xmit];
		else
			obuf = &cdp->cd_device->dat.sg.outBuf0[cdp->cd_xmit];
	}
	xmitlimit = cdp->cd_xmitlimit - cdp->cd_xmit;
	xmit = 0;
	if (OPB_VERY_BUSY == ostat)
		xmitlimit = 0;

	/* send XON or XOFF even if we have received XOFF, provided
	 * we have buffer space
	 */
	if (cdp->cd_state & CD_TX_TXON) {   /* send XON or XOFF */
		if (xmitlimit > 0 ) {
			cdp->cd_state &= ~(CD_TX_TXON|CD_TX_TXOFF|CD_BLOCK);
			if (DTRTEST(cdp)) {
				*obuf++ = cdp->cd_cc[VSTART];
				xmit++;
			}
		}
	} else if (cdp->cd_state & CD_TX_TXOFF) {
		if (xmitlimit > 0 ) {
			cdp->cd_state &= ~CD_TX_TXOFF;
			if (DTRTEST(cdp)) {
				cdp->cd_state |= CD_BLOCK;
				*obuf++ = cdp->cd_cc[VSTOP];
				xmit++;
			}
		}
	}

	/* if we have received XOFF, do not send any data
	 */
	if (cdp->cd_state & CD_TXSTOP)
		xmitlimit = 0;

	wbp = getq(cdp->cd_wq);
	while (wbp != 0) {
		switch (wbp->b_datap->db_type) {
		case M_DATA:
			if (!DTRTEST(cdp)) {
				freemsg(wbp);
				wbp = getq(cdp->cd_wq);
				continue;
			}

			/* do not let i become < 0
			 */
			if (xmit > xmitlimit) {
				putbq(cdp->cd_wq, wbp);
				goto exit;
			}

			p = wbp->b_rptr;
			i = wbp->b_wptr - p;
			xmit += i;
			if (xmit > xmitlimit) {
				i -= (xmit - xmitlimit);
				xmit = xmitlimit;
			}
			mask = cdp->cd_cmask * 0x101;
			if (0 != ((__psint_t)obuf & 1) && i > 0) {
				*obuf++ = *p++ & mask;
				--i;
			}
			while (i >= 2) {	/* 0.5 VME accesses/byte */
				w = (*p++) << 8;
				w |= *p++;
				*(ushort*)obuf = w & mask;
				obuf += 2;
				i -= 2;
			}
			if (i > 0) {
				*obuf++ = *p++ & mask;
			}

			wbp->b_rptr = p;
			if (p < wbp->b_wptr) {
				putbq(cdp->cd_wq, wbp);
				goto exit;
			}

			next_wbp = wbp->b_cont;
			freeb(wbp);
			wbp = next_wbp;
			break;


		case M_DELAY:
			if (xmit+cdp->cd_xmit != 0
			    || ostat != OPB_IDLE
			    || 0 != (cdp->cd_state & CD_TIMERS)
			    || BUSY_WAIT(cdp,0)) {
				putbq(cdp->cd_wq, wbp);
				goto exit;
			}
			i = *(int*)wbp->b_rptr;
			freemsg(wbp);
			if (0 != i) {
				cdTimeout(cdp,CD_TIMEOUT, i);
				goto exit;
			}
			wbp = getq(cdp->cd_wq);
			break;

		case M_IOCTL:
			if (xmit+cdp->cd_xmit != 0
			    || ostat != OPB_IDLE
			    || 0 != (cdp->cd_state & CD_TIMERS)
			    || BUSY_WAIT(cdp,0)) {
				putbq(cdp->cd_wq, wbp);
				goto exit;
			}
			if (cdInterruptIoctl(cdp, wbp))
				goto exit;
			wbp = getq(cdp->cd_wq);
			break;

		default:
			cmn_err(CE_PANIC, "ttyd%d: bad msg %d",
				cdp->cd_minor, wbp->b_datap->db_type);
		}
	}
exit:;
	cdp->cd_xmit += xmit;

	/* Bang the board if ok & needed.
	 */
	if (doit && ostat == OPB_IDLE && 0 != cdp->cd_xmit) {
		cdCmdOut(cdp);

		/* To reduce use of big streams buffers and output latency,
		 * copy as much data as we can to the board.
		 */
		if (xmit >= xmitlimit) {
			ostat = OPB_BUSY;
			goto again;
		}
	}

	/* keep the queue from being empty until the port is quiet,
	 * so data is not lost at close.
	 */
	if (0 == cdp->cd_wq->q_first
	    && (cdp->cd_xmit != 0 || ostat != OPB_IDLE)
	    && 0 != (wbp = allocb(0,BPRI_HI)))
		putbq(cdp->cd_wq,wbp);
}


/* start output commands for several ports
 *	should be called with interrupts off
 */
static void
cdCmdOut(CDPORT *cdp)			/* any port on the board */
{
	int i;
	int mask;
	METER(int n = 0);

	cdp = &cdport[BOARD_TO_EXPORT(CDP_TO_BOARD(cdp))];

	/* skip busy boards for a while */
	if (BDBUSY(cdp) && cdLater(cdp,0,__LINE__))
		return;

	mask = 0;
	for (i = 0; i < CDUPC; i++, cdp++) {
		if (cdp->cd_xmit == 0)
			continue;
		if (OPB_IDLE == cdp->cd_device->outputBusy) {
			cdp->cd_device->outputBusy = OPB_BUSY;
		} else {
			if (cdp->cd_rev < SGI_FIRMWARE)
				continue;
			cdp->cd_device->outputBusy = OPB_VERY_BUSY;
			METER(cdsiometers.overlap_out++);
		}

		cdp->cd_device->cmd = (0 != (cdp->cd_state & CD_BUF1)
				       ? CMD_SEND_1 : CMD_SEND_0);
		cdp->cd_device->cmdd = cdp->cd_xmit;
		SYSINFO.outch += cdp->cd_xmit;
		cdp->cd_xmit = 0;
		cdp->cd_state ^= CD_BUF1;
		cdp->cd_lineno = __LINE__;
		mask |= cdp->cd_bit;
		METER(n++);
	}
	if (0 != mask) {
		cdp -= CDUPC;
		cdp->cd_iodevice->cmdPort = mask;
		METER(cdsiometers.multi_cmds[n-1]++);
	}
}


/* slow and hopefully infrequently used function to put characters
 *	somewhere where they will go up stream.
 */
static mblk_t *
cdStuff(CDPORT *cdp,
	unchar c)
{
	mblk_t *bp;

	if (0 == (bp = cdp->cd_rbp)
	    && 0 == (bp = cdGetNewbp(cdp, BPRI_HI, 0))) {
		cdp->cd_allocb_fail++;;
		return bp;
	}
	*bp->b_wptr = c;
	if (++bp->b_wptr >= bp->b_datap->db_lim) {
		/* send buffer when full */
		bp = cdGetNewbp(cdp, BPRI_LO, 0);
	}
	return bp;
}

/*
 * Handle receive interrupt
 */
static void
cdReceiveInterrupt(CDPORT *cdp)
{
	volatile struct device *d;
	uint c, err;
	uint emptyPtr, fillPtr, n;
	mblk_t *bp;
	int needsend;

	if (0 == (bp = cdp->cd_rbp))	/* try for a buffer, outside loop */
		bp = cdGetNewbp(cdp, BPRI_HI, 0);

	d = cdp->cd_device;

	/* get a consistent pointer value since the new firmware does not
	 * lock the dual-port RAM
	 */
	do {
		n = d->inputFillPtr;
		fillPtr = d->inputFillPtr;
	} while (n != fillPtr);
	if (cdp->cd_rev < SGI_FIRMWARE) {
		emptyPtr = d->inputEmptyPtr;
	} else {
		fillPtr = ((fillPtr<<7) + (fillPtr>>9)) % INBUF_LEN;
		emptyPtr = cdp->cd_empty;
	}

	needsend = 0;
	n = 0;
	while (fillPtr != emptyPtr) {
		n++;
		if (cdp->cd_rev >= SGI_FIRMWARE) {
			c = d->dat.sg.inBuf[emptyPtr];
			err = c & (0xff & ~ERRB_OLD_BREAK);
			c = (c >> 8) & cdp->cd_cmask;
			emptyPtr = (emptyPtr+1) % INBUF_LEN;
		} else {
			c = d->dat.cd.inBuf[emptyPtr] & cdp->cd_cmask;
			err = d->dat.cd.errBuf[emptyPtr] & ~ERRB_NEW_BREAK;
			emptyPtr = (emptyPtr+1) % INBUF_LEN;
		}

		/*
		 * start or stop output (if permitted) when we
		 * get XOFF or XON
		 */
		if (cdp->cd_iflag & IXON) {
			unchar cs = c;

			if (cdp->cd_iflag & ISTRIP)
				cs &= 0x7f;
			if ((cdp->cd_state & CD_TXSTOP)
			    && (cs == cdp->cd_cc[VSTART]
				|| ((IXANY & cdp->cd_iflag)
				    && (cs != cdp->cd_cc[VSTOP]
					|| cdp->cd_line == LDISC0)))) {

				/* restart output, if none pending */
				cdp->cd_state &= ~CD_TXSTOP;
				(void)cdRTS_DTR(cdp);
				cdOut(cdp,0);
				if (cs == cdp->cd_cc[VSTART])
					continue;
			} else if (cdp->cd_state & CD_LIT) {
				cdp->cd_state &= ~CD_LIT;
			} else if (cs == cdp->cd_cc[VSTOP]) {
				cdp->cd_state |= CD_TXSTOP;
				(void)cdRTS_DTR(cdp);
				continue;
			} else if (cs == cdp->cd_cc[VSTART]) {
				continue;	/* ignore extra control-Qs */
			} else if (cs == cdp->cd_cc[VLNEXT]
				   && LDISC0 != cdp->cd_line) {
				cdp->cd_state |= CD_LIT;
			}
		}

		if (err & (ERRB_BREAK | ERRB_FRAMING
			   | ERRB_OVERRUN | ERRB_PARITY)) {
			if (err & ERRB_OVERRUN)
				cdp->cd_overruns++;

			/* if there was a BREAK	*/
			if (err & ERRB_BREAK) {
				if (cdp->cd_iflag & IGNBRK)
					continue;	/* ignore it if ok */
				if (cdp->cd_iflag & BRKINT) {
					cdFlushInput(cdp);
					(void)putctl1(cdp->cd_rq->q_next,
						      M_FLUSH,FLUSHRW);
					(void)putctl1(cdp->cd_rq->q_next,
						      M_PCSIG, SIGINT);
					continue;
				}
				if (cdp->cd_iflag & PARMRK) {
					(void)cdStuff(cdp, 0377);
					bp = cdStuff(cdp, 0);
				}
				c = '\0';

			} else if (IGNPAR & cdp->cd_iflag) {
				continue;

			} else if (!(INPCK & cdp->cd_iflag)) {
				/* ignore input parity errors if asked */

			} else if (err & (ERRB_PARITY|ERRB_FRAMING)) {
				if (err & ERRB_FRAMING)
					cdp->cd_framingErrors++;
				if (cdp->cd_iflag & PARMRK) {
					(void)cdStuff(cdp, 0377);
					bp = cdStuff(cdp, 0);
				} else
					c = '\0';
			}
		} else if (cdp->cd_iflag & ISTRIP) {
			c &= 0x7f;
		} else if (c == 0377 && (cdp->cd_iflag & PARMRK)) {
			bp = cdStuff(cdp, 0377);
		}

		if (!bp) {
			/* drop character if no buffer available */
			cdp->cd_allocb_fail++;
			continue;
		}
		*bp->b_wptr = c;
		if (++bp->b_wptr >= bp->b_datap->db_lim) {
			/* send buffer upstream when full */
			bp = cdGetNewbp(cdp, BPRI_HI, 0);
		}
		needsend = 1;
	}
	if (cdp->cd_rev < SGI_FIRMWARE) {
		d->inputEmptyPtr = emptyPtr;
	} else {
		cdp->cd_empty = emptyPtr;
	}
	SYSINFO.rawch += n;

	/* Try to send upstream whatever we have accumulated.
	 */
	if (needsend
	    && !cdSendUp(cdp->cd_rq,cdp)
	    && cdp->cd_rmsg_len >= XOFF_RMSG_LEN)
		cdRTSXoff(cdp,0);
}


/*
 * Carrier on or off interrupt
 */
void
cdStatusInterrupt(CDPORT *cdp)
{
	if (cdIsOn(cdp)) {		/* DCD-on interrupt */
		if ((cdp->cd_state & CD_ON_INT)) {
			cdp->cd_state &= ~CD_ON_INT;
			if (0 != ((cdp)->cd_state & (CD_ISOPEN|CD_WOPEN))) {
				if (!(cdp->cd_cflag & CLOCAL))
					cdp->cd_state |= CD_OFF_INT;
				if (cdp->cd_state & CD_WOPEN)
					wakeup((caddr_t)cdp);
			}
		}


	} else {			/* off-interrupt */
		if ((cdp->cd_state & CD_OFF_INT)) {
			cdp->cd_state &= ~CD_OFF_INT;
			if (!(cdp->cd_state & CD_ISOPEN)) {
				if (cdp->cd_state & CD_WOPEN)
					wakeup((caddr_t)cdp);
			} else {
				flushq(cdp->cd_wq, FLUSHDATA);
				(void)putctl1(cdp->cd_rq->q_next,
					      M_FLUSH,FLUSHW);
				(void)putctl(cdp->cd_rq->q_next, M_HANGUP);
			}
			HANGUP(cdp);
		}
	}

	(void)cdSetDCDint(cdp);
}



int
cdsiointr(int board)
{
	CDPORT *cdp0;

	cdp0 = &cdport[BOARD_TO_EXPORT(board)];

	if (!(cdp0->cd_state & CD_PROBED))  /* interrupt is too early */
		return 0;

	if (0 > streams_interrupt((strintrfunc_t)low_cdsiointr,cdp0,0,0)) {
		cdp0->cd_badint = 1;
		METER(cdsiometers.lost_mp_ints++);
	}
	return 1;
}


static int
low_cdsiointr(CDPORT *cdp0)
{
	int p;
	int needout;
	unchar intflg;

	/* process interrupts for each port
	 */
	needout = 0;
	if (cdp0->cd_rev >= SGI_FIRMWARE) {
		intflg = cdp0->cd_device->isf;
		cdp0->cd_device->isf = 0;

		for (p = 0; p < CDUPC; cdp0++, p++) {
			cdStatusInterrupt(cdp0);
			if (DTRTEST(cdp0) && (cdp0->cd_cflag & CREAD))
				cdReceiveInterrupt(cdp0);
			cdOut(cdp0,0);
			needout += cdp0->cd_xmit;
		}
	} else {
		intflg = 0;
		for (p = 0; p < CDUPC; cdp0++, p++) {
			intflg |= cdp0->cd_device->isf;
			cdp0->cd_device->isf = 0;

			cdStatusInterrupt(cdp0);
			if (DTRTEST(cdp0) && (cdp0->cd_cflag & CREAD)) {
				cdReceiveInterrupt(cdp0);
			} else {
				cdReceiveClear(cdp0);
			}
			cdOut(cdp0,0);
			needout += cdp0->cd_xmit;
		}
	}
	cdp0 -= CDUPC;

	/* count interrupts for sar(1) */
	if (intflg & ISF_INPUT)
		SYSINFO.rcvint++;
	if (intflg & ISF_OUTPUT)
		SYSINFO.xmtint++;
	if (intflg & ISF_STATUS)
		SYSINFO.mdmint++;
#ifdef OS_METER
	if (intflg & ISF_CMD)
		cdsiometers.retries++;
#endif

	if (needout != 0)		/* do delayed output */
		cdCmdOut(cdp0);

	cdp0->cd_state &= ~CD_IACK_RETRY;
	if (cdp0->cd_rev < SGI_FIRMWARE) {
		/* Try to acknowledge the interrupt on an old board until
		 * we decide the board is too busy to respond.
		 */
		p = 0;
		for (;;) {
			if (!BDBUSY(cdp0)) {
				cdp0->cd_iodevice->cmdPort = 0;	/* do it */
				break;
			}
			/* If the board is too busy, make a note to try
			 * again later and give up for now.
			 */
			if ((p += DELAY_CHECK) >= DELAY_ACK) {
				(void)cdLater(cdp0, CD_IACK_RETRY,__LINE__);
				METER(cdsiometers.delay_ack++);
				break;
			}
			METER(cdsiometers.wait_ack++);
			DELAY(DELAY_CHECK);
		}
	}

	return 1;
}

static void
mkhwg(void)
{
    char name[12];
    vertex_hdl_t ttydir_vhdl;
    /*REFERENCED*/
    graph_error_t rc;
    int x, total;
    siotype_t *siotype;
#if DEBUG
    siotype_t *last_siotype;
#endif

    /* create containing directory /hw/ttys */
    rc = hwgraph_path_add(hwgraph_root, "ttys", &ttydir_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    /* look for valid ports and create the siotype structs in a clump */
    total = 0;

    /* ordinary ttys have 3 types, d m and f */
    for(x = 0; x < CDNUMPORTS ; x++)
	if (cdport[x].cd_state & CD_PROBED)
	    total += 3;

    siotype = (siotype_t*)
	kmem_alloc(sizeof(siotype_t) * total, KM_NOSLEEP);
#if DEBUG
    last_siotype = siotype + total;
#endif

    /* look for ordinary serial ports and create the nodes.  unit is
     * in range [0-3] with 4 ports per unit, thus 16 ports max, oddly
     * offset by 1.
     */
    for(x = 0; x < CDNUMPORTS; x++) {
	static char types[] = {SIOTYPE_PORT, SIOTYPE_MODEM, SIOTYPE_FLOW};
	vertex_hdl_t dev_vhdl;
	int type;

	if (!(cdport[x].cd_state & CD_PROBED))
	    continue;

	/* create the device nodes pointing to this port */
	for(type = 0; type < 3; type++) {
	    siotype->type = types[type];
	    siotype->port = &cdport[x];
	    sprintf(name, "tty%c%d", "dmf"[type], EXPORT_TO_DEV(x));
	    rc = hwgraph_char_device_add(ttydir_vhdl, name, "cdsio", &dev_vhdl);
	    ASSERT(rc == GRAPH_SUCCESS);
	    hwgraph_chmod(dev_vhdl, HWGRAPH_PERM_TTY);
	    hwgraph_fastinfo_set(dev_vhdl, (arbitrary_info_t)siotype);

	    siotype++;
	}
    }
    
    ASSERT(siotype == last_siotype);
}

/*
 * Probe the given Central Data board
 */
void cdsioedtinit(struct edt *e)
{
	CDPORT *cdp;
	volatile struct device *d;
	volatile struct iodevice *iod;
	int i;
	unchar ipl, vec;
	int board;
	int s;
	vme_intrs_t *intrs = e->e_bus_info;
	piomap_t *piomap;


	board = e->e_ctlr;

	/* map I/O address space */
	piomap = pio_mapalloc (e->e_bus_type, e->e_adap, &e->e_space[0], 
		PIOMAP_FIXED,"cdsio");
	if (piomap == 0) 
		return;
	e->e_base = pio_mapaddr(piomap, e->e_iobase);

	d = (volatile struct device *)e->e_base;
	iod = (volatile struct iodevice *)d;

	STR_LOCK_SPL(s);
	/*
	 * Probe board by writing a zero.  This also has the side effect
	 * of stopping the board diagnostics.
	 * Modified to use badaddr to read status for the probe, since timing
	 * on bus error writes is suspect.  Shouldn't hurt to leave the
	 * wbadaddr write since it should only be executed if read succeeds.
	 */
	if ((badaddr(&iod->hostStatusPort, sizeof(iod->hostStatusPort))) ||
	    (wbadaddr(&iod->cmdPort, sizeof(iod->cmdPort)))) {
		STR_UNLOCK_SPL(s);
		if (showconfig)
			printf("cdsio%d: missing\n", board);
		pio_mapfree(piomap);
		return;
	}

	/* Since we are a STREAMS device, we must make sure STREAMS is up */
	strinit();

	/* wait for board to come up to a clean state */
	i = 0;
	while ((iod->hostStatusPort & HSP_READY) == 0) {
		if (i >= PROBE_TIME) {
			STR_UNLOCK_SPL(s);
			printf("cdsio%d: reset timed out, err=%s (0x%x)\n",
			       board, cdErrorMsg(d->dat.cd.outBuf0[0]),
			       d->dat.cd.outBuf0[0]);
			pio_mapfree(piomap);
			return;
		}
		i += DELAY_CHECK;
		DELAY(DELAY_CHECK);
	}

	cdp = &cdport[BOARD_TO_EXPORT(board)];

	for (i = 0; i < CDUPC; i++, cdp++) {
		cdp->cd_minor = BOARD_TO_MINOR(board,i);
		cdp->cd_bit = 1 << EXPORT_TO_BDPORT(i);
		cdp->cd_device = d + EXPORT_TO_BDPORT(i);
		cdp->cd_iodevice = iod;
		cdp->cd_rev = d->firmwareRev;
		cdp->cd_mask0 = ~DCD_BIT;	/* reset the port */
		cdp->cd_mask1 = ~DCD_BIT;
		cdp->cd_rts_dtr = ~WR5BITS(cdp);
		cdp->cd_state = CD_PROBED | CD_PARM_RETRY;
		cdp->cd_late_lbolt = lbolt + DEAD_TIME*HZ;
	}
	cdp -= CDUPC;

	/* program interrupt vectors and levels */
	if (intrs->v_vec == 0)
		intrs->v_vec = (unsigned char)vme_ivec_alloc(e->e_adap);
	if (intrs->v_vec == 255) {
		cmn_err(CE_WARN,"cdsio%d: no interrupt vector\n",board);
		pio_mapfree(piomap);
		return;		
	}
	vme_ivec_set(e->e_adap, intrs->v_vec, cdsiointr, board);
	vec = intrs->v_vec;
	ipl = intrs->v_brl | INT_ENABLE | INT_ROAK;
	cdCmd(cdp, CMD_SET_INPUT, B2W(ipl, vec), __LINE__);
	if (cdp->cd_rev < SGI_FIRMWARE) {
		cdCmd(cdp, CMD_SET_OUTPUT, B2W(ipl, vec), __LINE__);
		cdCmd(cdp, CMD_SET_STATUS, B2W(ipl, vec), __LINE__);
		cdCmd(cdp, CMD_SET_PARITY, B2W((ipl & ~INT_ENABLE), vec),
		      __LINE__);
	}

	/* Start timer to clear ports by calling cdSetParams.
	 * Also fake an initial interrupt for the new firmware.
	 */
	cdp->cd_state |= CD_IACK_RETRY;
	(void)STREAMS_TIMEOUT((strtimeoutfunc_t)cdRetry, cdp, HZ);
	STR_UNLOCK_SPL(s);

	if (showconfig)
		printf("cdsio%d: firmware revision %u, ipl %u, vec 0x%x\n",
		       board, cdp->cd_rev, intrs->v_brl, vec);

	/* add to the inventory table */
	add_to_inventory(INV_SERIAL, INV_CDSIO, board, 0, cdp->cd_rev);

	mkhwg();
}
