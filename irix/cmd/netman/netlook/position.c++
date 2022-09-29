/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Positioning
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

#include <tuRect.h>
#include "position.h"

Rectf::Rectf(void)
{
    set(0, 0, 0, 0);
}

Rectf::Rectf(float x0, float y0, float w, float h)
{
    set(x0, y0, w, h);
}

Rectf &
Rectf::operator=(const Rectf &c)
{
    x = c.x;
    y = c.y;
    width = c.width;
    height = c.height;
    return *this;
}

Rectf &
Rectf::operator=(tuRect &c)
{
    x = c.getX();
    y = c.getY();
    width = c.getWidth();
    height = c.getHeight();
    return *this;
}

void
Rectf::set(float x0, float y0, float w, float h)
{
    x = x0;
    y = y0;
    width = w;
    height = h;
}
