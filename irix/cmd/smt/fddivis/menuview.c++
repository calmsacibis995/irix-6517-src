/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.5 $
 */

#include "fddivis.h"

#define MMENU_COLS	3.0	// 3 cols
#define MMENU_ROWS	2.0	// 2 rows

#define PBTN_WD		MENU_BTN_WD
#define PBTN_HT		(1.6 * MENU_BTN_SPACE + MENU_BTN_HT)

#define FOOTSIZE	(PBTN_HT*MMENU_ROWS)
#define LOGOHEADSIZE	0.0

MenuView::MenuView()
{
	tkPen pen(BLACK,BG_COLOR,-1,-1);

	for (register int i = 0; i < MENUSIZE; i++) {
		menu[i].view = NULL;
	}

	currentMenu = 0;

	quitButton = makeMenuButton("Quit",PBTN_WD,PBTN_HT);
	quitButton->setClient(this);
	quitButton->enable();
	addAView(*quitButton);

	helpButton = makeMenuButton("Help",PBTN_WD,PBTN_HT);
	helpButton->setClient(this);
	helpButton->enable();
	addAView(*helpButton);

	dispButton = makePickButton("Display",FV_DISPLAY,PBTN_WD,PBTN_HT);
	dispButton->setClient(this);
	dispButton->enable();
	addAView(*dispButton);

	captButton = makePickButton("Capture",FV_CAPTURE,PBTN_WD,PBTN_HT);
	captButton->setClient(this);
	captButton->enable();
	addAView(*captButton);

	demoButton = makePickButton("Tutorial",FV_TUTORIAL,PBTN_WD,PBTN_HT);
	demoButton->setClient(this);
	demoButton->enable();
	addAView(*demoButton);

	statButton = makePickButton("Status",FV_STAT,PBTN_WD,PBTN_HT);
	statButton->setClient(this);
	statButton->enable();
	addAView(*statButton);

	switch (currentMenu) {
	  case FV_TUTORIAL:
		demoButton->setCurrentState(1);
		demoButton->setCurrentVisualState(tkBUTTON_QUIET);
		demoButton->paintInContext();
		break;

	  case FV_CAPTURE:
		captButton->setCurrentState(1);
		captButton->setCurrentVisualState(tkBUTTON_QUIET);
		captButton->paintInContext();
		break;

	  case FV_STAT:
		statButton->setCurrentState(1);
		statButton->setCurrentVisualState(tkBUTTON_QUIET);
		statButton->paintInContext();
		break;

	  default:
		dispButton->setCurrentState(1);
		dispButton->setCurrentVisualState(tkBUTTON_QUIET);
		dispButton->paintInContext();
		break;
	}

	popup = defpup("Control %t|Display|Capture|Tutorial|Status|Help"
		       "  %l|Quit fddivis");

	helpClose = new tkEvent(HELP_CLOSE,this,0);
	tkGetEventManager()->expressInterest(helpClose);

	helpApp = NULL;
}

MenuView::~MenuView()
{
	delete statButton;
	delete helpButton;
	delete quitButton;
	delete demoButton;
	delete captButton;
	delete dispButton;
	if (helpApp != NULL) {
		helpApp->appClose();
		delete (VhelpApp*)helpApp;
	}
	tkGetEventManager()->loseInterest(helpClose);
}

void
MenuView::resize()
{
	Box2 bb = getBounds();
	register float sw = bb.xlen / MMENU_COLS;
	register float sh = FOOTSIZE / MMENU_ROWS;

	statButton->setBounds(Box2(bb.xorg,	   bb.yorg,    sw,sh));
	helpButton->setBounds(Box2(bb.xorg+sw,     bb.yorg,    sw,sh));
	quitButton->setBounds(Box2(bb.xorg+sw*2.0+0.5, bb.yorg,    sw,sh));

	dispButton->setBounds(Box2(bb.xorg,	   bb.yorg+sh, sw,sh));
	captButton->setBounds(Box2(bb.xorg+sw,     bb.yorg+sh, sw,sh));
	demoButton->setBounds(Box2(bb.xorg+sw*2.0+0.5, bb.yorg+sh, sw,sh));


	Box2 vb(bb.xorg,bb.yorg+FOOTSIZE,bb.xlen,bb.ylen-FOOTSIZE-LOGOHEADSIZE);
	for (register int i = 0; i < MENUSIZE; i++) {
		if (menu[i].view) {
			menu[i].view->setBounds(vb);
			menu[i].view->resize();
		}
	}
}

void
MenuView::paint()
{
	pushmatrix();
	localTransform();

	Box2 bb = getBounds();
	color(BG_COLOR);
	bb.fill();
	tkPen pen(BLACK,BG_COLOR,-1,-1);

	helpButton->paint();
	quitButton->paint();

	demoButton->setCurrentState(currentMenu==FV_TUTORIAL?1:0);
	demoButton->setCurrentVisualState(tkBUTTON_QUIET);
	demoButton->paintInContext();
	demoButton->paint();

	captButton->setCurrentState(currentMenu==FV_CAPTURE?1:0);
	captButton->setCurrentVisualState(tkBUTTON_QUIET);
	captButton->paintInContext();
	captButton->paint();

	dispButton->setCurrentState(currentMenu==FV_DISPLAY?1:0);
	dispButton->setCurrentVisualState(tkBUTTON_QUIET);
	dispButton->paintInContext();
	dispButton->paint();

	statButton->setCurrentState(currentMenu==FV_STAT?1:0);
	statButton->setCurrentVisualState(tkBUTTON_QUIET);
	statButton->paintInContext();
	statButton->paint();

	menu[currentMenu].view->paint();

	popmatrix();
}

void
MenuView::rcvEvent(tkEvent *e)
{
	switch(e->name()) {
	case tkEVENT_VALUECHANGE:
		if (e->sender() == quitButton) {
			FV.quit();
			return;
		}
		if (e->sender() == helpButton) {
			tkEvent helpEvent(HELP_OPEN,this,0);
			menu[currentMenu].view->rcvEvent(&helpEvent);
			return;
		}

		// Menus
		if (e->sender() == captButton) {
			if (currentMenu != FV_CAPTURE) {
				getParentWindow()->getKeyboardFocus(this);
				select(FV_CAPTURE);
				return;
			}
			captButton->setCurrentState(1);
			captButton->setCurrentVisualState(tkBUTTON_QUIET);
			captButton->paintInContext();
			return;
		}

		if (e->sender() == demoButton) {
			if (currentMenu != FV_TUTORIAL) {
				getParentWindow()->getKeyboardFocus(this);
				select(FV_TUTORIAL);
				return;
			}
			demoButton->setCurrentState(1);
			demoButton->setCurrentVisualState(tkBUTTON_QUIET);
			demoButton->paintInContext();
			return;
		}

		if (e->sender() == dispButton) {
			if (currentMenu != FV_DISPLAY) {
				getParentWindow()->getKeyboardFocus(this);
				select(FV_DISPLAY);
				return;
			}
			dispButton->setCurrentState(1);
			dispButton->setCurrentVisualState(tkBUTTON_QUIET);
			dispButton->paintInContext();
			return;
		}

		if (e->sender() == statButton) {
			if (currentMenu != FV_STAT) {
				getParentWindow()->getKeyboardFocus(this);
				select(FV_STAT);
				return;
			}
			statButton->setCurrentState(1);
			statButton->setCurrentVisualState(tkBUTTON_QUIET);
			statButton->paintInContext();
			return;
		}
		break;

	case HELP_OPEN:
		if (helpApp != NULL) {
			helpApp->PopWindow();
			return;
		}

		helpApp = new VhelpApp(HELPFILE);
		helpApp->SetSection("fddivis");
		helpApp->SetTitle("Help on fddivis");
		helpApp->appInit(0, 0, (char **)0, (char **)0);
		break;

	case HELP_CLOSE:
		if (e->sender() != helpApp) {
			menu[currentMenu].view->rcvEvent(e);
			return;
		}
		helpApp->appClose();
		delete (VhelpApp*)helpApp;
		helpApp = NULL;
		break;

	default:
		menu[currentMenu].view->rcvEvent(e);
		break;
	}
}

void
MenuView::addView(register int id, ResizeParentView *view)
{
	if (id >= MENUSIZE)
		return;

	Box2 bb = getBounds();
	Box2 vb(bb.xorg,bb.yorg+FOOTSIZE,bb.xlen,bb.ylen-FOOTSIZE-LOGOHEADSIZE);
	view->setBounds(vb);
	view->resize();

	menu[id].view = view;
	addAView(*view);

	view->open();
	return;
}

void
MenuView::select(register int id)
{
	if ((id < 0) || (id >= MENUSIZE) || (menu[id].view == NULL))
		return;

	currentMenu = id;
	FV.curPanel = id;
	getParentWindow()->redrawWindow();
}

tkView*
MenuView::insideKids(
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
const Point& p
#else
Point& p
#endif
)
{
	register tkView *v = menu[currentMenu].view->inside(p);

	if (v != 0)
		return v;

	if (((v = statButton->inside(p)) != 0) ||
	    ((v = quitButton->inside(p)) != 0) ||
	    ((v = demoButton->inside(p)) != 0) ||
	    ((v = captButton->inside(p)) != 0) ||
	    ((v = dispButton->inside(p)) != 0))
		return v;

	return(helpButton->inside(p));
}

#define POPUP_CONN	1
#define POPUP_HIST	2
#define POPUP_DEMO	3
#define POPUP_STAT	4
#define POPUP_HELP	5
#define POPUP_QUIT	6

void
MenuView::menuSelect(Point &p)
{
	p = p;

	switch (dopup(popup)) {
		case POPUP_QUIT:
			FV.quit();
			break;

		case POPUP_HELP: {
			    tkEvent helpEvent(HELP_OPEN,this,0);
			    menu[currentMenu].view->rcvEvent(&helpEvent);
			}
			break;

		case POPUP_DEMO:
			select(FV_TUTORIAL);
			break;

		case POPUP_HIST:
			select(FV_CAPTURE);
			break;

		case POPUP_CONN:
			select(FV_DISPLAY);
			break;

		case POPUP_STAT:
			select(FV_STAT);
			break;
	}
}
