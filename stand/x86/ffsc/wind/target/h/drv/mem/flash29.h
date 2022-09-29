/* flash29.h - header for 29F0X0 FLASH memory devices */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,24feb94,dzb  made device code 1 byte.
01a,10feb94,dzb  created.
*/

/*
DESCRIPTION
This file contains header information for 29F0X0 FLASH memory devices.
*/

#ifndef __INCflash29h
#define __INCflash29h

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	_ASMLANGUAGE
#define CAST
#else	/* _ASMLANGUAGE */
#define CAST (char *)
#endif	/* _ASMLANGUAGE */

#ifndef FLASH29_REG_ADRS
#define FLASH29_REG_ADRS(reg) (CAST FLASH_ADRS + (reg * FLASH_WIDTH))
#endif  /* FLASH29_REG_ADRS */

/* FLASH29 command register addresses */

#define FLASH29_REG_FIRST_CYCLE		FLASH29_REG_ADRS (0x5555)
#define FLASH29_REG_SECOND_CYCLE	FLASH29_REG_ADRS (0x2aaa)

/* FLASH29 command definitions */

#define	FLASH29_CMD_FIRST		0xaaaaaaaa
#define	FLASH29_CMD_SECOND		0x55555555
#define	FLASH29_CMD_FOURTH		0xaaaaaaaa
#define	FLASH29_CMD_FIFTH		0x55555555
#define	FLASH29_CMD_SIXTH		0x10101010

#define	FLASH29_CMD_PROGRAM		0xa0a0a0a0
#define	FLASH29_CMD_CHIP_ERASE		0x80808080
#define	FLASH29_CMD_READ_RESET		0xf0f0f0f0
#define	FLASH29_CMD_AUTOSELECT		0x90909090
 
#define	FLASH_29F010			0x20		/* device code 29F010 */

#ifdef __cplusplus
}
#endif

#endif /* __INCflash29h */
