/* xdr_bool_t.h - header file for xdr_bool_t.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCxdr_bool_th
#define __INCxdr_bool_th

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int 	xdr_bool_t (XDR *xdrs, int *objp);

#else	/* __STDC__ */

extern int 	xdr_bool_t ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCxdr_bool_th */
