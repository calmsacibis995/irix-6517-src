#ident	"$Revision: 1.67 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <pon.h>
#include <libsc.h>
#include <libsk.h>

#if IP20
#define FOUND_NONE      0
#define FOUND_LG1       1
#define FOUND_GR2       2
#else
#define FOUND_NONE	0
#define FOUND_GR2	1
#define FOUND_NG1	2
#define FOUND_MGRAS	3
#endif

#if IP20 || IP22 || IP26 || IP28
extern int gr2_pon(char *base);
extern int Gr2ProbeAll(char *base[]);
#define DO_GR2	1
#endif

#if IP20
extern int lg1_pon(void);
extern int Lg1ProbeAll(void);
#define DO_LG1	1
#endif

#if IP22 && _MIPS_SIM != _ABI64				/* and IP24 */
extern int ng1_pon(char *base);
extern int Ng1ProbeAll(char *base[]);
#define DO_NG1 1
#endif

#if IP22 || IP28
#if !defined(IP24)
extern int mgras_pon(char *base);
extern int MgrasProbeAll(char *base[]);
#define DO_MGRAS 1
#endif
#endif

#if IP22 || IP26 || IP28
#define MAXGFX	2
#else
#define MAXGFX	1
#endif

static int probe_graphics_base(char *base[], int type[]);

#ifdef PROM
int
pon_graphics(void)
{
	int boards, i, badgfx = 0;
	char *base[MAXGFX];
	int  type[MAXGFX];

	/* 
	 * Initialize graphics with screen OFF and test for the ability
	 * to draw spans with patterns (the minimum functionality required
	 * to produce characters on the graphics console).
	 */
	bzero(type,sizeof(type));
	boards = probe_graphics_base(base,type);

	if (boards == 0)
		return FAIL_NOGRAPHICS;

	for (i = 0; i < boards; i++) {
#ifdef DO_MGRAS
		if (type[i] == FOUND_MGRAS) {
			if (mgras_pon(base[i]) < 0) {
				badgfx = 1;
			}	
		}
#endif
#ifdef DO_GR2
		if (type[i] == FOUND_GR2) {
			if (gr2_pon(base[i]) < 0) {
				badgfx = 1;
			}
		}
#endif
#ifdef DO_NG1
		if (type[i] == FOUND_NG1) {
			if (ng1_pon(base[i]) < 0) {
				badgfx = 1;
			}
		}
#endif
#ifdef DO_LG1
		if(type[i] == FOUND_LG1) {
			if(lg1_pon() < 0) {
				badgfx = 1;
			}
		}
#endif
	}

	/* init textport */
	if (badgfx || (_init_htp() == 0))
		return FAIL_BADGRAPHICS;

	return 0;
}
#endif /* PROM */

void
gfxreset(void)
{
	*(volatile unsigned int *)PHYS_TO_COMPATK1(CPU_CONFIG) &= ~CONFIG_GRRESET;
	flushbus();
	DELAY(5);
	*(volatile unsigned int *)PHYS_TO_COMPATK1(CPU_CONFIG) |= CONFIG_GRRESET;
	flushbus();
#if DO_NG1 && !IP24
	/*  On Indigo2, need to wait 30ms (be conservative) for REX PLL
	 * time to lock up.
	 */
	DELAY(30*1000);
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
	for (i=0; i < n; i++) {
		base[tmpi] = tmp[i];
		type[tmpi++] = FOUND_MGRAS;
	}
#endif

#ifdef DO_GR2
	n = Gr2ProbeAll(tmp);
	for (i=0; i < n; i++) {
		base[tmpi] = tmp[i];
		type[tmpi++] = FOUND_GR2;
	}
#endif
	/*
 	 * must delay at least 30ms due to RC delay on LG3 board
	 */
	DELAY( 30000 );

#ifdef DO_NG1
	n = Ng1ProbeAll(tmp);
	for (i=0; i < n; i++) {
		base[tmpi] = tmp[i];
		type[tmpi++] = FOUND_NG1;
	}
#endif
#ifdef DO_LG1
	if (Lg1ProbeAll()) {
		/* base[tmpi] = nothing, base not used for LIGHT */
		type[tmpi++] = FOUND_LG1;
	}
#endif
	
	return tmpi;
}

#ifndef PROM
int
probe_graphics(void)
{
	char *base[MAXGFX];
	int type[MAXGFX];

	return(probe_graphics_base(base,type));
}
#endif
