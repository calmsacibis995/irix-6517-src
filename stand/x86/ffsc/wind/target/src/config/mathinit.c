/* mathLibInit.c - math library initialization */

/* Copyright 1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,17mar95,ism  Removed 5.1.1 math routines.
01b,09jan95,ism  Added array to link in math routines missing from 5.2
01a,07sep92,smb  written.
*/

/*
This file is used to include the math ANSI C library routines in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef  __INCmathLibInitc
#define  __INCmathLibInitc 

#include "vxworks.h"
#include "math.h"

extern void gccMathInit ();
extern void gccUssInit ();

VOIDFUNCPTR mathFiles[] =
    {
    (VOIDFUNCPTR) acos,
    (VOIDFUNCPTR) asin,
    (VOIDFUNCPTR) atan,
    (VOIDFUNCPTR) atan2,
    (VOIDFUNCPTR) ceil,
    (VOIDFUNCPTR) cos,
    (VOIDFUNCPTR) cosh,
    (VOIDFUNCPTR) exp,
    (VOIDFUNCPTR) fabs,
    (VOIDFUNCPTR) floor,
    (VOIDFUNCPTR) fmod,
    (VOIDFUNCPTR) frexp,
    (VOIDFUNCPTR) ldexp,
    (VOIDFUNCPTR) log,
    (VOIDFUNCPTR) log10,
    (VOIDFUNCPTR) modf,
    (VOIDFUNCPTR) pow,
    (VOIDFUNCPTR) sin,
    (VOIDFUNCPTR) sinh,
    (VOIDFUNCPTR) sqrt,
    (VOIDFUNCPTR) tan,
    (VOIDFUNCPTR) tanh,
    };

#endif /* __INCmathLibInitc */
