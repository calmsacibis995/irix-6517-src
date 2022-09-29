#ifndef __MENUVIEW__
#define __MENUVIEW__
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.3 $
 */

/*
 * Main Menu
 */
#define FV_DISPLAY	0
#define FV_CAPTURE	1
#define FV_TUTORIAL	2
#define FV_STAT		3
#define MENUSIZE	4

#define HELP_OPEN	813

#include <stdParts.h>
#include "resizeview.h"

class MenuView : public tkParentView
{
protected:
	int lastid;
	int currentMenu;
	long popup;
	struct {
		ResizeParentView *view;
	} menu[MENUSIZE];
	tkButton *quitButton;
	tkButton *statButton;
	tkButton *helpButton;

	tkButton *demoButton;
	tkButton *captButton;
	tkButton *dispButton;

	tkEvent *helpClose;
	VhelpApp *helpApp;

public:
	MenuView();
	~MenuView();

	virtual void resize();
	virtual void paint();
	virtual void rcvEvent(tkEvent *e);
	virtual void menuSelect(Point &p);
	virtual tkView* insideKids(const Point& p);
	void addView(int id, ResizeParentView *view);
	void select(int id);
};
#endif
