/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	View Gadget
 *
 *	$Revision: 1.6 $
 *	$Date: 1992/09/15 01:39:16 $
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

#include <tuAccelerator.h>
#include <tuCallBack.h>
#include <tuColorMap.h>
#include <tuGLGadget.h>
#include <tuUnPickle.h>

#include <X11/keysym.h>

#include "netTop.h"
#include "viewGadget.h"
#include "viewLayout.h"

#include "messages.h"

// for each scrollbar: lo & hi of range, initial value,
//   increment, page-increment, % shown
// NOTE: LOW end of range is at the TOP; arrow at top DECrements.
#define INCLIN_INIT     550
#define INCLIN_INC       10
#define INCLIN_PAGE     100
#define INCLIN_SHOWN      0.1
#define AZIM_INIT     1350
#define AZIM_INC       10
#define AZIM_PAGE      100
#define AZIM_SHOWN       0.1
#define ZOOM_INIT    800
#define ZOOM_INC     -10
#define ZOOM_PAGE    -50
#define ZOOM_SHOWN     0.1


tuDeclareCallBackClass(ViewGadgetCallBack, ViewGadget);
tuImplementCallBackClass(ViewGadgetCallBack, ViewGadget);

ViewGadget::ViewGadget(NetTop *nt, const char* instanceName,
	    tuColorMap* cmap, tuResourceDB* db,
	    char* progName, char* progClass,
	    Window transientFor, const char* appGeometry)
: tuTopLevel(instanceName, cmap, db, progName, progClass,
	    transientFor, appGeometry)
{
    initialize(nt, instanceName);
}


void ViewGadget::initialize(NetTop* nt, const char* instanceName) {
    // Store name
    setName(instanceName);
    setIconName(instanceName);

    // Save pointer to nettop
    nettop = nt;

        
    // setup callbacks
    (new ViewGadgetCallBack(this, ViewGadget::inclinDrag))->
				registerName("__ViewGadget_inclinDrag");
    (new ViewGadgetCallBack(this, ViewGadget::inclinInc))->
				registerName("__ViewGadget_inclinInc");
    (new ViewGadgetCallBack(this, ViewGadget::inclinDec))->
				registerName("__ViewGadget_inclinDec");
    (new ViewGadgetCallBack(this, ViewGadget::inclinPageInc))->
				registerName("__ViewGadget_inclinPageInc");
    (new ViewGadgetCallBack(this, ViewGadget::inclinPageDec))->
				registerName("__ViewGadget_inclinPageDec");

    (new ViewGadgetCallBack(this, ViewGadget::azimDrag))->
				registerName("__ViewGadget_azimDrag");
    (new ViewGadgetCallBack(this, ViewGadget::azimInc))->
				registerName("__ViewGadget_azimInc");
    (new ViewGadgetCallBack(this, ViewGadget::azimDec))->
				registerName("__ViewGadget_azimDec");
    (new ViewGadgetCallBack(this, ViewGadget::azimPageInc))->
				registerName("__ViewGadget_azimPageInc");
    (new ViewGadgetCallBack(this, ViewGadget::azimPageDec))->
				registerName("__ViewGadget_azimPageDec");

    (new ViewGadgetCallBack(this, ViewGadget::zoomDrag))->
				registerName("__ViewGadget_zoomDrag");
    (new ViewGadgetCallBack(this, ViewGadget::zoomInc))->
				registerName("__ViewGadget_zoomInc");
    (new ViewGadgetCallBack(this, ViewGadget::zoomDec))->
				registerName("__ViewGadget_zoomDec");
    (new ViewGadgetCallBack(this, ViewGadget::zoomPageInc))->
				registerName("__ViewGadget_zoomPageInc");
    (new ViewGadgetCallBack(this, ViewGadget::zoomPageDec))->
				registerName("__ViewGadget_zoomPageDec");

    catchDeleteWindow((new tuFunctionCallBack(ViewGadget::quit, (void*)nt)));
    catchQuitApp((new tuFunctionCallBack(ViewGadget::quit, (void*)nt)));

    // create the children (before unpickling the ui, so the menugadget
    // callbacks will be declared before they're seen in the pickle)

        TUREGISTERGADGETS;
    view = tuUnPickle::UnPickle(this, layoutstr);

    // Put in the graph gadget
    tuGadget *p = view->findGadget("GLparent");
    if (p == 0) {
	nettop->post(NOGLPARENT);
//	fprintf(stderr, "No GLparent!\n");
//	exit(-1);
    }
    graph = new GraphGadget(nt, p, "nettop");
    
    inclinScrollBar = (tuScrollBar*) view->findGadget("inclinScrollBar");
    inclinScrollBar->setContinuousUpdate(True);
    inclinScrollBar->setPosition(INCLIN_INIT);
    inclinScrollBar->setEnabled(True);

    azimScrollBar = (tuScrollBar*) view->findGadget("azimScrollBar");
    azimScrollBar->setContinuousUpdate(True);
    azimScrollBar->setPosition(AZIM_INIT);
    azimScrollBar->setEnabled(True);

    zoomScrollBar = (tuScrollBar*) view->findGadget("zoomScrollBar");
    zoomScrollBar->setContinuousUpdate(True);
    zoomScrollBar->setPosition(ZOOM_INIT);
    zoomScrollBar->setEnabled(True);

    // adjust distance so looks good even if lots of srcs, dests
    int newMax = TU_MAX(nt->getNumSrcs(), nt->getNumDests());
    int dist = (int) (zoomScrollBar->getPosition() * newMax / 5.0);
    zoomScrollBar->setPosition(dist);
    graph->setDistance(dist);
    graph->setInclination((int)inclinScrollBar->getPosition());
    graph->setAzimuth((int)azimScrollBar->getPosition());
        
    homeAcc = new tuAccelerator(NULL, NULL,  XK_Home, False);
    homeAcc->setCallBack(new ViewGadgetCallBack(this, ViewGadget::home));
    addAccelerator(homeAcc);
    
    // xxx actually, should ask nettop about scale type, time, etc.
    // graph->setScaleUpdateTime(nt->getScaleUpdateTime());
} // initialize

