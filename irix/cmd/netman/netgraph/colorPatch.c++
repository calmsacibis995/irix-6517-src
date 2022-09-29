/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Color Patch (single GL color square for editControl)
 *
 *	$Revision: 1.1 $
 *	$Date: 1992/09/27 21:27:00 $
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

#include "colorPatch.h"

#include <gl/gl.h>
#include <gl/glws.h>
#include <stdio.h>


ColorPatch::ColorPatch(tuGadget *parent,  const char *instanceName, 
		int colorIdx)
	    : tuGLGadget(parent, instanceName) {
    colorIndex = colorIdx;

} // graphGadget


void
ColorPatch::bindResources(tuResourceDB *rdb, tuResourcePath *rp) {

    tuGLGadget::bindResources(rdb, rp);
    setDoubleBuf(True);
    setRGBmode(False);
    setZBuffer(False);
}


void
ColorPatch::getLayoutHints(tuLayoutHints* h) {
    tuGLGadget::getLayoutHints(h);
    h->prefWidth = 20;
    h->prefHeight = 20;
    h->flags |= tuLayoutHints_fixedHeight;
    h->flags |= tuLayoutHints_fixedWidth;

}


void
ColorPatch::render() {
    if (!isRenderable()) return;
    
    winset();

    color(colorIndex);
    clear();

    swapbuffers();

} // render

