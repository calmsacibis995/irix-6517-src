/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Scroll Gadget (for manually controlling history)
 *
 *	$Revision: 1.3 $
 *	$Date: 1992/10/23 06:20:08 $
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

#include <tuCallBack.h>
#include <tuLabel.h>
#include <tuFrame.h>
#include <tuStyle.h>
#include <tuUnPickle.h>

#include "graphGadget.h"
#include "netGraph.h"
#include "scrollGadget.h"
#include "scrollLayout.h"


tuDeclareCallBackClass(ScrollGadgetCallBack, ScrollGadget);
tuImplementCallBackClass(ScrollGadgetCallBack, ScrollGadget);

#define MIN_SPEED   -5
#define MAX_SPEED   5
#define SLOW_SPEED  1
#define FAST_SPEED  5

// xxx what do I really need in constructor?
ScrollGadget::ScrollGadget(NetGraph *ng, tuGadget *parent, const char* instanceName)
: tuRowColumn(parent, instanceName)
{
    // printf("ScrollGadget::ScrollGadget\n");
    
    netgraph = ng;
    
    (new ScrollGadgetCallBack(this, ScrollGadget::dragCB))->
				registerName("_dragCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::incCB))->
				registerName("_incCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::decCB))->
				registerName("_decCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::pageIncCB))->
				registerName("_pageIncCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::pageDecCB))->
				registerName("_pageDecCB");


    (new ScrollGadgetCallBack(this, ScrollGadget::stopCB))->
				registerName("_stopCB");

    (new ScrollGadgetCallBack(this, ScrollGadget::fastbackCB))->
				registerName("_fastbackCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::backCB))->
				registerName("_backCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::forwardCB))->
				registerName("_forwardCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::fastforwardCB))->
				registerName("_fastforwardCB");

/****
    (new ScrollGadgetCallBack(this, ScrollGadget::leftCB))->
				registerName("_leftCB");
    (new ScrollGadgetCallBack(this, ScrollGadget::rightCB))->
				registerName("_rightCB");
****/


    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(this, layoutstr);
    scroller = (tuScrollBar*) findGadget("scroller");
 

    
} // ScrollGadget

void
ScrollGadget::getLayoutHints(tuLayoutHints* h) {
    tuRowColumn::getLayoutHints(h);
    
    h->prefHeight = SCROLL_Y_SIZE;
    h->flags |= tuLayoutHints_fixedHeight;
     
}


void
ScrollGadget::setScrollPercShown(float p) {
    // printf("ScrollGadget::setScrollPercShown(%.2f)\n", p);
    scroller->setPercentShown(p);
}

void
ScrollGadget::setScrollPerc(float p) {
    // printf("ScrollGadget::setScrollPerc(%.2f)\n", p);
    scroller->setPosition(p);
}

// these could really be callbacks directly to netgraph, I guess.
void
ScrollGadget::dragCB(tuGadget*) {
    netgraph->scroll(scroller->getPosition());
}

void
ScrollGadget::incCB(tuGadget*) {
    netgraph->scrollInc(1);
}

void
ScrollGadget::decCB(tuGadget*) {
    netgraph->scrollInc(-1);
}

void
ScrollGadget::pageIncCB(tuGadget*) {
    netgraph->scrollInc(2);
}

void
ScrollGadget::pageDecCB(tuGadget*) {
    netgraph->scrollInc(-2);
}


void
ScrollGadget::stopCB(tuGadget*) {
    // set speed & send it to netgraph
    speed = 0;
    netgraph->throttle(speed);

}

void
ScrollGadget::fastbackCB(tuGadget*) {
    speed = -(FAST_SPEED);
    netgraph->throttle(speed);
}

void
ScrollGadget::backCB(tuGadget*) {
    speed = -(SLOW_SPEED);
    netgraph->throttle(speed);
}

void
ScrollGadget::fastforwardCB(tuGadget*) {
    speed = FAST_SPEED;
    netgraph->throttle(speed);
}

void
ScrollGadget::forwardCB(tuGadget*) {
    speed = SLOW_SPEED;
    netgraph->throttle(speed);
}


/****
void
ScrollGadget::leftCB(tuGadget*) {
    // set speed & send it to netgraph
    if (speed > MIN_SPEED) {
	speed--;
	netgraph->throttle(speed);
    }
}

void
ScrollGadget::rightCB(tuGadget*) {
    // set speed & send it to netgraph
    if (speed < MAX_SPEED) {
	speed++;
	netgraph->throttle(speed);
    }
}
***/

