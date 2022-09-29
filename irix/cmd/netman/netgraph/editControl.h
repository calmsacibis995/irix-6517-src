#ifndef __editControl_h
#define __editControl_h

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Edit control panel (for editing individual graphs)
 *
 *	$Revision: 1.4 $
 *	$Date: 1992/09/30 21:24:32 $
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

#include <tuTopLevel.h>

class tuButton;
class tuDeck;
class tuGadget;
class tuGLGadget;
class tuLabel;
class tuMultiChoice;
class tuRowColumn;
class tuTextField;

class ColorGadget;
class ColorPatch;
class NetGraph;
class StripGadget;

class EditControl : public tuTopLevel {
public:

    EditControl::EditControl(NetGraph *ng, const char* instanceName,
	tuTopLevel* othertoplevel, const char* appGeometry);
	
    void    setFilter(char*);
    void    filterType(tuGadget *);
    void    setSg(StripGadget* strip, StripGadget* belowStrip);
    void    enableIntfcButtons();
    
    char*   getFilter()	    { return filter; }
    int	    getType()	    { return type; }
    int	    getBaseRate();
    int	    getColor()	    { return color; }
    int	    getAvgColor()   { return avgColor; }
    int	    getStyle()	    { return style; }
    tuBool  getAlarmSet()   { return alarmSet; }
    tuBool  getAlarmBell()  { return alarmBell; }
    float   getAlarmLoVal() { return alarmLoVal; }
    float   getAlarmHiVal() { return alarmHiVal; }


private:
    const char  *name;
    NetGraph    *netgraph;
    StripGadget *sg;
    StripGadget *belowSg;
    ColorGadget *colorGadget;
//    ColorGadget *avgColorGadget;
    ColorPatch	*colorPatch;
    ColorPatch	*avgColorPatch;
	
    tuGadget    *ui;
    tuDeck	*filterDeck, *typeDeck;
    tuLabel	*filterLabel, *typeLabel;
    tuTextField *filterField;
    tuMultiChoice *typeMulti, *colorMulti;
    tuButton    *etherBtn, *fddiBtn, *tokenRingBtn;
    tuTextField *packetsField, *bytesField;
    tuGadget    *styleRC;
    tuTextField *colorField;
    tuTextField *avgColorField;
    tuButton    *alarmButton;
    tuRowColumn *alarmStuff;
    tuTextField *lowField, *highField;
    tuButton    *bellButton;

    tuBool	neverSetYet;

    char    filter[256];
    int	    type;
    int	    style;
    int	    color;
    int	    avgColor;

    tuBool  alarmSet;
    tuBool  alarmBell;
    float   alarmLoVal;
    float   alarmHiVal;

    void    open();
    void    closeIt(tuGadget*);
    
    void    netfilters(tuGadget *);
    
    void    whatTraffic(tuGadget *);
    void    whatStyle(tuGadget *);
    void    alarmActiveCB(tuGadget *);
    void    alarmBellCB(tuGadget *);
   
    void    colorCB(tuGadget*);

    void    colorType(tuGadget*);
    void    bytesPacketsType(tuGadget *);
    void    lowType(tuGadget *);
    void    highType(tuGadget *);
        
    void    setType(int);
    void    setStyle(int);
};

#endif /* __editControl_h */

