/* semSmLib.h - shared semaphore library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,29sep92,pme  changed semSm[BC]Create to sem[BC]SmCreate.
01c,22sep92,rrr  added support for c++
01b,28jul92,pme  added #include "semlib.h".
01a,19jul92,pme  written.
*/

#ifndef __INCsemSmLibh
#define __INCsemSmLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "semlib.h"
#include "smdlllib.h"


typedef struct sm_semaphore * SM_SEM_ID;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern    SEM_ID       semBSmCreate (int options, SEM_B_STATE initialState);
extern    SEM_ID       semCSmCreate (int options, int initialCount);
extern    void         semSmShowInit ();

#else   /* __STDC__ */

extern    SEM_ID       semBSmCreate ();
extern    SEM_ID       semCSmCreate ();
extern    void         semSmShowInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsemSmLibh */
