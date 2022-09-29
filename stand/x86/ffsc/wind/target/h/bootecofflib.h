/* bootEcoffLib.h - extended COFF object module boot loader library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,23jul92,ajm  written
*/

#ifndef __INCbootEcoffLibh
#define __INCbootEcoffLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS bootEcoffInit ();

#else	/* __STDC__ */

extern STATUS bootEcoffInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCbootEcoffLibh */
