/*
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Time Gadget (time strip)
 *
 *	$Revision: 1.4 $
 *	$Date: 1992/10/23 06:20:16 $
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

#include <tuDeck.h>
#include <tuGadget.h>
#include <tuLabel.h>
#include <tuUnPickle.h>

#include "netGraph.h"
#include "timeGadget.h"
#include "timeLayout.h"
#include "timeScroll.h"





TimeGadget::TimeGadget(NetGraph *ng, tuGadget *parent, const char* instanceName, 
			int per, int intvl, int newType)
: tuRowColumn(parent, instanceName) {
    // printf("TimeGadget::TimeGadget\n");
    
    netgraph = ng;

    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(this, layoutstr);
    
    deck	= (tuDeck*)ui->findGadget("deck");
    absChild    = deck->findGadget("absChild"); 
    relChild    = deck->findGadget("relChild");
    scrollChild = deck->findGadget("scrollChild");
    
    GLparent = scrollChild->findGadget("GLparent");
    scroll = new TimeScroll(ng, this, GLparent, "netgraph", per, intvl);

    relLeftTimeLabel = (tuLabel*) relChild->findGadget("relLeftTimeLabel");
    relMidTimeLabel  = (tuLabel*) relChild->findGadget("relMidTimeLabel");
    relUnitLabel     = (tuLabel*) relChild->findGadget("relUnitLabel");    

    leftTimeLabel  = (tuLabel*) absChild->findGadget("leftTimeLabel");  
    rightTimeLabel = (tuLabel*) absChild->findGadget("rightTimeLabel");  
    leftDateLabel  = (tuLabel*) absChild->findGadget("leftDateLabel");  
    rightDateLabel = (tuLabel*) absChild->findGadget("rightDateLabel");  
    
    timeType = newType;

    newTimeArgs(per, intvl, timeType);
    
} // TimeGadget

void
TimeGadget::map(tuBool propagate) {
    if (timeType == TIME_NONE)
	return;
    else
	tuGadget::map(propagate);
}

void
TimeGadget::getLayoutHints(tuLayoutHints* h) {
    tuRowColumn::getLayoutHints(h);
    // printf("TimeGadget::getLayoutHints; tuRowColumn says (%d, %d)\n",
    // 	h->prefWidth, h->prefHeight);

    if (!mapped || timeType == TIME_NONE) {
	h->prefHeight = 1;
    } else if (timeType == TIME_SCROLLING) {
    	h->prefWidth = 300;
    } else {
    	h->prefWidth = 300;
    }
    
    h->flags |= tuLayoutHints_fixedHeight;
     
//    printf("TimeGadget::getLayoutHints returns (%d, %d)\n", 
//    	h->prefWidth, h->prefHeight);
}


void
TimeGadget::newTimeArgs(int per, int intvl, int newType) {
    periodSec = per / 10; // per is in 10ths, periodSec is seconds
    intervalSec = intvl / 10;
    previousMinutes = -1;
    previousSeconds = -1;
    previousLabel1Clock = -1;
    numTicks = 1;

    setTimeType(newType);
    
    if (newType == TIME_SCROLLING)
	scroll->newTimeArgs(per, intvl);
	
} // newTimeArgs


void
TimeGadget::setScrollingTime() {
    // printf("TimeGadget::setScrollingTime\n");
    timeType = TIME_SCROLLING;
        
    deck->setVisibleChild(scrollChild);
    scroll->resize();
} // setScrollingTime


void
TimeGadget::setAbsoluteTime() {
    // printf("TimeGadget::setAbsoluteTime\n");
    timeType = TIME_ABSOLUTE;
    deck->setVisibleChild(absChild);

} // setAbsoluteTime


// this function sets the time labels to relative time (0, -30 seconds, etc)
void
TimeGadget::setRelativeTime() {
    // printf("TimeGadget::setRelativeTime\n");
    char tempStr[20];

    timeType = TIME_RELATIVE;
    deck->setVisibleChild(relChild);

    // if less than 10 minutes, mark seconds; <10 hours, in minutes, else hours
    if (periodSec < 600) {
	relUnitLabel->setText("Time in seconds");
    } else if (periodSec < 36000) {
	relUnitLabel->setText("Time in minutes");
	periodSec /= 60;
    } else {
	relUnitLabel->setText("Time in hours");
	periodSec /= 3600;
    }

    sprintf(tempStr, "-%d", periodSec);
    relLeftTimeLabel->setText(tempStr);

    float halfPeriod = periodSec / 2.0;
    if (halfPeriod == (int) halfPeriod)
	sprintf(tempStr, "-%d", (int)halfPeriod);
    else
	sprintf(tempStr, "-%.1f", halfPeriod);
    relMidTimeLabel->setText(tempStr);	

} // setRelativeTime

void
TimeGadget::setNoTime() {
    // printf("TimeGadget::setNoTime\n");
    timeType = TIME_NONE;
    // xxx probably remove from parent (pane or rowColumn)?
    unmap();
} // setNoTime


void
TimeGadget::setTimeType(int newType) {
    timeType = newType; // added so map() will work
    if (!isMapped() && newType != TIME_NONE)
	map();
    switch (newType) {
	case TIME_SCROLLING:
	    setScrollingTime();
	    break;
	case TIME_ABSOLUTE:
	    setAbsoluteTime();
	    break;
	case TIME_RELATIVE:
	    setRelativeTime();
	    break;
	case TIME_NONE:
	    setNoTime();
	    break;
    }
	
    updateLayout();
    
} // setTimeType


void
TimeGadget::updateTime(struct timeval tval) {

    if ((timeType == TIME_NONE) || (tval.tv_sec == 0))
	return;


    // if using relative time markers, labels never change.
    // if absolute, they change once per second or once per new sample
    // (whichever is less often).

    long clockSec = tval.tv_sec;
    struct tm* timeStruct;
    char tempStr[10];


    // printf("TimeGadget::updateTime(:%d)\n", (localtime(&clockSec))->tm_sec);


    if (timeType == TIME_SCROLLING) {
	scroll->updateTime(tval);

    } else if (timeType == TIME_ABSOLUTE) {
	// only change labels if we're in a new second since the last paint
	if (((timeStruct = localtime(&clockSec))->tm_sec == previousSeconds) &&
	    (intervalSec == 0))
	    return;

	// printf("  redrawing(:%d)\n", (localtime(&clockSec))->tm_sec);

        previousMinutes = timeStruct->tm_min;
        previousSeconds = timeStruct->tm_sec;

	// if interval is less than 5 minutes, show seconds
	if (intervalSec < 300)
	    ascftime(tempStr, "%H:%M:%S", timeStruct);
	else
	    ascftime(tempStr, "%H:%M", timeStruct);
	rightTimeLabel->setText(tempStr);

	ascftime(tempStr, "%b %e", timeStruct);
	rightDateLabel->setText(tempStr);

	clockSec -= (periodSec);
	timeStruct = localtime(&clockSec);
	if (intervalSec < 300)
	    ascftime(tempStr, "%H:%M:%S", timeStruct);
	else
	    ascftime(tempStr, "%H:%M", timeStruct);
	leftTimeLabel->setText(tempStr);

	ascftime(tempStr, "%b %e", timeStruct);
	leftDateLabel->setText(tempStr);
    }

} // updateTime

