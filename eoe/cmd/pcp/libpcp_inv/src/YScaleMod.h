/* -*- C++ -*- */

#ifndef _INV_YSCALEMOD_H_
#define _INV_YSCALEMOD_H_

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

#ident "$Id: YScaleMod.h,v 1.1 1997/08/20 05:13:44 markgw Exp $"

#include <iostream.h>
#include "ScaleMod.h"

class SoBaseColor;
class SoScale;
class SoNode;
class INV_Launch;

class INV_YScaleMod : public INV_ScaleMod
{
public:

    virtual ~INV_YScaleMod();

    INV_YScaleMod(const char *metric, double scale, const SbColor &color,
		  SoNode *obj);

    virtual void dump(ostream &) const;

private:

    INV_YScaleMod();
    INV_YScaleMod(const INV_YScaleMod &);
    const INV_YScaleMod &operator=(const INV_YScaleMod &);
    // Never defined
};

#endif /* _INV_YSCALEMOD_H_ */

