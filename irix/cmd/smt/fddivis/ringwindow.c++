/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Graphics functions for VisualNet
 *
 *	$Revision: 1.8 $
 */

#include "fddivis.h"
#ifdef CYPRESS_XGL
#include "cursorfont.h"
#include "Xutil.h"
#include "Xatom.h"
#include "MwmUtil.h"

extern Display* Xdpy;
extern Colormap ov_cmap;
#endif

void
RingWindow::open()
{
#define RINGX1	(float)(SCRXMAX-SCRYMAX+AEDGE)
#define RINGX2	(float)(SCRXMAX-AEDGE)
#define RINGY1	(float)(AEDGE)
#define RINGY2	(float)(SCRYMAX-AEDGE-BNR)
	fudge(0.0, 0.0);
	prefposition(RINGX1, RINGX2, RINGY1, RINGY2);
	setName("fddivis Ring");
	setTitle("fddivis Ring");
	setIconTitle("FV Ring");
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

	XColor   zcolor;

	// green
	zcolor.pixel = GREENOVLY;
	zcolor.flags = DoRed|DoGreen|DoBlue;
	zcolor.red = 0;
	zcolor.green = 65535;
	zcolor.blue = 0;
	XStoreColor(Xdpy, ov_cmap, &zcolor);

	// grey
	zcolor.flags = DoRed|DoGreen|DoBlue;
	zcolor.pixel = GREYOVLY;
	zcolor.red = 40000;
	zcolor.green = 40000;
	zcolor.blue = 40000;
	XStoreColor(Xdpy, ov_cmap, &zcolor);

	// red
	zcolor.flags = DoRed|DoGreen|DoBlue;
	zcolor.pixel = REDOVLY;
	zcolor.red = 65535;
	zcolor.green = 0;
	zcolor.blue = 0;
	XStoreColor(Xdpy, ov_cmap, &zcolor);

	Window      main_ov[4];
	main_ov[0] = getOverlayGID();
	main_ov[1] = getgid();
	main_ov[2] = 0;
	XSetWMColormapWindows(Xdpy, getgid(), main_ov, 2);
	XMapRaised(Xdpy, getOverlayGID());

	XFlush(Xdpy);
#else
	tkWindow::open();
	setopaque();
	picksize(1,1);
	doublebuffer();
	shademodel(GOURAUD);
	overlay(2);
	gconfig();
	drawmode(OVERDRAW);
	mapcolor(GREENOVLY, 0,255,0);	/* green */
	mapcolor(GREYOVLY,  128,128,128);	/* grey */
	mapcolor(REDOVLY,   255,0,0);	/* red */
	drawmode(NORMALDRAW);
#endif

	clearWindow();
	swapbuffers();

	float x,y;
	getsize(&x,&y);

	RINGView = new RingView;
	RINGView->setBounds(Box2(0.0,0.0,x,y));
	addView(*RINGView);
}

void
RingWindow::close()
{
	clearWindow();
	tkWindow::close();
}

void
RingWindow::resize()
{
	updateScreenArea();

	float x,y;
	getsize(&x,&y);
	Box2 bb(0,0,int(x),int(y));

	RINGView->setBounds(bb);
	RINGView->resize();
}

void
RingWindow::clearWindow()
{
	if (zbuf)
		zclear();

#ifdef CYPRESS_XGL
	tkCONTEXT savecx = tkWindow::pushContext(getOverlayGID());
	c3i(blackcol); clear();
	tkWindow::popContext(savecx);
	//clearOverlay();
#else
	drawmode(OVERDRAW);
	c3i(blackcol); clear();
	drawmode(NORMALDRAW);
#endif

	setcolor(BLACK);
	tkWindow::clearWindow();
}

void
RingWindow::redrawWindow()
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
RingWindow::stowWindow()
{
	clearWindow();
	tkWindow::stowWindow();
}

void
RingWindow::rcvEvent(register tkEvent* e)
{
	switch (e->name()) {
	case tkEVENT_STOW:
		stowWindow();
		break;
	default:
		tkWindow::rcvEvent(e);
	}
}

int
RingWindow::flashWindow()
{
	Colorindex c;
	Colorindex savc;

	if ((gid == tkInvalidGID) || ((FV.RingOpr&(BEACONING|CLAIMING)) == 0))
		return(0);

	tkCONTEXT savecx = tkWindow::pushContext(gid);
	reshapeviewport();
	updateScreenArea();
	frontbuffer(FALSE); backbuffer(TRUE);
	savc = getcolor();

	if (((FV.RingOpr & BEACONING) == BEACONING) &&
	    ((FV.FrameSelect & SEL_BCN) == SEL_BCN)) {
		::ringbell();
		c = FVC_RED;
	} else if (((FV.RingOpr & CLAIMING) == CLAIMING) &&
	    ((FV.FrameSelect & SEL_CLM) == SEL_CLM)) {
		c = FVC_WHITE;
	}

	setcolor(c);
	tkWindow::clearWindow();
	swapbuffers();
	setcolor(savc);
	frontbuffer(TRUE); backbuffer(FALSE);
	tkWindow::popContext(savecx);
	return(1);
}

void
RingWindow::drawToken()
{
	if (gid == tkInvalidGID)
		return;

	tkCONTEXT savecx = tkWindow::pushContext(gid);
	reshapeviewport();
	updateScreenArea();
#ifdef CYPRESS_XGL
	tkCONTEXT overcx = getOverlayGID();
	if (overcx != tkInvalidGID) {
		pushmatrix();
		pushviewport();
		tkCONTEXT savecx2 = tkWindow::pushContext(overcx);
		RINGView->drawtoken();
		XMapRaised(Xdpy, overcx);
		tkWindow::popContext(savecx2);
		popviewport();
		popmatrix();
	}
#else
	drawmode(OVERDRAW);
//	mapcolor(GREENOVLY, 0,255,0);	/* green */
//	mapcolor(GREYOVLY, 61,61,61);	/* grey */
	RINGView->drawtoken();
	drawmode(NORMALDRAW);
#endif
	tkWindow::popContext(savecx);
}
