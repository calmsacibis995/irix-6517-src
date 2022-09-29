#ifndef __viewGadget_h_
#define __viewGadget_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Viewing Gadget (main window)
 *
 *	$Revision: 1.2 $
 *	$Date: 1992/09/27 21:29:12 $
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


class NetGraph;
class tuRowColumn;
class ScrollGadget;
class StripGadget;
class TimeGadget;

class ViewGadget : public tuTopLevel {
public:
    ViewGadget(NetGraph *ng, const char* instanceName,
	   tuColorMap* cmap, tuResourceDB* db,
           char* progName, char* progClass,
	   Window transientFor = NULL, const char* appGeometry = NULL);

    void    open();
    void    getLayoutHints(tuLayoutHints *);

    void        newTimeArgs(TimeGadget*, int per, int intvl, int newType);
    void	addTime(TimeGadget*);
    void	addScrollBox(ScrollGadget*);
    void	addAGraph(StripGadget*);
    void	removeAGraph(StripGadget*);
    int		getNumberOfGraphs()	    { return numberOfGraphs; }
    tuRowColumn*	getStripParent();

    tuGadget*	getAddItem();
    tuGadget*	getEditItem();
    tuGadget*	getDeleteItem();

private:
    NetGraph	*netgraph;
    const char  *name;
    tuGadget    *view;
    tuGadget    *stripParent;
    GraphGadget *graph;
    int		numberOfGraphs;
    int		timeHeight;
    int		scrollHeight;
    
    void	initialize(NetGraph* ng, const char* instanceName);
    void	quit(tuGadget*);

};

#endif /* __viewGadget_h_ */
