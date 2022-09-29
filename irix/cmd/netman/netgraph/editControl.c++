/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netgraph Edit control panel
 *
 *	$Revision: 1.4 $
 *	$Date: 1992/10/01 01:13:57 $
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


#include <tuButton.h>
#include <tuCallBack.h>
#include <tuCheckBox.h>
#include <tuLabel.h>
#include <tuLabelMenuItem.h>
#include <tuMultiChoice.h>
#include <tuOption.h>
#include <tuRowColumn.h>
#include <tuScrollBar.h>
#include <tuTextField.h>
#include <tuUnPickle.h>
#include <tuWindow.h>

#include "constants.h"
#include "colorGadget.h"
#include "colorPatch.h"
#include "editControl.h"
#include "editLayout.h"
#include "netGraph.h"


tuDeclareCallBackClass(EditControlCallBack, EditControl);
tuImplementCallBackClass(EditControlCallBack, EditControl);


EditControl::EditControl(NetGraph *ng, const char* instanceName,
	  tuTopLevel* othertoplevel, const char* appGeometry)
: tuTopLevel(instanceName, othertoplevel, False, appGeometry)
{
    // Save name
    setName(instanceName);
    setIconName("Edit");
    
    // Save pointer to netgraph
    netgraph = ng;
    
    // Set up callbacks
    (new EditControlCallBack(this, EditControl::netfilters))->
				registerName("__EditControl_netfilters");

    (new EditControlCallBack(this, EditControl::whatTraffic))->
				registerName("_whatTraffic");
    (new EditControlCallBack(this, EditControl::whatStyle))->
				registerName("_whatStyle");
    (new EditControlCallBack(this, EditControl::alarmActiveCB))->
				registerName("_alarmActive");
    (new EditControlCallBack(this, EditControl::alarmBellCB))->
				registerName("_alarmBell");

    (new EditControlCallBack(this, EditControl::colorType))->
				registerName("_colorType");
    (new EditControlCallBack(this, EditControl::filterType))->
				registerName("_filterType");
    (new EditControlCallBack(this, EditControl::bytesPacketsType))->
				registerName("_bytesPacketsType");
    (new EditControlCallBack(this, EditControl::lowType))->
				registerName("_lowType");
    (new EditControlCallBack(this, EditControl::highType))->
				registerName("_highType");

    (new EditControlCallBack(this, EditControl::closeIt))->
				registerName("__EditControl_close");
    (new tuFunctionCallBack(NetGraph::doHelp, EDIT_HELP_CARD))->
				registerName("__EditControl_help");

    catchDeleteWindow((new EditControlCallBack(this, EditControl::closeIt)));
    
    // Unpickle UI  
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(this, layoutstr);

    filterDeck = (tuDeck*) findGadget("filterDeck");
    typeDeck = (tuDeck*) findGadget("typeDeck");
    if (netgraph->getHistory() || netgraph->getRecording()) {
	filterLabel = (tuLabel*) filterDeck->findGadget("filterLabel");
	typeMulti = NULL; // this is what we'll check; make sure it's null
	typeLabel = (tuLabel*) typeDeck->findGadget("typeLabel");

	filterDeck->setVisibleChild(filterDeck->findGadget("histRC"));
	typeDeck->setVisibleChild(typeDeck->findGadget("histRC"));

    } else {
	filterField = (tuTextField*) filterDeck->findGadget("filterField");
	typeMulti = tuMultiChoice::Find("typeMulti");
	
	filterDeck->setVisibleChild(filterDeck->findGadget("fullRC"));
	typeDeck->setVisibleChild(typeDeck->findGadget("fullRC"));

	char tempStr[10];
	sprintf(tempStr, "%d", TYPE_PETHER);
	etherBtn = (tuButton*) findGadget(tempStr);
	sprintf(tempStr, "%d", TYPE_PFDDI);
	fddiBtn  = (tuButton*) findGadget(tempStr);
	// sprintf(tempStr, "%d", TYPE_PTOKENRING);
	// tokenRingBtn  = (tuButton*) findGadget(tempStr);

	packetsField = (tuTextField*) findGadget("packetsField");
	bytesField = (tuTextField*) findGadget("bytesField");    
    }
    
    styleRC = findGadget("styleRC");
    
    colorMulti = tuMultiChoice::Find("colorMulti");
    colorField = (tuTextField*) findGadget("colorField");
    avgColorField = (tuTextField*) findGadget("avgColorField");
    
    tuGadget *p = ui->findGadget("colorParent");
    colorGadget = new ColorGadget(p, "colorGadget");
    colorGadget->setCallBack(new EditControlCallBack(this, EditControl::colorCB));
 
    p = ui->findGadget("colorPatchParent");
    colorPatch = new ColorPatch(p, "colorPatch");
    p = ui->findGadget("avgColorPatchParent");
    avgColorPatch = new ColorPatch(p, "avgColorPatch");

    alarmButton = (tuButton*) findGadget("alarmButton");
    alarmStuff  = (tuRowColumn*) findGadget("alarmStuff");
    bellButton  = (tuButton*) findGadget("bellButton");
    lowField	= (tuTextField*) alarmStuff->findGadget("lowField");
    highField	= (tuTextField*) alarmStuff->findGadget("highField");
    
    neverSetYet = True;
} // constructor

void
EditControl::open() {
    if (typeMulti)
	enableIntfcButtons();

    if (neverSetYet) {
	neverSetYet = False;

	strcpy(filter, "total");
	type = TYPE_PACKETS;
	style = STYLE_BAR;
	color = 4;
	avgColor = 1;
	alarmSet = False;
	alarmBell = False;
	alarmLoVal = 0.0;
	alarmHiVal = 10.0;

	setFilter(filter);
	setType(type);
	if (typeMulti) {
	    bytesField->setText("100000");
	    packetsField->setText("1000");
	}
	setStyle(style);
	colorField->setText("4");
	colorPatch->setColor(4);
 	avgColorField->setText("1");
	avgColorPatch->setColor(1);
	alarmButton->setSelected(alarmSet);
	bellButton->setSelected(alarmBell);
	lowField->setText("0.0");
	highField->setText("10.0");	
    } // neverSetYet
    
    tuTopLevel::open();
} // open



void
EditControl::closeIt(tuGadget *) {
    unmap();
    netgraph->setSelectedStrip(NULL);

}



void
EditControl::netfilters(tuGadget *) {
    if (netgraph->openNetFilters() == 0) {
	netgraph->openDialogBox();
	printf("EditControl::netfilters - openNetFilters failed\n");
    }
}


// enable %ether, %fddi, %tokenRing buttons depending on what kind of
// interface we're snooping on.
// Also if %fddi is selected & we're on an ether, switch them, etc.
// 9/25/92: get rid of token ring button for this release
void
EditControl::enableIntfcButtons() {
    
    tuBool ether = netgraph->isEther();
    tuBool fddi = netgraph->isFddi();
    tuBool tokenRing = netgraph->isTokenRing();

    etherBtn->setEnabled(ether);
    fddiBtn->setEnabled(fddi);
    // tokenRingBtn->setEnabled(tokenRing);
     
    tuButton* b = typeMulti->getCurrentButton();
    
    if ((b == etherBtn && !ether) ||
	// (b == tokenRingBtn && !tokenRing) ||
	(b == fddiBtn  && !fddi)) {
	if (ether)
	    typeMulti->setCurrentButton(etherBtn);
	else if (fddi)
	    typeMulti->setCurrentButton(fddiBtn);
	// else if (tokenRing)
	//    typeMulti->setCurrentButton(tokenRingBtn);
    }
} // enableIntfcButtons

void
EditControl::setFilter(char* newFilter) {
    if (typeMulti)
	filterField->setText(newFilter);
    else
	filterLabel->setText(newFilter);
}

void
EditControl::setSg(StripGadget* strip, StripGadget* belowStrip) {
    sg = strip;
    belowSg = belowStrip;
    
    if (sg) {
	neverSetYet = False;
	
	strcpy(filter, sg->getFilter());
	type = sg->getType();
	style = sg->getStyle();
	color = sg->getColor();
	avgColor = sg->getAvgColor(); 
	alarmSet = sg->getAlarmSet();
	alarmBell = sg->getAlarmBell();
	alarmLoVal = sg->getAlarmLoVal();
	alarmHiVal = sg->getAlarmHiVal();
	
	setFilter(filter);
	setType(type);
	setStyle(style); 
    
	char tempStr[10];
	sprintf(tempStr, " %d", color);
	colorField->setText(tempStr);
	colorPatch->setColor(color);
	sprintf(tempStr, " %d", avgColor);
 	avgColorField->setText(tempStr);
	avgColorPatch->setColor(avgColor);
   
	alarmButton->setSelected(alarmSet);
	alarmActiveCB(NULL);
	bellButton->setSelected(alarmBell);
	sprintf(tempStr, " %.2f", alarmLoVal);
	lowField->setText(tempStr);
	sprintf(tempStr, " %.2f", alarmHiVal);
	highField->setText(tempStr);
	
	if (typeMulti) {
	    char tempStr[10];
	    sprintf(tempStr, "%d", sg->getNBytes());
	    bytesField->setText(tempStr);
	    sprintf(tempStr, "%d", sg->getNPackets());
	    packetsField->setText(tempStr);
	}
    } 

    if (isRenderable())
	render();
} // setSg

void
EditControl::setType(int t) {
    if (typeMulti) {
	char tempStr[10];
	sprintf(tempStr, "%d", t);
	tuGadget *b = ui->findGadget(tempStr);
	typeMulti->setCurrentButton((tuButton*)b, False);
		
    } else {
	if (sg)
	    typeLabel->setText(sg->getTypeText());
	else {
	    typeLabel->setText(graphTypes[t]);
	    // note - this is safe, because we'll only be here
	    // if no strip, which means default type, which has
	    // a simple label (unlike %npackets, etc)
	}
    }
   
}

void
EditControl::setStyle(int s) {
    char tempStr[10];
    sprintf(tempStr, "%d", s);

    tuMultiChoice *m = tuMultiChoice::Find("styleMulti");
    tuGadget *b = styleRC->findGadget(tempStr);
    m->setCurrentButton((tuButton*)b, False);
}


void
EditControl::whatTraffic(tuGadget *g) {
    type = atoi(g->getInstanceName());
    if (sg)
	if (netgraph->setupStripInts(filter,  type,  getBaseRate(), 
		color, avgColor, style,
		alarmSet, alarmBell, alarmLoVal, alarmHiVal,
		0, sg, True)) {
	    // printf("problem in setupStripInts\n");
	    // xxx
	}
}

void
EditControl::whatStyle(tuGadget* g) {
    style = atoi(g->getInstanceName());
    if (sg)
	sg->setStyle(style);
    
}

void
EditControl::alarmActiveCB(tuGadget* g) {
    alarmSet = alarmButton->getSelected();
    bellButton->setEnabled(alarmSet);
    // note this is called w/ NULL to enable/disable the bellButton
    if (g && sg)
	sg->setAlarmSet(alarmSet);
}

void
EditControl::alarmBellCB(tuGadget *g) {
    alarmBell = g->getSelected();
    if (sg)
	sg->setAlarmBell(alarmBell);
}

void
EditControl::colorCB(tuGadget *g) {
    int newColor = ((ColorGadget*)g)->getColor();
    char tempStr[10];
    sprintf(tempStr, "%d", newColor);


//    if (strcmp(g->getInstanceName(), "colorGadget") == 0) {
    if (strcmp(colorMulti->getCurrentButton()->getInstanceName(),
	    "data") == 0) {
	color = newColor;
	colorField->setText(tempStr);
	colorPatch->setColor(color);
	if (sg)
	    sg->setColor(color);
    } else {
	avgColor = newColor;
	avgColorField->setText(tempStr);
	avgColorPatch->setColor(avgColor);
	if (sg)
	    sg->setAvgColor(avgColor);
    }
}


void
EditControl::colorType(tuGadget *g) {
    int newColor = atoi(g->getText());
    if (strcmp(g->getInstanceName(), "colorField") == 0) {
	color = newColor;
	colorPatch->setColor(color);
	if (sg)
	    sg->setColor(color);
	requestFocus(avgColorField, tuFocus_explicit);
    } else {
	avgColor = newColor;
	avgColorPatch->setColor(avgColor);
	if (sg)
	    sg->setAvgColor(avgColor);
	requestFocus(lowField, tuFocus_explicit);
    }
}

void
EditControl::filterType(tuGadget *) {
    strcpy(filter, filterField->getText());
    if (sg)
	if (netgraph->setupStripInts(filter,  type,  getBaseRate(), 
		color, avgColor, style,
		alarmSet, alarmBell, alarmLoVal, alarmHiVal,
		0, sg, True)) {
	    // printf("problem in setupStripInts\n");
	    // xxx
	}
    requestFocus(packetsField, tuFocus_explicit);

}

void
EditControl::bytesPacketsType(tuGadget *g) {
    if (g == packetsField)
	requestFocus(bytesField, tuFocus_explicit);
    else
	requestFocus(colorField, tuFocus_explicit);
    
    if ( (type == TYPE_PNPACKETS && g == packetsField) ||
	 (type == TYPE_PNBYTES && g == bytesField)) {
	if (netgraph->setupStripInts(filter,  type,  getBaseRate(), 
		color, avgColor, style,
		alarmSet, alarmBell, alarmLoVal, alarmHiVal,
		0, sg, True)) {
	    // printf("problem in setupStripInts\n");
	    // xxx
	}
    }
}

void
EditControl::lowType(tuGadget *g) {
    alarmLoVal = atof(g->getText());
    if (sg)
	sg->setAlarmLoVal(alarmLoVal);
    requestFocus(highField, tuFocus_explicit);
}
void
EditControl::highType(tuGadget *g) {
    alarmHiVal = atof(g->getText());
    if (sg)
	sg->setAlarmHiVal(alarmHiVal);
    
    if (typeMulti)
	requestFocus(filterField, tuFocus_explicit);
    else
	requestFocus(colorField, tuFocus_explicit);
}

int
EditControl::getBaseRate() {
    int baseRate;

    if (type == TYPE_PNPACKETS)
	baseRate = atoi(packetsField->getText());
    else 
	baseRate = atoi(bytesField->getText());

    return baseRate;    
}

