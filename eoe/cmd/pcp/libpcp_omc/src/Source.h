/* -*- C++ -*- */

#ifndef _OMC_SOURCE_H_
#define _OMC_SOURCE_H_

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

#ident "$Id: Source.h,v 1.4 1997/10/09 00:29:19 markgw Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <iostream.h>
#include "List.h"
#include "String.h"
#include "Bool.h"
#include "Context.h"

typedef OMC_List<OMC_Context*> OMC_ContextList;

class OMC_Source
{
public:

    enum TimeZoneFlag { userTZ, localTZ, sourceTZ };

private:

    OMC_ContextList	_contexts;
    TimeZoneFlag	_defaultTZ;	// default TZ
    uint_t		_use;
    uint_t		_tzIndex;	// index to source TZ
    int			_mode;
    int			_tzHndl;	// handle to default TZ
    int			_localTZ;	// handle to local TZ
    int			_userTZ;	// handle to specified TZ
    OMC_String		_userTZStr;	// specified TZ string
    OMC_String		_localTZStr;	// local TZ string
    OMC_String		_localhost;
    int			_localsts;
    struct timeval	_timeStart;
    struct timeval	_timeEnd;
    double		_timeEndDbl;

public:

    ~OMC_Source();

    OMC_Source();

    int numContexts() const
	{ return _contexts.length(); }
    int mode() const
	{ return _mode; }
    int isLive() const
	{ return (_mode != PM_CONTEXT_ARCHIVE); }
    const struct timeval &logStart() const
    	{ return _timeStart; }
    const struct timeval &logEnd() const
    	{ return _timeEnd; }
    const OMC_String &localhost() const
	{ return _localhost; }

    OMC_Context *which() const
	{ return _contexts[_use]; }

    int use(int type, const char *source);

    OMC_Bool defaultDefined() const
	{ return (numContexts() > 0 ? OMC_true : OMC_false); }
    int useDefault();

    void createLocalContext();

    // Use TZ of current context as default
    int useTZ();

    // Use this TZ as default
    int useTZ(const OMC_String &tz);

    // Use local TZ as default
    int useLocalTZ();

    // Return the default context
    void defaultTZ(OMC_String &label, OMC_String &tz);

    TimeZoneFlag defaultTZ() const
	{ return _defaultTZ; }

    // pmtime interaction
    int updateBounds();
    int sendBounds(const pmTime &tctl);
    int sendTimezones();

    friend ostream &operator<<(ostream &os, const OMC_Source &rhs);
    
private:

    int useContext();

    OMC_Source(const OMC_Source &);
    const OMC_Source &operator=(const OMC_Source &);
    // Never defined
};

#endif /* _OMC_SOURCE_H_ */
