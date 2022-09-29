#ifndef __nodeGizmo_h
#define __nodeGizmo_h

/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Node Gizmo (for controlling node-related parameters)
 *
 *	$Revision: 1.5 $
 *	$Date: 1992/10/13 19:42:03 $
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

#include <tuTopLevel.h>

#include "constants.h"
#include "netTop.h"

class tuCheckBox;
class tuDeck;
class tuDial;
class tuGadget;
class tuLabel;
class tuOption;
class tuTextField;
//class tuScrollBar;

class NodeGizmo : public tuTopLevel {
public:

	NodeGizmo::NodeGizmo(NetTop *nt, const char* instanceName,
	  tuTopLevel* othertoplevel, const char* appGeometry);
	
	// these are public so nettop can call them w/ filter from netfilters
	void getSrcName(tuGadget *);
	void getDestName(tuGadget *);
	
	void setSrcLocked(int s, tuBool locked);
	void setDestLocked(int d,  tuBool locked);
	
private:
	const char *name;
	tuGadget *ui;
	NetTop *nettop;
	tuDeck *bigDeck;
	tuDeck *numSrcDeck, *numDestDeck;
	tuDeck *showSrcDeck, *showDestDeck;
	tuGadget *numSrcLbl, *numNodeLbl, *numDestLbl, *numFiltLbl;
	tuGadget *showSrcLbl, *showNodeLbl, *showDestLbl, *showFiltLbl;
	tuOption *pairOption, *srcOption, *destOption;
	tuOption *nodeOption, *filterOption;
	tuGadget *srcsRC, *destsRC;
	tuCheckBox*  srcChecks[MAX_NODES];
	tuCheckBox*  destChecks[MAX_NODES];
	tuTextField* srcFields[MAX_NODES];
	tuTextField* destFields[MAX_NODES];
	tuOption* chgOption;
	tuGadget *busyBytesBtn, *busyPacketsBtn;
	
	int labelType;	// label type: ip, local, full, decnet, etc
	int whichAxes;  // srcs&dests, pairs, nodes&filters
	int whichSrcs;	// display top srcs, specific srcs
	int whichDests; 
	int whichNodes;
	
	tuBool busyBytes; // busy means most bytes (vs most packets)
	int numPairs;
	int numSrcs, numDests;
	int numNodes, numFilters;
	
	tuGadget *lastFocused;
	
	void initialize(NetTop *nt, const char* instanceName);
	void open();
	void closeIt(tuGadget*);
	
	void howLabel(tuGadget *);
	void whatAxesDisplay(tuGadget *);
	void busyMeans(tuGadget *);
	void netfilters(tuGadget *);

	void whatSrcsDisplay(tuGadget *);
	void whatDestsDisplay(tuGadget *);
	void whatNodesDisplay(tuGadget *);
	void autoAble(tuBool);
	void tellNettopSrcNames();
	void tellNettopDestNames();
	
	void clickSrcCheck(tuGadget *);
	void clickDestCheck(tuGadget *);
	void setInfoName(nodeInfo*, const char*);
	void grabSrcs(tuGadget *);
	void grabDests(tuGadget *);
	
	void pairDialSet(tuGadget*);
	void srcDialSet(tuGadget*);
	void destDialSet(tuGadget*);

	void srcOpt(tuGadget*);
	void destOpt(tuGadget*);
	void nodeOpt(tuGadget*);
	void filterOpt(tuGadget*);
	void pairOpt(tuGadget*);

	void chgOpt(tuGadget*);

	void processEvent(XEvent *);

};

#endif /* __nodeGizmo_h */
