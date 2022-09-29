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


//
// $Id: tv.c++,v 1.4 1999/04/30 01:44:04 kenmcd Exp $
//
#include "tv.h"
#include "impl.h"
#include "pmapi.h"

double
_pmtv2double(struct timeval *tv)
{
    return((double)tv->tv_sec + (double)tv->tv_usec / 1000000.0);
}

struct timeval *
_pmdouble2tv(double tm, struct timeval *tv)
{
    tv->tv_sec = (int)tm;
    tv->tv_usec = (int)(1000000 * (tm - (int)tm));
    return tv;
}


/*
 * a := a + b for struct timevals
 */
static void
tadd(struct timeval *a, struct timeval *b)
{
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
	a->tv_usec -= 1000000;
	a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
}

/*
 * a := a - b for struct timevals, but result is never less than zero
 */
static void
tsub(struct timeval *a, struct timeval *b)
{
    a->tv_usec -= b->tv_usec;
    if (a->tv_usec < 0) {
	a->tv_usec += 1000000;
	a->tv_sec--;
    }
    a->tv_sec -= b->tv_sec;
    if (a->tv_sec < 0) {
	/* clip negative values at zero */
	a->tv_sec = 0;
	a->tv_usec = 0;
    }
}

/*
 * add +-ms to struct timeval
 */
void
_pmtvadd(struct timeval *tv, int ms, int mode)
{
    struct timeval operand;

    if (ms < 0) {
	ms = -ms;
	pmXTBdeltaToTimeval(ms, mode, &operand);
	tsub(tv, &operand);
    }
    else {
	pmXTBdeltaToTimeval(ms, mode, &operand);
	tadd(tv, &operand);
    }
}

// Class to store a time to millisecond precision.
//
static const double secsIn24Days = 2073600.0;

_pmtvTimeVal::_pmtvTimeVal(void) : sec(0), msec(0) {}

_pmtvTimeVal::_pmtvTimeVal(const struct timeval &tval)
   : sec(tval.tv_sec), msec(tval.tv_usec / 1000)
{}

_pmtvTimeVal::_pmtvTimeVal(const float &val, const _pmtvUnits &units)
    : sec(0), msec(0)
{
    switch(units) {
	case _PMTV_UNITS_MSEC:
	    msec = (int)val;
	    break;
	case _PMTV_UNITS_SEC:
	    sec = (int)val;
	    msec = (int)((val - sec) * 1000); // 1sec = 1000msec
	    break;
	case _PMTV_UNITS_MIN:
	    sec = (int)(val * 60); // 1min = 60sec
	    msec = (int)((val * 60000) - (sec * 1000)); // 1min = (60 * 1000)msec
	    break;
	case _PMTV_UNITS_HOUR:
	    sec = (int)(val * 3600); // 1hr = (60 * 60)sec
	    // millisecond precision intentionally dropped
	    break;
	case _PMTV_UNITS_DAY:
	    sec = (int)(val * 86400); // 1day = (60 * 60 * 24)sec
	    // millisecond precision intentionally dropped
	    break;
	case _PMTV_UNITS_WEEK:
	    sec = (int)(val * 604800); // 1week = (60 * 60 * 24 * 7)sec
	    // millisecond precision intentionally dropped
	    break;
    }
}

_pmtvTimeVal::_pmtvTimeVal(const int &val, const int &units)
    : sec(0), msec(0)
{
    switch(PM_XTB_GET(units)) {
	case PM_TIME_NSEC:
	    msec = (int)(val / (1000.0 * 1000.0));
	    break;
	case PM_TIME_USEC:
	    msec = (int)(val / 1000.0);
	    break;
	case PM_TIME_MSEC:
	    msec = val;
	    break;
	case PM_TIME_SEC:
	    sec = val;
	    break;
	case PM_TIME_MIN:
	    sec = val * 60;
	    break;
	case PM_TIME_HOUR:
	    sec = val * 60 * 60;
	    break;
	default:
	    // XTB not in use
	    msec = val;
	    break;
    }
}

_pmtvTimeVal &_pmtvTimeVal::operator-(const _pmtvTimeVal &tval)
{
    double us = sec + msec / 1000.0;
    double them = tval.sec + tval.msec / 1000.0;
    us -= them;
    sec = (int)us;
    msec = (int)((us - sec) * 1000.0);

    return *this;
}

_pmtvTimeVal &_pmtvTimeVal::operator+(const _pmtvTimeVal &tval)
{
    double us = sec + msec / 1000.0;
    double them = tval.sec + tval.msec / 1000.0;
    us += them;
    sec = (int)us;
    msec = (int)((us - sec) * 1000);

    return *this;
}

_pmtvTimeVal &_pmtvTimeVal::operator/(const int &dval)
{
    double us = sec + msec / 1000.0;
    us = us / dval - 0.005; // want to round down bug #552900
    sec = (int)us;
    msec = (int)((us - sec) * 1000);

    return *this;
}

int _pmtvTimeVal::operator<(const _pmtvTimeVal &tval)
{
    if ((sec + msec / 1000.0) < (tval.sec + tval.msec / 1000.0))
	return 1;
    return 0;
}

int _pmtvTimeVal::operator!=(const _pmtvTimeVal &tval)
{
    if (sec != tval.sec || msec != tval.msec)
	return 1;
    return 0;
}

float _pmtvTimeVal::getValue(const _pmtvUnits &units)
{
    float rval; 

    switch(units) {
	case _PMTV_UNITS_MSEC:
	    rval = msec + sec * 1000.0;
	    break;
	case _PMTV_UNITS_SEC:
	    rval = sec + msec / 1000.0;
	    break;
	case _PMTV_UNITS_MIN:
	    rval = (sec + msec / 1000.0) / 60.0;
	    break;
	case _PMTV_UNITS_HOUR:
	    rval = (sec + msec / 1000.0) / (60.0 * 60.0);
	    break;
	case _PMTV_UNITS_DAY:
	    rval = (sec + msec / 1000.0) / (60.0 * 60.0 * 24.0);
	    break;
	case _PMTV_UNITS_WEEK:
	    rval = (sec + msec / 1000.0) / (60.0 * 60.0 * 24.0 * 7.0);
	    break;
    }
    return rval;
}

void _pmtvTimeVal::getTimeval(struct timeval &tval)
{
    tval.tv_sec = sec;
    tval.tv_usec = msec * 1000;
}

// This should calculate a value with the lowest possible precision
// so the result will be in either seconds or milliseconds.
void _pmtvTimeVal::getXTB(int &ival, int &units)
{
    // If its more than 24 days, use seconds, otherwise use milliseconds
    double us = sec + msec / 1000.0;
    if (us > secsIn24Days) {
	ival = sec + (int)(msec / 1000);
	units = (units & 0x0000ffff) | PM_XTB_SET(PM_TIME_SEC);
    }
    else {
	ival = sec * 1000 + msec;
	units = (units & 0x0000ffff) | PM_XTB_SET(PM_TIME_MSEC);
    }
}

//  units are specified
int _pmtvTimeVal::getXTB(const int &units)
{
    int rval;
    
    switch(PM_XTB_GET(units)) {
	case PM_TIME_NSEC:
	    rval = (sec * 1000 + msec) * 1000 * 1000;
	    break;
	case PM_TIME_USEC:
	    rval = (sec * 1000 + msec) * 1000;
	    break;
	case PM_TIME_MSEC:
	    rval = sec * 1000 + msec;
	    break;
	case PM_TIME_SEC:
	    rval = sec + (int)(msec / 1000.0);
	    break;
	case PM_TIME_MIN:
	    rval = (int)((sec + (int)(msec / 1000.0)) / 60.0);
	    break;
	case PM_TIME_HOUR:
	    rval = (int)((sec + (int)(msec / 1000.0)) / (60.0 * 60.0));
	    break;
	default:
	    // XTB not in use, default to milliseconds
	    rval = sec * 1000 + msec;
	    break;
    }

    return rval;
}

int _pmtvTimeVal::checkMax(int &mode, char *buf, const int &buflen)
{
    // A maximum of 24 days applies if a xtended time intervals 
    // are not being used, or units are milliseconds

    switch(PM_XTB_GET(mode)) {
	case PM_TIME_NSEC: // not implemented
	case PM_TIME_USEC: // not implemented
	case PM_TIME_SEC:
	case PM_TIME_MIN:
	case PM_TIME_HOUR:
	    break;
	case PM_TIME_MSEC:
	    // Milliseconds have an imposed maximum
	    { // new scope for secval field
	    double secval = sec + msec / 1000.0;
	    if (secval > secsIn24Days) {
		return 0;
	    }
	    }
	    break;
	default:
	    // XTB not in use, restriction applies
	    { // new scope for secval field
	    double secval = sec + msec / 1000.0;
	    if (secval > secsIn24Days) {
		static char *err_msg = "At least one currently connected client is not using extended time interval units,\n hence the maximum update interval allowed is 24 days.";
		if (strlen(err_msg) < buflen) 
		    sprintf(buf, "%s", err_msg);
		return 0;
	    }
	    }
	    break;
	}

    return 1;
}

