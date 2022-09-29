/* stringLibInit.c - string library initialization */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,07sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include the string ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef  __INCstringLibInitc
#define  __INCstringLibInitc


#include "vxworks.h"
#include "string.h"

VOIDFUNCPTR stringFiles[] =
    {
    (VOIDFUNCPTR) memchr,
    (VOIDFUNCPTR) memcmp,
    (VOIDFUNCPTR) memcpy,
    (VOIDFUNCPTR) memset,
    (VOIDFUNCPTR) memmove,
    (VOIDFUNCPTR) strcat,
    (VOIDFUNCPTR) strchr,
    (VOIDFUNCPTR) strcmp,
    (VOIDFUNCPTR) strcoll,
    (VOIDFUNCPTR) strcpy,
    (VOIDFUNCPTR) strcspn,
    (VOIDFUNCPTR) strerror,
    (VOIDFUNCPTR) strlen,
    (VOIDFUNCPTR) strncat,
    (VOIDFUNCPTR) strncmp,
    (VOIDFUNCPTR) strncpy,
    (VOIDFUNCPTR) strpbrk,
    (VOIDFUNCPTR) strrchr,
    (VOIDFUNCPTR) strspn,
    (VOIDFUNCPTR) strstr,
    (VOIDFUNCPTR) strtok,
    (VOIDFUNCPTR) strtok_r,
    (VOIDFUNCPTR) strxfrm
    };

#endif /* __INCstringLibInitc */
