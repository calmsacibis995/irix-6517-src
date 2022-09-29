/* loadAoutLib.h - a.out object module loader library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,18jun92,ajm  written for object module independant loadLib
*/

#ifndef __INCloadAoutLibh
#define __INCloadAoutLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "symlib.h"


/* status codes */

#define S_loadAoutLib_TOO_MANY_SYMBOLS		(M_loadAoutLib | 1)

/* function declarations */


#if defined(__STDC__) || defined(__cplusplus)

extern STATUS loadAoutInit ();
extern STATUS bootAoutInit ();

#else	/* __STDC__ */

extern STATUS loadAoutInit ();
extern STATUS bootAoutInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCloadAoutLibh */
