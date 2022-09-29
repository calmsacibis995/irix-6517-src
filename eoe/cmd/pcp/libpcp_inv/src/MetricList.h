/* -*- C++ -*- */

#ifndef _INV_METRICLIST_H_
#define _INV_METRICLIST_H_

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

#ident "$Id: MetricList.h,v 1.2 1997/09/25 06:40:41 markgw Exp $"

#include <sys/types.h>
#include <iostream.h>
#include <Inventor/SbColor.h>
#include "List.h"
#include "Metric.h"

typedef OMC_List<OMC_Metric *> INV_MetricsList;
typedef OMC_List<SbColor *> INV_ColorList;

class INV_MetricList
{
public:

    enum AlignColor { noColors, perMetric, perValue };

private:

    INV_MetricsList	_metrics;
    INV_ColorList	_colors;
    uint_t		_values;

public:

    ~INV_MetricList();

    INV_MetricList();

    uint_t numMetrics() const
	{ return _metrics.length(); }
    uint_t numValues() const
	{ return _values; }
    int numColors() const
        { return _colors.length(); }

    const OMC_Metric &metric(uint_t i) const
	{ return *(_metrics[i]); }
    OMC_Metric &metric(uint_t i)
	{ return *(_metrics[i]); }

    const SbColor &color(uint_t i) const
	{ return *(_colors[i]); }
    SbColor &color(uint_t i)
	{ return *(_colors[i]); }    
    void color(uint_t i, OMC_String &str) const;

    int add(char const* metric, double scale);
    int add(pmMetricSpec *metric, double scale);
    int traverse(const char *metric);

    void add(SbColor const& color);
    void add(uint32_t packedcol);

    void resolveColors(AlignColor align = perMetric);

    static void toString(const SbColor &color, OMC_String &str);

    friend ostream& operator<<(ostream&, INV_MetricList const &);

private:

    INV_MetricList(const INV_MetricList &);
    const INV_MetricList &operator=(const INV_MetricList &);
    // Never defined
};

#endif /* _INV_METRICLIST_H_ */

