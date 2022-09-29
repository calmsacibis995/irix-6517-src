/* mman.h - memory management library header file */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,05nov93,dvs  written
*/

#ifndef __INCmmanh
#define __INCmmanh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */
#include "vxworks.h"

/* memory locking flags */

#define MCL_CURRENT 	0x1		/* currently not used */
#define MCL_FUTURE	0x2		/* currently not used */

/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int mlockall (int flags);
extern int munlockall (void);
extern int mlock (const void * addr, size_t len);
extern int munlock (const void * addr, size_t len);

#else   /* __STDC__ */

extern int mlockall ();
extern int munlockall ();
extern int mlock ();
extern int munlock ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmmanh */
