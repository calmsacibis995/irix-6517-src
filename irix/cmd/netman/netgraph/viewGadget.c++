/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph View Gadget (main window)
 *
 *	$Revision: 1.6 $
 *	$Date: 1992/10/20 22:04:36 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED ZOOMS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>

#include <tuCallBack.h>
#include <tuColorMap.h>
#include <tuGLGadget.h>
#include <tuRowColumn.h>
#include <tuUnPickle.h>
#include <tuWindow.h>

#include <X11/keysym.h>

#include "netGraph.h"
#include "scrollGadget.h"
#include "stripGadget.h"
#include "timeGadget.h"
#include "viewGadget.h"
#include "viewLayout.h"


#define MIN_Y_PER_GRAPH 90
#define MENU_Y	30


tuDeclareCallBackClass(ViewGadgetCallBack, ViewGadget);
tuImplementCallBackClass(ViewGadgetCallBack, ViewGadget);

ViewGadget::ViewGadget(NetGraph *ng, const char* instanceName,
	    tuColorMap* cmap, tuResourceDB* db,
	    char* progName, char* progClass,
	    Window transientFor, const char* appGeometry)
: tuTopLevel(instanceName, cmap, db, progName, progClass,
	    transientFor, appGeometry)
{
    initialize(ng, instanceName);
}


void ViewGadget::initialize(NetGraph* ng, const char* instanceName) {
    netgraph = ng;
    setName(instanceName);
    setIconName(instanceName);
    numberOfGraphs = 0;
        
    // setup callbacks

    catchDeleteWindow((new ViewGadgetCallBack(this, ViewGadget::quit)));
    catchQuitApp((new ViewGadgetCallBack(this, ViewGadget::quit)));

    // create the children (before unpickling the ui, so the menugadget
    // callbacks will be declared before they're seen in the pickle)

    TUREGISTERGADGETS;
    view = tuUnPickle::UnPickle(this, layoutstr);

   
} // initialize

void
ViewGadget::open() {
    // we don't know until now about recording & history
    
    if (netgraph->getRecording()) {
	// don't allow strips to be added or deleted while we're recording!
	// also, no saving to rc file
	tuGadget* menu = findGadget("actionsMenu");
	menu->findGadget("addItem")->setEnabled(False);
	menu->findGadget("deleteItem")->setEnabled(False);
	findGadget("saveItem")->setEnabled(False);
	findGadget("saveAsItem")->setEnabled(False);
    } else if (netgraph->getHistory()) {
	// don't allow strips to be added or deleted while we're playing back!
	// also, no saving to rc file, and no 'catching up'
	tuGadget* menu = findGadget("actionsMenu");
	menu->findGadget("addItem")->setEnabled(False);
	menu->findGadget("deleteItem")->setEnabled(False);
	menu->findGadget("catchUpItem")->setEnabled(False);
	findGadget("saveItem")->setEnabled(False);
	findGadget("saveAsItem")->setEnabled(False);
    }
 
    
    tuTopLevel::open();    
}

void
ViewGadget::quit(tuGadget* g) {
    // printf("ViewGadget::quit\n");
    netgraph->quit(g);
}

void
ViewGadget::getLayoutHints(tuLayoutHints* h) {
    // printf("ViewGadget::getLayoutHints\n");
    tuTopLevel::getLayoutHints(h);
    
    int minHeight = MIN_Y_PER_GRAPH * numberOfGraphs +
		    timeHeight + scrollHeight + MENU_Y;

    h->prefHeight = TU_MAX(minHeight, tuMinDimension);

}


void
ViewGadget::newTimeArgs(TimeGadget* timeGadget, int per, int intvl, int newType) {
    // printf("ViewGadget::newTimeArgs\n");

    if (newType == TIME_NONE) {
        timeHeight = 0;
    } else {
        timeHeight = TIME_Y_SIZE;
    }

/********
    minsize((float)MIN_X_SIZE,
            (float)MIN_Y_PER_GRAPH * numberOfGraphs+timeHeight+scrollHeight);
*********/

    timeGadget->newTimeArgs(per, intvl, newType);

/******* xxx
    resize();
*********/

}

void
ViewGadget::addTime(TimeGadget *tg) {
    // printf("ViewGadget::addTime\n");
    
    if (tg->getTimeType() == TIME_NONE) {
        timeHeight = 0;
    } else {
        timeHeight = TIME_Y_SIZE;
    }

/*********
    minsize((float)MIN_X_SIZE,
          (float)MIN_Y_PER_GRAPH * numberOfGraphs + timeHeight + scrollHeight);
**********/
}

void
ViewGadget::addScrollBox(ScrollGadget*) {
    // printf("ViewGadget::addScrollBox\n");
    scrollHeight = SCROLL_Y_SIZE;

/**********

    tkCONTEXT savecx;
    tkGID gidGot = getgid();
    savecx = pushContext(gidGot);

    scrollHeight = SCROLL_Y_SIZE;

    if (gidGot != tkInvalidGID) {
        minsize((float)MIN_X_SIZE,
           (float)MIN_Y_PER_GRAPH * numberOfGraphs + timeHeight + scrollHeight);
    } else  {
        minsize((float)MIN_X_SIZE,
                (float)MIN_Y_PER_GRAPH * numberOfGraphs+timeHeight+scrollHeight);
    }

    popContext(savecx);
**********/

}

void
ViewGadget::addAGraph(StripGadget* sg) {
    // printf("ViewGadget::addAGraph\n");

    sg->map();
    updateLayout();
    numberOfGraphs++;
} // addAGraph

void
ViewGadget::removeAGraph(StripGadget* sg) {
    // printf("ViewGadget::removeAGraph\n");
    tuRowColumn* stripParent = (tuRowColumn*) sg->getParent();
    int row, col;
    stripParent->findChild(sg, &row, &col);
    int numRows = stripParent->getNumRows();
    
    if (isMapped()) tuWindow::Freeze();
    
    sg->markDelete();
    
    if (numRows > 1) {
	// printf("ViewGadget::removeAGraph (%d) - had %d rows\n", row, numRows);
	tuGadget* child;
	for (row++; row < numRows; row++) {
	    child = stripParent->getChildAt(row, col);
	    if (child)
		stripParent->place(child, row-1, col);
	}
	stripParent->grow(numRows-1, 1);
	// printf("   grew to %d rows\n", numRows-1);
    } // numRows > 1

    numberOfGraphs--;
    updateLayout();
    
    if (isMapped()) tuWindow::Unfreeze();
    
} // removeAGraph


tuRowColumn*
ViewGadget::getStripParent() {
    return((tuRowColumn*)findGadget("stripParent"));
}

tuGadget*
ViewGadget::getAddItem() {
    return(findGadget("addItem"));
}

tuGadget*
ViewGadget::getEditItem() {
    return(findGadget("editItem"));
}

tuGadget*
ViewGadget::getDeleteItem() {
    return(findGadget("deleteItem"));
}
