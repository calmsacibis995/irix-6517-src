#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/time.h>
#include <stand_htport.h>
#include <guicore.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <saio.h>
#include <libsk.h>

#include "ipxx.h"

int ms_update(int newx, int newy, int nb);
void guiCheckMouse(int,int,int);

static COMPONENT Ctrl = {
	ControllerClass,
	PointerController,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	6,
	"glms"
};

static COMPONENT Ms = {
	PeripheralClass,
	PointerPeripheral,
	Input,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};

extern struct htp_state *htp;
int glms_strat();

int
glms_install(COMPONENT *root)
{
	COMPONENT *ctrl,*per;
	extern int ioserial;
	extern struct htp_state *htp;

	if (ioserial) return 0;

	ctrl = AddChild(root,&Ctrl,(void *)NULL);
	if (ctrl == 0) cpu_acpanic("gl ms ctrl");
	per = AddChild(ctrl,&Ms,(void *)NULL);
	if (per == 0) cpu_acpanic("gl ms");
	RegisterDriverStrategy(per,glms_strat);
	htp->buttons = 0x07;

	return 0;
}

extern int glmsfd;

static int
glms_poll(IOBLOCK *iob)
{
	int newx,newy,buttons;
	fd_set rfd,wfd,efd;
	struct timeval tv;
	char cmd[3];
	int rc;

	FD_ZERO(&rfd);
	FD_SET(glmsfd,&rfd);
	FD_ZERO(&wfd);
	FD_ZERO(&efd);

	tv.tv_sec = 0;
	tv.tv_usec = 1;

	while (l_select(32,&rfd,&wfd,&efd,&tv)) {
		rc = l_read(glmsfd,cmd,3);
		if (rc == 0) {
			/* pipe is probably gone, exit! */
			l_exit(0);
		}
		newx = newy = buttons = -1;
		buttons = gfxgui.htp->buttons;
		switch(cmd[0]) {
		case 'x':
			newx = (cmd[1] << 8) | cmd[2];
			break;
		case 'y':
			newy = htp->yscreensize - ((cmd[1] << 8) | cmd[2]);
			break;
		case 'b':
			rc = (1 << cmd[3]);
			if (cmd[2])
				buttons &= ~rc;
			else
				buttons |= rc;
			break;
		}
		ms_update(newx,newy,buttons);
	}
	return(ESUCCESS);
}

int
glms_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {
	case FC_INITIALIZE:
		return(ESUCCESS);
		break;
	case FC_OPEN:
		iob->Flags |= F_SCAN;
		return(ESUCCESS);
	case FC_CLOSE:
	case FC_IOCTL:
		return(ESUCCESS);
	case FC_POLL:
		return(glms_poll(iob));
		break;
	case FC_GETREADSTATUS:
	case FC_READ:
		/* XXX -- needs work */
		return(iob->ErrorNumber = EINVAL);
	case FC_WRITE:
	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

int
ms_update(int newx, int newy, int nb)
{
	extern struct gfxgui gfxgui;
	int buttons;

	if (newx != -1)
		gfxgui.htp->x = newx;
	if (newy != -1)
		gfxgui.htp->y = newy;

	guiCheckMouse(0,0,nb);		/* check new pos */

}
