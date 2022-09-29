/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Node Gizmo
 *
 *	$Revision: 1.13 $
 *	$Date: 1996/02/26 01:27:58 $
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
#include <tuCallBack.h>
#include <tuDeck.h>
#include <tuMultiChoice.h>
#include <tuOption.h>
#include <tuScrollView.h>
#include <tuTextField.h>
#include <tuUnPickle.h>
#include <tuWindow.h>

#include "netTop.h"
#include "nodeGizmo.h"
#include "nodeLayout.h"
#include "messages.h"

tuDeclareCallBackClass(NodeGizmoCallBack, NodeGizmo);
tuImplementCallBackClass(NodeGizmoCallBack, NodeGizmo);


NodeGizmo::NodeGizmo(NetTop *nt, const char* instanceName,
	  tuTopLevel* othertoplevel, const char* appGeometry)
: tuTopLevel(instanceName, othertoplevel, False, appGeometry)
{
    initialize(nt, instanceName);
}


void
NodeGizmo::initialize(NetTop* nt, const char* instanceName)
{
    // Save name
    setName(instanceName);
    setIconName("Nodes");

    // Save pointer to nettop
    nettop = nt;
    whichAxes = nettop->getWhichAxes();
    whichSrcs  = nettop->getWhichSrcs();
    whichDests = nettop->getWhichDests();
    whichNodes = nettop->getWhichNodes();
    
    // Set up callbacks
    (new NodeGizmoCallBack(this, NodeGizmo::closeIt))->
				registerName("__NodeGizmo_close");
    (new tuFunctionCallBack(NetTop::doHelp, NODE_HELP_CARD))->
				registerName("__NodeGizmo_help");

    (new NodeGizmoCallBack(this, NodeGizmo::howLabel))->
				registerName("__NodeGizmo_howLabel");
    (new NodeGizmoCallBack(this, NodeGizmo::whatAxesDisplay))->
				registerName("__NodeGizmo_whatAxesDisplay");
    (new NodeGizmoCallBack(this, NodeGizmo::busyMeans))->
				registerName("__NodeGizmo_busyMeans");
    (new NodeGizmoCallBack(this, NodeGizmo::netfilters))->
				registerName("__NodeGizmo_netfilters");
    (new NodeGizmoCallBack(this, NodeGizmo::whatSrcsDisplay))->
				registerName("__NodeGizmo_whatSrcsDisplay");
    (new NodeGizmoCallBack(this, NodeGizmo::whatDestsDisplay))->
				registerName("__NodeGizmo_whatDestsDisplay");
    (new NodeGizmoCallBack(this, NodeGizmo::whatNodesDisplay))->
				registerName("__NodeGizmo_whatNodesDisplay");

    (new NodeGizmoCallBack(this, NodeGizmo::clickSrcCheck))->
				registerName("__NodeGizmo_clickSrcCheck");
    (new NodeGizmoCallBack(this, NodeGizmo::clickDestCheck))->
				registerName("__NodeGizmo_clickDestCheck");

    (new NodeGizmoCallBack(this, NodeGizmo::getSrcName))->
				registerName("__NodeGizmo_getSrcName");
    (new NodeGizmoCallBack(this, NodeGizmo::getDestName))->
				registerName("__NodeGizmo_getDestName");

    (new NodeGizmoCallBack(this, NodeGizmo::grabSrcs))->
				registerName("__NodeGizmo_grabSrcs");
    (new NodeGizmoCallBack(this, NodeGizmo::grabDests))->
				registerName("__NodeGizmo_grabDests");


    (new NodeGizmoCallBack(this, NodeGizmo::pairOpt))->
				registerName("__NodeGizmo_pairOpt");
    (new NodeGizmoCallBack(this, NodeGizmo::srcOpt))->
				registerName("__NodeGizmo_srcOpt");
    (new NodeGizmoCallBack(this, NodeGizmo::destOpt))->
				registerName("__NodeGizmo_destOpt");
    (new NodeGizmoCallBack(this, NodeGizmo::nodeOpt))->
				registerName("__NodeGizmo_nodeOpt");
    (new NodeGizmoCallBack(this, NodeGizmo::filterOpt))->
				registerName("__NodeGizmo_filterOpt");



    (new NodeGizmoCallBack(this, NodeGizmo::chgOpt))->
				registerName("__NodeGizmo_chgOpt");

    catchDeleteWindow((new NodeGizmoCallBack(this, NodeGizmo::closeIt)));

    // Unpickle UI
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(this, layoutstr);

    bigDeck = (tuDeck*) findGadget("bigDeck");    
    srcsRC = ui->findGadget("srcsRC");
    destsRC = ui->findGadget("destsRC");
        
    char tmpStr[12];
    for (int i = 0; i < MAX_NODES; i++) {
	// probably a better way to do this - just change the digit each time xxx
	sprintf(tmpStr, "srcField%d", i);
	srcFields[i] = (tuTextField*) srcsRC->findGadget(tmpStr);
	srcFields[i]->setText(nettop->srcInfo[i].fullname);
	// srcFields[i]->movePointToHome(False); // CAN'T do this until mapped!
	sprintf(tmpStr, "%d", i);
	srcChecks[i] = (tuCheckBox*) srcsRC->findGadget(tmpStr);
    }
    for (i = 0; i < MAX_NODES; i++) {
	sprintf(tmpStr, "destField%d", i);
	destFields[i] = (tuTextField*) destsRC->findGadget(tmpStr);
	destFields[i]->setText(nettop->destInfo[i].fullname);
	// destFields[i]->movePointToHome(False);
	sprintf(tmpStr, "%d", i);
	destChecks[i] = (tuCheckBox*) destsRC->findGadget(tmpStr);
	
    }
    
    chgOption = (tuOption*) ui->findGadget("chgOption");
    float updateTime = nettop->getNodeUpdateTime() / 10.0;
    sprintf(tmpStr, "%.2f", updateTime);
    // xxx make sure it's a legal value!
    chgOption->setText(tmpStr);
    
    busyBytesBtn = ui->findGadget("busyBytesBtn");
    busyPacketsBtn = ui->findGadget("busyPacketsBtn");

    
    labelType = nettop->getLabelType();
    
} // initialize

void
NodeGizmo::open() {

    // set current buttons in multiChoices
        
    tuMultiChoice *m = tuMultiChoice::Find("labelMulti");
    tuGadget *b = ui->findGadget(labelTypes[labelType]);
    m->setCurrentButton((tuButton*)b, False);

    m = tuMultiChoice::Find("axesDisplayMulti");
    b = ui->findGadget(axesTypes[whichAxes]);
    m->setCurrentButton((tuButton*)b, False);

    m = tuMultiChoice::Find("srcDisplayMulti");
    b = ui->findGadget(srcTypes[whichSrcs]);
    m->setCurrentButton((tuButton*)b, False);
    
    m = tuMultiChoice::Find("destDisplayMulti");
    b = ui->findGadget(destTypes[whichDests]);
    m->setCurrentButton((tuButton*)b, False);

    m = tuMultiChoice::Find("nodeDisplayMulti");
    b = ui->findGadget(nodeTypes[whichNodes]);
    m->setCurrentButton((tuButton*)b, False);

    m = tuMultiChoice::Find("busyMulti");
    if (nettop->getBusyBytes())
	b = ui->findGadget("busyBytesBtn");
    else
	b = ui->findGadget("busyPacketsBtn");
    m->setCurrentButton((tuButton*)b, False);


    switch (whichAxes) {
	case AXES_SRCS_DESTS:
	    bigDeck->setVisibleChild(bigDeck->findGadget("srcsDestsRC"));
	    break;
	case AXES_BUSY_PAIRS:
	    bigDeck->setVisibleChild(bigDeck->findGadget("pairsRC"));
	    break;
	case AXES_NODES_FILTERS:
	    bigDeck->setVisibleChild(bigDeck->findGadget("nodesFiltersRC"));
	    break;
    } // switch
   
    pairOption = (tuOption*) ui->findGadget("pairOption");
    srcOption  = (tuOption*) ui->findGadget("srcOption"); 
    destOption = (tuOption*) ui->findGadget("destOption");
    nodeOption = (tuOption*) ui->findGadget("nodeOption");
    filterOption = (tuOption*) ui->findGadget("filterOption");

    numPairs   = nettop->getNumPairs();
    numSrcs    = nettop->getNumSrcs();
    numDests   = nettop->getNumDests();
    numNodes   = nettop->getNumNodes();
    numFilters = nettop->getNumFilters();
    
    // set options
    char tmpStr[2];
    sprintf(tmpStr, "%d", numPairs);
    pairOption->setText(tmpStr);
    sprintf(tmpStr, "%d", numSrcs);
    srcOption->setText(tmpStr);
    sprintf(tmpStr, "%d", numDests);
    destOption->setText(tmpStr);
    sprintf(tmpStr, "%d", numNodes);
    nodeOption->setText(tmpStr);
    sprintf(tmpStr, "%d", numFilters);
    filterOption->setText(tmpStr);
        
    tuTopLevel::open();
} // open


void
NodeGizmo::closeIt(tuGadget *) {
    unmap();
}


void
NodeGizmo::whatAxesDisplay(tuGadget *g) {
    const char *s = g->getInstanceName();

    if (!strcmp(s, axesTypes[AXES_SRCS_DESTS])) {
	whichAxes = AXES_SRCS_DESTS;
	bigDeck->setVisibleChild(bigDeck->findGadget("srcsDestsRC"));
	autoAble(whichSrcs == NODES_TOP || whichDests == NODES_TOP);
	nettop->setNumSrcs(numSrcs);
	nettop->setNumDests(numDests);
	nettop->setWhichSrcs(whichSrcs);
	nettop->setWhichDests(whichDests);
	for (int i = 2; i < MAX_NODES; i++) {
	    srcChecks[i]->setEnabled(whichSrcs == NODES_TOP);
	    destChecks[i]->setEnabled(whichDests == NODES_TOP);
	}

    } else if (!strcmp(s, axesTypes[AXES_BUSY_PAIRS])) {
	whichAxes = AXES_BUSY_PAIRS;
	bigDeck->setVisibleChild(bigDeck->findGadget("pairsRC"));
	autoAble(True);
	nettop->setNumPairs(numPairs);
	for (int i = 2; i < MAX_NODES; i++) {
	    srcChecks[i]->setEnabled(False);
	    srcChecks[i]->setSelected(False);
	    destChecks[i]->setEnabled(False);
	    destChecks[i]->setSelected(False);
	    // if it's locked, unlock it
	    if (nettop->getSrcLocked(i))
		nettop->setSrcLocked(i, False);
	    if (nettop->getDestLocked(i))
		nettop->setDestLocked(i, False);
	}

    } else if (!strcmp(s, axesTypes[AXES_NODES_FILTERS])) {
	whichAxes = AXES_NODES_FILTERS;
	bigDeck->setVisibleChild(bigDeck->findGadget("nodesFiltersRC"));
	autoAble(whichNodes == NODES_TOP);
	nettop->setNumNodes(numNodes);
	nettop->setNumFilters(numFilters);
	nettop->setWhichNodes(whichNodes);
	for (int i = 2; i < MAX_NODES; i++) {
	    srcChecks[i]->setEnabled(whichSrcs == NODES_TOP);
	    destChecks[i]->setEnabled(False);
	}
    }

/*** xxx
also do:
 nettop->setWhichFoo()?
*****/

    nettop->setWhichAxes(whichAxes);

} // whatAxesDisplay


void
NodeGizmo::whatSrcsDisplay(tuGadget *g)
{
    const char *s = g->getInstanceName();
    if (!strcmp(s, "topSrcs")) {
	whichSrcs = NODES_TOP;
	for (int i = 2; i < MAX_NODES; i++)
	    srcChecks[i]->setEnabled(True);
    } else { // specificSrcs
	whichSrcs = NODES_SPECIFIC;
	for (int i = 2; i < MAX_NODES; i++)
	    srcChecks[i]->setEnabled(False);
	tellNettopSrcNames();
	nettop->restartSnooping(); // xxx is this necs'ry?
    }

    nettop->setWhichSrcs(whichSrcs);

    autoAble(whichSrcs == NODES_TOP || whichDests == NODES_TOP);
} // whatSrcsDisplay


void
NodeGizmo::whatDestsDisplay(tuGadget *g)
{
    const char *s = g->getInstanceName();
    
    if (!strcmp(s, "topDests")) {
	whichDests = NODES_TOP;
	for (int i = 2; i < MAX_NODES; i++)
	    destChecks[i]->setEnabled(True);
    } else { // specificDests
	whichDests = NODES_SPECIFIC;
	for (int i = 2; i < MAX_NODES; i++)
	    destChecks[i]->setEnabled(False);
	tellNettopDestNames();
	nettop->restartSnooping();
    }

    nettop->setWhichDests(whichDests);

    autoAble(whichSrcs == NODES_TOP || whichDests == NODES_TOP);
} // whatDestsDisplay

void
NodeGizmo::whatNodesDisplay(tuGadget *g) {
    const char *s = g->getInstanceName();
    
    if (!strcmp(s, "topNodes")) {
	whichNodes = NODES_TOP;
	for (int i = 2; i < MAX_NODES; i++)
	    srcChecks[i]->setEnabled(True);
    } else { // specificNodes
	whichNodes = NODES_SPECIFIC;
	for (int i = 2; i < MAX_NODES; i++)
	    srcChecks[i]->setEnabled(False);
	tellNettopSrcNames();
	nettop->restartSnooping();
    }

    nettop->setWhichNodes(whichNodes);

    autoAble(whichNodes == NODES_TOP);
} // whatNodesDisplay

void
NodeGizmo::tellNettopSrcNames() {
    for (int i = 0; i < MAX_NODES; i++)
	setInfoName(&nettop->srcInfo[i], srcFields[i]->getText());
    // refresh labels
    nettop->setLabelType(nettop->getLabelType());
} // tellNettopSrcNames

void
NodeGizmo::tellNettopDestNames() {
    for (int i = 0; i < MAX_NODES; i++)
	setInfoName(&nettop->destInfo[i], destFields[i]->getText());
    // refresh labels
    nettop->setLabelType(nettop->getLabelType());
} // tellNettopDestNames

// take care of enabling/disabling stuff relating to automatically
// calculating top nodes.
void
NodeGizmo::autoAble(tuBool enabled) {
    // printf("NodeGizmo::autoAble(%s)\n", enabled ? "True" : "False");
    
    chgOption->setEnabled(enabled);
    busyBytesBtn->setEnabled(enabled);
    busyPacketsBtn->setEnabled(enabled);
 
} // autoAble

// so nettop can tell us a source is locked
void
NodeGizmo::setSrcLocked(int s, tuBool locked) {
    srcChecks[s]->setSelected(locked);
    srcFields[s]->setText(*(nettop->srcInfo[s].displayStringPtr));
}

void
NodeGizmo::setDestLocked(int d, tuBool locked) {
    destChecks[d]->setSelected(locked);
    destFields[d]->setText(*(nettop->destInfo[d].displayStringPtr));
}

// callback for clicking on a src checkbox
// Set it locked, and if it is locked and the name is different
// than what nettop thinks it is, tell nettop the new name, etc.
void
NodeGizmo::clickSrcCheck(tuGadget *g) {
    tuBool locked = g->getSelected();
    int num = atoi(g->getInstanceName());
    
    nettop->setSrcLocked(num, locked);
    if (locked &&
	strcmp(nettop->srcInfo[num].fullname, srcFields[num]->getText())) {
	setInfoName(&nettop->srcInfo[num], srcFields[num]->getText());
	nettop->setLabelType(nettop->getLabelType());
	nettop->restartSnooping();
    }   
}

void
NodeGizmo::clickDestCheck(tuGadget *g) {
    tuBool locked = g->getSelected();
    int num = atoi(g->getInstanceName());
    
    nettop->setDestLocked(num, locked);
    if (locked &&
	strcmp(nettop->destInfo[num].fullname, destFields[num]->getText())) {
	setInfoName(&nettop->destInfo[num], destFields[num]->getText());
	nettop->setLabelType(nettop->getLabelType());
	nettop->restartSnooping();
    }   
}


void
NodeGizmo::setInfoName(nodeInfo* info, const char* name) {
    if (info->fullname)
	delete [] info->fullname;
    if (info->physaddr)
	delete [] info->physaddr;
    if (info->ipaddr)
	delete [] info->ipaddr;
    info->fullname = strdup(name);
    info->physaddr = NULL;
    info->ipaddr = NULL;
} // setInfoName


// callback for termination of typing in a source name field (srcFields[])
void
NodeGizmo::getSrcName(tuGadget *gadget) {
    tuTextField* field = (tuTextField*) gadget;
    // need to compare it to all srcField pointers to see which src it is
    for (int i = 0; i < MAX_NODES; i++)
	if (field == srcFields[i])
	    break;

    if (i == MAX_NODES)
	return;
	
    tuTextField* nextField;
    if (i < MAX_NODES-1) {
	nextField = srcFields[i+1];
    } else {
	nextField = destFields[2]; // xxx '2' to skip 'total' & 'other'
    }
    requestFocus(nextField, tuFocus_explicit);
    nextField->movePointToEnd(False);

    if ((whichAxes == AXES_SRCS_DESTS &&
	  (whichSrcs  == NODES_SPECIFIC || nettop->getSrcLocked(i))) ||
	(whichAxes == AXES_BUSY_PAIRS &&
	  nettop->getSrcLocked(i)) ||
	(whichAxes == AXES_NODES_FILTERS &&
	  (whichNodes == NODES_SPECIFIC || nettop->getSrcLocked(i)))) {
	
	setInfoName(&nettop->srcInfo[i], field->getText());
	nettop->setLabelType(nettop->getLabelType());
	nettop->restartSnooping();
	if (i < numSrcs)
	    nettop->restartTopSnooping();
    }
} // getSrcName

void
NodeGizmo::getDestName(tuGadget *gadget) {
    tuTextField* field = (tuTextField*) gadget;
    for (int i = 0; i < MAX_NODES; i++)
	if (field == destFields[i])
	    break;

    if (i == MAX_NODES)
	return;

    tuTextField* nextField;
    if (i < MAX_NODES-1) {
	nextField = destFields[i+1];
    } else {
	nextField = srcFields[2]; // xxx '2' to skip 'total' & 'other'
    }
    requestFocus(nextField, tuFocus_explicit);
    nextField->movePointToEnd(False);

    if ((whichAxes == AXES_SRCS_DESTS &&
	  (whichDests == NODES_SPECIFIC || nettop->getDestLocked(i))) ||
	(whichAxes == AXES_BUSY_PAIRS &&
	  nettop->getDestLocked(i)) ||
	(whichAxes == AXES_NODES_FILTERS)) {

	setInfoName(&nettop->destInfo[i], field->getText());
	nettop->setLabelType(nettop->getLabelType());
	nettop->restartSnooping();
	if (i < numDests)
	    nettop->restartTopSnooping();
    }
} // getDestName


void
NodeGizmo::grabSrcs(tuGadget *) {
    for (int i = 0; i < nettop->getNumSrcs(); i++) {
	if (nettop->srcInfo[i].fullname)
	    srcFields[i]->setText(nettop->srcInfo[i].fullname);
	else if (nettop->srcInfo[i].ipaddr[0])
	    srcFields[i]->setText(nettop->srcInfo[i].ipaddr);
	else if (nettop->srcInfo[i].physaddr[0])
	    srcFields[i]->setText(nettop->srcInfo[i].physaddr);
	else
	    srcFields[i]->setText("");
	srcFields[i]->movePointToHome(False);
    }
} // grabSrcs

void
NodeGizmo::grabDests(tuGadget *) {
    for (int i = 0; i < nettop->getNumDests(); i++) {
	if (nettop->destInfo[i].fullname)
	    destFields[i]->setText(nettop->destInfo[i].fullname);
	else if (nettop->destInfo[i].ipaddr[0])
	    destFields[i]->setText(nettop->destInfo[i].ipaddr);
	else if (nettop->destInfo[i].physaddr[0])
	    destFields[i]->setText(nettop->destInfo[i].physaddr);
	else
	    destFields[i]->setText("");
	destFields[i]->movePointToHome(False);
    }
} // grapDests

// this is used to choose between full domain name, local host name,
// ip addr, decnet addr, phys addr, phys addr + vendor, or filter.
// FOR FIRST RELEASE: just name ("full"), addr ("ip"), filter.
// (Note that even tho it says "ip", it will show the physaddr
// if there's no ipaddr.)
void
NodeGizmo::howLabel(tuGadget *g)
{
    const char *s = g->getInstanceName();
    
    for (int t = 1; t <= NUM_LABEL_TYPES; t++) {
	if (!strcmp(s, labelTypes[t])) {
	    labelType = t;
	    nettop->setLabelType(labelType);
	    break;
	}
    } // find selected type

} // howLabel



void
NodeGizmo::busyMeans(tuGadget *g) {
    const char *s = g->getInstanceName();
    busyBytes = strcmp(s, "busyPacketsBtn");
    nettop->setBusyBytes(busyBytes);
} // busyMeans

void
NodeGizmo::netfilters(tuGadget *) {
    // if focus not already set, set it to first destField, so
    // what we get back from netfilters has somewhere to go.
    if (getFocusedGadget() == NULL) {
	requestFocus(destFields[2], tuFocus_explicit);
	destFields[2]->movePointToHome(False);
    }

    if (nettop->openNetFilters() == 0) {
	nettop->openDialogBox();
	// printf("NodeGizmo::netfilters - openNetFilters failed\n");
    }
} // netfilters



// callbacks for dials & options

void
NodeGizmo::pairOpt(tuGadget* g) {
    numPairs = atoi(g->getText());
    nettop->setNumPairs(numPairs);
} // pairOpt

void
NodeGizmo::srcOpt(tuGadget* g) {
    numSrcs = atoi(g->getText());
    nettop->setNumSrcs(numSrcs);
} // srcOpt

void
NodeGizmo::destOpt(tuGadget* g) {
    numDests = atoi(g->getText());
    nettop->setNumDests(numDests);
} // destOpt

void
NodeGizmo::nodeOpt(tuGadget* g) {
    numNodes = atoi(g->getText());
    nettop->setNumNodes(numNodes);
} // nodeOpt

void
NodeGizmo::filterOpt(tuGadget* g) {
    numFilters = atoi(g->getText());
    nettop->setNumFilters(numFilters);
} // filterOpt


void
NodeGizmo::chgOpt(tuGadget*g) {
    float chgF = atof(g->getText());
    nettop->setNodeUpdateTime((int)(chgF * 10)); 
} // chgOpt


void
NodeGizmo::processEvent(XEvent *ev) {
    if (ev->type == LeaveNotify) {
	nettop->setLastGizmoFocused(this);
	// printf("leaving NodeGizmo\n");
	/****
	if (lastFocused != getFocusedGadget()) {
	    // printf("FOCUS changed\n");
	    lastFocused = getFocusedGadget();
	    nettop->setLastGizmoFocused(this);
	}
	****/
    }
    
    tuTopLevel::processEvent(ev);    
} // processEvent

