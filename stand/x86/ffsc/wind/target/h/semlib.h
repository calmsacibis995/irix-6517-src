/* semLib.h - semaphore library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
03c,14jul94,dvs  added non-ANSI prototype for semCreate/semOLibInit (SPR# 2648).
03b,22sep92,rrr  added support for c++
03a,04jul92,jcf  cleaned up.
02k,26may92,rrr  the tree shuffle
02j,18apr92,jmm  added prototype for semTerminate
02i,19nov91,rrr  shut up some ansi warnings.
02h,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
02g,10jun91.del  added pragma for gnu960 alignment.
02f,16oct90,shl  made #else ANSI style.
02e,05oct90,dnw  deleted private functions; doc tweaks
02d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
02c,27jun90,jcf  added defines for optimized version.
02a,26jun90,jcf  rewritten to provide binary/counting/mutex w/ one structure.
		 introduced semaphore options.
		 removed generic status codes.
01a,02jan90,jcf  written.
*/

#ifndef __INCsemLibh
#define __INCsemLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"

/* generic status codes */

#define S_semLib_INVALID_STATE			(M_semLib | 101)
#define S_semLib_INVALID_OPTION			(M_semLib | 102)
#define S_semLib_INVALID_QUEUE_TYPE		(M_semLib | 103)
#define S_semLib_INVALID_OPERATION		(M_semLib | 104)

/* semaphore options */

#define SEM_Q_MASK		0x3	/* q-type mask */
#define SEM_Q_FIFO		0x0	/* first in first out queue */
#define SEM_Q_PRIORITY		0x1	/* priority sorted queue */
#define SEM_DELETE_SAFE		0x4	/* owner delete safe (mutex opt.) */
#define SEM_INVERSION_SAFE	0x8	/* no priority inversion (mutex opt.) */

/* binary semaphore initial state */

typedef enum		/* SEM_B_STATE */
    {
    SEM_EMPTY,			/* 0: semaphore not available */
    SEM_FULL			/* 1: semaphore available */
    } SEM_B_STATE;

typedef struct semaphore *SEM_ID;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	semGive (SEM_ID semId);
extern STATUS 	semTake (SEM_ID semId, int timeout);
extern STATUS 	semFlush (SEM_ID semId);
extern STATUS 	semDelete (SEM_ID semId);
extern int 	semInfo (SEM_ID semId, int idList[], int maxTasks);
extern STATUS 	semBLibInit (void);
extern SEM_ID 	semBCreate (int options, SEM_B_STATE initialState);
extern STATUS 	semCLibInit (void);
extern SEM_ID 	semCCreate (int options, int initialCount);
extern STATUS 	semMLibInit (void);
extern SEM_ID 	semMCreate (int options);
extern STATUS 	semOLibInit (void);
extern SEM_ID 	semCreate (void);
extern void 	semShowInit (void);
extern STATUS 	semShow (SEM_ID semId, int level);

#else

extern STATUS 	semGive ();
extern STATUS 	semTake ();
extern STATUS 	semFlush ();
extern STATUS 	semDelete ();
extern int 	semInfo ();
extern STATUS 	semBLibInit ();
extern SEM_ID 	semBCreate ();
extern STATUS 	semCLibInit ();
extern SEM_ID 	semCCreate ();
extern STATUS 	semMLibInit ();
extern SEM_ID 	semMCreate ();
extern STATUS 	semOLibInit ();
extern SEM_ID 	semCreate ();
extern void 	semShowInit ();
extern STATUS 	semShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsemLibh */
