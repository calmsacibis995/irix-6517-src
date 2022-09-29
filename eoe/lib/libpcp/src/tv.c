/*
 * Copyright 1995, Silicon Graphics, Inc.
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


#ident "$Id: tv.c,v 2.3 1997/08/21 04:58:30 markgw Exp $"

#include "pmapi.h"
#include "impl.h"
#include <sys/time.h>

/*
 * real additive time, *ap plus *bp
 */
double
__pmtimevalAdd(const struct timeval *ap, const struct timeval *bp)
{
     return (double)(ap->tv_sec + bp->tv_sec) + (double)(ap->tv_usec + bp->tv_usec)/1000000.0;
}

/*
 * real time difference, *ap minus *bp
 */
double
__pmtimevalSub(const struct timeval *ap, const struct timeval *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec) + (double)(ap->tv_usec - bp->tv_usec)/1000000.0;
}

/*
 * convert a timeval to a double (units = seconds)
 */

double
__pmtimevalToReal(const struct timeval *val)
{
    double dbl = (double)(val->tv_sec);
    dbl += (double)val->tv_usec / 1000000.0;
    return dbl;
}

/*
 * convert double to a timeval
 */

void
__pmtimevalFromReal(double dbl, struct timeval *val)
{
    val->tv_sec = (time_t)dbl;
    val->tv_usec = (long)(((dbl - (double)val->tv_sec) * 1000000.0));
}
