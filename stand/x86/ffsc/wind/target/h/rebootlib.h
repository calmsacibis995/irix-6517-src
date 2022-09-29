/* rebootLib.h - header file for rebootLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCrebootLibh
#define __INCrebootLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	rebootHookAdd (FUNCPTR rebootHook);
extern void 	reboot (int startType);

#else	/* __STDC__ */

extern STATUS 	rebootHookAdd ();
extern void 	reboot ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrebootLibh */
