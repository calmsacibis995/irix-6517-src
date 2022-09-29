/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetTop Graph Gadget
 *
 *	$Revision: 1.13 $
 *	$Date: 1996/02/26 01:27:57 $
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

#include <tuCallBack.h>
#include <tuStyle.h>
#include <tuTimer.h>
#include <tuWindow.h>

#include <string.h>

#include "constants.h"
#include "font.h"
#include "graphGadget.h"
#include "netTop.h"

#include <gl/gl.h>
#include <gl/glws.h>
#include <stdio.h>

#define SCALE_WALL_COLOR    4   /* blue, for the scale walls */
#define SCALE_WALL_RGB	0xFF0000
#define WIRE_COLOR	1	/* red for drawing as wire frame */
#define WIRE_RGB	0x0000FF


#define BASE_HEIGHT    6    /* how thick base is */
#define TOWER_WIDTH    8    /* how wide towers are */
#define TOWER_GAP      1    /* space on each side of each tower, in its "plot of land" */
#define TOWER_SPACING 10    /* size of "plots" (== width + gap) */
#define TOWER_PORTION  0.8  /* what portion of the spacing is taken up by tower */
#define SCALE_HEIGHT  50.0  /* how high scale walls are */
#define PICK_BUF_SIZE 50    /* name stack buffer for picking */

#define MIN_INCLINATION	    0
#define MAX_INCLINATION	   900
#define MIN_AZIMUTH	  -450
#define MAX_AZIMUTH	  3150
#define MIN_DISTANCE	    60
#define MAX_DISTANCE	   600

#define TITLE_LENGTH	    80

// unit box, centered at origin in x,y; based at origin in z
float GraphGadget::u[8][3] = {
    -0.5, -0.5, 0.0,
     0.5, -0.5, 0.0,
     0.5,  0.5, 0.0,
    -0.5,  0.5, 0.0,
    -0.5, -0.5, 1.0,
     0.5, -0.5, 1.0,
     0.5,  0.5, 1.0,
    -0.5,  0.5, 1.0
};




tuDeclareCallBackClass(GraphGadgetCallBack, GraphGadget);
tuImplementCallBackClass(GraphGadgetCallBack, GraphGadget);

GraphGadget::GraphGadget(NetTop *nt, tuGadget *parent,
			     const char *instanceName)
		: tuGLGadget(parent, instanceName) {
    // Save pointer to nettop
    nettop = nt;

    distance = 99; // doesn't really matter; viewGadget has INIT val
    azimuth = 0;   // ditto
    inclination = 450; // ditto
    howDraw = DRAW_SQUARE;
    showTotals = True;
    useLighting = False;
    picking = False;
    recentDataMax = 0.0;
    numSrcs = 5; // initial distance looks good for 5 srcs, 5 dests
    numDests = 5;

    selectedSrc = -1;
    selectedDest = -1;

    scaleTimer = new tuTimer(True);
    scaleTimer->setCallBack(new GraphGadgetCallBack(this, &GraphGadget::figureScale));

    // xxx careful with getgdesc -- it shows for machine running on,
    // which might be different than the display. (but maybe it works fine)
    doRGB = (getgdesc(GD_BITS_NORM_DBL_RED) > 0);

// xxx aaah - maybe skip using zbuffer altogether, and do it all by hand?
    zbuf = False;
#ifdef USE_ZBUF
    zmax = getgdesc(GD_ZMAX);
    zbuf = (getgdesc(GD_BITS_NORM_ZBUFFER) > 0);

if (zbuf) {
    zbuf = False;
    printf("zbuffer available,  but I'm not going to use it\n");
} else 
  printf("no zbuffer available\n");
#endif // USE_ZBUF

} // graphGadget

// ask nettop for current values
void
GraphGadget::getVals() {
    rescaleType = nettop->getRescaleType();
    int srcs = nettop->getNumSrcs();
    int dests = nettop->getNumDests();
    
    adjustDistance(srcs, dests);
    numSrcs = srcs;
    numDests = dests;

    totalSrc = nettop->getTotalSrc();
    totalDest = nettop->getTotalDest();
    doSrcAndDest = nettop->getSrcAndDest();

    // initialize "t" array to centers of bottom of towers.
    // we'll use this to place towers, as well as to draw surface
    for (int s = 0; s < MAX_NODES; s++)
	for (int d = 0; d < MAX_NODES; d++) {
	    t[s][d][0] = (d + 0.5 - numDests * 0.5) * TOWER_SPACING;
	    t[s][d][1] = (s + 0.5 - numSrcs * 0.5) * TOWER_SPACING;
	    t[s][d][2] = 0.0;
	    
	}


    // initialize colors from nettop resources
    if (doRGB) {
	nettop->getRGBColors(
	    &mainBackgroundRGBColor, 
	    &selectRGBColor, 
	    &labelRGBColor, 
	    &nameRGBColor, 
	    &nameErrorRGBColor, 
	    &nameLockedRGBColor, 
	    &baseRGBColor, 
	    &scaleRGBColor, 
	    &titleRGBColor, 
	    &edgeRGBColor);
    } else {
	nettop->getIndexColors(
	    &mainBackgroundIndexColor, 
	    &selectIndexColor, 
	    &labelIndexColor, 
	    &nameIndexColor, 
	    &nameErrorIndexColor, 
	    &nameLockedIndexColor, 
	    &baseIndexColor, 
	    &scaleIndexColor, 
	    &titleIndexColor, 
	    &edgeIndexColor, 
	    &totalTotalIndexColor, 
	    &otherOtherIndexColor, 
	    &totalAnyIndexColor, 
	    &otherAnyIndexColor, 
	    &anyAnyIndexColor);
    } // not doRGB
    
    
    // I don't know what we'll do for colors ultimately; this is for show xxx
    assignColors();

}

void
GraphGadget::bindResources(tuResourceDB *rdb, tuResourcePath *rp) {
    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(True);
    setRGBmode(doRGB);
    if (zbuf)
	setZBuffer(True);
}

void
GraphGadget::forceColorMap() {
    doRGB = False;
}

// assign the colors of the towers
void
GraphGadget::assignColors() {

    if (doRGB) {
	// 0xAABBGGRR (alpha, blue, green, red)
	// (red = 10 * row, green = 10 * column, blue = 100)
	for (int i = 0; i < MAX_NODES; i++)
	    for (int j = 0; j < MAX_NODES; j++)
		rgbColors[i][j] =  i * 20 + j * 5120 + 6553600;
    } else {
	colors[0][0] = totalTotalIndexColor;
	for (int i = 1; i < MAX_NODES; i++) {
	    colors[0][i] = totalAnyIndexColor;
	    colors[i][0] = totalAnyIndexColor;
	    colors[1][i] = otherAnyIndexColor;
	    colors[i][1] = otherAnyIndexColor;
	    for (int j = 2; j < MAX_NODES; j++)
		colors[i][j] = anyAnyIndexColor;
	}
	colors[1][1] = otherOtherIndexColor;    
    }
} // assignColors


// draw a 3D box, given coords of all corners.
void
GraphGadget::drawBox(float v[8][3]) {
    // bottom
    bgnpolygon();
	v3f(v[0]); v3f(v[3]); v3f(v[2]); v3f(v[1]);
    endpolygon();
    // left
    bgnpolygon();
	v3f(v[0]); v3f(v[4]); v3f(v[7]); v3f(v[3]);
    endpolygon();
    // front
    bgnpolygon();
	v3f(v[0]); v3f(v[1]); v3f(v[5]); v3f(v[4]);
    endpolygon();
    // right
    bgnpolygon();
	v3f(v[1]); v3f(v[2]); v3f(v[6]); v3f(v[5]);
    endpolygon();
    // back
    bgnpolygon();
	v3f(v[2]); v3f(v[3]); v3f(v[7]); v3f(v[6]);
    endpolygon();
    // top
    bgnpolygon();
	v3f(v[4]); v3f(v[5]); v3f(v[6]); v3f(v[7]);
    endpolygon();
    
    // edges
    if (doRGB)
	cpack(edgeRGBColor);
    else
	color(edgeIndexColor);
	
    // top
    bgnclosedline();  v3f(v[4]); v3f(v[5]); v3f(v[6]); v3f(v[7]);  endclosedline();


    if (zbuf) {
	// xxx to make edges all look good, make them bigger,
	// or else draw only visible ones & force draw w/ zfunction(ZF_ALWAYS)
	bgnline();  v3f(v[0]); v3f(v[4]);  endline();
	bgnline();  v3f(v[1]); v3f(v[5]);  endline();
	bgnline();  v3f(v[2]); v3f(v[6]);  endline();
	bgnline();  v3f(v[3]); v3f(v[7]);  endline();
    } else {
	// If don't have zbuffer, then have to draw only the visible edges
	// front left
	if (quadrant != 1) {
	    bgnline();  v3f(v[0]); v3f(v[4]);  endline();
	}
	// front right
	if (quadrant != 3) {
	    bgnline();  v3f(v[1]); v3f(v[5]);  endline();
	}
	// rear right
	if (quadrant != 2) {
	    bgnline();  v3f(v[2]); v3f(v[6]);  endline();
	}
	// rear left
	if (quadrant != 0) {
	    bgnline();  v3f(v[3]); v3f(v[7]);  endline();
	}
    }

} // drawBox


// draw a 3D box
// srcIndex & destIndex imply its x & y position, based on TOWER_SPACING, etc.
// height is height of top, above zero.
// If height is negative, it's below z=zero, of course, but draw it upside
// down (from height to zero, not from zero to height) so polygons are
// drawn in the right order, etc. Also, that means it's the base, so
// leave out the gaps, and make it wide enough for whole thing.
void
GraphGadget::drawTower(int srcIndex, int destIndex, float height) {
    if (!showTotals && srcIndex == totalSrc && destIndex == totalDest)
	return;
	
    if ((selectedSrc == srcIndex && (selectedDest == -1 || selectedDest == destIndex)) ||
        (selectedSrc == -1 && selectedDest == destIndex))
	if (doRGB)
	    cpack(selectRGBColor);
	else
	    color(selectIndexColor);
    else
	if (doRGB)
	    cpack(rgbColors[srcIndex][destIndex]);
	else
	    color(colors[srcIndex][destIndex]);
	    
    pushmatrix();
	pushname(srcIndex);
	pushname(destIndex);
    
	translate(t[srcIndex][destIndex][0], t[srcIndex][destIndex][1], 0.0);
	scale(TOWER_SPACING * TOWER_PORTION, TOWER_SPACING * TOWER_PORTION, height);

	drawBox(u);

	popname();
	popname();
    popmatrix();
} // drawTower


// draw the nettop data as a wire-frame surface.
// Yes, I know this function isn't as general as the other drawing
// functions here; oh, well.
void
GraphGadget::drawWire() {
    // each point is: center of corresponding tower, z = data value (scaled)
    int s, d;
    if (doRGB)
	cpack(WIRE_RGB);
    else
	color(WIRE_COLOR);

    // fill in data and draw lines for each source
    for (s = 0; s < numSrcs; s++) {
	bgnline();
	for (d = 0; d < numDests; d++) {
	    t[s][d][2] = nettop->data[s][d];
	    v3f(t[s][d]);
	} // each dest
	endline();
    } // each source
    
    // now draw lines for each dest
    for (d = 0; d < numDests; d++) {
	bgnline();
	for (s = 0; s < numSrcs; s++)
	    v3f(t[s][d]);
	endline();
    }
    
} // drawWire

// problem: need normals for each vertex ( n3f() )
// also, should maybe do tmesh, not qmesh, since not flat quads!
// xxx MAYBE should do this in certain order, like drawing towers??
//  (unless zbuf)
// xxx WHAT colors?
//     lighting?
//     gouraud shading?
// draw the nettop data as a surface with quad-strips
// Yes, I know this function isn't as general as the other drawing
// functions here; oh, well.
void
GraphGadget::drawSurface() {
    // each point is: center of corresponding tower, z = data value (scaled)
    int s, d;
//    color(WIRE_COLOR);
    
    // fill in data
    // xxx  this should probably happen only when data changes, not
    // every time render (so can spin more quickly, etc)
    for (s = 0; s < numSrcs; s++)
	for (d = 0; d < numDests; d++)
	    t[s][d][2] = nettop->data[s][d];

    short rgbvec[3];
    // draw quad strips
    for (s = 0; s < numSrcs - 1; s++) {
	bgnqstrip();
	    for (d = 0; d < numDests; d++) {
		rgbvec[0] = (int)t[s+1][d][2];
		rgbvec[1] = 50;
		rgbvec[2] = (int)(t[s+1][d][2] / 2.0);
		c3s(rgbvec);
		v3f(t[s+1][d]);
		rgbvec[0] = (int)t[s][d][2];
		rgbvec[2] = (int)(t[s][d][2] / 2.0);
		c3s(rgbvec);
		v3f(t[s][d]);
	    }
	endqstrip();
    }

} // drawSurface

// draw the 2 visible sides (dependent on the quadrant we're in)
// for picking (draw as several smaller polygons, and handle name stuff)
void
GraphGadget::drawBaseForPicking() {
    int s0, s1, s2, s3; // the indices of the appropriate corners of the 
		    // source side
    int d0, d1, d2, d3; // ... of the destination side
    switch (quadrant) {
	case 0:
	    d0 = 0; d1 = 1; d2 = 5; d3 = 4;
	    s0 = 1; s1 = 2; s2 = 6; s3 = 5;
	    break;
	case 1:
	    d0 = 2; d1 = 3; d2 = 7; d3 = 6;
	    s0 = 1; s1 = 2; s2 = 6; s3 = 5;
	    break;
	case 2:
	    d0 = 0; d1 = 1; d2 = 5; d3 = 4;
	    s0 = 3; s1 = 0; s2 = 4; s3 = 7;
	    break;
	case 3:
	    d0 = 2; d1 = 3; d2 = 7; d3 = 6;
	    s0 = 3; s1 = 0; s2 = 4; s3 = 7;
	    break;	
    } // switch
    
    loadname(1);  // 1 means next is a source number
    pushname(99); // a dummy source number
    
    pushmatrix();
	translate(0.0, 0.0, - BASE_HEIGHT);
	scale((numDests + 0.5) * TOWER_SPACING, TOWER_SPACING, BASE_HEIGHT);
	translate(0.0, -0.5 * (numSrcs - 1),  0.0);
	for (int i = 0; i < numSrcs; i++) {
	    loadname(i);
	    bgnpolygon();
		v3f(u[s0]); v3f(u[s1]); v3f(u[s2]); v3f(u[s3]); 
	    endpolygon();
	    translate(0.0, 1.0, 0.0);
	}
    popmatrix();
    popname();	// get rid of src number
    
    loadname(2);	// 2 means next is a dest number
    pushname(99);	// a dummy dest number
    
    pushmatrix();
	translate(0.0, 0.0, - BASE_HEIGHT);
	scale(TOWER_SPACING, (numSrcs + 0.5) * TOWER_SPACING, BASE_HEIGHT);
	translate(-0.5 * (numDests - 1), 0.0, 0.0);
	for (i = 0; i < numDests; i++) {
	    loadname(i);
	    bgnpolygon();
		v3f(u[d0]); v3f(u[d1]); v3f(u[d2]); v3f(u[d3]); 		    
	    endpolygon();
	    translate(1.0, 0.0, 0.0);
	}
    popmatrix();
    popname();
    
} // drawBaseForPicking


// draw the flat, wide box that is the base for all the towers
void
GraphGadget::drawBase() {

    // if picking, draw separate polys along each edge so can find
    // which row or which column they picked, and don't bother with labels.
    // Otherwise, just draw a box, plus the labels.
    if (picking) {
	drawBaseForPicking();
    } else {
	if (doRGB)
	    cpack(baseRGBColor);
	else
	    color(baseIndexColor);
	    
	pushmatrix();
	    translate(0.0, 0.0, - BASE_HEIGHT);
	    scale((numDests + 0.5) * TOWER_SPACING,
		  (numSrcs  + 0.5) * TOWER_SPACING, BASE_HEIGHT);
	    drawBox(u);
	popmatrix();
    
	// write labels on the sides of the base
	if (doRGB)
	    cpack(labelRGBColor);
	else
	    color(labelIndexColor);
	    
	pushmatrix();
	    if (quadrant == 2 || quadrant == 3)
		rotate(1800, 'z');
	    translate((numDests+0.5)/2.0 * TOWER_SPACING, 0.0, -BASE_HEIGHT/2);
	    scale(0.4, 0.4, 0.4);
	    rotate(900, 'x');
	    rotate(900, 'y');
	    if (doSrcAndDest)
		DrawStringCenterCenter("Sources");
	    else
		DrawStringCenterCenter("Nodes");
	popmatrix();
	pushmatrix();
	    if (quadrant == 1 || quadrant == 3)
		rotate(1800, 'z');
	    
	    translate(0.0, -(numSrcs+0.5)/2.0 * TOWER_SPACING, -BASE_HEIGHT/2);
	    scale(0.4, 0.4, 0.4);
	    rotate(900, 'x');
	    if (doSrcAndDest)
		DrawStringCenterCenter("Destinations");
	    else
		DrawStringCenterCenter("Filters");
	popmatrix();
    } // not picking
    
    
    // now do node names
    pushmatrix();
	if (quadrant == 0 || quadrant == 1)
	    translate( (numDests+1)/2.0 * TOWER_SPACING,
		      -(numSrcs +1)/2.  * TOWER_SPACING, -BASE_HEIGHT);
	else
	    translate(-(numDests+1)/2.0 * TOWER_SPACING,
		      -(numSrcs +1)/2.0 * TOWER_SPACING, -BASE_HEIGHT);
	
	scale(0.4, 0.4, 1.0);
	
	loadname(1);	// 1 means next is a src number
	pushname(99);	// a dummy src number
	for (int i = 0; i < numSrcs; i++) {
	    loadname(i);
	    if (i == selectedSrc)
		if (doRGB)
		    cpack(selectRGBColor);
		else
		    color(selectIndexColor);
	    else if (nettop->srcInfo[i].err)
		if (doRGB)
		    cpack(nameErrorRGBColor);
		else
		    color(nameErrorIndexColor);
	    else if (nettop->srcInfo[i].locked)
		if (doRGB)
		    cpack(nameLockedRGBColor);
		else
		    color(nameLockedIndexColor);
	    else
		if (doRGB)
		    cpack(nameRGBColor);
		else
		    color(nameIndexColor);
	    translate(0.0, 25.0, 0.0);
	    pushmatrix();
		switch (quadrant) {
		    case 0:
			drawCenterLeft(*(nettop->srcInfo[i].displayStringPtr));
			break;
		    case 1:
			rotate(1800, 'z');
			drawCenterRight(*(nettop->srcInfo[i].displayStringPtr));
			break;
		    case 2:
			drawCenterRight(*(nettop->srcInfo[i].displayStringPtr));
			break;
		    case 3:
			rotate(1800, 'z');
			drawCenterLeft(*(nettop->srcInfo[i].displayStringPtr));
			break;
		} // switch
	    popmatrix();
	}
	popname(); // get rid of src number
    popmatrix();
    
    
    pushmatrix();
	if (quadrant == 1 || quadrant == 3)
	    translate(-(numDests+1)/2.0 * TOWER_SPACING,
		       (numSrcs +1)/2.0 * TOWER_SPACING, -BASE_HEIGHT);
	else
	    translate(-(numDests+1)/2.0 * TOWER_SPACING,
		      -(numSrcs +1)/2.0 * TOWER_SPACING, -BASE_HEIGHT);
	
	scale(0.4, 0.4, 1.0);
	
	loadname(2);  // 2 means next is a dest number
	pushname(99); // a dummy dest number
	for (i = 0; i < numDests; i++) {
	    loadname(i);
	    if (i == selectedDest)
		if (doRGB)
		    cpack(selectRGBColor);
		else
		    color(selectIndexColor);
	    else if (nettop->destInfo[i].err)
		if (doRGB)
		    cpack(nameErrorRGBColor);
		else
		    color(nameErrorIndexColor);
	    else if (nettop->destInfo[i].locked)
		if (doRGB)
		    cpack(nameLockedRGBColor);
		else
		    color(nameLockedIndexColor);
	    else
		if (doRGB)
		    cpack(nameRGBColor);
		else
		    color(nameIndexColor);
	    translate(25.0, 0.0, 0.0);
	    pushmatrix();
		switch (quadrant) {
		    case 0:
			rotate(900, 'z');
			drawCenterRight(*(nettop->destInfo[i].displayStringPtr));
			break;
		    case 1:
			rotate(900, 'z');
			drawCenterLeft(*(nettop->destInfo[i].displayStringPtr));
			break;
		    case 2:
			rotate(-900, 'z');
			drawCenterLeft(*(nettop->destInfo[i].displayStringPtr));
			break;
		    case 3:
			rotate(-900, 'z');
			drawCenterRight(*(nettop->destInfo[i].displayStringPtr));
			break;
		} // switch
	    popmatrix();
	}
	popname(); // get rid of dest number
    popmatrix();
	
    
} // drawBase

// normally, draw text.
// if picking, draw filled rectangle where text would be
void
GraphGadget::drawCenterLeft(char* text) {
    if (picking) {
	float w = (float) StringWidth(text);
	float h2 = 0.5 * (float) StringHeight(text);
	float r[4][3];
	r[0][0] = 0.0; r[0][1] = -h2; r[0][2] = 0.0;
	r[1][0] =   w; r[1][1] = -h2; r[1][2] = 0.0;
	r[2][0] =   w; r[2][1] =  h2; r[2][2] = 0.0;
	r[3][0] = 0.0; r[3][1] =  h2; r[3][2] = 0.0;

	bgnpolygon();
	    v3f(r[0]); v3f(r[1]); v3f(r[2]); v3f(r[3]);
	endpolygon();
    } else
	DrawStringCenterLeft(text);
}

void
GraphGadget::drawCenterRight(char* text) {
    if (picking) {
	float w = (float) StringWidth(text);
	float h2 = 0.5 * (float) StringHeight(text);
	float r[4][3];
	r[0][0] =  -w; r[0][1] = -h2; r[0][2] = 0.0;
	r[1][0] = 0.0; r[1][1] = -h2; r[1][2] = 0.0;
	r[2][0] = 0.0; r[2][1] =  h2; r[2][2] = 0.0;
	r[3][0] =  -w; r[3][1] =  h2; r[3][2] = 0.0;

	bgnpolygon();
	    v3f(r[0]); v3f(r[1]); v3f(r[2]); v3f(r[3]);
	endpolygon();
    } else
	DrawStringCenterRight(text);
}


// draw the appropriate number of horizontal scale lines for one side.
// xxx should handle arith vs log scale here.
void
GraphGadget::drawScaleLines(int a, int b, int c) {

    int numDivs = 5; // xxx
    
    bgnline(); v3f(u[a]); v3f(u[a+4]); endline();
    bgnline(); v3f(u[b]); v3f(u[b+4]); endline();
    bgnline(); v3f(u[c]); v3f(u[c+4]); endline();

    for (int i = 0; i <= numDivs; i++) {
	bgnline();
	    v3f(u[a]); v3f(u[b]); v3f(u[c]);
	endline();
	translate(0.0, 0.0, 1.0 / numDivs);
    }

} // drawScaleLines


void
GraphGadget::drawScaleText() {
    char tmpStr[8];
    sprintf(tmpStr, "%d", scaleMax);	
	
    float x, y;
    switch (quadrant) {
	case 0:
	    x = -(numDests + 0.5) / 2.0 * TOWER_SPACING;
	    y = -(numSrcs  + 0.5) / 2.0 * TOWER_SPACING;
	    break;
	case 1:
	    x = -(numDests + 0.5) / 2.0 * TOWER_SPACING;
	    y =  (numSrcs  + 0.5) / 2.0 * TOWER_SPACING;
	    break;
	case 2:
	    x =  (numDests + 0.5) / 2.0 * TOWER_SPACING;
	    y = -(numSrcs  + 0.5) / 2.0 * TOWER_SPACING;
	    break;
	case 3:
	    x =  (numDests + 0.5) / 2.0 * TOWER_SPACING;
	    y =  (numSrcs  + 0.5) / 2.0 * TOWER_SPACING;
	    break;
    } // switch
    
    pushmatrix();
	translate(x, y, SCALE_HEIGHT);
	scale(0.5, 0.5, 0.5);
	if (quadrant == 2 || quadrant == 3)
	    rotate(1800, 'z');
	rotate(900, 'x');
        rotate(900, 'y');
	if (quadrant == 0 || quadrant == 3)
	    DrawStringCenterRight(tmpStr);
	else
	    DrawStringCenterLeft(tmpStr);
    popmatrix();
    
    pushmatrix();
	translate(-x, -y, SCALE_HEIGHT);
	scale(0.5, 0.5, 0.5);
	if (quadrant == 1 || quadrant == 3)
	    rotate(1800, 'z');
	rotate(900, 'x');
	if (quadrant == 1 || quadrant == 2)
	    DrawStringCenterRight(tmpStr);
	else
	    DrawStringCenterLeft(tmpStr);
    popmatrix();
    
} // drawScaleText

void
GraphGadget::drawScale() {
    // xxx also need to deal with both arithmetic and log scale
    pushmatrix();
	scale((numDests + 0.5) * TOWER_SPACING,
	      (numSrcs  + 0.5) * TOWER_SPACING, SCALE_HEIGHT);
	// can go ahead and draw all 4 here,  because 'backface' will take
	// care of the 'front' ones
#ifdef DRAW_WALLS
	if (doRGB)
	    cpack(SCALE_WALL_RGB);
	else
	    color(SCALE_WALL_COLOR);
	loadname(3);

	bgnpolygon();
	    v3f(u[0]); v3f(u[4]); v3f(u[5]); v3f(u[1]);
	endpolygon();
	bgnpolygon();
	    v3f(u[1]); v3f(u[5]); v3f(u[6]); v3f(u[2]);
	endpolygon();
	bgnpolygon();
	    v3f(u[2]); v3f(u[6]); v3f(u[7]); v3f(u[3]);
	endpolygon();
	bgnpolygon();
	    v3f(u[3]); v3f(u[7]); v3f(u[4]); v3f(u[0]);
	endpolygon();	
#endif
	
#ifdef USE_ZBUF
	// change zfunction so that the lines show up on top of
	// the polygons they're directly on.
	if (zbuf)
	    zfunction(ZF_ALWAYS);
#endif
	if (doRGB)
	    cpack(scaleRGBColor);
	else
	    color(scaleIndexColor);
	
	switch (quadrant) {
	    case 0:
		drawScaleLines(0, 3, 2);
		break;
	    case 1:
		drawScaleLines(1, 0, 3);
		break;
	    case 2:
		drawScaleLines(3, 2, 1);
		break;
	    case 3:
		drawScaleLines(2, 1, 0);
		break;
	} // switch
	
#ifdef USE_ZBUF
	if (zbuf)
	    zfunction(ZF_LEQUAL);
#endif
    popmatrix();
    drawScaleText();
    
} // drawScale


// do the title text ('packets/sec', etc)
// and 
// if something is selected, draw the appropriate values
void
GraphGadget::drawText() {
    int graphType = nettop->getGraphType();
    char title[TITLE_LENGTH];

    if (graphType < TYPE_PNPACKETS)
	strcpy(title, heading[graphType]);
    else {
	strcpy(title, "Percent of ");
	char tmpStr[10];
	if (graphType == TYPE_PNPACKETS)
	    sprintf(tmpStr, "%d ", nettop->getNPackets());
	else
	    sprintf(tmpStr, "%d ", nettop->getNBytes());
	strcat(title, tmpStr);
	strcat(title, heading[graphType]);
    }

    pushmatrix();

    ortho2(-250.0, 250.0, -250.0, 250.0);
    
    if (doRGB)
	cpack(titleRGBColor);
    else
	color(titleIndexColor);
    
    translate(0.0, 225.0, 0.0);
    DrawStringCenterCenter(title);
    
    translate(0.0, -20.0, 0.0);
    DrawStringCenterCenter(nettop->getFilter());

    if (selectedSrc == -1 && selectedDest == -1) {
	popmatrix();
	return;
    }

//    static char total[] =   "total";


    char *srcName;
    char *destName;
    char traffic[10];
    float totalTraffic;
    int i;
    
    if (selectedSrc == -1) {
	srcName = "total";
	if (totalSrc >= 0)
	    totalTraffic = nettop->data[totalSrc][selectedDest];
	else {
	    totalTraffic = 0;
	    for (i = 0; i < numSrcs; i++)
		totalTraffic += nettop->data[i][selectedDest];
	}
    } else {
	srcName = *(nettop->srcInfo[selectedSrc].displayStringPtr);
    }
	
    if (selectedDest == -1) {
	destName = "total";
	if (totalDest >= 0)
	    totalTraffic = nettop->data[selectedSrc][totalDest];
	else {
	    totalTraffic = 0;
	    for (i = 0; i < numDests; i++)
		totalTraffic += nettop->data[selectedSrc][i];
	}
    } else {
	destName = *(nettop->destInfo[selectedDest].displayStringPtr);
    }

    if (selectedSrc >= 0 && selectedDest >= 0)
	sprintf(traffic, "%8.2f", nettop->data[selectedSrc][selectedDest]);
    else
	sprintf(traffic, "%8.2f", totalTraffic);
    
    if (doRGB)
	cpack(selectRGBColor);
    else
	color(selectIndexColor);
	
    translate(-10.0, -400.0, 0.0);
    if (doSrcAndDest)
	DrawStringCenterRight("Source:");
    else
	DrawStringCenterRight("Node:");

    translate(0.0, -15.0, 0.0);
    if (doSrcAndDest)
	DrawStringCenterRight("Destination:");
    else
	DrawStringCenterRight("Filter:");
    translate(0.0, -15.0, 0.0);
    DrawStringCenterRight("Value:");

    translate(20.0, 30.0, 0.0);
    DrawStringCenterLeft(srcName);
    translate(0.0, -15.0, 0.0);
    DrawStringCenterLeft(destName);
    translate(0.0, -15.0, 0.0);
    DrawStringCenterLeft(traffic);
    
    popmatrix();
} // drawText


void
GraphGadget::setScaleUpdateTime(int t) {
    // printf("GraphGadget::setScaleUpdateTime(%d)\n", t);
    rescaleType = TIMED;
    recentDataMax = 0.0;
    scaleTimer->start(t/10.0);
} // setScaleUpdateTime


// find out max value of data from nettop,
// figure out what vertical scale range should be.
// Note: this can be called from:
//    scaleTimer (callback),
//    render (if new data went over the scale's max), or
//    graph type changing in dataGizmo (packets, bytes, etc)
void
GraphGadget::figureScale(tuGadget* caller) {
    // printf("GraphGadge::figureScale\n");
    if (rescaleType == FIXED)
	return;

// xxx last max is biggest number last time, which means biggest number
// we're smoothing toward. currentmax would be new data, which isn't
// being graphed yet.
    float currentDataMax = nettop->getCurrentMax(); // lastmax? current max?
    int oldScale = scaleMax;
    if (currentDataMax <= 5)
	scaleMax = 5;
    else {
	for (scaleMax = 1; scaleMax < (int)currentDataMax; scaleMax *= 10)
	    ;
	if (scaleMax / 5 >= currentDataMax)
	    scaleMax /= 5;
	else if (scaleMax / 2 >= currentDataMax)
	    scaleMax /= 2;
    }

    // if scale changed and we didn't get called from render itself, render now.
    if (scaleMax != oldScale && caller != this)
	render();

} // figureScale


void
GraphGadget::setScale(int newVal) {
    scaleTimer->stop();
    if (newVal == 0) {
	rescaleType = KEEP_MAX;
    } else {
	rescaleType = FIXED;
	scaleMax = newVal;
    }
    render();
}


void
GraphGadget::render() {
    if (!isRenderable() || nettop->isIconic()) return;

    // scale can decrease only by timer; but increase whenever it needs to
    // (unless it's fixed)
    //printf("render - nettop last = %.1f, nettop current = %.1f, scaleMax = %.1f\n",
    //	nettop->getLastMax(), nettop->getCurrentMax(), scaleMax);
    float currentDataMax = nettop->getCurrentMax(); // lastmax? current max?
    if (rescaleType != FIXED) {
	if (currentDataMax > scaleMax) {
	    figureScale(this);
	    if (rescaleType == TIMED) {
		recentDataMax = 0.0;
		scaleTimer->start();
	    }
	} else if (currentDataMax > recentDataMax && rescaleType == TIMED)
	    recentDataMax = currentDataMax;
    } // not fixed
 
    long xsize,  ysize;
    getsize (&xsize, &ysize);
    float aspect = (float) xsize / (float) ysize;

    perspective (300, aspect, 30.0, 800.0);

    polarview(distance, azimuth, inclination, 0);

    shademodel(FLAT);
    backface(True);

    // change the size of the picking region
    picksize(2, 2);
    
    // czclear(0, zmax-1);  xxx
    

    if (doRGB)
	cpack(mainBackgroundRGBColor);
    else
	color(mainBackgroundIndexColor);

    clear();
    
    drawScale();
    drawBase();

    // all these cases are just needed if zbuffering isn't avail,
    // so that we draw the boxes in the right order.

    loadname(0);
    int s, d;
    
    scale(1.0, 1.0, SCALE_HEIGHT/scaleMax);
    
    // 0: s decreasing	    d increasing    
    // 1: s increasing	    d increasing    
    // 2: s decreasing	    d decreasing    
    // 3: s increasing	    d decreasing
    switch (howDraw) {
      case (DRAW_SQUARE):
      case (DRAW_ROUND): // xxx because I can't draw round yet
	switch (quadrant) {
	    case 0:
		for (s = numSrcs-1; s >= 0; s--)
		    for (d = 0; d < numDests; d++)
 			drawTower(s, d, nettop->data[s][d]);
		break;
	    case 1:
		for (s = 0; s < numSrcs; s++)
		    for (d = 0; d < numDests; d++)
 			drawTower(s, d, nettop->data[s][d]);
		break;
	    case 2:
		for (s = numSrcs-1; s >= 0; s--)
		    for (d = numDests-1;d >= 0; d--)
 			drawTower(s, d, nettop->data[s][d]);
		break;
	    case 3:
		for (s = 0; s < numSrcs; s++)
		    for (d = numDests-1;d >= 0; d--)
 			drawTower(s, d, nettop->data[s][d]);
	    break;

	} // switch quadrant
	break;

      case (DRAW_WIRE):
	drawWire();
	break;
	
      case (DRAW_SURFACE):
	drawSurface();
	break;
	
    } // switch howDraw
    
    if (!picking) {
	drawText();
	swapbuffers();
    }

    gflush();
} // render


/****
 *  names for picking:
 *	0  src  dest	-> tower; followed by srcIndex, destIndex
 *	1  src		-> base, source row, followed by srcIndex
 *	2  dest		-> base, dest column, followed by destIndex
 *	3		-> scale
 */
void
GraphGadget::handlePicks(short buffer[], long hits, tuBool doubleClick) {

    int oldSrc = selectedSrc;
    int oldDest = selectedDest;
    selectedSrc = -1;
    selectedDest = -1;
    int nodeNum;
    int indx = 0;
    tuBool locked;
    for (int h = 0; h < hits; h++) {
	// printf("h: %d; buffer: %d items, %d(type), %d, %d\n",
	//   h, buffer[indx], buffer[indx+1], buffer[indx+2], buffer[indx+3]);
	int items = buffer[indx++];
	if (items) {
	    switch (buffer[indx++]) {
	      case 0:	// tower (src + dest)
		if (doubleClick) {
		    nodeNum = buffer[indx++];
		    locked = !(nettop->getSrcLocked(nodeNum));
		    nettop->setSrcLocked(nodeNum, locked, True);
		    
		    nodeNum = buffer[indx++];
		    locked = !(nettop->getDestLocked(nodeNum));
		    nettop->setDestLocked(nodeNum, locked, True);

		} else {
		    selectedSrc = buffer[indx++];
		    selectedDest = buffer[indx++];
		    if (selectedSrc == oldSrc && selectedDest == oldDest) {
			selectedSrc = -1;
			selectedDest = -1;
		    }
		}
		break;
	      case 1:	// src
		if (doubleClick) {
		    nodeNum = buffer[indx++];
		    locked = !(nettop->getSrcLocked(nodeNum));
		    nettop->setSrcLocked(nodeNum, locked, True);
		} else {
		    selectedSrc = buffer[indx++];
		    if (selectedSrc == oldSrc)
			selectedSrc = -1;
		    selectedDest = oldDest;
		}
		break;
	      case 2:	// dest
		if (doubleClick) {
		    nodeNum = buffer[indx++];
		    locked = !(nettop->getDestLocked(nodeNum));
		    nettop->setDestLocked(nodeNum, locked, True);
		} else {
		    selectedSrc = oldSrc;
		    selectedDest = buffer[indx++];
		    if (selectedDest == oldDest)
			selectedDest = -1;
		}
		break;
	      case 3:	// scale
		// don't do anything now. 
		break;
	    } // switch
	} // if items
    } // for

} // handlePicks


void
GraphGadget::getLayoutHints(tuLayoutHints* h) {
    tuGLGadget::getLayoutHints(h);
    h->prefWidth = 550;
    h->prefHeight = 550;
}


// pan or spin	middle mouse button (adjustButton)
// select		left	    (selectButton)
// special select	double-left  (lock node)
// zoom/dolly	left+middle
// zoom/dolly	ctrl+middle
// home		home key
//    NOTE: Mod1Mask:    alt
//          ControlMask: control
//          ShiftMask:   shift
//	    XK_Home:     home key
void
GraphGadget::processEvent(XEvent *ev) {
    int whichButton;
    switch (ev->type) {
	case ButtonPress:
	    whichButton = ev->xbutton.button;
	    if (whichButton == tuStyle::getSelectButton()) {
		selectDown = True;
		if (adjustDown) {
		    if (dragging)
			stopDrag(ev->xbutton.x, ev->xbutton.y);
		    startDolly(ev->xbutton.x, ev->xbutton.y);
		} else {
		    // picking
		    tuBool doubleClick = (getWindow()->checkForMultiClick() == 2);
		    short pickBuffer[PICK_BUF_SIZE];
		    initnames();
		    picking = True;
		    pick(pickBuffer, PICK_BUF_SIZE);
		    render();
		    long hits = endpick(pickBuffer);
		    picking = False;
		    handlePicks(pickBuffer, hits, doubleClick);
		    render();
		}
	    } else if (whichButton == tuStyle::getAdjustButton()) {
		adjustDown = True;
		if (((ev->xbutton.state & ControlMask) != 0) ||
		     selectDown) {
		    if (dragging)
			stopDrag(ev->xbutton.x, ev->xbutton.y);
		    startDolly(ev->xbutton.x, ev->xbutton.y);
		} else {
		    if (dollying)
			stopDolly(ev->xbutton.x, ev->xbutton.y);
		    startDrag(ev->xbutton.x, ev->xbutton.y);
		}
	    }
	    break;
	    
	case MotionNotify:
	    while (XCheckWindowEvent(window->getDpy(), window->getWindow(),
				     ButtonMotionMask, ev)) {
		// only look at the last one
	    }
	    if (dragging)
		drag(ev->xbutton.x, ev->xbutton.y);
	    else if (dollying)
		dolly(ev->xbutton.x, ev->xbutton.y);
	    break;
	    
	case ButtonRelease:
	    whichButton = ev->xbutton.button;
	    if (whichButton == tuStyle::getAdjustButton()) {
		adjustDown = False;
		if (dragging)
		    stopDrag(ev->xbutton.x, ev->xbutton.y);
		else if (dollying)
		    stopDolly(ev->xbutton.x, ev->xbutton.y);
	    } else if (whichButton == tuStyle::getSelectButton()) {
		selectDown = False;
		if (dollying)
		    stopDolly(ev->xbutton.x, ev->xbutton.y);
		if (adjustDown)
		    startDrag(ev->xbutton.x, ev->xbutton.y);
	    }
	    break;

    } // switch
   
    
    tuGLGadget::processEvent(ev);
}

void
GraphGadget::startDrag(int x, int y) {
    dragX = x;
    dragY = y;
    dragging = True;
}

void
GraphGadget::drag(int x, int y) {
    int dx = x - dragX;
    int dy = y - dragY;
    
    setAzimuth(azimuth - dx, True);
    setInclination(inclination - dy, True);
    
    dragX = x;
    dragY = y;

    render();
} // drag

void
GraphGadget::stopDrag(int x, int y) {
    drag(x, y);
    dragging = False;
}

void
GraphGadget::startDolly(int x, int y) {
    dollyX = x;
    dollyY = y;
    dollying = True;
}

void
GraphGadget::dolly(int x, int y) {
    int deltaX = (x - dollyX);  // x increases to the right
    int deltaY = (dollyY - y);  // y increases down
    int delta;
    
    if (TU_ABS(deltaX) > TU_ABS(deltaY))
	delta = deltaX;
    else
	delta = deltaY;

    setDistance(distance - delta, True);    
    
    dollyX = x;
    dollyY = y;

    render();
} // dolly

void
GraphGadget::stopDolly(int x, int y) {
    dolly(x, y);
    dollying = False;
}

void
GraphGadget::setDistance(int d,  tuBool tellView) {
    d = TU_MAX(d, MIN_DISTANCE);
    d = TU_MIN(d, MAX_DISTANCE);
    distance = d;
    if (tellView)
	nettop->setDistanceScrollBar(d);
}

void
GraphGadget::setAzimuth(int a,  tuBool tellView) {
    // wrap around if beyond MIN or MAX
    if (a < MIN_AZIMUTH)
	a = MAX_AZIMUTH - (MIN_AZIMUTH - a);
    if (a > MAX_AZIMUTH)
	a = MIN_AZIMUTH + (a - MAX_AZIMUTH);
    azimuth = a;
    if (tellView)
	nettop->setAzimuthScrollBar(a);

    if (azimuth >= 0 && azimuth < 900) quadrant = 0;
    else if (azimuth >= 900 && azimuth < 1800) quadrant = 1;
    else if (azimuth >= 1800 && azimuth < 2700) quadrant = 3;
    else if (azimuth >= 2700 || azimuth < 0) quadrant = 2;
}

void
GraphGadget::setInclination(int i,  tuBool tellView) {
    i = TU_MAX(i, MIN_INCLINATION);
    i = TU_MIN(i, MAX_INCLINATION);
    inclination = i;
    if (tellView)
	nettop->setInclinationScrollBar(i);
}

// zoom in or out so that the graph base looks about the same size
// even though the number of srcs or dests changed
// (same size == larger dimension (x, y) will be same size)
void
GraphGadget::adjustDistance(int newNumSrcs, int newNumDests) {
    int oldMax = TU_MAX(numSrcs, numDests);
    int newMax = TU_MAX(newNumSrcs, newNumDests);
    int dist = (int) ((float)distance * newMax / oldMax);
    setDistance(dist, True);    
}

void
GraphGadget::setNumSrcs(int sources) {
    adjustDistance(sources, numDests);
    numSrcs = sources;
    
    for (int s = 0; s < numSrcs; s++)
	for (int d = 0; d < numDests; d++) {
	    t[s][d][0] = (d + 0.5 - numDests * 0.5) * TOWER_SPACING;
	    t[s][d][1] = (s + 0.5 - numSrcs  * 0.5) * TOWER_SPACING;
	    t[s][d][2] = 0.0;
	}

    render();
}

void
GraphGadget::setNumDests(int dests) {
    adjustDistance(numSrcs, dests);
    numDests = dests;

    for (int s = 0; s < numSrcs; s++)
	for (int d = 0; d < numDests; d++) {
	    t[s][d][0] = (d + 0.5 - numDests * 0.5) * TOWER_SPACING;
	    t[s][d][1] = (s + 0.5 - numSrcs * 0.5) * TOWER_SPACING;
	    t[s][d][2] = 0.0;
	}

    render();
}

void
GraphGadget::setHowDraw(int type) {
    howDraw = type;
    if (howDraw == DRAW_SURFACE)
	shademodel(GOURAUD);
    else
	shademodel(FLAT);
    render();
}

void
GraphGadget::setShowTotals(tuBool show) {
    showTotals = show;
    render();
}

void
GraphGadget::setLighting(tuBool light) {
    useLighting = light;
    render();
}
