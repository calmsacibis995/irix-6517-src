/* memc040.h - Memory Controller ASIC for the MC68040-type bus */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01a,18jun91,ccc	 written.
*/

#ifdef	DOC
#define	INCmemc040h
#endif	/* DOC */

#ifndef __INCmemc040h
#define __INCmemc040h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file contains constants for Motorola's Memory Controller ASIC.
 * The macro MEMC_BASE_ADRS must be defined when including this header.
 */

#ifdef	_ASMLANGUAGE
#define CAST
#else
#define CAST (char *)
#endif	/* _ASMLANGUAGE */

/* on-board access, register definitions */

#define	MEMC_REG_INTERVAL	1

#ifndef MEMC_ADRS	/* to permit alternative board addressing */
#define MEMC_ADRS(reg)   (CAST (MEMC_BASE_ADRS + (reg * MEMC_REG_INTERVAL)))
#endif	/* MEMC_ADRS */

#define MEMC_ID		MEMC_ADRS(0x00) /* Chip ID 			*/
#define	MEMC_REVISION	MEMC_ADRS(0x04) /* Chip Revision Register	*/
#define	MEMC_MCR	MEMC_ADRS(0x08) /* Memory Configuration Register*/
#define	MEMC_ASR	MEMC_ADRS(0x0c) /* Alternate Status Register	*/
#define	MEMC_ACR	MEMC_ADRS(0x10) /* Alternate Control Register	*/
#define	MEMC_BAR	MEMC_ADRS(0x14) /* Base Address Register	*/
#define	MEMC_RCR	MEMC_ADRS(0x18) /* RAM Control Register		*/
#define	MEMC_BCR	MEMC_ADRS(0x1c) /* Bus Clock Register		*/

/****************************************************************
 * MEMC_ID		0x00	Chip ID				*
 ****************************************************************/

#define	MEMC_ID_RESET		0x0f	/* software reset of chip	*/

/****************************************************************
 * MEMC_REVISION	0x04	Chip Revision Register		*
 ****************************************************************/
/* NONE */

/****************************************************************
 * MEMC_MCR		0x08	Memory Configuration Register	*
 ****************************************************************/

#define	MEMC_MCR_4MB		0x00	/* 4 Meg of Memory		2-0 */
#define	MEMC_MCR_8MB		0x01	/* 8 Meg of Memory		2-0 */
#define	MEMC_MCR_16MB		0x02	/* 16 Meg of Memory		2-0 */
#define	MEMC_MCR_32MB		0x03	/* 32 Meg of Memory		2-0 */
#define	MEMC_MCR_64MB		0x04	/* 64 Meg of Memory		2-0 */
#define	MEMC_MCR_128MB		0x05	/* 128 Meg of Memory		2-0 */
#define	MEMC_MCR_WPB		0x08	/* Write-Per-Bit mode		3 */
#define	MEMC_MCR_EXTPEN		0x10	/* Enternal Parity Enable	4 */
#define	MEMC_MCR_FSTRD		0x20	/* Fast Read			5 */

/****************************************************************
 * MEMC_ASR		0x0c	Alternate Status Register	*
 ****************************************************************/

#define	MEMC_ASR_APD		0xf0	/* State of the APD3-APD0 pins	7-4 */
#define	MEMC_ASR_BPD		0x0f	/* State of the BPD3-BPD0 pins	3-0 */

/****************************************************************
 * MEMC_ACR		0x10	Alternate Control Register	*
 ****************************************************************/

#define	MEMC_ACR_DPD0		0x01	/* DPD0 output pin		0 */
#define	MEMC_ACR_DPD1		0x02	/* DPD1 output pin		1 */
#define	MEMC_ACR_DPD2		0x04	/* DPD2 output pin		2 */
#define	MEMC_ACR_DPD3		0x08	/* DPD3 output pin		3 */
#define	MEMC_ACR_CPD0		0x10	/* CPD0 output pin		4 */
#define	MEMC_ACR_CPD1		0x20	/* CPD1 output pin		5 */
#define	MEMC_ACR_CPD2		0x40	/* CPD2 output pin		6 */
#define	MEMC_ACR_CPD3		0x80	/* CPD3 output pin		7 */

/****************************************************************
 * MEMC_BAR		0x14	Base Address Register		*
 ****************************************************************/
/* NONE */

/****************************************************************
 * MEMC_RCR		0x18	RAM Control Register		*
 ****************************************************************/

#define	MEMC_RCR_RAMEN		0x01	/* RAM Enable			0 */
#define	MEMC_RCR_PAREN		0x02	/* Parity Enable		1 */
#define	MEMC_RCR_PARINT		0x04	/* Parity Interrupt Enable	2 */
#define	MEMC_RCR_WWP		0x08	/* Write Wrong Parity		3 */
#define	MEMC_RCR_SWAIT		0x10	/* Snoop Wait			4 */
#define	MEMC_RCR_DMCTL		0x20	/* Data Mux Control		5 */

/****************************************************************
 * MEMC_BCR		0x1c	Bus Clock Register		*
 ****************************************************************/

#define	MEMC_BCR_16MHZ		16	/* 16 MHz clock */
#define	MEMC_BCR_25MHZ		25	/* 25 MHz clock */
#define	MEMC_BCR_33MHZ		33	/* 33 MHz clock */

#ifdef __cplusplus
}
#endif

#endif /* __INCmemc040h */
