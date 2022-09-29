#if IP20 || IP22 || IP26 || IP28 || EVEREST

#ident	"saio/io/z8530cons.c: $Revision: 1.107 $"

/* Zilog 85130 driver */

#include <sys/cpu.h>
#if EVEREST
#include <sys/EVEREST/epc.h>
#endif /* EVEREST */
#include <sys/sbd.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/z8530.h>
#include <arcs/folder.h>
#include <tty.h>
#include <saio.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>
#include <arcs/hinv.h>
#include <arcs/errno.h>

#if IP20 || EVEREST
#define NUMDUARTS		3
#define	CONSOLE_PORT		0
#define	REMOTE_PORT0		1
#define	KEYBD_PORT		2
#define	MOUSE_PORT		3
#define	REMOTE_PORT1		4
#define	REMOTE_PORT2		5
#endif
#if IP22 || IP26 || IP28
#define NUMDUARTS		1
#define CONSOLE_PORT		0
#define REMOTE_PORT0		1
#endif

#define	NPORTS			NUMDUARTS*2

struct z8530info {
    volatile u_char	*cntrl;
    volatile u_char	*data;
    int			si_brate;
    int			wflag;
    struct device_buf	si_ibuf;
} z8530ports[NPORTS];

#if EVEREST
lock_t du_lock[NUMDUARTS];	/* per-duart lock on cntrl regs */
#endif /* EVEREST */

#if IP22 || IP26 || IP28
extern uint	_hpc3_write2;	       /* software copy of write2 reg */
#endif 

extern int	sk_sable;

#define DELAY(x) us_delay(x)

int 		_z8530init(IOBLOCK *);
int 		_z8530ioctl(IOBLOCK *);
int 		_z8530open(IOBLOCK *);
int 		_z8530strategy(IOBLOCK *, int);
static void	configure_port(int);
int 		consgetc(int);
void 		consputc(u_char, int);
#if IP20
int		enet_carrier_on(void);
void		reset_enet_carrier(void);
#endif /* IP20 */
static int 	map_baud(int);
static int 	next_baud(int);
char 		*check_serial_baud_rate(char*);
void 		z8530cons_errputc(u_char);
void		get_baud(int);

static int	nports;		/* number of ports, and init flag */

volatile int z8530cons_was_polled;	/* console port activity indicator */


#if MOUSE_PORT
static int mscnt;		/* count of opens -- only allow one open */
#endif

int
_z8530init(IOBLOCK *iob)
{
    static LONG pass;
    register int i;
#if IP22
    char *c = getenv("console");
#endif
#ifdef IP20
    volatile u_char *cntrl;
#endif

    if (sk_sable)
	return(0);

    if (iob) {
	if (iob->Count == pass)
	    return(0);
	pass = iob->Count;
    }
    else if (!nports) {			/* _z8530init(0) for early init */
#if IP20
	nports = (NPORTS-2);
#else
	nports = NPORTS;
#endif
    }

#if IP20
    /* use the last 2 ports as regular tty ports, not APPLE ports */
    *(u_char *)PHYS_TO_K1(PORT_CONFIG) |= PCON_SER0RTS | PCON_SER1RTS;
#elif IP22 || IP26 || IP28
	_hpc3_write2 |= UART0_ARC_MODE | UART1_ARC_MODE;
	*(volatile int *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;
#endif

    for (i = 0; i < nports; i++)
	bzero(&z8530ports[i].si_ibuf, sizeof(struct device_buf));

    /*
     * initialize the needed addresses.  
     * The layout is:
     *  Channel   Logical device	unit	Comments
     *	1B (1A)	tty(0)		0	diagnostic console.
     *	1A (1B)	tty(1)		1	remote
     *	0A	gfx(0)		2	gfx device - keyboard.
     *	0B	gfx(1)		3	gfx device - mouse.
     *	2B (2A)	tty(4)		4	remote
     *	2A (2B)	tty(5)		5	remote
     *
     * The number in parentheses is the channel used in Everest
     * systems.  
     *
     * The devices gfx(0) and gfx(1) can be considered aliases to
     * the devices tty(2) and tty(3).
     */

#ifdef KEYBD_PORT
    z8530ports[KEYBD_PORT].cntrl = (u_char *)DUART0A_CNTRL;
    z8530ports[KEYBD_PORT].data = (u_char *)DUART0A_DATA;
#endif

#ifdef MOUSE_PORT
#ifdef IP20
    cntrl = 
#endif
    z8530ports[MOUSE_PORT].cntrl = (u_char *)DUART0B_CNTRL;
    z8530ports[MOUSE_PORT].data = (u_char *)DUART0B_DATA;
#endif

#ifdef EVEREST
    z8530ports[CONSOLE_PORT].cntrl = (u_char *)DUART2A_CNTRL;
    z8530ports[CONSOLE_PORT].data = (u_char *)DUART2A_DATA;

    z8530ports[REMOTE_PORT0].cntrl = (u_char *)DUART1B_CNTRL;
    z8530ports[REMOTE_PORT0].data = (u_char *)DUART1B_DATA;

    z8530ports[REMOTE_PORT1].cntrl = (u_char *)DUART1A_CNTRL;
    z8530ports[REMOTE_PORT1].data = (u_char *)DUART1A_DATA;

    z8530ports[REMOTE_PORT2].cntrl = (u_char *)DUART2B_CNTRL;
    z8530ports[REMOTE_PORT2].data = (u_char *)DUART2B_DATA;

    /* We've already been doing output under Everest, so we
     * want to make sure that FIFO is clear before we mess with
     * the control registers.  Setting wflag accomplishes this.
     */
    z8530ports[CONSOLE_PORT].wflag = 1;
#else
/* IP20 has no d2 support.  IP22 does it in z8530.  IP26 and IP28 do it
 * in IP2x.c.
 */
#ifdef IP22
    if (c != 0 && ((*c == 'd' && *(c+1) == '2') || *c == 'r')) {
	/* Console on port 2 tty(1) */
	z8530ports[1].cntrl = (u_char *)DUART1B_CNTRL;
	z8530ports[1].data = (u_char *)DUART1B_DATA;

	z8530ports[0].cntrl = (u_char *)DUART1A_CNTRL;
	z8530ports[0].data = (u_char *)DUART1A_DATA;
    }
    else
#endif
    {
	/* Console of port 1 tty(0) */
	z8530ports[0].cntrl = (u_char *)DUART1B_CNTRL;
	z8530ports[0].data = (u_char *)DUART1B_DATA;

	z8530ports[1].cntrl = (u_char *)DUART1A_CNTRL;
	z8530ports[1].data = (u_char *)DUART1A_DATA;
    }

#if IP20
    /* for etherent carrier detection */
    WR_CNTRL(cntrl, WR9, 0x00)		/* no interrupt to the MIPS */
    WR_CNTRL(cntrl, WR15, WR15_DCD_IE | WR15_Z85130_EXTENDED)	/* latch */
    WR_CNTRL(cntrl, WR1, WR1_EXT_INT_ENBL)	/* carrier signal */
#endif

#ifdef REMOTE_PORT1
    z8530ports[REMOTE_PORT1].cntrl = (u_char *)DUART2B_CNTRL;
    z8530ports[REMOTE_PORT1].data = (u_char *)DUART2B_DATA;

    z8530ports[REMOTE_PORT2].cntrl = (u_char *)DUART2A_CNTRL;
    z8530ports[REMOTE_PORT2].data = (u_char *)DUART2A_DATA;
#endif
#endif /* EVEREST */

    /* Get baud rate and configure port so the port doesn't have
     * to be opened to be at the correct baud rate.
     */
    for (i = 0; i < nports; i++) {
	get_baud(i);
	configure_port(i);
    }

#ifdef MOUSE_PORT
    ms_install();			/* initialize mouse pseudo driver */
    mscnt = 0;				/* any mouse access in internal */
#endif

    return(0);
}


/* get_baud - get the baud rate for the port */
void
get_baud(int unit)
{
    char			*abaud = 0;

    switch (unit) {
    case CONSOLE_PORT:
#ifdef EVEREST
	if (EVCFGINFO->ecfg_debugsw & VDS_DEFAULTS)
	    abaud = "9600";
	else
	    abaud = getenv("dbaud");
#elif IP22 || IP26 || IP28
	abaud = getenv("console");
	if (abaud && !strcmp(abaud, "d2")) {
		if (!(abaud = getenv("rbaud")))
			abaud = "19200";
	} else 
		abaud = getenv("dbaud");
	abaud = check_serial_baud_rate(abaud);
#else
	abaud = getenv("dbaud");
#endif /* EVEREST */
	break;

    case REMOTE_PORT0:
#ifdef EVEREST
	if ((EVCFGINFO->ecfg_debugsw & VDS_DEFAULTS) ||
	    !(abaud = getenv("rbaud")))
	    abaud = "19200";
#elif IP22 || IP26 || IP28
	abaud = getenv("console");
	if (abaud && !strcmp(abaud, "d2")) 
		abaud = getenv("dbaud");
	else if (!(abaud = getenv("rbaud")))
		abaud = "19200";
	abaud = check_serial_baud_rate(abaud);
#else
	if (!(abaud = getenv("rbaud")))
		abaud = "19200";
#endif /* EVEREST */
	break;

#ifdef REMOTE_PORT1
    case REMOTE_PORT1:
    case REMOTE_PORT2:
	abaud = "9600";
	break;
#endif

#ifdef KEYBD_PORT
    case KEYBD_PORT:
	abaud = "600";
	break;
#endif

#ifdef MOUSE_PORT
    case MOUSE_PORT:
	abaud = "4800";
	break;
#endif
    }

    if (!abaud || *atob(abaud, &z8530ports[unit].si_brate) ||
        (map_baud(z8530ports[unit].si_brate) <= 0)) {
	if (unit == CONSOLE_PORT)
	    z8530ports[unit].si_brate = 9600;
	else
	    z8530ports[unit].si_brate = 1200;
	if (environ)
	    printf("\nIllegal %s value: %s\n", unit ? "rbaud" : "dbaud", abaud);
    }

}

/* _z8530open -- initialize DUARTs */
int
_z8530open(IOBLOCK *iob)
{
    int unit = (int)iob->Controller;

    if ((unit < 0) || (unit >= nports))
	return(iob->ErrorNumber = ENXIO);

    /*  Serial and sgi keyboard only handle console(0), which in this case
     * is the same as not specifying console(0).
     */
    if (iob->Flags & F_ECONS)
	    return(iob->ErrorNumber = EINVAL);

#ifdef MOUSE_PORT
    if (unit == MOUSE_PORT) {
	if (mscnt)
	    return(iob->ErrorNumber = EBUSY);
	mscnt++;
    }
#endif

    iob->Flags |= F_SCAN;

    get_baud(unit);
    configure_port(unit);

    return(ESUCCESS);
}



/* _z8530strategy -- perform io */
int
_z8530strategy(IOBLOCK *iob, int func)
{
    register int		c;
    register struct device_buf	*db;
    int				ocnt = (int)iob->Count;
    char 			*addr = (char *)iob->Address;

    if (func == READ) {
	db = &z8530ports[iob->Controller].si_ibuf;

	while (iob->Count > 0) {
	    while (c = consgetc((int)iob->Controller)) {
#ifdef MOUSE_PORT
		if (iob->Controller == MOUSE_PORT) {
		    ms_input(c);
		}
		else
#endif
#ifdef KEYBD_PORT
		if (iob->Controller == KEYBD_PORT) {
		    register int cc;
		    char buff0[8];

		    cc = kb_translate(c & 0xff, buff0);
		    for (c = 0; c < cc; c++)
			_ttyinput(db, buff0[c]);
		}
		else
#endif
		    _ttyinput(db, c);
	    }

	    if ((iob->Flags & F_NBLOCK) == 0) {
	    	while (CIRC_EMPTY(db))
		    _scandevs();
	    }

	    if (CIRC_EMPTY(db)) {
		iob->Count = ocnt - iob->Count;
		return(ESUCCESS);
	    }

	    *addr++ = _circ_getc(db);
	    iob->Count--;
	}
	iob->Count = ocnt;
	return(ESUCCESS);

    } else if (func == WRITE) {
	while (iob->Count-- > 0)
	    consputc(*addr++, (int)iob->Controller);
	iob->Count = ocnt;
	return(ESUCCESS);

    } else
        return(iob->ErrorNumber = EINVAL);
}


void
_z8530poll(IOBLOCK *iob)
{
    register struct device_buf	*db = &z8530ports[iob->Controller].si_ibuf;
    register int c;

    while (c = consgetc((int)iob->Controller)) {
#ifdef MOUSE_PORT
	if (iob->Controller == MOUSE_PORT) {
	    ms_input(c);
	}
	else
#endif
#ifdef KEYBD_PORT
	if (iob->Controller == KEYBD_PORT) {
	    register int cc;
	    char buff0[8];

	    cc = kb_translate(c & 0xff, buff0);
	    for (c = 0; c < cc; c++)
		_ttyinput(db, buff0[c]);
	}
	else
#endif
	    _ttyinput(db, c);
    }
    iob->ErrorNumber = ESUCCESS;
}

int
_z8530ioctl(IOBLOCK *iob)
{
    register struct device_buf	*db = &z8530ports[iob->Controller].si_ibuf;
    int				retval = 0;

    switch ((long)iob->IoctlCmd) {
    case TIOCRAW:
	if (iob->IoctlArg)
	    db->db_flags |= DB_RAW;
	else
	    db->db_flags &= ~DB_RAW;
	break;

    case TIOCRRAW:
	if (iob->IoctlArg)
	    db->db_flags |= DB_RRAW;
	else
	    db->db_flags &= ~DB_RRAW;
	break;

    case TIOCFLUSH:
	CIRC_FLUSH(db);
	break;

    case TIOCREOPEN:
	retval = _z8530open(iob);
	break;

    case TIOCCHECKSTOP:
	while (CIRC_STOPPED(db))
		_scandevs();
	break;

    default:
	return(iob->ErrorNumber = EINVAL);
    }

    return (retval);
}

static int
_z8530readstat(IOBLOCK *iob)
{
    register struct device_buf	*db = &z8530ports[iob->Controller].si_ibuf;
    iob->Count = _circ_nread(db);
    return(iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN));
}

/* ARCS - new stuff */

/*ARGSUSED*/
int _z8530_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
		return(_z8530init(iob));

	case	FC_OPEN:
		return (_z8530open (iob));

	case	FC_READ:
		return (_z8530strategy (iob, READ));

	case	FC_WRITE:
		return (_z8530strategy (iob, WRITE)); 

	case	FC_CLOSE:
#ifdef MOUSE_PORT
		if (iob->Controller == MOUSE_PORT) mscnt--;
#endif
		return (0);

	case	FC_IOCTL:
		return (_z8530ioctl(iob));

	case	FC_GETREADSTATUS:
		return (_z8530readstat(iob));

	case	FC_POLL:
		_z8530poll(iob);
		return 0;

	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

/* ARCS - new stuff */

static
int map_baud(int brate)
{
    return ((CLK_SPEED + brate * CLK_FACTOR) / (brate * CLK_FACTOR * 2) - 2);
}



static int baud_table[] = {
    110, 300, 1200, 2400, 4800, 9600, 19200, 38400
};
#define	NBAUD	(sizeof(baud_table) / sizeof(int))

static int
next_baud(int baud)
{
    register int	i;

    for (i = 1; i < NBAUD; i++) {
	if (baud_table[i] > baud)
	    return (baud_table[i]);
    }
    return (baud_table[0]);
}


