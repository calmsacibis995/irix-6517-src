/* loadEcoffLib.h - object module dependant loader library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,12sep92,ajm  got rid of bootEcoffInit prototype
01a,27may92,ajm  written
*/

#ifndef __INCloadEcoffLibh
#define __INCloadEcoffLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "symlib.h"

/* status codes */

#define S_loadEcoffLib_HDR_READ			(M_loadEcoffLib | 1)
#define S_loadEcoffLib_OPTHDR_READ		(M_loadEcoffLib | 2)
#define S_loadEcoffLib_SCNHDR_READ		(M_loadEcoffLib | 3)
#define S_loadEcoffLib_READ_SECTIONS		(M_loadEcoffLib | 4)
#define S_loadEcoffLib_LOAD_SECTIONS		(M_loadEcoffLib | 5)
#define S_loadEcoffLib_RELOC_READ		(M_loadEcoffLib | 6)
#define S_loadEcoffLib_SYMHDR_READ		(M_loadEcoffLib | 7)
#define S_loadEcoffLib_EXTSTR_READ		(M_loadEcoffLib | 8)
#define S_loadEcoffLib_EXTSYM_READ		(M_loadEcoffLib | 9)
#define S_loadEcoffLib_GPREL_REFERENCE		(M_loadEcoffLib | 10)
#define S_loadEcoffLib_JMPADDR_ERROR		(M_loadEcoffLib | 11)
#define S_loadEcoffLib_NO_REFLO_PAIR		(M_loadEcoffLib | 12)
#define S_loadEcoffLib_UNRECOGNIZED_RELOCENTRY	(M_loadEcoffLib | 13)
#define S_loadEcoffLib_REFHALF_OVFL		(M_loadEcoffLib | 14)
#define S_loadEcoffLib_UNEXPECTED_SYM_CLASS	(M_loadEcoffLib | 15)
#define S_loadEcoffLib_UNRECOGNIZED_SYM_CLASS	(M_loadEcoffLib | 16)
#define S_loadEcoffLib_FILE_READ_ERROR		(M_loadEcoffLib | 17)
#define S_loadEcoffLib_FILE_ENDIAN_ERROR	(M_loadEcoffLib | 18)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS loadEcoffInit ();

#else	/* __STDC__ */

extern STATUS loadEcoffInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCloadEcoffLibh */
