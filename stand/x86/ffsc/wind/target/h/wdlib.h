/* wdLib.h - watchdog timer library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01h,26may92,rrr  the tree shuffle
01g,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01f,23oct90,shl  changed wdStart()'s third parameter type to VOIDFUNCPTR.
01e,05oct90,dnw  deleted private functions.
01d,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01c,26jun90,jcf  removed generic status codes.
01b,17apr90,jcf  subsumed into wind 2.0.
01a,21may84,dnw  written
*/

#ifndef __INCwdLibh
#define __INCwdLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"

/* typedefs */

typedef struct wdog *WDOG_ID;		/* watchdog id */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	wdLibInit (void);
extern WDOG_ID 	wdCreate (void);
extern STATUS 	wdDelete (WDOG_ID wdId);
extern STATUS 	wdStart (WDOG_ID wdId, int delay, FUNCPTR pRoutine,
			 int parameter);
extern STATUS 	wdCancel (WDOG_ID wdId);
extern void 	wdShowInit (void);
extern STATUS 	wdShow (WDOG_ID wdId);

#else	/* __STDC__ */

extern STATUS 	wdLibInit ();
extern WDOG_ID 	wdCreate ();
extern STATUS 	wdDelete ();
extern STATUS 	wdStart ();
extern STATUS 	wdCancel ();
extern void 	wdShowInit ();
extern STATUS 	wdShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCwdLibh */
