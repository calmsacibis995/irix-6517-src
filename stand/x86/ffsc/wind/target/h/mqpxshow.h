/* mqPxShow.h - private POSIX message queue library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,02dec93,dvs  written
*/

#ifndef __INCmqPxShowh
#define __INCmqPxShowh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	mqPxShowInit (void);

#else	/* __STDC__ */

extern STATUS	mqPxShowInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmqPxShowh */
