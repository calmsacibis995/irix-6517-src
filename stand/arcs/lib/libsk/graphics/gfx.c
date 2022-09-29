#ident	"lib/libsk/graphics/gfx.c:  $Revision: 1.148 $"

#include <sys/errno.h>
#include <sys/types.h>
#include <setjmp.h>
#include "saio.h"
#include "saioctl.h"
#include <arcs/hinv.h>
#include "stand_htport.h"
#include "standcfg.h"
#include <gfxgui.h>
#include <ctype.h>
#include <libsc.h>
#include <libsk.h>
#ifdef SN0
#include <sys/SN/SN0/ip27config.h>
#endif /* SN0 */

#define	GFX_ENV_VAR	"gfx"
#define	GFX_OK	        "alive"
#define	GFX_DEAD        "dead"

static int gfxISopen;
int gfxalive;

#if defined(IP22) || defined(IP28) || defined(IP30)
int _mgras_run_ide = 0;
#endif

static int _gfxReopen(IOBLOCK *, int );

extern struct htp_state *htp;

#if IPXX || IP22 || IP24 || IP26 || IP28 || IP30
#define MULTIHEAD
#endif

int gfxboard;			/* global Key so video(x) is unambiguous */

static int
htp_gfx_init(int force)
{
	extern struct standcfg standcfg[];
	struct htp_fncs *ptrbuf;
	struct standcfg *s;
	static int found;
	void *base;
	int x, y;
#ifdef MULTIHEAD
	struct standcfg *first = 0;
	int gX, board;
	void *fbase;
	char *p;
#endif

	if (found && !force)			/* We've already got one. */
		return 0;

	gfxboard = 0;

#ifdef MULTIHEAD
	gX = board = 0;

	if ((p=getenv("console")) && p && (*p == 'g') && isdigit(p[1]))
		gX = p[1] - '0';

	/*  Find the gXth graphics board.  If console is invalid, then
	 * fall back to the first found board.
	 */
	for (s = standcfg; s->install; s++) {
		if (!s->gfxprobe)
			continue;
		while (base = (*s->gfxprobe)(0)) {
			found = 1;
			if (board++ == gX) {
init_board:
				htp->hw = (void*)base;
				(*s->gfxprobe)(&htp->fncs);
				(*htp->fncs->init)((void*)base, &x, &y);
#if 1 /* XXX-TJN IDE was blanked */
				(*htp->fncs->blankscreen)((void*)base, 1);
#endif
				htp->xscreensize = x;
				htp->yscreensize = y;
				txMap(htp);
				return(0);
			}
			if (!first) {
				fbase = base;
				first = s;
			}
		}

		/* reset gfx drivers bard counter */
		(*s->gfxprobe)(&ptrbuf);
	}

	if (first) {
		s = first;
		base = fbase;
		goto init_board;
	}
#else
#ifdef SN0
	/*  Find the only graphics board and init textport on it.
	 *  Note that on SN0 systems, only one of the gfx standcfg
	 *  entries will have a valid gfxprobe function.
	 */
#else
	/*  Find the first graphics board and init textport on it.
	 */
#endif
	for (s = standcfg; s->install; s++) {
		if (!s->gfxprobe)
			continue;
		base = (void *)(*s->gfxprobe)(0);
		if (base) {
			found = 1;
			htp->hw = base;
			(*s->gfxprobe)(&htp->fncs);
			(*htp->fncs->init)(base, &x, &y);
			(*htp->fncs->blankscreen)(base, 1);
			htp->xscreensize = x;
			htp->yscreensize = y;
			txMap(htp);
			return 0;
		}

		/* reset gfx drivers bard counter */
		(*s->gfxprobe)(&ptrbuf);
	}
#endif

	return -1;		/* cannot find any gfx board! */
}

static void _ttyputc(int c)
{
	if (c == '\n')
		cpu_errputc('\r');
	cpu_errputc(c);
}

int
_init_htp(void)
{
	extern int gfx_ok;
#if defined(IP22) || defined(IP28) || defined(IP30)
        if (_mgras_run_ide) {
                gfx_ok = htp_gfx_init(1);
                return(gfx_ok);
        }
#endif

	_init_ttyputc(_ttyputc);

	if (!gfx_ok) {
		if (gfx_ok = (htp_gfx_init(0) != -1))
			htp_init();
	}
	/* XXX--used to rely on calling twice to populate htp vectors in gfxgui
	 */
	if (gfx_ok) 
		initGfxGui(htp);
	
	return(gfx_ok);
}

int
txPrint(char *c, int nc)
{
	if (htp_write(c, nc) == 0)
		return 0;
	return 1;
}

/*
 * Reset to a known state on soft reset
 */
static int
_gfxinit(void)
{
	return(gfxISopen = 0);
}

static int
_gfxopen(IOBLOCK *io)
{
#ifndef _TP_COLOR
	/*  Currently do not support extended console since color makes
	 * scrolling slow plus is kinda sorta large.
	 */
	if (io->Flags & F_ECONS)
		return(io->ErrorNumber = EINVAL);
#endif

	_init_htp();
	return _gfxReopen(io, 0);
}

static int
_gfxReopen(IOBLOCK *io, int dosetup)
{
#ifdef SN0
	if ((!SN00)) { /* on SN00 Unit might be pci slot number */
		/* validate unit (only allow 0) */
		if (io->Unit)
			return io->ErrorNumber = ENXIO;
	}
#else
	/* validate unit (only allow 0) */
        if (io->Unit)
                return io->ErrorNumber = ENXIO;
#endif
	
	/* make multiple opens do nothing */
	if (!dosetup && gfxISopen) {
		gfxISopen++;
		return ESUCCESS;
	}
	gfxalive = 0;
	if (htp_gfx_init(dosetup) == -1) {
		gfxISopen--;
		syssetenv(GFX_ENV_VAR, GFX_DEAD);
		return io->ErrorNumber = EIO;
	}
	
	/*
	 * Everything is working.
	 */
	if (getenv(GFX_ENV_VAR) == NULL)
		syssetenv(GFX_ENV_VAR, GFX_OK);
	gfxalive = 1;
	syssetenv(GFX_ENV_VAR, GFX_OK);
	gfxISopen++;
	
	return ESUCCESS;
}

/*ARGSUSED*/
static int
_gfxclose(IOBLOCK *io)
{
	if (gfxISopen == 1)
		gfxISopen = 0;
	return ESUCCESS;
}

static int
_gfxwrite(IOBLOCK *io)
{
	if (!gfxalive)
		return io->ErrorNumber = ENXIO;

	ioctl(StandardIn,TIOCCHECKSTOP,0);

	txPrint(io->Address, (int)io->Count);
	return ESUCCESS;
}

static int
_gfxioctl(IOBLOCK *io)
{
	if (!gfxalive)
		return io->ErrorNumber = ENXIO;

	switch ((__psint_t)io->IoctlCmd) {
	case TIOCREOPEN:
		/*
		** It has been decided that a re-open for the graphics
		** should reinitialize the graphics hardware.
		*/
		if (!io->IoctlArg)
			return _gfxReopen(io, 1);
		return(ESUCCESS);
	case GIOCSETJMPBUF:
		txSetJumpbuf((jmp_buf *)io->IoctlArg);
		break;
	case GIOCSETBUTSTR:
		txSetString((char *)io->IoctlArg);
		break;
	default:
		return io->ErrorNumber = EINVAL;
	}

	return 0;
}

/* ARCS - new stuff */
/*ARGSUSED*/
int
_gfx_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
		return(_gfxinit());

	case	FC_OPEN:
		return (_gfxopen (iob));

	case	FC_WRITE:
		return (_gfxwrite (iob)); 

	case	FC_CLOSE:
		return (_gfxclose (iob));

	case	FC_IOCTL:
		return (_gfxioctl(iob));

	case	FC_READ:
	default:
		return(iob->ErrorNumber = EINVAL);
	};
}

/* monitor template used by XXXXX/yy_init.c */
COMPONENT montmpl = {
	PeripheralClass,		/* Class */
	MonitorPeripheral,		/* Type */
	(IDENTIFIERFLAG)0,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	0				/* Identifier */
};

