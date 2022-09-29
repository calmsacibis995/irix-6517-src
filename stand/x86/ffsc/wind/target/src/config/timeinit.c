/* timeLibInit.c - time library initialization */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,07sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include the time ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef  __INCtimeLibInitc
#define  __INCtimeLibInitc


#include "vxworks.h"
#include "time.h"

VOIDFUNCPTR timeFiles[] =
    {
    (VOIDFUNCPTR) asctime,
    (VOIDFUNCPTR) clock,
    (VOIDFUNCPTR) ctime,
    (VOIDFUNCPTR) difftime,
    (VOIDFUNCPTR) gmtime,
    (VOIDFUNCPTR) localtime,
    (VOIDFUNCPTR) mktime,
    (VOIDFUNCPTR) strftime,
    (VOIDFUNCPTR) time
    };

#endif /* __INCtimeLibInitc */
