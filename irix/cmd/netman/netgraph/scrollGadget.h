#ifndef __scrollGadget_h_
#define __scrollGadget_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Scroll Gadget (for manually controlling history)
 *
 *	$Revision: 1.3 $
 *	$Date: 1992/10/23 06:20:12 $
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
#include <tuLabel.h>
#include <tuRowColumn.h>

class tuSlider;

class NetGraph;

#define SCROLL_Y_SIZE	    90
     // vertical size of scrollgadget

class ScrollGadget : public tuRowColumn {
public:
    ScrollGadget(NetGraph *ng, tuGadget *parent, const char* instanceName);
    
    void    getLayoutHints(tuLayoutHints *);

    void    setScrollPerc(float);
    void    setScrollPercShown(float);

private:
    NetGraph*	netgraph;
    tuGadget*	ui;
    tuScrollBar*    scroller;
    
    float	speed;
    
    void	dragCB(tuGadget*);
    void	incCB(tuGadget*);
    void	decCB(tuGadget*);
    void	pageIncCB(tuGadget*);
    void	pageDecCB(tuGadget*);

    void	stopCB(tuGadget*);

    void	fastbackCB(tuGadget*);
    void	backCB(tuGadget*);
    void	forwardCB(tuGadget*);
    void	fastforwardCB(tuGadget*);

    /***
    void	leftCB(tuGadget*);
    void	rightCB(tuGadget*);
    ***/

};

#endif /* __scrollGadget_h_ */
