/* unixLib.h - UNIX kernel compatability library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,26may92,rrr  the tree shuffle
01a,01apr92,elh  written.
*/

#ifndef __INCunixLibh
#define __INCunixLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "semlib.h"

/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int 	splnet (void);
extern int 	splimp (void);
extern void 	splx (int x);
extern void 	panic (char *msg);
extern void 	wakeup (SEM_ID semId);

#else	/* __STDC__ */

extern int	splnet ();
extern int	splimp ();
extern void	splx ();
extern void 	panic ();
extern void 	wakeup ();

#endif	/* __STDC__ */


#ifdef __cplusplus
}
#endif

#endif /* __INCunixLibh */
