/* -*- C++ -*- */

#ifndef _INV_LAUNCH_H_
#define _INV_LAUNCH_H_

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

#include <iostream.h>
#include "pmapi.h"
#include "String.h"
#include "Bool.h"

class SbColor;
class OMC_Metric;
class OMC_Desc;
class INV_ColorScale;

class INV_Launch
{
private:

    static const OMC_String	theVersion1Str;
    static const OMC_String	theVersion2Str;
    
    OMC_StrList		_strings;
    int			_groupMetric;
    int			_groupCount;
    OMC_String		_groupHint;
    int			_metricCount;
    int			_version;

public:

    ~INV_Launch();

    INV_Launch(const OMC_String &version = "");	// defaults to version 2.0
    INV_Launch(const INV_Launch &rhs);
    const INV_Launch &operator=(const INV_Launch &rhs);

    static const char *launchPath();

    void setDefaultOptions(int interval = 5,
			   int debug = 0,
			   const char *pmnsfile = NULL,
			   const char *timeport = NULL,
			   const char *starttime = NULL,
			   const char *endtime = NULL,
			   const char *offset = NULL,
			   const char *timezone = NULL,
			   const char *defsourcetype = NULL,
			   const char *defsourcename = NULL,
			   OMC_Bool selected = OMC_false);

    void addOption(const char *name, const char *value);
    void addOption(const char *name, int value);

    void addMetric(const OMC_Metric &metric, 
		   const SbColor &color,
		   int instance,
		   OMC_Bool useSocks = OMC_false);

    void addMetric(const OMC_Metric &metric, 
		   const INV_ColorScale &scale,
		   int instance,
		   OMC_Bool useSocks = OMC_false);

    void addMetric(int context,
		   const OMC_String &source,
		   const OMC_String &host,
		   const OMC_String &metric,
		   const char *instance,
		   const OMC_Desc &desc,
		   const SbColor &color,
		   double scale,
		   OMC_Bool useSocks = OMC_false);
    // Add metric with static color and scale

    void addMetric(int context,
		   const OMC_String &source,
		   const OMC_String &host,
		   const OMC_String &metric,
		   const char *instance,
		   const OMC_Desc &desc,
		   const INV_ColorScale &scale,
		   OMC_Bool useSocks = OMC_false);
    // Add metric with dynamic color range

    void startGroup(const char *hint = "none");
    void endGroup();

    void append(INV_Launch const& rhs);

    void output(int fd) const;

friend ostream& operator<<(ostream& os, INV_Launch const& rhs);

private:

    void preColor(int context, 
    		  const OMC_String &source, 
		  const OMC_String &host,
		  const OMC_String &metric,
		  OMC_Bool useSocks,
		  OMC_String &str);
    void postColor(const OMC_Desc &desc, const char *instance, OMC_String &str);
};

#endif /* _INV_LAUNCH_H_ */
