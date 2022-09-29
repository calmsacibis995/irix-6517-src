/* semSmLibP.h - private shared semaphore library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,29jan93,pme  added little endian support
01b,22sep92,rrr  added support for c++
01a,19jul92,pme  extracted from semSmLib v1c.
*/

#ifndef __INCsemSmLibPh
#define __INCsemSmLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "semsmlib.h"
#include "smdlllib.h"

/* typedefs */

#define SEM_TYPE_SM_BINARY      0x4     /* shared binary semaphore */
#define SEM_TYPE_SM_COUNTING    0x5     /* shared counting semaphore */

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

typedef struct  sm_semaphore    /* SHARED MEMORY SEMAPHORE */
    {
    UINT32      verify;         /* 0x00: semaphore verification */
    UINT32      objType;        /* 0x04: semaphore type */
    UINT32      lock;           /* 0x08: semaphore spin lock */
    SM_DL_LIST  smPendQ;        /* 0x0c: semaphore pend Queue */
    union
        {
        UINT32           flag;  /* 0x14: current state */
        UINT32           count; /* 0x14: semaphore counter */
        } state;

    } SM_SEMAPHORE;

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

/* variable declarations */

extern FUNCPTR semSmShowRtn;		/* shared semaphore show routine ptr */
extern FUNCPTR semSmInfoRtn;		/* shared semaphore info routine ptr */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void   semSmLibInit (void);
extern STATUS semSmBInit (SM_SEMAPHORE * pSem, int options,
                          SEM_B_STATE initialState);
extern STATUS semSmCInit (SM_SEMAPHORE * pSem, int options, int initialCount);
extern STATUS semSmGive (SM_SEM_ID smSemId);
extern STATUS semSmTake (SM_SEM_ID smSemId, int timeout);
extern STATUS semSmFlush (SM_SEM_ID smSemId);
extern int    semSmInfo (SM_SEM_ID smSemId, int idList[], int maxTasks);
extern int    semSmShow (SM_SEM_ID smSemId, int level);

#else   /* __STDC__ */

extern void   semSmLibInit ();
extern STATUS semSmBInit ();
extern STATUS semSmCInit ();
extern STATUS semSmGive ();
extern STATUS semSmTake ();
extern STATUS semSmFlush ();
extern int    semSmInfo ();
extern int    semSmShow ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsemSmLibPh */
