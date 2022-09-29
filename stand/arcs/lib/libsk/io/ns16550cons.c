#if defined(IP30) || defined(SN0)

#ident	"lib/libsk/io/ns16550cons.c: $Revision: 1.43 $"

/* National Semiconductor 16550 driver */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/ns16550.h>
#include <arcs/folder.h>
#include <tty.h>
#include <saio.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>
#include <arcs/hinv.h>
#include <arcs/errno.h>
#include <standcfg.h>
#ifdef SN0
#include <sys/PCI/ioc3.h>
#include <ksys/elsc.h>
#include <pgdrv.h>
int junkuart_probe(void);
#endif

#define	NUMDUARTS		1
#define	CONSOLE_PORT		0
#define	REMOTE_PORT0		1

#define	NPORTS			NUMDUARTS*2

struct ns16550info {
	volatile uart_reg_t	*cntrl;
	int			si_brate;
	struct device_buf	si_ibuf;
} ns16550ports[NPORTS];

#ifdef IP30
#if defined(MFG_IO6)
volatile uart_reg_t    *ser0_base = (volatile uart_reg_t *)( PHYS_TO_COMPATK1(MAIN_WIDGET(XBOW_PORT_A) +
                           BRIDGE_DEVIO2 + IOC3_SIO_UA_BASE) );
#if (NPORTS > 1)
volatile uart_reg_t    *ser1_base = (volatile uart_reg_t *)( PHYS_TO_COMPATK1(MAIN_WIDGET(XBOW_PORT_A) +
                           BRIDGE_DEVIO2 + IOC3_SIO_UB_BASE) );
#endif
#else
volatile uart_reg_t    *ser0_base = (volatile uart_reg_t *)MIPS_SER0_BASE;
#if (NPORTS > 1)
volatile uart_reg_t    *ser1_base = (volatile uart_reg_t *)MIPS_SER1_BASE;
#endif
#endif /* MFG_IO6 */
#elif defined(SN0)
volatile uart_reg_t    *ser0_base ;
#if (NPORTS > 1)
volatile uart_reg_t    *ser1_base ;
#endif
#endif

#ifndef IP30_RPROM

static int	_ns16550init(IOBLOCK *);
static int 	_ns16550ioctl(IOBLOCK *);
static int 	_ns16550open(IOBLOCK *);
static int 	_ns16550strategy(IOBLOCK *, int);
char 		*check_serial_baud_rate(char *);
static void	configure_port(int);
int 		consgetc(int);
void 		consputc(uchar_t, int);
static int 	map_baud(int);
static int 	next_baud(int);
void 		ns16550cons_errputc(uchar_t);

static int	nports;		/* number of ports, and init flag */

volatile int ns16550cons_was_polled;	/* console port activity indicator */


static int
_ns16550init(IOBLOCK *iob)
{
	static int	pass;
	register int	i;

	if (iob) {
		if (iob->Count == pass)
			return 0;
		pass = (int)iob->Count;
	}
	else if (!nports)	/* _ns16550init(0) for early init */
		nports = NPORTS;
#ifdef SN0
	if (!nports)	/* SN0 does not call early init */
		nports = NPORTS;
#endif

	for (i = 0; i < nports; i++)
		bzero(&ns16550ports[i].si_ibuf, sizeof(struct device_buf));

	/*
	 * initialize the needed addresses.  
	 * The layout is:
	 *  Logical device	unit	Comments
	 *	tty(0)		0	diagnostic console.
	 *	tty(1)		1	remote
	 */

	/* Console of port 1 tty(0) */
	ns16550ports[0].cntrl = ser0_base;
#if (NPORTS > 1)
	ns16550ports[1].cntrl = ser1_base;
#endif

	/*
	 * get baud rate and configure port so the port doesn't have
	 * to be opened to be at the correct baud rate.
	 */
#ifndef SN0
	/* 
	 * This does not work in init for SN0. Ports are configured
 	 * in the open routine for SN0. 
 	 */
	for (i = 0; i < nports; i++) {
		get_baud(i);
		configure_port(i);
	}
#endif

	return 0;
}


/* get_baud - get the baud rate for the port */
void
get_baud(int unit)
{
	char	*abaud = 0;

	switch (unit) {
	case CONSOLE_PORT:
		abaud = getenv("console");
		if (abaud && !strcmp(abaud, "d2")) {
			if (!(abaud = getenv("rbaud")))
				abaud = "19200";
		} else 
			abaud = getenv("dbaud");
		abaud = check_serial_baud_rate(abaud);
		break;

	case REMOTE_PORT0:
		abaud = getenv("console");
		if (abaud && !strcmp(abaud, "d2")) 
			abaud = getenv("dbaud");
		else if (!(abaud = getenv("rbaud")))
			abaud = "19200";
		abaud = check_serial_baud_rate(abaud);
		break;
	}

	if (!abaud || *atob(abaud, &ns16550ports[unit].si_brate)
	    || (map_baud(ns16550ports[unit].si_brate) <= 0)) {
		if (unit == CONSOLE_PORT)
			ns16550ports[unit].si_brate = 9600;
		else
			ns16550ports[unit].si_brate = 1200;
		if (environ)
			printf("\nIllegal %s value: %s\n",
				unit ? "rbaud" : "dbaud", abaud);
	}
}

/* _ns16550open -- initialize DUARTs */
static int
_ns16550open(IOBLOCK *iob)
{
	int	unit = (int)iob->Controller;

	if ((unit < 0) || (unit >= nports))
		return iob->ErrorNumber = ENXIO;

	/*
	 * serial and sgi keyboard only handle console(0), which in this case
	 * is the same as not specifying console(0).
	 */
	if (iob->Flags & F_ECONS)
		return iob->ErrorNumber = EINVAL;

	iob->Flags |= F_SCAN;

	get_baud(unit);
	configure_port(unit);

	return ESUCCESS;
}



/* _ns16550strategy -- perform io */
static int
_ns16550strategy(IOBLOCK *iob, int func)
{
	register int			c;
	register struct device_buf	*db;
	int				ocnt = (int)iob->Count;
	char 				*addr = (char *)iob->Address;

	if (func == READ) {
		db = &ns16550ports[iob->Controller].si_ibuf;

		while (iob->Count > 0) {
			while (c = consgetc((int)iob->Controller))
				_ttyinput(db, c);

			if ((iob->Flags & F_NBLOCK) == 0) {
				while (CIRC_EMPTY(db))
					_scandevs();
			}

			if (CIRC_EMPTY(db)) {
				iob->Count = ocnt - iob->Count;
				return ESUCCESS;
			}

			*addr++ = _circ_getc(db);
			iob->Count--;
		}

		iob->Count = ocnt;
		return ESUCCESS;

	}
	else if (func == WRITE) {
		while (iob->Count-- > 0)
			consputc(*addr++, (int)iob->Controller);

		iob->Count = ocnt;
		return ESUCCESS;
	} else
		return iob->ErrorNumber = EINVAL;
}


static void
_ns16550poll(IOBLOCK *iob)
{
	register struct device_buf	*db;
	register int			c;

	db = &ns16550ports[iob->Controller].si_ibuf;
	while (c = consgetc((int)iob->Controller))
		_ttyinput(db, c);

	iob->ErrorNumber = ESUCCESS;
}

static int
_ns16550ioctl(IOBLOCK *iob)
{
	register struct device_buf	*db;
	int				retval = 0;

	db = &ns16550ports[iob->Controller].si_ibuf;

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
		retval = _ns16550open(iob);
		break;

	case TIOCCHECKSTOP:
		while (CIRC_STOPPED(db))
			_scandevs();
		break;

	default:
		return iob->ErrorNumber = EINVAL;
	}

	return retval;
}

static int
_ns16550readstat(IOBLOCK *iob)
{
	register struct device_buf	*db;

	db = &ns16550ports[iob->Controller].si_ibuf;
	iob->Count = _circ_nread(db);
	return iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN);
}

/* ARCS - new stuff */

/*ARGSUSED*/
int
_ns16550_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
		return _ns16550init(iob);

	case	FC_OPEN:
		return _ns16550open(iob);

	case	FC_READ:
		return _ns16550strategy(iob, READ);

	case	FC_WRITE:
		return _ns16550strategy(iob, WRITE); 

	case	FC_CLOSE:
		return 0;

	case	FC_IOCTL:
		return _ns16550ioctl(iob);

	case	FC_GETREADSTATUS:
		return _ns16550readstat(iob);

	case	FC_POLL:
		_ns16550poll(iob);
		return 0;

	default:
		return iob->ErrorNumber = EINVAL;
	}
}

/* ARCS - new stuff */

#define	SERIAL_DIVISOR(x)	SER_DIVISOR(x, SER_XIN_CLK/SER_PREDIVISOR)

static
int map_baud(int brate)
{
	return SERIAL_DIVISOR(brate);
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
			return baud_table[i];
	}

	return baud_table[0];
}


char *
check_serial_baud_rate(char *baud)
{
	register int	i;
	register int	ibaud = atoi(baud);

	if (!baud)
		return NULL;

	for (i = 0; i < NBAUD; i++) {
		if (baud_table[i] == ibaud)
			return baud;
	}

	return "9600";
}

#ifdef SN0

static int
use_junk_uart(void)
{
	static junk_uart = -1;

	if (junk_uart == -1)
		junk_uart = junkuart_probe();
	return junk_uart;
}

#endif /* SN0 */

int
consgetc(int unit)
{
	char				buf[80];
	register volatile uart_reg_t	*cntrl;
	uchar_t				data;
	register uchar_t		status;
	extern int			Verbose;
#ifdef SN0
	jmp_buf				cons_hw_jmpbuf;
	extern int			cons_hw_faulted;
	extern int			junkuart_getc(void);
	extern int			junkuart_poll(void);

	/*
	 * Due to known hub II problems, we can take data bus errors when
	 * reading/writing IOC3 registers. We don't want to go into an
	 * infinite loop when trying to print stuff out. Catch the exception
	 * so we can switch to alternate console.
	 *
	 * FIXME: should call elsc driver once we get rid of junk uart.
	 */
	if (cons_hw_faulted)
		if (use_junk_uart()) {
			return junkuart_poll() ? junkuart_getc() : 0;
		} else {
			elscuart_flush();
			return elscuart_poll() ? elscuart_getc() : 0;
		}

	if (setjmp(cons_hw_jmpbuf)) {
		cons_hw_faulted = 1;
		if (use_junk_uart()) {
			return junkuart_poll() ? junkuart_getc() : 0;
		} else {
			elscuart_flush();
			return elscuart_poll() ? elscuart_getc() : 0;
		}
	}
#endif /* SN0 */

	cntrl = ns16550ports[unit].cntrl;

	status = RD_REG(cntrl, LINE_STATUS_REG);

	ns16550cons_was_polled++;	/* bump activity indicator */

	if (status & LSR_BREAK) {	/* just receive a BREAK */
		/* pop the NULL char associated with the BREAK off the FIFO */
		RD_REG(cntrl, RCV_BUF_REG);

		/* cycle baud rate on dumb terminals only */
		ns16550ports[unit].si_brate =
			next_baud(ns16550ports[unit].si_brate);
		configure_port(unit);
		sprintf(buf, "\n\n%s baud rate set to %d\n\n",
			unit == CONSOLE_PORT ? "diagnostic" : "remote",
		ns16550ports[unit].si_brate);
		_errputs(buf);

		return 0;
	}

	if (!(status & LSR_DATA_READY))	/* no character available */
		return 0;

	data = RD_REG(cntrl, RCV_BUF_REG);

	if (status & (LSR_OVERRUN_ERR | LSR_PARITY_ERR | LSR_FRAMING_ERR)) {
		if (Verbose) {
			if (status & LSR_OVERRUN_ERR)
				_errputs("\nNS16550 overrun error\n");
			else if (status & LSR_FRAMING_ERR)
				_errputs("\nNS16550 framing error\n");
			else
				_errputs("\nNS16550 parity error\n");
		}

		return 0;
   	}

	return data | 0x100;
}



void
consputc(uchar_t c, int unit)
{
	register volatile uart_reg_t	*cntrl;
	register struct device_buf	*db;
#ifdef SN0
	jmp_buf				cons_hw_jmpbuf;
	extern int			cons_hw_faulted;
	extern int			junkuart_putc(int);


	/*
	 * Due to known hub II problems, we can take data bus errors when
	 * reading/writing IOC3 registers. We don't want to go into an
	 * infinite loop when trying to print stuff out. Catch the exception
	 * so we can switch to alternate console.
	 *
	 * FIXME: should call elsc driver once we get rid of junk uart.
	 */
	if (cons_hw_faulted) {
		if (use_junk_uart()) {
			junkuart_putc((int)c);
		} else {
			elscuart_putc((int)c);
		}
		return;
	}

	if (setjmp(cons_hw_jmpbuf)) {
		cons_hw_faulted = 1;
		if (use_junk_uart()) {
			junkuart_putc((int)c);
		} else {
			elscuart_putc((int)c);
		}
		return;
	}
#endif /* SN0 */

	ns16550cons_was_polled++;	/* bump activity indicator */

	db = &ns16550ports[unit].si_ibuf;

	while (CIRC_STOPPED(db))
		_scandevs();

	cntrl = ns16550ports[unit].cntrl;

	while (!(RD_REG(cntrl, LINE_STATUS_REG) & LSR_XMIT_BUF_EMPTY))
		_scandevs();

	WR_REG(cntrl, XMIT_BUF_REG, c);
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
nmi_ns16550cons_steal(int unit, int owner_cpuid)
{
	/* Unlike the z8530cons driver, this driver does not have a
	 * duart lock, so there is no lock to "break".
	 */
	return(0);
}


/* primitive output routines in case all else fails */

#define	ERR_PORT	ser0_base

void
ns16550cons_errputc(uchar_t c)
{
	register volatile uart_reg_t	*cntrl = ERR_PORT;

	if (!nports)
		_ns16550init(0);	/* sets nports */

	while (!(RD_REG(cntrl, LINE_STATUS_REG) & LSR_XMIT_BUF_EMPTY))
		continue;

	WR_REG(cntrl, XMIT_BUF_REG, c);
}


static void
configure_port(int unit)
{
	int				baud;
	register volatile uart_reg_t	*cntrl;

#if (defined(SABLE) && defined(SN0)) 
	/* Do nothing if sable and sn0. no support for this in sable. */
	return ;
#endif
	cntrl = ns16550ports[unit].cntrl;

	/* make sure the transmit FIFO is empty */
	while (!(RD_REG(cntrl, LINE_STATUS_REG) & LSR_XMIT_EMPTY))
		;

	/* baud rate divisor */
	if ((baud = map_baud(ns16550ports[unit].si_brate)) <= 0)
		return;

	WR_REG(cntrl, LINE_CNTRL_REG, LCR_DLAB);
	WR_REG(cntrl, DIVISOR_LATCH_MSB_REG, (baud >> 8) & 0xff);
	WR_REG(cntrl, DIVISOR_LATCH_LSB_REG, baud & 0xff);

	/* while dlab is set program the pre-divisor */
	WR_REG(cntrl, SCRATCH_PAD_REG, SER_PREDIVISOR * 2);

	/* set operating parameters and set DLAB to 0 */
	WR_REG(cntrl, LINE_CNTRL_REG, LCR_8_BITS_CHAR | LCR_1_STOP_BITS);

	/* no interrupt */
	WR_REG(cntrl, INTR_ENABLE_REG, 0x0);
	
	/* turn on RTS and DTR in case the console terminal expects
	 * flow control signals to make sense
	 */
	WR_REG(cntrl, MODEM_CNTRL_REG, MCR_DTR | MCR_RTS);

#if !defined(SN0) || defined(FIFO_MODE)

	/* enable FIFO mode and reset both FIFOs */
	WR_REG(cntrl, FIFO_CNTRL_REG, FCR_ENABLE_FIFO);
	WR_REG(cntrl, FIFO_CNTRL_REG, 
		FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET | FCR_XMIT_FIFO_RESET);
#endif	/* FIFO_MODE */
	/* flush all pending input */
	CIRC_FLUSH(&ns16550ports[unit].si_ibuf);
}

static char	_IP30_tty[] = "IP30 tty";

static COMPONENT ttytmpl = {
	ControllerClass,		/* Class */
	SerialController,		/* Type */
	(IDENTIFIERFLAG)0,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	sizeof _IP30_tty - 1,		/* IdentifierLength */
	_IP30_tty			/* Identifier */
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

#ifdef IP30
static char ns16550_installed;		/* only install/init once */

/* install ports that are usable as ttys on config tree.
 */
int
ns16550_install(COMPONENT *top)
{
	COMPONENT	*ctrl;
	int		i;
	COMPONENT	*per;


	if (ns16550_installed)
	    return(1);
	ns16550_installed = 1;

	_ns16550init(0);

	for (i = 0; i < nports; i++) {
		ctrl = AddChild(top, &ttytmpl, (void *)NULL);
		if (ctrl == (COMPONENT *)NULL)
			cpu_acpanic("serial");
		ctrl->Key = i;

		/* Add line() for real serial ports */
		per = AddChild(ctrl, &ttyptmpl, (void *)NULL);
		if (per == (COMPONENT *)NULL)
			cpu_acpanic("line");
		RegisterDriverStrategy(per, _ns16550_strat);
	}

	return 1;
}
#elif defined(SN0)
#ifdef SN_PDI
int
ns16550_install(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
{

        ser0_base = (volatile uart_reg_t *) pdipMemBase(pdip) + 
						IOC3_SIO_UA_BASE ;

	pdipDevUnit(pdip) = 0 ;
	snAddChild(pdih, pdip) ;

#if (NPORTS > 1)
        ser1_base = (volatile uart_reg_t *) pdipMemBase(pdip) + 
                                                IOC3_SIO_UB_BASE ;
	pdipDevUnit(pdip) = 1 ;
	snAddChild(pdih, pdip) ;
#endif

        return 0;

}
#else
int
ns16550_install(COMPONENT *c)
{
	ser0_base = (volatile uart_reg_t *)c->Key ;

#if (NPORTS > 1)
	ser1_base = (volatile uart_reg_t *)
		    (c->Key - IOC3_SIO_UA_BASE + IOC3_SIO_UB_BASE);
#endif

	return 0;
	
}
#endif /* SN_PDI */
#endif
#endif /* ! IP30_RPROM */

#endif /* IP30 || SN0 */
