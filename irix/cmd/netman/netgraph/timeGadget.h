#ifndef __timeGadget_h_
#define __timeGadget_h_

/*
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Time Gadget (for time strip)
 *
 *	$Revision: 1.3 $
 *	$Date: 1992/10/23 06:20:19 $
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

#include <tuRowColumn.h>

class tuDeck;
class tuGadget;
class tuLabel;

class NetGraph;
class TimeScroll;

#define TIME_NUM	    -2
#define TIME_Y_SIZE	    100
     // vertical size of time view.
#define MAX_SCR_LBLS    22
     // max number of scrolling time labels



class TimeGadget : public tuRowColumn {
public:
    TimeGadget(NetGraph *ng, tuGadget *parent, const char* instanceName, 
			int per, int intvl, int newType);
    void    getLayoutHints(tuLayoutHints *);
    void    map(tuBool propagate = True);
    
    void    newTimeArgs(int, int, int);
    void    setTimeType(int);
    int	    getTimeType()			{ return timeType; }
    void    setUnitString(char* label)		{ relUnitLabel->setText(label); }
    void    updateTime(struct timeval);

private:
    NetGraph*	netgraph;
    TimeScroll*	scroll;
    tuGadget*	ui;
    tuGadget*	GLparent;
    tuDeck*	deck;
    tuGadget*	absChild;	    // deck children for various time types
    tuGadget*	relChild;
    tuGadget*	scrollChild;
    
    tuLabel*	relLeftTimeLabel;   // for relative time
    tuLabel*	relMidTimeLabel;
    tuLabel*	relUnitLabel;

    tuLabel*	leftTimeLabel;	    // for absolute time
    tuLabel*	rightTimeLabel;
    tuLabel*	leftDateLabel;
    tuLabel*	rightDateLabel;
    
	
    int		periodSec;	// time span of whole thing, in seconds
    int		intervalSec;	// time between samples, in seconds
    float	timeScale;	// horizontal pixels per second
    int		timeType;	// absolute, relative, scrolling, none
    int		numTicks;	// how many ticks between scr lbls
    float	tickSpacing;	// how many pixels between ticks
    int		labelDeltaTime;	// seconds between scrolling labels
    float	labelSpacing;	// how many pixels between labels
    int		previousMinutes;// used for absolute time labels only
    int		previousSeconds;
    time_t	previousLabel1Clock;  // used for scrolling time labels
    
    void    setScrollingTime();
    void    setAbsoluteTime();
    void    setRelativeTime();
    void    setNoTime();
 
};

#endif /* __timeGadget_h_ */

