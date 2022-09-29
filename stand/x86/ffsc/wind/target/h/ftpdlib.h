/* ftpdLib.h - header file for ftpdLib.c */

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
01b,05oct90,dnw deleted ftpdWorkTask().
01a,05oct90,shl created.
*/

#ifndef __INCftpdLibh
#define __INCftpdLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	ftpdInit (int stackSize);
extern STATUS 	ftpdTask (void);
extern void 	ftpdDelete (void);

#else	/* __STDC__ */

extern STATUS 	ftpdInit ();
extern STATUS 	ftpdTask ();
extern void 	ftpdDelete ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCftpdLibh */
