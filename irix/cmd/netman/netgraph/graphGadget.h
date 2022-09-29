#ifndef __graphGadget_h_
#define __graphGadget_h_

/*
 * Copyright 1991, 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Graph Gadget (GL stuff for each graph)
 *
 *	$Revision: 1.4 $
 *	$Date: 1992/09/30 21:24:39 $
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

#include <tuGLGadget.h>
#include <tuGadget.h>
#include <tuTopLevel.h>

#include "boxes.h"

class NetGraph;
class StripGadget;


class GraphGadget : public tuGLGadget {
public:
    GraphGadget(NetGraph *ng, StripGadget *s,
		tuGadget *parent, const char* instanceName);
    
    void    bindResources(tuResourceDB * = NULL, tuResourcePath * = NULL);
    void    render();
    void    getLayoutHints(tuLayoutHints *);
    void    processEvent(XEvent *);

    void    setStyle(int style)	    { graphStyle = style; render(); }
    void    setColor(int c)	    { colorIndex = c; render(); }
    void    setAvgColor(int c)	    { avgColorIndex = c; render(); }

    int	    getStyle()		    { return graphStyle; }
    int	    getColor()		    { return colorIndex; }
    int	    getAvgColor()	    { return avgColorIndex; }

    void    setScaleType(tuBool type);

    void    setDataHeight(float newHeight);
    fBox*   getDataBounds()	    { return dataBounds; }


    void    setNumPoints(int n)	    { numberOfPoints = n; }

    int	    findVertScale(float minScale);
    void    rescale(tuBool okToShrink = True);

private:
    NetGraph*	netgraph;
    StripGadget* strip;
    
    iBox*	graphBounds;
    fBox*	dataBounds;
    int		numberOfPoints;
    float	graphScaleX;
    float	graphScaleY;
    unsigned short  colorIndex;
    unsigned short  avgColorIndex;
    
    short	bgColor;
    short	scaleColor;
    
    tuBool	scaleType;
    int		graphStyle;



};

#endif /* __graphGadget_h_ */
