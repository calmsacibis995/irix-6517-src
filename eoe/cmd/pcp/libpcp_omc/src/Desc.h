/* -*- C++ -*- */

#ifndef _OMC_DESC_H_
#define _OMC_DESC_H_

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

#ident "$Id: Desc.h,v 1.4 1999/05/11 00:28:03 kenmcd Exp $"

#include <sys/types.h>
#include "pmapi.h"
#include "impl.h"
#include "String.h"
#include "Bool.h"
#include "Hash.h"

class OMC_Desc
{
public:

    enum Extensions	{ none = 0, 
			  timeUtil = 1, 
			  canonicalSpace = 2 };

private:

    int		_sts;
    pmID	_pmid;
    pmDesc	_desc;
    OMC_String	_units;
    OMC_String	_abvUnits;
    pmUnits	_scaleUnits;
    OMC_Bool	_scaleFlag;

public:

    ~OMC_Desc()
	{}

    OMC_Desc(pmID pmid);

    int status() const
	{ return _sts; }
    pmID id() const
	{ return _pmid; }
    pmDesc desc() const
	{ return _desc; }
    const pmDesc *descPtr() const
	{ return &_desc; }
    const OMC_String &units() const
	{ return _units; }
    const OMC_String &abvUnits() const
	{ return _abvUnits; }
    OMC_Bool useScaleUnits() const
	{ return _scaleFlag; }
    const pmUnits &scaleUnits() const
	{ return _scaleUnits; }
    void setScaleUnits(const pmUnits &units);

    OMC_Desc();
    // OMC_Desc(const OMC_Desc &);
    const OMC_Desc &operator=(const OMC_Desc &);
    // Never defined

    void setUnitStrs();
    static const char *abvUnitsStr(pmUnits *pu);
};

#endif /* _OMC_DESC_H_ */
