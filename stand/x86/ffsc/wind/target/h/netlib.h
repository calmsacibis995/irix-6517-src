/* netLib.h - header file for netLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01b,05oct90,dnw deleted private routine.
01a,05oct90,shl created.
*/

#ifndef __INCnetLibh
#define __INCnetLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	netJobAdd (FUNCPTR routine, int param1, int param2, int param3,
		           int param4, int param5);
extern STATUS 	netLibInit (void);
extern void 	netErrnoSet (int status);
extern void 	netTask (void);
extern void 	schednetisr (void);

#else	/* __STDC__ */

extern STATUS 	netJobAdd ();
extern STATUS 	netLibInit ();
extern void 	netErrnoSet ();
extern void 	netTask ();
extern void 	schednetisr ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCnetLibh */
