/* -*- C++ -*- */

#ifndef _INV_COLSCALEMOD_H
#define _INV_COLSCALEMOD_H

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

#ident "$Id: ColScaleMod.h,v 1.1 1997/08/20 05:05:15 markgw Exp $"

#include <iostream.h>
#include "ColorScale.h"
#include "Modulate.h"

class SoBaseColor;
class SoScale;
class SoNode;
class INV_Launch;

class INV_ColorScaleMod : public INV_Modulate
{
private:

    State		_state;
    INV_ColorScale	_colScale;
    SoScale		*_scale;
    float		_xScale;
    float		_yScale;
    float		_zScale;
    SoBaseColor		*_color;    

public:

    virtual ~INV_ColorScaleMod();

    INV_ColorScaleMod(const char *metric, double scale, 
		      const INV_ColorScale &colors, SoNode *obj,
		      float xScale, float yScale, float zScale);

    virtual void refresh(OMC_Bool fetchFlag);

    virtual uint_t select(SoPath *);
    virtual uint_t remove(SoPath *);

    virtual void infoText(OMC_String &str, OMC_Bool) const;

    virtual void launch(INV_Launch &launch, OMC_Bool) const;

    virtual void dump(ostream &) const;

private:

    INV_ColorScaleMod();
    INV_ColorScaleMod(const INV_ColorScaleMod &);
    const INV_ColorScaleMod &operator=(const INV_ColorScaleMod &);
    // Never defined
};

#endif /* _INV_COLSCALEMOD_H */
