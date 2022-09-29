#include <sys/sbd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/termio.h>
#undef CTRL
#undef TIOCFLUSH
#include <arcs/folder.h>
#include <tty.h>
#include <saio.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>
#include <arcs/hinv.h>
#include <arcs/errno.h>

#include "ipxx.h"

tty_errputc(char c)
{
	l_write(0,&c,1);
}


#define	NPORTS			1
#define	CONSOLE_PORT		0

struct ttyinfo {
    struct device_buf   si_ibuf;
    int                 wflag;
} ttyports[NPORTS];

int 		_ttyioctl(IOBLOCK *);
int 		_ttyopen(IOBLOCK *);
int 		_ttystrategy(IOBLOCK *, int);
int 		consgetc(int);
void 		consputc(u_char, int);

int
_ttyinit(IOBLOCK *iob)
{
	int i;

	for (i=0; i < NPORTS; i++) {
		bzero(&ttyports[i].si_ibuf,sizeof(struct device_buf));
		CIRC_FLUSH(&ttyports[i].si_ibuf);
	}

	return(0);
}

/* _ttyopen */
int
_ttyopen(IOBLOCK *iob)
{
	struct termio t;
	int i;

	/*  Serial and sgi keyboard only handle console(0), which in this case
	 * is the same as not specifying console(0).
	 */
	if (iob->Flags & F_ECONS)
		return(iob->ErrorNumber = EINVAL);

	/* set irix tty to raw mode */
	i=l_ioctl(0,TCGETA,&t);
	if (i != 0) printf("ioctl failed!\n");
	t.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0;
	i=l_ioctl(0,TCSETA,&t);
	if (i != 0) printf("ioctl failed!\n");
    
	iob->Flags |= F_SCAN;

	return(ESUCCESS);
}


/* _ttystrategy -- perform io */
int
_ttystrategy(IOBLOCK *iob, int func)
{
    register int		c;
    register struct device_buf	*db;
    int				ocnt = iob->Count;
    char 			*addr = (char *)iob->Address;

    if (func == READ) {
	db = &ttyports[iob->Controller].si_ibuf;

	while (iob->Count > 0) {
	    while (c = consgetc(iob->Controller)) {
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
	    consputc(*addr++, iob->Controller);
	iob->Count = ocnt;
	return(ESUCCESS);

    } else
        return(iob->ErrorNumber = EINVAL);
}


void
_ttypoll(IOBLOCK *iob)
{
    register struct device_buf	*db = &ttyports[iob->Controller].si_ibuf;
    register int c;

    while (c = consgetc(iob->Controller)) {
	    _ttyinput(db, c);
    }
    iob->ErrorNumber = ESUCCESS;
}

int
_ttyioctl(IOBLOCK *iob)
{
    register struct device_buf	*db = &ttyports[iob->Controller].si_ibuf;
    int				retval = ESUCCESS;

    switch ((int)iob->IoctlCmd) {
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
	retval = _ttyopen(iob);
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
_ttyreadstat(IOBLOCK *iob)
{
    register struct device_buf	*db = &ttyports[iob->Controller].si_ibuf;
    iob->Count = _circ_nread(db);
    return(iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN));
}

/* ARCS - new stuff */

_tty_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
		return(_ttyinit(iob));

	case	FC_OPEN:
		return (_ttyopen (iob));

	case	FC_READ:
		return (_ttystrategy (iob, READ));

	case	FC_WRITE:
		return (_ttystrategy (iob, WRITE)); 

	case	FC_CLOSE:
		return (0);

	case	FC_IOCTL:
		return (_ttyioctl(iob));

	case	FC_GETREADSTATUS:
		return (_ttyreadstat(iob));

	case	FC_POLL:
		_ttypoll(iob);
		return 0;

	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

/* return character on standard in if availiable, else return 0
 */
int
consgetc(int unit)
{
	fd_set rfd,wfd,efd;
	struct timeval tv;
	char c = 0;

	FD_ZERO(&rfd);
	FD_SET(0,&rfd);
	FD_ZERO(&wfd);
	FD_ZERO(&efd);

	tv.tv_sec = 0;
	tv.tv_usec = 1;

	if (l_select(1,&rfd,&wfd,&efd,&tv)) {
		l_read(0,&c,1);
		return((int)c);
	}
	else
		return(0);
}



void
consputc(u_char c, int unit)
{
    register volatile u_char	*cntrl;
    register struct device_buf	*db;

    db = &ttyports[unit].si_ibuf;
    while (CIRC_STOPPED(db))
	_scandevs();

    l_write(0,&c,1);
    ttyports[unit].wflag = 1;
}



/* primitive output routines in case all else fails */

#define	ERR_PORT_CNTRL	(volatile u_char *)DUART1B_CNTRL
#define	ERR_PORT_DATA	(volatile u_char *)DUART1B_DATA

static COMPONENT ttytmpl = {
	ControllerClass,		/* Class */
	SerialController,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	16,				/* IdentifierLength */
	"IPXX pseudo-tty"		/* Identifier */
};

static COMPONENT ttyptmpl = {
	PeripheralClass,		/* Class */
	LinePeripheral,			/* Controller */
	ConsoleIn|ConsoleOut|Input|Output,/* Flags */
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
tty_install(COMPONENT *top)
{
	COMPONENT *ctrl,*per;
	int i;

	_ttyinit(0);

	for (i = 0; i < NPORTS; i++) {
		ctrl = AddChild(top,&ttytmpl,(void *)NULL);
		if (ctrl == (COMPONENT *)NULL) cpu_acpanic("serial");
		ctrl->Key = i;

		/* Add line() for real serial ports */
		per = AddChild(ctrl,&ttyptmpl,(void *)NULL);
		if (per == (COMPONENT *)NULL) cpu_acpanic("line");
		RegisterDriverStrategy(per,_tty_strat);
	}

	return(0);
}
