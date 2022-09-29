/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	MapView class
 *
 *	$Revision: 1.5 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <math.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"

MapView::MapView()
{
	oldVang = FV.Vang;
	setUniverse();
}

void
MapView::setUniverse()
{
	perspective(50, 1.0, 400.0, 600.0);
	lookat(0., 0., 500., 0., 0., 0., 0);
}

#define MAP_RADIUS (RADIUS+2.5)
#define HND_RADIUS (RADIUS-3.3)
#define STA_RADIUS (0.5)
void
MapView::paint()
{
	char str[32];
	register int i;
	register RING *rp;
	register Coord x, y;

	drawHand(FALSE);

	for (i = FV.RingNum, rp = FV.ring; i >  0; i--, rp=rp->next)
		drawlink(rp, MAP_RADIUS);

	for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->next) {
		// Decide where to put the station.
		if (rp->flt > LT_TWIST) {
			x = (Coord)(cos(rp->radian) * (MAP_RADIUS+HALFGAP));
			y = (Coord)(sin(rp->radian) * (MAP_RADIUS+HALFGAP));
		} else {
			x = (Coord)(cos(rp->radian) * MAP_RADIUS);
			y = (Coord)(sin(rp->radian) * MAP_RADIUS);
		}
		// yellow if selected, otherwise grey or white.
		if (rp == FV.magrp)
			c3i(yellowcol);
		else
			c3i(rp->pcp);
		circf(x, y, STA_RADIUS);
		if (rp->smac) {
			x = (Coord)(cos(rp->radian) * (MAP_RADIUS-RINGGAP));
			y = (Coord)(sin(rp->radian) * (MAP_RADIUS-RINGGAP));
			circf(x, y, STA_RADIUS);
		}
	}

#ifdef CYPRESS_XGL
	fmsetfont(FV.font2->getFontHandle());
#else
	fmsetfont(FV.font2);
#endif
	// display number of UNAs
	sprintf(str, "%d", FV.RingNum);
	cmov2(-21.0, -21.0);
	c3i(yellowcol); fmprstr(str);
	c3i(ltbluecol); fmprstr(" node");
	if (FV.RingNum > 1)
		fmprstr("s");

	/* print ring state */
	if (FV.RingOpr & BEACONING) {
		c3i(redcol);	cmov2( 6.5, -21.0); fmprstr("BEACON");
	} else if (FV.RingOpr & CLAIMING) {
		c3i(whitecol);	cmov2(11.0, -21.0); fmprstr("CLAIM");
	} else if (FV.RingOpr & WRAPPED) {
		c3i(yellowcol);	cmov2(11.3, -21.0); fmprstr("WRAP");
	} else {
		c3i(ltbluecol); cmov2(12.0, -21.0); fmprstr("THRU");
	}
}

void
MapView::beginSelect(Point& p)
{
	continueSelect(p);
}

void
MapView::continueSelect(Point& p)
{
	Coord ang;

	transform(p);
	ang = (float)DEG(atan2((double)p.getY(), (double)p.getX()));
	if (FV.Vang == ang)
		return;
	FV.Vang = ang;
	FV.MAPWindow->drawHand();
	FV.RINGWindow->redrawWindow();
}

void
MapView::endSelect(Point& p)
{
	continueSelect(p);
}

void
MapView::transform(Point& p)
{
	float px = p.getX();
	float py = p.getY();

	p.set(px-getExtentX()/2.0, py-getExtentY()/2.0);

}

void
MapView::drawHand(int doclr)
{
	float v[2];

	setlinestyle(0);
	linewidth(1);

	if (doclr) {
		c3i(blackcol);
		bgnline();
			v[0] = 0.0; v[1] = 0.0; v2f(v);
			v[0] = HND_RADIUS*(float)(cos(RAD(oldVang)));
			v[1] = HND_RADIUS*(float)(sin(RAD(oldVang)));
			v2f(v);
		endline();
		circf(0, 0, 0.5);
	}
	oldVang = FV.Vang;

	if (FV.FullScreenMode)
		return;

	c3i(redcol);
	bgnline();
		v[0] = 0.0; v[1] = 0.0; v2f(v);
		v[0] = HND_RADIUS*(float)(cos(RAD(FV.Vang)));
		v[1] = HND_RADIUS*(float)(sin(RAD(FV.Vang)));
		v2f(v);
	endline();
	circf(0, 0, 0.5);
}
