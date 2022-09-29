#include <sys/types.h>
#include <sys/time.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <saioctl.h>
#include <saio.h>
#include <tty.h>
#include "ipxx.h"

extern int glkbfd;

int glkbd_strat();

bell()
{
	l_write(0,"\007",1);
}

static COMPONENT Ctrl = {
	ControllerClass,
	KeyboardController,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	6,
	"glkbd"
};

static COMPONENT Kbd = {
	PeripheralClass,
	KeyboardPeripheral,
	Input|ConsoleIn,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};

glkbd_install(COMPONENT *root)
{
	COMPONENT *ctrl,*per;
	extern int ioserial;

	if (ioserial) return;

	ctrl = AddChild(root,&Ctrl,(void *)NULL);
	if (ctrl == 0) cpu_acpanic("gl kbd ctrl");
	per = AddChild(ctrl,&Kbd,(void *)NULL);
	if (per == 0) cpu_acpanic("gl kbd");
	RegisterDriverStrategy(per,glkbd_strat);
}

static struct device_buf kbd_ibuf;

static int
glkbd_read(IOBLOCK *iob)
{
	register struct device_buf *db = &kbd_ibuf;
	char *addr = (char *)iob->Address;
	int ocnt = iob->Count;
	int c;

	while (iob->Count > 0) {
		while (c = glkbd_getc(iob->Controller)) {
			_ttyinput(db,c);
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
}

static int
glkbd_ioctl(IOBLOCK *iob)
{
	struct device_buf *db = &kbd_ibuf;

	switch ((int)iob->IoctlCmd) {
	case TIOCCHECKSTOP:
		while (CIRC_STOPPED(db))
			_scandevs();
		return(ESUCCESS);
		break;
	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

static int
glkbd_grs(IOBLOCK *iob)
{
	register struct device_buf *db = &kbd_ibuf;
	iob->Count = _circ_nread(db);
	return(iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN));
}

static int
glkbd_poll(IOBLOCK  *iob)
{
	register struct device_buf *db = &kbd_ibuf;
	register int c;
	
	while (c = glkbd_getc(iob->Controller)) {
		_ttyinput(db, c);
	}
	iob->ErrorNumber = ESUCCESS;
}

int
glkbd_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {
	case FC_INITIALIZE:
		bzero(&kbd_ibuf,sizeof(struct device_buf));
		CIRC_FLUSH(&kbd_ibuf);
		return(ESUCCESS);
		break;
	case FC_OPEN:
		if (iob->Flags & F_ECONS)
			return(iob->ErrorNumber = EINVAL);
		iob->Flags |= F_SCAN;
		return(ESUCCESS);
	case FC_READ:
		return(glkbd_read(iob));
	case FC_IOCTL:
		return(glkbd_ioctl(iob));
	case FC_CLOSE:
		return(ESUCCESS);
	case FC_GETREADSTATUS:
		return(glkbd_grs(iob));
	case FC_POLL:
		return(glkbd_poll(iob));
		break;
	case FC_WRITE:
	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

int glkbd_getc(int ctrl)
{
	fd_set rfd,wfd,efd;
	struct timeval tv;
	int rc;
	char c = 0;

	FD_ZERO(&rfd);
	FD_SET(glkbfd,&rfd);
	FD_ZERO(&wfd);
	FD_ZERO(&efd);

	tv.tv_sec = 0;
	tv.tv_usec = 1;

	if (l_select(32,&rfd,&wfd,&efd,&tv)) {
		rc = l_read(glkbfd,&c,1);
		if (rc == 0) {
			/* pipe is probably gone, exit! */
			l_exit(0);
		}
		return((int)c);
	}
	else
		return(0);
}
