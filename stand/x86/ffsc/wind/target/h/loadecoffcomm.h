/* loadEcoffComm.h - UNIX ECOFF object module common library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,23jul92,jmm  added include of ecoff.h
01a,23jul92,ajm  created
*/

#ifndef __INCloadEcoffCommh
#define __INCloadEcoffCommh

#ifdef __cplusplus
extern "C" {
#endif

#include "ecoff.h"

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void swapCoffoptHdr (AOUTHDR *pOptHdr);

extern void swapCoffhdr (FILHDR *pHdr);

extern STATUS ecoffHdrRead (int fd, FILHDR *pHdr, BOOL *pSwap);

extern STATUS ecoffOpthdrRead (int fd, AOUTHDR *pOptHdr, FILHDR *pHdr,
    BOOL swapTables);

extern void swabLong ( char input[], char output[] );

#else	/* __STDC__ */

extern void swapCoffoptHdr ();

extern void swapCoffhdr ();

extern STATUS ecoffHdrRead ();

extern STATUS ecoffOpthdrRead ();

extern void swabLong ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCloadEcoffCommh */
