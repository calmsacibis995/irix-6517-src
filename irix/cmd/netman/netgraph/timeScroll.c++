/*
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Time Scroller (for time strip)
 *
 *	$Revision: 1.4 $
 *	$Date: 1996/02/26 01:27:48 $
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

#include <tuLabel.h>
#include <tuUnPickle.h>

#include "netGraph.h"
#include "timeGadget.h"
#include "timeScroll.h"

#include <gl/gl.h>
#include <gl/glws.h>

#define ORTHO_HEIGHT	99.0
#define ORTHO_WIDTH	99.0
#define TICK_Y1		 8.0
#define TICK_Y2		50.0


// this table is used to determine the spacing between scrolling time labels;
// the numbers are seconds.
static struct {
    int delta; // time between labels
    int ticks; // number of ticks between labels
} timeTable[] = {
         1,	3,	//  1 second
         2,	3,	//  2 seconds
         5,	4,	//  5 seconds
        10,	4,	// 10 seconds
        15,	2,	// 15 seconds
        30,	5,	// 30 seconds
        60,	5,	//  1 minute
       120,	3,	//  2 minutes
       300,	4,	//  5 minutes
       600,	4,	// 10 minutes
       900,	2,	// 15 minutes
      1800,	5,	// 30 minutes
      3600,	5,	//  1 hour
      7200,	3,	//  2 hours
     14400,	3,	//  4 hours
     21600,	5,	//  6 hours
     43200,	5,	// 12 hours
     86400,	3	// 24 hours
};

const int TABLESIZE = 17; // index of highest entry in the table



TimeScroll::TimeScroll(NetGraph *ng, TimeGadget *tg, tuGadget *parent,
			const char* instanceName, int per, int intvl)
: tuGLGadget(parent, instanceName) {
    // printf("TimeScroll::TimeScroll\n");
    
    netgraph = ng;
    timegadget = tg;

    newTimeArgs(per, intvl);

    bgColor = netgraph->getStripBackgroundIndexColor();
    scrollColor = netgraph->getScrollingTimeIndexColor();

} // TimeScroll


void
TimeScroll::bindResources(tuResourceDB *rdb, tuResourcePath *rp) {
    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(True);
    setRGBmode(False);
    setZBuffer(False);
}

void
TimeScroll::getLayoutHints(tuLayoutHints* h) {
    if (timegadget->getTimeType() == TIME_SCROLLING) {
	tuGLGadget::getLayoutHints(h);
	h->prefHeight = 25;
	h->flags |= tuLayoutHints_fixedHeight;
	// printf("TimeScroll::getLayoutHints returns(%d, %d)\n",
	//    h->prefWidth, h->prefHeight);
    }
}

void
TimeScroll::resize() {
    int w = getWidth();
    int h = getHeight();
    if (w > 0 && h > 0)
	resize(w, h);
}


// Basically, we just have to figure out how many labels & ticks,
// since our width might have changed.
void
TimeScroll::resize(int w, int h) {
    // printf("TimeScroll::resize(%d,%d)\n", w, h);
    tuGLGadget::resize(w, h);

    // world coords matter, not screen, so this doesn't change
    // except when periodSec changes; So, now do it in newTimeArgs()
    // timeScale = w / (float) periodSec;

    previousNumScrLabels = numScrLabels;
    // labels should be roughly >= 150 pixels apart
    // add 1 since 2 divisions = 3 labels, and .5 to round off fractions
    numScrLabels = (int) (1.5 + w / 150.0);
    numScrLabels = TU_MIN(numScrLabels, MAX_SCR_LBLS);
    numScrLabels = TU_MAX(numScrLabels, 2);
    // printf("   resize: numScrLabels = %d\n", numScrLabels);

    calcLabelDelta();
    labelSpacing = labelDeltaTime * timeScale;
    tickSpacing = labelSpacing / (numTicks + 1);

    updateTime(netgraph->getH_timestamp());
} // resize
 

void
TimeScroll::newTimeArgs(int per, int intvl) {
    // printf("TimeScroll::newTimeArgs\n");
    periodSec = per / 10; // per is in 10ths, periodSec is seconds
    intervalSec = intvl / 10;
    previousLabel1Clock = -1;
    numTicks = 1;
    numScrLabels = 4;
    previousNumScrLabels = -1;

    // Recalculate the scale (pixels per second)
    // (not really pixels, since world coordinates, not screen
    timeScale = ORTHO_WIDTH / periodSec;

//    calcLabelDelta();
    resize();

} // newTimeArgs


void
TimeScroll::updateTime(struct timeval tval) {

    if (tval.tv_sec == 0)
	return;

    // printf("TimeScroll::updateTime\n");

    doScrollingLabels(tval);

} // updateTime


// based on the period & how many labels, determine the spacing (in seconds)
// between successive scrolling time labels, labelDeltaTime.
void
TimeScroll::calcLabelDelta() {

    // first, find the nominal time (in seconds) between labels, based on the
    // period and numScrLabels.
    // If this is called from resize(), we already figured roughly
    // how many labels there s/b based on ~150 pixels between labels.
    // Note that 3 labels break the period into 2 pieces
    int nomDelta = (int) (periodSec / (numScrLabels - 1));

    // find the largest value in the table that's <= the nominal figure
    for (int i = TABLESIZE; i > 0; i--) {
	if (timeTable[i].delta <= nomDelta)
	    break;
    }


    labelDeltaTime = timeTable[i].delta;

    // update numLabels to what will actually fit with labelDeltaTime between
    // note: add 1 since need n+1 labels to have n sections, and add
    // another 1 so we'll round up.
    numScrLabels = (int) (2 + periodSec / labelDeltaTime);
//    numScrLabels = (int) (1.5 + getWidth() / 150.0);
    numScrLabels = TU_MIN(numScrLabels, MAX_SCR_LBLS);
    numScrLabels = TU_MAX(numScrLabels, 2);
    // printf("calcLabelDelta - nomDelta: %d, numScrLabels = %d\n",
    //   nomDelta, numScrLabels);

    // determine number of ticks in between labels
    numTicks = timeTable[i].ticks;
    
    // xxx copied from resize()
    labelSpacing = labelDeltaTime * timeScale;
    tickSpacing = labelSpacing / (numTicks + 1);

} // calcLabelDelta


// Take the current time, the time between labels, and mods, and set
// the text and positions of the scrolling time labels.
// We'll put scrTimeLabel[0] in the future (to the right of the right edge),
// so it will scroll into sight smoothly.
void
TimeScroll::doScrollingLabels(struct timeval tval) {
    // secondsAgo is how far in the past the 1st label should be
    float secondsAgo = tval.tv_sec % labelDeltaTime + tval.tv_usec/1000000.0;
    // printf("secondsAgo: %.6f\n", secondsAgo);
    // set clock to time of 1st label
    long clock = tval.tv_sec - (int)secondsAgo;

    // only set strings for labels if they've changed
    if ((clock != previousLabel1Clock) || ( numScrLabels != previousNumScrLabels)) {
	previousLabel1Clock = clock;
	previousNumScrLabels = numScrLabels;

	// scrTimeLabel[0] is actually in the future
	clock += labelDeltaTime;

	for (int i = 0; i < numScrLabels; i++) {
	    ascftime(scrTimeText[i], "%H:%M:%S", localtime(&clock));
	    clock -= labelDeltaTime;
	    // printf("label %d: %s\n", i, scrTimeText[i]);
	}
    }

    // scrTimeX is *midpoint* of text
    float x = ORTHO_WIDTH - (secondsAgo * timeScale) + labelSpacing;
    for (int i = 0; i < numScrLabels; i++) {
	scrTimeX[i] = x;
	x -= labelSpacing;
    }


    render();
    
} // doScrollingLabels


/*
 * Draw the time labels
 */
void
TimeScroll::render() {
    if (!isRenderable() || netgraph->isIconic()) return;
    // printf("TimeScroll::render()\n");

    winset();
    pushmatrix();
    pushviewport();

    // make viewport bigger to the left than we really need,
    // and use screenmask to exact area we need, so we'll use
    // fine instead of gross clipping for labels as they scroll off
    //  (double viewport width, double ortho2 width)
    Screencoord left,  right,  bottom,  top;
    getviewport(&left, &right, &bottom, &top);
    viewport(0 - right, right,  bottom,  top);
    scrmask(0, right, bottom, top);

    ortho2(0 - ORTHO_WIDTH, ORTHO_WIDTH, 0.0, ORTHO_HEIGHT);
    
    color(bgColor);
    clear();

    color(scrollColor);

    float tickX;
    float textX;
    // for text position, need to worry about screen vs world
    float textOffset = 0.5 * strwidth(scrTimeText[0]) *
		ORTHO_WIDTH / getWidth();

    int j;
    // draw the labels and the ticks to the left of each
    for (int i = 0; i < numScrLabels; i++) {
	tickX = scrTimeX[i];
	textX = tickX -	textOffset;

	move2(tickX, 0);
	draw2(tickX, TICK_Y1);
	move2(tickX, TICK_Y2);
	draw2(tickX, ORTHO_HEIGHT);
	// ticks
	for (j = 0; j < numTicks; j++) {
	    tickX -= tickSpacing;
	    move2(tickX, 0);
	    draw2(tickX, TICK_Y1);
	}
	cmov2(textX, 10.0);
	charstr(scrTimeText[i]);
    }

    popviewport();
    popmatrix();
    swapbuffers();

//    gflush();
 
} // render

