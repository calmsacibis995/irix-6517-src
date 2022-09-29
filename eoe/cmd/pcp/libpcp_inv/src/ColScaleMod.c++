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

#ident "$Id: ColScaleMod.c++,v 1.3 1997/09/12 02:41:50 markgw Exp $"

#include <iostream.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoScale.h>
#include "Inv.h"
#include "ColScaleMod.h"
#include "ModList.h"
#include "Metric.h"
#include "Launch.h"

INV_ColorScaleMod::~INV_ColorScaleMod()
{
}

INV_ColorScaleMod::INV_ColorScaleMod(const char *metric, double scale,
				     const INV_ColorScale &colors, SoNode *obj,
				     float xScale, float yScale, float zScale)
: INV_Modulate(metric, scale), 
  _state(INV_Modulate::start),
  _colScale(colors),
  _scale(0),
  _xScale(xScale),
  _yScale(yScale),
  _zScale(zScale),
  _color(0)
{
    _root = new SoSeparator;
    _color = new SoBaseColor;

    _color->rgb.setValue(_errorColor.getValue());
    _root->addChild(_color);

    if (_metrics->numValues() == 1 && _colScale.numSteps() && status() >= 0) {

	_scale = new SoScale;
	_scale->scaleFactor.setValue(1.0, 1.0, 1.0);
	_root->addChild(_scale);
	_root->addChild(obj);

	add();
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_ColorScaleMod: Added " << metric << " (Id = " 
		 << _root->getName().getString() << ")" << endl;
#endif

    }
    else if (_metrics->numValues() > 1) {
	INV_warningMsg(_POS_, 
		       "Color modulated metric (%s) has more than one value (%d)",
		       metric, _metrics->numValues());
	_root->addChild(obj);
    }
    else if (status() < 0)
	_root->addChild(obj);
    else {
	INV_warningMsg(_POS_,
		       "No color steps for color modulated metric (%s)",
		       metric);
	_root->addChild(obj);
    }
}

void
INV_ColorScaleMod::refresh(OMC_Bool fetchFlag)
{
    OMC_Metric &metric = _metrics->metric(0);
    
    if (status() < 0)
	return;

    if (fetchFlag)
	metric.update();

    if (metric.error(0) <= 0) {
	if (_state != INV_Modulate::error) {
	    _color->rgb.setValue(_errorColor.getValue());
            _scale->scaleFactor.setValue((_xScale==0.0f ? 1.0 : theMinScale),
					 (_yScale==0.0f ? 1.0 : theMinScale),
					 (_zScale==0.0f ? 1.0 : theMinScale));
	    _state = INV_Modulate::error;
	}
    }
    else {
	double value = metric.value(0) * theScale;
	if (value > theNormError) {
	    if (_state != INV_Modulate::saturated) {
		_color->rgb.setValue(INV_Modulate::_saturatedColor);
		_scale->scaleFactor.setValue(1.0, 1.0, 1.0);
		_state = INV_Modulate::saturated;
	    }
	}
	else {
	    if (_state != INV_Modulate::normal)
		_state = INV_Modulate::normal;
	    _color->rgb.setValue(_colScale.step(value).color().getValue());
            if (value < INV_Modulate::theMinScale)
                value = INV_Modulate::theMinScale;
            else if (value > 1.0)
                value = 1.0;
            _scale->scaleFactor.setValue((_xScale==0.0f ? 1.0 : _xScale*value),
					 (_yScale==0.0f ? 1.0 : _yScale*value),
					 (_zScale==0.0f ? 1.0 : _zScale*value));
	}
    }
}

void
INV_ColorScaleMod::dump(ostream &os) const
{
    os << "INV_ColorScaleMod: ";
    if (status() < 0)
    	os << "Invalid Metric: " << pmErrStr(status()) << endl;
    else {
	os << "state = ";
	dumpState(os, _state);
	os << ", col scale = " << _colScale << ", y scale = "
	   << _scale->scaleFactor.getValue()[1]
	   << ": ";
        _metrics->metric(0).dump(os, OMC_true);
    }
}

void
INV_ColorScaleMod::infoText(OMC_String &str, OMC_Bool) const
{
    const OMC_Metric &metric = _metrics->metric(0);
    str = metric.spec(OMC_true, OMC_true, 0);
    str.appendChar('\n');
    if (_state == INV_Modulate::error)
	str.append(theErrorText);
    else if (_state == INV_Modulate::start)
	str.append(theStartText);
    else {
	str.appendReal(metric.realValue(0), 4);
	str.appendChar(' ');
	if (metric.units().length() > 0)
	    str.append(metric.units());
	str.append(" [");
	str.appendReal(metric.value(0) * 100.0 * theScale, 4);
	str.append("% of color scale]");
    }
}

void
INV_ColorScaleMod::launch(INV_Launch &launch, OMC_Bool) const
{
    if (status() < 0)
	return;

    launch.startGroup("point");
    launch.addMetric(_metrics->metric(0), _colScale, 0);
    launch.endGroup();
}

uint_t
INV_ColorScaleMod::select(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ColorScaleMod::select: " << _metrics->metric(0) << endl;
#endif
    return 1;
}

uint_t
INV_ColorScaleMod::remove(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ColorScaleMod::remove: " << _metrics->metric(0) << endl;
#endif
    return 0;
}
