/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Strip Gadget (one for each graph)
 *
 *	$Revision: 1.7 $
 *	$Date: 1996/02/26 01:27:47 $
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

#include <string.h>

#include <tuColor.h>
#include <tuFrame.h>
#include <tuLabel.h>
#include <tuPalette.h>
#include <tuStyle.h>
#include <tuUnPickle.h>
#include <tuWindow.h>

#include "graphGadget.h"
#include "myLabel.h"
#include "netGraph.h"
#include "stripGadget.h"
#include "stripLayout.h"



//========
void
setBGs(tuGadget* gadget, tuColor* newColor) {
    if (!gadget)
	return;
	
    gadget->setBackground(newColor);
    gadget->getWindow()->addRect(gadget, gadget->getBBox(), True);

    if (!strcmp(gadget->getClassName(), "RowColumn")) {
	tuRowColumn *rc = (tuRowColumn*)gadget;
	int rows = rc->getNumRows();
	int cols = rc->getNumCols();
	for (int r = 0; r < rows; r++)
	    for (int c = 0; c < cols; c++)
		setBGs(rc->getChildAt(r, c), newColor);
	
    } else if (!strcmp(gadget->getClassName(), "BBoard")) {
	tuBBoard *bb = (tuBBoard*)gadget;
	int numChildren = bb->getNumExternalChildren();
	for (int i = 0; i < numChildren; i++)
	    setBGs(bb->getExternalChild(i), newColor);
    }
}

//========


// xxx what do I really need in constructor?
StripGadget::StripGadget(NetGraph *ng, tuGadget *parent, const char* instanceName)
: tuRowColumn(parent, instanceName)
{
    // printf("StripGadget::StripGadget\n");
    
    netgraph = ng;
    
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(this, layoutstr);
   
    filterLabel = (tuLabel*) ui->findGadget("filterLabel");
    typeLabel   = (tuLabel*) ui->findGadget("typeLabel");
    scaleLabel  = (myLabel*) ui->findGadget("scaleLabel");
    maxValLabel = (myLabel*) ui->findGadget("maxValLabel");
    curValLabel = (myLabel*) ui->findGadget("curValLabel");
    
    stripRC = ui->findGadget("stripRC");
    tuGadget *p = ui->findGadget("GLparent");
    graph = new GraphGadget(ng, this, p, "netgraph");


    maxVal = 0.0;
    
    alarmSet = False;
    alarmBell = False;
    alarmLoMet = False;
    alarmHiMet = False;
    alarmLoVal = 0.0;
    alarmHiVal = 0.0;
    
    strcpy(curValText, "       0.00");
    strcpy(maxValText, "       0.00");
    curValText[20] = NULL;
    maxValText[20] = NULL;
    nBytes = 10000;
    nPackets = 1000;
    
} // StripGadget

void
StripGadget::render() {
    // printf("StripGadget::render\n");
// fill background w/ xfill???
    if (netgraph->isIconic())
	return;

    curValLabel->setText(curValText);
    maxValLabel->setText(maxValText);
    graph->render();
}

void
StripGadget::changeBackground() {
    tuColor* newColor;
        
    if (selected)
	newColor = netgraph->getSelectColor();
    else if (alarmLoMet || alarmHiMet)
	newColor = netgraph->getAlarmColor();
    else
	newColor = tuPalette::getBasicBackground(stripRC->getScreen());

    
    tuWindow::Freeze();
    setBGs(stripRC, newColor);
    tuWindow::Unfreeze();

}

void
StripGadget::changeSelected(tuBool sel) {
    // tuBool sel = this == netgraph->getSelectedStrip();
    // printf("strip # %d %sselected\n", number, sel? "":"DE");

    setSelected(sel);
    changeBackground();
 
} // changeSelected

void
StripGadget::setAlarmSet(tuBool s) {
    alarmSet = s;
    if (!alarmSet) {
	setAlarmLoMet(False);
	setAlarmHiMet(False);
    }
}

void
StripGadget::setAlarmLoMet(tuBool a) {
    alarmLoMet = a;
    changeBackground();
} // setAlarmLoMet

void
StripGadget::setAlarmHiMet(tuBool a) {
    alarmHiMet = a;
    changeBackground();
} // setAlarmHiMet

void
StripGadget::setType(int t) {
    type = t;
    
    typeLabel->setText(getTypeText());
} // setType

void
StripGadget::setBaseRate(int n) {
    if (type == TYPE_PNPACKETS)
	setNPackets(n);
    else
	setNBytes(n);

    typeLabel->setText(getTypeText());

}

int
StripGadget::getBaseRate() {
    if (type == TYPE_PNPACKETS)
	return getNPackets();
    else
	return getNBytes();
}

char*
StripGadget::getTypeText() {
    static char typeStr[256];
    if (type < TYPE_PNPACKETS)
	strcpy(typeStr, heading[type]);
    else {
	strcpy(typeStr, "Percent of ");
	char tmpStr[10];
	if (type == TYPE_PNPACKETS)
	    sprintf(tmpStr, "%d ", nPackets);
	else
	    sprintf(tmpStr, "%d ", nBytes);
	strcat(typeStr, tmpStr);
	strcat(typeStr, heading[type]);
    }
    return(typeStr);
    
} // getTypeText

void
StripGadget::setHeadData(int n, dataStruct* ds) {
    // printf("StripGadget::setHeadData(%d, %x); strip %d\n",  n, ds, number);
	
    graph->setNumPoints(n);
    headData = ds;
} // setHeadData

void
StripGadget::zeroMaxVal() {
    strcpy(maxValText, "       0.00");
    maxValLabel->setText(maxValText);
    maxVal = 0.0;
}

void
StripGadget::setCurVal(float cv) {

    // if value greater than or equal to 10, round to nearest integer;
    // if between 1.0 and 10.0, show 1 decimal float (round to 1 dec place);
    // if between 0.01 and 1.0, show 2 decimal float (round to 2 dec places);
    // else display "0.00"
    // Pad on right end w/ blanks for decimal point and 2 dec places,
    // and add an extra zero since with a trailing blank the label
    // will end up narrower than it ought to be.
    // (if last char is '1' it's a problem, too.)
    static char buf[15];
    if (cv >= 10.0)
        sprintf(buf, "%8d   ", (int) (cv + 0.5));
    else if (cv >= 1.0)
        sprintf(buf, "%10.1f ", cv+0.05);
    else if (cv >= .005)
        sprintf(buf, "%11.2f", cv+0.005);
    else
        sprintf(buf, "       0.00");

//printf("setCurVal(%s)(%f)- buf: <%s>\n", filterLabel->getText(), cv, buf);
// xxxxxxxxXXXXXXXXXXXXX
    //curValLabel->setText(buf);
    strncpy(curValText, buf, 20);

    if (cv > maxVal) {
        maxVal = cv;
        // maxValLabel->setText(buf);
	strncpy(maxValText, buf, 20);
	// printf("StripGadget::setCurVal - new max value: <%s>\n", buf);
    }
    
}

u_long
StripGadget::getEventMask() {
    return tuGadget::getEventMask() | ButtonPressMask | ButtonReleaseMask;
}

void
StripGadget::dispatchEvent(XEvent *ev) {
	
    if (ev->type == ButtonPress &&
	ev->xbutton.button == tuStyle::getSelectButton())
	processEvent(ev);
    else
	tuRowColumn::dispatchEvent(ev);
}

// xxx if we select this one, need to deselect any other that's selected
void
StripGadget::processEvent(XEvent *ev) {
    // printf("StripGadget::processEvent(type = %d\n", ev->type);

    if (ev->type == ButtonPress &&
	  ev->xbutton.button == tuStyle::getSelectButton())
	handleSelection();

    tuGadget::processEvent(ev);
}

void
StripGadget::handleSelection() {
    StripGadget* selectedStrip = netgraph->getSelectedStrip();
    if (selectedStrip == this) {
	netgraph->setSelectedStrip(NULL);
	// changeSelected(False);
    } else {
	netgraph->setSelectedStrip(this);
	// if (selectedStrip)
	//     selectedStrip->changeSelected(False);
	// changeSelected(True);
    }
    
}
