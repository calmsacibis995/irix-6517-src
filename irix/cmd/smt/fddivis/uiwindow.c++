/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.6 $
 */

#include "fddivis.h"

void
UserInterfaceWindow::open()
{
	image_fp = 0;
	image_location = 0;

#define UIX1 (float)(AEDGE)
#define UIX2 (float)(SCRXMAX-SCRYMAX-AEDGE+1)
#define UIY1 (float)(AEDGE)
#define UIY2 (float)(SCRYMAX-((BNR+AEDGE)*2)-(UIX2-UIX1))
#define UIY2DASH (float)(SCRYMAX-((BNR+AEDGE)*2))
	fudge(0.0, 0.0);
	prefposition(UIX1, UIX2, UIY1, (SCRYMAX>=1024)?UIY2:UIY2DASH);
	setName("fddivis Control");
	setTitle("fddivis Control");
	setIconTitle("FV Ctrl");
	setStartUpNoClose(TRUE);
	setStartUpNoQuit(TRUE);
	setNoResize(TRUE);
	setHandleClose(TRUE);

	tkWindow::open();
	setopaque();

	float x,y;
	getsize(&x,&y);

	ortho2(0.0,x,0.0,y);
	clearWindow();
	swapbuffers();

	Menu = new MenuView;
	Menu->setBounds(Box2(0.0,0.0,x,y));
	addView(*Menu);
	Menu->resize();

	Menu->addView(FV_DISPLAY,  new DispMenuView);
	Menu->addView(FV_CAPTURE,  new CaptureMenuView);
	Menu->addView(FV_TUTORIAL, new DemoMenuView);
	Menu->addView(FV_STAT,	   new StatMenuView);

	Menu->select(FV.curPanel);
}

void
UserInterfaceWindow::resize()
{
	reshapeviewport();
	updateScreenArea();
}

void
UserInterfaceWindow::clearWindow()
{
	setcolor(BG_COLOR);
	tkWindow::clearWindow();
}

void
UserInterfaceWindow::redrawWindow()
{
	tkWindow::redrawWindow();
}
