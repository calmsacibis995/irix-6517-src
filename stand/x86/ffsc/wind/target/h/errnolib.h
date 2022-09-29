/* errnoLib.h - error status library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,22sep92,rrr  added support for c++
01i,18jul92,smb  added include errno.h.
01i,09jul92,smb  move function declarations from errno.h
01h,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,26nov91,llk  added S_errnoLib_NO_STAT_SYM_TBL.
		 removed errno and function prototypes (already in errno.h).
01e,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01c,02mar90,jcf  changed to support _errno.
01b,05jun88,dnw  changed stsLib to errnoLib.
01a,29apr88,gae  written.
*/

#ifndef __INCerrnoLibh
#define __INCerrnoLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "errno.h"

/* status codes */

#define S_errnoLib_NO_STAT_SYM_TBL      (M_errnoLib | 1)

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS   errnoOfTaskSet (int taskId, int errorValue);
extern STATUS   errnoSet (int errorValue);
extern int      errnoGet (void);
extern int      errnoOfTaskGet (int taskId);

#else   /* __STDC__ */

extern STATUS   errnoOfTaskSet ();
extern STATUS   errnoSet ();
extern int      errnoGet ();
extern int      errnoOfTaskGet ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCerrnoLibh */
