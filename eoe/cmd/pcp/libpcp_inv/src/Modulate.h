/* -*- C++ -*- */

#ifndef _INV_MODULATE_H_
#define _INV_MODULATE_H_

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

#ident "$Id: Modulate.h,v 1.3 1997/09/12 02:47:51 markgw Exp $"

#include <Inventor/SbString.h>
#include "String.h"
#include "MetricList.h"

class SoSeparator;
class SoPath;
class INV_Launch;
class INV_Record;

extern double		theNormError;
extern float		theScale;

class INV_Modulate
{
public:

    enum State	{ start, error, saturated, normal };

protected:

    static const OMC_String	theErrorText;
    static const OMC_String	theStartText;
    static const float		theDefErrorColor[];
    static const float		theDefSaturatedColor[];
    static const double		theMinScale;

    int				_sts;
    INV_MetricList		*_metrics;
    SoSeparator			*_root;
    SbColor			_errorColor;
    SbColor			_saturatedColor;

public:

    virtual ~INV_Modulate();

    INV_Modulate(const char *metric, double scale,
		 INV_MetricList::AlignColor align = INV_MetricList::perMetric);

    INV_Modulate(const char *metric, double scale, const SbColor &color,
		 INV_MetricList::AlignColor align = INV_MetricList::perMetric);

    INV_Modulate(INV_MetricList *list);

    int status() const
	{ return _sts; }
    const SoSeparator *root() const
	{ return _root; }
    SoSeparator *root()
	{ return _root; }

    uint_t numValues() const
	{ return _metrics->numValues(); }

    const char *add();

    void setErrorColor(const SbColor &color)
	{ _errorColor.setValue(color.getValue()); }
    void setSaturatedColor(const SbColor &color)
        { _saturatedColor.setValue(color.getValue()); }

    virtual void refresh(OMC_Bool fetchFlag) = 0;

    // Return the number of objects still selected
    virtual void selectAll();
    virtual uint_t select(SoPath *)
	{ return 0; }
    virtual uint_t remove(SoPath *)
	{ return 0; }

    // Should expect selectInfo calls to different paths without
    // previous removeInfo calls
    virtual void selectInfo(SoPath *)
	{}
    virtual void removeInfo(SoPath *)
	{}

    virtual void infoText(OMC_String &str, OMC_Bool selected) const = 0;

    virtual void launch(INV_Launch &launch, OMC_Bool all) const = 0;
    virtual void record(INV_Record &rec) const;

    virtual void dump(ostream &) const
	{}
    void dumpState(ostream &os, State state) const;

    friend ostream &operator<<(ostream &os, const INV_Modulate &rhs);

protected:

    static void add(INV_Modulate *obj);

private:

    INV_Modulate();
    INV_Modulate(const INV_Modulate &);
    const INV_Modulate &operator=(const INV_Modulate &);
    // Never defined
};

#endif /* _INV_MODULATE_H_ */
