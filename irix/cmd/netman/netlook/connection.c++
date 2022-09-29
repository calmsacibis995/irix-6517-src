/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Connection class member functions
 *
 *	$Revision: 1.9 $
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
#include <gl.h>
#include <tuGLGadget.h>
#include <macros.h>
#include "connection.h"
#include "netlook.h"
#include "node.h"
#include "network.h"
#include "netlookgadget.h"

// XXX can this go away?
extern NetLook *netlook;
extern NetLookGadget *netlookgadget;

Connection::Connection()
{
    src.timestamp = src.intensity = src.packets = src.bytes = 0;
    dst.timestamp = dst.intensity = dst.packets = dst.bytes = 0;
    info = 0;
}

void
Connection::update()
{
    unsigned int oldsrc = src.intensity;
    unsigned int olddst = dst.intensity;

    calculate();
    if (src.intensity > oldsrc || dst.intensity > olddst) {
	netlookgadget->winset();
	pushmatrix();
	pushviewport();
	frontbuffer(TRUE);
	backbuffer(FALSE);

	render();

	frontbuffer(FALSE);
	backbuffer(TRUE);
	popviewport();
	popmatrix();
    }
}

void
Connection::calculate()
{
    // Do intensity calculations
    if (netlook->getTrafficBasis() == PACKETS) {
	src.intensity = src.packets / (netlook->getTrafficPacketColorStep()
				       * netlook->getTrafficRescaleValue());
	dst.intensity = dst.packets / (netlook->getTrafficPacketColorStep()
				       * netlook->getTrafficRescaleValue());
    } else {
	src.intensity = src.bytes / (netlook->getTrafficByteColorStep()
				     * netlook->getTrafficRescaleValue());
	dst.intensity = dst.bytes / (netlook->getTrafficByteColorStep()
				     * netlook->getTrafficRescaleValue());
    }
    unsigned int i = netlook->getTrafficColorValues();
    if (src.intensity >= i)
	src.intensity = i - 1;
    if (dst.intensity >= i)
	dst.intensity = i - 1;
}

void
Connection::render()
{
    p3f s, d;

    s[X] = src.node->getNetwork()->position.position[X]
	   + src.node->position.position[X];

    s[Y] = src.node->getNetwork()->position.position[Y]
	   + src.node->position.position[Y];

    d[X] = dst.node->getNetwork()->position.position[X]
	   + dst.node->position.position[X];

    d[Y] = dst.node->getNetwork()->position.position[Y]
	   + dst.node->position.position[Y];

    // XXX colors!
    if ((info & CONNPICKED) != 0) {
	color(netlook->getSelectColor());
	s[Z] = d[Z] = 0.0;
	bgnline();
	v3f(s);
	v3f(d);
	endline();
    } else {
	int n = netlook->getTrafficColorValues();
	s[Z] = (signed) src.intensity - n;
	d[Z] = (signed) dst.intensity - n;
	n = netlook->getTrafficMinColorValue();
	int a;
	if (n < netlook->getTrafficMaxColorValue())
	    a = 1;
	else
	    a = -1;
	bgnline();
	color(n + a * src.intensity);
	v3f(s);
	color(n + a * dst.intensity);
	v3f(d);
	endline();
    }
}
