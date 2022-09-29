/* vsam.h - SBE VSAM Chip Set header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,10sep89,rld  original SBE release.
*/

/*
This header file defines the register layout of the SBE VMEbus
Slave Address Manager chip (VSAM) and supplies alpha-numeric
equivalences for register bit definitions.
*/

#ifndef __INCvsamh
#define __INCvsamh

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	_ASMLANGUAGE
#define VSAM_ADRS(reg)	 (VSAM + (reg * VSAM_REG_OFFSET))
#else
#define VSAM_ADRS(reg)	 ((char *) VSAM + (reg * VSAM_REG_OFFSET))
#endif	/* _ASMLANGUAGE */

/* Location Monitor */

#define VSAM_LM_VAC_0	VSAM_ADRS(0x00)		/* VME Address Register 0 */
#define VSAM_LM_VAC_1	VSAM_ADRS(0x01)		/* VME Address Register 1 */
#define VSAM_LM_VAC_2	VSAM_ADRS(0x02)		/* VME Address Register 2 */
#define VSAM_LM_VAC_3	VSAM_ADRS(0x03)		/* VME Address Register 3 */
#define VSAM_LM_BLKS_UPR VSAM_ADRS(0x04)	/* VME Block Size */
#define VSAM_LM_BLKS_LWR VSAM_ADRS(0x05)	/* VME Block Size */
#define	LM_BLK_2	0x0000	 		/* 2 byte size */
#define	LM_BLK_4	0x0002	 		/* 4 byte size */
#define	LM_BLK_8	0x0006	 		/* 8 byte size */
#define	LM_BLK_16	0x000e	 		/* 16 byte size */
#define	LM_BLK_32	0x001e			/* 32 byte size */
#define	LM_BLK_64	0x003e			/* 64 byte size */
#define	LM_BLK_128	0x007e			/* 128 byte size */
#define	LM_BLK_256	0x00fe	 		/* 256 byte size */
#define	LM_BLK_512	0x01fe	 		/* 512 byte size */
#define	LM_BLK_1K	0x03fe	 		/* 1K byte size */
#define	LM_BLK_2K	0x07fe	 		/* 2K byte size */
#define	LM_BLK_4K	0x0ffe			/* 4K byte size */
#define	LM_BLK_8K	0x1ffe	 		/* 8K byte size */
#define	LM_BLK_16K	0x3ffe			/* 16K byte size */
#define	LM_BLK_32K	0x7ffe	 		/* 32K byte size */
#define	LM_BLK_64K	0xfffe	 		/* 64K byte size */

#define VSAM_LM_AMU0	VSAM_ADRS(0x06)	/* User Defined VME AM code #1 */
#define VSAM_LM_AMU1	VSAM_ADRS(0x07)	/* User Defined VME AM code #2 */
#define VSAM_LM_CTR	VSAM_ADRS(0x08)	/* Counter */
#define VSAM_LM_MC	VSAM_ADRS(0x0a)	/* Match count */
#define VSAM_LM_CMD	VSAM_ADRS(0x0c)	/* Location Monitor Command */
#define	LM_CMD_FLAG		0x08	/* Command Flag */
#define	LM_CMD_DISABLE		0x0b	/* cmd: disable Loc Monitor */
#define	LM_CMD_ENABLE		0x0c	/* cmd: enable Loc Monitor */
#define	LM_CMD_READ_CNTR	0x0e	/* cmd: read Monitor counter */
#define	LM_CMD_WRITE_CNTR	0x0f	/* cmd: write Monitor counter */
#define VSAM_LM_CTL	VSAM_ADRS(0x0d)	/* Location Monitor Control */
#define	LM_AMU0_ENABLE		0x04	/* Enable AMU0 usage */
#define	LM_AMU1_ENABLE		0x08	/* Enable AMU1 usage */
#define	LM_MODE_NONE		0x00	/* No address monitoring */
#define	LM_MODE_A16		0x01	/* Select A16 monitoring */
#define	LM_MODE_A24		0x02	/* Select A24 monitoring */
#define	LM_MODE_A32		0x03	/* Select A32 monitoring */
#define VSAM_LM_VSL_UPR	VSAM_ADRS(0x0e)	/* VME Status Latch */
#define	LM_VSL_UAT		0x80	/* Decoded UAT transfer */
#define	LM_VSL_WORD		0x40	/* Decoded Word transfer */
#define	LM_VSL_BYTE		0x20	/* Decoded Byte transfer */
#define	LM_VSL_WRITE		0x10	/* Latched VWRITE* signal */
#define	LM_VSL_LDS1		0x08	/* Latched VDS1* signal */
#define	LM_VSL_LDS0		0x04	/* Latched VDS0* signal */
#define	LM_VSL_LA01		0x02	/* Latched VA1 signal */
#define	LM_VSL_LLWORD		0x01	/* Latched VLWORD* signal */
#define VSAM_LM_VSL_LWR	VSAM_ADRS(0x0f)	/* VME Status Latch */
#define	LM_VSL_LIACK		0x80	/* Latched VIACK* signal */
#define	LM_VSL_AMASK		0x3f	/* Mask for AM codes */
#define VSAM_LM_AMR	VSAM_ADRS(0x10)	/* Regular VME AM codes */
#define	LM_AM_SUPER		0x38	/* Respond to supervisor */
#define	LM_AM_NONPRIV		0x07	/* Respond to non-privilege */
#define	LM_AM_SUP_ASCENDING	0x20	/* AMF */
#define	LM_AM_SUP_PGM		0x10	/* AME */
#define	LM_AM_SUP_DATA		0x08	/* AMD */
#define	LM_AM_USR_ASCENDING	0x04	/* AMB */
#define	LM_AM_USR_PGM		0x02	/* AMA */
#define	LM_AM_USR_DATA		0x01	/* AM9 */
#define VSAM_LM_VEC	VSAM_ADRS(0x11)	/* Interrupt Vector */

/* Shared Memory */

#define VSAM_SM_BAV_UPR	VSAM_ADRS(0x12)	/* Base Addr on VMEbus */
#define VSAM_SM_BAV_LWR	VSAM_ADRS(0x13)	/* Base Addr on VMEbus */
#define VSAM_SM_BLKS	VSAM_ADRS(0x14)	/* Block Size */
#define	SM_BLK_64K		0x00	/* 64K byte size */
#define	SM_BLK_128K		0x01	/* 128K byte size */
#define	SM_BLK_256K		0x03	/* 256K byte size */
#define	SM_BLK_512K		0x07	/* 512K byte size */
#define	SM_BLK_1MB		0x0f	/* 1MB byte size */
#define	SM_BLK_2MB		0x1f	/* 2MB byte size */
#define	SM_BLK_4MB		0x3f	/* 4MB byte size */
#define	SM_BLK_8MB		0x7f	/* 8MB byte size */
#define VSAM_SM_BAL	VSAM_ADRS(0x15)	/* Base Addr on Local Bus */
#define VSAM_SM_AM	VSAM_ADRS(0x16)	/* AM codes */
#define	SM_AM_A32		0x80	/* 32-bit address response */
#define	SM_AM_A24		0x00	/* 24-bit address response */
#define	SM_AM_SUPER		0x38	/* Respond to supervisor */
#define	SM_AM_NONPRIV		0x07	/* Respond to non-privilege */
#define	SM_AM_SUP_ASCENDING	0x20	/* AMF */
#define	SM_AM_SUP_PGM		0x10	/* AME */
#define	SM_AM_SUP_DATA		0x08	/* AMD */
#define	SM_AM_USR_ASCENDING	0x04	/* AMB */
#define	SM_AM_USR_PGM		0x02	/* AMA */
#define	SM_AM_USR_DATA		0x01	/* AM9 */
#define VSAM_IO_BAV_UPR	VSAM_ADRS(0x18)	/* Short I/O Base Address */
#define VSAM_IO_BAV_LWR	VSAM_ADRS(0x19)	/* Short I/O Base Address */
#define VSAM_IO_AM	VSAM_ADRS(0x1a)	/* Short I/O AM codes */
#define	EN_29			0x01	/* Short supervisory AM */
#define	EN_2D			0x02	/* Short non-priv AM */
#define	EN_MBX			0x04	/* Assertion of mailbox int */

/* Mailbox and Status Functions */

#define VSAM_MBX_VEC	VSAM_ADRS(0x1b)	/* Mailbox Intr Vector */
#define VSAM_MBX_ISR	VSAM_ADRS(0x1c)	/* Interrupt Status Register */
#define	MBXP			0x80	/* Mailbox intr is pending */
#define	LMQP			0x40	/* Loc Monitor intr is pending */
#define	MBXEN			0x08	/* Mailbox intr is enabled */
#define	LMQEN			0x04	/* Loc Monitor is enabled */
#define VSAM_MBX_USER	VSAM_ADRS(0x1d)	/* User Defined Register */
#define	MBX_RESET		0xa5	/* VMEbus resets local board */
#define VSAM_MAILBOX_UPR VSAM_ADRS(0x1e)/* Mailbox Register */
#define VSAM_MAILBOX_LWR VSAM_ADRS(0x1f)/* Mailbox Write Code Register */

#ifdef __cplusplus
}
#endif

#endif /* __INCvsamh */
