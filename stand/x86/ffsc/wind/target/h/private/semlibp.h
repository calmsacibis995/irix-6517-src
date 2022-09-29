/* semLibP.h - private semaphore library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,27jul92,jcf  changed semMTakeKern to semMPendQPut.
01b,19jul92,pme  added external declaration of shared sem show routine.
01a,04jul92,jcf  created.
*/

#ifndef __INCsemLibPh
#define __INCsemLibPh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE
#include "vxworks.h"
#include "vwmodnum.h"
#include "semlib.h"
#include "qlib.h"
#include "private/classlibp.h"
#include "private/objlibp.h"

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* semaphore types */

#define MAX_SEM_TYPE		8	/* maximum # of sem classes */
#define SEM_TYPE_MASK		0x7	/* semaphore class mask */

#define SEM_TYPE_BINARY		0x0	/* binary semaphore */
#define SEM_TYPE_MUTEX		0x1	/* mutual exclusion semaphore */
#define SEM_TYPE_COUNTING	0x2	/* counting semaphore */
#define SEM_TYPE_OLD		0x3	/* VxWorks 4.0 semaphore */

typedef struct semaphore /* SEMAPHORE */
    {
    OBJ_CORE	objCore;	/* 0x00: object management */
    UINT8	semType;	/* 0x04: semaphore type */
    UINT8	options;	/* 0x05: semaphore options */
    UINT16	recurse;	/* 0x06: semaphore recursive take count */
    Q_HEAD	qHead;		/* 0x08: blocked task queue head */
    union
	{
	UINT		 count;	/* 0x18: current state */
	struct windTcb	*owner;	/* 0x18: current state */
	} state;

    } SEMAPHORE;

#define semCount	state.count
#define semOwner	state.owner

#if CPU_FAMILY==I960
#pragma align 0				/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

/* variable declarations */

extern OBJ_CLASS	semClass;		/* semaphore object class */
extern CLASS_ID		semClassId;		/* semaphore class id */
extern FUNCPTR		semGiveTbl [];		/* semGive() methods */
extern FUNCPTR		semTakeTbl [];		/* semTake() methods */
extern FUNCPTR		semFlushTbl [];		/* semFlush() methods */
extern FUNCPTR		semGiveDeferTbl [];	/* semGiveDefer() methods */
extern FUNCPTR		semFlushDeferTbl [];	/* semFlushDefer() methods */
extern int		semMGiveKernWork;	/* semMGiveKern() parameter */

extern FUNCPTR  semSmShowRtn;	/* shared semaphore show routine pointer */
extern FUNCPTR  semSmInfoRtn;	/* shared semaphore info routine pointer */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	semLibInit (void);
extern STATUS	semTerminate (SEM_ID semId);
extern STATUS	semDestroy (SEM_ID semId, BOOL dealloc);
extern STATUS	semGiveDefer (SEM_ID semId);
extern STATUS	semFlushDefer (SEM_ID semId);
extern STATUS	semInvalid (SEM_ID semId);
extern STATUS	semIntRestrict (SEM_ID semId);
extern STATUS	semQInit (SEMAPHORE *pSemaphore, int options);
extern STATUS	semQFlush (SEM_ID semId);
extern void	semQFlushDefer (SEM_ID semId);
extern STATUS	semBInit (SEMAPHORE *pSem,int options,SEM_B_STATE initialState);
extern STATUS	semBCoreInit (SEMAPHORE *pSemaphore, int options,
			      SEM_B_STATE initialState);
extern STATUS	semBGive (SEM_ID semId);
extern STATUS	semBTake (SEM_ID semId, int timeout);
extern void	semBGiveDefer (SEM_ID semId);
extern STATUS	semMInit (SEMAPHORE *pSem, int options);
extern STATUS	semMCoreInit (SEMAPHORE *pSemaphore, int options);
extern STATUS	semMGive (SEM_ID semId);
extern STATUS	semMTake (SEM_ID semId, int timeout);
extern STATUS	semMGiveForce (SEM_ID semId);
extern STATUS	semMGiveKern (SEM_ID semId);
extern STATUS	semMPendQPut (SEM_ID semId, int timeout);
extern STATUS	semCInit (SEMAPHORE *pSem,int options,int initialCount);
extern STATUS	semCCoreInit (SEMAPHORE *pSemaphore, int options,
			      int initialCount);
extern STATUS	semCGive (SEM_ID semId);
extern STATUS	semCTake (SEM_ID semId, int timeout);
extern void	semCGiveDefer (SEM_ID semId);
extern STATUS	semOTake (SEM_ID semId);

#else

extern STATUS	semLibInit ();
extern STATUS	semTerminate ();
extern STATUS	semDestroy ();
extern STATUS	semGiveDefer ();
extern STATUS	semFlushDefer ();
extern STATUS	semInvalid ();
extern STATUS	semIntRestrict ();
extern STATUS	semQInit ();
extern STATUS	semQFlush ();
extern void	semQFlushDefer ();
extern STATUS	semBInit ();
extern STATUS	semBCoreInit ();
extern STATUS	semBGive ();
extern STATUS	semBTake ();
extern void	semBGiveDefer ();
extern STATUS	semCInit ();
extern STATUS	semCCoreInit ();
extern STATUS	semCGive ();
extern STATUS	semCTake ();
extern void	semCGiveDefer ();
extern STATUS	semMInit ();
extern STATUS	semMCoreInit ();
extern STATUS	semMGive ();
extern STATUS	semMTake ();
extern STATUS	semMGiveForce ();
extern STATUS	semMGiveKern ();
extern STATUS	semMPendQPut ();
extern STATUS	semOTake ();

#endif	/* __STDC__ */

#else	/* _ASMLANGUAGE */

#define	SEM_TYPE		0x04		/* offsets into SEMAPHORE */
#define	SEM_OPTIONS		0x05
#define	SEM_RECURSE		0x06
#define	SEM_Q_HEAD		0x08
#define	SEM_STATE		0x18

#endif	/* _ASMLANGUAGE */

#define SEM_M_Q_GET		0x1		/* semMGiveKernWork() defines */
#define SEM_M_SAFE_Q_FLUSH	0x2
#define SEM_M_PRI_RESORT	0x4

#ifdef __cplusplus
}
#endif

#endif /* __INCsemLibPh */
