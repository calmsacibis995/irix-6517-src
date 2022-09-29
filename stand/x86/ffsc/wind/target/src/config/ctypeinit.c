/* ctypeLibInit.c - ctype library initialization */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,07sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include the ctype ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef  __INCctypeLibInitc 
#define  __INCctypeLibInitc 


#include "vxworks.h"
#include "ctype.h"

#undef isalnum			/* #undef needed for the MIPS compiler */
#undef isalpha
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit
#undef tolower
#undef toupper

VOIDFUNCPTR ctypeFiles[] =
    {
    (VOIDFUNCPTR) isalnum,
    (VOIDFUNCPTR) isalpha,
    (VOIDFUNCPTR) iscntrl,
    (VOIDFUNCPTR) isdigit,
    (VOIDFUNCPTR) isgraph,
    (VOIDFUNCPTR) islower,
    (VOIDFUNCPTR) isprint,
    (VOIDFUNCPTR) ispunct,
    (VOIDFUNCPTR) isspace,
    (VOIDFUNCPTR) isupper,
    (VOIDFUNCPTR) isxdigit,
    (VOIDFUNCPTR) tolower,
    (VOIDFUNCPTR) toupper
    };

#endif /* __INCctypeLibInitc */
