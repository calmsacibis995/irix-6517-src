/* mcecc.h - Memory Controller ASIC with Error Correction Code */

/* Copyright 1991-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,04jul94,tpr	 written.
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
 * This file contains constants for Motorola's Memory Controller ASIC
 * with ecc (error correction code).
 * The macro MCECC_BASE_ADRS must be defined when including this header.
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

#define MCECC_ID	MCECC_ADRS(0x00) /* Chip ID 			*/
#define	MCECC_REVISION	MCECC_ADRS(0x04) /* Chip Revision Register	*/
#define	MCECC_MCR	MCECC_ADRS(0x08) /* Memory Configuration Reg	*/
#define	MCECC_DUMMY_0	MCECC_ADRS(0x0c) /* Alternate Status Register	*/
#define	MCECC_DUMMY_1	MCECC_ADRS(0x10) /* Alternate Control Register	*/
#define	MCECC_BAR	MCECC_ADRS(0x14) /* Base Address Register	*/
#define	MCECC_RCR	MCECC_ADRS(0x18) /* RAM Control Register	*/
#define	MCECC_BCR	MCECC_ADRS(0x1c) /* Bus Clock Register		*/
#define MCECC_DCR	MCECC_ADRS(0x20) /* Data Control Register	*/
#define MCECC_SCR	MCECC_ADRS(0x24) /* Scrub Control Register	*/
#define MCECC_SPR_0	MCECC_ADRS(0x28) /* Scrub Period Register 0 	*/
#define MCECC_SPR_1	MCECC_ADRS(0x2c) /* Scrub Period Register 1 	*/
#define MCECC_CPR	MCECC_ADRS(0x30) /* Chip prescale Register	*/
#define MCECC_STOOR	MCECC_ADRS(0x34) /* Scrub Time On/Off Register	*/
#define MCECC_SPSR_0	MCECC_ADRS(0x38) /* Scrub PreScale Register 0	*/
#define MCECC_SPSR_1	MCECC_ADRS(0x3c) /* Scrub PreScale Register 1	*/
#define MCECC_SPSR_2	MCECC_ADRS(0x40) /* Scrub PreScale Register 2	*/
#define MCECC_STR_0	MCECC_ADRS(0x44) /* Scrub Timer Register 0	*/
#define MCECC_STR_1	MCECC_ADRS(0x48) /* Scrub Timer Register 1	*/
#define MCECC_SACR_0	MCECC_ADRS(0x4c) /* Scrub Addr Cntr Register 0	*/
#define MCECC_SACR_1	MCECC_ADRS(0x50) /* Scrub Addr Cntr Register 1	*/
#define MCECC_SACR_2	MCECC_ADRS(0x54) /* Scrub Addr Cntr Register 2	*/
#define MCECC_SACR_3	MCECC_ADRS(0x58) /* Scrub Addr Cntr Register 3	*/
#define MCECC_ELR	MCECC_ADRS(0x5c) /* Error Logger Register	*/
#define MCECC_EAR_0	MCECC_ADRS(0x60) /* Error Addr Register 0	*/
#define MCECC_EAR_1	MCECC_ADRS(0x64) /* Error Addr Register 1	*/
#define MCECC_EAR_2	MCECC_ADRS(0x68) /* Error Addr Register 2	*/
#define MCECC_EAR_3	MCECC_ADRS(0x6c) /* Error Addr Register 3	*/
#define MCECC_ESR	MCECC_ADRS(0x70) /* Error Syndrome Register	*/
#define MCECC_DR_1	MCECC_ADRS(0x74) /* Default Register 1 		*/
#define MCECC_DR_2	MCECC_ADRS(0x78) /* Default Register 2 		*/

/* MCECC_ID:	0x00	Chip ID	*/

#define	MCECC_ID_RESET		0x0f	/* software reset of chip	*/


/* MCECC_MCR:	0x08	Memory Configuration Register	*/

#define	MCECC_MCR_4MB		0x00	/*   4 Meg of Memory		2-0 */
#define	MCECC_MCR_8MB		0x01	/*   8 Meg of Memory		2-0 */
#define	MCECC_MCR_16MB		0x02	/*  16 Meg of Memory		2-0 */
#define	MCECC_MCR_32MB		0x03	/*  32 Meg of Memory		2-0 */
#define	MCECC_MCR_64MB		0x04	/*  64 Meg of Memory		2-0 */
#define	MCECC_MCR_128MB		0x05	/* 128 Meg of Memory		2-0 */
#define	MCECC_MCR_FSTRD		0x20	/* Fast Read			5 */


/* MCECC_RCR:	0x18	RAM Control Register		*/

#define	MCECC_RCR_RAMEN		0x01	/* RAM Enable			0 */
#define	MCECC_RCR_NCEBEN	0x02	/* Non correctable error Enable	1 */
#define	MCECC_RCR_NCEIEN	0x04	/* Non Correctable error Int Enable2 */
#define	MCECC_RCR_SWAIT		0x10	/* Snoop Wait			4 */

/* MCECC_BCR:	0x1c	Bus Clock Register		*/

#define	MCECC_BCR_16MHZ		16	/* 16 MHz clock */
#define	MCECC_BCR_20MHZ		20	/* 20 MHz clock */
#define	MCECC_BCR_25MHZ		25	/* 25 MHz clock */
#define	MCECC_BCR_33MHZ		33	/* 33 MHz clock */
#define	MCECC_BCR_50MHZ		50	/* 50 MHz clock */
#define	MCECC_BCR_60MHZ		60	/* 60 MHz clock */

/* MCECC_DCR:	0x20	Data Control Register		*/

#define	MCECC_DCR_RWCKB		0x08	/* Read Write Check Bits	3 */
#define MCECC_DCR_ZFILL		0x10	/* Zero fill			4 */
#define MCECC_DCR_DERC		0x20	/* Disable Error Correction	5 */

/* MCECC_SCR:	0x24	Scrub Control Register		*/

#define MCECC_SCR_IDIS		0x01	/* Image Disable		0 */
#define MCECC_SCR_SBEIEN	0x02	/* Single Bit Error Int Enable	1 */
#define MCECC_SCR_SCRBEN	0x08	/* Scrubber Enable		3 */
#define MCECC_SCR_SCRB		0x10	/* Scrubber state		4 */


/* MCECC_STOOR:	0x34	Scrub Time On/Off Register	*/

#define MCECC_STOOR_STOFF_IMMED		0x00	/* Rqst DRAM immediatly      */
#define MCECC_STOOR_STOFF_16_BCLK	0x01	/* Rqst DRAM after  16 BCLK  */
#define MCECC_STOOR_STOFF_32_BCLK	0x02	/* Rqst DRAM after  32 BCLK  */
#define MCECC_STOOR_STOFF_64_BCLK	0x03	/* Rqst DRAM after  64 BCLK  */
#define MCECC_STOOR_STOFF_128_BCLK	0x04	/* Rqst DRAM after 128 BCLK  */
#define MCECC_STOOR_STOFF_256_BCLK	0x05	/* Rqst DRAM after 256 BCLK  */
#define MCECC_STOOR_STOFF_512_BCLK	0x06	/* Rqst DRAM after 512 BCLK  */
#define MCECC_STOOR_STOFF_NEVER		0x07	/* Rqst DRAM never	     */
#define MCECC_STOOR_STON_1BLCK		0x00	/* Keep DRAM for 1 Mem cycle */
#define MCECC_STOOR_STON_16_BCLK	0x08	/* Keep DRAM for  16 BCLK    */
#define MCECC_STOOR_STON_32_BCLK	0x10	/* Keep DRAM for  32 BCLK    */
#define MCECC_STOOR_STON_64_BCLK	0x18	/* Keep DRAM for  64 BCLK    */
#define MCECC_STOOR_STON_128_BCLK	0x20	/* Keep DRAM for 128 BCLK    */
#define MCECC_STOOR_STON_256_BCLK	0x28	/* Keep DRAM for 256 BCLK    */
#define MCECC_STOOR_STON_512_BCLK	0x30	/* Keep DRAM for 512 BCLK    */
#define MCECC_STOOR_STON_TOTAL		0x38	/* Keep DRAM for total srub  */
#define MCECC_STOOR_SRDIS		0x80	/* Scrubber Disable          */

/* MCECC_ELR: 0x5c	Error LOGGER Register		*/

#define MCECC_ELR_SBE		0x01	/* Single Bit Error */
#define MCECC_ELR_MBE		0x02	/* Multiple Bit Error */
#define MCECC_ELR_EALT		0x08	/* Error by an Alternate bus master */
#define MCECC_ELR_ESCRB		0x20	/* Error from Scrubber */
#define MCECC_ELR_ERD		0x40	/* READ signal pin state */
#define MCECC_ELR_ERRLOG	0x80	/* single or double bit error logged */

/* MCECC_DR_1: 0x74	Default Register 1		*/

#define MCECC_DR_1_RSIZ_4MB	0x00	/*   4 Mbytes of Dram */
#define MCECC_DR_1_RSIZ_8MB	0x01	/*   8 Mbytes of Dram */
#define MCECC_DR_1_RSIZ_16MB	0x02	/*  16 Mbytes of Dram */
#define MCECC_DR_1_RSIZ_32MB	0x03	/*  32 Mbytes of Dram */
#define MCECC_DR_1_RSIZ_64MB	0x04	/*  64 Mbytes of Dram */
#define MCECC_DR_1_RSIZ_128MB	0x05	/* 128 Mbytes of Dram */
#define MCECC_DR_1_SELI_000	0x00	/* Reg Base Address : 0xfff43000 */
#define MCECC_DR_1_SELI_100	0x08	/* Reg Base Address : 0xfff43100 */
#define MCECC_DR_1_SELI_200	0x10	/* Reg Base Address : 0xfff43200 */
#define MCECC_DR_1_SELI_300	0x18	/* Reg Base Address : 0xfff43300 */
#define MCECC_DR_1_FSTRD	0x20	/* Fast Read */
#define MCECC_DR_1_STATCOL	0x40	/* RACODE and RADATA locked */

#ifdef __cplusplus
}
#endif

#endif /* __INCmcecch */
