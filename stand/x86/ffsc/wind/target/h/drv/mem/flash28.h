/* flash28.h - header for 28F0X0 FLASH memory devices */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,24feb94,dzb  added device codes for different flash devices.
01a,10feb94,dzb  created.
*/

/*
DESCRIPTION
This file contains header information for 28F0X0 FLASH memory devices.
*/

#ifndef __INCflash28h
#define __INCflash28h

#ifdef __cplusplus
extern "C" {
#endif

/* defines */

#define	FLASH28_CMD_READ_MEM		0x00000000
#define	FLASH28_CMD_READ_ID		0x90909090
#define	FLASH28_CMD_ERASE_SETUP		0x20202020
#define	FLASH28_CMD_ERASE		0x20202020
#define	FLASH28_CMD_ERASE_VERIFY	0xa0a0a0a0
#define	FLASH28_CMD_PROG_SETUP		0x40404040
#define	FLASH28_CMD_PROG_VERIFY		0xc0c0c0c0
#define	FLASH28_CMD_RESET		0xffffffff

#define FLASH_28F256			0xa1		/* device code 28F256 */
#define FLASH_28F512			0x25		/* device code 28F512 */
#define FLASH_28F010			0xa7		/* device code 28F010 */
#define FLASH_28F020			0x2a		/* device code 28F020 */
 
#ifdef __cplusplus
}
#endif

#endif /* __INCflash28h */
