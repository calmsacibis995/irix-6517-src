/* bootLoadLib.h - object module boot loader library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,04jul92,jcf  cleaned up.
01b,23jun92,ajm  fixed function prototype for bootLoadModule
01a,01jun92,ajm  written
*/

#ifndef __INCbootLoadLibh
#define __INCbootLoadLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "symlib.h"

/* status codes */

#define S_bootLoadLib_ROUTINE_NOT_INSTALLED		(M_bootLoadLib | 1)

/* data structures */

extern FUNCPTR bootLoadRoutine;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	bootLoadModule (int fd, FUNCPTR *pEntry);

#else	/* __STDC__ */

extern STATUS 	bootLoadModule ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCbootLoadLibh */
