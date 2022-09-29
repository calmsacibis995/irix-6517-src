/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Node class member functions
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

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <bstring.h>
#include <snoopstream.h>
#include "protocols/decnet.h"
}
#include <tuGLGadget.h>
#include <gl/gl.h>
#include "netlook.h"
#include "conntable.h"
#include "network.h"
#include "node.h"
#include "hidegadget.h"
#include "nis.h"

#define HOSTCOLOR	GREEN
#define SNOOPCOLOR	MAGENTA
#define GATEWAYCOLOR	CYAN
#define YPMASTERCOLOR	YELLOW
#define YPSERVERCOLOR	WHITE

#define FONTMINSIZE	0.2		/* Scale value to cheat at */

// XXX get rid of this
extern NetLook *netlook;
extern HideGadget *hidegadget;

Node::Node(void)
{
    display = NULL;
    timestamp = 0;
    info = 0;
    stringwidth = 0;
    connections = 0;
    interface = 0;
}

Node::Node(InterfaceNode *i)
{
    display = NULL;
    timestamp = 0;
    info = 0;
    stringwidth = 0;
    connections = 0;
    add(i);
}

Node::~Node(void)
{
    if ((info & SNOOPHOST) != 0)
    	netlook->stopSnoop(this);
    if ((info & HIDDEN) != 0)
    	hidegadget->remove(this);
    if (display != 0)
    	delete display;
    if (connections != 0)
	netlook->getConnectionTable()->clear();
}

void
Node::add(InterfaceNode *i)
{
    interface = i;
    YPCheck(this);
    getNetwork()->nnodes++;
    newInfo();
}

void
Node::render(unsigned int complex)
{
    if (netlook->getNodeShow() == ACTIVE
	&& connections == 0
	&& (info & (NODEPICKED | CONNPICKED)) == 0)
	return;

    pushmatrix();

    if (info & NODEPICKED)
	color(netlook->getSelectColor());
    else if (info & ADJUST)
	color(netlook->getAdjustColor());
    else if (info & SNOOPHOST)
	color(netlook->getActiveDataStationColor());
    else if (info & GATEWAY)
	color(netlook->getGatewayColor());
    else if (info & YPMASTER)
	color(netlook->getNISMasterColor());
    else if (info & YPSERVER)
	color(netlook->getNISSlaveColor());
    else
	color(netlook->getStandardNodeColor());

    translate(position.position[X], position.position[Y], 0.0);
    rotate(angle, 'Z');

    if (complex != 0) {
	if (position.position[X] <= 0.0)
	    DrawStringCenterRight(display);
	else
	    DrawStringCenterLeft(display);
    } else {
	move2(0.0, 0.0);
	if (position.position[X] <= 0.0)
	    draw2((Coord) -StringWidth(display), 0.0);
	else
	    draw2((Coord) StringWidth(display), 0.0);
    }

    popmatrix();
}

void
Node::newInfo(void)
{
    int maxstringlen = 0;

    for (int i = 1; i <= DMMAX; i++) {
	char* s = label(i);
	int l = StringWidth(s);
	if (l > stringwidth)
	    stringwidth = l;
	l = strlen(s);
	if (l > maxstringlen)
	    maxstringlen = l;
    }

    if (display != NULL) {
	if (maxstringlen > strlen(display)) {
	    delete display;
	    display = new char[maxstringlen + 1];
	}
    } else
	display = new char[maxstringlen + 1];

    updateDisplayString();
}

void
Node::updateDisplayString(void)
{
    strcpy(display, label());
}

char *
Node::label(unsigned int dm)
{
    static char s[64];
    char *n;

    if (dm == 0)
		dm = netlook->getNodeDisplay();
    switch (dm) {
	case DMFULLNAME:
	case DMNODENAME:
	    n = interface->getName();
	    if (n != NULL) {
		int c;

		if (dm == DMFULLNAME)
		    return n;

		c = strcspn(n, ".");
		strncpy(s, n, c);
		s[c] = '\0';
		return s;
	    }
	    /* Fall through... */

	case DMINETADDR:
	    n = interface->ipaddr.getString();
	    if (n != 0) {
		strcpy(s, n);
		return s;
	    }
	    /* Fall through... */

	case DMDECNETADDR:
	    n = interface->dnaddr.getString();
	    if (n != 0) {
				strcpy(s, n);
				return(s);
	    }
	    /* Fall through... */

	case DMVENDOR:
	    n = interface->physaddr.getString();
	    if (n == 0) {
		n = interface->ipaddr.getString();
		if (n == 0) {
		    n = interface->getName();
		    if (n == 0)
			return "error";
		}
		strcpy(s, n);
	    } else {
		char *v = ether_vendor((struct etheraddr *)
					interface->physaddr.getAddr());
		if (v == 0)
		    strcpy(s, n);
		else {
		    char t[12];
		    sscanf(n, "%*x:%*x:%*x:%s", t);
		    sprintf(s, "%s:%s", v, t);
		}
	    }
	    return s;

	case DMPHYSADDR:
	    n = interface->physaddr.getString();
	    if (n == 0) {
		n = interface->ipaddr.getString();
		if (n == 0) {
		    n = interface->getName();
		    if (n == 0)
			return "error";
		}
	    }
	    strcpy(s, n);
	    return s;

	default:
	    fprintf(stderr, "Bad node display mode\n");
	    exit(-1);
    }
}

int
Node::getSnoopSocket(void)
{
    return snoopstream == 0 ? -1 : snoopstream->ss_sock;
}
