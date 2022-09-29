#ifndef __viewGadget_h_
#define __viewGadget_h_

/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Viewing Gadget
 *
 *	$Revision: 1.6 $
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

#include <tuGadget.h>
#include <tuScrollBar.h>
#include <tuTopLevel.h>

#include "graphGadget.h"

class NetTop;
class tuAccelerator;

class ViewGadget : public tuTopLevel {
public:
	ViewGadget(NetTop *nt, const char* instanceName,
	   tuColorMap* cmap, tuResourceDB* db,
           char* progName, char* progClass,
	   Window transientFor = NULL, const char* appGeometry = NULL);

	//~ViewGadget(void);
	
	void getVals()		{ graph->getVals(); }
	void renderGraph()	{ graph->render(); }
	void figureScale()	{ graph->figureScale((tuGadget*)0); }
	void setSrcAndDest(tuBool b)	{ graph->setSrcAndDest(b); }
	void setNumSrcs(int s)	{ graph->setNumSrcs(s); }
	void setNumDests(int d)	{ graph->setNumDests(d); }
	int  getNumSrcs()	{ return graph->getNumSrcs(); }
	int  getNumDests()	{ return graph->getNumDests(); }
	
	void setHowDraw(int t)	    { graph->setHowDraw(t); }
	void setScale(int s)	    { graph->setScale(s); }
	int  getHowDraw()	    { return graph->getHowDraw(); }
	int  getScale()		    { return graph->getScale(); }
	int  getRescaleType()	    { return graph->getRescaleType(); }
	void setRescaleType(int t)  { graph->setRescaleType(t); }
	
	void setScaleUpdateTime(int t)	{ graph->setScaleUpdateTime(t); }
	void setShowTotals(tuBool s)	{ graph->setShowTotals(s); }
	void setLighting(tuBool l)	{ graph->setLighting(l); }
	void forceColorMap()		{ graph->forceColorMap(); }

	void setInclinationScrollBar(int i)
			    { inclinScrollBar->setPosition((float)i); }
	void setAzimuthScrollBar(int a)
			    { azimScrollBar->setPosition((float)a); }
	void setDistanceScrollBar(int d)
			    { zoomScrollBar->setPosition((float)d); }

private:
	const char  *name;
	tuGadget    *view;
	GraphGadget *graph;
	
	NetTop      *nettop;
	tuAccelerator *homeAcc;
		
	tuScrollBar *inclinScrollBar, *azimScrollBar, *zoomScrollBar;
	
	void home(tuGadget*);
	
	void initialize(NetTop* nt, const char* instanceName);
	static void quit(tuGadget*, void*);
	void inclinDrag(tuGadget*);
	void inclinInc(tuGadget*);
	void inclinPageInc(tuGadget*);
	void inclinDec(tuGadget*);
	void inclinPageDec(tuGadget*);
	
	void azimDrag(tuGadget*);
	void azimInc(tuGadget*);
	void azimPageInc(tuGadget*);
	void azimDec(tuGadget*);
	void azimPageDec(tuGadget*);
	
	void zoomDrag(tuGadget*);
	void zoomInc(tuGadget*);
	void zoomPageInc(tuGadget*);
	void zoomDec(tuGadget*);
	void zoomPageDec(tuGadget*);
};

#endif /* __viewGadget_h_ */
