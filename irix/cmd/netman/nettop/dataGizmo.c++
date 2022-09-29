/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Gizmo
 *
 *	$Revision: 1.11 $
 *	$Date: 1992/10/21 01:37:28 $
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
#include <tuLabelMenuItem.h>
#include <tuMultiChoice.h>
#include <tuOption.h>
#include <tuRowColumn.h>
#include <tuScrollBar.h>
#include <tuTextField.h>
#include <tuUnPickle.h>
#include <tuWindow.h>
#include <dialog.h>

#include "constants.h"
#include "dataGizmo.h"
#include "dataLayout.h"
#include "netTop.h"
#include "messages.h"

#define MANSCALE_LO	     5
#define MANSCALE_HI	100000

tuDeclareCallBackClass(DataGizmoCallBack, DataGizmo);
tuImplementCallBackClass(DataGizmoCallBack, DataGizmo);


DataGizmo::DataGizmo(NetTop *nt, const char* instanceName,
	  tuTopLevel* othertoplevel, const char* appGeometry)
: tuTopLevel(instanceName, othertoplevel, False, appGeometry)
{
    initialize(nt, instanceName);
}


void
DataGizmo::initialize(NetTop* nt, const char* instanceName) {
    // Save name
    setName(instanceName);
    setIconName("Traffic");
    
    // Save pointer to nettop
    nettop = nt;
    dataDialog = nettop->getDialogBox();
    
    // Set up callbacks
    (new DataGizmoCallBack(this, DataGizmo::netfilters))->
				registerName("__DataGizmo_netfilters");
    (new DataGizmoCallBack(this, DataGizmo::closeIt))->
				registerName("__DataGizmo_close");
    (new tuFunctionCallBack(NetTop::doHelp, DATA_HELP_CARD))->
				registerName("__DataGizmo_help");


    (new DataGizmoCallBack(this, DataGizmo::interfaceType))->
				registerName("__DataGizmo_interfaceType");
    (new DataGizmoCallBack(this, DataGizmo::filterType))->
				registerName("__DataGizmo_filterType");
    (new DataGizmoCallBack(this, DataGizmo::packetsType))->
				registerName("__DataGizmo_packetsType");
    (new DataGizmoCallBack(this, DataGizmo::bytesType))->
				registerName("__DataGizmo_bytesType");

    (new DataGizmoCallBack(this, DataGizmo::dataChgOpt))->
				registerName("__DataGizmo_dataChgOpt");	
    (new DataGizmoCallBack(this, DataGizmo::smoothOpt))->
				registerName("__DataGizmo_smoothOpt");	

    (new DataGizmoCallBack(this, DataGizmo::whatDisplay))->
				registerName("__DataGizmo_whatDisplay");
    (new DataGizmoCallBack(this, DataGizmo::whatLayer))->
				registerName("__DataGizmo_whatLayer");
    (new DataGizmoCallBack(this, DataGizmo::totals))->
				registerName("__DataGizmo_totals");
    (new DataGizmoCallBack(this, DataGizmo::smooth))->
				registerName("__DataGizmo_smooth");
    (new DataGizmoCallBack(this, DataGizmo::howSmooth))->
				registerName("__DataGizmo_howSmooth");

    (new DataGizmoCallBack(this, DataGizmo::scaleChange))->
				registerName("__DataGizmo_scaleChange");
    (new DataGizmoCallBack(this, DataGizmo::scaleChgOpt))->
				registerName("__DataGizmo_scaleChgOpt");
    (new DataGizmoCallBack(this, DataGizmo::fixedScaleType))->
				registerName("__DataGizmo_fixedScaleType");

    catchDeleteWindow((new DataGizmoCallBack(this, DataGizmo::closeIt)));
    
    // Unpickle UI  
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(this, layoutstr);
    etherBtn = (tuButton*) findGadget("%Ether");
    fddiBtn  = (tuButton*) findGadget("%FDDI");

    interfaceField = (tuTextField*) findGadget("interfaceField");
    interfaceField->setText(nettop->getFullSnooperString());

    filterField = (tuTextField*) findGadget("filterField");
    filterField->setText(nettop->getFilter());

    char tempstr[10];
    sprintf(tempstr, " %d", nettop->getNPackets());   
    packetsField = (tuTextField*) findGadget("packetsField");
    packetsField->setText(tempstr);
    
    sprintf(tempstr, " %d", nettop->getNBytes());    
    bytesField = (tuTextField*) findGadget("bytesField");
    bytesField->setText(tempstr);

    // note that smoothOption is a member, but dataChgOption isn't.
    // This is because we need to enable and disable the smoothOption,
    // but aside from setting the initial value, we don't have to touch dataChgOption.
    tuOption* dataChgOption = (tuOption*)findGadget("dataChgOption");
    int tempInt = nettop->getDataUpdateTime();
    char tempStr[4];
    sprintf(tempStr, "%.1f", (float)tempInt * 0.1);
    dataChgOption->setText(tempStr);
    
    smoothOption = (tuOption*)findGadget("smoothOption");
    tempInt = nettop->getDataSmoothPerc();
    sprintf(tempStr, "%d", tempInt);
    smoothOption->setText(tempStr);

    // ui->findGadget("smoothCheckBox")->setSelected(tempInt > 0);

    scaleChgOption = (tuOption*) findGadget("scaleChgOption");
    tempInt = nettop->getScaleUpdateTime();
    sprintf(tempStr, "%.1f", (float)tempInt * 0.1);
    scaleChgOption->setText(tempStr);
    
    fixedScaleField = (tuTextField*) findGadget("fixedScaleField");
    updateFixedScaleField(nettop->getFixedScale());

} // initialize

void
DataGizmo::open() {
    // set current buttons in multiChoices
    
    tuMultiChoice *m = tuMultiChoice::Find("typeMulti");
    if (m == 0) {
	nettop->post(NOMULTICHOICE);
// CVG	fprintf(stderr, "No type multichoice!\n");
// CVG	exit(-1);
    }
    tuGadget *b = ui->findGadget(graphTypes[nettop->getGraphType()]);
    if (b == 0) {
    char buf[100];
    // CVG	fprintf(stderr, "No button for %s\n", graphTypes[nettop->getGraphType()]);
    // CVG	exit(-1);
    sprintf(buf, "No button for %s", graphTypes[nettop->getGraphType()]);
    dataDialog->error(buf);

    }
    m->setCurrentButton((tuButton*)b, False);
    
    
    m = tuMultiChoice::Find("rescaleMulti");

    // xxx for now, set it to 'timed rescaling', and disable the fixed stuff.
    b = ui->findGadget("timed");
    m->setCurrentButton((tuButton*)b, False);
    fixedScaleField->setEnabled(False);

    // enable/disable %ether & %fddi buttons, depending on intfc type
    enableIntfcButtons();

    tuTopLevel::open();
}

void
DataGizmo::setFilter(char* f) {
    filterField->setText(f);
}

void
DataGizmo::closeIt(tuGadget *) {
    unmap();
}

void
DataGizmo::netfilters(tuGadget *) {
    if (nettop->openNetFilters() == 0) {
	nettop->openDialogBox();
	// printf("DataGizmo::netfilters - openNetFilters failed\n");
    }
}

// enable %ether, %fddi buttons depending on what kind of interface
// we're snooping on.
// Also if %fddi is selected & we're on an ether, switch them, etc.
void
DataGizmo::enableIntfcButtons() {

    tuBool ether = nettop->isEther();
    tuBool fddi = nettop->isFddi();

    etherBtn->setEnabled(ether);
    fddiBtn->setEnabled(fddi);
     
    tuMultiChoice *m = tuMultiChoice::Find("typeMulti");
    tuButton* b = m->getCurrentButton();
    
    if (b == etherBtn && fddi) {
	nettop->setGraphType(TYPE_PFDDI);
	m->setCurrentButton(fddiBtn);
    } else if (b == fddiBtn && ether) {
	nettop->setGraphType(TYPE_PETHER);
	m->setCurrentButton(etherBtn);
    }
}

void
DataGizmo::interfaceType(tuGadget *) {
    nettop->setInterface(strdup(interfaceField->getText()));
    enableIntfcButtons();

    // check return code xxx
}

void
DataGizmo::filterType(tuGadget *) {
    nettop->setFilter(strdup(filterField->getText()));
}

void
DataGizmo::packetsType(tuGadget *) {
    nettop->setNPackets(atoi(packetsField->getText()));    
}

void
DataGizmo::bytesType(tuGadget *) {
    nettop->setNBytes(atoi(bytesField->getText()));        
}

void
DataGizmo::whatDisplay(tuGadget *g) {
    /********
    // find which picked of multichoice
    tuMultiChoice *m = tuMultiChoice::Find("typeMulti");
    const char *currentName = m->getCurrentButton()->getInstanceName();
    **********/
    const char *currentName = g->getInstanceName();
    
    if (currentName == 0) {
        char buf[100];
	// fprintf(stderr, "DataGizmo::whatDisplay - No instance name!\n");
	nettop->post(NOINSTANCE);
	
	// exit(-1);
    }

    for (int t = 0; t < NUM_GRAPH_TYPES; t++) {
	if (!strcmp(currentName, graphTypes[t])) {
	    nettop->setGraphType(t);
	}
    } // find selected type
    
    // instead of just field, maybe have multichoice: ether, fddi, <field>?

    if (t == TYPE_PNPACKETS)
	nettop->setNPackets(atoi(packetsField->getText()));
    else if (t == TYPE_PNBYTES)
	nettop->setNBytes(atoi(bytesField->getText()));
} // whatDisplay


void
DataGizmo::whatLayer(tuGadget *g) {
    /********
    // find which picked of multichoice
    tuMultiChoice *m = tuMultiChoice::Find("layerMulti");
    const char *currentName = m->getCurrentButton()->getInstanceName();
    *********/
    const char *currentName = g->getInstanceName();
    
    if (currentName == 0) {
	nettop->post(NOINSTANCE);
//	fprintf(stderr, "No instance name!\n");
//	exit(-1);
    }

    nettop->setUseEaddr(!strcmp(currentName, "gateway"));
} // whatLayer


void
DataGizmo::howSmooth(tuGadget *g) {
    /********
    // find which picked of multichoice
    tuMultiChoice *m = tuMultiChoice::Find("layerMulti");
    const char *currentName = m->getCurrentButton()->getInstanceName();
    *********/
    const char *currentName = g->getInstanceName();

    if (currentName == 0) {
	nettop->post(NOINSTANCE);
//	fprintf(stderr, "No instance name!\n");
//	exit(-1);
    }

    nettop->setLogSmooth(strcmp(currentName, "logSmooth") == 0); 
} // howSmooth



void
DataGizmo::totals(tuGadget *totalsControl) {
    nettop->setShowTotals(totalsControl->getSelected());
}

void
DataGizmo::dataChgOpt(tuGadget *g) {
    float chgTime = atof(g->getText());
    nettop->setDataUpdateTime((int)(chgTime * 10.0));
}

void
DataGizmo::smoothOpt(tuGadget *g) {
    int smoothPerc = atoi(g->getText());
    int oldSmoothPerc = nettop->getDataSmoothPerc();
    nettop->setDataSmoothPerc(smoothPerc);
    if (smoothPerc == 0)
	nettop->stopSmoothing();
    else if (oldSmoothPerc == 0)
	nettop->startSmoothing();
}

void
DataGizmo::smooth(tuGadget *smoothControl) {
    // xxx do I need this checkbox, or just set option to zero?
    if (smoothControl->getSelected()) {
	nettop->startSmoothing();
	smoothOption->setEnabled(True);
    } else {
	nettop->stopSmoothing();
	smoothOption->setEnabled(False);
    }
}


void
DataGizmo::scaleChange(tuGadget *g)
{
    const char *s = g->getInstanceName();
    
    if (s == 0) {
	nettop->post(NOINSTANCE);
//	fprintf(stderr, "No instance name!\n");
//	exit(-1);
    }
    
    if (strcmp(s, "keepMax") == 0) {
	scaleChgOption->setEnabled(False);
	fixedScaleField->setEnabled(False);
	nettop->setScale(0);
    } else if (strcmp(s, "timed") == 0) {
	scaleChgOption->setEnabled(True);
	fixedScaleField->setEnabled(False);
	float chgTime = atof(scaleChgOption->getText());
	nettop->setScaleUpdateTime((int)(chgTime * 10));
    } else if (strcmp(s, "manual") == 0) {
	scaleChgOption->setEnabled(False);
	fixedScaleField->setEnabled(True);
    	nettop->setScale(atoi(fixedScaleField->getText()));
    } else
	puts("bad label");
}


void
DataGizmo::scaleChgOpt(tuGadget *g) {
    float chgTime = atof(g->getText());
    nettop->setScaleUpdateTime((int)(chgTime * 10));
}

void
DataGizmo::fixedScaleType(tuGadget *) {
    int scaleVal = atoi(fixedScaleField->getText());
    scaleVal = TU_MAX(scaleVal, MANSCALE_LO);
    scaleVal = TU_MIN(scaleVal, MANSCALE_HI);
    	
    updateFixedScaleField(scaleVal);
}


void
DataGizmo::updateFixedScaleField(int newValue, tuBool tellNT) {
    char setStr[5];
    sprintf(setStr, " %d", newValue);
    fixedScaleField->setText(setStr);
    if (tellNT)
	nettop->setScale(newValue);
}

void
DataGizmo::setFixedScale(int newValue) {
    updateFixedScaleField(newValue, False);
}

int
DataGizmo::getFixedScale() {
    return(atoi(fixedScaleField->getText()));
}

void
DataGizmo::processEvent(XEvent *ev) {
    if (ev->type == LeaveNotify) {
	nettop->setLastGizmoFocused(this);
	// printf("leaving DataGizmo\n");
	/*****
	if (lastFocused != getFocusedGadget()) {
	    // printf("FOCUS changed\n");
	    lastFocused = getFocusedGadget();
	    nettop->setLastGizmoFocused(this);
	}
	******/
    }
    
    tuTopLevel::processEvent(ev);    
} // processEvent

