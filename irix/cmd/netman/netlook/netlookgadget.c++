/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetLook Gadget
 *
 *	$Revision: 1.10 $
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

#include <tuRect.h>
#include <tuGLGadget.h>
#include <tuScrollBar.h>
#include <tuWindow.h>
#include <tuTopLevel.h>
#include <tuStyle.h>
#include <gl/gl.h>
#include <gl/glws.h>
#include <macros.h>
#include <math.h>
#include <stdio.h>

#include "trig.h"
#include "netlook.h"
#include "network.h"
#include "node.h"
#include "conntable.h"
#include "viewgadget.h"
#include "netlookgadget.h"
#include "glmapgadget.h"

static const float drawScaleValue = 6.0;
static const unsigned int Solid = 0;
static const unsigned int Dashed = 1;

// XXX Can this go away?
NetLookGadget *netlookgadget;
extern GLMapGadget *glmapgadget;

NetLookGadget::NetLookGadget(NetLook *nl, tuGadget *parent,
			     const char *instanceName)
		: tuGLGadget(parent, instanceName)
{
    // Save pointer to netlook
    netlook = nl;
    netlookgadget = this;
    adjustButton = selectButton = 0;
    dragging = sweeping = zooming = 0;
}

void
NetLookGadget::bindResources(tuResourceDB *rdb, tuResourcePath *rp)
{
    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(True);
    setRGBmode(False);
    setZBuffer(True);
    //selectEvents(ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
}

void
NetLookGadget::open(void)
{
    tuGLGadget::open();
    zmax = getgdesc(GD_BITS_NORM_ZBUFFER);
    if (zmax == 0)
	zbuffer(FALSE);
    else {
	zmax = getgdesc(GD_ZMAX);
	zfunction(ZF_LEQUAL);
    }
    deflinestyle(Dashed, 0xF0F0);
    fontInit();
    setUniverse();
    ev->setViewPort();
}

void
NetLookGadget::render(void)
{
    // don't render if we're iconified
    if (netlook->isIconic())
	return;

    winset();
    pushmatrix();
    if (zmax != 0)
	czclear(netlook->getMapBackgroundColor(), zmax);
    else {
	color(netlook->getMapBackgroundColor());
	clear();
    }

    // Trivial reject
    Position pos;
    pos.extent = ev->getViewPort();
    pos.position[X] = pos.position[Y] = 0.0;

    // When the scene is zoomed out, render with less complexity
    unsigned int complex = ev->getScaleValue() < drawScaleValue;

    NetworkNode *n;
    SegmentNode *s;
    Network *network;

    // Render the networks
    for (n = (NetworkNode *) netlook->getDatabase()->child;
		n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    network = (Network *) s->data;
	    if ((network->info & HIDDEN) == 0
		&& extentOverlap(&pos, &(network->position)) != 0)
		network->render(complex);
	}
    }

    // Render the connections
    netlook->getConnectionTable()->render();

    // Render the sweep box
    if (sweeping) {
	color(netlook->getViewboxColor());
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
NetLookGadget::render(Network *network, Node *node)
{
    winset();
    pushmatrix();
    frontbuffer(TRUE);
    backbuffer(FALSE);

    // Trivial reject
    Position pos;
    pos.extent = ev->getViewPort();
    pos.position[X] = pos.position[Y] = 0.0;

    // When the scene is zoomed out, render with less complexity
    unsigned int complex = ev->getScaleValue() < drawScaleValue;

    if (extentOverlap(&pos, &(network->position)) != 0) {
	if (node == 0)
	    network->render(complex);
	else {
	    translate(network->position.position[X],
		      network->position.position[Y], 0.0);
	    node->render(complex);
	}
    }

    frontbuffer(FALSE);
    backbuffer(TRUE);
    popmatrix();
}

void
NetLookGadget::getLayoutHints(tuLayoutHints* h)
{
    tuGLGadget::getLayoutHints(h);
    h->prefWidth = 400;
    h->prefHeight = 400;
}

void
NetLookGadget::resize(int w, int h)
{
    tuGadget::resize(w, h);

    if (isMapped()) {
	ev->recalculateScrollBar();
	Rectf view = ev->getViewPort();
	float scaleValue = ev->getScaleValue();
	float fx = getWidth() * scaleValue - view.getWidth();
	float fy = getHeight() * scaleValue - view.getHeight();
	ev->setViewPort(view.getX() - fx / 2.0, view.getY() - fy / 2.0,
			view.getWidth() + fx, view.getHeight() + fy);
	glmapgadget->render();
    }
}

void
NetLookGadget::processEvent(XEvent *e)
{
    winset();

    switch (e->type) {
	case ButtonPress:
	    if (e->xbutton.button == tuStyle::getSelectButton()) {
		if (adjustButton) {
		    if (adjusting)
			stopAdjust(e->xbutton.x, e->xbutton.y);
		    else if (dragging)
			stopDrag(e->xbutton.x, e->xbutton.y);
		    else if (sweeping)
			stopSweep(e->xbutton.x, e->xbutton.y);
		    startZoom(e->xbutton.x, e->xbutton.y);
		} else {
		    pickIt(e->xbutton.x, e->xbutton.y);
		    startAdjust(e->xbutton.x, e->xbutton.y,
				e->xbutton.state & Mod1Mask);
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
	    else if (adjusting)
		adjust(e->xbutton.x, e->xbutton.y);
	    else if (zooming)
		zoom(e->xbutton.x, e->xbutton.y);
	    break;

	case ButtonRelease:
	    if (adjusting)
		stopAdjust(e->xbutton.x, e->xbutton.y);
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

unsigned int
NetLookGadget::getAvailableBitplanes(void)
{
    return getgdesc(GD_BITS_NORM_DBL_CMODE);
}

void
NetLookGadget::setUniverse(void)
{
    float w = (float) getBBox().getWidth();
    float h = (float) getBBox().getHeight();
    universe.set(-w / 2.0, -h / 2.0, w, h);
    ev->setViewPort();
    mg->setUniverse(universe);
}

void
NetLookGadget::setUniverse(const Rectf &r)
{
    universe = r;
    ev->recalculateScrollBar();
    mg->setUniverse(universe);
}

void
NetLookGadget::setUniverse(float x, float y, float w, float h)
{
    universe.set(x, y, w, h);
    ev->recalculateScrollBar();
    mg->setUniverse(universe);
}

void
NetLookGadget::position(void)
{
    Position *pos, *last, *first;
    float radius, radius2x4, size, angle, ainc, maxnetsize = 0.0;
    unsigned int nets = 0;

    for (NetworkNode *n = (NetworkNode *) netlook->getDatabase()->child;
		n != 0; n = (NetworkNode *) n->next) {
	for (SegmentNode *s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    nets++;
	    Network *network = (Network *) s->data;
	    if (network->position.extent.getWidth() > maxnetsize)
		maxnetsize = network->position.extent.getWidth();
	}
    }

tryagain:
    radius = MIN(universe.getWidth(), universe.getHeight());
    radius = (radius - maxnetsize) * 0.5;
    if (radius <= 0.0) {
	growUniverse();
	goto tryagain;
    }

    radius2x4 = 4.0 * radius * radius;
    last = 0;
    first = 0;

    for (n = (NetworkNode *) netlook->getDatabase()->child;
		n != 0; n = (NetworkNode *) n->next) {
	for (SegmentNode *s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    Network *network = (Network *) s->data;
	    if ((network->info & HIDDEN) != 0)
		continue;
	    pos = &(network->position);

	    if (first == 0) {
		first = pos;
		angle = 0.0;
	    } else {
		size = last->extent.getWidth() + pos->extent.getWidth();
		size = 1 - size * size / radius2x4;
		ainc = acosf(size);
		angle += ainc;
		if (angle >= M_2PI) {
		    growUniverse();
	            goto tryagain;
	        }
	    }

	    pos->position[X] = -l_cos((float) angle) * radius;
	    pos->position[Y] = l_sin((float) angle) * radius;
	    last = pos;
	}
    }

    if (first != last && extentOverlap(first, last)) {
	growUniverse();
	goto tryagain;
    }
}

void
NetLookGadget::growUniverse(void)
{
    float g = 1.0 + 2.0 / log(MIN(universe.getWidth(), universe.getHeight()));
    setUniverse(universe.getX() * g,
		universe.getY() * g,
		universe.getWidth() * g,
		universe.getHeight() * g);
}

unsigned int
NetLookGadget::extentOverlap(Position *p, Position *q)
{
    p2f pll,pur,qll,qur;

    pll[X] = p->position[X] + p->extent.getX();
    pll[Y] = p->position[Y] + p->extent.getY();
    pur[X] = pll[X] + p->extent.getWidth();
    pur[Y] = pll[Y] + p->extent.getHeight();

    qll[X] = q->position[X] + q->extent.getX();
    qll[Y] = q->position[Y] + q->extent.getY();
    qur[X] = qll[X] + q->extent.getWidth();
    qur[Y] = qll[Y] + q->extent.getHeight();

    if (pur[X] < qll[X] || pur[Y] < qll[Y]
	|| qur[X] < pll[X] || qur[Y] < pll[Y])
    	return 0;

    return 1;
}

void
NetLookGadget::startDrag(int x, int y)
{
    dragx = x;
    dragy = y;
    dragging = True;
    netlook->setDragCursor(this);
}

void
NetLookGadget::drag(int x, int y)
{
    // Find difference
    float sv = ev->getScaleValue();
    float dx = (x - dragx) * sv;
    float dy = (y - dragy) * sv;

    // Get current viewport
    Rectf v = ev->getViewPort();
    float vx = v.getX() - dx;
    float vy = v.getY() + dy;

    // Constrain movement
    float v2 = MIN(v.getWidth(), v.getHeight()) / 2.0;
    vx = MAX(vx, universe.getX() - v2);
    vx = MIN(vx, universe.getX() + universe.getWidth() - v2);
    vy = MAX(vy, universe.getY() - v2);
    vy = MIN(vy, universe.getY() + universe.getWidth() - v2);

    // Set new viewport
    v.setX(vx);
    v.setY(vy);
    ev->setViewPort(v);

    dragx = x;
    dragy = y;
    render();
    glmapgadget->render();
    winset();
}

void
NetLookGadget::stopDrag(int x, int y)
{
    drag(x, y);
    dragging = False;
    netlook->setNormalCursor(this);
}

void
NetLookGadget::startSweep(int x, int y)
{
    sweeping = True;
    sweepx = x;
    sweepy = getHeight() - y;
    netlook->setSweepCursor(this);
}

void
NetLookGadget::sweep(int x, int y)
{
    dragx = x;
    dragy = getHeight() - y;

    float sv = ev->getScaleValue();
    Rectf v = ev->getViewPort();

    // Set sweep box
    if (sweepx < dragx) {
	sweepBox.setX(sweepx * sv + v.getX());
	sweepBox.setWidth((dragx - sweepx) * sv);
    } else {
	sweepBox.setX(dragx * sv + v.getX());
	sweepBox.setWidth((sweepx - dragx) * sv);
    }
    if (sweepy < dragy) {
	sweepBox.setY(sweepy * sv + v.getY());
	sweepBox.setHeight((dragy - sweepy) * sv);
    } else {
	sweepBox.setY(dragy * sv + v.getY());
	sweepBox.setHeight((sweepy - dragy) * sv);
    }

    // Maintain minimum size
    float minScale = ev->getMinScaleValue();
    float f = minScale * getWidth();
    if (sweepBox.getWidth() < f) {
	if (sweepx < dragx)
	    sweepBox.setWidth(f);
	else {
	    sweepBox.setX(sweepBox.getX() - (f - sweepBox.getWidth()));
	    sweepBox.setWidth(f);
	}
    }
    f = minScale * getHeight();
    if (sweepBox.getHeight() < f) {
	if (sweepy < dragy)
	    sweepBox.setHeight(f);
	else {
	    sweepBox.setY(sweepBox.getY() - (f - sweepBox.getHeight()));
	    sweepBox.setHeight(f);
	}
    }
	
    // Maintain aspect ratio
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
NetLookGadget::stopSweep(int x, int y)
{
    sweep(x, y);
    ev->setViewPort(sweepBox);
    sweeping = False;
    render();
    glmapgadget->render();
    winset();
    netlook->setNormalCursor(this);
}

void
NetLookGadget::startZoom(int x, int y)
{
    dragx = x;
    dragy = y;
    zooming = True;
    netlook->setDragCursor(this);
}

void
NetLookGadget::zoom(int x, int y)
{
    int dx = dragx - x;
    int dy = y - dragy;
    float sv = (abs(dx) < abs(dy) ? dy : dx) / 100.0;
    ev->setScaleValue(ev->getScaleValue() + sv);
    dragx = x;
    dragy = y;
}

void
NetLookGadget::stopZoom(int x, int y)
{
    zoom(x, y);
    zooming = False;
    netlook->setNormalCursor(this);
}

void
NetLookGadget::pickIt(int x, int y)
{
    tuBool hit = False;
    switch (netlook->getPickedObjectType()) {
	case pkNone:
	    break;
	case pkNetwork:
	    {
		Network *n = netlook->getPickedNetwork();
		netlook->unpickObject();
		render(n);
		glmapgadget->render();
		winset();
	    }
	    break;
	case pkNode:
	    {
		Node *n = netlook->getPickedNode();
		netlook->unpickObject();
		render(n->getNetwork(), n);
		glmapgadget->render();
		winset();
	    }
	    break;
	case pkConnection:
	    netlook->unpickObject();
	    render();
	    glmapgadget->render();
	    winset();
	    break;
    }

    // Check connections
    Connection *c = netlook->getConnectionTable()->checkPick();
    if (c != 0) {
	hit = True;
	netlook->setPickedConnection(c);
	render();
    } else {
	// Check networks
	float sv = ev->getScaleValue();
	Rectf v = ev->getViewPort();
	float fx = x * sv + v.getX();
	float fy = v.getHeight() - y * sv + v.getY();
	for (NetworkNode *n = (NetworkNode *) netlook->getDatabase()->child;
		n != 0 && !hit; n = (NetworkNode *) n->next) {
	    for (SegmentNode *s = (SegmentNode *) n->child;
			s != 0; s = (SegmentNode *) s->next) {
		Network *network = (Network *) s->data;
		if ((network->info & HIDDEN) != 0)
		    continue;
		Position *p = &network->position;

		// See the pick is in this network
		float nx = fx - p->position[X];
		float ny = fy - p->position[Y];
		if (nx < p->extent.getX()
		    || nx > p->extent.getX() + p->extent.getWidth()
		    || ny < p->extent.getY()
		    || ny > p->extent.getY() + p->extent.getHeight())
		    continue;

		// Check nodes
		Node *node = network->checkPick();
		if (node != 0) {
		    netlook->setPickedNode(node);
		    render(network, node);
		} else {
		    if (nx * nx + ny * ny > network->radius * network->radius)
			continue;
		    netlook->setPickedNetwork(network);
		    render(network);
		}
		hit = True;
		break;
	    }
	}
    }
}

void
NetLookGadget::startAdjust(int, int, unsigned int mask)
{
#ifdef DO_PICK
    adjustType = aoNone;
    adjustPick = 0;
    float sv = ev->getScaleValue();
    Rectf v = ev->getViewPort();
    float fx = x * sv + v.getX();
    float fy = v.getHeight() - y * sv + v.getY();

    NetworkNode *n;
    SegmentNode *s;
    Network *network = 0;
    for (n = (NetworkNode *) netlook->getDatabase()->child;
		n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    network = (Network *) s->data;
	    if ((network->info & HIDDEN) != 0)
		continue;

	    // See the pick is in this network
	    Position *p = &network->position;
	    float nx = fx - p->position[X];
	    float ny = fy - p->position[Y];
	    if (nx < p->extent.getX()
		|| nx > p->extent.getX() + p->extent.getWidth()
		|| ny < p->extent.getY()
		|| ny > p->extent.getY() + p->extent.getHeight()) {
		adjustPrev = network;
		continue;
	    }

	    // Check if hit a node
	    Node *node = network->checkPick();
	    if (node != 0) {
		adjustPick = node;
		adjustNext = getNextUnhiddenNode(node);
		if (adjustNext == node)
		    return;
		adjustPrev = getPrevUnhiddenNode(node);
		adjustType = aoNode;
		adjusting = True;
		node->info |= ADJUST;
		render(node->getNetwork(), node);
		return;
	    }

	    // Check within the radius of the network
	    if (nx * nx + ny * ny > network->radius * network->radius) {
		adjustPrev = network;
		continue;
	    }

	    // Hit network
	    adjustPick = network;
	    if (mask == 0) {
		// Adjust networks
		adjustNext = getNextUnhiddenNetwork((Network *) adjustPick);
		if (adjustNext == adjustPick)
		    return;
		adjustPrev = getPrevUnhiddenNetwork((Network *) adjustPick);
		adjustType = aoNetwork;
		adjusting = True;
		for (s = (SegmentNode *) n->child;
			s != 0; s = (SegmentNode *) s->next) {
		    network = (Network *) s->data;
		    if ((network->info & HIDDEN) == 0) {
			network->info |= ADJUST;
			render(network);
		    }
		}
	    } else {
		// Adjust segments of a network
		adjustNext = getNextUnhiddenSegment((Network *) adjustPick);
		if (adjustNext == adjustPick)
		    return;
		adjustPrev = getPrevUnhiddenSegment((Network *) adjustPick);
		adjustType = aoSegment;
		adjusting = True;
		network->info |= ADJUST;
		render(network);
	    }
	    return;
	}
    }
#else
    switch (netlook->getPickedObjectType()) {
	case pkNone:
	case pkConnection:
	    return;
	case pkNetwork:
	    adjustPick = netlook->getPickedNetwork();
	    if (mask == 0) {
		// Adjust networks
		adjustNext = getNextUnhiddenNetwork((Network *) adjustPick);
		if (adjustNext == adjustPick)
		    return;
		adjustPrev = getPrevUnhiddenNetwork((Network *) adjustPick);
		adjustType = aoNetwork;
		adjusting = True;
		SegmentNode *s = ((Network *) adjustPick)->segment;
		for (s = (SegmentNode *) s->parent->child;
			s != 0; s = (SegmentNode *) s->next) {
		    Network *network = (Network *) s->data;
		    if ((network->info & HIDDEN) == 0) {
			network->info |= ADJUST;
			render(network);
		    }
		}
	    } else {
		// Adjust segments of a network
		adjustNext = getNextUnhiddenSegment((Network *) adjustPick);
		if (adjustNext == adjustPick)
		    return;
		adjustPrev = getPrevUnhiddenSegment((Network *) adjustPick);
		adjustType = aoSegment;
		adjusting = True;
		((Network *) adjustPick)->info |= ADJUST;
		render((Network *) adjustPick);
	    }
	    return;
	case pkNode:
	    adjustPick = netlook->getPickedNode();
	    adjustNext = getNextUnhiddenNode((Node *) adjustPick);
	    if (adjustNext == adjustPick)
	        return;
	    adjustPrev = getPrevUnhiddenNode((Node *) adjustPick);
	    adjustType = aoNode;
	    adjusting = True;
	    ((Node *) adjustPick)->info |= ADJUST;
	    render(((Node *) adjustPick)->getNetwork(), (Node *) adjustPick);
	    return;
    }
#endif
}

void
NetLookGadget::adjust(int x, int y)
{
    float sv = ev->getScaleValue();
    Rectf v = ev->getViewPort();
    float fx = x * sv + v.getX();
    float fy = v.getHeight() - y * sv + v.getY();

    tuBool good = False;
    float *r, *prev, *next;
    switch (adjustType) {
	case aoNone:
	    return;
	case aoNetwork:
	case aoSegment:
	    r = ((Network *) adjustPick)->position.position;
	    prev = ((Network *) adjustPrev)->position.position;
	    next = ((Network *) adjustNext)->position.position;
	    break;
	case aoNode:
	    r = ((Node *) adjustPick)->position.position;
	    prev = ((Node *) adjustPrev)->position.position;
	    next = ((Node *) adjustNext)->position.position;
	    fx -= ((Node *) adjustPick)->getNetwork()->position.position[X];
	    fy -= ((Node *) adjustPick)->getNetwork()->position.position[Y];
	    break;
    }

    if (prev[X] < r[X]) {
	if (fx < prev[X])
	    good = True;
    } else {
	if (fx > prev[X])
	    good = True;
    }
    if (good) {
	if (prev[Y] < r[Y]) {
	    if (fy > prev[Y])
		good = False;
	} else {
	    if (fy < prev[Y])
		good = False;
	}
    }
    if (good) {
	switch (adjustType) {
	    case aoNone:
		return;
	    case aoNetwork:
		swapNetwork((Network *) adjustPrev, (Network *) adjustPick);
		adjustNext = adjustPrev;
		adjustPrev = getPrevUnhiddenNetwork((Network *) adjustPick);
		break;
	    case aoSegment:
		swapSegment((Network *) adjustPrev, (Network *) adjustPick);
		adjustNext = adjustPrev;
		adjustPrev = getPrevUnhiddenSegment((Network *) adjustPick);
		break;
	    case aoNode:
		swapNode((Node *) adjustPrev, (Node *) adjustPick);
		adjustNext = adjustPrev;
		adjustPrev = getPrevUnhiddenNode((Node *) adjustPick);
		break;
	}
	return;
    }

    if (next[X] < r[X]) {
	if (fx > next[X])
	    return;
    } else {
	if (fx < next[X])
	    return;
    }
    if (next[Y] < r[Y]) {
	if (fy > next[Y])
	    return;
    } else {
	if (fy < next[Y])
	    return;
    }
    switch (adjustType) {
	case aoNone:
	    return;
	case aoNetwork:
	    swapNetwork((Network *) adjustPick, (Network *) adjustNext);
	    adjustPrev = adjustNext;
	    adjustNext = getNextUnhiddenNetwork((Network *) adjustPick);
	    break;
	case aoSegment:
	    swapSegment((Network *) adjustPick, (Network *) adjustNext);
	    adjustPrev = adjustNext;
	    adjustNext = getNextUnhiddenSegment((Network *) adjustPick);
	    break;
	case aoNode:
	    swapNode((Node *) adjustPick, (Node *) adjustNext);
	    adjustPrev = adjustNext;
	    adjustNext = getNextUnhiddenNode((Node *) adjustPick);
	    break;
    }
    adjust(x, y);
}

void
NetLookGadget::stopAdjust(int x, int y)
{
    switch (adjustType) {
	case aoNone:
	    return;
	case aoNetwork:
	    {
		adjust(x, y);
		Network *network = (Network *) adjustPick;
	        for (SegmentNode *s =
			(SegmentNode *) network->segment->parent->child;
				s != 0; s = (SegmentNode *) s->next) {
		    network = (Network *) s->data;
		    if ((network->info & HIDDEN) == 0) {
			network->info &= ~ADJUST;
			render(network);
		    }
		}
	    }
	    break;
	case aoSegment:
	    adjust(x, y);
	    ((Network *) adjustPick)->info &= ~ADJUST;
	    render((Network *) adjustPick);
	    break;
	case aoNode:
	    adjust(x, y);
	    ((Node *) adjustPick)->info &= ~ADJUST;
	    render(((Node *) adjustPick)->getNetwork(), (Node *) adjustPick);
	    break;
    }
    adjusting = False;
    adjustPrev = 0;
    adjustPick = 0;
    adjustNext = 0;
}

Node *
NetLookGadget::getNextUnhiddenNode(Node *node)
{
    InterfaceNode *i;
    for (i = (InterfaceNode *) node->interface->next;
		i != 0; i = (InterfaceNode *) i->next) {
	if ((((Node *) i->data)->info & HIDDEN) == 0)
	    return (Node *) i->data;
    }

    // Wrap to beginning
    for (i = (InterfaceNode *) node->getSegment()->child;
		i != node->interface; i = (InterfaceNode *) i->next) {
	if ((((Node *) i->data)->info & HIDDEN) == 0)
	    return (Node *) i->data;
    }

    return node;
}

Node *
NetLookGadget::getPrevUnhiddenNode(Node *node)
{
    InterfaceNode *i;
    for (i = (InterfaceNode *) node->interface->prev;
		i != 0; i = (InterfaceNode *) i->prev) {
	if ((((Node *) i->data)->info & HIDDEN) == 0)
	    return (Node *) i->data;
    }

    // Wrap to end
    for (i = node->interface; i->next != 0; i = (InterfaceNode *) i->next)
	;
    for ( ; i != node->interface; i = (InterfaceNode *) i->prev) {
	if ((((Node *) i->data)->info & HIDDEN) == 0)
	    return (Node *) i->data;
    }

    return node;
}

void
NetLookGadget::swapNetwork(Network *n1, Network *n2)
{
    swap(n1->segment->parent, n2->segment->parent);
    position();
    render();
    glmapgadget->render();
    winset();
}

void
NetLookGadget::swapSegment(Network *n1, Network *n2)
{
    swap(n1->segment, n2->segment);
    position();
    render();
    glmapgadget->render();
    winset();
}

void
NetLookGadget::swapNode(Node *n1, Node *n2)
{
    swap(n1->interface, n2->interface);
    n1->getNetwork()->place();
    render();
    glmapgadget->render();
    winset();
}

void
NetLookGadget::swap(CfNode *n1, CfNode *n2)
{
    if (n1->next == 0) {
	if (n1->prev == n2) {
	    n1->next = n2;
	    n2->prev = n1;
	    n2->parent->child = n1;
	    n2->next = 0;
	    n1->prev = 0;
	} else {
	    n1->prev->next = n2;
	    n2->next->prev = n1;
	    n2->prev = n1->prev;
	    n1->prev = 0;
	    n1->next = n2->next;
	    n2->next = 0;
	    n2->parent->child = n1;
	}
    } else {
	if (n1->prev != 0)
	    n1->prev->next = n2;
	else
	    n1->parent->child = n2;
	if (n2->next != 0)
	    n2->next->prev = n1;
	n1->next = n2->next;
	n2->prev = n1->prev;
	n1->prev = n2;
	n2->next = n1;
    }
}

Network *
NetLookGadget::getNextUnhiddenNetwork(Network *network)
{
    NetworkNode *n;
    SegmentNode *s;
    for (n = (NetworkNode *) network->segment->parent->next;
		n != 0; n = (NetworkNode *) n->next) {
	// Traverse segment list backwards
	if (n->child == 0)
	    continue;
	for (s = (SegmentNode *) n->child;
		s->next != 0; s = (SegmentNode *) s->next)
	    ;
	for ( ; s != 0; s = (SegmentNode *) s->prev) {
	    if ((((Network *) s->data)->info & HIDDEN) == 0)
		return (Network *) s->data;
	}
    }

    // Wrap to beginning
    for (n = (NetworkNode *) netlook->getDatabase()->child;
		n != (NetworkNode *) network->segment->parent;
			n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    if ((((Network *) s->data)->info & HIDDEN) == 0)
		return (Network *) s->data;
	}
    }

    return network;
}

Network *
NetLookGadget::getPrevUnhiddenNetwork(Network *network)
{
    NetworkNode *n;
    SegmentNode *s;
    for (n = (NetworkNode *) network->segment->parent->prev;
		n != 0; n = (NetworkNode *) n->prev) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    if ((((Network *) s->data)->info & HIDDEN) == 0)
		return (Network *) s->data;
	}
    }

    // Wrap to end
    for (n = (NetworkNode *) network->segment->parent;
		n->next != 0; n = (NetworkNode *) n->next)
	;
    for ( ; n != (NetworkNode *) network->segment->parent;
		n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    if ((((Network *) s->data)->info & HIDDEN) == 0)
		return (Network *) s->data;
	}
    }

    return network;
}

Network *
NetLookGadget::getNextUnhiddenSegment(Network *network)
{
    SegmentNode *s;
    for (s = (SegmentNode *) network->segment->next;
		s != 0; s = (SegmentNode *) s->next) {
	if ((((Network *) s->data)->info & HIDDEN) == 0)
	    return (Network *) s->data;
    }

    // Wrap to beginning
    for (s = (SegmentNode *) network->segment->parent->child;
		s != network->segment; s = (SegmentNode *) s->next) {
	if ((((Network *) s->data)->info & HIDDEN) == 0)
	    return (Network *) s->data;
    }

    return network;
}

Network *
NetLookGadget::getPrevUnhiddenSegment(Network *network)
{
    SegmentNode *s;
    for (s = (SegmentNode *) network->segment->prev;
		s != 0; s = (SegmentNode *) s->prev) {
	if ((((Network *) s->data)->info & HIDDEN) == 0)
	    return (Network *) s->data;
    }

    // Wrap to end
    for (s = (SegmentNode *) network->segment;
		s->next != 0; s = (SegmentNode *) s->next)
	;
    for ( ; s != network->segment; s = (SegmentNode *) s->prev) {
	if ((((Network *) s->data)->info & HIDDEN) == 0)
	    return (Network *) s->data;
    }

    return network;
}
