/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netgraph Parameters control panel
 *
 *	$Revision: 1.5 $
 *	$Date: 1992/10/20 17:46:36 $
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

#include <stdio.h>
#include <string.h>

#include <osfcn.h>


#include <tuCallBack.h>
#include <tuCheckBox.h>
#include <tuMultiChoice.h>
#include <tuRowColumn.h>
#include <tuTextField.h>
#include <tuUnPickle.h>
#include <tuWindow.h>

#include "arg.h"
#include "constants.h"
#include "paramControl.h"
#include "paramLayout.h"
#include "netGraph.h"


tuDeclareCallBackClass(ParamControlCallBack, ParamControl);
tuImplementCallBackClass(ParamControlCallBack, ParamControl);


ParamControl::ParamControl(NetGraph *ng, const char* instanceName,
	  tuTopLevel* othertoplevel, const char* appGeometry)
: tuTopLevel(instanceName, othertoplevel, False, appGeometry)
{
    // Save name
    setName(instanceName);
    setIconName("Parameters");
    
    // Save pointer to netgraph
    netgraph = ng;

    // Set up callbacks
    /*****
    (new ParamControlCallBack(this, ParamControl::accept))->
				registerName("__ParamControl_accept");
    ******/
    (new ParamControlCallBack(this, ParamControl::closeIt))->
				registerName("__ParamControl_close");

    (new ParamControlCallBack(this, ParamControl::whatTime))->
				registerName("_whatTime");
    (new ParamControlCallBack(this, ParamControl::keepMax))->
				registerName("_keepMax");
    (new ParamControlCallBack(this, ParamControl::lockPercScales))->
				registerName("_lockPercScales");
    (new ParamControlCallBack(this, ParamControl::syncScales))->
				registerName("_syncScales");
    (new ParamControlCallBack(this, ParamControl::interfaceType))->
				registerName("_interfaceType");
    (new ParamControlCallBack(this, ParamControl::intervalType))->
				registerName("_intervalType");
    (new ParamControlCallBack(this, ParamControl::avgPerType))->
				registerName("_avgPerType");
    (new ParamControlCallBack(this, ParamControl::perType))->
				registerName("_perType");
    (new ParamControlCallBack(this, ParamControl::updateTimeType))->
				registerName("_updateTimeType");


    (new tuFunctionCallBack(NetGraph::doHelp, PARAM_HELP_CARD))->
				registerName("__ParamControl_help");


    catchDeleteWindow((new ParamControlCallBack(this, ParamControl::closeIt)));
    
    // Unpickle UI  
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(this, layoutstr);
    interfaceDeck = (tuDeck*) findGadget("interfaceDeck");
    intervalDeck = (tuDeck*) findGadget("intervalDeck");

    if (netgraph->getHistory() || netgraph->getRecording()) {
	interfaceGadget = (tuLabel*) interfaceDeck->findGadget("interfaceLabel");
	intervalGadget  = (tuLabel*) intervalDeck->findGadget("intervalLabel");
	
	interfaceDeck->setVisibleChild(interfaceDeck->findGadget("interfaceLabel"));
	intervalDeck->setVisibleChild(intervalDeck->findGadget("intervalLabel"));
	
	findGadget("updateLabel")->markDelete();
	findGadget("updateRC")->markDelete();
	
	// we'll use updateField to keep track of whether history or recording
	updateField = NULL;
    } else {
	interfaceGadget = (tuTextField*) interfaceDeck->findGadget("interfaceField");
	intervalGadget  = (tuTextField*) intervalDeck->findGadget("intervalField");
 	
	interfaceDeck->setVisibleChild(interfaceDeck->findGadget("interfaceFrame"));
	intervalDeck->setVisibleChild(intervalDeck->findGadget("intervalRC"));
	updateField    = (tuTextField*) findGadget("updateField");
	
     }
    
    maxButton  = (tuCheckBox*) findGadget("maxButton");
    percButton = (tuCheckBox*) findGadget("percButton");
    syncButton = (tuCheckBox*) findGadget("syncButton");
    
    avgPeriodField = (tuTextField*) findGadget("avgPeriodField");
    periodField    = (tuTextField*) findGadget("periodField");
 
    arg = new Arg(netgraph);
    arg->initValues();
} // constructor

void
ParamControl::open() {
    // set current values everywhere

    tuMultiChoice *m = tuMultiChoice::Find("timeMulti");
    tuGadget *b = ui->findGadget(timeTypes[arg->timeType]);
    m->setCurrentButton((tuButton*)b, False);

    maxButton->setSelected(arg->maxValues);
    percButton->setSelected(arg->lockPercentages);
    syncButton->setSelected(arg->sameScale);
    
    interfaceGadget->setText(arg->interface);
    
    char tempStr[20];
    if (updateField) {
	sprintf(tempStr, "%.1f", arg->updateTime/10.0);
	updateField->setText(tempStr);
	
	sprintf(tempStr, "%.1f", arg->interval/10.0);
    } else {
	sprintf(tempStr, "%.1f seconds", arg->interval/10.0);
    }
    intervalGadget->setText(tempStr);
    
    sprintf(tempStr, "%.1f", arg->avgPeriod/10.0);
    avgPeriodField->setText(tempStr);

    sprintf(tempStr, "%.1f", arg->period/10.0);
    periodField->setText(tempStr);
    

    tuTopLevel::open();
}


void
ParamControl::closeIt(tuGadget *) {
    unmap();
}


void
ParamControl::whatTime(tuGadget* g) {
    const char *currentName = g->getInstanceName();
    
    for (int t = 0; t < NUM_TIME_TYPES; t++) {
	if (!strcmp(currentName, timeTypes[t])) {
	    arg->timeType = t;
	    break;
	}
    } // find selected type

    netgraph->setTimeType(t);
}

void
ParamControl::keepMax(tuGadget* g) {
    arg->maxValues = g->getSelected();
    
    netgraph->setMaxVals(arg->maxValues);
}

void
ParamControl::lockPercScales(tuGadget* g) {
    arg->lockPercentages = g->getSelected();
    
    netgraph->setLockPercentages(arg->lockPercentages);
}

void
ParamControl::syncScales(tuGadget* g) {
    arg->sameScale = g->getSelected();
    
    netgraph->setSameScale(arg->sameScale);
}



void
ParamControl::interfaceType(tuGadget*) {
    if (arg->interface)
     	delete [] arg->interface;
    arg->interface = strdup(interfaceGadget->getText());
    requestFocus(intervalGadget, tuFocus_explicit);
    
    netgraph->handleNewArgs(arg);
// xxxx replace w/ netgraph->set
}

void
ParamControl::intervalType(tuGadget*) {
    arg->interval = (int) (atof(intervalGadget->getText()) * 10.0 + 0.5 );
    requestFocus(avgPeriodField, tuFocus_explicit);
    
    netgraph->handleNewArgs(arg);
// xxxx replace w/ netgraph->set
}

void
ParamControl::avgPerType(tuGadget*) {
    // multiply by 10 and round off to get nearest integer # of tenths
    arg->avgPeriod = (int) (atof(avgPeriodField->getText()) * 10.0 + 0.5 );
    requestFocus(periodField, tuFocus_explicit);
    
    netgraph->handleNewArgs(arg);
// xxxx replace w/ netgraph->set
}

void
ParamControl::perType(tuGadget*) {
    // multiply by 10 and round off to get nearest integer # of tenths
    arg->period = (int) (atof(periodField->getText()) * 10.0 + 0.5 );
    
    if (updateField)
	requestFocus(updateField, tuFocus_explicit);
    else
	requestFocus(avgPeriodField, tuFocus_explicit);
    
    netgraph->handleNewArgs(arg);
// xxxx replace w/ netgraph->set
}

void
ParamControl::updateTimeType(tuGadget*) {
    arg->updateTime = (int) (atof(updateField->getText()) * 10.0 + 0.5 );
    requestFocus(interfaceGadget, tuFocus_explicit);

    netgraph->handleNewArgs(arg);
// xxxx replace w/ netgraph->set
}


void
ParamControl::setInterfaceField(char* text) {
    interfaceGadget->setText(text);
    arg->interface = strdup(text);
}

void
ParamControl::setIntervalField(char* text) {
    intervalGadget->setText(text);
    arg->interval = (int) (atof(text) * 10.0 + 0.5 );
}

void
ParamControl::setPeriodField(char* text) {
    periodField->setText(text);
    arg->period = (int) (atof(text) * 10.0 + 0.5 );
}

void
ParamControl::setAvgPeriodField(char* text) {
    avgPeriodField->setText(text);
    arg->avgPeriod = (int) (atof(text) * 10.0 + 0.5 );
}

void
ParamControl::setUpdateField(char* text) {
    updateField->setText(text);
    arg->updateTime = (int) (atof(text) * 10.0 + 0.5 );
}
