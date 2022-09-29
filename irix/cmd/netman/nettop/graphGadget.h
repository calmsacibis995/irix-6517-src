#ifndef __graphGadget_h_
#define __graphGadget_h_

/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Graph Gadget
 *
 *	$Revision: 1.6 $
 *	$Date: 1992/10/22 21:12:44 $
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

#include "constants.h"

class tuTimer;
class NetTop;

class GraphGadget : public tuGLGadget {
public:
	GraphGadget(NetTop *, tuGadget *, const char *);

	virtual void bindResources(tuResourceDB * = NULL, tuResourcePath * = NULL);
	virtual void render();
	virtual void getLayoutHints(tuLayoutHints *);
	virtual void processEvent(XEvent *);

	void	getVals();
	void	setDistance(int d, tuBool tellView = False);
	void	setDistance(float d)	    { distance = (int)d; }
	void	setAzimuth(int a, tuBool tellView = False);
	void	setAzimuth(float a)	    { setAzimuth((int) a); }
	void	setInclination(int i, tuBool tellView = False);
	void	setInclination(float i)	    { inclination = (int)i; }
	void	setSrcAndDest(tuBool b)	    { doSrcAndDest = b; }
	void	setNumSrcs(int sources);
	void	setNumDests(int dests);
	int	getNumSrcs()		    { return numSrcs; }
	int	getNumDests()		    { return numDests; }
	
	void	setHowDraw(int t);
	int	getHowDraw()		    { return howDraw; }
	void	setShowTotals(tuBool s);
	void	setLighting(tuBool l);
	void	forceColorMap();
	
	void	setScaleUpdateTime(int t);
	void	setScale(int s);
	int	getScale()		    { return scaleMax; }
	int	getRescaleType()	    { return rescaleType; }
	void	setRescaleType(int t)	    { rescaleType = t; }
	void	figureScale(tuGadget*);
	
private:
	static float u[8][3]; // coordinates of a unit box, centered at origin
	float t[MAX_NODES][MAX_NODES][3]; // coords of center of top of tower
	int	colors[MAX_NODES][MAX_NODES];
	int	rgbColors[MAX_NODES][MAX_NODES];
	
	NetTop* nettop;
	tuTimer	*scaleTimer;

	tuBool	showTotals;	// True = display 'total' towers
	tuBool	doSrcAndDest;	// T = src & dest; F = node & filter
	tuBool	useLighting;	// True = use gl lighting; False = flat shading
	tuBool	picking;	// True = in picking mode
	tuBool	zbuf;		// True = hardware zbuf available
	tuBool	doRGB;		// True = rgb mode; False = colormap
	long	zmax;		// zdepth, from getgdesc
	int	distance;	// for polarview
	int	azimuth;
	int	inclination;
	int	numSrcs;
	int	numDests;
	int	howDraw;
	int	quadrant;   // where eye is (azimuth)
			    // 0 = 0 <= a <= 90; 1 = 90 < a <= 180;
			    // 2 = -90 <= a < 0; 3 = -180 <= a < -90
			    // note: viewgadget scrollbar looks like this:
			    //   quadrant:   |..3..|..2..|..0..|..1..|
	int	selectedSrc;
	int	selectedDest;
	int	totalSrc;   // src that's really total, not real source
	int	totalDest;
	
	long	mainBackgroundRGBColor;
	long	selectRGBColor;
	long	labelRGBColor;
	long	nameRGBColor;
	long	nameErrorRGBColor;
	long	nameLockedRGBColor;
	long	baseRGBColor;
	long	scaleRGBColor;
	long	titleRGBColor;
	long	edgeRGBColor;

	short	mainBackgroundIndexColor;
	short	selectIndexColor;
	short	labelIndexColor;
	short	nameIndexColor;
	short	nameErrorIndexColor;
	short	nameLockedIndexColor;
	short	baseIndexColor;
	short	scaleIndexColor;
	short	titleIndexColor;
	short	edgeIndexColor;
	short	totalTotalIndexColor;
	short	otherOtherIndexColor;
	short	totalAnyIndexColor;
	short	otherAnyIndexColor;
	short	anyAnyIndexColor;
	
	tuBool	selectDown;
	tuBool	adjustDown;
	tuBool	dragging;
	tuBool	dollying;
	int	dragX, dragY;
	int	dollyX, dollyY;
	
	int	scaleMax;	    // high value on vertical scale
	float	recentDataMax;	    // highest seen lately
	int	rescaleType;
	
	void	startDrag(int, int);
	void	drag(int, int);
	void	stopDrag(int, int);
	void	startDolly(int, int);
	void	dolly(int, int);
	void	stopDolly(int, int);

	void	assignColors();
	void	drawBox(float v[8][3]);
	void	drawTower(int srcIndex, int destIndex, float height);
	void	drawWire();
	void	drawSurface();
	void	drawBaseForPicking();
	void	drawBase();
	void	drawScaleLines(int a, int b, int c);
	void	drawScaleText();
	void	drawScale();
	void	drawText();
	
	void	drawCenterLeft(char*);
	void	drawCenterRight(char*);
	
	void	adjustDistance(int, int);
	void	handlePicks(short buffer[], long hits, tuBool doubleClick);
};

#endif /* __graphGadget_h_ */
