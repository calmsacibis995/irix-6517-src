/*
 * Copyright 1991, 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Graph Gadget (GL stuff for each graph)
 *
 *	$Revision: 1.7 $
 *	$Date: 1992/10/19 18:29:57 $
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

#include <tuStyle.h>
#include <tuWindow.h>

#include <string.h>

#include "boxes.h"
#include "constants.h"
#include "graphGadget.h"
#include "netGraph.h"
#include "stripGadget.h"

#include <gl/gl.h>
#include <gl/glws.h>
#include <stdio.h>

#define NUM_DIVS	 10
#define ORTHO_HEIGHT	99.0
#define ORTHO_WIDTH	99.0


GraphGadget::GraphGadget(NetGraph *ng, StripGadget* s, tuGadget *parent,
			     const char *instanceName)
		: tuGLGadget(parent, instanceName) {
    // Save pointer to netgraph
    netgraph = ng;
    strip = s;
 
    dataBounds = new fBox();
    graphBounds = new iBox();

    bgColor = netgraph->getStripBackgroundIndexColor();
    scaleColor = netgraph->getStripScaleIndexColor();

} // graphGadget


void
GraphGadget::bindResources(tuResourceDB *rdb, tuResourcePath *rp) {

    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(True);
    setRGBmode(False);
    setZBuffer(False);
}


void
GraphGadget::getLayoutHints(tuLayoutHints* h) {
    tuGLGadget::getLayoutHints(h);
    h->prefWidth = 300;
    h->prefHeight = 20;
}


// node: selectButton is left, adjustButton is middle
void
GraphGadget::processEvent(XEvent *ev) {
    // printf("GraphGadget::processEvent(type = %d\n", ev->type);
    if (ev->type == ButtonPress &&
	  ev->xbutton.button == tuStyle::getSelectButton())
	strip->handleSelection();

    tuGLGadget::processEvent(ev);
}


// find appropriate new vertical scale, 
// but don't rescale the graph or change dataBounds
int
GraphGadget::findVertScale(float minScale) {

    if (numberOfPoints <= 0) {
	return 1;
    } else if (scaleType == SCALE_CONSTANT) {
	// someday we should allow locking on anynumber, but
	// now only on 100
	return 100;
//    } else if (scaleType == SCALE_MAXVALUE) {
//	return (int) dataBounds->getHeight();
    } else {

	dataStruct* pnt = strip->getCurrentData();
	float* dataPtr = (float*) pnt;
	float ymax = minScale;

	int i;
	if (netgraph->getHistory()) {
	    int increment = 2 * netgraph->getHighestNumber() + 2;

	    for (i = numberOfPoints; i != 0; i--) {
		// Scale in y
		if (*dataPtr > ymax)
		    ymax = *dataPtr;

		dataPtr += increment;
		if (dataPtr > netgraph->getHistoryEnd())
		    break;
	    }
	    
	} else {
	    for (i = numberOfPoints; i != 0; i--) {
		// Scale in y
		if (pnt->y > ymax)
		    ymax = pnt->y;

		pnt = pnt->next;
	    }
	} // not history


	// Find nice bounds
	int ymaxInt = (int) (ymax + 0.5); // (round up)

	for (i = 1; i < (int)ymaxInt; i *= 10)
	    ;
	if (i / 5 >= ymaxInt)
	    i /= 5;
	else if (i / 2 >= ymaxInt)
	    i /= 2;


	
	return i;
    } 

} // GraphGadget::findVertScale

void
GraphGadget::rescale(tuBool okToShrink) {
    int xmax;
    // printf("GraphGadget::rescale,  strip %d, %d points\n",
    //   strip->getNumber(), numberOfPoints);
    // printf("   dataBounds.height= %.2f, findVertScale()=%.2f\n", 
    //   dataBounds->getHeight(), findVertScale(dataBounds->getHeight()));
    
    // scale in x
    xmax = numberOfPoints - 1;
    dataBounds->setWidth(xmax - dataBounds->getX());

    dataBounds->setY(0.0); // we used to allow non-zero min, but why bother?

    if (scaleType == SCALE_VARIABLE && okToShrink)
	dataBounds->setHeight(findVertScale(0.0));
    else if (scaleType == SCALE_MAXVALUE)
	dataBounds->setHeight(
	    TU_MAX((float)findVertScale(dataBounds->getHeight()), 
		   dataBounds->getHeight()));
    else
	dataBounds->setHeight(findVertScale(dataBounds->getHeight()));

    // Fill in the label
    char buf[8];
    sprintf(buf, "%d", (int)(dataBounds->getHeight()));
    strip->setScale(buf);
	

    // Recalculate the scale
    if (dataBounds->getWidth() != 0 && dataBounds->getHeight() != 0) {
	graphScaleX = ORTHO_WIDTH / dataBounds->getWidth();
	graphScaleY = ORTHO_HEIGHT / dataBounds->getHeight();
    }

    // printf("   new scale: %d\n", (int)(dataBounds->getHeight()));
} // GraphGadget::rescale

void
GraphGadget::render() {
    
    if (!isRenderable() || strip->getCurrentData() == NULL)
	return;
    
    
    // printf("GraphGadget::render, strip %d starts at %x; %d points\n",
    //	 strip->getNumber(),strip->getCurrentData(), numberOfPoints);
    
    winset();
    pushmatrix();
    pushviewport();
    ortho2(0.0, ORTHO_WIDTH, 0.0, ORTHO_HEIGHT);
 
    color(bgColor);

    clear();
 
    // draw horizontal lines
    color(scaleColor);

    float x =  0.0;
    float x2 = ORTHO_WIDTH;
    float y =  0.0;
 
    int numDivs;
    int height = getHeight();
    
    if (height < 25)
	numDivs = 0;
    else if (height < 60)
	numDivs = 2;
    else
	numDivs = 5;

    for (int i = 1; i < numDivs; i++) {
	y+= ORTHO_HEIGHT/numDivs;
	move2(x,y);
	draw2(x2, y);
    }

    // Draw data
    if (numberOfPoints > 0) {
        color(colorIndex);

        if (graphStyle == STYLE_LINE) {
	    if (netgraph->getHistory()) {
		int increment = 2 * netgraph->getHighestNumber() + 2;

		// Move to the first (oldest) point
		float* dataPtr = (float*) strip->getCurrentData();
		// printf("strip %d; first point %x = %f\n", 
		//     strip->getNumber(), dataPtr, *dataPtr);
		x = 0.0;
		y = *dataPtr * graphScaleY;
		move2(x,y);
		dataPtr += increment;

		// Draw to the rest
		for (i = 1; i < numberOfPoints; i++) {
		    x = i * graphScaleX ;
		    y = *dataPtr * graphScaleY;
		    draw2(x,y);
		    dataPtr += increment;
		    if (dataPtr > netgraph->getHistoryEnd())
			break;
		}
	    } else {
		// Move to the first (oldest) point
		dataStruct* pnt = strip->getCurrentData();
		x = 0.0;
		y = pnt->y * graphScaleY;
		move2(x,y);
		pnt = pnt->next;

		// Draw to the rest
		for (i = 1; i < numberOfPoints; i++) {
		    x = i * graphScaleX;
		    y = pnt->y * graphScaleY;
		    draw2(x,y);
		    pnt = pnt->next;
		}
	    } // not history
	} // STYLE_LINE

	if (graphStyle == STYLE_BAR) {
	    if (netgraph->getHistory()) {
		int increment = 2 * netgraph->getHighestNumber() + 2;

		// Move to the first (oldest) point
		float* dataPtr = (float*) strip->getCurrentData();
		// printf("strip %d; first point %x = %f\n", 
		//    strip->getNumber(), dataPtr, *dataPtr);
		float lastx = 0.0;
		dataPtr += increment;
		for (i = 1; i < numberOfPoints; i++) {
		    x = i * graphScaleX;
		    y = *dataPtr * graphScaleY;
		    sboxf(lastx, 0.0, x, y);
		    dataPtr += increment;
		    lastx = x;
		    if (dataPtr > netgraph->getHistoryEnd())
			break;
		}
	    } else {
		// Move to the first (oldest) point
		dataStruct* pnt = strip->getCurrentData();
		float lastx = 0.0;
		pnt = pnt->next;
		for (i = 1; i < numberOfPoints; i++) {
		    x = i * graphScaleX;
		    y = pnt->y * graphScaleY;
		    sboxf(lastx, 0.0, x, y);
		    pnt = pnt->next;
		    lastx = x;
		}
	    } // not history
	} // STYLE_BAR


	if (netgraph->getAvgPeriod() != 0) {
	    // now draw moving average line..
	    color(avgColorIndex);
	    
	    if (netgraph->getHistory()) {
		int increment = 2 * netgraph->getHighestNumber() + 2;

		// Move to the first (oldest) point
		float* dataPtr = (float*) strip->getCurrentData() + 1;
		// printf(" first avg %x = %f\n",   dataPtr, *dataPtr);
		x = 0.0;
		y = *dataPtr * graphScaleY;
		move2(x,y);
		dataPtr += increment;

		    // Draw to the rest
		    for (i = 1; i < numberOfPoints; i++) {
			x = i * graphScaleX ;
			y = *dataPtr * graphScaleY;
			draw2(x,y);
			dataPtr += increment;
			if (dataPtr > netgraph->getHistoryEnd())
			    break;
		    }

	    } else { // not history
		// Move to the first (oldest) point
		dataStruct* pnt = strip->getCurrentData();
		x = 0.0;
		y = pnt->avg * graphScaleY;
		move2(x,y);
		pnt = pnt->next;

		// Draw to the rest
		for (i = 1; i < numberOfPoints; i++) {
		    x = i * graphScaleX;
		    y = pnt->avg * graphScaleY;
		    draw2(x,y);
		    pnt = pnt->next;
		}
	    } // not history

	} // if need to do average
	
    } // if numberOfPoints > 0


    popviewport();
    popmatrix();


    swapbuffers();

    // printf("about to call gflush\n");
//    gflush();
    // printf("               done.\n");
    
} // render


void
GraphGadget::setScaleType(tuBool type) {
    scaleType = type;
    if (type == SCALE_CONSTANT) {
	dataBounds->setHeight(100);
	strip->setScale("100.0");	
    }
    rescale();
}

void
GraphGadget::setDataHeight(float newHeight) {
    dataBounds->setHeight(newHeight);
    // *dataBounds = *bounds;
}
