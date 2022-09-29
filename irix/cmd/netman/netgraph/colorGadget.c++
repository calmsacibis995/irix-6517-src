/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Color Gadget (GL color picker for editControl)
 *
 *	$Revision: 1.3 $
 *	$Date: 1992/10/01 01:13:49 $
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

#include "colorGadget.h"

#include <gl/gl.h>
#include <gl/glws.h>
#include <stdio.h>

// #define LEFT	    -40
#define LEFT	    0
#define RIGHT	    160
#define BOTTOM	      0
#define SMALL_TOP    40
#define BIG_TOP	     80

static int colors[] = {
      0,   1,   2,   3,   4,   5,   6,   7,
      8,   9,  10,  11,  12,  13,  14,  15,
     58,  59,  60,  61,  80,  89,  90,  91,
    128, 168, 248, 232, 136, 216, 226, 219
};

ColorGadget::ColorGadget(tuGadget *parent,  const char *instanceName)
	    : tuGLGadget(parent, instanceName) {

    long bitPlanes = getgdesc(GD_BITS_NORM_DBL_CMODE);
    if (bitPlanes < 8) {
	// numColors = 16;
	top = SMALL_TOP;
    } else {
	// numColors = 32;
	top = BIG_TOP;
    } 

} // ColorGadget


void
ColorGadget::bindResources(tuResourceDB *rdb, tuResourcePath *rp) {

    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(True);
    setRGBmode(False);
    setZBuffer(False);
}


void
ColorGadget::getLayoutHints(tuLayoutHints* h) {
    tuGLGadget::getLayoutHints(h);
    h->prefWidth = -(LEFT) + RIGHT + 1;
    h->prefHeight = top + 1;
    h->flags |= tuLayoutHints_fixedHeight;
    h->flags |= tuLayoutHints_fixedWidth;
    
}


// note: selectButton is left, adjustButton is middle
// note: I incorporated tuGLGadget::processEvent() here, so that
// the callback would only get called for a button press, not
// for expose, etc.
void
ColorGadget::processEvent(XEvent *ev) {
    // printf("ColorGadget::processEvent(type = %d\n", ev->type);
    if (ev->type == ButtonPress &&
	  ev->xbutton.button == tuStyle::getSelectButton()) {
	
	int x = ev->xbutton.x;
	int y = top - ev->xbutton.y;
	render();
	winset();
	Colorindex parray[2];
	rectread(x, y, x+1, y, parray);
	colorIndex = parray[0];

	render();

	if (callback) callback->doit(this);

    } else if (ev->type == Expose || ev->type == ConfigureNotify) {
        winset();
        viewport(0, getWidth() - 1, 0, getHeight() - 1);
    }
    
    lastevent = *ev;
    tuGadget::processEvent(ev);

}



void
ColorGadget::render() {
    if (!isRenderable()) return;
    // printf("ColorGadget::render, strip # %d\n", strip->getNumber());
    
    winset();
    pushmatrix();
    pushviewport();
	
    ortho2(LEFT, RIGHT, BOTTOM, top);
    
    color(WHITE);
    clear();
    
    // draw array of boxes
    int c = 0;
    int x, y;

    for (y = 0; y < top; y += 20)
	for (x = 0; x < 160; x += 20, c++) {
	    color(colors[c]);
	    sboxfi(x, y, x+20, y+20);
	}
    // draw borders around boxes
    color(BLACK);
    for (y = 0; y <= top; y += 20) {
	move2(0, y);
	draw2(160, y);
    }
    for (x = 0; x <= 160; x += 20) {
	move2(x, 0);
	draw2(x, 80);
    }
    
    popviewport();
    popmatrix();


    swapbuffers();

    // printf("about to call gflush\n");
//    gflush();
    // printf("               done.\n");
    
} // render

