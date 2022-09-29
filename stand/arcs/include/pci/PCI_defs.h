/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __PCI_DEFS_H__
#define __PCI_DEFS_H__

#ident "$Revision: 1.1 $"

/* defines for the PCI bus architecture */

#define	PCI_CFG_VENDOR_ID	0x00		/* Vendor ID (2 bytes) */
#define	PCI_CFG_DEVICE_ID	0x02		/* Device ID (2 bytes) */

#define	PCI_CFG_COMMAND		0x04		/* Command (2 bytes) */
#define	PCI_CFG_STATUS		0x06		/* Status (2 bytes) */

#define	PCI_CFG_REV_ID		0x08		/* Revision Id (1 byte) */
#define	PCI_CFG_BASE_CLASS	0x09		/* Base Class (1 byte) */
#define	PCI_CFG_SUB_CLASS	0x0A		/* Sub Class (1 byte) */
#define	PCI_CFG_PROG_IF		0x0B		/* Prog Interface (1 byte) */

#define	PCI_CFG_CACHE_LINE	0x0C		/* Cache line size (1 byte) */
#define	PCI_CFG_LATENCY_TIMER	0x0D		/* Latency Timer (1 byte) */
#define	PCI_CFG_HEADER_TYPE	0x0E		/* Header Type (1 byte) */
#define	PCI_CFG_BIST		0x0F		/* Built In Self Test */

#define	PCI_CFG_BASE_ADDR_0	0x10		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_OFF	0x04		/* Base Address Offset (1..5)*/

#define	PCI_CFG_BASE_ADDR_1	0x14		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_2	0x18		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_3	0x1C		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_4	0x20		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_5	0x24		/* Base Address (4 bytes) */

#define	PCI_CFG_CARDBUS_CIS	0x28		/* Cardbus CIS Pointer (4B) */

#define	PCI_CFG_SUBSYS_VEND_ID	0x2C		/* Subsystem Vendor ID (2B) */
#define	PCI_CFG_SUBSYS_ID	0x2E		/* Subsystem ID */

#define	PCI_EXPANSION_ROM	0x30		/* Expansion Rom Base (4B) */

#define	PCI_INTR_LINE		0x3C		/* Interrupt Line (1B) */
#define	PCI_INTR_PIN		0x3D		/* Interrupt Pin (1B) */
#define	PCI_MIN_GNT		0x3E		/* Minimum Grant (1B) */
#define	PCI_MAX_LAT		0x3F		/* Maximum Latency (1B) */

#define PCI_CFG_VEND_SPECIFIC	0x40		/* first vendor specific reg */

/* Command Register layout (0x04) */
#define	PCI_CMD_IO_SPACE	0x001		/* I/O Space device */
#define	PCI_CMD_MEM_SPACE	0x002		/* Memory Space */
#define	PCI_CMD_BUS_MASTER	0x004		/* Bus Master */
#define	PCI_CMD_SPEC_CYCLES	0x008		/* Special Cycles */
#define	PCI_CMD_MEMW_INV_ENAB	0x010		/* Memory Write Inv Enable */
#define	PCI_CMD_VGA_PALETTE_SNP	0x020		/* VGA Palette Snoop */
#define	PCI_CMD_PAR_ERR_RESP	0x040		/* Parity Error Response */
#define	PCI_CMD_WAIT_CYCLE_CTL	0x080		/* Wait Cycle Control */
#define	PCI_CMD_SERR_ENABLE	0x100		/* SERR# Enable */
#define	PCI_CMD_F_BK_BK_ENABLE	0x200		/* Fast Back-to-Back Enable */

/* Status Register Layout (0x06) */
#define	PCI_STAT_PAR_ERR_DET	0x8000		/* Detected Parity Error */
#define	PCI_STAT_SYS_ERR	0x4000		/* Signaled System Error */
#define	PCI_STAT_RCVD_MSTR_ABT	0x2000		/* Received Master Abort */
#define	PCI_STAT_RCVD_TGT_ABT	0x1000		/* Received Target Abort */
#define	PCI_STAT_SGNL_TGT_ABT	0x0800		/* Signaled Target Abort */

#define	PCI_STAT_DEVSEL_TIMING	0x0600		/* DEVSEL Timing Mask */
#define	DEVSEL_TIMING(_x)	(((_x) >> 9) & 3)	/* devsel tim macro */
#define	DEVSEL_FAST		0		/* Fast timing */
#define	DEVSEL_MEDIUM		1		/* Medium timing */
#define	DEVSEL_SLOW		2		/* Slow timing */

#define	PCI_STAT_DATA_PAR_ERR	0x0100		/* Data Parity Err Detected */
#define	PCI_STAT_F_BK_BK_CAP	0x0080		/* Fast Back-to-Back Capable */
#define	PCI_STAT_UDF_SUPP	0x0040		/* UDF Supported */
#define	PCI_STAT_66MHZ_CAP	0x0020		/* 66 MHz Capable */

/* BIST Register Layout (0x0F) */
#define	PCI_BIST_BIST_CAP	0x80		/* BIST Capable */
#define	PCI_BIST_START_BIST	0x40		/* Start BIST */
#define	PCI_BIST_CMPLTION_MASK	0x0F		/* COMPLETION MASK */
#define	PCI_BIST_CMPL_OK	0x00		/* 0 value is completion OK */

/* Base Address Register 0x10 */
#define	PCI_BA_IO_SPACE		0x01		/* I/O Space Marker */

#define	PCI_BA_MEM_LOCATION	0x06		/* 2 bits for location avail */
#define	PCI_BA_MEM_32BIT	0x00		/* Anywhere in 32bit space */
#define	PCI_BA_MEM_1MEG		0x02		/* Locate below 1 Meg */
#define	PCI_BA_MEM_64BIT	0x40		/* Anywhere in 64bit space */

#define	PCI_BA_PREFETCH		0x80		/* Prefetchable, no side */

/* PIO interface macros */

#ifndef IOC3_EMULATION

#define PCI_INB(x)          (*(x))
#define PCI_INH(x)          (*(x))
#define PCI_INW(x)          (*(x))
#define PCI_OUTB(x,y)       (*(x) = y)
#define PCI_OUTH(x,y)       (*(x) = y)
#define PCI_OUTW(x,y)       (*(x) = y)

#else

extern int pci_read(void * address, int type);
extern void pci_write(void * address, int data, int type);

#define BYTE   1
#define HALF   2
#define WORD   4

#define PCI_INB(x)          pci_read((void *)(x),BYTE)
#define PCI_INH(x)          pci_read((void *)(x),HALF)
#define PCI_INW(x)          pci_read((void *)(x),WORD)
#define PCI_OUTB(x,y)       pci_write((void *)(x),(y),BYTE)
#define PCI_OUTH(x,y)       pci_write((void *)(x),(y),HALF)
#define PCI_OUTW(x,y)       pci_write((void *)(x),(y),WORD)

#endif /* !IOC3_EMULATION */
						/* effects on reads, merges */

/*
 * IP32 uses PCI configuration mechanism #1.
 * The first three are defs related to IP32 PCI bridge policy.
 * The rest are defs related to config. mechanism #1.
 */
#if defined(IP32)
#define PCI_CONFIG_BITS			0xff000500
#define PCI_FIRST_IO_ADDR		0x1000
#define PCI_IO_MAP_INCR			0x1000

#define CFG1_ADDR_REGISTER_MASK		0x000000fc
#define CFG1_ADDR_FUNCTION_MASK		0x00000700
#define CFG1_ADDR_DEVICE_MASK		0x0000f800
#define CFG1_ADDR_BUS_MASK		0x00ff0000

#define CFG1_REGISTER_SHIFT		2
#define CFG1_FUNCTION_SHIFT		8
#define CFG1_DEVICE_SHIFT		11
#define CFG1_BUS_SHIFT			16
#endif /* IP32 */

#endif /* __PCI_DEF_H__ */
