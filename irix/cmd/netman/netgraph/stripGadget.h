#ifndef __stripGadget_h_
#define __stripGadget_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Strip Gadget (one for each graph)
 *
 *	$Revision: 1.5 $
 *	$Date: 1992/10/16 18:26:19 $
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
#include <tuScrollBar.h>

#include "graphGadget.h"

class NetGraph;
class myLabel;

#define SELECTED_COLOR	"yellow2"
#define ALARMED_COLOR	"tomato"
#define BG_COLOR	"inherited"


typedef struct dataStr {
    float   y;
    float   avg;
    struct  dataStr* next;
} dataStruct;


class StripGadget : public tuRowColumn {
public:
    StripGadget(NetGraph *ng, tuGadget *parent, const char* instanceName);
    
    void    setAlarmBell(tuBool b)	{ alarmBell = b; } 
    tuBool  getAlarmBell()		{ return alarmBell; } 
    void    setAlarmSet(tuBool b);
    tuBool  getAlarmSet()		{ return alarmSet; } 
    void    setAlarmLoVal(float av)	{ alarmLoVal = av; }
    float   getAlarmLoVal()		{ return alarmLoVal; }
    void    setAlarmHiVal(float av)	{ alarmHiVal = av; }
    float   getAlarmHiVal()		{ return alarmHiVal; }
    void    setAlarmLoMet(tuBool);
    void    setAlarmHiMet(tuBool);
    tuBool  getAlarmLoMet()		{ return alarmLoMet; }
    tuBool  getAlarmHiMet()		{ return alarmHiMet; }
    tuBool  getAlarmed()		{ return alarmLoMet || alarmHiMet; }
	
    void    handleSelection();
    void    changeSelected(tuBool sel);

    void    setDataHeight(float newHeight)
				    { graph->setDataHeight(newHeight); }
    fBox*   getDataBounds()	    { return graph->getDataBounds(); }

    void    setHeadData(int n, dataStruct* ds);
    void    setCurrentData(dataStruct* ds)  { currentData = ds; }
    dataStruct*	getHeadData()	    { return headData; }
    dataStruct*	getCurrentData()    { return currentData; }

    void    freeData()		    { headData = NULL; currentData = NULL; }
    void    setNext(StripGadget* s) { next = s; }
    StripGadget* getNext()	    { return next; }

    void    setType(int);
    void    setBin(int b)	    { bin = b; }
    void    setNumber(int n)	    { number = n; }
     
    int	    getType()		    { return type; }
    int	    getBin()		    { return bin; }
    int	    getNumber()		    { return number; }
    char*   getTypeText();
    
    void    shiftBin(int junkedBin)
		{ if (bin > junkedBin) bin--; }
 
    void    setBaseRate(int n);	    // NBytes or NPackets, as appropriate
    int     getBaseRate();	    // NBytes or NPackets, as appropriate
				      
    void    setNBytes(int n)	    { nBytes = n; }
    void    setNPackets(int n)	    { nPackets = n; }
    int     getNBytes()		    { return nBytes; }
    int     getNPackets()	    { return nPackets; }
    
    void    setFilter(char* s)	    { filterLabel->setText(s); }
    void    setScale(char* s)	    { scaleLabel->setText(s); }
    void    setCurVal(float cv);
    dataStruct*	getAvgData()	    {return avgData; }
    void    setAvgData(dataStruct* p) {avgData = p; }
    float   getAvgVal()		    {return avgVal; }
    void    setAvgVal(float v)	    {avgVal = v; }
    int	    getSamplesSoFar()	    {return samplesSoFar; }
    void    setSamplesSoFar(int newVal)
	    	    		    {samplesSoFar = newVal; }
    void    zeroMaxVal();
    const char* getFilter()	    { return filterLabel->getText(); }

    void    setStyle(int style)	    { graph->setStyle(style); }
    void    setColor(int c)	    { graph->setColor(c); }
    void    setAvgColor(int c)	    { graph->setAvgColor(c); }
    int	    getStyle()		    { return graph->getStyle(); }
    int	    getColor()		    { return graph->getColor(); }
    int	    getAvgColor()	    { return graph->getAvgColor(); }
    
    void    setScaleType(int s)	    { graph->setScaleType(s); }
    void    setNumPoints(int n)	    { graph->setNumPoints(n); }
    int	    findVertScale(float minScale)
				{ return graph->findVertScale(minScale); }
    void    rescale(tuBool okToShrink = True)
				{ graph->rescale(okToShrink); }
    void    render(void);

private:
    NetGraph*	    netgraph;
    GraphGadget*    graph;
    tuGadget*	    ui;
    StripGadget*    next;

    tuGadget*	    stripRC;
    tuLabel*	    filterLabel;
    tuLabel*	    typeLabel;
    tuLabel*	    scaleLabel;
    myLabel*	    maxValLabel;
    myLabel*	    curValLabel;
    char	    curValText[21];
    char	    maxValText[21];
    
    char*	    bgColor;

    tuBool	    alarmSet;   // alarms are active
    tuBool	    alarmBell;  // ring bell when get alarm
    tuBool	    alarmLoMet; // lo alarm condition was met
    tuBool	    alarmHiMet; // hi alarm condition was met
    float	    alarmLoVal; // ring if go below
    float	    alarmHiVal; // ring if go above

    dataStruct*	    headData;   // head of whole data memory area
    dataStruct*	    currentData;// current location in data area
    dataStruct*	    avgData;	// start of averaged data
    float	    maxVal;
    float	    avgVal;	// current average
    int		    samplesSoFar;   // used for calculating average

    int		    type;
    int		    bin;
    int		    number;	// which strip this is.
    int		    nBytes;
    int		    nPackets;
    fBox*	    dataBounds;

    void	    changeBackground(void);
    void	    dispatchEvent(XEvent *);
    void	    processEvent(XEvent *);
    u_long	    getEventMask(void);
};

#endif /* __stripGadget_h_ */
