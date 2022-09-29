/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	GL Tile Gadget
 *
 *	$Revision: 1.1 $
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
#include "gltilegadget.h"

GLTileGadget::GLTileGadget(tuGadget *parent, const char *instanceName)
		: tuGLGadget(parent, instanceName)
{
    tileColor[0] = 0;
    colors = 1;
}

void
GLTileGadget::bindResources(tuResourceDB *rdb, tuResourcePath *rp)
{
    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(False);
    setRGBmode(False);
    setZBuffer(False);
}

void
GLTileGadget::render(void)
{
    long v[4][2];
    v[0][0] = v[0][1] = v[1][0] = v[3][1] = 0;
    v[1][1] = v[2][1] = getHeight();
    v[2][0] = v[3][0] = getWidth();

    winset();
    switch (colors) {
	case 1:
	    color(tileColor[0]);
	    clear();
	    break;
	case 2:
	    bgnpolygon();
	    color(tileColor[0]);
	    v2i(v[0]);
	    v2i(v[1]);
	    color(tileColor[1]);
	    v2i(v[2]);
	    v2i(v[3]);
	    endpolygon();
	    break;
	case 4:
	    bgnpolygon();
	    color(tileColor[0]);
	    v2i(v[0]);
	    color(tileColor[1]);
	    v2i(v[1]);
	    color(tileColor[2]);
	    v2i(v[2]);
	    color(tileColor[3]);
	    v2i(v[3]);
	    endpolygon();
	    break;
    }
    if (getDoubleBuf())
	swapbuffers();
}

void
GLTileGadget::getLayoutHints(tuLayoutHints* h)
{
    tuGLGadget::getLayoutHints(h);
    h->prefWidth = 24;
    h->prefHeight = 24;
}

void
GLTileGadget::setColor(unsigned int c)
{
    tileColor[0] = c;
    colors = 1;
}

void
GLTileGadget::setColor(unsigned int c1, unsigned int c2)
{
    tileColor[0] = c1;
    tileColor[1] = c2;
    colors = 2;
}

void
GLTileGadget::setColor(unsigned int c1, unsigned int c2,
		       unsigned int c3, unsigned int c4)
{
    tileColor[0] = c1;
    tileColor[1] = c2;
    tileColor[2] = c3;
    tileColor[3] = c4;
    colors = 4;
}