char *
check_serial_baud_rate(char *baud)
{
    register int i, ibaud = atoi(baud);

    if (!baud)
	return NULL;
    for (i = 0; i < NBAUD; i++) {
	if (baud_table[i] == ibaud)
	    return baud;
    }
    return("9600");
}


int
consgetc(int unit)
{
    static int			brk[NPORTS];	/* BREAK monitor */
    char			buf[80];
    register volatile u_char	*cntrl;
    u_char			rx_data;
    register u_char		status;
    extern int			Verbose;
    register int		s;

    cntrl = z8530ports[unit].cntrl;

    s = LOCK_PORT(unit);

    status = RD_RR0(cntrl);

    z8530cons_was_polled++;	/* bump activity indicator */

    if (status & RR0_BRK) {			/* in the middle of a break */
	if (!brk[unit])
		WR_WR0(cntrl, WR0_RST_EXT_INT)	/* reset latch */
	brk[unit] = 1;
	UNLOCK_PORT(unit, s);
	return (0);
    } else if (brk[unit]) {			/* break just terminated */
	brk[unit] = 0;
	/* cycle baud rate on dumb terminals only */
#ifdef MOUSE_PORT
	if (unit != KEYBD_PORT && unit != MOUSE_PORT) {
#endif
	    z8530ports[unit].si_brate = next_baud(z8530ports[unit].si_brate);
	    configure_port(unit);
	    sprintf(buf, "\n\n%s baud rate set to %d\n\n",
	    	unit == CONSOLE_PORT ? "diagnostic" : "remote",
	    	z8530ports[unit].si_brate);
	    _errputs(buf);
#ifdef MOUSE_PORT
	} else {
	    _errputs("Warning: keyboard/mouse cable problem\n");
	    configure_port(unit);
	}
#endif
	UNLOCK_PORT(unit, s);
	return (0);
    }

    if (!(status & RR0_RX_CHR)) {	/* no character available */
	UNLOCK_PORT(unit, s);
	return (0);
    }

    status = RD_CNTRL(cntrl, RR1);
    rx_data = RD_DATA(z8530ports[unit].data);

    if (status & (RR1_RX_ORUN_ERR | RR1_FRAMING_ERR | RR1_PARITY_ERR)) {
	WR_WR0(cntrl, WR0_RST_ERR)	/* reset error */
#ifdef MOUSE_PORT
	if (Verbose && unit != MOUSE_PORT) {
#else
	if (Verbose) {
#endif
	    if (status & RR1_RX_ORUN_ERR)
	    	_errputs("\nZ8530 overrun error\n");
	    else if (status & RR1_FRAMING_ERR)
	    	_errputs("\nZ8530 framing error\n");
	    else
	    	_errputs("\nZ8530 parity error\n");
	}
	UNLOCK_PORT(unit, s);
	return (0);
    }

    UNLOCK_PORT(unit, s);
    return (rx_data | 0x100);
}



void
consputc(u_char c, int unit)
{
    register volatile u_char	*cntrl;
    register struct device_buf	*db;
    register int s;

    z8530cons_was_polled++;	/* bump activity indicator */

    if (sk_sable) {
	cpu_errputc(c);
	return;
    }

    db = &z8530ports[unit].si_ibuf;
    while (CIRC_STOPPED(db))
	_scandevs();

    cntrl = z8530ports[unit].cntrl;

    s = LOCK_PORT(unit);

    while (!(RD_RR0(cntrl) & RR0_TX_EMPTY))
	_scandevs();

    WR_DATA(z8530ports[unit].data, c);
    z8530ports[unit].wflag = 1;
    UNLOCK_PORT(unit, s);
}

/* steal console from specified owner
 *
 * Intended for use by a CPU which wants to steal the console after an NMI
 * because the current "owner" has not responded to the NMI.
 * To be used by symmon.
 *
 * return 0 if console was not held by indicated cpu
 */
int
nmi_z8530cons_steal(int unit, int owner_cpuid)
{
#if EVEREST
	int status;

	if (nports == 0)
		return(0);	/* _z8530init was never called */
	status = spsteallock( du_lock[(unit/2)], owner_cpuid);
	return(status);
#else
	return(0);
#endif
}

/* primitive output routines in case all else fails */

#define ERR_PORT 1
#define	ERR_PORT_CNTRL	(volatile u_char *)DUART1B_CNTRL
#define	ERR_PORT_DATA	(volatile u_char *)DUART1B_DATA

void
z8530cons_errputc(u_char c)
{
    register volatile u_char	*cntrl = ERR_PORT_CNTRL;

    int s = LOCK_PORT(ERR_PORT);

    if (!nports) {
	_z8530init(0);			/* sets nports */
	z8530ports[CONSOLE_PORT].wflag = 1;
    }
    while (!(RD_RR0(cntrl) & RR0_TX_EMPTY))
	continue;
    WR_DATA(ERR_PORT_DATA, c)
    UNLOCK_PORT(ERR_PORT, s);
}

static void
configure_port(int unit)
{
    int				baud;
    register volatile u_char	*cntrl;

    INITLOCK_PORT(unit);

    /* make sure the transmit FIFO is empty */
    cntrl = z8530ports[unit].cntrl;
    if (z8530ports[unit].wflag) {
    	while (!(RD_CNTRL(cntrl, RR1) & RR1_ALL_SENT))
	    ;
    }
    z8530ports[unit].wflag = 0;

    /* reset channel */
    if ((__scunsigned_t)cntrl & CHNA_CNTRL_OFFSET)
	WR_CNTRL(cntrl, WR9, WR9_RST_CHNA)
    else
	WR_CNTRL(cntrl, WR9, WR9_RST_CHNB)

    /* set clock factor, parity, and stop bits */
#ifdef KEYBD_PORT
    if (unit == KEYBD_PORT)
    	WR_CNTRL(cntrl, WR4, WR4_X16_CLK | WR4_1_STOP | WR4_PARITY_ENBL)
    else
#endif
    	WR_CNTRL(cntrl, WR4, WR4_X16_CLK | WR4_1_STOP)

    /* latch break or ethernet carrier detect signal */
#ifdef MOUSE_PORT
    if (unit ==  MOUSE_PORT) {
    	WR_CNTRL(cntrl, WR15, WR15_DCD_IE | WR15_Z85130_EXTENDED)
    	WR_CNTRL(cntrl, WR1, WR1_EXT_INT_ENBL)
    } else
#endif
    	WR_CNTRL(cntrl, WR15, WR15_BRK_IE | WR15_Z85130_EXTENDED)

    WR_CNTRL(cntrl, WR7, WR7_EXTENDED_READ)

    /* set up receiver/transmitter operating parameters */
    /* RTS/DTR must be asserted for the WYSE terminal to work correctly */
    WR_CNTRL(cntrl, WR3, WR3_RX_8BIT)
#if IP22 || IP26 || IP28
    WR_CNTRL(cntrl, WR5, WR5_TX_8BIT | WR5_RTS | WR5_DTR)
#else
    WR_CNTRL(cntrl, WR5, WR5_TX_8BIT)
#endif

    /* receiver/transmitter clock source is baud rate generator, BRG */
    WR_CNTRL(cntrl, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG)

    if ((baud = map_baud(z8530ports[unit].si_brate)) <= 0)
	return;

    /* set baud rate, safer to disable BRG first */
    WR_CNTRL(cntrl, WR14, 0x00)
    WR_CNTRL(cntrl, WR12, baud & 0xff)
    WR_CNTRL(cntrl, WR13, (baud >> 8) & 0xff)
    WR_CNTRL(cntrl, WR14, WR14_BRG_ENBL)

    /* enable receiver/transmitter */
    WR_CNTRL(cntrl, WR3, WR3_RX_8BIT | WR3_RX_ENBL)
#if IP22 || IP26 || IP28
    WR_CNTRL(cntrl, WR5, WR5_TX_8BIT | WR5_TX_ENBL | WR5_RTS | WR5_DTR)
#else
    WR_CNTRL(cntrl, WR5, WR5_TX_8BIT | WR5_TX_ENBL)
#endif

    /*
     * a mysterious BREAK signal sometimes gets generated during reset,
     * this tries to get around the problem
     */
    WR_WR0(cntrl, WR0_RST_EXT_INT)		/* reset latches */
    if (RD_RR0(cntrl) & RR0_BRK) {
	int maxcount = 100000;			/* don't hang forever */
	while (RD_RR0(cntrl) & RR0_BRK)
	    if (--maxcount < 0)
		break;
	WR_WR0(cntrl, WR0_RST_EXT_INT)		/* reset latches */
    }
    if (RD_RR0(cntrl) & RR0_BRK)
	_errputs("Warning: persistent break condition on serial port 0.\n");

    /* clear FIFO and error */
    while (RD_RR0(cntrl) & RR0_RX_CHR) {
	RD_CNTRL(cntrl, RR8);
	WR_WR0(cntrl, WR0_RST_ERR)
    }
    CIRC_FLUSH(&z8530ports[unit].si_ibuf);	/* flush all pending input */
}




#if IP20
static int enet_carrier;

void
reset_enet_carrier()
{
    volatile u_char *cntrl = z8530ports[MOUSE_PORT].cntrl;

    int s = LOCK_PORT(MOUSE_PORT);

    WR_WR0(cntrl, WR0_RST_EXT_INT)

    enet_carrier = DU_DCD_ASSERTED(RD_RR0(cntrl));

    UNLOCK_PORT(MOUSE_PORT, s);
}



int
enet_carrier_on()
{
    /* interrupt pending status readable from channel A only */
    volatile u_char	*cntrl = z8530ports[KEYBD_PORT].cntrl;

    int s = LOCK_PORT(KEYBD_PORT);

    if (!enet_carrier)
    	enet_carrier = RD_CNTRL(cntrl, RR3) & RR3_CHNB_EXT_IP;

    UNLOCK_PORT(KEYBD_PORT, s);

    return (enet_carrier);
}
#endif /* IP20 */

#ifdef KEYBD_PORT
int
du_keyboard_port(void)
{
    return KEYBD_PORT;
}
#endif

static COMPONENT ttytmpl = {
	ControllerClass,		/* Class */
	SerialController,		/* Type */
	(IDENTIFIERFLAG)0,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
#ifdef IP20
	9,				/* IdentifierLength */
	"IP20 tty"			/* Identifier */
#endif  /* IP20 */
#ifdef  IP22
	9,				/* IdentifierLength */
	"IP22 tty"			/* Identifier */
#endif	/* IP22 */
#ifdef  IP26
	9,				/* IdentifierLength */
	"IP26 tty"			/* Identifier */
#endif	/* IP28 */
#ifdef  IP28
	9,				/* IdentifierLength */
	"IP28 tty"			/* Identifier */
#endif	/* IP28 */
#ifdef EVEREST
	8,				/* IdentifierLength */
	"IO4 tty"			/* Identifier */
#endif
};

static COMPONENT ttyptmpl = {
	PeripheralClass,		/* Class */
	LinePeripheral,			/* Controller */
	(IDENTIFIERFLAG)(ConsoleIn|ConsoleOut|Input|Output), /* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	NULL				/* Identifier */
};

/* install ports that are usable as ttys on config tree.
 */
int
z8530_install(COMPONENT *top)
{
	COMPONENT *ctrl,*per;
	int i;
#ifdef MOUSE_PORT
	int ms=0;
#endif

	_z8530init(0);

	for (i = 0; i < nports; i++) {
		ctrl = AddChild(top,&ttytmpl,(void *)NULL);
		if (ctrl == (COMPONENT *)NULL) cpu_acpanic("serial");
		ctrl->Key = i;

#ifdef KEYBD_PORT
		if (i == KEYBD_PORT) {
			/* keyboard can be accessed as serial or kbd */
			map_baud(z8530ports[i].si_brate = 600);
			configure_port(i);
			if (config_keyboard(ctrl) != 0)
				DeleteComponent(ctrl);
			else ms=1;
		}
#endif
#ifdef MOUSE_PORT
		else if (i == MOUSE_PORT) {
			/* cannot probe for mouse--add if kbd is present */
			if (!ms || ms_config(ctrl))
				DeleteComponent(ctrl);
		}
		else {
#else
		{
#endif
			/* Add line() for real serial ports */
			per = AddChild(ctrl,&ttyptmpl,(void *)NULL);
			if (per == (COMPONENT *)NULL) cpu_acpanic("line");
			RegisterDriverStrategy(per,_z8530_strat);
		}
	}

	return(0);
}

#endif /* IP20 || IP22 || IP26 || IP28 || EVEREST */
