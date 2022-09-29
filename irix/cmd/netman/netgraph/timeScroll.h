#ifndef __timeScroll_h_
#define __timeScroll_h_

/*
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Time Scroller (for time strip)
 *
 *	$Revision: 1.2 $
 *	$Date: 1992/09/27 21:29:07 $
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

#include <time.h>
#include <sys/time.h>

#include <tuGadget.h>
#include <tuGLGadget.h>

#include "boxes.h"

class TimeGadget;

#define MAX_SCR_LBLS    22
     // max number of scrolling time labels



class TimeScroll : public tuGLGadget {
public:
    TimeScroll(NetGraph *ng, TimeGadget *tg, tuGadget *parent,
			const char* instanceName, int per, int intvl);
    
    void    bindResources(tuResourceDB * = NULL, tuResourcePath * = NULL);
    void    getLayoutHints(tuLayoutHints *);
    void    resize();
    void    resize(int w, int h);
    void    render(void);

    void    newTimeArgs(int, int);
    void    updateTime(struct timeval);
    void    calcLabelDelta();
    void    doScrollingLabels(struct timeval);

private:
    NetGraph*	netgraph;
    TimeGadget*	timegadget;

    fBox	bounds;		// bounds of that whole rectangle
    fBox	timeBounds;	// bounds of the part where time is
    fBox	scrollBounds;	// bounds of scrolling part
    int		graphScaleX;
    int		graphScaleY;

    short	bgColor;
    short	scrollColor;
 
    tuLabel*	scrTimeLabel[MAX_SCR_LBLS];
    char	scrTimeText[MAX_SCR_LBLS][10];
    float	scrTimeX[MAX_SCR_LBLS]; // x position of start of text

    int		periodSec;	// time span of whole thing, in seconds
    int		intervalSec;	// time between samples, in seconds
    float	timeScale;	// horizontal pixels per second
    int		numTicks;	// how many ticks between scr lbls
    float	tickSpacing;	// how many pixels between ticks
    int		numScrLabels;	// how many scrolling time labels
    int		previousNumScrLabels; // previous value
    int		labelDeltaTime;	// seconds between scrolling labels
    float	labelSpacing;	// how many pixels between labels
    time_t	previousLabel1Clock;  // used for scrolling time labels
    
};

#endif /* __timeScroll_h_ */

