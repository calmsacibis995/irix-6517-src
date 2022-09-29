/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Management System
 *
 *	$Revision: 1.13 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <osfcn.h>
#include <math.h>
#include <string.h>
#include <bstring.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"

void
drawlink(register RING *rp, register float radi)
{
	float v[2];
	register Coord s_rd;
	register double radang;
	register Coord delta;
	register float x, y, x2, y2;

	radang = rp->radian;
	delta = FV.RINGWindow->RINGView->Delta;
	s_rd = (Coord)radi-RINGGAP;

	switch (rp->blt) {
	case LT_WRAP:
		x = (float)cos(radang)*(radi-HALFGAP);
		y = (float)sin(radang)*(radi-HALFGAP);
		c3i(rp->blcp); linewidth(1);
		arc(x, y, HALFGAP, (Angle)rp->ang, (Angle)(rp->ang+1800));
		break;

	case LT_STAR:
		c3i(rp->blcp);
		linewidth(1);
		if (rp->next->flt == LT_STAR || rp->next->blt == LT_STAR)
			arc(0, 0, radi+HALFGAP, (Angle)(rp->ang-delta),
				 (Angle)rp->ang);
		else {
			register double radang_3 = RAD(delta);

			bgnline();

			v[0] = (float)cos(radang-radang_3)*radi;
			v[1] = (float)sin(radang-radang_3)*radi;

			v2f(v);
			v[0] = (float)cos(radang)*((Coord)radi+HALFGAP);
			v[1] = (float)sin(radang)*((Coord)radi+HALFGAP);
			v2f(v);
			endline();
		}
		break;

	case LT_STARWRAP:
#ifdef DOSTARWRAP
		c3i(rp->blcp);
		x2 = (float)cos(radang)*((Coord)radi+HALFGAP+QUATERGAP);
		y2 = (float)sin(radang)*((Coord)radi+HALFGAP+QUATERGAP);
		circ(x2, y2, QUATERGAP);
#endif
		break;
	}

	c3i(rp->flcp);
	setlinestyle(rp->fls);
	switch (rp->flt) {
	case LT_LINK: {
		int q = 1;
		RING *tp = rp;
		while (tp != tp->next &&
		      (tp->next->flt == LT_STAR || tp->next->blt == LT_STAR)) {
			q++;
			tp = tp->next;
		}

		linewidth(2);
		arc(0, 0, radi, (Angle)(rp->ang-delta*q), (Angle)rp->ang);
		linewidth(1);
		/*
		 * XXX - even if not datt, secondary path shouldn't
		 *	 affected theoretically.
		 */
		c3i(rp->fslcp);
		setlinestyle(rp->fsls);
		arc(0, 0, s_rd, (Angle)(rp->ang-delta*q), (Angle)rp->ang);
	} break;

	case LT_WRAP:
		x = (float)cos(radang)*(radi-HALFGAP);
		y = (float)sin(radang)*(radi-HALFGAP);
		linewidth(1);
		arc(x, y, HALFGAP, (Angle)(rp->ang-1800), (Angle)(rp->ang));
		break;

	case LT_TWIST: {
		register double radang_2 = RAD(delta);

		bgnline();
			v[0] = (float)cos(radang)*radi;
			v[1] = (float)sin(radang)*radi;
			v2f(v);
			v[0] = (float)cos(radang-radang_2)*s_rd;
			v[1] = (float)sin(radang-radang_2)*s_rd;
			v2f(v);
		endline();
		bgnline();
			v[0] = (float)cos(radang-radang_2)*radi;
			v[1] = (float)sin(radang-radang_2)*radi;
			v2f(v);
			v[0] = (float)cos(radang)*s_rd;
			v[1] = (float)sin(radang)*s_rd;
			v2f(v);
		endline();
	} break;

	case LT_STAR:
		linewidth(1);
		if (rp->next->flt == LT_STAR || rp->next->blt == LT_STAR)
			arc(0, 0, radi+HALFGAP, (Angle)(rp->ang-delta),
				 (Angle)rp->ang);
		else {
			register double radang_3 = RAD(delta);

			bgnline();
//			v[0] = (float)cos(rp->next->radian)*radi;
//			v[1] = (float)sin(rp->next->radian)*radi;
			v[0] = (float)cos(radang-radang_3)*radi;
			v[1] = (float)sin(radang-radang_3)*radi;
			v2f(v);
			v[0] = (float)cos(radang)*((Coord)radi+HALFGAP);
			v[1] = (float)sin(radang)*((Coord)radi+HALFGAP);
			v2f(v);
			endline();
		}
		break;

	case LT_STARWRAP:
#ifdef DOSTARWRAP
		x2 = (float)cos(radang)*((Coord)radi+HALFGAP+QUATERGAP);
		y2 = (float)sin(radang)*((Coord)radi+HALFGAP+QUATERGAP);
		circ(x2, y2, QUATERGAP);
#endif
		break;
	}

	if ((rp->smac != 0) && (rp->datt != 0) && (rp->rooted != 0)) {
		x2 = (float)cos(radang)*(s_rd);
		y2 = (float)sin(radang)*(s_rd);

		c3i(lgreycol);
		bgnline();
			v[0] = (float)cos(radang)*radi;
			v[1] = (float)sin(radang)*radi;
			v2f(v);
			v[0] = x2; v[1] = y2; v2f(v);
		endline();
		circf(x2, y2, s_rd/100.0);
	}
}

#define SHADOWFACT	0.50

void
workstation(register RING *rp, register float radi)
{
	register float x, y;
	register double radang;
	register double len;
	float y_pmac;

	radang = rp->radian;
	if (rp->flt > LT_TWIST)
		radi += HALFGAP;
	x = (float)cos(radang)*radi;
	y = y_pmac = (float)sin(radang)*radi;

	pushmatrix();
	translate(x, y, 0.);
	scale(FV.sfact, FV.sfact, FV.sfact);

	rotate((Angle)200,'x');
	rotate((Angle)300,'y');

	setlinestyle(0);
	if (rp->ghost&SHADOW) {
		scale(SHADOWFACT, SHADOWFACT, SHADOWFACT);
		drawmonitor(rp);
		scale(1.0/SHADOWFACT, 1.0/SHADOWFACT, 1.0/SHADOWFACT);
	} else if (rp->sm.r_nodeclass == SMT_STATION) {
		drawbase(rp);
		drawmonitor(rp);
		drawkeybd(rp);
	} else {
		drawcc(rp);
	}

	if (FV.picked == PICK_MODE) {
		popmatrix();
		return;
	}

	c3i(rp == FV.magrp ? orangecol : rp->acp);
	if (FV.FullScreenMode) {
		/*
		 * The workstation picked for magnification doesn't get
		 * its address displayed over its icon if it has SIF info.
		 */
		if (rp != FV.magrp || !(rp->sm.sif & FSI_SIF)) {
			register float tf = 0.05/FV.sfact;

			len = (double)StringWidth("QQQQ")*tf;
			x = (float)cos(radang)*len;
			y = (float)sin(radang)*len;

			translate(-x, -y, 0.);
			scale(tf, tf, tf);
			if (x > 0.0) {
				rotate((Angle)rp->ang, 'z');
				DrawStringCenterLeft(getinfolbl(&rp->sm));
			} else {
				rotate((Angle)(rp->ang-1800), 'z');
				DrawStringCenterRight(getinfolbl(&rp->sm));
			}
		}
	} else {
		char *a;
		a = getinfolbl(&rp->sm);
		len = (float)strlen(a);
		popmatrix();
		pushmatrix();
		drawstr(x-(len/4.0), y, a);
	}
	popmatrix();

	if ((rp->smac != 0) && (rp->datt != 0) && (rp->rooted != 0)) {
		pushmatrix();
		x = (float)cos(radang)*((Coord)radi-RINGGAP);
		y = (float)sin(radang)*((Coord)radi-RINGGAP);
		translate(x, y, 0.);
		scale(FV.sfact, FV.sfact, FV.sfact);

		rotate((Angle)200,'x');
		rotate((Angle)300,'y');
		drawmac2(rp);

		c3i(rp == FV.magrp ? orangecol : rp->acp);
		if (FV.FullScreenMode) {
			if (rp != FV.magrp || !(rp->dm.sif & FSI_SIF)) {
				register float tf = 0.05/FV.sfact;

				len = (double)StringWidth("QQQQ")*tf;
				x = (float)cos(radang)*len;
				y = (float)sin(radang)*len;

				translate(x, y, 0.);
				scale(tf, tf, tf);
				if (x > 0.0) {
				    rotate((Angle)rp->ang, 'z');
				    DrawStringCenterRight(getinfolbl(&rp->dm));
				} else {
				    rotate((Angle)(rp->ang-1800), 'z');
				    DrawStringCenterLeft(getinfolbl(&rp->dm));
				}
			}
		} else {
			float diff;
			char *a;
			a = getinfolbl(&rp->dm);
			len = (float)strlen(a);
			popmatrix();
			pushmatrix();
			diff = y - y_pmac;
			if (diff > 0.0 && diff < 1.0)
				y += 1.0;
			else if (diff <= 0.0 && diff > -1.0)
				y -= 1.0;
			drawstr(x-(len/4.0), y, a);
		}
		popmatrix();
	}
}

static float base_front[][3] = {
	{-8., -10., 1.},
	{ 8., -10., 1.},
	{ 8.,  -7., 1.},
	{-8.,  -7., 1.},
};
static float base_side[][3] = {
	{-8., -10.,   1.},
	{-8., -10., -10.},
	{-8.,  -7., -10.},
	{-8.,  -7.,   1.},
};
static float base_top[][3] = {
	{-8., -7.,   1.},
	{-8., -7., -10.},
	{ 8., -7., -10.},
	{ 8., -7.,   1.},
};

void
drawbase(RING *rp)
{
	c3i(rp->pcp);
	bgnpolygon(); DOV3FS(base_front); endpolygon();
	bgnpolygon(); DOV3FS(base_side);  endpolygon();
	bgnpolygon(); DOV3FS(base_top);   endpolygon();

	c3i(rp->lcp);
	bgnclosedline(); DOV3FS(base_front); endclosedline();
	bgnclosedline(); DOV3FS(base_side);  endclosedline();
	bgnclosedline(); DOV3FS(base_top);   endclosedline();
}

/* front of concentrator */
static float cc_front[][3] = {
	{-12., -5., 10.},
	{ 12., -5., 10.},
	{ 12.,  3., 10.},
	{-12.,  3., 10.},
};
/* top of concentrator */
static float cc_top[][3] = {
	{-12.,  3.,  10.},
	{-12.,  3., -5.},
	{ 12.,  3., -5.},
	{ 12.,  3.,  10.},
};
/* left side of concentrator */
static float cc_side[][3] = {
	{-12., -5.,  10.},
	{-12., -5., -5.},
	{-12.,  3., -5.},
	{-12.,  3.,  10.},
};
/* screen */
static float cc_scr[][3] = {
	{-10., -3., 10.},
	{ -2., -3., 10.},
	{ -2.,  1., 10.},
	{-10.,  1., 10.},
};

void
drawcc(RING *rp)
{
	register int ports, i, j;
	register SINFO *si;
	register PARM_PD_PHY *pdp;
	register int rows, cols;

	c3i(rp == FV.magrp ? yellowcol : rp->pcp);
	bgnpolygon(); DOV3FS(cc_front); endpolygon();
	bgnpolygon(); DOV3FS(cc_side);  endpolygon();
	bgnpolygon(); DOV3FS(cc_top);   endpolygon();

	c3i(rp->lcp);
	bgnclosedline(); DOV3FS(cc_front); endclosedline();
	bgnclosedline(); DOV3FS(cc_top);   endclosedline();
	bgnclosedline(); DOV3FS(cc_side);  endclosedline();

	c3i(rp == FV.pickrp ? yellowcol : rp->scrcp);
	bgnpolygon(); DOV3FS(cc_scr); endpolygon();
	c3i(rp->lcp);
	bgnclosedline(); DOV3FS(cc_scr); endclosedline();

	if ((rp->sm.sif&FSI_SIF) != 0) {
		si = &rp->sm;
	} else if ((rp->dm.sif&FSI_SIF) != 0) {
		si = &rp->dm;
	} else
		return;

	ports = si->r_master_ct + si->r_nonmaster_ct;
	if (ports <= 10) {
		cols = ports;
		rows = 1;
	} else if (ports <= 20) {
		cols = ports/2;
		rows = 2;
	} else {
		cols = 10;
		rows = (ports/cols)+1;
		if (rows > 2)
			rows = 2;
	}

	if ((pdp = si->pds) == 0)
		return;
	for (i = 0; (i < rows && ports > 0); i++) {
		register float b, h;

		b = -3.0+((4.0/((float)rows))*((float)i)) + 0.5;
		h = -3.0+((4.0/((float)rows))*((float)(i+1))) - 0.5;
		for (j = 0; (j < cols && ports > 0); j++, ports--, pdp++) {
			register float l, r;
			float ccl[4][3];

			l = -1.0+((12.0/((float)cols))*((float)j)) + 0.2;
			r = l+0.8;

			ccl[0][0] = l; ccl[0][1] = h; ccl[0][2] = 10.0;
			ccl[1][0] = l; ccl[1][1] = b; ccl[1][2] = 10.0;
			ccl[2][0] = r; ccl[2][1] = b; ccl[2][2] = 10.0;
			ccl[3][0] = r; ccl[3][1] = h; ccl[3][2] = 10.0;

			if (pdp->conn_state == PC_ACTIVE)
				c3i(greencol);
			else
				c3i(redcol);
			bgnpolygon(); DOV3FS(ccl); endpolygon();
			c3i(rp->lcp);
			bgnclosedline(); DOV3FS(ccl); endclosedline();
		}
	}
}

/* front of monitor */
static float mon_front[][3] = {
	{-8., 8.,1.},
	{-8.,-6.,1.},
	{ 8.,-6.,1.},
	{ 8., 8.,1.},
};
/* top of monitor */
static float mon_top[][3] = {
	{-8.,8., 1.},
	{-7.,7.,-9.},
	{ 7.,7.,-9.},
	{ 8.,8., 1.},
};
/* left side of monitor */
static float mon_side[][3] = {
	{-8.,  8.,  1.},
	{-7.,  7., -9.},
	{-7., -5., -9.},
	{-8., -6.,  1.},
};
/* screen */
static float mon_screen[][3] = {
	{-6., 6.,1.},
	{-6.,-4.,1.},
	{ 6.,-4.,1.},
	{ 6., 6.,1.},
};
/* light on front of monitor */
static float mon_llite[][3] = {
	{4.0,-4.8,1.},
	{4.0,-5.5,1.},
	{7.0,-5.5,1.},
	{7.0,-4.8,1.},
};

void
drawmonitor(RING *rp)
{
	c3i(rp == FV.magrp ? yellowcol : rp->pcp);
	bgnpolygon(); DOV3FS(mon_front); endpolygon();
	bgnpolygon(); DOV3FS(mon_side);  endpolygon();
	bgnpolygon(); DOV3FS(mon_top);   endpolygon();

	c3i(rp->lcp);
	bgnclosedline(); DOV3FS(mon_front); endclosedline();
	bgnclosedline(); DOV3FS(mon_top);   endclosedline();
	bgnclosedline(); DOV3FS(mon_side);  endclosedline();

	c3i(rp == FV.pickrp ? yellowcol : rp->scrcp);
	bgnpolygon(); DOV3FS(mon_screen); endpolygon();
	c3i(blackcol);
	bgnclosedline(); DOV3FS(mon_screen); endclosedline();

	c3i(rp->litecp);
	bgnpolygon(); DOV3FS(mon_llite); endpolygon();
}


/* The box on the secondary ring for a dual-MAC system */
#define MAC2_EDGE 3.0
/* front */
static float mac2_front[][3] = {
	{-MAC2_EDGE, MAC2_EDGE,1.},
	{-MAC2_EDGE,-MAC2_EDGE,1.},
	{ MAC2_EDGE,-MAC2_EDGE,1.},
	{ MAC2_EDGE, MAC2_EDGE,1.},
};
/* top */
static float mac2_top[][3] = {
	{-MAC2_EDGE,MAC2_EDGE, 1.},
	{-MAC2_EDGE,MAC2_EDGE,-MAC2_EDGE},
	{ MAC2_EDGE,MAC2_EDGE,-MAC2_EDGE},
	{ MAC2_EDGE,MAC2_EDGE, 1.},
};
/* left side */
static float mac2_side[][3] = {
	{-MAC2_EDGE,  MAC2_EDGE,  1.},
	{-MAC2_EDGE,  MAC2_EDGE, -MAC2_EDGE},
	{-MAC2_EDGE, -MAC2_EDGE, -MAC2_EDGE},
	{-MAC2_EDGE, -MAC2_EDGE,  1.},
};

void
drawmac2(RING *rp)
{
	c3i(rp == FV.magrp ? yellowcol : rp->pcp);
	bgnpolygon(); DOV3FS(mac2_front); endpolygon();
	bgnpolygon(); DOV3FS(mac2_side);  endpolygon();
	bgnpolygon(); DOV3FS(mac2_top);   endpolygon();

	c3i(rp->lcp);
	bgnclosedline(); DOV3FS(mac2_front); endclosedline();
	bgnclosedline(); DOV3FS(mac2_top);   endclosedline();
	bgnclosedline(); DOV3FS(mac2_side);  endclosedline();
}

/* side of keybd */
static float kbd_side[][3] = {
	{-8., -10.,  3.},
	{-8., -10., 10.},
	{-8.,  -9., 10.},
	{-8.,  -8.,  3.},
};
/* top of keybd */
static float kbd_top[][3] = {
	{-8., -9., 10.},
	{-8., -8.,  3.},
	{ 8., -8.,  3.},
	{ 8., -9., 10.},
};
/* front of keybd */
static float kbd_front[][3] = {
	{-8.,  -9., 10.},
	{ 8.,  -9., 10.},
	{ 8., -10., 10.},
	{-8., -10., 10.},
};

void
drawkeybd(RING *rp)
{
	c3i(rp->pcp);
	bgnpolygon(); DOV3FS(kbd_side);  endpolygon();
	bgnpolygon(); DOV3FS(kbd_top);   endpolygon();
	bgnpolygon(); DOV3FS(kbd_front); endpolygon();

	c3i(rp->lcp);
	bgnclosedline(); DOV3FS(kbd_side);  endclosedline();
	bgnclosedline(); DOV3FS(kbd_top);   endclosedline();
	bgnclosedline(); DOV3FS(kbd_front); endclosedline();
}

#define GATETRAJ	15
#define GATEFRMWIDTH	4.0
#define GATEFRMLEN	1.0
#define GATELEN		10.0
#define GATEHEIGHT	8.0
#define GATEBASE	0.0

static int gate_inited = 0;
float gate_frame_int[GATETRAJ][2][3];
float gate_frame_ext[GATETRAJ][2][3];
float gate_frame_front[GATETRAJ][2][3];
float gate_gate[GATETRAJ][3];

void
setup_gate()
{
	register int i;
	register float x1, y1;
	register float x, y, z;
	float ang = M_PI;
	float delta = (M_PI)/(float)(GATETRAJ-3);

	x = GATELEN/2.0;
	y = GATEBASE;
	z = GATEFRMWIDTH;

	gate_frame_int[0][0][X] = -x;
	gate_frame_int[0][0][Y] = y;
	gate_frame_int[0][0][Z] = 0.0;
	gate_frame_int[0][1][X] = -x;
	gate_frame_int[0][1][Y] = y;
	gate_frame_int[0][1][Z] = -z;

	gate_frame_int[GATETRAJ-1][0][X] = x;
	gate_frame_int[GATETRAJ-1][0][Y] = y;
	gate_frame_int[GATETRAJ-1][0][Z] = 0.0;
	gate_frame_int[GATETRAJ-1][1][X] = x;
	gate_frame_int[GATETRAJ-1][1][Y] = y;
	gate_frame_int[GATETRAJ-1][1][Z] = -z;

	gate_frame_ext[0][0][X] = -x - GATEFRMLEN;
	gate_frame_ext[0][0][Y] = y;
	gate_frame_ext[0][0][Z] = 0.0;
	gate_frame_ext[0][1][X] = -x - GATEFRMLEN;
	gate_frame_ext[0][1][Y] = y;
	gate_frame_ext[0][1][Z] = -z;

	gate_frame_ext[GATETRAJ-1][0][X] = x + GATEFRMLEN;
	gate_frame_ext[GATETRAJ-1][0][Y] = y;
	gate_frame_ext[GATETRAJ-1][0][Z] = 0.0;
	gate_frame_ext[GATETRAJ-1][1][X] = x + GATEFRMLEN;
	gate_frame_ext[GATETRAJ-1][1][Y] = y;
	gate_frame_ext[GATETRAJ-1][1][Z] = -z;

	gate_frame_front[0][0][X] = -x;
	gate_frame_front[0][0][Y] = y;
	gate_frame_front[0][0][Z] = 0.0;
	gate_frame_front[0][1][X] = -x - GATEFRMLEN;
	gate_frame_front[0][1][Y] = y;
	gate_frame_front[0][1][Z] = 0.0;

	gate_frame_front[GATETRAJ-1][0][X] = x;
	gate_frame_front[GATETRAJ-1][0][Y] = y;
	gate_frame_front[GATETRAJ-1][0][Z] = 0.0;
	gate_frame_front[GATETRAJ-1][1][X] = x+GATEFRMLEN;
	gate_frame_front[GATETRAJ-1][1][Y] = y;
	gate_frame_front[GATETRAJ-1][1][Z] = 0.0;

	gate_gate[0][X] = -x;
	gate_gate[0][Y] = y;
	gate_gate[0][Z] = 0.0;

	gate_gate[GATETRAJ-1][X] = x;
	gate_gate[GATETRAJ-1][Y] = y;
	gate_gate[GATETRAJ-1][Z] = 0.0;

	for (i = 1; i < GATETRAJ-1; i++, ang -= delta) {
		x = (GATELEN/2.0)*cos(ang);
		y = GATEBASE + (GATELEN/2.0)*sin(ang) + GATEHEIGHT;

		x1 = (GATELEN/2.0+GATEFRMLEN)*cos(ang);
		y1 = GATEBASE + (GATELEN/2.0+GATEFRMLEN)*sin(ang) + GATEHEIGHT;

		gate_frame_int[i][0][X] = x;
		gate_frame_int[i][0][Y] = y;
		gate_frame_int[i][0][Z] = 0.0;
		gate_frame_int[i][1][X] = x;
		gate_frame_int[i][1][Y] = y;
		gate_frame_int[i][1][Z] = -z;

		gate_frame_ext[i][0][X] = x1;
		gate_frame_ext[i][0][Y] = y1;
		gate_frame_ext[i][0][Z] = 0.0;
		gate_frame_ext[i][1][X] = x1;
		gate_frame_ext[i][1][Y] = y1;
		gate_frame_ext[i][1][Z] = -z;

		gate_frame_front[i][0][X] = x;
		gate_frame_front[i][0][Y] = y;
		gate_frame_front[i][0][Z] = 0.0;
		gate_frame_front[i][1][X] = x1;
		gate_frame_front[i][1][Y] = y1;
		gate_frame_front[i][1][Z] = 0.0;

		gate_gate[i][X] = x;
		gate_gate[i][Y] = y;
		gate_gate[i][Z] = 0.0;
	}
}

#define DFLT_XROT 200
#define DFLT_YROT 300
#define DFLT_XROTPRIM 0
static Angle xrot = DFLT_XROT;
static Angle yrot = DFLT_YROT;
static Angle xrotprim = DFLT_XROTPRIM;
static Coord yadj = 0.0;
void
drawgate(Coord radi, Coord sang, char *gatename, int devnum)
{
	register int i;
	register float x, y;
	register double radang;
	float btmp[3];

	if (!gate_inited) {
		setup_gate();
		gate_inited = 1;
	}

	radang = RAD(sang);
	if (FV.FullScreenMode || FV.RingNum < SIZETHRESHOLD) {
		radi += HALFGAP+QUATERGAP + yadj;
	} else {
		radi += RINGGAP + yadj;
	}
	x = (float)cos(radang)*radi;
	y = (float)sin(radang)*radi;

	if (devnum == FV.DevNum)
		goto dgonly;

	pushmatrix();
	translate(x, y, 0.);
	scale(FV.sfact, FV.sfact, FV.sfact);
	rotate(xrot,'x'); rotate(yrot,'y');
	setlinestyle(0);

	c3i(whitecol);
	bgnpolygon();
	for (i = 0; i < GATETRAJ; i++)
		v3f(gate_gate[i]);
	endpolygon();

	c3i(greycol);
	bgnpolygon();
		v3f(gate_frame_int[0][0]);
		v3f(gate_frame_int[0][1]);
		v3f(gate_frame_int[GATETRAJ-1][1]);
		v3f(gate_frame_int[GATETRAJ-1][0]);
	endpolygon();

	c3i(blackcol);
	bgnclosedline();
		v3f(gate_frame_int[0][0]);
		v3f(gate_frame_int[0][1]);
		v3f(gate_frame_int[GATETRAJ-1][1]);
		v3f(gate_frame_int[GATETRAJ-1][0]);
	endclosedline();

	for (i = 0; i < GATETRAJ - 1; i++) {
		c3i(greycol);
		bgnpolygon();
		v3f(gate_frame_int[i][0]);
		v3f(gate_frame_int[i][1]);
		v3f(gate_frame_int[i+1][1]);
		v3f(gate_frame_int[i+1][0]);
		endpolygon();

		c3i(blackcol);
		bgnclosedline();
		v3f(gate_frame_int[i][0]);
		v3f(gate_frame_int[i][1]);
		v3f(gate_frame_int[i+1][1]);
		v3f(gate_frame_int[i+1][0]);
		endclosedline();
	}

	for (i = 0; i < GATETRAJ - 1; i++) {
		c3i(greycol);
		bgnpolygon();
		v3f(gate_frame_ext[i][0]);
		v3f(gate_frame_ext[i][1]);
		v3f(gate_frame_ext[i+1][1]);
		v3f(gate_frame_ext[i+1][0]);
		endpolygon();

		c3i(blackcol);
		bgnclosedline();
		v3f(gate_frame_ext[i][0]);
		v3f(gate_frame_ext[i][1]);
		v3f(gate_frame_ext[i+1][1]);
		v3f(gate_frame_ext[i+1][0]);
		endclosedline();
	}

	for (i = 0; i < GATETRAJ - 1; i++) {
		c3i(greycol);
		bgnpolygon();
		v3f(gate_frame_front[i][0]);
		v3f(gate_frame_front[i][1]);
		v3f(gate_frame_front[i+1][1]);
		v3f(gate_frame_front[i+1][0]);
		endpolygon();

		c3i(blackcol);
		bgnclosedline();
		v3f(gate_frame_front[i][0]);
		v3f(gate_frame_front[i][1]);
		v3f(gate_frame_front[i+1][1]);
		v3f(gate_frame_front[i+1][0]);
		endclosedline();
	}

	/* draw door */
	rotate(xrotprim,'x');
	c3i(goldcol);
	bgnpolygon();
	for (i = 0; i < GATETRAJ; i++) {
		v3f(gate_gate[i]);
	}
	endpolygon();

	c3i(blackcol);
	bgnclosedline();
	for (i = 0; i < GATETRAJ; i++) {
		v3f(gate_gate[i]);
	}
	endclosedline();

	btmp[Y] = gate_gate[0][Y];
	btmp[Z] = gate_gate[0][Z];
	for (i = 3; i < GATETRAJ-3; i++) {
		btmp[X] = gate_gate[i][X];
		bgnline(); v3f(btmp); v3f(gate_gate[i]); endline();
	}
	popmatrix();

dgonly:
	pushmatrix();
	int len = strlen(gatename);
	if (devnum == FV.DevNum)
		c3i(goldcol);
	else if (FV.sfact > 0.3)
		c3i(FVC_BLUE);
	else
		c3i(FVC_WHITE);

	fmsetfont(FV.font1->getFontHandle());
	cmov2(x-(len/4.0), y+GATEHEIGHT*FV.sfact);
	fmprstr(gatename);
	popmatrix();
}

#ifdef DEBUGGATES
static realDevNum = 0;
#endif
void
opengate(int gatepicked, Coord radi, Coord sang, Coord gdelta)
{
	register int i;
	Angle xrot_delta, yrot_delta;
	Angle xrotprim_delta;
	int steps = 40;
#define INCR		0.003
#define CAMFACT		2.0
#define PAUSEMSEC	2


#ifdef DEBUGGATES
	FV.DevNum = realDevNum;
#endif
#ifdef STASH
	gatepicked--;
	for (i = 0, j = 0; i < FV.Gates; i++, j++) {
		if (i == FV.DevNum) {
			j--;
			continue;
		}
		if (j == gatepicked) {
			FV.DevNum = i;
			break;
		}
	}
	sang -= (gatepicked*gdelta);
#else
	sang -= ((gatepicked-FV.DevNum)*gdelta);
#endif
#ifdef DEBUGGATES
	realDevNum = FV.DevNum;
	FV.DevNum = 0;
#endif

	pushmatrix();
	FV.RINGWindow->RINGView->setUniverse();
	frontbuffer(FALSE); backbuffer(TRUE);

	xrot_delta = DFLT_XROT/steps;
	yrot_delta = DFLT_YROT/steps;
	xrotprim_delta = 1200/steps;

	// keep size of gate constant.
	FV.sfact = 0.2;
	for (i = 1; i <= steps; i++) {
		xrot -= xrot_delta;
		yrot -= yrot_delta;
		xrotprim += xrotprim_delta;

#ifdef DEBUGGATES
		drawgate(radi, sang, FV.gatenames[realDevNum], gatepicked);
#else
		drawgate(radi, sang, FV.gatenames[FV.DevNum], gatepicked);
#endif
		swapbuffers();
	}

	Coord yadj_base = (GATEHEIGHT*1.5+GATEFRMLEN) * FV.sfact;
	for (i = 1; i <= steps; i++) {
		FV.sfact += ((float)i * CAMFACT * INCR);
		yadj = yadj_base - (GATEHEIGHT*1.5+GATEFRMLEN)*FV.sfact;

#ifdef DEBUGGATES
		drawgate(radi, sang, FV.gatenames[realDevNum], gatepicked);
#else
		drawgate(radi, sang, FV.gatenames[FV.DevNum], gatepicked);
#endif
		swapbuffers();
	}

	// reset gates
	xrot = DFLT_XROT;
	yrot = DFLT_YROT;
	xrotprim = DFLT_XROTPRIM;
	yadj = 0.0;

	c3i(whitecol);
	::clear();
	::swapbuffers();

	FV.RINGWindow->clearWindow();
	popmatrix();

	FV.DevNum = gatepicked;
	FV.Restart(FV.Agent);
}
