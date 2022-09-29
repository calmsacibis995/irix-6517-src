/* assertLibInit.c - assert library initialization */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,07sep92,smb  written.
*/

/*
DESCRIPTION
This file is used to include the assert ANSI C library routine in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c

NOMANUAL
*/

#ifndef  __INCassertLibInitc 
#define  __INCassertLibInitc 


#include "vxworks.h"
#include "assert.h"

VOIDFUNCPTR assertFiles[] =
    {
    (VOIDFUNCPTR) __assert,
    };

#endif /* __INCassertLibInitc */
