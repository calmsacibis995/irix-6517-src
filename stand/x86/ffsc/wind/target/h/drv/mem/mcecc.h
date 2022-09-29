/* mcecc.h - Memory Controller ASIC for ECC DRAM */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,26oct93,dzb  written based on version 01d of drv/multi/memc040.h.
*/

#ifdef	DOC
#define	INCmcecch
#endif	/* DOC */

#ifndef __INCmcecch
#define __INCmcecch

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file contains constants for Motorola's Memory Controller ASIC for
 * ECC DRAM modules.  The macro MCECC_BASE_ADRS must be defined when including
 * this header.
 */

#ifdef	_ASMLANGUAGE
#define CAST
#else
#define CAST (char *)
#endif	/* _ASMLANGUAGE */

/* on-board access, register definitions */

#define	MCECC_REG_INTERVAL	1

#ifndef MCECC_ADRS	/* to permit alternative board addressing */
#define MCECC_ADRS(reg)   (CAST (MCECC_BASE_ADRS + (reg * MCECC_REG_INTERVAL)))
#endif	/* MCECC_ADRS */

#define MCECC_ID	MCECC_ADRS(0x00)	/* Chip ID */
#define	MCECC_REVISION	MCECC_ADRS(0x04)	/* Chip Revision Register */
#define	MCECC_MCR	MCECC_ADRS(0x08)	/* Memory Configuration Reg. */
#define	MCECC_BAR	MCECC_ADRS(0x14)	/* Base Address Register */
#define	MCECC_RCR	MCECC_ADRS(0x18)	/* RAM Control Register */
#define	MCECC_BCR	MCECC_ADRS(0x1c)	/* Bus Clock Frequency Reg. */
#define	MCECC_DCR	MCECC_ADRS(0x20)	/* Data Control Register */
#define	MCECC_SCR	MCECC_ADRS(0x24)	/* Scrub Control Register */
#define	MCECC_SPR_HI	MCECC_ADRS(0x28)	/* Scrub Period Reg (8-15) */
#define	MCECC_SPR_LO	MCECC_ADRS(0x2c)	/* Scrub Period Reg (7-0) */
#define	MCECC_CPC	MCECC_ADRS(0x30)	/* Chip Prescaler Counter */
#define	MCECC_STR	MCECC_ADRS(0x34)	/* Scrub Time Register */
#define	MCECC_SPC_HI	MCECC_ADRS(0x38)	/* Scrub Prescale Cnt (21-16) */
#define	MCECC_SPC_MID	MCECC_ADRS(0x3c)	/* Scrub Prescale Cnt (15-8) */
#define	MCECC_SPC_LO	MCECC_ADRS(0x40)	/* Scrub Prescale Cnt (7-0) */
#define	MCECC_STC_HI	MCECC_ADRS(0x44)	/* Scrub Timer Cnt (15-8) */
#define	MCECC_STC_LO	MCECC_ADRS(0x48)	/* Scrub Timer Cnt (7-0) */
#define	MCECC_SAC_HH	MCECC_ADRS(0x4c)	/* Scrub Address Cnt (26-24) */
#define	MCECC_SAC_H	MCECC_ADRS(0x50)	/* Scrub Address Cnt (23-16) */
#define	MCECC_SAC_L	MCECC_ADRS(0x54)	/* Scrub Address Cnt (15-8) */
#define	MCECC_SAC_LL	MCECC_ADRS(0x58)	/* Scrub Address Cnt (7-4) */
#define	MCECC_ELR	MCECC_ADRS(0x5c)	/* Error Logger Register */
#define	MCECC_EA_HH	MCECC_ADRS(0x60)	/* Error Address (31-24) */
#define	MCECC_EA_H	MCECC_ADRS(0x64)	/* Error Address (23-16) */
#define	MCECC_EA_L	MCECC_ADRS(0x68)	/* Error Address (15-8) */
#define	MCECC_EA_LL	MCECC_ADRS(0x6c)	/* Error Address (7-4) */
#define	MCECC_ESR	MCECC_ADRS(0x70)	/* Error Syndrome Register */
#define	MCECC_DR1	MCECC_ADRS(0x74)	/* Defaults Register 1 */
#define	MCECC_DR2	MCECC_ADRS(0x78)	/* Defaults Register 2 */

/****************************************************************
 * MCECC_ID		0x00	Chip ID				*
 ****************************************************************/

#define	MCECC_ID_RESET		0x0f	/* software reset of MCECC chip */

/****************************************************************
 * MCECC_MCR		0x08	Memory Configuration Register	*
 ****************************************************************/

#define	MCECC_MCR_4MB		0x00	/* 4Mb of DRAM */
#define	MCECC_MCR_8MB		0x01	/* 8Mb of DRAM */
#define	MCECC_MCR_16MB		0x02	/* 16Mb of DRAM */
#define	MCECC_MCR_32MB		0x03	/* 32Mb of DRAM */
#define	MCECC_MCR_64MB		0x04	/* 64Mb of DRAM */
#define	MCECC_MCR_128MB		0x05	/* 128Mb of DRAM */
#define	MCECC_MCR_FSTRD		0x20	/* fast read */

/****************************************************************
 * MCECC_RCR		0x18	RAM Control Register		*
 ****************************************************************/

#define	MCECC_RCR_RAMEN		0x01	/* RAM enable */
#define	MCECC_RCR_NCEBEN	0x02	/* assert TEA* on noncorrectable err */
#define	MCECC_RCR_NCEIEN	0x04	/* interrupt on noncorrectable err */
#define	MCECC_RCR_RWB3		0x08	/* general purpose R/W bit */
#define	MCECC_RCR_SWAIT		0x10	/* snoop wait */
#define	MCECC_RCR_RWB5		0x20	/* general purpose R/W bit */
#define	MCECC_RCR_BAD		0xc0	/* base address bits 22 & 23 */

/****************************************************************
 * MCECC_BCR		0x1c	Bus Clock Register		*
 ****************************************************************/

#define	MCECC_BCR_25MHZ		25	/* 25 MHz clock */
#define	MCECC_BCR_33MHZ		33	/* 33 MHz clock */

/****************************************************************
 * MCECC_DCR		0x20	Data Control Register		*
 ****************************************************************/

#define	MCECC_DCR_RWCKB		0x08	/* R/W checkbits */
#define	MCECC_DCR_ZFILL		0x10	/* zero fill */
#define	MCECC_DCR_DERC		0x20	/* disable error correction */

/****************************************************************
 * MCECC_SCR		0x24	Scrub Control Register		*
 ****************************************************************/

#define	MCECC_SCR_IDIS		0x01	/* image disable */
#define	MCECC_SCR_SBEIEN	0x02	/* interrupt on single bit error */
#define	MCECC_SCR_SCRBEN	0x08	/* enable scrubber */
#define	MCECC_SCR_SCRB		0x10	/* scrubber busy */
#define	MCECC_SCR_HITDIS	0x20	/* not implemented (Mot) */
#define	MCECC_SCR_RADATA	0x40	/* not implemented (Mot) */
#define	MCECC_SCR_RACODE	0x80	/* not implemented (Mot) */
#define MCECC_SCRUB_DIS		0x00	/* disable scrubber */

/****************************************************************
 * MCECC_STR		0x34	Scrub Time On/Time Off Register	*
 ****************************************************************/

#define	MCECC_STR_OFF_IMMED	0x00	/* request DRAM immediately */
#define	MCECC_STR_OFF_16	0x01	/* request DRAM after 16 clocks */
#define	MCECC_STR_OFF_32	0x02	/* request DRAM after 32 clocks */
#define	MCECC_STR_OFF_64	0x03	/* request DRAM after 64 clocks */
#define	MCECC_STR_OFF_128	0x04	/* request DRAM after 128 clocks */
#define	MCECC_STR_OFF_256	0x05	/* request DRAM after 256 clocks */
#define	MCECC_STR_OFF_512	0x06	/* request DRAM after 512 clocks */
#define	MCECC_STR_OFF_NEVER	0x07	/* never request DRAM */
#define	MCECC_STR_ON_1		0x00	/* keep DRAM for 1 clock */
#define	MCECC_STR_ON_16		0x08	/* keep DRAM for 16 clocks */
#define	MCECC_STR_ON_32		0x10	/* keep DRAM for 32 clocks */
#define	MCECC_STR_ON_64		0x18	/* keep DRAM for 64 clocks */
#define	MCECC_STR_ON_128	0x20	/* keep DRAM for 128 clocks */
#define	MCECC_STR_ON_256	0x28	/* keep DRAM for 256 clocks */
#define	MCECC_STR_ON_512	0x30	/* keep DRAM for 512 clocks */
#define	MCECC_STR_ON_TOTAL	0x38	/* keep DRAM for total scrub time */
#define	MCECC_STR_SRDIS		0x80	/* scrubber read disable */

/****************************************************************
 * MCECC_ELR		0x5c	Error Logger Register		*
 ****************************************************************/

#define	MCECC_ELR_SBE		0x01	/* single bit error */
#define	MCECC_ELR_MBE		0x02	/* multiple bit error */
#define	MCECC_ELR_EALT		0x08	/* error with alternate bus master */
#define	MCECC_ELR_ERA		0x10	/* not implemented (Mot) */
#define	MCECC_ELR_ESCRB		0x20	/* error with scrubber */
#define	MCECC_ELR_ERD		0x40	/* error occurred on a READ */
#define	MCECC_ELR_ERRLOG	0x80	/* error log is full */

/****************************************************************
 * MCECC_DR1		0x74	Default Register 1		*
 ****************************************************************/

#define	MCECC_DR1_4M		0x00	/* 4Mb of DRAM */
#define	MCECC_DR1_8M		0x01	/* 8Mb of DRAM */
#define	MCECC_DR1_16M		0x02	/* 16Mb of DRAM */
#define	MCECC_DR1_32M		0x03	/* 32Mb of DRAM */
#define	MCECC_DR1_64M		0x04	/* 64Mb of DRAM */
#define	MCECC_DR1_128M		0x05	/* 128Mb of DRAM */
#define	MCECC_DR1_SELI0		0x80	/* register base address select 0 */
#define	MCECC_DR1_SELI1		0x10	/* register base address select 1 */
#define	MCECC_DR1_FSTRD		0x20	/* fast DRAM reads */
#define	MCECC_DR1_STATCOL	0x40	/* ? */
#define	MCECC_DR1_WRHDIS	0x80	/* not implemented (Mot) */

/****************************************************************
 * MCECC_DR2		0x78	Default Register 2		*
 ****************************************************************/

#define	MCECC_DR2_RESST0	0x01	/* general purpose R/W bit */
#define	MCECC_DR2_RESST1	0x02	/* general purpose R/W bit */
#define	MCECC_DR2_RESST2	0x04	/* general purpose R/W bit */
#define	MCECC_DR2_NOCACHE	0x08	/* SCR can be cleared by software */
#define	MCECC_DR2_TVECT		0x10	/* test vendor vectors */
#define	MCECC_DR2_REFDIS	0x20	/* disable refreshing DRAM */
#define	MCECC_DR2_XY_FLIP	0x40	/* select opposite cache latches */
#define	MCECC_DR2_FRC_OPN	0x80	/* force DRAM read latches open */

#ifdef __cplusplus
}
#endif

#endif	/* __INCmcecch */
