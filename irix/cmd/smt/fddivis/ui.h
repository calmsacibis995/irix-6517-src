#ifndef __UI__
#define __UI__
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.5 $
 */

#include <stdParts.h>
#include <tkWindow.h>
#include <tkRadioButton.h>
#include <tkLabel.h>
#include <VhelpApp.h>
#include "menuview.h"

#define HELPFILE	"/usr/lib/fddivis.help"

#undef MENU_BTN_WD
#define MENU_BTN_WD	80.0

#undef PICK_BTN_WD
#define PICK_BTN_WD	80.0

#define MENU_BTN_WD_2 (MENU_BTN_WD / 2.0)
#define MENU_BTN_HT_2 (MENU_BTN_HT / 2.0)

#define TOPMARGIN	20.0
#define LEFTMARGIN	10.0

#define NMAINBUTTONS	4
#define NPROTOS		14

class UserInterfaceWindow : public tkWindow
{
protected:
	MenuView *Menu;

	char *image_fp;		// image file name
	u_long *image_location;	// image buf ptr
	int xsize;		// image size X
	int ysize;		// image size Y
	int image_size;		// total image size

public:
	virtual void open();
	virtual void resize();
	virtual void clearWindow();
	virtual void redrawWindow();
};

class CaptureMenuView : public ResizeParentView
{
protected:
// Snapshot
	tkLabel		*fileText;
	tkButton	*replayButton;
	tkButton	*recordButton;
	tkButton	*saveRGBButton;
	tkButton	*clearButton;
	
	// Snap shot interval
	tkLabel		*DintText;
	tkLabel		*DintUnitText;
	tkTextView	*DintEdit;
	char DintBuf[128];

// Frame selection buttons
	tkLabel		*FrameText;
	tkButton	*NifButton;
	tkButton	*SrfButton;
	tkButton	*SifButton;
	tkButton	*OpsifButton;

// Capture Mode
	tkLabel		*ModeText;
	tkRadioButton	*ModeButton;
	tkButton	*ActModeButton;
	tkButton	*PasModeButton;
	// Query interval
	tkLabel		*QintText;
	tkLabel		*QintUnitsText;
	tkTextView	*QintEdit;
	char QintBuf[128];
	// Aging limit
	tkLabel		*AgeText;
	tkLabel		*AgeUnitsText;
	tkTextView	*AgeEdit;
	char AgeBuf[128];

	tkTextViewEvent *actionEvent;

// Help App
	VhelpApp *helpApp;
public:
	CaptureMenuView();
	~CaptureMenuView();

	virtual void open();
	virtual void resize();
	virtual void paint();
	virtual void rcvEvent(tkEvent *e);
};

class DemoMenuView : public ResizeParentView
{
protected:
	char UnaBuf[8];
	char DnaBuf[8];
	char OrphBuf[8];

	tkTextViewEvent *actionEvent;

	// Frame selection buttons
	int		OpState;
	tkLabel		*OpText;
	tkRadioButton	*OpButton;
	tkButton	*RingOpButton;
	tkButton	*BeaconButton;
	tkButton	*ClaimButton;

	// Una
	tkLabel		*UnaText;
	tkTextView	*UnaEdit;
	// Dna
	tkLabel		*DnaText;
	tkTextView	*DnaEdit;
	// Orphants
	tkLabel		*OrphText;
	tkTextView	*OrphEdit;

	// Station config
	tkLabel		*ConfText;
	tkRadioButton	*StationTypeButton;
	tkButton	*StaStationButton;
	tkButton	*StaConcentratorButton;

	tkRadioButton	*AttTypeButton;
	tkButton	*DualAttButton;
	tkButton	*SingleAttButton;

	tkRadioButton	*ConnTypeButton;
	tkButton	*WrapButton;
	tkButton	*TwistButton;

	// Doit
	tkButton	*DoDemoButton;

	// Help App
	VhelpApp *helpApp;

public:
	DemoMenuView();
	~DemoMenuView();

	virtual void open();
	virtual void resize();
	virtual void paint();
	virtual void rcvEvent(tkEvent *e);
};

class DispMenuView : public ResizeParentView
{
protected:
	tkTextViewEvent *actionEvent;

// Station Labeling
	tkLabel		*NameNumberText;
	tkRadioButton	*NameNumberButton;
	tkButton	*NameButton;
	tkButton	*NumberButton;

// Address BIT order
	tkLabel		*FCBitOrderText;
	tkRadioButton	*FCBitOrderButton;
	tkButton	*CBitOrderButton;
	tkButton	*FBitOrderButton;

// Display Mode
	tkLabel		*PartialFullText;
	tkRadioButton	*PartialFullButton;
	tkButton	*PartialButton;
	tkButton	*FullButton;

// Magnifed Addr.
	tkLabel		*MagAddrText;
	tkRadioButton	*MagAddrButton;
	tkButton	*MagAddrOffButton;
	tkButton	*MagAddrOnButton;

// Alarm selection buttons
	tkLabel		*AlarmText;
	tkButton	*ClmButton;
	tkButton	*BcnButton;

// Map ops.
	tkLabel		*MapText;
	tkButton	*FreezeButton;	// Toggle button
	tkButton	*RestartButton;	// Momentary buttons
	tkButton	*FindButton;
	tkButton	*TokenButton;	// Toggle button

	tkLabel		*VersText;

// Help App
	VhelpApp *helpApp;
public:
	DispMenuView();
	~DispMenuView();

	virtual void open();
	virtual void resize();
	virtual void paint();
	virtual void rcvEvent(tkEvent *e);
};

class StatMenuView : public ResizeParentView
{
protected:
	tkTextViewEvent *actionEvent;

// Local status.
	tkLabel		*StatText;
	tkButton	*StatButton;
	tkButton	*PingButton;

// Operational parameters.
	tkLabel		*OprText;
	tkButton	*NbrButton;
	tkButton	*TreqButton;
	tkButton	*TnegButton;
	tkButton	*TmaxButton;
	tkButton	*TvxButton;
	tkButton	*TminButton;
	tkButton	*SbaButton;
	tkButton	*FrameButton;
	tkButton	*ErrButton;
	tkButton	*LostButton;
	tkButton	*LerButton;
	tkButton	*LemButton;
	tkButton	*RecvButton;
	tkButton	*XmitButton;
	tkButton	*NccButton;
	tkButton	*EbsButton;

	// Help App
	VhelpApp *helpApp;
public:
	StatMenuView();
	~StatMenuView();

	virtual void open();
	virtual void resize();
	virtual void paint();
	virtual void rcvEvent(tkEvent *e);
};

#endif
