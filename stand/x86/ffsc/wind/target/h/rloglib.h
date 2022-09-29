/* rlogLib.h - header file for rlogLib.c */

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

#ifndef __INCrlogLibh
#define __INCrlogLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	rlogInit (void);
extern STATUS 	rlogin (char *host);
extern void 	rlogChildTask (void);
extern void 	rlogInTask (int sock, int ptyMfd);
extern void 	rlogOutTask (int sock, int ptyMfd);
extern void 	rlogind (void);

#else	/* __STDC__ */

extern STATUS 	rlogInit ();
extern STATUS 	rlogin ();
extern void 	rlogChildTask ();
extern void 	rlogInTask ();
extern void 	rlogOutTask ();
extern void 	rlogind ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrlogLibh */
