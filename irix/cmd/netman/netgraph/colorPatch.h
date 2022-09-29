#ifndef __colorPatch_h_
#define __colorPatch_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Color Patch (single GL color square for editControl)
 *
 *	$Revision: 1.1 $
 *	$Date: 1992/09/27 21:27:04 $
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
#include <tuGadget.h>
#include <tuScrollBar.h>
#include <tuTopLevel.h>

#include "boxes.h"

class NetGraph;
class StripGadget;


class ColorPatch : public tuGLGadget {
public:
    ColorPatch(tuGadget *parent, const char* instanceName, int colorIdx = 4);
    
    void    bindResources(tuResourceDB * = NULL, tuResourcePath * = NULL);
    void    render();
    void    getLayoutHints(tuLayoutHints *);

    void    setColor(int c)	    { colorIndex = c; render(); }
    int	    getColor()		    { return colorIndex; }

private:
    int	    colorIndex;

};

#endif /* __colorPatch_h_ */
