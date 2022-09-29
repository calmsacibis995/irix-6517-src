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

#ident "$Id: Metric.c++,v 1.16 1997/12/12 05:52:45 markgw Exp $"

#include <iostream.h>
#include <ctype.h>
#include "Metric.h"

int		OMC_Metric::theNumMetrics = 0;
optcost_t	OMC_Metric::theOCost;
optcost_t	OMC_Metric::theNCost;

OMC_Source	theSource;
double		theStartValue = 1.0;

static struct timeval	theInitTime = { 0, 0 };

OMC_Metric::~OMC_Metric()
{
    uint_t	i;

    for (i = 0; i < _handles.length(); i++)
	if (_handles[i] >= 0)
	    __pmFetchGroupDel(_handles[i]);
}

OMC_Metric::OMC_Metric(const char *str, double theScale)
: _sts(0), 
  _name(),
  _context(0), 
  _desc(0), 
  _indom(0),
  _instances(),
  _scale(theScale),
  _values(1, theStartValue),
  _prevValues(1, theStartValue),
  _currValues(1, theStartValue),
  _handles(1, 0),
  _errors(1, PM_ERR_VALUE),
  _prevErrors(1, PM_ERR_VALUE),
  _currErrors(1, PM_ERR_VALUE),
  _prevTime(1, theInitTime),
  _currTime(1, theInitTime)
{
    pmMetricSpec	*theMetric;
    char		*msg;

    _sts = pmParseMetricSpec((char *)str, 0, (char *)0, &theMetric, &msg);
    if (_sts < 0) {
	pmprintf("%s: Error: Unable to parse metric spec:\n%s\n", 
		 pmProgname, msg);
	free(msg);
	return;
	/*NOTREACHED*/
    }

    if (_sts >= 0) {
	_name = theMetric->metric;
	setup(theMetric);
    }

    free(theMetric);
}

OMC_Metric::OMC_Metric(pmMetricSpec *theMetric, double theScale)
: _sts(0), 
  _name(theMetric->metric),
  _context(0), 
  _desc(0), 
  _indom(0),
  _instances(),
  _scale(theScale),
  _values(1),
  _prevValues(1),
  _currValues(1),
  _handles(1),
  _errors(1),
  _prevErrors(1),
  _currErrors(1),
  _prevTime(1),
  _currTime(1)
{
    setup(theMetric);
}

void
OMC_Metric::setup(pmMetricSpec *theMetric)
{
    if (_sts >= 0)
	setupDesc(theMetric);
    if (_sts >= 0)
	setupIndom(theMetric);
    if (_sts >= 0)
	setupValues();
    if (_sts < 0) {
	return;
	/*NOTREACHED*/
    }

 #ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
	dumpAll();
#endif

    setupFetch();
}

void
OMC_Metric::setupDesc(pmMetricSpec *theMetric)
{
    int		contextType = PM_CONTEXT_HOST;
    int		descType;

    if (theMetric->source && strlen(theMetric->source) > 0) {
	if (theMetric->isarch)
	    contextType = PM_CONTEXT_ARCHIVE;
    }

    _sts = theSource.use(contextType, theMetric->source);

    if (_sts >= 0) {
	_context = theSource.which();
	contextType = _context->type();

	_sts = _context->lookupDesc(theMetric->metric, contextType, _desc, 
				    _indom);

	if (_sts < 0)
	    pmprintf("%s: Error: %s%c%s: %s\n", 
		     pmProgname, _context->source().ptr(),
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     _name.ptr(), pmErrStr(_sts));
    }

    if (_sts >= 0) {
	descType = _desc->desc().type;
	if (descType == PM_TYPE_NOSUPPORT) {
	    _sts = PM_ERR_CONV;
	    pmprintf("%s: Error: %s%c%s is not supported on %s\n",
		     pmProgname, _context->source().ptr(),
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     _name.ptr(), _context->host().ptr());
	}
	else if (descType == PM_TYPE_STRING ||
		 descType == PM_TYPE_AGGREGATE ||
		 descType == PM_TYPE_AGGREGATE_STATIC ||
		 descType == PM_TYPE_UNKNOWN) {

	    _sts = PM_ERR_CONV;
	    pmprintf("%s: Error: %s%c%s has type \"%s\" which is not numeric\n",
		     pmProgname, _context->source().ptr(),
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     _name.ptr(), pmTypeStr(descType));
	}
    }
}

void
OMC_Metric::setupIndom(pmMetricSpec *theMetric)
{
    uint_t	i;
    uint_t	j;
    char	*p = NULL;
    const char	*q = NULL;
    int		instLen;

    if (_desc->desc().indom == PM_INDOM_NULL) {
	if (theMetric->ninst > 0) {
	    _sts = PM_ERR_INST;
	    dumpErr(theMetric->inst[0]);
	}
    }
    else if (theMetric->ninst) {
	assert(_indom != NULL);
	_instances.resize(theMetric->ninst);
	
	for (i = 0 ; i < theMetric->ninst && _sts >= 0; i++) {
	    for (j = 0; j < _indom->numInsts(); j++) {
		if (_indom->name(j) == theMetric->inst[i]) {
		    _instances.appendCopy(j);
		    break;;
		}
	    }

	    if (j == _indom->numInsts()) {

		// Match up to the first space
		// Need this for proc and similiar agents

		instLen = strlen(theMetric->inst[i]);
		for (j = 0; j < _indom->numInsts(); j++) {
		    p = strchr(_indom->name(j).ptr(), ' ');
		    
		    if (p != NULL &&
			instLen == (p - _indom->name(j).ptr()) &&
			strncmp(theMetric->inst[i], _indom->name(j).ptr(),
				p - _indom->name(j).ptr()) == 0) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_PMC)
			    cerr << "OMC_Metric::setupDesc: inst \""
				 << theMetric->inst[i] << "\"(" << i
				 << ") matched to \"" << _indom->name(j) 
				 << "\"(" << j << ')' << endl;
#endif

			_instances.appendCopy(j);
			break;
		    }
		}
	    }

	    if (j == _indom->numInsts()) {

		// If the instance requested is numeric, then ignore leading
		// zeros in the instance up to the first space

		for (j = 0; j < instLen; j++)
		    if (!isdigit(theMetric->inst[i][j]))
			break;

		// The requested instance is numeric
		if (j == instLen) {
		    for (j = 0; j < _indom->numInsts(); j++) {
			p = strchr(_indom->name(j).ptr(), ' ');

			if (p == NULL)
			    continue;

			for (q = _indom->name(j).ptr(); 
			     isdigit(*q) && *q == '0' && q < p;
			     q++);

			if (q < p && isdigit(*q) && instLen == (p - q) &&
			    strncmp(theMetric->inst[i], q, p - q) == 0) {
			
#ifdef PCP_DEBUG
			    if (pmDebug & DBG_TRACE_PMC)
				cerr << "OMC_Metric::setupDesc: numerical inst \""
				     << theMetric->inst[i] << "\"(" << i
				     << ") matched to \"" << _indom->name(j) 
				     << "\"(" << j << ')' << endl;
#endif

			    _instances.appendCopy(j);
			    break;
			}
		    }
		}
		else
		    j = _indom->numInsts();
	    }

	    if (j == _indom->numInsts()) {

		// Looks like I don't know about that instance, I tried my best
		
		_sts = PM_ERR_INST;
		dumpErr(theMetric->inst[i]);
	    }
	}
    }
    else {
	assert(_indom != NULL);
	assert(_indom->numInsts());
	_instances.resize(_indom->numInsts());
	for (i = 0; i < _indom->numInsts(); i++)
	    _instances.appendCopy(i);
    }
}

void
OMC_Metric::setupValues()
{
    uint_t		num = numValues();

    _values.resize(num, theStartValue);
    _prevValues.resize(num, theStartValue);
    _currValues.resize(num, theStartValue);
    _handles.resizeCopy(num, 0);
    _errors.resizeCopy(num, PM_ERR_VALUE);
    _prevErrors.resizeCopy(num, PM_ERR_VALUE);
    _currErrors.resizeCopy(num, PM_ERR_VALUE);
    _prevTime.resize(num, theInitTime);
    _currTime.resize(num, theInitTime);
}

void
OMC_Metric::setupFetch()
{
    uint_t	i;
    char	*src = (char *)(source().ptr());	// Avoid const cast
    pmDesc	*desc = (pmDesc *)(_desc->descPtr());	// warnings, sigh

    if (numInst() > 1) {
	if (theNumMetrics == 0) {
	    theNumMetrics++;
	    __pmOptFetchGetParams(&theOCost);
	    theNCost = theOCost;
	    theNCost.c_fetch = theNCost.c_indomsize * 
		theNCost.c_xtrainst + 10;
	}
	__pmOptFetchPutParams(&theNCost);
    }
    
    if (hasInstances()) {
	for (i = 0; i < numInst() && _sts >= 0; i++) {
	    _sts = _handles[i] = __pmFetchGroupAdd(type(), src, desc, instID(i),
					       &_currValues[i], 
					       &_prevValues[i],
					       &_currTime[i], 
					       &_prevTime[i],
					       &_currErrors[i], 
					       &_prevErrors[i]);
	}
    }
    else {
	_sts = _handles[0] = __pmFetchGroupAdd(type(), src, desc, 0,
					   &_currValues[0], &_prevValues[0],
					   &_currTime[0], &_prevTime[0],
					   &_currErrors[0], &_prevErrors[0]);
    }

    if (_sts < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "OMC_Metric::setupFetch: failed to add "
		 << spec(OMC_true, OMC_true, i) << endl;
	}
#endif
	if (hasInstances())
	    dumpErr(instName(i).ptr());
	else
	    dumpErr();
	return;
	/*NOTREACHED*/
    }

    if (numInst() > 1)
	__pmOptFetchPutParams(&theOCost);
}

OMC_String
OMC_Metric::spec(OMC_Bool srcFlag, OMC_Bool instFlag, uint_t instance) const
{
    OMC_String	str;
    uint_t	len = 4;
    uint_t	i;

    if (srcFlag)
	len += source().length();
    len += name().length();
    if (hasInstances() && instFlag) {
	if (instance != UINT_MAX)
	    len += instName(instance).length() + 2;
	else {
	    for (i = 0; i < numInst(); i++)
		len += instName(i).length() + 4;
	}
    }
    str.resize(len);

    if (srcFlag) {
	str.append(source());
	if (type() == PM_CONTEXT_ARCHIVE)
	    str.appendChar('/');
	else
	    str.appendChar(':');
    }
    str.append(name());
    if (hasInstances() && instFlag) {
	str.appendChar('[');
	str.appendChar('\"');
	if (instance != UINT_MAX)
	    str.append(instName(instance));
	else {
	    str.append(instName(0));
	    for (i = 1; i < numInst(); i++) {
		str.append("\", \"");
		str.append(instName(i));
	    }
	}
	str.append("\"]");
    }

    return str;
}

void
OMC_Metric::dumpSource(ostream &os) const
{
    switch(type()) {
    case PM_CONTEXT_LOCAL:
	os << "localhost:";
	break;
    case PM_CONTEXT_HOST:
	os << source() << ':';
	break;
    case PM_CONTEXT_ARCHIVE:
	os << source() << '/';
	break;
    }
}

void
OMC_Metric::dumpValue(ostream &os, uint_t inst) const
{
    if (error(inst) < 0)
	os << pmErrStr(error(inst));
    else
	os << value(inst) << " " << units();
}

void
OMC_Metric::dump(ostream &os, OMC_Bool srcFlag, uint_t instance) const
{
    uint_t	i;

    if (srcFlag == OMC_true)
	dumpSource(os);

    os << name();

    if (hasInstances()) {
	if (instance == UINT_MAX) {
	    if (numInst() == 1)
		os << ": 1 instance" << endl;
	    else
		os << ": " << numInst() << " instances" << endl;

	    for (i = 0; i < numInst(); i++) {
		os << "  [" << instID(i) << " or \"" << instName(i) << "\"] = ";
		dumpValue(os, i);
		os << endl;
	    }
	}
	else {
	    os << '[' << instID(instance) << " or \"" << instName(instance) 
	       << "\"] = ";
	    dumpValue(os, instance);
	    os << endl;
	}
    }
    else {
	os << " = ";
	dumpValue(os, 0);
	os << endl;
    }
}

ostream&
operator<<(ostream &os, const OMC_Metric &mtrc)
{
    uint_t	num = mtrc.numValues();
    uint_t	i;

    mtrc.dumpSource(os);

    os << mtrc.name();
    if (mtrc.hasInstances()) {
	os << "[\"" << mtrc.instName(0);
	for (i = 1; i < num; i++)
	    os << "\", \"" << mtrc.instName(i);
	os << "\"]";
    }

    return os;
}

int
OMC_Metric::update()
{
    double	delta;
    double	*v = _values.ptr();
    double	*c = _currValues.ptr();
    double	*p = _prevValues.ptr();
    uint_t	err = 0;
    uint_t	num = numValues();
    uint_t	i;
    int		sts;
    pmAtomValue	ival;
    pmAtomValue oval;
    static int	wrap = -1;

    if (num == 0 || _sts < 0)
	return _sts;

    if (wrap == -1) {
	// PCP_COUNTER_WRAP in environment enables "counter wrap" logic
	       if (getenv("PCP_COUNTER_WRAP") == NULL)
            wrap = 0;
        else
	    wrap = 1;
    }

    for (i = 0; i < num; i++) {
	if (_currErrors[i] == 0)
	    _currErrors[i] = PM_ERR_VALUE;
	_errors[i] = _currErrors[i];
	if (_errors[i] < 0)
	    err++;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    if (_errors[i] < 0)
		cerr << "OMC_Metric::update: " << spec(OMC_true, OMC_true, i) 
		     << ": " << pmErrStr(_errors[i]) << endl;
#endif
    }
    
    if (_desc->desc().sem == PM_SEM_COUNTER) {
	for (i = 0; i < num; i++, v++, c++, p++) {

	    if (_errors[i] <= 0) {		// we already know we
		*v = theStartValue;		// don't have this value
		continue;
		/*NOTREACHED*/
	    }

	    if (_prevErrors[i] <= 0) {	// we need two values
		*v = theStartValue;		// for a rate
		_errors[i] = _prevErrors[i];
		err++;

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PMC) {
		    cerr << "OMC_Metric::update: Previous: " 
			 << spec(OMC_true, OMC_true, i) << ": "
			 << pmErrStr(_errors[i]) << endl;
		}
#endif

		continue;
		/*NOTREACHED*/
	    }

	    *v = *c - *p;
	    delta = __pmtimevalSub(&_currTime[i], &_prevTime[i]);

	    if (*v < 0 && delta > 0) {	// wrapped going forward
		if (wrap) {
		    switch(_desc->desc().type) {
		    case PM_TYPE_32:
		    case PM_TYPE_U32:
			*v += (double)UINT_MAX+1;
			break;
		    case PM_TYPE_64:
		    case PM_TYPE_U64:
			*v += (double)ULONGLONG_MAX+1;
			break;
		    }
		}
		else {			// counter not montonic
		    *v = theStartValue;	// increasing
		    _errors[i] = PM_ERR_VALUE;
		    err++;
		    continue;
		    /*NOTREACHED*/
		}
	    }
	    else if (*v > 0 && delta < 0) {	// wrapped going backward
		if (wrap) {
		    switch(_desc->desc().type) {
		    case PM_TYPE_32:
		    case PM_TYPE_U32:
			*v -= (double)UINT_MAX+1;
			break;
		    case PM_TYPE_64:
		    case PM_TYPE_U64:
			*v -= (double)ULONGLONG_MAX+1;
			break;
		    }
		}
		else {			// counter not montonic
		    *v = theStartValue;	// increasing
		    _errors[i] = PM_ERR_VALUE;
		    err++;
		    continue;
		    /*NOTREACHED*/
		}
	    }

	    if (delta != 0)		// sign of delta and v
		*v /= delta;		// should be the same
	    else
		*v = 0;			// nothing can have happened
	}	    
    }
    else {
	for (i = 0; i < num; i++, c++, v++) {
	    if (_errors[i] < 0)
		*v = theStartValue;
	    else
		*v = *c;
	}
    }

    if (_scale != 0.0) {
	v = _values.ptr();
	for (i = 0; i < num; i++, v++) {
	    if (_errors[i] >= 0)
		*v /= _scale;
	}
    }

    if (_desc->useScaleUnits()) {
	for (i = 0; i < num; i++) {
	    ival.d = _values[i];
	    pmUnits units = _desc->desc().units;
	    sts = pmConvScale(PM_TYPE_DOUBLE, &ival, &units,
			      &oval, (pmUnits *)&(_desc->scaleUnits()));
	    if (sts < 0)
		_errors[i] = sts;
	    else {
		_values[i] = oval.d;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PMC)
		    cerr << "OMC_Metric::update: scaled " << _name
			 << " from " << ival.d << " to " << oval.d
			 << endl;
#endif
	    }
	}
    }

    return err;
}

void
OMC_Metric::fetch()
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
        cerr << "OMC_Metric::fetch: refreshing metrics now" << endl;
#endif

    __pmFetchGroupFetch();
}

void
OMC_Metric::setArchiveMode(int mode, const struct timeval *when, int interval)
{
    __pmFetchGroupArchiveMode(mode, (struct timeval *)when, interval);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
        cerr << "OMC_Metric::setArchiveMode: mode = " << mode << ", when = "
             << when->tv_sec << "." << when->tv_usec << ", interval = "
             << interval << endl;
#endif
}

void
OMC_Metric::dumpAll() const
{
    cerr << *this << " from host \"" << host() << "\" with scale = " 
	 << _scale << " and units = " << units() << endl;
}

void
OMC_Metric::dumpErr() const
{
    pmprintf("%s: Error: %s: %s\n", 
	     pmProgname, spec(OMC_true).ptr(), pmErrStr(_sts));
}

// Instance list may not be valid, so pass inst as a string rather than
// as an index

void
OMC_Metric::dumpErr(const char *inst) const
{
    pmprintf("%s: Error: %s[%s]: %s\n", 
	     pmProgname, spec(OMC_true).ptr(), inst, pmErrStr(_sts));
}

const char *
OMC_Metric::formatNumber(double value)
{
    static char buf[8];

    if (value >= 0.0) {
	if (value > 99950000000000.0)
	    strcpy(buf, "  inf?");
	else if (value > 99950000000.0)
	    sprintf(buf, "%5.2fT", value / 1000000000000.0);
	else if (value > 99950000.0)
	    sprintf(buf, "%5.2fG", value / 1000000000.0);
	else if (value > 99950.0)
	    sprintf(buf, "%5.2fM", value / 1000000.0);
	else if (value > 99.95)
	    sprintf(buf, "%5.2fK", value / 1000.0);
	else if (value > 0.005)
	    sprintf(buf, "%5.2f ", value);
	else
	    strcpy(buf, " 0.00 ");
    }
    else {
	if (value < -9995000000000.0)
	    strcpy(buf, " -inf?");
	else if (value < -9995000000.0)
	    sprintf(buf, "%.2fT", value / 1000000000000.0);
	else if (value < -9995000.0)
	    sprintf(buf, "%.2fG", value / 1000000000.0);
	else if (value < -9995.0)
	    sprintf(buf, "%.2fM", value / 1000000.0);
	else if (value < -9.995)
	    sprintf(buf, "%.2fK", value / 1000.0);
	else if (value < -0.005)
	    sprintf(buf, "%.2f ", value);
	else
	    strcpy(buf, " 0.00  ");
    }

    return buf;
}
