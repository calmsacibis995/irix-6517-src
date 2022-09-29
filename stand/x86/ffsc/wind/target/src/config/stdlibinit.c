/* stdlibLibInit.c - stdlib library initialization */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,21sep92,smb  added some more functions.
01a,07sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include the stdlib ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef  __INCstdlibLibInitc
#define  __INCstdlibLibInitc


#include "vxworks.h"
#include "stdlib.h"

VOIDFUNCPTR stdlibFiles[] =
    {
    (VOIDFUNCPTR) abort,
    (VOIDFUNCPTR) abs,
    (VOIDFUNCPTR) atexit,
    (VOIDFUNCPTR) atof,
    (VOIDFUNCPTR) atoi,
    (VOIDFUNCPTR) atol,
    (VOIDFUNCPTR) bsearch,
    (VOIDFUNCPTR) calloc,
    (VOIDFUNCPTR) div,
    (VOIDFUNCPTR) exit,
    (VOIDFUNCPTR) free,
    (VOIDFUNCPTR) getenv,
    (VOIDFUNCPTR) labs,
    (VOIDFUNCPTR) ldiv,
    (VOIDFUNCPTR) malloc,
    (VOIDFUNCPTR) mblen,
    (VOIDFUNCPTR) qsort,
    (VOIDFUNCPTR) rand,
    (VOIDFUNCPTR) realloc,
    (VOIDFUNCPTR) strtod,
    (VOIDFUNCPTR) strtol,
    (VOIDFUNCPTR) strtoul,
    (VOIDFUNCPTR) system
    };

#endif /* __INCstdlibLibInitc */
