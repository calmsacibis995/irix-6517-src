/* rdbLib.h - remote debugger header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,19sep92,smb  written
*/

#ifndef __INCrdbLibh
#define __INCrdbLibh

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)

STATUS rdbInit (void);

#else	/* __STDC__ */

STATUS rdbInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrdbLibh */
