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

#ident "$Id: Desc.c++,v 1.6 1999/05/11 00:28:03 kenmcd Exp $"

#include <iostream.h>
#include "Desc.h"
#include "Hash.h"

OMC_Desc::OMC_Desc(pmID pmid)
: _sts(0), _pmid(pmid), _scaleFlag(OMC_false)
{
    _sts = pmLookupDesc(_pmid, &_desc);
    if (_sts >= 0) {
	_scaleUnits = _desc.units;
	setUnitStrs();
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_PMC) {
	cerr << "OMC_Desc::OMC_Desc: unable to lookup "
	     << pmIDStr(_pmid) << ": " << pmErrStr(_sts) << endl;
    }
#endif
}

void
OMC_Desc::setUnitStrs()
{
    const char *units = pmUnitsStr(&_scaleUnits);
    const char *abvUnits = abvUnitsStr(&_scaleUnits);
    if (_desc.sem == PM_SEM_COUNTER) {
	// Time utilisation
	if (_scaleFlag &&
	    _scaleUnits.dimTime == 1 &&
	    _scaleUnits.dimSpace == 0 &&
	    _scaleUnits.dimCount == 0) {
	    _units = "Time Utilization";
	    _abvUnits = "util";
	}
	else {
	    _units = units;
	    _units.append(" / second");
	    _abvUnits = abvUnits;
	    _abvUnits.append("/s");
	}
    }
    else {
	if (units[0] == '\0')
	    _units = "none";
	else
	    _units = units;

	if (abvUnits[0] == '\0')
	    _abvUnits = "none";
	else
	    _abvUnits = abvUnits;
    }
}

const char *
OMC_Desc::abvUnitsStr(pmUnits *pu)
{
    char	*spacestr;
    char	*timestr;
    char	*countstr;
    char	*p;
    char	sbuf[20];
    char	tbuf[20];
    char	cbuf[20];
    static char	buf[60];

    buf[0] = '\0';

    if (pu->dimSpace) {
	switch (pu->scaleSpace) {
	    case PM_SPACE_BYTE:
		spacestr = "b";
		break;
	    case PM_SPACE_KBYTE:
		spacestr = "Kb";
		break;
	    case PM_SPACE_MBYTE:
		spacestr = "Mb";
		break;
	    case PM_SPACE_GBYTE:
		spacestr = "Gb";
		break;
	    case PM_SPACE_TBYTE:
		spacestr = "Tb";
		break;
	    default:
		sprintf(sbuf, "space-%d", pu->scaleSpace);
		spacestr = sbuf;
		break;
	}
    }
    if (pu->dimTime) {
	switch (pu->scaleTime) {
	    case PM_TIME_NSEC:
		timestr = "ns";
		break;
	    case PM_TIME_USEC:
		timestr = "us";
		break;
	    case PM_TIME_MSEC:
		timestr = "msec";
		break;
	    case PM_TIME_SEC:
		timestr = "s";
		break;
	    case PM_TIME_MIN:
		timestr = "m";
		break;
	    case PM_TIME_HOUR:
		timestr = "h";
		break;
	    default:
		sprintf(tbuf, "time-%d", pu->scaleTime);
		timestr = tbuf;
		break;
	}
    }
    if (pu->dimCount) {
	switch (pu->scaleCount) {
	    case 0:
		countstr = "c";
		break;
	    case 1:
		countstr = "cx10";
		break;
	    default:
		sprintf(cbuf, "cx10^%d", pu->scaleCount);
		countstr = cbuf;
		break;
	}
    }

    p = buf;

    if (pu->dimSpace > 0) {
	if (pu->dimSpace == 1)
	    sprintf(p, "%s", spacestr);
	else
	    sprintf(p, "%s^%d", spacestr, pu->dimSpace);
	while (*p) p++;
    }
    if (pu->dimTime > 0) {
	if (pu->dimTime == 1)
	    sprintf(p, "%s", timestr);
	else
	    sprintf(p, "%s^%d", timestr, pu->dimTime);
	while (*p) p++;
    }
    if (pu->dimCount > 0) {
	if (pu->dimCount == 1)
	    sprintf(p, "%s", countstr);
	else
	    sprintf(p, "%s^%d", countstr, pu->dimCount);
	while (*p) p++;
    }
    if (pu->dimSpace < 0 || pu->dimTime < 0 || pu->dimCount < 0) {
	*p++ = '/';
	if (pu->dimSpace < 0) {
	    if (pu->dimSpace == -1)
		sprintf(p, "%s", spacestr);
	    else
		sprintf(p, "%s^%d", spacestr, -pu->dimSpace);
	    while (*p) p++;
	}
	if (pu->dimTime < 0) {
	    if (pu->dimTime == -1)
		sprintf(p, "%s", timestr);
	    else
		sprintf(p, "%s^%d", timestr, -pu->dimTime);
	    while (*p) p++;
	}
	if (pu->dimCount < 0) {
	    if (pu->dimCount == -1)
		sprintf(p, "%s", countstr);
	    else
		sprintf(p, "%s^%d", countstr, -pu->dimCount);
	    while (*p) p++;
	}
    }

    if (buf[0] == '\0') {
	if (pu->scaleCount == 1)
	    sprintf(buf, "x10");
	else if (pu->scaleCount != 0)
	    sprintf(buf, "x10^%d", pu->scaleCount);
    }
    else
	*p = '\0';

    return buf;
}

void
OMC_Desc::setScaleUnits(const pmUnits &units)
{
    _scaleUnits = units;
    _scaleFlag = OMC_true;
    setUnitStrs();
}
