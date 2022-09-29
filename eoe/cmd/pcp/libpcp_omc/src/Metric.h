/* -*- C++ -*- */

#ifndef _OMC_METRIC_H_
#define _OMC_METRIC_H_

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

#ident "$Id: Metric.h,v 1.4 1997/12/12 04:45:42 markgw Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include "pmapi.h"
#include "impl.h"
#include "Vector.h"
#include "String.h"
#include "Bool.h"
#include "Context.h"
#include "Source.h"

typedef OMC_Vector<struct timeval> OMC_TimeVector;

extern OMC_Source	theSource;
extern double		theStartValue;

class OMC_Metric
{
private:

    static int          theNumMetrics;
    static optcost_t    theOCost;
    static optcost_t    theNCost;

    int			_sts;
    OMC_String		_name;
    OMC_Context		*_context;
    OMC_Desc		*_desc;
    OMC_Indom		*_indom;
    OMC_IntList		_instances;
    double		_scale;

    OMC_RealVector	_values;
    OMC_RealVector	_prevValues;
    OMC_RealVector	_currValues;
    OMC_IntVector	_handles;
    OMC_IntVector	_errors;
    OMC_IntVector	_prevErrors;
    OMC_IntVector	_currErrors;
    OMC_TimeVector	_prevTime;
    OMC_TimeVector	_currTime;

public:

    ~OMC_Metric();

    OMC_Metric(const char *str, double theScale);
    OMC_Metric(pmMetricSpec *theMetric, double theScale);

    int status() const
	{ return _sts; }
    const OMC_String &name() const
	{ return _name; }
    int type() const
	{ return _context->type(); }
    const OMC_String &source() const
	{ return _context->source(); }
    const OMC_String &host() const
	{ return _context->host(); }
    const OMC_Desc &desc() const
	{ return *_desc; }
    const OMC_String &units() const
	{ return _desc->units(); }
    const OMC_String &abvUnits() const
	{ return _desc->abvUnits(); }

    int hasInstances() const
	{ return (_sts >= 0 ? _instances.length() > 0 : 0); }
    uint_t numInst() const
	{ return _instances.length(); }
    uint_t numValues() const
	{ return (_sts >= 0 ? (hasInstances() ? numInst() : 1) : 0); }
    int instID(int index) const
	{ return _indom->inst(_instances[index]); }
    const OMC_String &instName(int index) const
	{ return _indom->name(_instances[index]); }
    double scale() const
	{ return _scale; }

    double value(int index) const
	{ return _values[index]; }
    double realValue(int index) const
	{ return _values[index] * _scale; }
    double currValue(int index) const
	{ return _currValues[index]; }
    int error(int index) const
	{ return _errors[index]; }
    int currError(int index) const
	{ return _currErrors[index]; }
    struct timeval const &timeStamp(int index) const
	{ return _currTime[index]; }

    void setScaleUnits(const pmUnits &units)
	{ _desc->setScaleUnits(units); }

    OMC_String spec(OMC_Bool srcFlag = OMC_false,
		    OMC_Bool instFlag = OMC_false,
		    uint_t instance = UINT_MAX) const;

    void dump(ostream &os, 
	      OMC_Bool srcFlag = OMC_false,
	      uint_t instance = UINT_MAX) const;

    void dumpValue(ostream &os, uint_t instance) const;
    void dumpSource(ostream &os) const;

    static const char *formatNumber(double value);

    friend ostream &operator<<(ostream &os, const OMC_Metric &metric);

    //
    // Fetch
    //

    int update();

    static void fetch();
    static void setArchiveMode(int mode, const struct timeval *when, 
    			       int interval);

private:

    void setup(pmMetricSpec *theMetric);
    void setupDesc(pmMetricSpec *theMetric);
    void setupIndom(pmMetricSpec *theMetric);
    void setupValues();
    void setupFetch();

    void dumpAll() const;
    void dumpErr() const;
    void dumpErr(const char *inst) const;

    OMC_Metric();
    OMC_Metric(const OMC_Metric &);
    const OMC_Metric &operator=(const OMC_Metric &);
    // Not defined

};

#endif /* _OMC_STRING_H_ */
