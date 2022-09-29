/* ledLib.h - header file for ledLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCledLibh
#define __INCledLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	ledClose (int led_id);
extern int 	ledOpen (int inFd, int outFd, int histSize);
extern int 	ledRead (int led_id, char *string, int maxBytes);
extern void 	ledControl (int led_id, int inFd, int outFd, int histSize);

#else	/* __STDC__ */

extern STATUS 	ledClose ();
extern int 	ledOpen ();
extern int 	ledRead ();
extern void 	ledControl ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCledLibh */
