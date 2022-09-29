/* routeLib.h - header file for the network routing library */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02d,11jul94,dzb  added prototype for routeNetAdd() (SPR #3395).
02c,22sep92,rrr  added support for c++
02b,27jul92,elh  moved routeShow to netShow.
02a,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,02mar92,elh  added routeCmd.
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01c,07aug90,shl  added IMPORT type to function declarations.
01b,16nov87,llk  documentation
01a,01nov87,llk  written
*/

#ifndef __INCrouteLibh
#define __INCrouteLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"


/* status codes */

#define S_routeLib_ILLEGAL_INTERNET_ADDRESS		(M_routeLib | 1)
#define S_routeLib_ILLEGAL_NETWORK_NUMBER		(M_routeLib | 2)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	routeAdd (char *destination, char *gateway);
extern STATUS 	routeDelete (char *destination, char *gateway);
extern STATUS 	routeNetAdd (char *destination, char *gateway);
extern STATUS 	routeCmd (int destInetAddr, int gateInetAddr, int ioctlCmd);

#else	/* __STDC__ */

extern STATUS 	routeAdd ();
extern STATUS 	routeDelete ();
extern STATUS 	routeNetAdd ();
extern STATUS 	routeCmd ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrouteLibh */
