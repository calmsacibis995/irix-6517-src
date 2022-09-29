/* localeP.h - private locale header file */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,11jul92,smb  written.
*/

/*
DESCRIPTION
This header file tries to group all the pieces related to the locale
library together. VxWorks does not implement the locale functionality.
Implementation requires a linked list of tables of the __linfo structure
defined below. The is not feasiable in a real-time environment.

INCLUDE FILE: locale.h

SEE ALSO: Plauger's ANSI C library, 1992.
*/

#include "locale.h"
#include "private/timep.h"

#ifndef __INClocalePh
#define __INClocalePh

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __linfo
    {
    const char *	__localeName;
    struct __linfo *	__next;		/* list */
    const unsigned *	__ctype;	/* FOR LC_CTYPE */
    unsigned char 	__mbcurmax;
    struct lconv 	__lc;	        /* LC_MONETARY, LC_NUMERIC */
    TIMELOCALE 		__times;	/* for LC_TIME */
    } __linfo;

#if FALSE	/* NOT IMPLEMENTED */
    const short *	__toUpper;	/* FOR LC_CTYPE */
    const short *	__toLower;	/* FOR LC_CTYPE */
    __statab		__mbstate;
    __statab 		__wcstate;
    __statab		__costate; 	/* for LC_COLLATE */
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INClocalePh */
