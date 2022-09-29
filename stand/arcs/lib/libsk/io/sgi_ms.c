/* SGI mouse driver for standalone.
 */

#ident "$Revision: 1.34 $"

#include <arcs/hinv.h>
#include <arcs/errno.h>
#include <arcs/eiob.h>
#include <arcs/io.h>
#include <saio.h>
#include <gfxgui.h>
#include <libsc.h>
#include <libsk.h>
#include <gfxgui.h>

#if IP22 || IP26 || IP28 || IP30 || PCMOUSE || IP32 || SN0
/* PC mouse */
#define PCMOUSE
#define MSBYTES		3
#define MSWARP		2
#define NO_ISSYNC	1
#define notsync(md)	((md.report[0]&0x30) != (((md.report[1]&0x80) >> 3) | \
						 ((md.report[2]&0x80) >> 2)))
#define deltax(md)	(md.report[1])
#define deltay(md)	(-(md.report[2]))
#define but(md)		((((md.report[0]&6)>>1)|((md.report[0]&1)<<2)) ^ 7)
#else
/* Mouse Systems/Indigo mouse */
#define MSBYTES		5
#define MSWARP		2
#define issync(C)	(((C)&0xF8)==0x80)
#define notsync(md)	(0)
#define deltax(md)	(md.report[1]+md.report[3])
#define deltay(md)	(-(md.report[2]+md.report[4]))
#define but(md)		(md.report[0] & 0x07)
#endif

static ULONG sgimsfd;

#ifndef PCMOUSE
static COMPONENT sgims = {
	PeripheralClass,		/* Class */
	PointerPeripheral,		/* Peripheral */
	(IDENTIFIERFLAG)(ReadOnly|Input), /* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	0				/* Identifier */
};

int
ms_config(COMPONENT *p)
{
	extern int _z8530_strat();

	p = AddChild(p,&sgims,(void *)NULL);
	if (p == (COMPONENT *)NULL) cpu_acpanic("sgi mouse");
	RegisterDriverStrategy(p,_z8530_strat);
	return(ESUCCESS);
}
#endif

static struct md {
	int state;
	int dropsync;
	signed char report[MSBYTES];
	/* 0 - button state (bit 1,2,3 are right, middle, and left)
	 * 1 - X delta 1
	 * 2 - Y delta 1
	 * 3 - X delta 2		(SGI mouse only)
	 * 4 - Y delta 2		(SGI mouse only)
	 */
} md;

/* init mouse data
 */
void
ms_install(void)
{
	bzero(&md,sizeof(struct md));
	md.report[0] = 0x07;		/* all buttons off */
}

/*  Mouse state machine.  Send data when 5 bytes come ok.  Use button btye
 * to sync on dropped characters.
 */
void
ms_input(int c)
{
	extern struct htp_state *htp;
	int dx,dy,buttons;

#ifndef NO_ISSYNC
	if (issync(c))
		md.state = 0;
#endif

	if (md.dropsync) {
		md.dropsync = 0;
		return;
	}

	md.report[md.state++] = c;

	if (md.state >= MSBYTES) {
		md.state = 0;

		/* if sync byte looks funny, drop this packet */
		if (notsync(md)) {
			md.dropsync = 1;
			return;
		}

		dx = deltax(md) * MSWARP;
		dy = deltay(md) * MSWARP;
		buttons = but(md);

		guiCheckMouse(dx,dy,buttons);		/* check new pos */
	}
}

/* _mspoll -- look at sgims fd and poll it.
 */
void
_mspoll(void)
{
	register struct eiob *io;

	io = get_eiob(sgimsfd);

	if (io && (io->iob.Flags&F_MS)) {
		io->iob.FunctionCode = FC_POLL;
		(*io->devstrat)(io->dev,&io->iob);
	}
}

void
_init_mouse(void)
{
	ULONG notfirst = sgimsfd;

	if (!isgraphic(StandardOut)) return;

	if (Open(cpu_get_mouse(),OpenReadOnly,&sgimsfd) == 0)
		if (notfirst == 0)
			_gfxsetms((gfxWidth()>>3)*5,((gfxHeight()>>3)*3));
}
