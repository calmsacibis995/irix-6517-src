/* iFlash.h - Intel Flash Memory header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,30oct92,ccc  created.
*/

/*
DESCRIPTION
This file contains header information for the Intel 256 Flash Memory device.
*/

#ifndef __INCiFlashh
#define __INCiFlashh

#ifdef __cplusplus
extern "C" {
#endif

/* device constants */

#define	FLASH_CMD_READ_MEM	0x0000
#define	FLASH_CMD_READ_ID	0x9090
#define	FLASH_CMD_ERASE_SETUP	0x2020
#define	FLASH_CMD_ERASE		0x2020
#define	FLASH_CMD_ERASE_VERIFY	0xa0a0
#define	FLASH_CMD_PROG_SETUP	0x4040
#define	FLASH_CMD_PROG_VERIFY	0xc0c0
#define	FLASH_CMD_RESET		0xffff

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

#endif /* __INCiFlashh */
