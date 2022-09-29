/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	GL Map Gadget
 *
 *	$Revision: 1.7 $
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

#include <gl/glws.h>
#include <gl/gl.h>
#include <tuRect.h>
#include <tuGLGadget.h>
#include <tuWindow.h>
#include <tuTopLevel.h>
#include <tuStyle.h>
#include <tuCallBack.h>
#include <macros.h>
#include <math.h>
#include <stdio.h>

#include "trig.h"
#include "netlook.h"
#include "network.h"
#include "mapgadget.h"
#include "glmapgadget.h"
#include "netlookgadget.h"
#include "nlwindow.h"
#include "viewgadget.h"

static const unsigned int Solid = 0;
static const unsigned int Dashed = 1;

tuDeclareCallBackClass(GLMapGadgetCallBack, GLMapGadget);
tuImplementCallBackClass(GLMapGadgetCallBack, GLMapGadget);

// XXX Can this go away?
GLMapGadget *glmapgadget;
extern NetLookGadget *netlookgadget;
extern ViewGadget *viewgadget;

GLMapGadget::GLMapGadget(NetLook *nl, tuGadget *parent,
			     const char *instanceName)
		: tuGLGadget(parent, instanceName)
{
    // Save pointer to netlook
    netlook = nl;
    glmapgadget = this;
    netlookgadget->setMapGadget(this);
    adjustButton = selectButton = False;
    dragging = sweeping = zooming = False;
}

void
GLMapGadget::bindResources(tuResourceDB *rdb, tuResourcePath *rp)
{
    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(True);
    setRGBmode(False);
    setZBuffer(False);

    // Catch iconify
    getTopLevel()->setIconicCallBack(
			new GLMapGadgetCallBack(this, GLMapGadget::iconify));
    iconic = getTopLevel()->getIconic();
}

void
GLMapGadget::open(void)
{
    tuGLGadget::open();
    deflinestyle(Dashed, 0xF0F0);
}

void
GLMapGadget::render(void)
{
    if (!isRenderable() || iconic)
	return;

    winset();
    pushmatrix();
    color(netlook->getMapBackgroundColor());
    clear();

    viewport(0, getWidth() - 1, 0, getHeight() - 1);
    ortho(universe.getX(), universe.getX() + universe.getWidth(),
	  universe.getY(), universe.getY() + universe.getHeight(),
	  0.0, 65535.0);

    // Draw networks
    for (NetworkNode *n = (NetworkNode *) netlook->getDatabase()->child;
		n != 0; n = (NetworkNode *) n->next) {
	for (SegmentNode *s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    Network *network = (Network *) s->data;
	    if ((network->info & HIDDEN) != 0)
		continue;
	    if ((network->info & NETWORKPICKED) != 0)
		color(netlook->getSelectColor());
	    else if (netlook->getNetShow() == ACTIVE
		     && network->connections == 0
		     && (network->info & NODEPICKED) == 0)
		color(netlook->getInactiveNetworkColor());
	    else
		color(netlook->getActiveNetworkColor());

	    circ(network->position.position[X], network->position.position[Y],
		 network->radius);
	}
    }

    // Draw viewport
    Rectf v = netlook->getViewPort();
    color(netlook->getViewboxColor());
    rect(v.getX(), v.getY(), v.getX() + v.getWidth(), v.getY() + v.getHeight());

    if (sweeping) {
	setlinestyle(Dashed);
	rect(sweepBox.getX(), sweepBox.getY(),
	     sweepBox.getX() + sweepBox.getWidth(),
	     sweepBox.getY() + sweepBox.getHeight());
	setlinestyle(Solid);
    }

    swapbuffers();
    popmatrix();
}

void
GLMapGadget::getLayoutHints(tuLayoutHints* h)
{
    tuGLGadget::getLayoutHints(h);
    h->prefWidth = 150;
    h->prefHeight = 150;
}

void
GLMapGadget::processEvent(XEvent *e)
{
    winset();

    switch (e->type) {
	case Expose:
	case ConfigureNotify:
	    break;

	case ButtonPress:
	    if (e->xbutton.button == tuStyle::getSelectButton()) {
		if (!adjustButton)
		    pickIt(e->xbutton.x, e->xbutton.y);
		else {
		    if (dragging)
			stopDrag(e->xbutton.x, e->xbutton.y);
		    else if (sweeping)
			stopSweep(e->xbutton.x, e->xbutton.y);
		    startZoom(e->xbutton.x, e->xbutton.y);
		}
		selectButton = True;
	    } else if (e->xbutton.button == tuStyle::getAdjustButton()) {
		if (selectButton || (e->xbutton.state & ControlMask) != 0)
		    startZoom(e->xbutton.x, e->xbutton.y);
		else if ((e->xbutton.state & Mod1Mask) != 0)
		    startSweep(e->xbutton.x, e->xbutton.y);
		else
		    startDrag(e->xbutton.x, e->xbutton.y);
		adjustButton = True;
	    }
	    break;

	case MotionNotify:
	    while (XCheckWindowEvent(window->getDpy(), window->getWindow(),
				     ButtonMotionMask, e)) {
		// only look at the last one
	    }
	    if (dragging)
		drag(e->xbutton.x, e->xbutton.y);
	    else if (sweeping)
		sweep(e->xbutton.x, e->xbutton.y);
	    else if (zooming)
		zoom(e->xbutton.x, e->xbutton.y);
	    break;

	case ButtonRelease:
	    if (dragging)
		stopDrag(e->xbutton.x, e->xbutton.y);
	    else if (sweeping)
		stopSweep(e->xbutton.x, e->xbutton.y);
	    else if (zooming)
		stopZoom(e->xbutton.x, e->xbutton.y);
	    if (e->xbutton.button == tuStyle::getAdjustButton())
		adjustButton = False;
	    else if (e->xbutton.button == tuStyle::getSelectButton()) {
		selectButton = False;
		if (adjustButton)
		    startDrag(e->xbutton.x, e->xbutton.y);
	    }
	    break;
    }

    tuGLGadget::processEvent(e);
}

void
GLMapGadget::setUniverse(void)
{
    float w = (float) getBBox().getWidth();
    float h = (float) getBBox().getHeight();
    setUniverse(-w / 2.0, -h / 2.0, w, h);
}

void
GLMapGadget::setUniverse(const Rectf &r)
{
    universe = r;
}

void
GLMapGadget::setUniverse(float x, float y, float w, float h)
{
    universe.set(x, y, w, h);
}

void
GLMapGadget::startDrag(int x, int y)
{
    dragging = True;
    dragx = x;
    dragy = y;
    netlook->setDragCursor(this);
}

void
GLMapGadget::drag(int x, int y)
{
    float sx = universe.getWidth() / getWidth();
    float sy = universe.getHeight() / getHeight();

    // Find difference
    float dx = (x - dragx) * sx;
    float dy = (y - dragy) * sy;

    // Get current viewport
    Rectf v = netlook->getViewPort();
    float vx = v.getX() + dx;
    float vy = v.getY() - dy;

    // Constrain movement
    float v2 = MIN(v.getWidth(), v.getHeight()) / 2.0;
    vx = MAX(vx, universe.getX() - v2);
    vx = MIN(vx, universe.getX() + universe.getWidth() - v2);
    vy = MAX(vy, universe.getY() - v2);
    vy = MIN(vy, universe.getY() + universe.getHeight() - v2);

    // Set new viewport
    v.setX(vx);
    v.setY(vy);
    netlook->setViewPort(v);

    dragx = x;
    dragy = y;
    render();
    netlookgadget->render();
}

void
GLMapGadget::stopDrag(int x, int y)
{
    drag(x, y);
    dragging = False;
    netlook->setNormalCursor(this);
}

void
GLMapGadget::startSweep(int x, int y)
{
    sweeping = True;
    sweepx = x;
    sweepy = -y;
    netlook->setSweepCursor(this);
}

void
GLMapGadget::sweep(int x, int y)
{
    dragx = x;
    dragy = -y;

    float sx = universe.getWidth() / getWidth();
    float sy = universe.getHeight() / getHeight();

    // Set sweep box
    if (sweepx < dragx) {
	sweepBox.setX(sweepx * sx - universe.getWidth() / 2.0);
	sweepBox.setWidth((dragx - sweepx) * sx);
    } else {
	sweepBox.setX(dragx * sx - universe.getWidth() / 2.0);
	sweepBox.setWidth((sweepx - dragx) * sx);
    }
    if (sweepy < dragy) {
	sweepBox.setY(sweepy * sy + universe.getHeight() / 2.0);
	sweepBox.setHeight((dragy - sweepy) * sy);
    } else {
	sweepBox.setY(dragy * sy + universe.getHeight() / 2.0);
	sweepBox.setHeight((sweepy - dragy) * sy);
    }

    // Maintain minimum size
    float minScale = viewgadget->getMinScaleValue();
    float f = minScale * netlookgadget->getWidth();
    if (sweepBox.getWidth() < f) {
	if (sweepx < dragx)
	    sweepBox.setWidth(f);
	else {
	    sweepBox.setX(sweepBox.getX() - (f - sweepBox.getWidth()));
	    sweepBox.setWidth(f);
	}
    }
    f = minScale * netlookgadget->getHeight();
    if (sweepBox.getHeight() < f) {
	if (sweepy < dragy)
	    sweepBox.setHeight(f);
	else {
	    sweepBox.setY(sweepBox.getY() - (f - sweepBox.getHeight()));
	    sweepBox.setHeight(f);
	}
    }
	
    // Maintain aspect ratio
    Rectf v = netlook->getViewPort();
    f = v.getWidth() / v.getHeight();
    if (sweepBox.getWidth() < sweepBox.getHeight() * f) {
	if (sweepx < dragx)
	    sweepBox.setWidth(f * sweepBox.getHeight());
	else {
	    float w = sweepBox.getWidth() - f * sweepBox.getHeight();
	    sweepBox.setX(sweepBox.getX() + w);
	    sweepBox.setWidth(sweepBox.getWidth() - w);
	}
    } else {
	f = 1.0 / f;
	if (sweepy < dragy)
	    sweepBox.setHeight(f * sweepBox.getWidth());
	else {
	    float h = sweepBox.getHeight() - f * sweepBox.getWidth();
	    sweepBox.setY(sweepBox.getY() + h);
	    sweepBox.setHeight(sweepBox.getHeight() - h);
	}
    }

    // Render with sweepBox
    render();
}

void
GLMapGadget::stopSweep(int x, int y)
{
    sweep(x, y);
    netlook->setViewPort(sweepBox);
    sweeping = False;
    render();
    netlookgadget->render();
    netlook->setNormalCursor(this);
}

void
GLMapGadget::pickIt(int x, int y)
{
    Network *picked = 0;
    float fx = x * universe.getWidth() / getWidth() + universe.getX();
    float fy = (getHeight() - y) * universe.getHeight() / getHeight()
	       + universe.getY();
    for (NetworkNode *n = (NetworkNode *) netlook->getDatabase()->child;
		n != 0 && picked == 0; n = (NetworkNode *) n->next) {
	for (SegmentNode *s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    Network *network = (Network *) s->data;
	    if ((network->info & HIDDEN) != 0)
		continue;
	    float nx = fx - network->position.position[X];
	    float ny = fy - network->position.position[Y];
	    if (nx * nx + ny * ny < network->radius * network->radius) {
		picked = network;
		break;
	    }
	}
    }

    if (picked != 0) {
	netlook->setPickedNetwork(picked);
	render();
	netlookgadget->render();
    } else if (netlook->getPickedObjectType() != pkNone) {
	netlook->unpickObject();
	render();
	netlookgadget->render();
    }
}

void
GLMapGadget::startZoom(int x, int y)
{
    dragx = x;
    dragy = y;
    zooming = True;
    netlook->setDragCursor(this);
}

void
GLMapGadget::zoom(int x, int y)
{
    int dx = dragx - x;
    int dy = y - dragy;
    float sv = (abs(dx) < abs(dy) ? dy : dx) / 50.0;
    viewgadget->setScaleValue(viewgadget->getScaleValue() + sv);
    dragx = x;
    dragy = y;
}

void
GLMapGadget::stopZoom(int x, int y)
{
    zoom(x, y);
    zooming = False;
    netlook->setNormalCursor(this);
}

void
GLMapGadget::iconify(tuGadget *)
{
    iconic = getTopLevel()->getIconic();
}
