/* ftpLibP.h - private header for ftpLib */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,20sep92,kdl	 written.
*/

#ifndef __INCftpLibPh
#define __INCftpLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "ftplib.h"

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	ftpLs (char *dirName);

#else

extern STATUS	ftpLs ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCftpLibPh */
