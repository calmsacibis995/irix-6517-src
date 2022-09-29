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
// $Id: tv.h,v 1.3 1999/04/30 01:44:04 kenmcd Exp $
//
#ifndef _TV_H
#define _TV_H

#include <sys/time.h>
#include "pmapi.h"

extern double _pmtv2double(struct timeval *);
extern struct timeval *_pmdouble2tv(double, struct timeval *);
extern void _pmtvadd(struct timeval *, int, int); /* add +- time units */

enum _pmtvUnits {
    _PMTV_UNITS_MSEC,
    _PMTV_UNITS_SEC,
    _PMTV_UNITS_MIN,
    _PMTV_UNITS_HOUR,
    _PMTV_UNITS_DAY,
    _PMTV_UNITS_WEEK
};

// Class to store a time to millisecond precision.
//
class _pmtvTimeVal 
{
public:
    _pmtvTimeVal(void);
    _pmtvTimeVal(const struct timeval &);
    _pmtvTimeVal(const float &, const _pmtvUnits &); // value plus units
    _pmtvTimeVal(const int &, const int &); // value plus XTB units

    _pmtvTimeVal &operator-(const _pmtvTimeVal &);
    _pmtvTimeVal &operator+(const _pmtvTimeVal &);
    _pmtvTimeVal &operator/(const int &);
    int operator<(const _pmtvTimeVal &);
    int operator!=(const _pmtvTimeVal &);

    float getValue(const _pmtvUnits &);
    void getTimeval(struct timeval &);
    void getXTB(int &, int &); // ival and units
    int getXTB(const int &); //  units are specified

    int checkMax(int &, char *, const int &); // check if the 24 day limit applies

private:
    int	sec;
    int	msec;
};


#endif
