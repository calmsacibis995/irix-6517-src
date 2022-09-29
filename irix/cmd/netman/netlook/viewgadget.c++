/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	View Gadget
 *
 *	$Revision: 1.3 $
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
#include <tuGLGadget.h>
#include <tuScrollBar.h>
#include <tuUnPickle.h>
#include <gl/gl.h>
#include <macros.h>
#include <stdio.h>
#include "viewgadget.h"
#include "netlookgadget.h"
#include "glmapgadget.h"
#include "menugadget.h"
#include "viewgadgetlayout.h"

static const float minScaleValue = 0.5;
static const float pages = 5.0;
static const float lines = pages * 10.0;

tuDeclareCallBackClass(ViewGadgetCallBack, ViewGadget);
tuImplementCallBackClass(ViewGadgetCallBack, ViewGadget);

// XXX Can this go away
ViewGadget *viewgadget;
extern GLMapGadget *glmapgadget;

ViewGadget::ViewGadget(NetLook *nl, tuGadget *parent, const char *instanceName)
{
    // Store name
    name = instanceName;

    (new ViewGadgetCallBack(this, ViewGadget::moveX))->
						registerName("View_moveX");
    (new ViewGadgetCallBack(this, ViewGadget::pageIncX))->
						registerName("View_pageIncX");
    (new ViewGadgetCallBack(this, ViewGadget::pageDecX))->
						registerName("View_pageDecX");
    (new ViewGadgetCallBack(this, ViewGadget::incX))->
						registerName("View_incX");
    (new ViewGadgetCallBack(this, ViewGadget::decX))->
						registerName("View_decX");
    (new ViewGadgetCallBack(this, ViewGadget::moveY))->
						registerName("View_moveY");
    (new ViewGadgetCallBack(this, ViewGadget::pageIncY))->
						registerName("View_pageIncY");
    (new ViewGadgetCallBack(this, ViewGadget::pageDecY))->
						registerName("View_pageDecY");
    (new ViewGadgetCallBack(this, ViewGadget::incY))->
						registerName("View_incY");
    (new ViewGadgetCallBack(this, ViewGadget::decY))->
						registerName("View_decY");
    (new ViewGadgetCallBack(this, ViewGadget::moveZ))->
						registerName("View_moveZ");
    (new ViewGadgetCallBack(this, ViewGadget::pageIncZ))->
						registerName("View_pageIncZ");
    (new ViewGadgetCallBack(this, ViewGadget::pageDecZ))->
						registerName("View_pageDecZ");
    (new ViewGadgetCallBack(this, ViewGadget::incZ))->
						registerName("View_incZ");
    (new ViewGadgetCallBack(this, ViewGadget::decZ))->
						registerName("View_decZ");

    // Unpickle the UI
    TUREGISTERGADGETS;
    gadget = tuUnPickle::UnPickle(parent, layoutstr);

    // Put in the NetLook gadget
    tuGadget *p = gadget->findGadget("GLparent");
    if (p == 0) {
	fprintf(stderr, "No GLparent!\n");
	exit(-1);
    }
    nlg = new NetLookGadget(nl, p, "netlook");
    nlg->setEnclosingView(this);

    // Put in the menu bar
    p = gadget->findGadget("menuparent");
    if (p == 0) {
	fprintf(stderr, "No menuparent!\n");
	exit(-1);
    }
    menu = new MenuGadget(nl, p, "menu");

    // Find scroll bars
    scrollX = (tuScrollBar *) gadget->findGadget("ScrollX");
    if (scrollX == 0) {
	fprintf(stderr, "No ScrollX\n");
	exit(-1);
    }
    scrollY = (tuScrollBar *) gadget->findGadget("ScrollY");
    if (scrollX == 0) {
	fprintf(stderr, "No ScrollY\n");
	exit(-1);
    }
    scrollZ = (tuScrollBar *) gadget->findGadget("ScrollZ");
    if (scrollX == 0) {
	fprintf(stderr, "No ScrollZ\n");
	exit(-1);
    }

    // Store pointer
    viewgadget = this;
}

void
ViewGadget::setViewPort(void)
{
    setViewPort(nlg->getUniverse());
}

void
ViewGadget::setViewPort(const Rectf &v)
{
    view = v;
    adjustViewPort();
}

void
ViewGadget::setViewPort(tuRect &v)
{
    view = v;
    adjustViewPort();
}

void
ViewGadget::setViewPort(float x, float y, float w, float h)
{
    view.set(x, y, w, h);
    adjustViewPort();
}

// Protected
void
ViewGadget::adjustViewPort(void)
{
    nlg->winset();
    viewport(0, nlg->getWidth() - 1, 0, nlg->getHeight() - 1);
    ortho(view.getX() + 0.5, view.getX() + view.getWidth() + 0.5,
	  view.getY() + 0.5, view.getY() + view.getHeight() + 0.5,
	  0.0, 65536.0);
    recalculateScrollBar();
}

void
ViewGadget::recalculateScrollBar(void)
{
    Rectf u = nlg->getUniverse();

    float fx = u.getWidth() / nlg->getWidth();
    float fy = u.getHeight() / nlg->getHeight();
    fx = MAX(fx, fy);
    scrollZ->setRange(fx, minScaleValue);
    scrollZ->setPercentShown(minScaleValue / fx);
    fx = view.getWidth() / nlg->getWidth();
    fy = view.getHeight() / nlg->getHeight();
    scaleValue = MIN(fx, fy);
    scrollZ->setPosition(scaleValue);
    adjustScrollBar();
}

// Protected
void
ViewGadget::adjustScrollBar(void)
{
    Rectf u = nlg->getUniverse();
    float maxz = scrollZ->getLowerBound();

    // X
    float f = nlg->getWidth() * maxz;
    scrollX->setRange((-f - view.getWidth()) / 2.0,
		      (f - view.getWidth()) / 2.0);
    scrollX->setPercentShown(minScaleValue * view.getWidth() / f);
    scrollX->setPosition(view.getX());

    // Y
    f = nlg->getHeight() * maxz;
    scrollY->setRange((f - view.getHeight()) / 2.0,
		      (-f - view.getHeight()) / 2.0);
    scrollY->setPercentShown(minScaleValue * view.getHeight() / f);
    scrollY->setPosition(view.getY());
}

void
ViewGadget::moveX(tuGadget *g)
{
    nlg->winset();
    view.setX(((tuScrollBar *) g)->getPosition());
    ortho(view.getX() + 0.5, view.getX() + view.getWidth() + 0.5,
	  view.getY() + 0.5, view.getY() + view.getHeight() + 0.5,
	  0.0, 65535.0);
    nlg->render();
    glmapgadget->render();
}

void
ViewGadget::pageIncX(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / pages;
    float p = s->getPosition() + d;
    if (p > s->getUpperBound())
	p = s->getUpperBound();
    s->setPosition(p);
    moveX(g);
}

void
ViewGadget::pageDecX(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / pages;
    float p = s->getPosition() - d;
    if (p < s->getLowerBound())
	p = s->getLowerBound();
    s->setPosition(p);
    moveX(g);
}

void
ViewGadget::incX(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / lines;
    float p = s->getPosition() + d;
    if (p > s->getUpperBound())
	p = s->getUpperBound();
    s->setPosition(p);
    moveX(g);
}

void
ViewGadget::decX(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / lines;
    float p = s->getPosition() - d;
    if (p < s->getLowerBound())
	p = s->getLowerBound();
    s->setPosition(p);
    moveX(g);
}

void
ViewGadget::moveY(tuGadget *g)
{
    nlg->winset();
    view.setY(((tuScrollBar *) g)->getPosition());
    ortho(view.getX() + 0.5, view.getX() + view.getWidth() + 0.5,
	  view.getY() + 0.5, view.getY() + view.getHeight() + 0.5,
	  0.0, 65535.0);
    nlg->render();
    glmapgadget->render();
}

void
ViewGadget::pageIncY(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / pages;
    float p = s->getPosition() + d;
    if (p > s->getLowerBound())
	p = s->getLowerBound();
    s->setPosition(p);
    moveY(g);
}

void
ViewGadget::pageDecY(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / pages;
    float p = s->getPosition() - d;
    if (p < s->getUpperBound())
	p = s->getUpperBound();
    s->setPosition(p);
    moveY(g);
}

void
ViewGadget::incY(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / lines;
    float p = s->getPosition() + d;
    if (p > s->getLowerBound())
	p = s->getLowerBound();
    s->setPosition(p);
    moveY(g);
}

void
ViewGadget::decY(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / lines;
    float p = s->getPosition() - d;
    if (p < s->getUpperBound())
	p = s->getUpperBound();
    s->setPosition(p);
    moveY(g);
}

void
ViewGadget::moveZ(tuGadget *g)
{
    nlg->winset();
    scaleValue = ((tuScrollBar *) g)->getPosition();
    float fx = nlg->getWidth() * scaleValue - view.getWidth();
    float fy = nlg->getHeight() * scaleValue - view.getHeight();
    view.set(view.getX() - fx / 2.0, view.getY() - fy / 2.0,
	     view.getWidth() + fx, view.getHeight() + fy);
    ortho(view.getX() + 0.5, view.getX() + view.getWidth() + 0.5,
	  view.getY() + 0.5, view.getY() + view.getHeight() + 0.5,
	  0.0, 65535.0);
    adjustScrollBar();
    nlg->render();
    glmapgadget->render();
}

void
ViewGadget::pageIncZ(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / pages;
    float p = s->getPosition() + d;
    if (p > s->getLowerBound())
	p = s->getLowerBound();
    s->setPosition(p);
    moveZ(g);
}

void
ViewGadget::pageDecZ(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / pages;
    float p = s->getPosition() - d;
    if (p < s->getUpperBound())
	p = s->getUpperBound();
    s->setPosition(p);
    moveZ(g);
}

void
ViewGadget::incZ(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / lines;
    float p = s->getPosition() + d;
    if (p > s->getLowerBound())
	p = s->getLowerBound();
    s->setPosition(p);
    moveZ(g);
}

void
ViewGadget::decZ(tuGadget *g)
{
    tuScrollBar *s = (tuScrollBar *) g;
    float d = (s->getUpperBound() - s->getLowerBound()) / lines;
    float p = s->getPosition() - d;
    if (p < s->getUpperBound())
	p = s->getUpperBound();
    s->setPosition(p);
    moveZ(g);
}

float
ViewGadget::getMinScaleValue(void)
{
    return minScaleValue;
}

void
ViewGadget::setScaleValue(float sv)
{
    if (sv >= minScaleValue && sv < scrollZ->getLowerBound()) {
	scrollZ->setPosition(sv);
	moveZ(scrollZ);
    }
}
