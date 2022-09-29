/* vsbchip2.h - VSBchip2 Interface Controller */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,16feb94,dzb	 written.
*/

/*
DESCRIPTION
This file contains constants for the VSBchip2 interface controller.
*/

#ifdef	DOC
#define __INCvsbchip2h
#endif	/* DOC */

#ifndef	__INCvsbchip2h
#define	__INCvsbchip2h

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	_ASMLANGUAGE
#define CAST
#define CAST_US
#define CAST_UL
#else
#define CAST (char *)
#define CAST_US (unsigned short *)
#define CAST_UL (unsigned long *)
#endif	/* _ASMLANGUAGE */

#ifndef	VSBCHIP2_BASE_ADRS
#define	VSBCHIP2_BASE_ADRS 0xfff41000
#endif	/* VSBCHIP2_BASE_ADRS */

#ifndef	VSBCHIP2_REG_INTERVAL
#define	VSBCHIP2_REG_INTERVAL 1
#endif	/* VSBCHIP2_REG_INTERVAL */

#ifndef	VSB_ADRS
#define	VSB_ADRS(reg) (CAST VSBCHIP2_BASE_ADRS + (reg * VSBCHIP2_REG_INTERVAL))
#endif	/* VSB_ADRS */

#ifndef	VSB_ADRS_US
#define	VSB_ADRS_US(reg) (CAST_US VSB_ADRS(reg))
#endif	/* VSB_ADRS_US */

#ifndef	VSB_ADRS_UL
#define	VSB_ADRS_UL(reg) (CAST_UL VSB_ADRS(reg))
#endif	/* VSB_ADRS_UL */


/**********************************/
/* VSBCHIP2 register locations:   */
/**********************************/

#define VSBCHIP2_CCCSR	VSB_ADRS_US (0x000)	/* Chip Control/Status Reg */
#define VSBCHIP2_LIVBR	VSB_ADRS_US (0x002)	/* Local Int Vector Base Reg */
#define VSBCHIP2_LISR	VSB_ADRS_US (0x004)	/* Local Interrupt Status Reg */
#define VSBCHIP2_LIER	VSB_ADRS_US (0x006)	/* Local Interrupt Enable Reg */
#define VSBCHIP2_LILR	VSB_ADRS_UL (0x008)	/* Local Interrupt Level Reg */
#define VSBCHIP2_VRCSR	VSB_ADRS_US (0x010)	/* VSB Req Control/Status Reg */
#define VSBCHIP2_TCR	VSB_ADRS_US (0x014)	/* Timer Control Register */
#define VSBCHIP2_TCPR	VSB_ADRS_US (0x016)	/* Timer Clock Prescale Reg */
#define VSBCHIP2_LS1ARR	VSB_ADRS_UL (0x018)	/* Local Bus Slave Addr Reg 1 */
#define VSBCHIP2_LS1AOR	VSB_ADRS_US (0x01c)	/* Local Bus Slave Off Reg 1 */
#define VSBCHIP2_LS1AR	VSB_ADRS_US (0x01e)	/* Local Bus Slave Attr Reg 1 */
#define VSBCHIP2_LS2ARR	VSB_ADRS_UL (0x020)	/* Local Bus Slave Addr Reg 2 */
#define VSBCHIP2_LS2AOR	VSB_ADRS_US (0x024)	/* Local Bus Slave Off Reg 2 */
#define VSBCHIP2_LS2AR	VSB_ADRS_US (0x026)	/* Local Bus Slave Attr Reg 2 */
#define VSBCHIP2_LS3ARR	VSB_ADRS_UL (0x028)	/* Local Bus Slave Addr Reg 3 */
#define VSBCHIP2_LS3AOR	VSB_ADRS_US (0x02c)	/* Local Bus Slave Off Reg 3 */
#define VSBCHIP2_LS3AR	VSB_ADRS_US (0x02e)	/* Local Bus Slave Attr Reg 3 */
#define VSBCHIP2_LS4ARR	VSB_ADRS_UL (0x030)	/* Local Bus Slave Addr Reg 4 */
#define VSBCHIP2_LS4AOR	VSB_ADRS_US (0x034)	/* Local Bus Slave Off Reg 4 */
#define VSBCHIP2_LS4AR	VSB_ADRS_US (0x036)	/* Local Bus Slave Attr Reg 4 */
#define VSBCHIP2_LEAR	VSB_ADRS_UL (0x074)	/* Local Error Address Reg */
#define VSBCHIP2_PCCR	VSB_ADRS_UL (0x078)	/* Prescaler Current Cnt Reg */
#define VSBCHIP2_PTR	VSB_ADRS_UL (0x078)	/* Prescaler Test Register */
#define VSBCHIP2_EAR	VSB_ADRS_UL (0x100)	/* EVSB Attention Register */
#define VSBCHIP2_ETASR	VSB_ADRS_UL (0x104)	/* EVSB Test-And-Set Reg */
#define VSBCHIP2_GPR1	VSB_ADRS_UL (0x108)	/* General Purpose Reg 1 */
#define VSBCHIP2_GPR2	VSB_ADRS_UL (0x10c)	/* General Purpose Reg 2 */
#define VSBCHIP2_VESR	VSB_ADRS_UL (0x110)	/* VSB Error Status Register */
#define VSBCHIP2_VICR	VSB_ADRS (0x114)	/* VSB Interrupt Control Reg */
#define VSBCHIP2_VIVR	VSB_ADRS (0x115)	/* VSB Interrupt Vector Reg */
#define VSBCHIP2_VIER	VSB_ADRS (0x116)	/* VSB Interrupt Enable Reg */
#define VSBCHIP2_VISR	VSB_ADRS (0x117)	/* VSB Interrupt Status Reg */
#define VSBCHIP2_VS1ARR	VSB_ADRS_UL (0x118)	/* VSB Slave Addr Range Reg 1 */
#define VSBCHIP2_VS1AOR	VSB_ADRS_US (0x11c)	/* VSB Slave Addr Off Reg 1 */
#define VSBCHIP2_VS1AR	VSB_ADRS_US (0x11e)	/* VSB Slave Attribute Reg 1 */
#define VSBCHIP2_VS2ARR	VSB_ADRS_UL (0x120)	/* VSB Slave Addr Range Reg 2 */
#define VSBCHIP2_VS2AOR	VSB_ADRS_US (0x124)	/* VSB Slave Addr Off Reg 2 */
#define VSBCHIP2_VS2AR	VSB_ADRS_US (0x126)	/* VSB Slave Attribute Reg 2 */

/**********************************/
/* VSBCHIP2 register definitions: */
/**********************************/

/* VSBCHIP2_VRCSR register definitions: */

#define VRCSR_LVRWD     0x0100  /* release when done */

/* VSBCHIP2_VTCR register definitions: */

#define VTCR_VARBTD     0x1000  /* VSB arbitration timer disable */
#define VTCR_VATS_32MS  0x0800  /* VSB access timer select (32 msecs) */
#define VTCR_VATS_DIS   0x0c00  /* VSB access timer select (32 msecs) */
#define VTCR_VTXS_256US 0x0200  /* VSB transfer timer select (256 usecs) */
#define VTCR_VTXS_DIS   0x0300  /* VSB transfer timer select disabled */


/* VSBCHIP2_LS1AR register definitions: */

#define LS1AR_REN       0x8000  /* read enable (when set, read access) */
#define LS1AR_WEN       0x4000  /* write enable (when set, write access) */
#define LS1AR_WPE       0x0800  /* write post enable */
#define LS1AR_BNCEN     0x0080  /* bounce mode enable */

#define LS1AR_VSP1      0x0020
#define LS1AR_VSP0      0x0010


/* VSBCHIP2_VS1AR register definitions: */

#define VS1AR_REN       0x8000  /* read enable (when set, read access) */
#define VS1AR_WEN       0x4000  /* write enable (when set, write access) */
#define VS1AR_POR       0x2000  /* participate on read */
#define VS1AR_POW       0x1000  /* participate on write */
#define VS1AR_WPE       0x0800  /* write post enable */
#define VS1AR_SAS       0x0400  /* system address space */
#define VS1AR_ALTAS     0x0200  /* alternate address space */
#define VS1AR_IOAS      0x0100  /* I/O address space */
#define VS1AR_LOCK      0x0040  /* lock local bus on block transfers */

#define VS1AR_LBTS1     0x0008
#define VS1AR_LBTS0     0x0004  
#define VS1AR_LBSC1     0x0002
#define VS1AR_LBSC0     0x0001
 
#ifdef __cplusplus
}
#endif
 
#endif	/* __INCvsbchip2h */
