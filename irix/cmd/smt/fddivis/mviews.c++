/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.8 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <libc.h>
#include <string.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"

// Vertical spacing in control panel
#define VSPACE		1.25
#define GROUP_GAP	1.1

static char NumbersOnly[256] = {
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,127,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };

/*
 * Connection Menu
 */
DispMenuView::DispMenuView()
{
	tkPen pen(BLACK,BG_COLOR,-1,-1);
	Box2 box(0.0,0.0,100.0,100.0);

	// Station Labeling
	NameNumberText = new tkLabel(box,pen,"Label stations by:");
	NameNumberText->setjust();
	NameNumberButton = new tkRadioButton;
	NameNumberButton->setClient(this);
	addAView(*NameNumberButton);

	NameButton = makePickButton("Name",ON,PICK_BTN_WD,PICK_BTN_HT);
	NameNumberButton->addButton(*NameButton);
	NumberButton = makePickButton("Number",OFF,PICK_BTN_WD,PICK_BTN_HT);
	NameNumberButton->addButton(*NumberButton);
	if (FV.SymAdrMode)
		NameNumberButton->setCurrentButton(NameButton);
	else
		NameNumberButton->setCurrentButton(NumberButton);

	// Address BIT order
	FCBitOrderText = new tkLabel(box,pen,"Address bit order:");
	FCBitOrderText->setjust();
	FCBitOrderButton = new tkRadioButton;
	FCBitOrderButton->setClient(this);
	addAView(*FCBitOrderButton);
	FBitOrderButton = makePickButton("FDDI",ON,PICK_BTN_WD,PICK_BTN_HT);
	FCBitOrderButton->addButton(*FBitOrderButton);
	CBitOrderButton=makePickButton("Canonic",OFF,PICK_BTN_WD,PICK_BTN_HT);
	FCBitOrderButton->addButton(*CBitOrderButton);
	if (FV.FddiBitMode)
		FCBitOrderButton->setCurrentButton(FBitOrderButton);
	else
		FCBitOrderButton->setCurrentButton(CBitOrderButton);

	// Display Mode
	PartialFullText = new tkLabel(box,pen,"Ring display mode:");
	PartialFullText->setjust();
	PartialFullButton = new tkRadioButton;
	PartialFullButton->setClient(this);
	addAView(*PartialFullButton);
	PartialButton = makePickButton("Partial",OFF,PICK_BTN_WD,PICK_BTN_HT);
	PartialFullButton->addButton(*PartialButton);
	FullButton = makePickButton("Full",ON,PICK_BTN_WD,PICK_BTN_HT);
	PartialFullButton->addButton(*FullButton);
	if (FV.FullScreenMode)
		PartialFullButton->setCurrentButton(FullButton);
	else
		PartialFullButton->setCurrentButton(PartialButton);

	// Magnified Addr
	MagAddrText = new tkLabel(box,pen,"Show station info:");
	MagAddrText->setjust();

	MagAddrButton = new tkRadioButton;
	MagAddrButton->setClient(this);
	addAView(*MagAddrButton);
	MagAddrOffButton = makePickButton("Off",OFF,PICK_BTN_WD,PICK_BTN_HT);
	MagAddrButton->addButton(*MagAddrOffButton);
	MagAddrOnButton = makePickButton("On",ON,PICK_BTN_WD,PICK_BTN_HT);
	MagAddrButton->addButton(*MagAddrOnButton);
	if (FV.MagAddr)
		MagAddrButton->setCurrentButton(MagAddrOnButton);
	else
		MagAddrButton->setCurrentButton(MagAddrOffButton);

	// AlarmSelect
	AlarmText = new tkLabel(box,pen,"Alarm triggers:");
	AlarmText->setjust();

	ClmButton = makeToggleButton("CLAIM",ON,PICK_BTN_WD,PICK_BTN_HT);
	ClmButton->setClient(this);
	addAView(*ClmButton);
	if (FV.FrameSelect&SEL_CLM) {
		ClmButton->setCurrentState(1);
		ClmButton->setCurrentVisualState(tkBUTTON_QUIET);
		ClmButton->paintInContext();
	}

	BcnButton = makeToggleButton("BEACON",ON,PICK_BTN_WD,PICK_BTN_HT);
	BcnButton->setClient(this);
	addAView(*BcnButton);
	if (FV.FrameSelect&SEL_BCN) {
		BcnButton->setCurrentState(1);
		BcnButton->setCurrentVisualState(tkBUTTON_QUIET);
		BcnButton->paintInContext();
	}

	// Map buttons
	MapText = new tkLabel(box,pen,"Map operations:");
	MapText->setjust();

	// Restart
	RestartButton = makeMenuButton("Restart",PICK_BTN_WD,PICK_BTN_HT);
	RestartButton->setClient(this);
	addAView(*RestartButton);

	// Freeze
	FreezeButton = makeToggleButton("Freeze",ON,PICK_BTN_WD,PICK_BTN_HT);
	FreezeButton->setClient(this);
	addAView(*FreezeButton);
	if (FV.Freeze) {
		FreezeButton->setCurrentState(1);
		FreezeButton->setCurrentVisualState(tkBUTTON_QUIET);
		FreezeButton->paintInContext();
	}

	// Find station's icon
	FindButton = makeMenuButton("Find",PICK_BTN_WD,PICK_BTN_HT);
	FindButton->setClient(this);
	addAView(*FindButton);


	// Token
	TokenButton = makeToggleButton("Token",ON,PICK_BTN_WD,PICK_BTN_HT);
	TokenButton->setClient(this);
	addAView(*TokenButton);
	if (FV.ShowToken) {
		TokenButton->setCurrentState(1);
		TokenButton->setCurrentVisualState(tkBUTTON_QUIET);
		TokenButton->paintInContext();
	}

	// Display version
	VersText = new tkLabel(box,pen,"version " FDDIVIS_VERSTR);

	helpApp = NULL;
}

DispMenuView::~DispMenuView()
{
	tkGetEventManager()->loseInterest(actionEvent);
	delete actionEvent;
	delete AlarmText;
	delete ClmButton;
	delete BcnButton;
	delete FindButton;
	if (helpApp != NULL) {
		helpApp->appClose();
		delete (VhelpApp*)helpApp;
	}
}

void
DispMenuView::open()
{
	actionEvent = new tkTextViewEvent(tkEVENT_ACTION,this,
						getParentWindow()->getgid());
	tkGetEventManager()->expressInterest(actionEvent);
}

void
DispMenuView::resize()
{
	register float x;
	Box2 bb = getBounds();
	register float y = bb.ylen - TOPMARGIN;

	// Station Labeling
	NameNumberText->changeArea(Box2(LEFTMARGIN,y,
					bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
	Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
	NameButton->setBounds(bloc);
	bloc.xorg = 2.0 * x + PICK_BTN_WD;
	NumberButton->setBounds(bloc);
	y -= GROUP_GAP * PICK_BTN_HT;

	// Address BIT order
	FCBitOrderText->changeArea(Box2(LEFTMARGIN,y,
					bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		CBitOrderButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		FBitOrderButton->setBounds(bloc);
	}
	y -= GROUP_GAP * PICK_BTN_HT;

	// Display Mode
	PartialFullText->changeArea(Box2(LEFTMARGIN,y,
					bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		PartialButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		FullButton->setBounds(bloc);
	}
	y -= GROUP_GAP * PICK_BTN_HT;

	// Magnified Addr
	MagAddrText->changeArea(Box2(LEFTMARGIN,y,
					bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		MagAddrOffButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		MagAddrOnButton->setBounds(bloc);
	}
	y -= GROUP_GAP * PICK_BTN_HT;


	AlarmText->changeArea(Box2(LEFTMARGIN,y,
				   bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		ClmButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		BcnButton->setBounds(bloc);
	}
	y -= GROUP_GAP * PICK_BTN_HT;


	// Map buttons
	MapText->changeArea(Box2(LEFTMARGIN,y,
					bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;

	// Misc. operations
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		RestartButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		FreezeButton->setBounds(bloc);
		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		FindButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		TokenButton->setBounds(bloc);
	}
	y -= GROUP_GAP * PICK_BTN_HT;
}

void
DispMenuView::rcvEvent(tkEvent *e)
{
	int v;
	register tkObject *sender = e->sender();

	switch (e->name()) {
	case tkEVENT_VALUECHANGE:
		if (sender == NameNumberButton) {
			((tkValueEvent*)e)->getValue(&v);
			v--;
			if (v != FV.SymAdrMode) {
				FV.SymAdrMode = v;
				FV.RINGWindow->redrawWindow();
			}
			break;
		}
		if (sender == FCBitOrderButton) {
			((tkValueEvent*)e)->getValue(&v);
			v--;
			if (v != FV.FddiBitMode) {
				FV.FddiBitMode = v;
				FV.RINGWindow->redrawWindow();
			}
			break;
		}
		if (sender == PartialFullButton) {
			((tkValueEvent*)e)->getValue(&v);
			v--;
			if (v != FV.FullScreenMode) {
				FV.FullScreenMode = v;
				FV.magrp = 0;
				FV.MAPWindow->drawHand();
				FV.RINGWindow->redrawWindow();
			}
			break;
		}
		if (sender == ClmButton) {
			((tkValueEvent*)e)->getValue(&v);
			if (v == ON)
				FV.FrameSelect |= SEL_CLM;
			else
				FV.FrameSelect &= ~SEL_CLM;
			FV.Frame.kickpoke();
			break;
		}
		if (sender == BcnButton) {
			((tkValueEvent*)e)->getValue(&v);
			if (v == ON)
				FV.FrameSelect |= SEL_BCN;
			else
				FV.FrameSelect &= ~SEL_BCN;
			FV.Frame.kickpoke();
			break;
		}
		if (sender == MagAddrButton) {
			((tkValueEvent*)e)->getValue(&v);
			v--;
			if (v != FV.MagAddr) {
				FV.MagAddr = v;
				if (FV.magrp)
					FV.magrp = 0;
				FV.RINGWindow->redrawWindow();
			}
			break;
		}
		if (sender == FreezeButton) {
			((tkValueEvent*)e)->getValue(&v);
			FV.Freeze = (v == ON) ? 1 : 0;
			FV.Frame.kickpoke();
			break;
		}
		if (sender == TokenButton) {
			((tkValueEvent*)e)->getValue(&v);
			v = (v == ON) ? 1 : 0;
			if (v == FV.ShowToken)
				break;
			FV.ShowToken = v;
			break;
		}
		if (sender == RestartButton) {
			// Unfreeze at restart
			FV.Freeze = 0;
			FreezeButton->setCurrentState(0);
			FreezeButton->setCurrentVisualState(tkBUTTON_QUIET);
			FreezeButton->paintInContext();

			FV.ask(RESTARTMSG,FV_RESTART,FV.Agent);
			break;
		}
		if (sender == FindButton) {
			FV.askSearch();
			break;
		}

		break;

	case HELP_OPEN:
		if (helpApp != NULL) {
			helpApp->PopWindow();
			break;
		}
		helpApp = new VhelpApp(HELPFILE);
		helpApp->SetSection("Display");
		helpApp->SetTitle("Help on Display");
		helpApp->appInit(0, 0, (char **)0, (char **)0);
		break;

	case HELP_CLOSE:
		if (sender == helpApp) {
			helpApp->appClose();
			delete (VhelpApp*)helpApp;
			helpApp = NULL;
		}
		break;
	}
}

void
DispMenuView::paint()
{
	pushmatrix();
	localTransform();

	NameNumberText->draw();
	FCBitOrderText->draw();
	PartialFullText->draw();
	MagAddrText->draw();
	AlarmText->draw();
	MapText->draw();
	VersText->draw();

	popmatrix();

	tkParentView::paint();
}


/*
 * History Menu
 */
CaptureMenuView::CaptureMenuView()
{
	tkPen pen(BLACK,BG_COLOR,-1,-1);
	Box2 box(0.0,0.0,100.0,100.0);

	fileText = new tkLabel(box,pen,"Snapshot selection:");
	fileText->setjust();

	saveRGBButton = makeMenuButton("Save RGB",PICK_BTN_WD,PICK_BTN_HT);
	saveRGBButton->setClient(this);
	addAView(*saveRGBButton);

	recordButton = makeToggleButton("Record",ON,PICK_BTN_WD,PICK_BTN_HT);
	recordButton->setClient(this);
	addAView(*recordButton);
	if (FV.Drecord) {
		recordButton->setCurrentState(1);
		recordButton->setCurrentVisualState(tkBUTTON_QUIET);
		recordButton->paintInContext();
	}
	FV.cb = recordButton;

	replayButton = makeMenuButton("Replay",PICK_BTN_WD,PICK_BTN_HT);
	replayButton->setClient(this);
	addAView(*replayButton);

	tkPen *tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
					TVIEW_EDGE_ICOLOR,TVIEW_BORDER_LW);

	// Snapshot interval
	DintText = new tkLabel(box,pen,"Record interval");
	DintText->setjust(LEFTALIGNED,CENTERED);
	DintUnitText = new tkLabel(box,pen,"in seconds: ");
	DintUnitText->setjust(LEFTALIGNED,CENTERED);
	sprintf(DintBuf, "%d", FV.Dint);
	DintEdit = makeFancyTextView(DintBuf,sizeof(DintBuf),box,tvPen);
	DintEdit->setClient(this);
	DintEdit->setTranslationTable(NumbersOnly);
	addAView(*DintEdit);

	// FrameSelect
	FrameText = new tkLabel(box,pen,"Frame capture selection:");
	FrameText->setjust();

	NifButton = makeToggleButton("NIF",ON,PICK_BTN_WD,PICK_BTN_HT);
	NifButton->setClient(this);
	addAView(*NifButton);
	if (FV.FrameSelect&SEL_NIF) {
		NifButton->setCurrentState(1);
		NifButton->setCurrentVisualState(tkBUTTON_QUIET);
		NifButton->paintInContext();
	}

	SrfButton = makeToggleButton("SRF",ON,PICK_BTN_WD,PICK_BTN_HT);
	SrfButton->setClient(this);
	addAView(*SrfButton);
	if (FV.FrameSelect&SEL_SRF) {
		SrfButton->setCurrentState(1);
		SrfButton->setCurrentVisualState(tkBUTTON_QUIET);
		SrfButton->paintInContext();
	}

	SifButton = makeToggleButton("Conf SIF",ON,PICK_BTN_WD,PICK_BTN_HT);
	SifButton->setClient(this);
	addAView(*SifButton);
	if (FV.FrameSelect&SEL_CSIF) {
		SifButton->setCurrentState(1);
		SifButton->setCurrentVisualState(tkBUTTON_QUIET);
		SifButton->paintInContext();
	}

	OpsifButton = makeToggleButton("Op SIF",ON,PICK_BTN_WD,PICK_BTN_HT);
	OpsifButton->setClient(this);
	addAView(*OpsifButton);
	if (FV.FrameSelect&SEL_OPSIF) {
		OpsifButton->setCurrentState(1);
		OpsifButton->setCurrentVisualState(tkBUTTON_QUIET);
		OpsifButton->paintInContext();
	}


	// Info capture mode
	ModeText = new tkLabel(box,pen,"Frame capture mode:");
	ModeText->setjust();

	ModeButton = new tkRadioButton;
	ModeButton->setClient(this);
	addAView(*ModeButton);
	ActModeButton = makePickButton("Active",ON,PICK_BTN_WD,PICK_BTN_HT);
	ModeButton->addButton(*ActModeButton);
	PasModeButton = makePickButton("Passive",OFF,PICK_BTN_WD,PICK_BTN_HT);
	ModeButton->addButton(*PasModeButton);
	if (FV.ActiveMode)
		ModeButton->setCurrentButton(ActModeButton);
	else
		ModeButton->setCurrentButton(PasModeButton);

	// Query interval
	QintText = new tkLabel(box,pen,"Query interval");
	QintText->setjust();
	QintUnitsText = new tkLabel(box,pen,"in seconds: ");
	QintUnitsText->setjust(LEFTALIGNED,CENTERED);
	sprintf(QintBuf, "%d", FV.Qint);
	QintEdit = makeFancyTextView(QintBuf,sizeof(QintBuf),box,tvPen);
	QintEdit->setClient(this);
	QintEdit->setTranslationTable(NumbersOnly);
	if (FV.ActiveMode) {
		addAView(*QintEdit);
	}

	// Aging interval
	AgeText = new tkLabel(box,pen,"Aging interval");
	AgeText->setjust();
	AgeUnitsText = new tkLabel(box,pen,"in seconds: ");
	AgeUnitsText->setjust(LEFTALIGNED,CENTERED);
	sprintf(AgeBuf, "%d", FV.AgeFactor);
	AgeEdit = makeFancyTextView(AgeBuf,sizeof(AgeBuf),box,tvPen);
	AgeEdit->setClient(this);
	AgeEdit->setTranslationTable(NumbersOnly);
	addAView(*AgeEdit);

	helpApp = NULL;
}

CaptureMenuView::~CaptureMenuView()
{
	tkGetEventManager()->loseInterest(actionEvent);
	delete actionEvent;

	delete fileText;
	delete replayButton;
	delete recordButton;
	delete saveRGBButton;

	delete FrameText;
	delete NifButton;
	delete SrfButton;
	delete SifButton;
	delete OpsifButton;

	delete ModeText;
	delete ModeButton;

	if (helpApp != NULL) {
		helpApp->appClose();
		delete (VhelpApp*)helpApp;
	}
}

void
CaptureMenuView::open()
{
	actionEvent = new tkTextViewEvent(tkEVENT_ACTION,this,
						getParentWindow()->getgid());
	tkGetEventManager()->expressInterest(actionEvent);
}

void
CaptureMenuView::resize()
{
	register float x;
	Box2 bb = getBounds();
	register float y = bb.ylen - TOPMARGIN;

	fileText->changeArea(Box2(LEFTMARGIN,y,
				  bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		// Record and Replay Buttons
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		saveRGBButton->setBounds(bloc);

		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		bloc.xorg = x;
		recordButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		replayButton->setBounds(bloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	DintText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		Box2 tbb;
		DintUnitText->getBoundingBox(tbb);

		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 tloc(x,y,tbb.xlen,PICK_BTN_HT);

		DintUnitText->changeArea(tloc);

		// Snapshot interval
		tloc.xorg += tloc.xlen;
		tloc.xlen = bb.xlen - x - tloc.xorg;
		DintEdit->setBounds(tloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	FrameText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		OpsifButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		SifButton->setBounds(bloc);

		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		bloc.xorg = x;
		NifButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		SrfButton->setBounds(bloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	ModeText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		// Mode buttons
		register float x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		ActModeButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		PasModeButton->setBounds(bloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	QintText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		// Units prompt and value
		Box2 tbb;
		QintUnitsText->getBoundingBox(tbb);

		register float x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 tloc(x,y,tbb.xlen,PICK_BTN_HT);

		QintUnitsText->changeArea(tloc);

		tloc.xorg += tloc.xlen;
		tloc.xlen = bb.xlen - x - tloc.xorg;
		QintEdit->setBounds(tloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	// Aging interval
	AgeText->changeArea(Box2(LEFTMARGIN,y,
				 bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		// Units prompt and value
		Box2 tbb;
		AgeUnitsText->getBoundingBox(tbb);

		register float x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 tloc(x,y,tbb.xlen,PICK_BTN_HT);

		AgeUnitsText->changeArea(tloc);

		tloc.xorg += tloc.xlen;
		tloc.xlen = bb.xlen - x - tloc.xorg;
		AgeEdit->setBounds(tloc);
	}
	y -= 1.5 * PICK_BTN_HT;

}

void
CaptureMenuView::rcvEvent(tkEvent *e)
{
	int v;
	register tkObject *sender = e->sender();

	switch (e->name()) {
	case tkEVENT_VALUECHANGE:
		if (sender == saveRGBButton) {
			FV.saveImage();
			break;
		}
		if (sender == recordButton) {
			FV.ask3way(RECORDMSG,
				   FV_RECORD,FV_RECORD_OFF,FV.RecordDir);
			break;
		}
		if (sender == replayButton) {
			tkEvent replay(FV_REPLAY);
			FV.rcvEvent(&replay);
			break;
		}
		if (sender == NifButton) {
			((tkValueEvent*)e)->getValue(&v);
			if (v == ON)
				FV.FrameSelect |= SEL_NIF;
			else
				FV.FrameSelect &= ~SEL_NIF;
			FV.Frame.kickpoke();
			break;
		}
		if (sender == SrfButton) {
			((tkValueEvent*)e)->getValue(&v);
			if (v == ON)
				FV.FrameSelect |= SEL_SRF;
			else
				FV.FrameSelect &= ~SEL_SRF;
			break;
		}
		if (sender == SifButton) {
			((tkValueEvent*)e)->getValue(&v);
			if (v == ON)
				FV.FrameSelect |= SEL_CSIF;
			else
				FV.FrameSelect &= ~SEL_CSIF;
			FV.Frame.kickpoke();
			break;
		}
		if (sender == OpsifButton) {
			((tkValueEvent*)e)->getValue(&v);
			if (v == ON)
				FV.FrameSelect |= SEL_OPSIF;
			else
				FV.FrameSelect &= ~SEL_OPSIF;
			FV.Frame.kickpoke();
			break;
		}
		if (sender == ModeButton) {
			((tkValueEvent*)e)->getValue(&v);
			FV.ActiveMode = (v == ON) ? 1 : 0;
			if (FV.ActiveMode)
				addAView(*QintEdit);
			else
				removeAView(*QintEdit);
			getParentWindow()->redrawWindow();
			FV.Frame.kickpoke();
			break;
		}
		break;

	case tkEVENT_ACTION:
		if (sender == QintEdit) {
			if (((tkTextViewEvent*)e)->getterminator()) {
				int qint;
				strcpy(QintBuf, QintEdit->gettext());
				QintEdit->loseKeyboardFocus();
				qint = (int) atol(QintBuf);
				if (qint > 2)
					FV.Qint = qint;
				FV.Frame.kickpoke();
				break;
			}
			QintEdit->setText(QintBuf);
			break;
		}
		if (sender == DintEdit) {
			if (((tkTextViewEvent*)e)->getterminator()) {
				strcpy(DintBuf, DintEdit->gettext());
				DintEdit->loseKeyboardFocus();
				FV.Dint = (int) atol(DintBuf);
				break;
			}
			DintEdit->setText(DintBuf);
			break;
		}
		if (sender == AgeEdit) {
			if (((tkTextViewEvent*)e)->getterminator()) {
				int age;
				strcpy(AgeBuf, AgeEdit->gettext());
				AgeEdit->loseKeyboardFocus();
				age = (int) atol(AgeBuf);
				if (age > 0)
					FV.AgeFactor = age;
				break;
			}
			AgeEdit->setText(AgeBuf);
			break;
		}
		break;

	case HELP_OPEN:
		if (helpApp != NULL) {
			helpApp->PopWindow();
			break;
		}

		helpApp = new VhelpApp(HELPFILE);
		helpApp->SetSection("Capture");
		helpApp->SetTitle("Help on Capture");
		helpApp->appInit(0, 0, (char **)0, (char **)0);
		break;

	case HELP_CLOSE:
		if (sender == helpApp) {
			helpApp->appClose();
			delete (VhelpApp*)helpApp;
			helpApp = NULL;
		}
		break;
	}
}

void
CaptureMenuView::paint()
{
	pushmatrix();
	localTransform();

	fileText->draw();
	DintText->draw();
	DintUnitText->draw();
	FrameText->draw();
	ModeText->draw();
	if (FV.ActiveMode) {
		QintText->draw();
		QintUnitsText->draw();
	}
	AgeText->draw();
	AgeUnitsText->draw();

	popmatrix();

	tkParentView::paint();
}

/*
 * Demo Menu
 */
DemoMenuView::DemoMenuView()
{
	tkPen pen(BLACK,BG_COLOR,-1,-1);
	Box2 box(0.0,0.0,100.0,100.0);

	tkPen *tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
					TVIEW_EDGE_ICOLOR,TVIEW_BORDER_LW);

	// RingOp State
	OpState = RINGOPR;
	OpText = new tkLabel(box,pen,"Ring operation state:");
	OpText->setjust();
	OpButton = new tkRadioButton;
	OpButton->setClient(this);
	addAView(*OpButton);

	RingOpButton = makePickButton("Ring OK",1,PICK_BTN_WD,PICK_BTN_HT);
	OpButton->addButton(*RingOpButton);
	BeaconButton = makePickButton("Beacon",2,PICK_BTN_WD,PICK_BTN_HT);
	OpButton->addButton(*BeaconButton);
	ClaimButton = makePickButton("Claim",3,PICK_BTN_WD,PICK_BTN_HT);
	OpButton->addButton(*ClaimButton);

	// Una
	sprintf(UnaBuf, "%d", FV.DemoUnas);
	UnaText = new tkLabel(box,pen,"Upstream nbrs: ");
	UnaText->setjust(LEFTALIGNED,CENTERED);
	UnaEdit = makeFancyTextView(UnaBuf,sizeof(UnaBuf),box,tvPen);
	UnaEdit->setClient(this);
	UnaEdit->setTranslationTable(NumbersOnly);
	addAView(*UnaEdit);

	// Dna
	sprintf(DnaBuf, "%d", FV.DemoDnas);
	DnaText = new tkLabel(box,pen,"Downstream nbrs: ");
	DnaText->setjust(LEFTALIGNED,CENTERED);
	DnaEdit = makeFancyTextView(DnaBuf,sizeof(DnaBuf),box,tvPen);
	DnaEdit->setClient(this);
	DnaEdit->setTranslationTable(NumbersOnly);
	addAView(*DnaEdit);

	// Orphans
	sprintf(OrphBuf,"%d", FV.DemoOrphs);
	OrphText = new tkLabel(box,pen,"Unhealthy nbrs: ");
	OrphText->setjust(LEFTALIGNED,CENTERED);
	OrphEdit = makeFancyTextView(OrphBuf,sizeof(OrphBuf),box,tvPen);
	OrphEdit->setClient(this);
	OrphEdit->setTranslationTable(NumbersOnly);
	addAView(*OrphEdit);

	// Config
	ConfText = new tkLabel(box,pen,"Ring Configuration: ");
	ConfText->setjust(LEFTALIGNED,CENTERED);

	StationTypeButton = new tkRadioButton;
	StationTypeButton->setClient(this);
	addAView(*StationTypeButton);
	StaStationButton = makePickButton("Station",1,PICK_BTN_WD,PICK_BTN_HT);
	StationTypeButton->addButton(*StaStationButton);
	StaConcentratorButton =
		makePickButton("Concent",2,PICK_BTN_WD,PICK_BTN_HT);
	StationTypeButton->addButton(*StaConcentratorButton);
	if (FV.DemoStaType)
		StationTypeButton->setCurrentButton(StaConcentratorButton);
	else
		StationTypeButton->setCurrentButton(StaStationButton);

	AttTypeButton = new tkRadioButton;
	AttTypeButton->setClient(this);
	addAView(*AttTypeButton);
	DualAttButton = makePickButton("Dual",1,PICK_BTN_WD,PICK_BTN_HT);
	AttTypeButton->addButton(*DualAttButton);
	SingleAttButton = makePickButton("Single",2,PICK_BTN_WD,PICK_BTN_HT);
	AttTypeButton->addButton(*SingleAttButton);
	if (FV.DemoAttType)
		AttTypeButton->setCurrentButton(SingleAttButton);
	else
		AttTypeButton->setCurrentButton(DualAttButton);

	ConnTypeButton = new tkRadioButton;
	ConnTypeButton->setClient(this);
	addAView(*ConnTypeButton);
	WrapButton = makePickButton("Wrapped",1,PICK_BTN_WD,PICK_BTN_HT);
	ConnTypeButton->addButton(*WrapButton);
	TwistButton = makePickButton("Twisted",2,PICK_BTN_WD,PICK_BTN_HT);
	ConnTypeButton->addButton(*TwistButton);
	if (FV.DemoConnType)
		ConnTypeButton->setCurrentButton(TwistButton);
	else
		ConnTypeButton->setCurrentButton(WrapButton);

	// DoDemo
	DoDemoButton = makeMenuButton("Do It",MENU_BTN_WD,MENU_BTN_HT);
	DoDemoButton->setClient(this);
	addAView(*DoDemoButton);

	helpApp = NULL;
}

DemoMenuView::~DemoMenuView()
{
	if (helpApp != NULL) {
		helpApp->appClose();
		delete (VhelpApp*)helpApp;
	}
}

void
DemoMenuView::open()
{
	actionEvent = new tkTextViewEvent(tkEVENT_ACTION,this,
						getParentWindow()->getgid());
	tkGetEventManager()->expressInterest(actionEvent);
}

void
DemoMenuView::resize()
{
	register float x;
	Box2 bb = getBounds();
	register float y = bb.ylen - TOPMARGIN;


	OpText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = bb.xorg + bb.xlen / 2.0 - MENU_BTN_WD_2;
		Box2 bloc(x,y,MENU_BTN_WD,MENU_BTN_HT);

		RingOpButton->setBounds(bloc);
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		BeaconButton->setBounds(bloc);
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		ClaimButton->setBounds(bloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	// Una
	UnaText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	{
		// Add mode add and ignore buttons
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD*3.0/4.0,PICK_BTN_HT);
		bloc.xorg = 2.0 * x + PICK_BTN_WD*5.0/4.0;
		UnaEdit->setBounds(bloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	// Dna
	DnaText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	{
		// Add mode add and ignore buttons
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD*3.0/4.0,PICK_BTN_HT);
		bloc.xorg = 2.0 * x + PICK_BTN_WD*5.0/4.0;
		DnaEdit->setBounds(bloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	// Orph
	OrphText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	{
		// Add mode add and ignore buttons
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD*3.0/4.0,PICK_BTN_HT);
		bloc.xorg = 2.0 * x + PICK_BTN_WD*5.0/4.0;
		OrphEdit->setBounds(bloc);
	}
	y -= VSPACE * PICK_BTN_HT;

	ConfText->changeArea(Box2(LEFTMARGIN,y,bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);

		bloc.yorg = y;
		bloc.xorg = x;
		StaStationButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		StaConcentratorButton->setBounds(bloc);
		y -= VSPACE * PICK_BTN_HT;

		bloc.yorg = y;
		bloc.xorg = x;
		DualAttButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		SingleAttButton->setBounds(bloc);
		y -= VSPACE * PICK_BTN_HT;

		bloc.yorg = y;
		bloc.xorg = x;
		WrapButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		TwistButton->setBounds(bloc);
		y -= VSPACE * PICK_BTN_HT;
	}
	y -= 1.5 * PICK_BTN_HT;

	// Dodemo
	{
		// show mode all and active buttons
		x = (bb.xlen - 2.0 * MENU_BTN_WD) / 3.0;
		Box2 bloc(x,y,MENU_BTN_WD,MENU_BTN_HT);
		bloc.xorg = 2.0 * x +  MENU_BTN_WD;
		DoDemoButton->setBounds(bloc);
	}
}

void
DemoMenuView::rcvEvent(tkEvent *e)
{
	int v;
	register tkObject *sender = e->sender();

	switch (e->name()) {
	case tkEVENT_VALUECHANGE:
		if (sender == DoDemoButton) {
			v = FV.Freeze;
			FV.Freeze = 1;
			FV.Ring.flush_ring();
			FV.Frame.do_demo();
			FV.Freeze = v;
			break;
		}
		if (sender == StationTypeButton) {
			((tkValueEvent*)e)->getValue(&v);
			switch (v) {
				case 1: FV.DemoStaType = 0; break;
				case 2: FV.DemoStaType = 1; break;
			}
			break;
		}
		if (sender == AttTypeButton) {
			((tkValueEvent*)e)->getValue(&v);
			switch (v) {
				case 1: FV.DemoAttType = 0; break;
				case 2: FV.DemoAttType = 1; break;
			}
			break;
		}
		if (sender == ConnTypeButton) {
			((tkValueEvent*)e)->getValue(&v);
			switch (v) {
				case 1: FV.DemoConnType = 0; break;
				case 2: FV.DemoConnType = 1; break;
			}
			break;
		}
		if (sender == OpButton) {
			((tkValueEvent*)e)->getValue(&v);
			switch (v) {
				case 1:
					FV.RingOpr = RINGOPR;
					FV.sig_blink = 0;
					break;
				case 2:
					FV.RingOpr = BEACONING;
					break;
				case 3:
					FV.RingOpr = CLAIMING;
					break;
			}
			FV.RINGWindow->redrawWindow();
			FV.MAPWindow->redrawWindow();
			break;
		}
		break;

	case tkEVENT_ACTION:
		if (sender == UnaEdit) {
			if (((tkTextViewEvent*)e)->getterminator()) {
				strcpy(UnaBuf, UnaEdit->gettext());
				sscanf(UnaBuf, "%d", &FV.DemoUnas);
				UnaEdit->loseKeyboardFocus();
				break;
			}
			UnaEdit->setText(UnaBuf);
			break;
		}
		if (sender == DnaEdit) {
			if (((tkTextViewEvent*)e)->getterminator()) {
				strcpy(DnaBuf, DnaEdit->gettext());
				sscanf(DnaBuf, "%d", &FV.DemoDnas);
				DnaEdit->loseKeyboardFocus();
				break;
			}
			DnaEdit->setText(DnaBuf);
			break;
		}
		if (sender == OrphEdit) {
			if (((tkTextViewEvent*)e)->getterminator()) {
				strcpy(OrphBuf, OrphEdit->gettext());
				sscanf(OrphBuf,"%d", &FV.DemoOrphs);
				OrphEdit->loseKeyboardFocus();
				break;
			}
			OrphEdit->setText(OrphBuf);
			break;
		}
		break;

	case HELP_OPEN:
		if (helpApp != NULL) {
			helpApp->PopWindow();
			break;
		}

		helpApp = new VhelpApp(HELPFILE);
		helpApp->SetSection("Tutorial");
		helpApp->SetTitle("Help on Tutorial");
		helpApp->appInit(0, 0, (char **)0, (char **)0);
		break;

	case HELP_CLOSE:
		if (sender == helpApp) {
			helpApp->appClose();
			delete (VhelpApp*)helpApp;
			helpApp = NULL;
		}
		break;
	}
}

void
DemoMenuView::paint()
{
	pushmatrix();
	localTransform();

	UnaText->draw();
	DnaText->draw();
	OrphText->draw();
	ConfText->draw();
	OpText->draw();

	popmatrix();

	tkParentView::paint();
}

/*
 * Status Menu
 */
StatMenuView::StatMenuView()
{
	tkPen pen(BLACK,BG_COLOR,-1,-1);
	Box2 box(0.0,0.0,100.0,100.0);

// Local status.
	StatText = new tkLabel(box,pen,"Local status:");
	StatText->setjust();

	// Smtstat
	StatButton = makeMenuButton("SMT stat",PICK_BTN_WD,PICK_BTN_HT);
	StatButton->setClient(this);
	addAView(*StatButton);

	// Ping a station
	PingButton = makeMenuButton("SMT Ping",PICK_BTN_WD,PICK_BTN_HT);
	PingButton->setClient(this);
	addAView(*PingButton);


// Operational parameters
	OprText = new tkLabel(box,pen,"Operational parameters:");
	OprText->setjust();

	// Find station's icon
	NbrButton = makeMenuButton("Neighbor",PICK_BTN_WD,PICK_BTN_HT);
	NbrButton->setClient(this);
	addAView(*NbrButton);

	// Find station's icon
	TreqButton = makeMenuButton("Treq",PICK_BTN_WD,PICK_BTN_HT);
	TreqButton->setClient(this);
	addAView(*TreqButton);

	// Find station's icon
	TnegButton = makeMenuButton("Tneg",PICK_BTN_WD,PICK_BTN_HT);
	TnegButton->setClient(this);
	addAView(*TnegButton);

	// Find station's icon
	TmaxButton = makeMenuButton("Tmax",PICK_BTN_WD,PICK_BTN_HT);
	TmaxButton->setClient(this);
	addAView(*TmaxButton);

	// Find station's icon
	TvxButton = makeMenuButton("Tvx",PICK_BTN_WD,PICK_BTN_HT);
	TvxButton->setClient(this);
	addAView(*TvxButton);

	// Find station's icon
	TminButton = makeMenuButton("Tmin",PICK_BTN_WD,PICK_BTN_HT);
	TminButton->setClient(this);
	addAView(*TminButton);

	// Find station's icon
	SbaButton = makeMenuButton("SBA",PICK_BTN_WD,PICK_BTN_HT);
	SbaButton->setClient(this);
	addAView(*SbaButton);

	// Find station's icon
	FrameButton = makeMenuButton("Frame Ct",PICK_BTN_WD,PICK_BTN_HT);
	FrameButton->setClient(this);
	addAView(*FrameButton);

	// Find station's icon
	ErrButton = makeMenuButton("Error Ct",PICK_BTN_WD,PICK_BTN_HT);
	ErrButton->setClient(this);
	addAView(*ErrButton);

	// Find station's icon
	LostButton = makeMenuButton("Lost Ct",PICK_BTN_WD,PICK_BTN_HT);
	LostButton->setClient(this);
	addAView(*LostButton);

	// Find station's icon
	LerButton = makeMenuButton("LER",PICK_BTN_WD,PICK_BTN_HT);
	LerButton->setClient(this);
	addAView(*LerButton);

	// Find station's icon
	LemButton = makeMenuButton("LEM Ct",PICK_BTN_WD,PICK_BTN_HT);
	LemButton->setClient(this);
	addAView(*LemButton);

	// Find recv_ct
	RecvButton = makeMenuButton("Recv Ct",PICK_BTN_WD,PICK_BTN_HT);
	RecvButton->setClient(this);
	addAView(*RecvButton);

	// Find xmit_ct
	XmitButton = makeMenuButton("Xmit Ct",PICK_BTN_WD,PICK_BTN_HT);
	XmitButton->setClient(this);
	addAView(*XmitButton);

	// Find ncc
	NccButton = makeMenuButton("NCC",PICK_BTN_WD,PICK_BTN_HT);
	NccButton->setClient(this);
	addAView(*NccButton);

	// Find EBS
	EbsButton = makeMenuButton("EBE Ct",PICK_BTN_WD,PICK_BTN_HT);
	EbsButton->setClient(this);
	addAView(*EbsButton);

	helpApp = NULL;
}

StatMenuView::~StatMenuView()
{
	tkGetEventManager()->loseInterest(actionEvent);
	delete actionEvent;
	delete StatText;
	delete StatButton;
	delete PingButton;
	delete OprText;
	delete NbrButton;
	delete TreqButton;
	delete TnegButton;
	delete TmaxButton;
	delete TvxButton;
	delete TminButton;
	delete SbaButton;
	delete FrameButton;
	delete ErrButton;
	delete LostButton;
	delete LerButton;
	delete LemButton;
	delete RecvButton;
	delete XmitButton;
	delete NccButton;
	delete EbsButton;

	if (helpApp != NULL) {
		helpApp->appClose();
		delete (VhelpApp*)helpApp;
	}
}

void
StatMenuView::open()
{
	actionEvent = new tkTextViewEvent(tkEVENT_ACTION,this,
						getParentWindow()->getgid());
	tkGetEventManager()->expressInterest(actionEvent);
}

void
StatMenuView::resize()
{
	register float x;
	Box2 bb = getBounds();
	register float y = bb.ylen - TOPMARGIN;

	// Local status.
	StatText->changeArea(Box2(LEFTMARGIN,y, bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		StatButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		PingButton->setBounds(bloc);
	}
	y -= GROUP_GAP * PICK_BTN_HT;

	// Operational parameters.
	OprText->changeArea(Box2(LEFTMARGIN,y, bb.xlen-LEFTMARGIN,HEADING_HT));
	y -= VSPACE * PICK_BTN_HT;
	{
		x = (bb.xlen - 2.0 * PICK_BTN_WD) / 3.0;
		Box2 bloc(x,y,PICK_BTN_WD,PICK_BTN_HT);
		NbrButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		TreqButton->setBounds(bloc);

		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		TnegButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		TmaxButton->setBounds(bloc);

		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		TvxButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		TminButton->setBounds(bloc);

		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		SbaButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		FrameButton->setBounds(bloc);

		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		ErrButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		LostButton->setBounds(bloc);

		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		LerButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		LemButton->setBounds(bloc);

		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		RecvButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		XmitButton->setBounds(bloc);

		bloc.xorg = x;
		bloc.yorg = (y -= VSPACE * PICK_BTN_HT);
		NccButton->setBounds(bloc);
		bloc.xorg = 2.0 * x + PICK_BTN_WD;
		EbsButton->setBounds(bloc);
	}
	y -= GROUP_GAP * PICK_BTN_HT;
}

void
StatMenuView::rcvEvent(tkEvent *e)
{
	register tkObject *sender = e->sender();

	switch (e->name()) {
	case tkEVENT_VALUECHANGE:
		if (sender == StatButton) {
			dostat(FV.trunkname);
			break;
		}
		if (sender == PingButton) {
			FV.ask(PINGHOSTMSG,FV_PINGHOST,FV.PingBuf);
			break;
		}
		if (sender == NbrButton)	FV.statcmd = DUMP_RING;
		else if (sender == TreqButton)	FV.statcmd = DUMP_TREQ;
		else if (sender == TnegButton)	FV.statcmd = DUMP_TNEG;
		else if (sender == TmaxButton)	FV.statcmd = DUMP_TMAX;
		else if (sender == TvxButton)	FV.statcmd = DUMP_TVX;
		else if (sender == TminButton)	FV.statcmd = DUMP_TMIN;
		else if (sender == SbaButton)	FV.statcmd = DUMP_SBA;
		else if (sender == FrameButton) FV.statcmd = DUMP_FRAME;
		else if (sender == ErrButton)	FV.statcmd = DUMP_ERR;
		else if (sender == LostButton)	FV.statcmd = DUMP_LOST;
		else if (sender == LerButton)	FV.statcmd = DUMP_LER;
		else if (sender == LemButton)	FV.statcmd = DUMP_LEM;
		else if (sender == RecvButton)	FV.statcmd = DUMP_RECV;
		else if (sender == XmitButton)	FV.statcmd = DUMP_XMIT;
		else if (sender == NccButton)	FV.statcmd = DUMP_NCC;
		else if (sender == EbsButton)	FV.statcmd = DUMP_EBS;
		else {
			FV.statcmd = -1;
			break;
		}
		{// for shutopido compiler.
			tkEvent stsev(FV_STATUS);
			FV.rcvEvent(&stsev);
		}
		break;

	case HELP_OPEN:
		if (helpApp != NULL) {
			helpApp->PopWindow();
			break;
		}
		helpApp = new VhelpApp(HELPFILE);
		helpApp->SetSection("Status");
		helpApp->SetTitle("Help on Status");
		helpApp->appInit(0, 0, (char **)0, (char **)0);
		break;

	case HELP_CLOSE:
		if (sender == helpApp) {
			helpApp->appClose();
			delete (VhelpApp*)helpApp;
			helpApp = NULL;
		}
		break;
	}
}

void
StatMenuView::paint()
{
	pushmatrix();
	localTransform();
	OprText->draw();
	StatText->draw();
	popmatrix();
	tkParentView::paint();
}
