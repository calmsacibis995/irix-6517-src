/* loadElfLib.h - object module dependant loader library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01a,27oct92,ajm  written from loadEcoffLib.h
*/

#ifndef __INCloadElfLibh
#define __INCloadElfLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "symlib.h"

/* status codes */

#define S_loadElfLib_HDR_READ			(M_loadElfLib | 1)
#define S_loadElfLib_HDR_ERROR			(M_loadElfLib | 2)
#define S_loadElfLib_PHDR_MALLOC		(M_loadElfLib | 3)
#define S_loadElfLib_PHDR_READ			(M_loadElfLib | 4)
#define S_loadElfLib_SHDR_MALLOC		(M_loadElfLib | 5)
#define S_loadElfLib_SHDR_READ			(M_loadElfLib | 6)
#define S_loadElfLib_READ_SECTIONS		(M_loadElfLib | 7)
#define S_loadElfLib_LOAD_SECTIONS		(M_loadElfLib | 8)
#define S_loadElfLib_LOAD_PROG			(M_loadElfLib | 9)
#define S_loadElfLib_SYMTAB_ERROR		(M_loadElfLib | 10)
#define S_loadElfLib_RELA_SECTION		(M_loadElfLib | 11)

#define S_loadElfLib_JMPADDR_ERROR		(M_loadElfLib | 20)
#define S_loadElfLib_GPREL_REFERENCE		(M_loadElfLib | 21)
#define S_loadElfLib_UNRECOGNIZED_RELOCENTRY	(M_loadElfLib | 22)
#define S_loadElfLib_RELOC_ERROR		(M_loadElfLib | 23)
#define S_loadElfLib_UNSUPPORTED_ERROR		(M_loadElfLib | 24)
#define S_loadElfLib_REL_SECTION		(M_loadElfLib | 25)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS loadElfInit ();

#else	/* __STDC__ */

extern STATUS loadElfInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCloadElfLibh */
