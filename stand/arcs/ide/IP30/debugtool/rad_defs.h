/******************************************************************************
 *                                                                   
 *  File:  rad.h						     
 *                                                                   
 *  Description:                                                     
 *
 *	Defines of offsets to RAD's registers
 *  								     
 *  Special Notes:                                                   
 *                                                                   
 *
 *  (C) Copyright 1995 by Silicon Graphics, Inc.                     	
 *  All Rights Reserved.                                             
 *****************************************************************************/
/*
 * $Id: rad_defs.h,v 1.1 1996/12/07 00:47:40 rattan Exp $
 */

#include "sys/RACER/IP30.h"

/*-----------------------------------------------------------------------------
 *
 * define the RAD's base address
 * (Note: these are only temporary.  The forthcoming IO infrastructure will
 * provide a programatic device-indepedent way of obtaining base addresses.)
 *
 *----------------------------------------------------------------------------*/

#define	PCI_CFG_BASE	RAD_PCI_CFG_BASE	/* RAD is Device 3 (to the bridge) */
#define	PCI_MEM_BASE	RAD_PCI_DEVIO_BASE	/* RAD is Device 3 (to the bridge) 1f700000*/

/*-------------------------------------------------------------------------------
 *
 * RAD config space registers
 *
 *------------------------------------------------------------------------------*/

#define	RAD_CFG_ID		(RAD_PCI_CFG_BASE+0x00)	/* ID reg. */
#define	RAD_CFG_STATUS		(RAD_PCI_CFG_BASE+0x04)	/* status/command reg. */
#define	RAD_CFG_REV		(RAD_PCI_CFG_BASE+0x08)	/* class code/rev reg. */
#define	RAD_CFG_LATENCY		(RAD_PCI_CFG_BASE+0x0C)	/* latency timer reg. */
#define	RAD_CFG_MEMORY_BASE	(RAD_PCI_CFG_BASE+0x10)	/* base address reg. */


/*-------------------------------------------------------------------------------
 *
 * RAD memory space registers
 *
 *------------------------------------------------------------------------------*/

/*
 * Misc. Status Registers
 */

#define	RAD_PCI_STATUS		(RAD_PCI_DEVIO_BASE+0x00000000)	/* mirror of RAD_CFG_STATUS reg. */
#define	RAD_ADAT_RX_MSC_UST	(RAD_PCI_DEVIO_BASE+0x00000004)
#define	RAD_ADAT_RX_MSC0_SUBMSC	(RAD_PCI_DEVIO_BASE+0x00000008)
#define	RAD_AES_RX_MSC_UST	(RAD_PCI_DEVIO_BASE+0x0000000C)
#define	RAD_AES_RX_MSC0_SUBMSC	(RAD_PCI_DEVIO_BASE+0x00000010)
#define	RAD_ATOD_MSC_UST	(RAD_PCI_DEVIO_BASE+0x00000014)
#define	RAD_ATOD_MSC0_SUBMSC	(RAD_PCI_DEVIO_BASE+0x00000018)
#define	RAD_ADAT_TX_MSC_UST	(RAD_PCI_DEVIO_BASE+0x0000001C)
#define	RAD_ADAT_TX_MSC0_SUBMSC	(RAD_PCI_DEVIO_BASE+0x00000020)
#define	RAD_AES_TX_MSC_UST	(RAD_PCI_DEVIO_BASE+0x00000024)
#define	RAD_AES_TX_MSC0_SUBMSC	(RAD_PCI_DEVIO_BASE+0x00000028)
#define	RAD_DTOA_MSC_UST	(RAD_PCI_DEVIO_BASE+0x0000002C)
#define	RAD_UST_REGISTER	(RAD_PCI_DEVIO_BASE+0x00000030)
#define	RAD_GPIO_STATUS		(RAD_PCI_DEVIO_BASE+0x00000034)
#define	RAD_CHIP_STATUS1	(RAD_PCI_DEVIO_BASE+0x00000038)
#define	RAD_CHIP_STATUS0	(RAD_PCI_DEVIO_BASE+0x0000003C)

/*
 * Control Registers
 */

#define	RAD_UST_CLOCK_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000040)
#define	RAD_ADAT_RX_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000044)
#define	RAD_AES_RX_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000048)
#define	RAD_ATOD_CONTROL	(RAD_PCI_DEVIO_BASE+0x0000004C)
#define	RAD_ADAT_TX_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000050)
#define	RAD_AES_TX_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000054)
#define	RAD_DTOA_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000058)
#define	RAD_STATUS_TIMER	(RAD_PCI_DEVIO_BASE+0x0000005C)

/*
 * PCI Master DMA Control Registers
 */

#define	RAD_MISC_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000070)
#define	RAD_PCI_HOLDOFF		(RAD_PCI_DEVIO_BASE+0x00000074)
#define	RAD_PCI_ARB_CONTROL	(RAD_PCI_DEVIO_BASE+0x00000078)

/*
 * Device PIO Indirect Register
 */

#define	RAD_VOLUME_CONTROL	(RAD_PCI_DEVIO_BASE+0x0000007C)

/*
 * Reset Register
 */

#define	RAD_RESET		(RAD_PCI_DEVIO_BASE+0x00000080)

/*
 * GPIO Registers
 */

#define	RAD_GPIO0		(RAD_PCI_DEVIO_BASE+0x00000084)
#define	RAD_GPIO1		(RAD_PCI_DEVIO_BASE+0x00000088)
#define	RAD_GPIO2		(RAD_PCI_DEVIO_BASE+0x0000008C)
#define	RAD_GPIO3		(RAD_PCI_DEVIO_BASE+0x00000090)

/*
 * Clock Generator Registers
 */

#define	RAD_CLOCKGEN_ICTL	(RAD_PCI_DEVIO_BASE+0x000000A0)
#define	RAD_CLOCKGEN_REM	(RAD_PCI_DEVIO_BASE+0x000000A4)
#define	RAD_FREQ_SYNTH3_MUX_SEL	(RAD_PCI_DEVIO_BASE+0x000000A8)
#define	RAD_FREQ_SYNTH2_MUX_SEL	(RAD_PCI_DEVIO_BASE+0x000000AC)
#define	RAD_FREQ_SYNTH1_MUX_SEL	(RAD_PCI_DEVIO_BASE+0x000000B0)
#define	RAD_FREQ_SYNTH0_MUX_SEL	(RAD_PCI_DEVIO_BASE+0x000000B4)
#define	RAD_MPLL0_LOCK_CONTROL	(RAD_PCI_DEVIO_BASE+0x000000B8)
#define	RAD_MPLL1_LOCK_CONTROL	(RAD_PCI_DEVIO_BASE+0x000000BC)

/*
 * DMA Descriptor RAM START TESTING
 */

#define	RAD_PCI_LOADR_D0	(RAD_PCI_DEVIO_BASE+0x00000400)
#define	RAD_PCI_HIADR_D0	(RAD_PCI_DEVIO_BASE+0x00000404)
#define	RAD_PCI_CONTROL_D0	(RAD_PCI_DEVIO_BASE+0x00000408)
#define	RAD_PCI_LOADR_D1	(RAD_PCI_DEVIO_BASE+0x0000040C)
#define	RAD_PCI_HIADR_D1	(RAD_PCI_DEVIO_BASE+0x00000410)
#define	RAD_PCI_CONTROL_D1	(RAD_PCI_DEVIO_BASE+0x00000414)
#define	RAD_PCI_LOADR_D2	(RAD_PCI_DEVIO_BASE+0x00000418)
#define	RAD_PCI_HIADR_D2	(RAD_PCI_DEVIO_BASE+0x0000041C)
#define	RAD_PCI_CONTROL_D2	(RAD_PCI_DEVIO_BASE+0x00000420)
#define	RAD_PCI_LOADR_D3	(RAD_PCI_DEVIO_BASE+0x00000424)
#define	RAD_PCI_HIADR_D3	(RAD_PCI_DEVIO_BASE+0x00000428)
#define	RAD_PCI_CONTROL_D3	(RAD_PCI_DEVIO_BASE+0x0000042C)
#define	RAD_PCI_LOADR_D4	(RAD_PCI_DEVIO_BASE+0x00000430)
#define	RAD_PCI_HIADR_D4	(RAD_PCI_DEVIO_BASE+0x00000434)
#define	RAD_PCI_CONTROL_D4	(RAD_PCI_DEVIO_BASE+0x00000438)
#define	RAD_PCI_LOADR_D5	(RAD_PCI_DEVIO_BASE+0x0000043C)
#define	RAD_PCI_HIADR_D5	(RAD_PCI_DEVIO_BASE+0x00000440)
#define	RAD_PCI_CONTROL_D5	(RAD_PCI_DEVIO_BASE+0x00000444)
#define	RAD_PCI_LOADR_D6	(RAD_PCI_DEVIO_BASE+0x00000448)
#define	RAD_PCI_HIADR_D6	(RAD_PCI_DEVIO_BASE+0x0000044C)
#define	RAD_PCI_CONTROL_D6	(RAD_PCI_DEVIO_BASE+0x00000450)
#define	RAD_PCI_LOADR_D7	(RAD_PCI_DEVIO_BASE+0x00000454)
#define	RAD_PCI_HIADR_D7	(RAD_PCI_DEVIO_BASE+0x00000458)
#define	RAD_PCI_CONTROL_D7	(RAD_PCI_DEVIO_BASE+0x0000045C)
#define	RAD_PCI_LOADR_D8	(RAD_PCI_DEVIO_BASE+0x00000460)
#define	RAD_PCI_HIADR_D8	(RAD_PCI_DEVIO_BASE+0x00000464)
#define	RAD_PCI_CONTROL_D8	(RAD_PCI_DEVIO_BASE+0x00000468)
#define	RAD_PCI_LOADR_D9	(RAD_PCI_DEVIO_BASE+0x0000046C)
#define	RAD_PCI_HIADR_D9	(RAD_PCI_DEVIO_BASE+0x00000470)
#define	RAD_PCI_CONTROL_D9	(RAD_PCI_DEVIO_BASE+0x00000474)
#define	RAD_PCI_LOADR_D10	(RAD_PCI_DEVIO_BASE+0x00000478)
#define	RAD_PCI_HIADR_D10	(RAD_PCI_DEVIO_BASE+0x0000047C)
#define	RAD_PCI_CONTROL_D10	(RAD_PCI_DEVIO_BASE+0x00000480)
#define	RAD_PCI_LOADR_D11	(RAD_PCI_DEVIO_BASE+0x00000484)
#define	RAD_PCI_HIADR_D11	(RAD_PCI_DEVIO_BASE+0x00000488)
#define	RAD_PCI_CONTROL_D11	(RAD_PCI_DEVIO_BASE+0x0000048C)
#define	RAD_PCI_LOADR_D12	(RAD_PCI_DEVIO_BASE+0x00000490)
#define	RAD_PCI_HIADR_D12	(RAD_PCI_DEVIO_BASE+0x00000494)
#define	RAD_PCI_CONTROL_D12	(RAD_PCI_DEVIO_BASE+0x00000498)
#define	RAD_PCI_LOADR_D13	(RAD_PCI_DEVIO_BASE+0x0000049C)
#define	RAD_PCI_HIADR_D13	(RAD_PCI_DEVIO_BASE+0x000004A0)
#define	RAD_PCI_CONTROL_D13	(RAD_PCI_DEVIO_BASE+0x000004A4)
#define	RAD_PCI_LOADR_D14	(RAD_PCI_DEVIO_BASE+0x000004A8)
#define	RAD_PCI_HIADR_D14	(RAD_PCI_DEVIO_BASE+0x000004AC)
#define	RAD_PCI_CONTROL_D14	(RAD_PCI_DEVIO_BASE+0x000004B0)
#define	RAD_PCI_LOADR_D15	(RAD_PCI_DEVIO_BASE+0x000004B4)
#define	RAD_PCI_HIADR_D15	(RAD_PCI_DEVIO_BASE+0x000004B8)
#define	RAD_PCI_CONTROL_D15	(RAD_PCI_DEVIO_BASE+0x000004BC)

/*
 * DMA working registers
 */

#define	RAD_PCI_LOADR_ADAT_RX		(RAD_PCI_DEVIO_BASE+0x000004C0)
#define	RAD_PCI_CONTROL_ADAT_RX		(RAD_PCI_DEVIO_BASE+0x000004C4)
#define	RAD_PCI_LOADR_AES_RX		(RAD_PCI_DEVIO_BASE+0x000004C8)
#define	RAD_PCI_CONTROL_AES_RX		(RAD_PCI_DEVIO_BASE+0x000004CC)
#define	RAD_PCI_LOADR_ATOD		(RAD_PCI_DEVIO_BASE+0x000004D0)
#define	RAD_PCI_CONTROL_ATOD		(RAD_PCI_DEVIO_BASE+0x000004D4)
#define	RAD_PCI_LOADR_ADATSUB_RX	(RAD_PCI_DEVIO_BASE+0x000004D8)
#define	RAD_PCI_CONTROL_ADATSUB_RX	(RAD_PCI_DEVIO_BASE+0x000004DC)
#define	RAD_PCI_LOADR_AESSUB_RX		(RAD_PCI_DEVIO_BASE+0x000004E0)
#define	RAD_PCI_CONTROL_AESSUB_RX	(RAD_PCI_DEVIO_BASE+0x000004E4)
#define	RAD_PCI_LOADR_ADAT_TX		(RAD_PCI_DEVIO_BASE+0x000004E8)
#define	RAD_PCI_CONTROL_ADAT_TX		(RAD_PCI_DEVIO_BASE+0x000004EC)
#define	RAD_PCI_LOADR_AES_TX		(RAD_PCI_DEVIO_BASE+0x000004F0)
#define	RAD_PCI_CONTROL_AES_TX		(RAD_PCI_DEVIO_BASE+0x000004F4)
#define	RAD_PCI_LOADR_DTOA		(RAD_PCI_DEVIO_BASE+0x000004F8)
#define	RAD_PCI_CONTROL_DTOA		(RAD_PCI_DEVIO_BASE+0x000004FC)
#define	RAD_PCI_LOADR_STATUS		(RAD_PCI_DEVIO_BASE+0x00000500)
#define	RAD_PCI_CONTROL_STATUS		(RAD_PCI_DEVIO_BASE+0x00000504)
#define	RAD_PCI_HIADR_ADAT_RX		(RAD_PCI_DEVIO_BASE+0x00000508)
#define	RAD_PCI_HIADR_AES_RX		(RAD_PCI_DEVIO_BASE+0x0000050C)
#define	RAD_PCI_HIADR_ATOD		(RAD_PCI_DEVIO_BASE+0x00000510)
#define	RAD_PCI_HIADR_ADATSUB_RX	(RAD_PCI_DEVIO_BASE+0x00000514)
#define	RAD_PCI_HIADR_AESSUB_RX		(RAD_PCI_DEVIO_BASE+0x00000518)
#define	RAD_PCI_HIADR_ADAT_TX		(RAD_PCI_DEVIO_BASE+0x0000051C)
#define	RAD_PCI_HIADR_AES_TX		(RAD_PCI_DEVIO_BASE+0x00000520)
#define	RAD_PCI_HIADR_DTOA		(RAD_PCI_DEVIO_BASE+0x00000524)
#define	RAD_PCI_HIADR_STATUS		(RAD_PCI_DEVIO_BASE+0x00000528)

/*
 * ADAT Subcode Tx A RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define	RAD_ADAT_SUBCODE_TXA_U0_0 	(RAD_PCI_DEVIO_BASE+0x00001000)
#define	RAD_ADAT_SUBCODE_TXA_U0_1 	(RAD_PCI_DEVIO_BASE+0x00001004)
#define	RAD_ADAT_SUBCODE_TXA_U0_2 	(RAD_PCI_DEVIO_BASE+0x00001008)
#define	RAD_ADAT_SUBCODE_TXA_U0_3	(RAD_PCI_DEVIO_BASE+0x0000100C)
#define	RAD_ADAT_SUBCODE_TXA_U0_4	(RAD_PCI_DEVIO_BASE+0x00001010)
#define	RAD_ADAT_SUBCODE_TXA_U0_5	(RAD_PCI_DEVIO_BASE+0x00001014)

#define	RAD_ADAT_SUBCODE_TXA_U1_0 	(RAD_PCI_DEVIO_BASE+0x00001018)
#define	RAD_ADAT_SUBCODE_TXA_U1_1 	(RAD_PCI_DEVIO_BASE+0x0000101C)
#define	RAD_ADAT_SUBCODE_TXA_U1_2 	(RAD_PCI_DEVIO_BASE+0x00001020)
#define	RAD_ADAT_SUBCODE_TXA_U1_3 	(RAD_PCI_DEVIO_BASE+0x00001024)
#define	RAD_ADAT_SUBCODE_TXA_U1_4 	(RAD_PCI_DEVIO_BASE+0x00001028)
#define	RAD_ADAT_SUBCODE_TXA_U1_5 	(RAD_PCI_DEVIO_BASE+0x0000102C)

#define	RAD_ADAT_SUBCODE_TXA_U2_0 	(RAD_PCI_DEVIO_BASE+0x00001030)
#define	RAD_ADAT_SUBCODE_TXA_U2_1 	(RAD_PCI_DEVIO_BASE+0x00001034)
#define	RAD_ADAT_SUBCODE_TXA_U2_2 	(RAD_PCI_DEVIO_BASE+0x00001038)
#define	RAD_ADAT_SUBCODE_TXA_U2_3 	(RAD_PCI_DEVIO_BASE+0x0000103C)
#define	RAD_ADAT_SUBCODE_TXA_U2_4 	(RAD_PCI_DEVIO_BASE+0x00001040)
#define	RAD_ADAT_SUBCODE_TXA_U2_5 	(RAD_PCI_DEVIO_BASE+0x00001044)

#define	RAD_ADAT_SUBCODE_TXA_U3_0 	(RAD_PCI_DEVIO_BASE+0x00001048)
#define	RAD_ADAT_SUBCODE_TXA_U3_1 	(RAD_PCI_DEVIO_BASE+0x0000104C)
#define	RAD_ADAT_SUBCODE_TXA_U3_2 	(RAD_PCI_DEVIO_BASE+0x00001050)
#define	RAD_ADAT_SUBCODE_TXA_U3_3 	(RAD_PCI_DEVIO_BASE+0x00001054)
#define	RAD_ADAT_SUBCODE_TXA_U3_4 	(RAD_PCI_DEVIO_BASE+0x00001058)
#define	RAD_ADAT_SUBCODE_TXA_U3_5 	(RAD_PCI_DEVIO_BASE+0x0000105C)

/*
 * ADAT Subcode Tx B RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define	RAD_ADAT_SUBCODE_TXB_U0_0	(RAD_PCI_DEVIO_BASE+0x00001080)

/*
 * ADAT Subcode Rx A RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define RAD_ADAT_SUBCODE_RXA_U0_0       (RAD_PCI_DEVIO_BASE+0x00001800)

/*
 * ADAT Subcode Rx B RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define RAD_ADAT_SUBCODE_RXB_U0_0       (RAD_PCI_DEVIO_BASE+0x00001880)

/*
 * AES Subcode Tx A RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define	RAD_AES_SUBCODE_TXA_LU0		(RAD_PCI_DEVIO_BASE+0x00001100)
#define RAD_AES_SUBCODE_TXA_RV2         (RAD_PCI_DEVIO_BASE+0x00001200)

/*
 * AES Subcode Tx B RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define RAD_AES_SUBCODE_TXB_LU0		(RAD_PCI_DEVIO_BASE+0x00001180)
#define RAD_AES_SUBCODE_TXB_RV2		(RAD_PCI_DEVIO_BASE+0x00001210)

/*
 * AES Subcode Rx A RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define	RAD_AES_SUBCODE_RXA_LU0		(RAD_PCI_DEVIO_BASE+0x00001900)
#define RAD_AES_SUBCODE_RXA_RV2         (RAD_PCI_DEVIO_BASE+0x00001A00)

/*
 * AES Subcode Rx B RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define RAD_AES_SUBCODE_RXB_LU0		(RAD_PCI_DEVIO_BASE+0x00001980)
#define RAD_AES_SUBCODE_RXB_RV2		(RAD_PCI_DEVIO_BASE+0x00001A10)

/*
 * Tx RAM
 * (Note: reading and writing only enabled during RAM test mode.)
 */

#define RAD_ADAT_TX_DATA	(RAD_PCI_DEVIO_BASE+0x00001400)
#define	RAD_AES_TX_DATA		(RAD_PCI_DEVIO_BASE+0x00001600)
#define	RAD_DTOA_DATA		(RAD_PCI_DEVIO_BASE+0x00001700)

/*
 * Rx RAM
 * (Note: reading and writing only enabled during RAM test mode.)
 */

#define RAD_ADAT_RX_DATA	(RAD_PCI_DEVIO_BASE+0x00001C00)
#define	RAD_AES_RX_DATA		(RAD_PCI_DEVIO_BASE+0x00001E00)
#define	RAD_ATOD_DATA		(RAD_PCI_DEVIO_BASE+0x00001F00)

