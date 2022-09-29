
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

#ident "$Id: Launch.c++,v 1.7 1997/11/13 02:03:54 markgw Exp $"

#include <iostream.h>
#include <stdlib.h>
#include <syslog.h>
#include "pmapi.h"
#include "String.h"
#include "Metric.h"
#include "Launch.h"
#include "ColorScale.h"
#include "MetricList.h"

#define PM_LAUNCH_VERSION1 1
#define PM_LAUNCH_VERSION2 2

const OMC_String INV_Launch::theVersion1Str = "pmlaunch Version 1.0\n";
const OMC_String INV_Launch::theVersion2Str = "pmlaunch Version 2.0\n";

#ifndef PM_LAUNCH_PATH
#define PM_LAUNCH_PATH "/var/pcp/config/pmLaunch"
#endif


INV_Launch::~INV_Launch()
{
}

INV_Launch::INV_Launch(const OMC_String &version)
: _strings(), 
  _groupMetric(-1),
  _groupCount(0),
  _groupHint(),
  _metricCount(0)
{
    if (version == "1.0")
	_version = PM_LAUNCH_VERSION1;
    else
	_version = PM_LAUNCH_VERSION2;
}

INV_Launch::INV_Launch(const INV_Launch &rhs)
: _strings(rhs._strings),
  _groupMetric(rhs._groupMetric),
  _groupCount(rhs._groupCount),
  _groupHint(rhs._groupHint),
  _metricCount(rhs._metricCount),
  _version(rhs._version)
{
}

const INV_Launch &
INV_Launch::operator=(const INV_Launch &rhs)
{
    if (this != &rhs) {
	 _strings = rhs._strings;
	 _groupMetric = rhs._groupMetric;
	 _groupCount = rhs._groupCount;
	 _groupHint = rhs._groupHint;
	 _metricCount = rhs._metricCount;
    }
    return *this;
}

void
INV_Launch::setDefaultOptions(int interval,
			      int debug,
			      const char *pmnsfile,
			      const char *timeport,
			      const char *starttime,
			      const char *endtime,
			      const char *offset,
			      const char *timezone,
			      const char *defsourcetype,
			      const char *defsourcename,
			      OMC_Bool selected)
{
    addOption("interval", (interval < 0 ? -interval : interval));
    addOption("debug", debug);
    
    if (pmnsfile != NULL && strlen(pmnsfile) > 0)
	addOption("namespace", pmnsfile);
    if (timeport != NULL)
	addOption("timeport", timeport);
    if (starttime != NULL)
	addOption("starttime", starttime);
    if (endtime != NULL)
	addOption("endtime", endtime);
    if (offset != NULL)
	addOption("offset", offset);
    if (timezone != NULL)
	addOption("timezone", timezone);
    if (defsourcetype != NULL)
	addOption("defsourcetype", defsourcetype);
    if (defsourcename != NULL)
	addOption("defsourcename", defsourcename);
    if (pmProgname != NULL)
	addOption("progname", pmProgname);
    addOption("pid", (int)getpid());

    if (selected)
	addOption("selected", "true");
    else
	addOption("selected", "false");
}

void 
INV_Launch::addOption(const char *name, const char *value)
{
    OMC_String	str = "option ";

    if (name == NULL)
	return;

    if (value == NULL) {
	str.append(name).append("=\n");
    }
    else {
	str.append(name).appendChar('=').append(value).appendChar('\n');
	str.appendChar('\n');
    }
    _strings.append(str);
}

void 
INV_Launch::addOption(const char *name, int value)
{
    OMC_String	str = "option ";

    if (name == NULL)
	return;

    str.append(name).appendChar('=').appendInt(value).appendChar('\n');
    _strings.append(str);
}

void
INV_Launch::addMetric(const OMC_Metric &metric, 
		      const SbColor &color,
		      int instance,
		      OMC_Bool useSocks)
{
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    cerr << "INV_Launch::addMetric(1): Adding ";
	    metric.dump(cerr, OMC_true, instance);
	    cerr << " (" << _metricCount << ')' << endl;
	}
#endif

    addMetric(metric.type(), metric.source(), metric.host(), metric.name(),
	      (metric.hasInstances() == 0 ? NULL : 
					    metric.instName(instance).ptr()),
	      metric.desc(), color, metric.scale(), useSocks);
}

void
INV_Launch::addMetric(const OMC_Metric &metric, 
		      const INV_ColorScale &scale,
		      int instance,
		      OMC_Bool useSocks)
{
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    cerr << "INV_Launch::addMetric(2): Adding ";
	    metric.dump(cerr, OMC_true, instance);
	    cerr << " (" << _metricCount << ')' << endl;
	}
#endif

    addMetric(metric.type(), metric.source(), metric.host(), metric.name(),
	      (metric.hasInstances() == 0 ? NULL : 
					    metric.instName(instance).ptr()),
	      metric.desc(), scale, useSocks);
}

void 
INV_Launch::addMetric(int context,
		      const OMC_String &source,
		      const OMC_String &host,
		      const OMC_String &metric,
		      const char *instance,
		      const OMC_Desc &desc,
		      const SbColor &color,
		      double scale,
		      OMC_Bool useSocks)
{
    OMC_String	str(256);
    OMC_String	col;

    if (_groupMetric == -1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Launch::addMetric: Called before startGroup."
		 << " Adding a group." << endl;
#endif
	startGroup();
    }

    preColor(context, source, host, metric, useSocks, str);

    str.append(" S ");
    INV_MetricList::toString(color, col);
    str.append(col).appendChar(',').appendReal(scale).appendChar(' ');

    postColor(desc, instance, str);
    _strings.append(str);
}

void 
INV_Launch::addMetric(int context,
		      const OMC_String &source,
		      const OMC_String &host,
		      const OMC_String &metric,
		      const char *instance,
		      const OMC_Desc &desc,
		      const INV_ColorScale &scale,
		      OMC_Bool useSocks)
{
    uint_t	i;
    OMC_String	str(128);
    OMC_String	col;

    if (_groupMetric == -1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Launch::addMetric: Called before startGroup."
		 << " Adding a group." << endl;
#endif
	startGroup();
    }

    preColor(context, source, host, metric, useSocks, str);
    str.append(" D ");

    INV_MetricList::toString(scale[0].color(), col);
    str.append(col).appendChar(',').appendReal(scale[0].min());
    for (i = 1; i < scale.numSteps(); i++) {
	INV_MetricList::toString(scale[i].color(), col);
	str.appendChar(',').append(col).appendChar(',');
	str.appendReal(scale[i].min());
    }
    str.appendChar(' ');

    postColor(desc, instance, str);
    _strings.append(str);
}

void
INV_Launch::preColor(int context,
		     const OMC_String &source,
		     const OMC_String &host,
		     const OMC_String &metric,
		     OMC_Bool useSocks,
		     OMC_String &str)
{
    str.append("metric ").appendInt(_metricCount++).appendChar(' ');
    str.appendInt(_groupCount).appendChar(' ').append(_groupHint);

    switch(context) {
    case PM_CONTEXT_LOCAL:
	str.append(" l ");
	break;
    case PM_CONTEXT_HOST:
	str.append(" h ");
	break;
    case PM_CONTEXT_ARCHIVE:
	str.append(" a ");
	break;
    }

    str.append(source).appendChar(' ');

    if (context == PM_CONTEXT_ARCHIVE)
	str.append(host);
    else if (useSocks)
	str.append("true");
    else
	str.append("false");

    str.appendChar(' ').append(metric);
}

void
INV_Launch::postColor(const OMC_Desc &desc, 
		      const char *instance,
		      OMC_String &str)
{
    const pmDesc d = desc.desc();

    if (_version == PM_LAUNCH_VERSION2) {
	str.appendInt(d.type).appendChar(' ');
	str.appendInt(d.sem).appendChar(' ');
	str.appendInt(d.units.scaleSpace).appendChar(' ');
	str.appendInt(d.units.scaleTime).appendChar(' ');
	str.appendInt(d.units.scaleCount).appendChar(' ');
    }

    str.appendInt(d.units.dimSpace).appendChar(' ');
    str.appendInt(d.units.dimTime).appendChar(' ');
    str.appendInt(d.units.dimCount).appendChar(' ');
    str.appendInt((int)(d.indom)).append(" [");
    if (instance != NULL)
	str.append(instance);
    str.append("]\n");
}

void
INV_Launch::startGroup(const char *hint)
{

    if (_groupMetric != -1)
    	cerr << "INV_Launch::startGroup: Two groups started at once!" << endl;
    else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Launch::startGroup: Starting group " << _groupCount 
	         << endl;
#endif

	_groupMetric = _metricCount;
	_groupHint = hint;
    }
}

void
INV_Launch::endGroup()
{
    if (_groupMetric == -1)
    	cerr << "INV_Launch::endGroup: No group to end!" << endl;
    else if (_groupMetric == _metricCount) {
    	cerr << "INV_Launch::endGroup: No metrics added to group "
	     << _groupCount << endl;
	_groupMetric = -1;
    } else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Launch::endGroup: ending group " << _groupCount 
	         << endl;
#endif

	_groupMetric = -1;
	_groupCount++;
    }
}

void
INV_Launch::append(INV_Launch const &rhs)
{
    if (rhs._groupMetric != -1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Launch::append: Group not finished in appended object."
	         << " Completing group" << endl;
#endif

	// Cast away const, yuk.
	((INV_Launch *)(&rhs))->endGroup();
    }
    _strings.append(rhs._strings);
}

ostream& 
operator<<(ostream& os, INV_Launch const& rhs)
{
    uint_t i;
    for (i = 0; i < rhs._strings.length(); i++)
	os << rhs._strings[i];
    return os;
}

const char *
INV_Launch::launchPath()
{
    static char *env = getenv("PM_LAUNCH_PATH");

    if (env != NULL)
	return env;
    else
	return PM_LAUNCH_PATH;
}

void
INV_Launch::output(int fd) const
{
    if (_version == PM_LAUNCH_VERSION2)
	write(fd, theVersion2Str.ptr(), theVersion2Str.length());
    else
	write(fd, theVersion1Str.ptr(), theVersion1Str.length());

    for (int i = 0; i < _strings.length(); i++)
	write(fd, _strings[i].ptr(), _strings[i].length());
}
