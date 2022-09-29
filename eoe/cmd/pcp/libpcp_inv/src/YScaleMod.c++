/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: YScaleMod.c++,v 1.1 1997/08/20 05:13:28 markgw Exp $"

#include <iostream.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include "Inv.h"
#include "YScaleMod.h"
#include "ModList.h"
#include "Metric.h"
#include "Launch.h"

INV_YScaleMod::~INV_YScaleMod()
{
}

INV_YScaleMod::INV_YScaleMod(const char *str,
                     double scale,
                     const SbColor &color,
                     SoNode *obj)
: INV_ScaleMod(str, scale, color, obj, 0.0, 1.0, 0.0)
{
}

void
INV_YScaleMod::dump(ostream &os) const
{
    os << "INV_YScaleMod: ";

    if (status() < 0)
        os << "Invalid metric";
    else {
        os << "state = ";
	dumpState(os, _state);
        os << ", scale = " << _scale->scaleFactor.getValue()[1] << ": ";
        _metrics->metric(0).dump(os, OMC_true);
    }
}
