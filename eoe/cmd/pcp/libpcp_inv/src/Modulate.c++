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

#ident "$Id: Modulate.c++,v 1.6 1997/11/28 03:10:56 markgw Exp $"

#include <iostream.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include "Modulate.h"
#include "ModList.h"
#include "Record.h"

double			theNormError = 1.05;

const OMC_String	INV_Modulate::theErrorText = "Metric Unavailable";
const OMC_String	INV_Modulate::theStartText = "Metric has not been fetched from source";
const float		INV_Modulate::theDefErrorColor[] = {0.2, 0.2, 0.2};
const float		INV_Modulate::theDefSaturatedColor[] = {1.0, 1.0, 1.0};
const double		INV_Modulate::theMinScale = 0.01;

INV_Modulate::~INV_Modulate()
{
}

INV_Modulate::INV_Modulate(const char *metric, double scale,
			   INV_MetricList::AlignColor align)
: _sts(0), _metrics(0), _root(0)
{
    _metrics = new INV_MetricList();
    _sts = _metrics->add(metric, scale);
    if (_sts >= 0) {
	_metrics->resolveColors(align);
	_saturatedColor.setValue(theDefSaturatedColor);
    }
    _errorColor.setValue(theDefErrorColor);
}

INV_Modulate::INV_Modulate(const char *metric, double scale, 
			   const SbColor &color,
			   INV_MetricList::AlignColor align)
:  _sts(0), _metrics(0), _root(0)
{
    _metrics = new INV_MetricList();
    _sts = _metrics->add(metric, scale);
    if (_sts >= 0) {
	_metrics->add(color),
	_metrics->resolveColors(align);
    }
    _saturatedColor.setValue(theDefSaturatedColor);
    _errorColor.setValue(theDefErrorColor);
}

INV_Modulate::INV_Modulate(INV_MetricList *list)
:  _sts(0), _metrics(list), _root(0)
{
    _saturatedColor.setValue(theDefSaturatedColor);
    _errorColor.setValue(theDefErrorColor);
}

const char *
INV_Modulate::add()
{
    const char *str = theModList->add(this);
    _root->setName((SbName)str);
    return str;
}

ostream &
operator<<(ostream & os, const INV_Modulate &rhs)
{
    rhs.dump(os);
    return os;
}

void
INV_Modulate::dumpState(ostream &os, INV_Modulate::State state) const
{
    switch(state) {
    case INV_Modulate::start:
	os << "Start";
	break;
    case INV_Modulate::error:
	os << "Error";
	break;
    case INV_Modulate::saturated:
	os << "Saturated";
	break;
    case INV_Modulate::normal:
	os << "Normal";
	break;
    default:
	os << "Unknown";
	break;
    }
}

void
INV_Modulate::record(INV_Record &rec) const
{
    uint_t	i;

    if (_metrics != NULL)
	for (i = 0; i < _metrics->numMetrics(); i++) {
	    const OMC_Metric &metric = _metrics->metric(i);
	    rec.add(metric.host().ptr(), 
		    metric.spec(OMC_false, OMC_true).ptr());
	}
}

void
INV_Modulate::selectAll()
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_Modulate::selectAll: selectAll for " << *this << endl;
#endif

    theModList->selectAllId(_root, 1);
    theModList->selectSingle(_root);
}
