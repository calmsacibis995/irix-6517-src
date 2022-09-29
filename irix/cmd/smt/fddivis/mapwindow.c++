/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

//#include <events.h>
#include "fddivis.h"

void
MapWindow::open()
{
#define MAPX1 (float)(AEDGE)
#define MAPX2 (float)(SCRXMAX-SCRYMAX-AEDGE+1)
#define MAPY2 (float)(SCRYMAX-(BNR+AEDGE))
#define MAPY1 (float)(MAPY2-(MAPX2-MAPX1)+AEDGE-1)
	fudge(0.0, 0.0);
	prefposition(MAPX1, MAPX2, MAPY1, MAPY2);
	setName("fddivis Map");
	setTitle("fddivis Map");
	setIconTitle("FV Map");
	setStartUpNoClose(TRUE);
	setStartUpNoQuit(TRUE);
	setNoResize(TRUE);
	setHandleClose(TRUE);

#ifdef CYPRESS_XGL
	setopaque();
	picksize(1,1);
	shademodel(GOURAUD);
	setOverlayTrue();
	tkWindow::open();
#else
	tkWindow::open();
	setopaque();
	picksize(1,1);
	doublebuffer();
	shademodel(GOURAUD);
	overlay(2);
	gconfig();
#endif

	clearWindow();
	swapbuffers();

	float x,y;
	getsize(&x,&y);
	MAPView = new MapView;
	MAPView->setBounds(Box2(0.0,0.0,x,y));
	addView(*MAPView);
}

void
MapWindow::resize()
{
	reshapeviewport();
	updateScreenArea();
}

void
MapWindow::clearWindow()
{
#ifdef CYPRESS_XGL
	tkCONTEXT savecx = tkWindow::pushContext(getOverlayGID());
	c3i(blackcol); clear();
	tkWindow::popContext(savecx);
//	clearOverlay();
#else
	drawmode(OVERDRAW);
	c3i(blackcol); clear();
	drawmode(NORMALDRAW);
#endif

	setcolor(BLACK);
	tkWindow::clearWindow();
}

void
MapWindow::stowWindow()
{
	clearWindow();
	tkWindow::stowWindow();
}

void
MapWindow::redrawWindow()
{
	if (gid == tkInvalidGID)
		return;

	tkCONTEXT savecx = tkWindow::pushContext(gid);
	reshapeviewport();
	updateScreenArea();
	frontbuffer(FALSE); backbuffer(TRUE);
	clearWindow();
	tkParentView::paint();
	swapbuffers();
	frontbuffer(TRUE); backbuffer(FALSE);
	tkWindow::popContext(savecx);
}

void
MapWindow::drawHand()
{
	if (gid == tkInvalidGID)
		return;

	tkCONTEXT savecx = tkWindow::pushContext(gid);
	reshapeviewport();
	updateScreenArea();
	MAPView->drawHand(TRUE);
	tkWindow::popContext(savecx);
}

