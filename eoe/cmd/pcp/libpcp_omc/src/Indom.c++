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

#ident "$Id: Indom.c++,v 1.2 1997/09/05 00:51:47 markgw Exp $"

#include <iostream.h>
#include "Indom.h"
#include "Desc.h"

OMC_Indom::~OMC_Indom()
{
}

OMC_Indom::OMC_Indom(int type, OMC_Desc &desc)
: _sts(0), _id(desc.desc().indom), _numInsts(0), _instlist(4), _namelist(4)
{
    int		*instlist;
    char	**namelist;
    uint_t	i;

    if (type == PM_CONTEXT_HOST || type == PM_CONTEXT_LOCAL)
	_sts = pmGetInDom(_id, &(instlist), &(namelist));
    else if (type == PM_CONTEXT_ARCHIVE)
	_sts = pmGetInDomArchive(_id, &(instlist), &(namelist));
    else
	_sts = PM_ERR_NOCONTEXT;

    if (_sts > 0) {
	_numInsts = _sts;

	_instlist.resize(_numInsts);
	_namelist.resize(_numInsts);

	for (i = 0; i < _numInsts; i++) {
	    OMC_String tmpStr = namelist[i];
	    _instlist.append(instlist[i]);
	    _namelist.append(tmpStr);
	}

	free(instlist);
	free(namelist);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_INDOM)
	    cerr << "OMC_Indom::OMC_Indom: indom " << *this;
#endif

    }
    else if (_sts == 0)
	_sts = PM_ERR_VALUE;

#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_PMC) {
	cerr << "OMC_Indom::OMC_Indom: unable to lookup "
	     << pmInDomStr(_id) << " from "
	     << (type == PM_CONTEXT_ARCHIVE ? "archive" : "host/local")
	     << " source: " << pmErrStr(_sts) << endl;
    }
#endif
}

ostream&
operator<<(ostream &os, const OMC_Indom &indom)
{
    uint_t	i;

    os << pmInDomStr(indom._id) << ": " << indom._numInsts 
       << " instances" << endl;
    for (i = 0; i < indom._numInsts; i++)
	os << "  [" << indom._instlist[i] << "] = \""
	   << indom._namelist[i] << '\"' << endl;

    return os;
}
