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

#ident "$Id: Context.c++,v 1.8 1997/09/12 02:51:06 markgw Exp $"

#include <iostream.h>
#include <limits.h>
#include "Context.h"

OMC_Context::~OMC_Context()
{
}

OMC_Context::OMC_Context(int type, const char *source, uint_t srcid)
: _sts(0), 
  _context(-1), 
  _type(type), 
  _srcid(srcid), 
  _source(source),
  _host(),
  _string(),
  _names(),
  _pmids(),
  _indoms()
{
    __pmContext	*ctxp;
    char	*tzs;
    int		oldTZ;

    switch(type) {
    case PM_CONTEXT_LOCAL:
	_string = "localhost";
	break;
    case PM_CONTEXT_HOST:
	_string = "host \"";
	_string.append(source);
	_string.appendChar('"');
	break;
    case PM_CONTEXT_ARCHIVE:
	_string = "archive \"";
	_string.append(source);
	_string.appendChar('"');
	break;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
	cerr << "OMC_Context::OMC_Context: creating context " << srcid
	     << " to " << _string << endl;
#endif

#ifdef PCP_UNLICENSED    
    _sts = _pmAuthNewContext(_type, _source.ptr());
#else
    _sts = pmNewContext(_type, _source.ptr());
#endif

    if (_sts >= 0) {
	_context = _sts;

	// Determine host
	switch (_type) {
	case PM_CONTEXT_LOCAL:
	    char	buf[MAXHOSTNAMELEN];
	    (void)gethostname(buf, MAXHOSTNAMELEN);
	    buf[MAXHOSTNAMELEN-1] = '\0';
	    _host = buf;
	    break;
	case PM_CONTEXT_HOST:
	    _host = _source;
	    break;
        case PM_CONTEXT_ARCHIVE:
	    ctxp = __pmHandleToPtr(pmWhichContext());
	    _host = ctxp->c_archctl->ac_log->l_label.ill_hostname;
	    break;
	}

	oldTZ = pmWhichZone(&tzs);

	_tz = pmNewContextZone();
	if (_tz < 0) {
	    pmprintf("%s: Warning: Unable to obtain timezone for %s: %s\n",
		     pmProgname, _string.ptr(), pmErrStr(_tz));
	    _timezone = "";
	}
	else {
	    _sts = pmWhichZone(&tzs);
	    if (_sts >= 0)
		_timezone = tzs;
	    else {
		pmprintf("%s: Warning: Unable to obtain timezone for %s: %s\n",
			 pmProgname, _string.ptr(), pmErrStr(_sts));
		_timezone = "";
		_sts = 0;	// Ignore this problem
	    }
	}

	_sts = pmUseZone(oldTZ);
	if (_sts < 0) {
	    pmprintf("%s: Warning: Using timezone for %s: %s\n",
		     pmProgname, _string.ptr(), pmErrStr(_sts));
	    _sts = 0;		// Ignore this problem
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "OMC_Context::OMC_Context: Created context to "
		 << *this << " to host " << _host << ", hndl = " 
		 << _context << ", srcid = " << _srcid << endl;
	}
#endif

    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_PMC) {
	cerr << "OMC_Context::OMC_Context: context to " << source
	     << " failed: " << pmErrStr(_sts) << endl;
    }
#endif
}

int
OMC_Context::lookupDesc(const char *name,
			int type, 
			OMC_Desc *&desc, 
			OMC_Indom *&indom)
{
    int		sts;
    uint_t	i;
    uint_t	len = strlen(name);
    pmID	id;

    for (i = 0; i < _names.length(); i++) {
	const OMC_NameToId &item = _names[i];
	if (item._name.length() == len && 
	    strcmp(item._name.ptr(), name) == 0) {
	    id = item._id;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "OMC_Context::lookupDesc: Matched \"" << name
		     << "\" to id " << pmIDStr(id) << endl;
#endif
	    break;
	}
    }

    if (i == _names.length()) {
	sts = pmLookupName(1, (char **)(&name), &id);
	if (sts >= 0) {
	    OMC_NameToId newName;
	    newName._name = name;
	    newName._id = id;
	    _names.append(newName);
	}
	else
	    return sts;
    }

    return lookupDesc(id, type, desc, indom);    
}

int
OMC_Context::lookupDesc(pmID pmid, 
			int type, 
			OMC_Desc *&desc, 
			OMC_Indom *&indom)
{
    int		sts = 0;

    desc = NULL;
    indom = NULL;
    desc = _pmids.search(pmid);

    if (desc == NULL) {
	desc = new OMC_Desc(pmid);
	if (desc->status() < 0) {
	    sts = desc->status();
	    delete desc;
	    desc = NULL;
	    return sts;
	    /*NOTREACHED*/
	}

	sts = _pmids.add(pmid, desc);
	if (sts < 0) {
	    delete desc;
	    desc = NULL;
	    return sts;
	    /*NOTREACHED*/
	}
    }

    if (desc->desc().indom != PM_INDOM_NULL) {
	indom = _indoms.search(desc->desc().indom);

	if (indom == NULL) {
	    indom = new OMC_Indom(type, *desc);
	    if (indom->status() < 0) {
		sts = indom->status();
		delete indom;
		indom = NULL;
		return sts;
		/*NOTREACHED*/
	    }

	    sts = _indoms.add(indom->id(), indom);
	    if (sts < 0) {
		delete indom;
		indom = NULL;
		return sts;
		/*NOTREACHED*/
	    }
	}
    }

    return sts;
}

ostream&
operator<<(ostream &os, const OMC_Context &cntx)
{
    os << cntx._string;
    return os;
}

int
OMC_Context::useTZ()
{
    if (_tz >= 0)
	return pmUseZone(_tz);
    return 0;
}