void
ViewGadget::quit(tuGadget* g, void* nt) {
    // printf("ViewGadget::quit\n");
    ((NetTop*)nt)->quit(g);
}

void
ViewGadget::home(tuGadget*) {

    // adjust distance so looks good even if lots of srcs, dests
    int newMax = TU_MAX(graph->getNumSrcs(), graph->getNumDests());
    int dist = (int) ((float)ZOOM_INIT * newMax / 5.0);
    zoomScrollBar->setPosition(dist);
    graph->setDistance(dist);
    graph->setInclination(INCLIN_INIT);
    graph->setAzimuth(AZIM_INIT);
    
    zoomScrollBar->setPosition(dist);
    inclinScrollBar->setPosition(INCLIN_INIT);
    azimScrollBar->setPosition(AZIM_INIT);
    graph->render();
}

void ViewGadget::inclinDrag(tuGadget*) {
    graph->setInclination((int)inclinScrollBar->getPosition());
    graph->render();
} // inclinDrag

// set scrollbar to new value, then get that value back (which slider
// has clamped for us) and send it to the graph
void ViewGadget::inclinInc(tuGadget*) {
    inclinScrollBar->setPosition(inclinScrollBar->getPosition() + INCLIN_INC);
    graph->setInclination(inclinScrollBar->getPosition());
    graph->render();
} // inclinInc

void ViewGadget::inclinPageInc(tuGadget*) {
    inclinScrollBar->setPosition(inclinScrollBar->getPosition() + INCLIN_PAGE);
    graph->setInclination(inclinScrollBar->getPosition());
    graph->render();
} // inclinPageInc

void ViewGadget::inclinDec(tuGadget*) {
    inclinScrollBar->setPosition(inclinScrollBar->getPosition() - INCLIN_INC);
    graph->setInclination(inclinScrollBar->getPosition());
    graph->render();
} // inclinDec

void ViewGadget::inclinPageDec(tuGadget*) {
    inclinScrollBar->setPosition(inclinScrollBar->getPosition() - INCLIN_PAGE);
    graph->setInclination(inclinScrollBar->getPosition());
    graph->render();
} // inclinPageDec



void ViewGadget::azimDrag(tuGadget*) {
    graph->setAzimuth((int)azimScrollBar->getPosition());
    graph->render();
} // azimDrag

void ViewGadget::azimInc(tuGadget*) {
    azimScrollBar->setPosition(azimScrollBar->getPosition() + AZIM_INC);
    graph->setAzimuth(azimScrollBar->getPosition());
    graph->render();
} // azimInc

void ViewGadget::azimPageInc(tuGadget*) {
    azimScrollBar->setPosition(azimScrollBar->getPosition() + AZIM_PAGE);
    graph->setAzimuth(azimScrollBar->getPosition());
    graph->render();
} // azimPageInc

void ViewGadget::azimDec(tuGadget*) {
    azimScrollBar->setPosition(azimScrollBar->getPosition() - AZIM_INC);
    graph->setAzimuth(azimScrollBar->getPosition());
    graph->render();
} // azimDec

void ViewGadget::azimPageDec(tuGadget*) {
    azimScrollBar->setPosition(azimScrollBar->getPosition() - AZIM_PAGE);
    graph->setAzimuth(azimScrollBar->getPosition());
    graph->render();
} // azimPageDec



void ViewGadget::zoomDrag(tuGadget*) {
    graph->setDistance((int)zoomScrollBar->getPosition());
    graph->render();
} // zoomDrag

void ViewGadget::zoomInc(tuGadget*) {
    zoomScrollBar->setPosition(zoomScrollBar->getPosition() + ZOOM_INC);
    graph->setDistance(zoomScrollBar->getPosition());
    graph->render();
} // zoomInc

void ViewGadget::zoomPageInc(tuGadget*) {
    zoomScrollBar->setPosition(zoomScrollBar->getPosition() + ZOOM_PAGE);
    graph->setDistance(zoomScrollBar->getPosition());
    graph->render();
} // zoomPageInc

void ViewGadget::zoomDec(tuGadget*) {
    zoomScrollBar->setPosition(zoomScrollBar->getPosition() - ZOOM_INC);
    graph->setDistance(zoomScrollBar->getPosition());
    graph->render();
} // zoomDec

void ViewGadget::zoomPageDec(tuGadget*) {
    zoomScrollBar->setPosition(zoomScrollBar->getPosition() - ZOOM_PAGE);
    graph->setDistance(zoomScrollBar->getPosition());
    graph->render();
} // zoomPageDec

