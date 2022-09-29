/* semPxLibP.h - private POSIX semaphore library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,12jan94,kdl  changed semaphoreInit() to semPxLibInit().
01b,17dec93,dvs  added semPxClassId
01a,29nov93,dvs  written
*/

#ifndef __INCsemPxLibPh
#define __INCsemPxLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"

extern CLASS_ID semPxClassId;			/* POSIX sem class ID */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	semPxLibInit (void);

#else	/* __STDC__ */

extern STATUS	semPxLibInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsemPxLibPh */
