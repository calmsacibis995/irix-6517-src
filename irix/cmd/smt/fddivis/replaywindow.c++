/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.5 $
 */

#include <tkTypes.h>
#include <stdParts.h>
#include <tkParentView.h>
#include <tkScrollBar.h>
#include <tkExec.h>
#include <BorderPntView.h>
#include <tkNotifier.h>
#include <tkPrompt.h>
#include <tkEvent.h>
#include <events.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <values.h>
#include "fddivis.h"

const int summaryAllocSize = 20; /* rows */
const int summaryTableWidth = 3; /* cols */
static float summaryWidths[] = { 0.5, 0.8, 15.0};
static char* summaryTitles[] = { "Num", "Status", "Time"};

/*
 * Containing window for analyzer application
 */
ReplayWindow::ReplayWindow()
{
	summaryTable = new titledTable(summaryAllocSize, summaryTableWidth);
	summaryTable->setColumns(summaryWidths, summaryTitles);
	summaryTable->setFont("ScreenBold11");
	summaryTable->setRowSelection(TRUE);
	summaryTable->setOneSelection(TRUE);
	summaryTable->leftJustify(0);
	summaryTable->leftJustify(1);
	summaryTable->leftJustify(2);
	addView(*summaryTable);

	Box2 box(0.0,0.0,100.0,100.0);
	tkPen *tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
				 TVIEW_EDGE_ICOLOR,TVIEW_BORDER_LW);
	strcpy(FV.CurReplayDir,
	       FV.ReplayDir[0] != '\0' ? FV.ReplayDir : FV.RecordDir);
	ReplayDirText = makeFancyTextView(FV.CurReplayDir,
					  sizeof(FV.ReplayDir), box,tvPen);
	ReplayDirText->setClient(this);
	addAView(*ReplayDirText);

	// Undelete Button
	undeleteButton = makeMenuButton("Undel", MENU_BTN_WD, MENU_BTN_HT);
	undeleteButton->setClient(this);
	addAView(*undeleteButton);

	// Undelete All Button
	undeleteAllButton = makeMenuButton("Undel All",
					   MENU_BTN_WD, MENU_BTN_HT);
	undeleteAllButton->setClient(this);
	addAView(*undeleteAllButton);

	// Delete All Button
	deleteAllButton = makeMenuButton("Del All",
					 MENU_BTN_WD, MENU_BTN_HT);
	deleteAllButton->setClient(this);
	addAView(*deleteAllButton);

	// Delete Button
	deleteButton = makeMenuButton("Del", MENU_BTN_WD, MENU_BTN_HT);
	deleteButton->setClient(this);
	addAView(*deleteButton);

	// Quit Button
	quitButton = makeMenuButton("Quit", MENU_BTN_WD, MENU_BTN_HT);
	quitButton->setClient(this);
	addAView(*quitButton);

	setTitle("fddivis Replay");
#define RPX1 (float)(AEDGE)
#define RPX2 (float)((SCRXMAX-SCRYMAX-AEDGE)*1.8)
#define RPY2 (float)(SCRYMAX-((BNR+AEDGE)*2)-(RPX2-RPX1))
#define RPY1 (float)(RPY2*1.0/3.0)
	prefposition(RPX1, RPX2, RPY1, RPY2);
	setStartUpNoClose(TRUE);
	setStartUpNoQuit(TRUE);
	setHandleClose(TRUE);

	if (FV.CurReplayDir[0] != '\0')
		setTbl(0);
}


ReplayWindow::~ReplayWindow()
{
	delete (titledTable*) summaryTable;
	delete (tkButton*) undeleteButton;
	delete (tkButton*) undeleteAllButton;
	delete (tkButton*) deleteButton;
	delete (tkButton*) deleteAllButton;
	delete (tkButton*) quitButton;
	delete (tkTextView*) ReplayDirText;
}


void
ReplayWindow::open()
{
	summaryTable->setClient(this);
	tkWindow::open();
	setIconTitle("FV Replay");
	::winconstraints();

	ReplayEvent = new tkTextViewEvent(tkEVENT_ACTION,this,
						getParentWindow()->getgid());
	tkGetEventManager()->expressInterest(ReplayEvent);

	selectedEvent = new tkEvent(tkEVENT_SELECTED, this);
	tkGetEventManager()->expressInterest(selectedEvent);

	curSeq = -1;
}

void
ReplayWindow::close()
{
	tkGetEventManager()->loseInterest(ReplayEvent);
	tkGetEventManager()->loseInterest(selectedEvent);
	tkWindow::close();
}

void
ReplayWindow::redrawWindow()
{
	const float menuSpace = 2.5 * MENU_BTN_HT;

#ifdef CYPRESS_XGL
	if (gid == tkInvalidGID)
		return;
	tkCONTEXT savecx = tkWindow::pushContext(gid);
#else
	if (gid < 0)
		return;
	long savedgid = winget();
	winset(gid);
#endif

	reshapeviewport();
	updateScreenArea();
	ortho2(-0.5, screenArea.xlen+0.5, -0.5, screenArea.ylen+0.5);

	Box2 bb = getBounds();
	withStdColor(BG_ICOLOR) {
		sboxf(0.0, 0.0, bb.xlen, bb.ylen);
	}

	float ylen = bb.ylen - menuSpace;
	Box2 summaryBounds(0.0, bb.ylen - ylen, bb.xlen, ylen);
	summaryTable->setBounds(summaryBounds);
	summaryTable->resize();

	float y = (menuSpace-2.0 * MENU_BTN_HT) / 3.0;
	Box2 txBounds(bb.xorg, 2.0*y+MENU_BTN_HT, bb.xlen, MENU_BTN_HT);
	ReplayDirText->setBounds(txBounds);

	// Menu buttons
	Box2 menuBounds(bb.xlen-MENU_BTN_WD-3.0, y, MENU_BTN_WD, MENU_BTN_HT);
	quitButton->setBounds(menuBounds);
	menuBounds.xorg -= (MENU_BTN_WD+3.0);
	undeleteButton->setBounds(menuBounds);
	menuBounds.xorg -= (MENU_BTN_WD+3.0);
	deleteButton->setBounds(menuBounds);
	menuBounds.xorg -= (MENU_BTN_WD+3.0);
	deleteAllButton->setBounds(menuBounds);
	menuBounds.xorg -= (MENU_BTN_WD+3.0);
	undeleteAllButton->setBounds(menuBounds);

	tkParentView::paint();

	color(15);
	summaryBounds.outline();

#ifdef CYPRESS_XGL
	tkWindow::popContext(savecx);
#else
	if (savedgid >= 0)
		winset(savedgid);
#endif
}


void
ReplayWindow::doDel(int seq, int op)
{
	RECPLAY *rp;

	rp = FV.RINGWindow->RINGView->ReplayDelete(seq, op);
	if (rp)
		setATbl(rp, seq);

}

void
ReplayWindow::rcvEvent(tkEvent *e)
{
	int i;

	switch(e->name()) {
	case tkEVENT_VALUECHANGE:
		if (e->sender() == quitButton) {
			tkEvent quit(FV_REPLAY_OFF);
			FV.rcvEvent(&quit);
		} else if (e->sender() == deleteButton) {
			doDel(curSeq, TRUE);
		} else if (e->sender() == deleteAllButton) {
			i = FV.RINGWindow->RINGView->NumRecords;
			while (i > 0)
				doDel(--i, TRUE);
		} else if (e->sender() == undeleteButton) {
			doDel(curSeq, FALSE);
		} else if (e->sender() == undeleteAllButton) {
			i = FV.RINGWindow->RINGView->NumRecords;
			while (i > 0)
				doDel(--i, FALSE);
		}
		redrawWindow();
		break;

	case tkEVENT_SELECTED:
		if (e->sender() == summaryTable) {
			int value;
			short* svalue = (short*) &value;
			((tkValueEvent*) e)->getValue(&value);
			curSeq = svalue[0];
			FV.RINGWindow->RINGView->ReplaySeq(curSeq);
			FV.RINGWindow->RINGView->RecordOff();
			FV.UIWindow->redrawWindow();
		}
		break;

	case tkEVENT_ACTION:
		if (e->sender() == ReplayDirText) {
			if (((tkTextViewEvent*)e)->getterminator()
			    && strcmp(FV.ReplayDir, FV.CurReplayDir)) {
				strcpy(FV.ReplayDir,
				       ReplayDirText->gettext());
				strcpy(FV.CurReplayDir,
				       ReplayDirText->gettext());
				ReplayDirText->loseKeyboardFocus();
				FV.RINGWindow->RINGView->ReplayOn(1);
				break;
			}
			ReplayDirText->setText(FV.CurReplayDir);
		}
		break;
	default:
		tkWindow::rcvEvent(e);
	}
}

void
ReplayWindow::setATbl(RECPLAY *recp, int seq)
{
	char *cp;

	if (recp == 0)
		return;
	summaryTable->setEntry(seq, 0, recp->RecNum);
	if (recp->RecStat & DELETED)
		cp = "delete";
	else if (recp->RecStat & STALE)
		cp = "stale";
	else if (recp->RecStat & BEACONING)
		cp = "BEACON";
	else if (recp->RecStat & CLAIMING)
		cp = "CLAIM";
	else if (recp->RecStat & WRAPPED)
		cp = "WRAP";
	else
		cp = "THRU";
	summaryTable->setEntry(seq, 1, cp);
	summaryTable->setEntry(seq, 2, recp->name);
}

void
ReplayWindow::setTbl(int complain)
{
	register int i, n;

	FV.RINGWindow->RINGView->ReplayOn(complain);

	n = FV.RINGWindow->RINGView->NumRecords;
	if (n <= 0)
		return;

	RECPLAY	 *recp = FV.RINGWindow->RINGView->RecordTbl;
	for (i = 0; i < n; i++, recp = recp->next) {
		setATbl(recp, i);
	}
}
