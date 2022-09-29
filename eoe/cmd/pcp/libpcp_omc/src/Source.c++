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

#ident "$Id: Source.c++,v 1.14 1999/05/11 00:28:03 kenmcd Exp $"

#include <limits.h>
#include "pmapi.h"
#include "impl.h"
#include "String.h"
#include "Source.h"

#if defined(IRIX6_5)
#include <optional_sym.h>
#endif


OMC_Source::~OMC_Source()
{
    uint_t	i;

    for (i = 0; i < _contexts.length(); i++)
	if (_contexts[i])
	    delete _contexts[i];
}

OMC_Source::OMC_Source()
: _defaultTZ(localTZ),
  _use((uint_t)-1),
  _tzIndex(0),
  _mode(PM_CONTEXT_HOST), 
  _tzHndl(-1),
  _localTZ(-1),
  _userTZ(-1),
  _userTZStr(),
  _localTZStr(),
  _localsts(PM_ERR_NOTCONN),
  _timeEndDbl(0.0)
{
    char	buf[MAXHOSTNAMELEN];
    char	*tz;

    (void)gethostname(buf, MAXHOSTNAMELEN);
    buf[MAXHOSTNAMELEN-1] = '\0';
    _localhost = buf;

#if defined(IRIX6_5)
    if (_MIPS_SYMBOL_PRESENT(__pmTimezone))
        tz = __pmTimezone();
    else
        tz = getenv("TZ");
#else
    tz = __pmTimezone();
#endif

    if (tz == NULL)
	pmprintf("%s: Warning: Unable to get timezone for host \"localhost\", TZ not defined\n",
		 pmProgname);
    else {
	_localTZ = pmNewZone(tz);
	if (_localTZ < 0)
	    pmprintf("%s: Warning: Timezone for host \"localhost\": %s\n",
		     pmProgname, pmErrStr(_localTZ));
	else {
	    _localTZStr = tz;
	    _tzHndl = _localTZ;
	    _defaultTZ = localTZ;
	}
    }
}

int
OMC_Source::use(int type, const char *source)
{
    int		sts = 0;
    uint_t	i;

    if (source == NULL) {
	if (!defaultDefined()) {
	    createLocalContext();
	    if (!defaultDefined()) {
		pmprintf("%s: Error: Cannot connect to PMCD on host \"localhost\": %s\n",
			 pmProgname, pmErrStr(_localsts));
		return _localsts;
		/*NOTREACHED*/
	    }
	}
	source = _contexts[0]->source().ptr();
    }

    for (i = 0; i < numContexts(); i++)
	// Archive hostnames may be truncated, so only compare up to
	// PM_LOG_MAXHOSTLEN - 1.
	if (strncmp(_contexts[i]->source().ptr(), source, 
		    PM_LOG_MAXHOSTLEN - 1) == 0)
	    break;

    if (i == numContexts()) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "OMC_Source::use: No context defined for " << source
		 << ", yet." << endl;
	}
#endif

	// Determine live or archive mode by the first source
	if (i == 0)
	    _mode = type;

	// If the assumed mode differs from the requested context type
	// we may need to map the host to an archive
	if (_mode != type) {

	    // If we think we are live, then an archive is an error
	    if (_mode != PM_CONTEXT_ARCHIVE &&
		type == PM_CONTEXT_ARCHIVE) {
		pmprintf("%s: Error: Archive \"%s\" requested after live mode was assumed.\n",
			 pmProgname, source);
		return PM_ERR_MODE;
		/*NOTREACHED*/
	    }

	    // If we are in archive mode, map hosts to archives with the
	    // same host
	    if (_mode == PM_CONTEXT_ARCHIVE &&
		type != PM_CONTEXT_ARCHIVE) {
		for (i = 0; i < numContexts(); i++)
		    if (strncmp(_contexts[i]->host().ptr(), source, 
				PM_LOG_MAXHOSTLEN - 1) == 0)
			break;

		if (i == numContexts()) {
		    pmprintf("%s: Error: No archives were specified for host \"%s\"\n",
			     pmProgname, source);
		    return PM_ERR_NOTARCHIVE;
		    /*NOTREACHED*/
		}
	    }
	}
    }
	
    if (i == numContexts()) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "OMC_Source::use: Creating new context for " << source
		 << endl;
	}
#endif

	OMC_Context *newContext = new OMC_Context(type, source, numContexts());

	if (newContext->status() < 0) {
	    sts = newContext->status();
	    pmprintf("%s: Error: %s: %s\n", 
		     pmProgname, newContext->string().ptr(), pmErrStr(sts));
	    delete newContext;
	    return sts;
	    /*NOTREACHED*/
	}


	// If we are in archive mode and are adding an archive,
	// make sure another archive for the same host does not exist
	if (_mode == PM_CONTEXT_ARCHIVE && type == PM_CONTEXT_ARCHIVE) {
	    for (i = 0; i < numContexts(); i++)
		// No need to restrict comparison here, both are from
		// log labels.
		if (_contexts[i]->host() == newContext->host()) {
		    pmprintf("%s: Error: Archives \"%s\" and \"%s\" are from "
			     "the same host \"%s\"\n",
			     pmProgname, _contexts[i]->source().ptr(),
			     newContext->source().ptr(),
			     _contexts[i]->host().ptr());
		    delete newContext;
		    return -1;
		}
	}

	_contexts.append(newContext);
	_use = newContext->srcid();

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "OMC_Source::use: Added context " << _use << " to "
		 << *newContext << endl;
#endif
    }
    else if (i != _use) {
	_use = i;
	sts = useContext();
	if (sts < 0) {
	    pmprintf("%s: Error: Unable to use context to %s: %s\n",
		     pmProgname, which()->string().ptr(), pmErrStr(sts));
	    return sts;
	    /*NOTREACHED*/
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "OMC_Source::use: Using existing context " << _use
		 << " for " << *which() << endl;
#endif
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_PMC)
	cerr << "OMC_Source::use: Using current context " << _use
	    << " for " << *which() << endl;
#endif
    
    return 0;
}

int
OMC_Source::useTZ()
{
    int sts;
    if ((sts = which()->useTZ()) >= 0) {
	_tzHndl = which()->tzHandle();
	_defaultTZ = sourceTZ;
	_tzIndex = _use;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "OMC_Source::useTZ: Using timezone of " << *which()
		 << " (" << _tzIndex << ')' << endl;
#endif
    }
    return sts;
}

int
OMC_Source::useTZ(const OMC_String &tz)
{
    int sts = pmNewZone(tz.ptr());

    if (sts >= 0) {
	_userTZ = sts;
	_userTZStr = tz;
	_defaultTZ = userTZ;
	_tzHndl = sts;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "OMC_Source::useTZ: Switching timezones to \"" << tz
		 << "\" (" << _userTZStr << ')' << endl;
	}
#endif
    }
    return sts;
}

int
OMC_Source::useLocalTZ()
{
    int sts;

    if (_localTZ >= 0) {
	sts = pmUseZone(_localTZ);
	if (sts > 0) {
	    _defaultTZ = localTZ;
	    _tzHndl = _localTZ;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "OMC_Source::useTZ: Using timezone of host \"localhost\"" << endl;
#endif
	}
    }
    else
	sts = _localTZ;
    return sts;
}

void
OMC_Source::defaultTZ(OMC_String &label, OMC_String &tz)
{
    if (_defaultTZ == userTZ) {
	label = _userTZStr;
	tz = _userTZStr;
    }
    else if (_defaultTZ == localTZ) {
	label = _localhost;
	tz = _localTZStr;
    }
    else {
	label = _contexts[_tzIndex]->host();
	tz = _contexts[_tzIndex]->timezone();
    }
}

int
OMC_Source::useDefault()
{
    int		sts;

    if (numContexts() == 0)
	createLocalContext();

    if (numContexts() == 0)
	return _localsts;

    _use = 0;
    sts = pmUseContext(which()->hndl());
    return sts;
}

void
OMC_Source::createLocalContext()
{
    // Determine default source, use localhost if none listed on the
    // command line
    if (numContexts() == 0) {
	OMC_Context *newContext = new OMC_Context(PM_CONTEXT_HOST,
						  _localhost.ptr(), 0);
	_localsts = newContext->status();
	if (_localsts < 0) {
	    delete newContext;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "OMC_Source::finishedOptions: Warning: default context to "
		     << _localhost.ptr() << " failed: "
		     << pmErrStr(_localsts) << endl;
#endif
	}
	else {
	    _contexts.append(newContext);
	    _use = newContext->srcid();	

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "OMC_Source::use: Added default context " << _use << " to "
		 << *newContext << endl;
#endif
	}
    }
}

int
OMC_Source::updateBounds()
{
    int		sts;

    _timeStart.tv_sec = 0;
    _timeStart.tv_usec = 0;
    _timeEnd = _timeStart;

    sts = __pmFetchGroupArchiveBounds(&_timeStart, &_timeEnd);
    _timeEndDbl = __pmtimevalToReal(&_timeEnd);
    
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC) {
        cerr << "OMC_Source::getTimeBounds: sts = " << sts << ", start = " 
             << _timeStart.tv_sec << '.' << _timeStart.tv_usec << ", end = "
             << _timeEnd.tv_sec << '.' << _timeEnd.tv_usec << endl;
    }
#endif
    
    return sts;
}

int
OMC_Source::sendBounds(const pmTime &tctl)
{
    int			sts = 0;

    if (!isLive() && tctl.delta > 0) {
	
	double oldEnd = _timeEndDbl;
	double pos = __pmtimevalToReal(&(tctl.position));

	if (pos + (tctl.delta / 1000.0) >= _timeEndDbl) {
	    sts = updateBounds();
	    if (sts >=0 && _timeEndDbl != oldEnd)
		sts = pmTimeSendBounds(&_timeStart, &_timeEnd);
	}
    }

    return sts;
}

int
OMC_Source::sendTimezones()
{
    uint_t	i;
    int		sts = 0;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "OMC_Source::sendTimezones: " << numContexts()
		 << " hosts" << endl;
#endif

    for (i = 0; i < numContexts(); i++) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "  [" << i << "] " << _contexts[i]->host() << " -> " 
		 << _contexts[i]->timezone() << endl;
#endif

	sts = pmTimeSendTimezone((char *)(_contexts[i]->host().ptr()),
				 (char *)(_contexts[i]->timezone().ptr()));

	if (sts < 0)
	    break;
    }

    if (_localTZStr.length() > 0 && sts >= 0 && _localTZ >= 0)
	sts = pmTimeSendTimezone((char *)(_localhost.ptr()),
				 (char *)(_localTZStr.ptr()));

    if (_userTZStr.length() > 0 && sts >= 0 && _userTZ >= 0)
	sts = pmTimeSendTimezone((char *)(_userTZStr.ptr()),
				 (char *)(_userTZStr.ptr()));

    return sts;
}

ostream&
operator<<(ostream &os, const OMC_Source &src)
{
    uint_t	i;

    os << "Number of Contexts = " << src.numContexts() << ", Mode: ";
    switch(src._mode) {
    case PM_CONTEXT_LOCAL:
	os << "local";
	break;
    case PM_CONTEXT_HOST:
	os << "live host";
	break;
    case PM_CONTEXT_ARCHIVE:
	os << "archive";
	break;
    }
    os << endl;
    for (i = 0; i < src.numContexts(); i++)
	os << '[' << i << "] " << *(src._contexts[i]) << endl;
    return os;
}

int
OMC_Source::useContext()
{
    int		sts;

    sts = pmUseContext(which()->hndl());
    if (sts < 0)
	pmprintf("%s: Error: Unable to reuse context to %s: %s\n",
		 pmProgname, which()->string().ptr(), pmErrStr(sts));
    return sts;
}
