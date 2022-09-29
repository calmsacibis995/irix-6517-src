/* bootElfLib.h - ELF object module boot loader library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,25oct93,cd   created from bootEcoffLib.h v01b
*/

#ifndef __INCbootElfLibh
#define __INCbootElfLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS bootElfInit ();

#else	/* __STDC__ */

extern STATUS bootElfInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCbootElfLibh */
