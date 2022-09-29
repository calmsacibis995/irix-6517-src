/* envLib.h - environment varable library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,22sep92,rrr  added support for c++
01h,19jul92,smb  moved prototype for getenv to stdlib.h
                 added include stdlib.h
01g,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,09dec91,rrr  fixed bad prototype (envShow).
01d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,01aug90,jcf  cleanup.
01a,12jul90,rdc  written.
*/


#ifndef __INCenvLibh
#define __INCenvLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "tasklib.h"
#include "stdlib.h"

/*******************************************************************************
*
* environ - environment pointer
*
* The following macro is provided for code that manipulates the environment
* variable array directly.
*/

#define environ 							\
    (								      	\
    (taskIdCurrent->ppEnviron == NULL) ? 				\
    ppGlobalEnviron : taskIdCurrent->ppEnviron 				\
    )

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	envLibInit (BOOL installHooks);
extern STATUS 	envPrivateCreate (int taskId, int envSource);
extern STATUS 	envPrivateDestroy (int taskId);
extern STATUS 	putenv (char *pEnvString);
extern void 	envShow (int taskId);

#else	/* __STDC__ */

extern STATUS 	envLibInit ();
extern STATUS 	envPrivateCreate ();
extern STATUS 	envPrivateDestroy ();
extern STATUS 	putenv ();
extern void 	envShow ();

#endif	/* __STDC__ */
#ifdef __cplusplus
}
#endif

#endif /* __INCenvLibh */
