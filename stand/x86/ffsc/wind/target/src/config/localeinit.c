/* localeLibInit.c - locale library initialization */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,07sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include the locale ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef  __INClocaleLibInitc 
#define  __INClocaleLibInitc 

#include "vxworks.h"
#include "locale.h"

#undef localeconv		/* #undef needed for the MIPS compiler */

VOIDFUNCPTR localeFiles[] =
    {
    (VOIDFUNCPTR) localeconv,
    (VOIDFUNCPTR) setlocale
    };

#endif /* __INClocaleLibInitc */
