/* ramDrv.h - header file for ramDrv.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,18sep92,jcf  added include of blkIo.h.
01d,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCramDrvh
#define __INCramDrvh

#ifdef __cplusplus
extern "C" {
#endif

#include "blkio.h"

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	ramDrv (void);
extern BLK_DEV *ramDevCreate (char *ramAddr, int bytesPerSec, int secPerTrack,
			      int nSectors, int secOffset);

#else

extern STATUS 	ramDrv ();
extern BLK_DEV *ramDevCreate ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCramDrvh */
