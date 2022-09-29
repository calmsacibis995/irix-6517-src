#ifdef EVEREST

#ident "lib/libsk/io/ccuart.c: $Revision: 1.6 $"

/* EVEREST IP19 CC UART driver */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/i8251uart.h>
#include <arcs/folder.h>
#include <tty.h>
#include <saio.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>
#include <arcs/hinv.h>
#include <arcs/errno.h>

#define CC_WRITE_CMD(_x) 	store_double_lo((long long *)EV_UART_CMD, (_x))
#define CC_READ_CMD 		load_double_lo((long long *)EV_UART_CMD)
#define WRITE_DATA(_x)		store_double_lo((long long *)EV_UART_DATA, (_x))
#define READ_DATA		load_double_lo((long long *)EV_UART_DATA)

/* Device buffer for CC UART */
static struct device_buf dbvalue;
static struct device_buf *db;
static int ccgetc(void);
static void ccputc(int);
static int _ccuartinit(void);

/*
 * Initialize the IP19 CC UART
 */
static int
_ccuartinit(void)
{
    /* Initialize CC UART -- Probably okay, but don't do it again
    CC_WRITE_CMD(0);
    CC_WRITE_CMD(0);
    CC_WRITE_CMD(0);
    CC_WRITE_CMD(I8251_RESET);
    CC_WRITE_CMD(I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | I8251_STOPB1);
    CC_WRITE_CMD(I8251_TXENB | I8251_RXENB | I8251_RESETERR);
    */

    db = &dbvalue;
    bzero(db, sizeof(struct device_buf));
    CIRC_FLUSH(db);

    return 0;
}


/*
 * _ccuartopen()
 *	Opens the cc uart for I/O.  We don't even try to support multiple
 * 	CC UARTS right now.
 */

int
_ccuartopen(IOBLOCK *iob)
{
    if (iob->Controller != 0)
	return (iob->ErrorNumber = ENXIO);

    iob->Flags |= F_SCAN;

    return ESUCCESS;
}


/*
 * _ccuartstrategy()
 *	Actually deal with I/O
 */

int 
_ccuartstrategy(IOBLOCK *iob, int func)
{
    register int c;
    int		 ocnt = iob->Count;
    char	*addr = (char*)iob->Address;


    if (func == READ) {

	while (iob->Count > 0) {
	    
	    while (c = ccgetc()) {
		_ttyinput(db, c);
	    }
	
	    if ((iob->Flags & F_NBLOCK) == 0) {
		while (CIRC_EMPTY(db))
		    _scandevs();
	    }

	    if (CIRC_EMPTY(db)) {
		iob->Count = ocnt - iob->Count;
		return ESUCCESS;
	    }

	    c = _circ_getc(db);	
	    *addr++ = c;
	    iob->Count--;
	}

	iob->Count = ocnt;
	return ESUCCESS;

    } else if (func == WRITE) {
	while (iob->Count-- > 0)
	    ccputc(*addr++);
	iob->Count = ocnt;
	return ESUCCESS;

    } else {
	return (iob->ErrorNumber = EINVAL);
    }
}


void
_ccuartpoll(IOBLOCK *iob)
{
    int c;

    while (c = ccgetc()) {
	_ttyinput(db, c);
    }
    iob->ErrorNumber = ESUCCESS;
}


/*
 * _ccuartioctl()
 *	Do nasty device-specific things 
 */

int
_ccuartioctl(IOBLOCK *iob)
{
    int retval = 0;

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
	retval = _ccuartopen(iob);
	break;

      case TIOCCHECKSTOP:
	while (CIRC_STOPPED(db))
	    _scandevs();
	break;

      default:
	return (iob->ErrorNumber = EINVAL);
    }

    return retval;
}

/*
 * _ccuartreadstat()
 *	Returns the status of the current read.
 */

static int
_ccuartreadstat(IOBLOCK *iob)
{
    iob->Count = _circ_nread(db);
    return (iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN));
}


/*
 * _ccuart_strat
 *	ARCS-compatible strategy routine.
 */

/*ARGSUSED*/
int
_ccuart_strat(COMPONENT *self, IOBLOCK *iob)
{
    switch (iob->FunctionCode) {
      case FC_INITIALIZE:
	return _ccuartinit();

      case FC_OPEN:
	return _ccuartopen(iob);

      case FC_READ:
	return _ccuartstrategy(iob, READ);

      case FC_WRITE:
	return _ccuartstrategy(iob, WRITE);

      case FC_CLOSE:
	return 0;

      case FC_IOCTL:
	return _ccuartioctl(iob);

      case FC_GETREADSTATUS:
	return _ccuartreadstat(iob);

      case FC_POLL:
	_ccuartpoll(iob);
	return 0;

      default:
	return (iob->ErrorNumber = EINVAL);
    }
}


/*
 * ccgetc()
 *	If a character is waiting to be read, read it from the
 *	uart and return it.  Otherwise, return zero.
 */

static int
ccgetc(void)
{
    ulong data; 

    if (CC_READ_CMD & I8251_RXRDY) {
	data = READ_DATA;
	return (data);
    } else
	return 0;
}

/*
 * ccputc()
 *	Writes a character out.
 */

static void
ccputc(int c)
{
    while (CIRC_STOPPED(db))
	_scandevs();

    while (! (CC_READ_CMD & I8251_TXRDY))
	_scandevs();

    WRITE_DATA(c);
}

/*
 * ccuart_install
 *	Add the CC UART to the ARCS configuration tree.
 */

static COMPONENT ttytmpl = {
    PeripheralClass,			/* Class */
    SerialController,			/* Controller */
    (IDENTIFIERFLAG)(ConsoleIn|ConsoleOut|Input|Output), /* Flags */
    SGI_ARCS_VERS,			/* Version */
    SGI_ARCS_REV,			/* Revision */
    6,					/* Key */
    0x01,				/* Affinity */
    0,					/* ConfigurationDataSize */
    12,					/* IdentifierLength */
    "EVEREST TTY"			/* Identifier */
};

#if 0
static COMPONENT ttyperiphtmpl = {
    PeripheralClass,			/* Class */
    LinePeripheral,			/* Controller */
    ConsoleIn|ConsoleOut|Input|Output,	/* Flags */
    SGI_ARCS_VERS,			/* Version */
    SGI_ARCS_REV,			/* Revision */
    6,					/* Key */
    0x01,				/* Affinity */
    0,					/* ConfigurationDataSize */
    0,					/* IdentifierLength */
    NULL				/* Identifier */
};
#endif

int
ccuart_install(COMPONENT *top)
{
    COMPONENT *ctrl;

    (void) _ccuartinit();

    ctrl = AddChild(top,&ttytmpl,(void *)NULL);
    if (ctrl == (COMPONENT *)NULL) cpu_acpanic("serial");
    RegisterDriverStrategy(ctrl, _ccuart_strat);

#if 0
    {
	COMPONENT *per;
	/* Add line() for real serial ports */
	per = AddChild(ctrl,&ttyperiphtmpl,(void *)NULL);
	if (per == (COMPONENT *)NULL) cpu_acpanic("line");
	RegisterDriverStrategy(per,_ccuart_strat);
    }
#endif
    return(0);
}

#endif /* EVEREST */
