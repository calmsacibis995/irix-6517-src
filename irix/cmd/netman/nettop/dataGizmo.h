#ifndef __dataGizmo_h
#define __dataGizmo_h

/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Gizmo (for controlling data-related parameters)
 *
 *	$Revision: 1.7 $
 *	$Date: 1992/10/21 01:37:39 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <tuTextField.h>
#include <tuTopLevel.h>

class tuButton;
class tuCheckBox;
class tuGadget;
class tuOption;
class tuRowColumn;
class tuScrollBar;

class NetTop;
class DialogBox;

class DataGizmo : public tuTopLevel {
public:

	DataGizmo::DataGizmo(NetTop *nt, const char* instanceName,
	  tuTopLevel* othertoplevel, const char* appGeometry);
	
	void	    setFilter(char*);
	void	    setInterface(char* text)
			{ interfaceField->setText(text); }
	
	void	    setFixedScale(int);
	int	    getFixedScale();
	
private:
	const char  *name;
	tuGadget    *ui;
	NetTop	    *nettop;
	DialogBox   *dataDialog; // CVG
	tuOption    *smoothOption;
	tuButton    *etherBtn, *fddiBtn;
	tuTextField *packetsField, *bytesField;
	tuTextField *interfaceField, *filterField;
	tuTextField *chgField, *smoothField;
	tuScrollBar *chgScrollBar, *smoothScrollBar;
	tuRowColumn *smoothStuff;

	tuOption    *scaleChgOption;
    	tuTextField *fixedScaleField;

	tuGadget    *lastFocused;
	
	void initialize(NetTop *nt, const char* instanceName);
	void open();
	void closeIt(tuGadget*);
	
	void whatDisplay(tuGadget *);
	void whatLayer(tuGadget *);
	void filter(tuGadget *);
	void totals(tuGadget *);
	void smooth(tuGadget *);
	void howSmooth(tuGadget *);
	
	void netfilters(tuGadget *);
	void enableIntfcButtons();
	
	void interfaceType(tuGadget *);
	void filterType(tuGadget *);
	void packetsType(tuGadget *);
	void bytesType(tuGadget *);
	
	void dataChgOpt(tuGadget *);
	void smoothOpt(tuGadget *);

	void scaleChange(tuGadget*);	
	void scaleChgOpt(tuGadget*);
	void fixedScaleType(tuGadget*);
	void updateFixedScaleField(int, tuBool tellNT = True);
	
	void processEvent(XEvent *);

};

#endif /* __dataGizmo_h */

