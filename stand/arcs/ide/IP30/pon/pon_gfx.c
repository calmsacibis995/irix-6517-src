#ident	"$Revision: 1.8 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <pon.h>
#include <libsc.h>
#include <libsk.h>

#define FOUND_NONE	0
#define FOUND_MGRAS	1
#define FOUND_ODSY	2

#ifdef IP30
#include "sys/mgrashw.h"
#include "sys/mgras.h"
extern int mgras_pon(char *base);
extern int mgras_pon_chkcfg(char *base);
extern int MgrasProbeAll(char *base[]);
#define DO_MGRAS 1

/* ODY support */
extern int odsy_pon(char *base);
extern int odsy_pon_chkcfg(char *base);
extern int odsyProbeAll(char *base[]);
#define DO_ODSY 1
#endif

#define MAXGFX	2

static int probe_graphics_base(char *base[], int type[]);

#ifdef PROM
int
pon_graphics(void)
{
	int boards, i, failflags = 0;
	char *base[MAXGFX];
	int  type[MAXGFX];

	/* 
	 * Initialize graphics with screen OFF and test for the ability
	 * to draw spans with patterns (the minimum functionality required
	 * to produce characters on the graphics console).
	 */
	bzero(type,sizeof(type));
	boards = probe_graphics_base(base, type);

	if (boards == 0)
		return FAIL_NOGRAPHICS;

	for (i = 0; i < boards; i++) {
#ifdef DO_MGRAS
		if (type[i] == FOUND_MGRAS) {
			if (mgras_pon(base[i]) < 0) {
				failflags |= FAIL_BADGRAPHICS;
			} else {
#ifdef IP30
				if (mgras_pon_chkcfg(base[i]) < 0)
					failflags |= FAIL_ILLEGALGFXCFG;
#endif
			}
                }
#endif
#ifdef DO_ODSY
		if (type[i] == FOUND_ODSY) {
			if (odsy_pon(base[i]) < 0) {
				failflags |= FAIL_BADGRAPHICS;
			} else {
				if (odsy_pon_chkcfg(base[i]) < 0)
					failflags |= FAIL_ILLEGALGFXCFG;
			}
                }
#endif
	}

	/* init textport */
	if (!(failflags & FAIL_BADGRAPHICS) && (_init_htp() == 0))
		failflags |= FAIL_BADGRAPHICS;

	return failflags;
}
#endif	/* PROM */

void
gfxreset(void)
{
	char *base[MAXGFX];
	int type[MAXGFX];
	int i, n;
	
	/*
	 * XXX this fct seems to be inactive, due to the
	 *     #ifdef NOTYET below and when it does become
	 *     active needs to have odsyReset incorporated
	 *     with the MgrasReset based on the gfx type
	 *     discovered.
	 */

#ifdef NOTYET
	bzero(type, sizeof(type));
	n = probe_graphics_base(base, type);
	for (i = 0; i < n; i++) {
		MgrasReset((mgras_hw *) base[i]);
	}
#endif
}

static int
probe_graphics_base(char *base[], int type[])
{
	char *tmp[MAXGFX];
	int n, i, tmpi=0;

	gfxreset();		/* first reset all graphics boards */

#ifdef DO_MGRAS
	n = MgrasProbeAll(tmp);
	for (i = 0; i < n; i++) {
		base[tmpi] = tmp[i];
		type[tmpi++] = FOUND_MGRAS;
	}
#endif
#ifdef DO_ODSY
	n = odsyProbeAll(tmp);
	for (i = 0; i < n; i++) {
		base[tmpi] = tmp[i];
		type[tmpi++] = FOUND_ODSY;
	}
#endif
	return tmpi;
}
