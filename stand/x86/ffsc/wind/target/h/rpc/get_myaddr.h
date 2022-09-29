/* get_myaddr.h - header file for get_myaddr.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCget_myaddrh
#define __INCget_myaddrh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	int       get_myaddress (struct sockaddr_in *addr);

#else

extern	int       get_myaddress ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCget_myaddrh */
