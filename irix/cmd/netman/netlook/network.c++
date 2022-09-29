/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Network Segments
 *
 *	$Revision: 1.14 $
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
#include <gl/gl.h>
#include <string.h>
#include <math.h>

#include "trig.h"
#include "cf.h"
#include "db.h"
#include "netlook.h"
#include "node.h"
#include "network.h"
#include "conntable.h"
#include "hidegadget.h"

#define DEFAULTNETSIZE	100	/* Default radius of a network		*/
#define NETGROWRATE	10	/* Increase in radius when network grows*/
#define MINHOSTDIST	15.0	/* Minimum distance between nodes       */
#define BORDERDEPTH	100.0	/* How far back the border is		*/

static struct in_addr thisNet, thisNetMask;

// XXX can this go away?
extern NetLook *netlook;
extern HideGadget *hidegadget;

Network::Network(void)
{
    float ns2 = DEFAULTNETSIZE * 2.0;
    position.extent.set(-DEFAULTNETSIZE, -DEFAULTNETSIZE, ns2, ns2);
    radius = DEFAULTNETSIZE;
    nnodes = 0;
    connections = 0;
    info = 0;
    display = 0;
    segment = 0;
}

Network::Network(SegmentNode *s)
{
    float ns2 = DEFAULTNETSIZE * 2.0;
    position.extent.set(-DEFAULTNETSIZE, -DEFAULTNETSIZE, ns2, ns2);
    radius = DEFAULTNETSIZE;
    nnodes = 0;
    connections = 0;
    info = 0;
    display = 0;
    add(s);
}

Network::~Network(void)
{
    if ((info & HIDDEN) != 0)
	hidegadget->remove(this);
    if (display != 0)
	delete display;
    if (connections != 0)
	netlook->getConnectionTable()->clear();
}

void
Network::add(SegmentNode *s)
{
    segment = s;
    updateDisplayString();
}

void
Network::render(unsigned int complex)
{
    pushmatrix();

    translate(position.position[X], position.position[Y], -BORDERDEPTH);

    if (netlook->getNetShow() == ACTIVE
	&& connections == 0
	&& (info & (NODEPICKED | CONNPICKED)) == 0) {
	if (info & NETWORKPICKED)
	    color(netlook->getSelectColor());
	else if (info & ADJUST)
	    color(netlook->getAdjustColor());
	else
	    color(netlook->getInactiveNetworkColor());
	circ(0.0, 0.0, radius);
	translate(0.0, 0.0, BORDERDEPTH);
	if (display != 0) {
	    if (complex != 0)
		DrawStringCenterCenter(display);
	    else {
		float w = (float) StringWidth(display) / 2.0;
		move2(-w, 0.0);
	        draw2(w, 0.0);
	    }
	}
	popmatrix();
	return;
    }
		
    if (info & NETWORKPICKED)
	color(netlook->getSelectColor());
    else if (info & ADJUST)
        color(netlook->getAdjustColor());
    else
	color(netlook->getActiveNetworkColor());
    circ(0.0, 0.0, radius);
    translate(0.0, 0.0, BORDERDEPTH);
    if (display != 0) {
	if (complex != 0)
	    DrawStringCenterCenter(display);
	else {
	    float w = (float) StringWidth(display) / 2.0;
	    move2(-w, 0.0);
	    draw2(w, 0.0);
	}
    }

    // Render the nodes
    for (InterfaceNode *i = (InterfaceNode *) segment->child;
		i != 0; i = (InterfaceNode *) i->next) {
	Node *node = (Node *) i->data;
	if ((node->info & HIDDEN) == 0)
	    node->render(complex);
    }

    popmatrix();
}

/*
 * Position nodes around a circle.  Nodes are displayed in list order.
 */
void
Network::place(void)
{
    unsigned int n, unhidden;
    Node *node;
    Position *pos;
    short hangle;
    float size,nsw,angle;

    // Find number of unhidden nodes
    unhidden = 0;
    for (InterfaceNode *i = (InterfaceNode *) segment->child;
		i != 0; i = (InterfaceNode *) i->next) {
	if ((((Node *) i->data)->info & HIDDEN) == 0)
	    unhidden++;
    }

startover:
    n = 0;

    /*
     * If the minimum angle between nodes has been exceeded, keep growing
     * the network until it's big enough.
     */
    while (1) {
	size = radius;
	angle = M_2PI / unhidden;
	if (((float) size * atanf(angle)) > MINHOSTDIST)
	    break;

	grow();
    }

    nsw = -position.extent.getX() - radius;

    for (i = (InterfaceNode *) segment->child;
		i != 0; i = (InterfaceNode *) i->next) {
	node = (Node *) i->data;
	if ((node->info & HIDDEN) != 0)
	    continue;

	/*
	 * If the string of this nodes is the longest in this network,
	 * then we have to reposition the networks so that the extents
	 * will not overlap.
	 */
	if (node->stringwidth > nsw) {
	    float ll = -radius - node->stringwidth;
	    float len = -(ll + ll);

	    position.extent.set(ll, ll, len, len);
	    netlook->position();
	    goto startover;
	}

	hangle = short(DEG(angle * n) * 10.0);
	hangle += hangle % 2;

	pos = &(node->position);
	pos->position[X] = -l_cos(hangle) * size;
	pos->position[Y] = l_sin(hangle) * size;

	if ((hangle > 900) && (hangle < 2700))
	    node->angle = 1800 - hangle;
	else
	    node->angle = -hangle;

	n++;
    }
}

void
Network::updateDisplayString(void)
{
    char *d = label();
    if (display != 0)
	delete display;
    if (d != 0) {
	display = new char[strlen(d) + 1];
	strcpy(display, d);
    }
}

char*
Network::label(void)
{
    static char s[64];
    char *n;

    switch (netlook->getNetDisplay()) {
	case DMFULLNAME:
	    n = segment->getName();
	    if (n != 0)
		return(n);
	    /* Fall through... */

	case DMINETADDR:
	    n = segment->ipnum.getString();
	    if (n != 0) {
		strcpy(s, n);
		return(s);
	    } else {
		n = segment->getName();
		if (n != 0)
		    return(n);
	    }
	    strcpy(s, "?");
	    return(s);

	default:
	    exit(-1);
	    //perror(-1, "Bad Display Mode");
    }
}

/*
 * Make a network bigger.
 */
void
Network::grow(void)
{
    float sw = -position.extent.getX() - radius;

    radius += NETGROWRATE;

    float ll = -radius - sw;
    float len = -(ll + ll);

    position.extent.set(ll, ll, len, len);
    netlook->position();
}

Node *
Network::checkPick(void)
{
    static const unsigned int pickBufSize = 16;
    static const unsigned int pickExtent = 2;

    if (netlook->getNetShow() == ACTIVE
	&& connections == 0
	&& !(info & NODEPICKED))
	return 0;

    unsigned short c = 0;
    short pickBuf[pickBufSize];
    Node **nodes = new Node *[nnodes];
    Node *n;
    Rectf v = netlook->getViewPort();

    initnames();
    picksize(pickExtent, pickExtent);
    pick(pickBuf, pickBufSize);

    ortho(v.getX() + 0.5, v.getX() + v.getWidth() + 0.5,
	  v.getY() + 0.5, v.getY() + v.getHeight() + 0.5,
	  0.0, 65535.0);
    translate(position.position[X], position.position[Y], 0.0);
    for (InterfaceNode *i = (InterfaceNode *) segment->child;
		i != 0; i = (InterfaceNode *) i->next) {
	n = (Node *) i->data;
	if (netlook->getNodeShow() == ACTIVE && n->connections == 0
	    && !(n->info & NODEPICKED))
	    continue;
	loadname(c);
	nodes[c++] = n;
	pushmatrix();
	translate(n->position.position[X], n->position.position[Y], 0.0);
	rotate(n->angle, 'Z');
	float w;
	if (n->position.position[X] > 0.0)
	    w = StringWidth(n->display);
	else
	    w = -StringWidth(n->display);
	float h = StringHeight(n->display) / 2.0;
	rectf(0.0, -h, w, h);
	popmatrix();
    }

    long writes = endpick(pickBuf);
    ortho(v.getX() + 0.5, v.getX() + v.getWidth() + 0.5,
	  v.getY() + 0.5, v.getY() + v.getHeight() + 0.5,
	  0.0, 65535.0);
    if (writes == 0 || pickBuf[0] == 0)
	n = 0;
    else
	n = nodes[pickBuf[1]];
    delete nodes;
    return n;
}
