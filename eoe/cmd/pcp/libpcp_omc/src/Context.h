/* -*- C++ -*- */

#ifndef _OMC_CONTEXT_H_
#define _OMC_CONTEXT_H_

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

#ident "$Id: Context.h,v 1.4 1997/09/05 00:51:05 markgw Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <iostream.h>
#include "Vector.h"
#include "String.h"
#include "Hash.h"
#include "Desc.h"
#include "Indom.h"

struct OMC_NameToId
{
    OMC_String	_name;
    pmID	_id;
};

typedef OMC_List<OMC_NameToId> OMC_NameList;
typedef OMC_Hash<OMC_Desc> OMC_DescHash;
typedef OMC_Hash<OMC_Indom> OMC_IndomHash;

class OMC_Context
{
private:

    // Local State

    int			_sts;
    int			_context;
    int			_type;
    int			_tz;
    uint_t		_srcid;
    OMC_String		_source;
    OMC_String		_host;
    OMC_String		_string;
    OMC_String		_timezone;
    OMC_NameList	_names;
    OMC_DescHash	_pmids;
    OMC_IndomHash	_indoms;

public:

    ~OMC_Context();
    OMC_Context(int type, const char *source, uint_t srcid);

    int status() const
	{ return _sts; }
    int hndl() const
	{ return _context; }
    int type() const
	{ return _type; }
    uint_t srcid() const
	{ return _srcid; }
    const OMC_String &source() const
	{ return _source; }
    const OMC_String &host() const
	{ return _host; }
    const OMC_String &string() const
	{ return _string; }
    const OMC_String &timezone() const
	{ return _timezone; }
    int tzHandle() const
	{ return _tz; }

    int lookupDesc(const char *name, int type, OMC_Desc *&desc, 
		   OMC_Indom *&indom);
    int lookupDesc(pmID pmid, int type, OMC_Desc *&desc, OMC_Indom *&indom);

    int useTZ();

    friend ostream &operator<<(ostream &os, const OMC_Context & rhs);
    
private:

    OMC_Context();
    OMC_Context(const OMC_Context &);
    const OMC_Context &operator=(const OMC_Context &);
    // Never defined
};

#endif /* _OMC_CONTEXT_H_ */
