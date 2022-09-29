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
 * $Id: rad.h,v 1.4 1996/08/31 01:19:15 jeffs Exp $
 */

/*-----------------------------------------------------------------------------
 *
 * define the RAD's base address
 * (Note: these are only temporary.  The forthcoming IO infrastructure will
 * provide a programatic device-indepedent way of obtaining base addresses.)
 *
 *----------------------------------------------------------------------------*/

#ifdef RADIGO	/* RAD on Indigo2 */

#define	PCI_CFG_BASE	0x1f4c0000	/* slot 0: 1f480000, slot 1: 1f4c0000 */
#define	PCI_MEM_BASE	0x1f500000	/* both slots are mapped in this space */

#define	BYTE_COUNT	0x1f400000	/* specific to GIO2PCI backplane */

#else 		/* RAD on IP30 */

#define	PCI_CFG_BASE	0x1f025000	/* RAD is Device 5 (to the bridge) */
#define	PCI_MEM_BASE	0x1f900000	/* RAD is Device 5 (to the bridge) */

#endif


/*-------------------------------------------------------------------------------
 *
 * RAD config space registers
 *
 *------------------------------------------------------------------------------*/

#define	RAD_CFG_ID		(0x00)	/* ID reg. */
#define	RAD_CFG_STATUS		(0x04)	/* status/command reg. */
#define	RAD_CFG_CLASS		(0x08)	/* class code/rev reg. */
#define	RAD_CFG_LATENCY		(0x0C)	/* latency timer reg. */
#define	RAD_CFG_MEMORY_BASE	(0x10)	/* base address reg. */


/*-------------------------------------------------------------------------------
 *
 * RAD memory space registers
 *
 *------------------------------------------------------------------------------*/

/*
 * Misc. Status Registers
 */

#define	RAD_PCI_STATUS		(0x00000000)	/* mirror of RAD_CFG_STATUS reg. */
#define	RAD_ADAT_RX_MSC_UST	(0x00000004)
#define	RAD_ADAT_RX_MSC0_SUBMSC	(0x00000008)
#define	RAD_AES_RX_MSC_UST	(0x0000000C)
#define	RAD_AES_RX_MSC0_SUBMSC	(0x00000010)
#define	RAD_ATOD_MSC_UST	(0x00000014)
#define	RAD_ATOD_MSC0_SUBMSC	(0x00000018)
#define	RAD_ADAT_TX_MSC_UST	(0x0000001C)
#define	RAD_ADAT_TX_MSC0_SUBMSC	(0x00000020)
#define	RAD_AES_TX_MSC_UST	(0x00000024)
#define	RAD_AES_TX_MSC0_SUBMSC	(0x00000028)
#define	RAD_DTOA_MSC_UST	(0x0000002C)
#define	RAD_UST_REGISTER	(0x00000030)
#define	RAD_GPIO_STATUS		(0x00000034)
#define	RAD_CHIP_STATUS1	(0x00000038)
#define	RAD_CHIP_STATUS0	(0x0000003C)

/*
 * Control Registers
 */

#define	RAD_UST_CLOCK_CONTROL	(0x00000040)
#define	RAD_ADAT_RX_CONTROL	(0x00000044)
#define	RAD_AES_RX_CONTROL	(0x00000048)
#define	RAD_ATOD_CONTROL	(0x0000004C)
#define	RAD_ADAT_TX_CONTROL	(0x00000050)
#define	RAD_AES_TX_CONTROL	(0x00000054)
#define	RAD_DTOA_CONTROL	(0x00000058)
#define	RAD_STATUS_TIMER	(0x0000005C)

/*
 * PCI Master DMA Control Registers
 */

#define	RAD_MISC_CONTROL	(0x00000070)
#define	RAD_PCI_HOLDOFF		(0x00000074)
#define	RAD_PCI_ARB_CONTROL	(0x00000078)

/*
 * Device PIO Indirect Register
 */

#define	RAD_VOLUME_CONTROL	(0x0000007C)

/*
 * Reset Register
 */

#define	RAD_RESET		(0x00000080)

/*
 * GPIO Registers
 */

#define	RAD_GPIO0		(0x00000084)
#define	RAD_GPIO1		(0x00000088)
#define	RAD_GPIO2		(0x0000008C)
#define	RAD_GPIO3		(0x00000090)

/*
 * Clock Generator Registers
 */

#define	RAD_CLOCKGEN_ICTL	(0x000000A0)
#define	RAD_CLOCKGEN_REM	(0x000000A4)
#define	RAD_FREQ_SYNTH3_MUX_SEL	(0x000000A8)
#define	RAD_FREQ_SYNTH2_MUX_SEL	(0x000000AC)
#define	RAD_FREQ_SYNTH1_MUX_SEL	(0x000000B0)
#define	RAD_FREQ_SYNTH0_MUX_SEL	(0x000000B4)
#define	RAD_MPLL0_LOCK_CONTROL	(0x000000B8)
#define	RAD_MPLL1_LOCK_CONTROL	(0x000000BC)

/*
 * DMA Descriptor RAM
 */

#define	RAD_PCI_LOADR_D0	(0x00000400)
#define	RAD_PCI_HIADR_D0	(0x00000404)
#define	RAD_PCI_CONTROL_D0	(0x00000408)
#define	RAD_PCI_LOADR_D1	(0x0000040C)
#define	RAD_PCI_HIADR_D1	(0x00000410)
#define	RAD_PCI_CONTROL_D1	(0x00000414)
#define	RAD_PCI_LOADR_D2	(0x00000418)
#define	RAD_PCI_HIADR_D2	(0x0000041C)
#define	RAD_PCI_CONTROL_D2	(0x00000420)
#define	RAD_PCI_LOADR_D3	(0x00000424)
#define	RAD_PCI_HIADR_D3	(0x00000428)
#define	RAD_PCI_CONTROL_D3	(0x0000042C)
#define	RAD_PCI_LOADR_D4	(0x00000430)
#define	RAD_PCI_HIADR_D4	(0x00000434)
#define	RAD_PCI_CONTROL_D4	(0x00000438)
#define	RAD_PCI_LOADR_D5	(0x0000043C)
#define	RAD_PCI_HIADR_D5	(0x00000440)
#define	RAD_PCI_CONTROL_D5	(0x00000444)
#define	RAD_PCI_LOADR_D6	(0x00000448)
#define	RAD_PCI_HIADR_D6	(0x0000044C)
#define	RAD_PCI_CONTROL_D6	(0x00000450)
#define	RAD_PCI_LOADR_D7	(0x00000454)
#define	RAD_PCI_HIADR_D7	(0x00000458)
#define	RAD_PCI_CONTROL_D7	(0x0000045C)
#define	RAD_PCI_LOADR_D8	(0x00000460)
#define	RAD_PCI_HIADR_D8	(0x00000464)
#define	RAD_PCI_CONTROL_D8	(0x00000468)
#define	RAD_PCI_LOADR_D9	(0x0000046C)
#define	RAD_PCI_HIADR_D9	(0x00000470)
#define	RAD_PCI_CONTROL_D9	(0x00000474)
#define	RAD_PCI_LOADR_D10	(0x00000478)
#define	RAD_PCI_HIADR_D10	(0x0000047C)
#define	RAD_PCI_CONTROL_D10	(0x00000480)
#define	RAD_PCI_LOADR_D11	(0x00000484)
#define	RAD_PCI_HIADR_D11	(0x00000488)
#define	RAD_PCI_CONTROL_D11	(0x0000048C)
#define	RAD_PCI_LOADR_D12	(0x00000490)
#define	RAD_PCI_HIADR_D12	(0x00000494)
#define	RAD_PCI_CONTROL_D12	(0x00000498)
#define	RAD_PCI_LOADR_D13	(0x0000049C)
#define	RAD_PCI_HIADR_D13	(0x000004A0)
#define	RAD_PCI_CONTROL_D13	(0x000004A4)
#define	RAD_PCI_LOADR_D14	(0x000004A8)
#define	RAD_PCI_HIADR_D14	(0x000004AC)
#define	RAD_PCI_CONTROL_D14	(0x000004B0)
#define	RAD_PCI_LOADR_D15	(0x000004B4)
#define	RAD_PCI_HIADR_D15	(0x000004B8)
#define	RAD_PCI_CONTROL_D15	(0x000004BC)

/*
 * DMA working registers
 */

#define	RAD_PCI_LOADR_ADAT_RX		(0x000004C0)
#define	RAD_PCI_CONTROL_ADAT_RX		(0x000004C4)
#define	RAD_PCI_LOADR_AES_RX		(0x000004C8)
#define	RAD_PCI_CONTROL_AES_RX		(0x000004CC)
#define	RAD_PCI_LOADR_ATOD		(0x000004D0)
#define	RAD_PCI_CONTROL_ATOD		(0x000004D4)
#define	RAD_PCI_LOADR_ADATSUB_RX	(0x000004D8)
#define	RAD_PCI_CONTROL_ADATSUB_RX	(0x000004DC)
#define	RAD_PCI_LOADR_AESSUB_RX		(0x000004E0)
#define	RAD_PCI_CONTROL_AESSUB_RX	(0x000004E4)
#define	RAD_PCI_LOADR_ADAT_TX		(0x000004E8)
#define	RAD_PCI_CONTROL_ADAT_TX		(0x000004EC)
#define	RAD_PCI_LOADR_AES_TX		(0x000004F0)
#define	RAD_PCI_CONTROL_AES_TX		(0x000004F4)
#define	RAD_PCI_LOADR_DTOA		(0x000004F8)
#define	RAD_PCI_CONTROL_DTOA		(0x000004FC)
#define	RAD_PCI_LOADR_STATUS		(0x00000500)
#define	RAD_PCI_CONTROL_STATUS		(0x00000504)
#define	RAD_PCI_HIADR_ADAT_RX		(0x00000508)
#define	RAD_PCI_HIADR_AES_RX		(0x0000050C)
#define	RAD_PCI_HIADR_ATOD		(0x00000510)
#define	RAD_PCI_HIADR_ADATSUB_RX	(0x00000514)
#define	RAD_PCI_HIADR_AESSUB_RX		(0x00000518)
#define	RAD_PCI_HIADR_ADAT_TX		(0x0000051C)
#define	RAD_PCI_HIADR_AES_TX		(0x00000520)
#define	RAD_PCI_HIADR_DTOA		(0x00000524)
#define	RAD_PCI_HIADR_STATUS		(0x00000528)

/*
 * ADAT Subcode Tx A RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define	RAD_ADAT_SUBCODE_TXA_U0_0 	(0x00001000)
#define	RAD_ADAT_SUBCODE_TXA_U0_1 	(0x00001004)
#define	RAD_ADAT_SUBCODE_TXA_U0_2 	(0x00001008)
#define	RAD_ADAT_SUBCODE_TXA_U0_3	(0x0000100C)
#define	RAD_ADAT_SUBCODE_TXA_U0_4	(0x00001010)
#define	RAD_ADAT_SUBCODE_TXA_U0_5	(0x00001014)

#define	RAD_ADAT_SUBCODE_TXA_U1_0 	(0x00001018)
#define	RAD_ADAT_SUBCODE_TXA_U1_1 	(0x0000101C)
#define	RAD_ADAT_SUBCODE_TXA_U1_2 	(0x00001020)
#define	RAD_ADAT_SUBCODE_TXA_U1_3 	(0x00001024)
#define	RAD_ADAT_SUBCODE_TXA_U1_4 	(0x00001028)
#define	RAD_ADAT_SUBCODE_TXA_U1_5 	(0x0000102C)

#define	RAD_ADAT_SUBCODE_TXA_U2_0 	(0x00001030)
#define	RAD_ADAT_SUBCODE_TXA_U2_1 	(0x00001034)
#define	RAD_ADAT_SUBCODE_TXA_U2_2 	(0x00001038)
#define	RAD_ADAT_SUBCODE_TXA_U2_3 	(0x0000103C)
#define	RAD_ADAT_SUBCODE_TXA_U2_4 	(0x00001040)
#define	RAD_ADAT_SUBCODE_TXA_U2_5 	(0x00001044)

#define	RAD_ADAT_SUBCODE_TXA_U3_0 	(0x00001048)
#define	RAD_ADAT_SUBCODE_TXA_U3_1 	(0x0000104C)
#define	RAD_ADAT_SUBCODE_TXA_U3_2 	(0x00001050)
#define	RAD_ADAT_SUBCODE_TXA_U3_3 	(0x00001054)
#define	RAD_ADAT_SUBCODE_TXA_U3_4 	(0x00001058)
#define	RAD_ADAT_SUBCODE_TXA_U3_5 	(0x0000105C)

#define	RAD_ADAT_SUBCODE_TXA_UNUSED (0x00001060)

/*
 * ADAT Subcode Tx A RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define	RAD_ADAT_SUBCODE_TXB_U0_0	(0x00001080)
#define	RAD_ADAT_SUBCODE_TXB_U0_1	(0x00001084)
#define	RAD_ADAT_SUBCODE_TXB_U0_2	(0x00001088)
#define	RAD_ADAT_SUBCODE_TXB_U0_3	(0x0000108C)
#define	RAD_ADAT_SUBCODE_TXB_U0_4	(0x00001090)
#define	RAD_ADAT_SUBCODE_TXB_U0_5	(0x00001094)

#define	RAD_ADAT_SUBCODE_TXB_U1_0	(0x00001098)
#define	RAD_ADAT_SUBCODE_TXB_U1_1	(0x0000109C)
#define	RAD_ADAT_SUBCODE_TXB_U1_2	(0x000010A0)
#define	RAD_ADAT_SUBCODE_TXB_U1_3	(0x000010A4)
#define	RAD_ADAT_SUBCODE_TXB_U1_4	(0x000010A8)
#define	RAD_ADAT_SUBCODE_TXB_U1_5	(0x000010AC)

#define	RAD_ADAT_SUBCODE_TXB_U2_0	(0x000010B0)
#define	RAD_ADAT_SUBCODE_TXB_U2_1	(0x000010B4)
#define	RAD_ADAT_SUBCODE_TXB_U2_2	(0x000010B8)
#define	RAD_ADAT_SUBCODE_TXB_U2_3	(0x000010BC)
#define	RAD_ADAT_SUBCODE_TXB_U2_4	(0x000010C0)
#define	RAD_ADAT_SUBCODE_TXB_U2_5	(0x000010C4)

#define	RAD_ADAT_SUBCODE_TXB_U3_0	(0x000010C8)
#define	RAD_ADAT_SUBCODE_TXB_U3_1	(0x000010CC)
#define	RAD_ADAT_SUBCODE_TXB_U3_2	(0x000010D0)
#define	RAD_ADAT_SUBCODE_TXB_U3_3	(0x000010D4)
#define	RAD_ADAT_SUBCODE_TXB_U3_4	(0x000010D8)
#define	RAD_ADAT_SUBCODE_TXB_U3_5	(0x000010DC)

#define	RAD_ADAT_SUBCODE_TXB_UNUSED (0x000010E0)

/*
 * AES Subcode Tx A RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define	RAD_AES_SUBCODE_TXA_LU0		(0x00001100)
#define	RAD_AES_SUBCODE_TXA_LU1		(0x00001104)
#define	RAD_AES_SUBCODE_TXA_LU2		(0x00001108)
#define	RAD_AES_SUBCODE_TXA_LU3		(0x0000110C)
#define	RAD_AES_SUBCODE_TXA_LU4		(0x00001110)
#define	RAD_AES_SUBCODE_TXA_LU5		(0x00001114)

#define	RAD_AES_SUBCODE_TXA_LC0		(0x00001118)
#define	RAD_AES_SUBCODE_TXA_LC1		(0x0000111C)
#define	RAD_AES_SUBCODE_TXA_LC2		(0x00001120)
#define	RAD_AES_SUBCODE_TXA_LC3		(0x00001124)
#define	RAD_AES_SUBCODE_TXA_LC4		(0x00001128)
#define	RAD_AES_SUBCODE_TXA_LC5		(0x0000112C)

#define	RAD_AES_SUBCODE_TXA_LV0		(0x00001130)
#define	RAD_AES_SUBCODE_TXA_LV1		(0x00001134)
#define	RAD_AES_SUBCODE_TXA_LV2		(0x00001138)
#define	RAD_AES_SUBCODE_TXA_LV3		(0x0000113C)
#define	RAD_AES_SUBCODE_TXA_LV4		(0x00001140)
#define	RAD_AES_SUBCODE_TXA_LV5		(0x00001144)

#define	RAD_AES_SUBCODE_TXA_RU0		(0x00001148)
#define	RAD_AES_SUBCODE_TXA_RU1		(0x0000114C)
#define	RAD_AES_SUBCODE_TXA_RU2		(0x00001150)
#define	RAD_AES_SUBCODE_TXA_RU3		(0x00001154)
#define	RAD_AES_SUBCODE_TXA_RU4		(0x00001158)
#define	RAD_AES_SUBCODE_TXA_RU5		(0x0000115C)

#define	RAD_AES_SUBCODE_TXA_RC0		(0x00001160)
#define	RAD_AES_SUBCODE_TXA_RC1		(0x00001164)
#define	RAD_AES_SUBCODE_TXA_RC2		(0x00001168)
#define	RAD_AES_SUBCODE_TXA_RC3		(0x0000116C)
#define	RAD_AES_SUBCODE_TXA_RC4		(0x00001170)
#define	RAD_AES_SUBCODE_TXA_RC5		(0x00001174)

#define	RAD_AES_SUBCODE_TXA_RV0		(0x00001178)
#define	RAD_AES_SUBCODE_TXA_RV1		(0x0000117C)
#define	RAD_AES_SUBCODE_TXA_RV2		(0x00001200) /* Note break in address */
#define	RAD_AES_SUBCODE_TXA_RV3		(0x00001204)
#define	RAD_AES_SUBCODE_TXA_RV4		(0x00001208)
#define	RAD_AES_SUBCODE_TXA_RV5		(0x0000120C)

/*
 * AES Subcode Tx B RAM
 * (Note: reading only enabled during RAM test mode.)
 */

#define RAD_AES_SUBCODE_TXB_LU0		(0x00001180)
#define RAD_AES_SUBCODE_TXB_LU1     (0x00001184)
#define RAD_AES_SUBCODE_TXB_LU2     (0x00001188)
#define RAD_AES_SUBCODE_TXB_LU3     (0x0000118C)
#define RAD_AES_SUBCODE_TXB_LU4     (0x00001190)
#define RAD_AES_SUBCODE_TXB_LU5     (0x00001194)

#define RAD_AES_SUBCODE_TXB_LC0     (0x00001198)
#define RAD_AES_SUBCODE_TXB_LC1     (0x0000119C)
#define RAD_AES_SUBCODE_TXB_LC2     (0x000011A0)
#define RAD_AES_SUBCODE_TXB_LC3     (0x000011A4)
#define RAD_AES_SUBCODE_TXB_LC4     (0x000011A8)
#define RAD_AES_SUBCODE_TXB_LC5     (0x000011AC)

#define RAD_AES_SUBCODE_TXB_LV0     (0x000011B0)
#define RAD_AES_SUBCODE_TXB_LV1     (0x000011B4)
#define RAD_AES_SUBCODE_TXB_LV2     (0x000011B8)
#define RAD_AES_SUBCODE_TXB_LV3     (0x000011BC)
#define RAD_AES_SUBCODE_TXB_LV4     (0x000011C0)
#define RAD_AES_SUBCODE_TXB_LV5     (0x000011C4)

#define RAD_AES_SUBCODE_TXB_RU0     (0x000011C8)
#define RAD_AES_SUBCODE_TXB_RU1     (0x000011CC)
#define RAD_AES_SUBCODE_TXB_RU2     (0x000011D0)
#define RAD_AES_SUBCODE_TXB_RU3     (0x000011D4)
#define RAD_AES_SUBCODE_TXB_RU4     (0x000011D8)
#define RAD_AES_SUBCODE_TXB_RU5     (0x000011DC)

#define RAD_AES_SUBCODE_TXB_RC0     (0x000011E0)
#define RAD_AES_SUBCODE_TXB_RC1     (0x000011E4)
#define RAD_AES_SUBCODE_TXB_RC2     (0x000011E8)
#define RAD_AES_SUBCODE_TXB_RC3     (0x000011EC)
#define RAD_AES_SUBCODE_TXB_RC4     (0x000011F0)
#define RAD_AES_SUBCODE_TXB_RC5     (0x000011F4)

#define RAD_AES_SUBCODE_TXB_RV0     (0x000011F8)
#define RAD_AES_SUBCODE_TXB_RV1     (0x000011FC)
#define RAD_AES_SUBCODE_TXB_RV2     (0x00001210) /* Note break in address */
#define RAD_AES_SUBCODE_TXB_RV3     (0x00001214)
#define RAD_AES_SUBCODE_TXB_RV4     (0x00001218)
#define RAD_AES_SUBCODE_TXB_RV5     (0x0000121C)

#define RAD_AES_SUBCODE_TX_UNUSED   (0x00001220)

/*
 * Tx RAM
 * (Note: reading and writing only enabled during RAM test mode.)
 */

#define RAD_ADAT_TX_DATA	(0x00001400)
#define	RAD_AES_TX_DATA		(0x00001600)
#define	RAD_DTOA_DATA		(0x00001700)

