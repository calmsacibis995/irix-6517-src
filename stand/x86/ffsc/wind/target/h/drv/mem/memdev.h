/* memDev.h - generic non-volatile RAM header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,07jul92,ccc  change of name to memDev.h.
01b,29jun92,caf  changed genericNvram.h to genericNvRam.h.
01a,26jun92,caf  created.
*/

/*
DESCRIPTION
This file contains header information for generic non-volatile RAM routines.
*/

#ifndef __INCmemDevh
#define __INCmemDevh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#ifndef	_ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

IMPORT	STATUS	sysNvRamGet (char *string, int strLen, int offset);
IMPORT	STATUS	sysNvRamSet (char *string, int strLen, int offset);

#else	/* __STDC__ */

IMPORT	STATUS	sysNvRamGet ();
IMPORT	STATUS	sysNvRamSet ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCmemDevh */
