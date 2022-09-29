/* fcntl.h - standard header */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,18sep92,smb  added prototypes for open and creat.
01d,29jul92,smb  rearranged for the stdio library.
01c,04jul92,jcf  cleaned up.
01b,26may92,rrr  the tree shuffle
01a,05dec91,rrr  written.
*/

#ifndef __INCfcntlh
#define __INCfcntlh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"
#include "sys/fcntlcom.h"

#define	O_NDELAY	_FNDELAY	/* Non-blocking I/O (4.2 style) */

#if defined(__STDC__) || defined(__cplusplus)

extern int      open (const char *name, int flags, int mode );
extern int      creat (const char *name, int mode );

#else	/* __STDC__ */

extern int      open ();
extern int      creat ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCfcntlh */
