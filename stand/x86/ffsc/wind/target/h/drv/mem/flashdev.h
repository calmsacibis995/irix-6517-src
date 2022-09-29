/* flashDev.h - generic FLASH memory header */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,08jan94,dzb  created.
*/

/*
DESCRIPTION
This file contains header information for generic FLASH memory routines.
*/

#ifndef __INCflashDevh
#define __INCflashDevh

#ifdef __cplusplus
extern "C" {
#endif

/* defines */

#if     (FLASH_WIDTH == 1)
#define FLASH_DEF       UINT8
#define FLASH_CAST      (UINT8 *)
#endif  /* FLASH_WIDTH */

#if     (FLASH_WIDTH == 2)
#define FLASH_DEF       UINT16
#define FLASH_CAST      (UINT16 *)
#endif  /* FLASH_WIDTH */

#if     (FLASH_WIDTH == 4)
#define FLASH_DEF       UINT32
#define FLASH_CAST      (UINT32 *)
#endif  /* FLASH_WIDTH */

#define	FLASH_UNKNOWN	0

/* function declarations */

#ifndef	_ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

IMPORT	STATUS	sysFlashGet (char *string, int strLen, int offset);
IMPORT	STATUS	sysFlashSet (char *string, int strLen, int offset);

#else	/* __STDC__ */

IMPORT	STATUS	sysFlashGet ();
IMPORT	STATUS	sysFlashSet ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCflashDevh */
