/* timers.h - POSIX time header */

/* Copyright 1991-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02f,29nov93,dvs  moved code to time.h
02e,30sep92,smb  corrected STDC prototype listing.
02d,22sep92,rrr  added support for c++
02c,22jul92,gae  fixed clock_id types; more POSIXy.
02b,25jul92,smb  moves contents of time.h to timers.h.
02a,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,30apr92,rrr  some preparation for posix signals.
01b,10feb91,gae  revised according to DEC review.
		   defined sigevent and sigval structures according to Draft;
		   added ENOSYS errno here, temporarily;
		   flagged definitions in wrong location with xPOSIX.
01a,16oct91,gae  written.
*/

/*
DESCRIPTION
This file provides header information for the POSIX time interface definitions.
*/

#ifndef __INCtimersh
#define __INCtimersh

#ifdef __cplusplus
extern "C" {
#endif

#include "time.h"

#ifdef __cplusplus
}
#endif

#endif /* __INCtimersh */
