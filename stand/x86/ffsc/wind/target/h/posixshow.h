/* posixShow.h - posix show routines */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,06apr93,smb written
*/

#ifndef __INCposixshowh
#define __INCposixshowh

#include "semaphore.h"
#include "mqueue.h"

#ifdef __cplusplus
extern "C" {
#endif

extern CLASS_ID mqClassId;

/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

STATUS semaphoreShow (sem_t * semDesc, int level);
STATUS mqShow        (mqd_t mqDesc, int level);

#else   /* __STDC__ */

STATUS semaphoreShow ();
STATUS mqShow ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCposixshowh*/
