#ifndef __paramControl_h
#define __paramControl_h

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Param control panel (for setting overall parameters)
 *
 *	$Revision: 1.4 $
 *	$Date: 1992/10/20 17:46:39 $
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

#include <tuGadget.h>
#include <tuTextField.h>
#include <tuTopLevel.h>

class tuCheckBox;
class tuDeck;

class Arg;
class NetGraph;

class ParamControl : public tuTopLevel {
public:

    ParamControl::ParamControl(NetGraph *ng, const char* instanceName,
      tuTopLevel* othertoplevel, const char* appGeometry);

    void	setInterfaceField(char* text);
    void	setIntervalField(char* text);
    void	setPeriodField(char* text);
    void	setAvgPeriodField(char* text);
    void	setUpdateField(char* text);

private:
    const char  *name;
    NetGraph    *netgraph;
    Arg	    *arg;
	
    tuGadget    *ui;
    tuDeck	*interfaceDeck, *intervalDeck;
    tuCheckBox  *maxButton;
    tuCheckBox  *percButton;
    tuCheckBox  *syncButton;
	
    tuGadget    *interfaceGadget;
    tuGadget    *intervalGadget;
	
    tuTextField *avgPeriodField;
    tuTextField *periodField;
    tuTextField *updateField;
	
    void	open();
    void	closeIt(tuGadget*);

    void	whatTime(tuGadget*);
    void	keepMax(tuGadget*);
    void	lockPercScales(tuGadget*);
    void	syncScales(tuGadget*);
    void	interfaceType(tuGadget*);
    void	intervalType(tuGadget*);
    void	avgPerType(tuGadget*);
    void	perType(tuGadget*);
    void	updateTimeType(tuGadget*);
	
};

#endif /* __paramControl_h */

