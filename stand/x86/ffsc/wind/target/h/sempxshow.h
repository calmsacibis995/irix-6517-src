/* semPxShow.h - posix semaphore show routines */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,06apr93,smb written
*/

#ifndef __INCsemPxShowh
#define __INCsemPxShowh

#include "semaphore.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS semPxShow (sem_t * semDesc, int level);
extern STATUS semPxShowInit (void);

#else   /* __STDC__ */

extern STATUS semPxShow ();
extern STATUS semPxShowInit ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsemPxShowh*/
