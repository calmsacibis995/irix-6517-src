/* wdLibP.h - private watchdog timer library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,04jul92,jcf  created from v01h wdLib.h.
*/

#ifndef __INCwdLibPh
#define __INCwdLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "wdlib.h"
#include "classlib.h"
#include "qlib.h"
#include "private/objlibp.h"

/* watchdog status values */

#define WDOG_OUT_OF_Q	0x0	/* valid watchdog not in tick queue */
#define WDOG_IN_Q	0x1	/* valid watchdog in tick queue */
#define WDOG_DEAD	0x2	/* terminated watchdog */

typedef struct wdog	/* WDOG */
    {
    OBJ_CORE	objCore;	/* object management */
    Q_NODE	tickNode;	/* multi-way queue node for tick queue */
    USHORT	status;		/* status of watchdog */
    USHORT	deferStartCnt;	/* number of wdStarts still in work queue */
    FUNCPTR	wdRoutine;	/* routine to call upon expiration */
    int		wdParameter;	/* arbitrary parameter to routine */
    } WDOG;

/* variable definitions */

extern CLASS_ID wdClassId;		/* watchdog class id */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	wdInit (WDOG *pWdog);
extern STATUS	wdTerminate (WDOG_ID wdId);
extern STATUS	wdDestroy (WDOG_ID wdId, BOOL dealloc);
extern void	wdTick (void);

#else	/* __STDC__ */

extern STATUS	wdInit ();
extern STATUS	wdTerminate ();
extern STATUS	wdDestroy ();
extern void	wdTick ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCwdLibPh */
