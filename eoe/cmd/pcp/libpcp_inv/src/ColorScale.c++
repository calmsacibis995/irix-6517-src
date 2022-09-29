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

#ident "$Id: ColorScale.c++,v 1.2 1997/09/25 06:39:45 markgw Exp $"

#include <iostream.h>
#include "pmapi.h"
#include "impl.h"
#include "Inv.h"
#include "ColorScale.h"

INV_ColorStep::~INV_ColorStep()
{
}

INV_ColorStep::INV_ColorStep(SbColor col, float val)
: _color(), 
  _min(val)
{
    _color.setValue(col.getValue());
}

INV_ColorStep::INV_ColorStep(float r, float g, float b, float val)
: _color(), 
  _min(val)
{
    SbColor tmp(r, g, b);
    _color.setValue(tmp.getValue());
}

INV_ColorStep::INV_ColorStep(uint32_t col, float val)
: _color(), 
  _min(val)
{
    float dummy = 0.0;
    _color.setPackedValue(col, dummy);
}

INV_ColorStep::INV_ColorStep(const INV_ColorStep &rhs)
: _color(), 
  _min(rhs._min)
{
    _color.setValue(rhs._color.getValue());
}

const INV_ColorStep &
INV_ColorStep::operator=(const INV_ColorStep &rhs)
{
    if (this != &rhs) {
    	_color.setValue(rhs._color.getValue());
	_min = rhs._min;
    }
    return *this;
}

INV_ColorScale::~INV_ColorScale()
{
    uint_t	i;

    for (i = 0; i < _colors.length(); i++)
    	delete _colors[i];
    _colors.removeAll();
}

INV_ColorScale::INV_ColorScale(const SbColor &col)
: _colors()
{
    add(new INV_ColorStep(col));
}

INV_ColorScale::INV_ColorScale(float r, float g, float b)
: _colors()
{
    add(new INV_ColorStep(r, g, b));
}

INV_ColorScale::INV_ColorScale(uint32_t col)
: _colors()
{
    add(new INV_ColorStep(col));
}

INV_ColorScale::INV_ColorScale(const INV_ColorScale &rhs)
: _colors()
{
    uint_t	i;

    _colors.resize(rhs._colors.length());
    for (i = 0; i < rhs._colors.length(); i++)
    	add(new INV_ColorStep(rhs[i]));
}

const INV_ColorScale &
INV_ColorScale::operator=(const INV_ColorScale &rhs)
{
    uint_t	i;

    if (this != &rhs) {
    	for (i = 0; i < _colors.length(); i++)
	    delete _colors[i];
	_colors.removeAll();
	for (i = 0; i < rhs._colors.length(); i++)
	    add(new INV_ColorStep(rhs[i]));
    }
    return *this;
}

int
INV_ColorScale::add(INV_ColorStep *ptr)
{
    if (_colors.length()) {
    	float prev = _colors.tail()->min();
	if (prev >= ptr->min()) {
	    INV_warningMsg(_POS_, 
			   "Color step (%f) was less than previous step (%f), skipping.",
			   ptr->min(), prev);
	    return -1;
	}
    }
    _colors.append(ptr);

    return 0;
}

const INV_ColorStep &
INV_ColorScale::step(float value)
{
    uint_t	i = _colors.length();

    while (i > 0 && _colors[i-1]->min() > value)
    	i--;

    if (i == 0)
	return *(_colors[0]);
    return *(_colors[i-1]);
}

ostream&
operator<<(ostream &os, const INV_ColorScale &rhs)
{
    uint_t	i;

    if (rhs._colors.length() > 0) {
        os << '[' << rhs[0].min();
	for (i = 1; i < rhs.numSteps(); i++)
	    os << ", " << rhs[i].min();
	os << ']';
    }
    else {
    	os << "empty";
    }

    return os;
}

