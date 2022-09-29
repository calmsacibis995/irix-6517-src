/* -*- C++ -*- */

#ifndef _INV_COLSCALE_H
#define _INV_COLSCALE_H

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

#ident "$Id: ColorScale.h,v 1.1 1997/08/20 05:06:11 markgw Exp $"

#include <iostream.h>
#include <Inventor/SbColor.h>
#include "List.h"

class INV_ColorStep
{
private:

    SbColor	_color;
    float	_min;

public:

    ~INV_ColorStep();

    INV_ColorStep(SbColor col, float val = 0.0);
    INV_ColorStep(float r, float g, float b, float val = 0.0);
    INV_ColorStep(uint32_t col, float val = 0.0);
    INV_ColorStep(const INV_ColorStep &rhs);

    const INV_ColorStep &operator=(const INV_ColorStep &);

    const SbColor &color() const
    	{ return _color; }
    SbColor &color()
    	{ return _color; }

    const float &min() const
    	{ return _min; }
    float &min()
    	{ return _min; }
};

typedef OMC_List<INV_ColorStep *> INV_ColStepList;

class INV_ColorScale
{
private:

    INV_ColStepList	_colors;

public:

    ~INV_ColorScale();

    INV_ColorScale(const SbColor &col);
    INV_ColorScale(float r, float g, float b);
    INV_ColorScale(uint32_t col);
    INV_ColorScale(const INV_ColorScale &);
    const INV_ColorScale &operator=(const INV_ColorScale &);

    uint_t numSteps() const
	{ return _colors.length(); }

    int add(INV_ColorStep *ptr);

    const INV_ColorStep &operator[](int i) const
	{ return *(_colors[i]); }
    INV_ColorStep &operator[](int i)
	{ return *(_colors[i]); }

    const INV_ColorStep &step(float);

friend ostream& operator<<(ostream &os, const INV_ColorScale &rhs);

private:

    INV_ColorScale();
    // Not defined
};

#endif /* _INV_COLSCALE_H */

