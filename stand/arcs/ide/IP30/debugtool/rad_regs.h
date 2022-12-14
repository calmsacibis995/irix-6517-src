#ifndef __IDE_rad_H__
#define	__IDE_rad_H__

/*
 * d_rad.h
 *
 * IDE rad tests header 
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident	"ide/godzilla/include/d_rad.h:  $Revision: 1.1 $"

#include "d_ioregs.h"

typedef __uint32_t radregisters_t;

/* 
 * macro definitions: yes, the BR_xxxx_32 Reads and Writes are really 32 bits...
 * 	register access macros depend bridge_wid_no to be set to
 *	the correct xbow port id. Default is XBOW_PORT_F
 *	PIO_REG_WR_32(address, mask, value);
 *	PIO_REG_RD_32(address, mask, value);
 */	
                                         
/*
 * constants used in rad_regs.c
 */
#define	RAD_REGS_PATTERN_MAX	6

#define RAD_CFG_ID_MASK				0xFFFFFFFF
#define RAD_CFG_ID_DEFAULT			0x0

#define RAD_CFG_STATUS_MASK			0xFFFFFFFF
#define RAD_CFG_STATUS_DEFAULT			0x0

#define RAD_CFG_REV_MASK			0xFFFFFFFF
#define RAD_CFG_REV_DEFAULT			0x0

#define RAD_CFG_LATENCY_MASK			0xFFFFFFFF
#define RAD_CFG_LATENCY_DEFAULT			0x0

#define RAD_CFG_MEMORY_BASE_MASK		0xFFFFFFFF
#define RAD_CFG_MEMORY_BASE_DEFAULT		0x0

#define RAD_PCI_STATUS_MASK			0xFFFFFFFF
#define RAD_PCI_STATUS_DEFAULT			0x0

#define RAD_ADAT_RX_MSC_UST_MASK		0xFFFFFFFF
#define RAD_ADAT_RX_MSC_UST_DEFAULT		0x0

#define RAD_ADAT_RX_MSC0_SUBMSC_MASK		0xFFFFFFFF
#define RAD_ADAT_RX_MSC0_SUBMSC_DEFAULT		0x0

#define RAD_AES_RX_MSC_UST_MASK			0xFFFFFFFF
#define RAD_AES_RX_MSC_UST_DEFAULT		0x0

#define RAD_AES_RX_MSC0_SUBMSC_MASK		0xFFFFFFFF
#define RAD_AES_RX_MSC0_SUBMSC_DEFAULT		0x0

#define RAD_ATOD_MSC_UST_MASK			0xFFFFFFFF
#define RAD_ATOD_MSC_UST_DEFAULT		0x0

#define RAD_ATOD_MSC0_SUBMSC_MASK		0xFFFFFFFF
#define RAD_ATOD_MSC0_SUBMSC_DEFAULT		0x0

#define RAD_ADAT_TX_MSC_UST_MASK		0xFFFFFFFF
#define RAD_ADAT_TX_MSC_UST_DEFAULT		0x0

#define RAD_ADAT_TX_MSC0_SUBMSC_MASK		0xFFFFFFFF
#define RAD_ADAT_TX_MSC0_SUBMSC_DEFAULT		0x0

#define RAD_AES_TX_MSC_UST_MASK			0xFFFFFFFF
#define RAD_AES_TX_MSC_UST_DEFAULT		0x0

#define RAD_AES_TX_MSC0_SUBMSC_MASK		0xFFFFFFFF
#define RAD_AES_TX_MSC0_SUBMSC_DEFAULT		0x0

#define RAD_DTOA_MSC_UST_MASK			0xFFFFFFFF
#define RAD_DTOA_MSC_UST_DEFAULT		0x0

#define RAD_UST_REGISTER_MASK			0xFFFFFFFF
#define RAD_UST_REGISTER_DEFAULT		0x0

#define RAD_GPIO_STATUS_MASK			0xFFFFFFFF
#define RAD_GPIO_STATUS_DEFAULT			0x0

#define RAD_CHIP_STATUS1_MASK			0xFFFFFFFF
#define RAD_CHIP_STATUS1_DEFAULT		0x0

#define RAD_CHIP_STATUS0_MASK			0xFFFFFFFF
#define RAD_CHIP_STATUS0_DEFAULT		0x0

#define RAD_UST_CLOCK_CONTROL_MASK		0xFFFFFFFF
#define RAD_UST_CLOCK_CONTROL_DEFAULT		0x0

#define RAD_ADAT_RX_CONTROL_MASK		0xFFFFFFFF
#define RAD_ADAT_RX_CONTROL_DEFAULT		0x0

#define RAD_AES_RX_CONTROL_MASK			0xFFFFFFFF
#define RAD_AES_RX_CONTROL_DEFAULT		0x0

#define RAD_ATOD_CONTROL_MASK			0xFFFFFFFF
#define RAD_ATOD_CONTROL_DEFAULT		0x0

#define RAD_ADAT_TX_CONTROL_MASK		0xFFFFFFFF
#define RAD_ADAT_TX_CONTROL_DEFAULT		0x0

#define RAD_AES_TX_CONTROL_MASK			0xFFFFFFFF
#define RAD_AES_TX_CONTROL_DEFAULT		0x0

#define RAD_DTOA_CONTROL_MASK			0xFFFFFFFF
#define RAD_DTOA_CONTROL_DEFAULT		0x0

#define RAD_STATUS_TIMER_MASK			0xFFFFFFFF
#define RAD_STATUS_TIMER_DEFAULT		0x0

#define RAD_MISC_CONTROL_MASK			0xFFFFFFFF
#define RAD_MISC_CONTROL_DEFAULT		0x0

#define RAD_PCI_HOLDOFF_MASK			0xFFFFFFFF
#define RAD_PCI_HOLDOFF_DEFAULT			0x0

#define RAD_PCI_ARB_CONTROL_MASK		0xFFFFFFFF
#define RAD_PCI_ARB_CONTROL_DEFAULT		0x0

#define RAD_VOLUME_CONTROL_MASK			0xFFFFFFFF
#define RAD_VOLUME_CONTROL_DEFAULT		0x0

#define RAD_RESET_MASK				0xFFFFFFFF
#define RAD_RESET_DEFAULT			0x0

#define RAD_GPIO0_MASK				0xFFFFFFFF
#define RAD_GPIO0_DEFAULT			0x0

#define RAD_GPIO1_MASK				0xFFFFFFFF
#define RAD_GPIO1_DEFAULT			0x0

#define RAD_GPIO2_MASK				0xFFFFFFFF
#define RAD_GPIO2_DEFAULT			0x0

#define RAD_GPIO3_MASK				0xFFFFFFFF
#define RAD_GPIO3_DEFAULT			0x0

#define RAD_CLOCKGEN_ICTL_MASK			0xFFFFFFFF
#define RAD_CLOCKGEN_ICTL_DEFAULT		0x0

#define RAD_CLOCKGEN_REM_MASK			0xFFFFFFFF
#define RAD_CLOCKGEN_REM_DEFAULT		0x0

#define RAD_FREQ_SYNTH3_MUX_SEL_MASK		0xFFFFFFFF
#define RAD_FREQ_SYNTH3_MUX_SEL_DEFAULT		0x0

#define RAD_FREQ_SYNTH2_MUX_SEL_MASK		0xFFFFFFFF
#define RAD_FREQ_SYNTH2_MUX_SEL_DEFAULT		0x0

#define RAD_FREQ_SYNTH1_MUX_SEL_MASK		0xFFFFFFFF
#define RAD_FREQ_SYNTH1_MUX_SEL_DEFAULT		0x0

#define RAD_FREQ_SYNTH0_MUX_SEL_MASK		0xFFFFFFFF
#define RAD_FREQ_SYNTH0_MUX_SEL_DEFAULT		0x0

#define RAD_MPLL0_LOCK_CONTROL_MASK		0xFFFFFFFF
#define RAD_MPLL0_LOCK_CONTROL_DEFAULT		0x0

#define RAD_MPLL1_LOCK_CONTROL_MASK		0xFFFFFFFF
#define RAD_MPLL1_LOCK_CONTROL_DEFAULT		0x0

#define RAD_PCI_LOADR_D0_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D0_DEFAULT		0x0

#define RAD_PCI_HIADR_D0_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D0_DEFAULT		0x0

#define RAD_PCI_CONTROL_D0_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D0_DEFAULT		0x0

#define RAD_PCI_LOADR_D1_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D1_DEFAULT		0x0

#define RAD_PCI_HIADR_D1_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D1_DEFAULT		0x0

#define RAD_PCI_CONTROL_D1_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D1_DEFAULT		0x0

#define RAD_PCI_LOADR_D2_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D2_DEFAULT		0x0

#define RAD_PCI_HIADR_D2_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D2_DEFAULT		0x0

#define RAD_PCI_CONTROL_D2_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D2_DEFAULT		0x0

#define RAD_PCI_LOADR_D3_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D3_DEFAULT		0x0

#define RAD_PCI_HIADR_D3_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D3_DEFAULT		0x0

#define RAD_PCI_CONTROL_D3_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D3_DEFAULT		0x0

#define RAD_PCI_LOADR_D4_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D4_DEFAULT		0x0

#define RAD_PCI_HIADR_D4_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D4_DEFAULT		0x0

#define RAD_PCI_CONTROL_D4_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D4_DEFAULT		0x0

#define RAD_PCI_LOADR_D5_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D5_DEFAULT		0x0

#define RAD_PCI_HIADR_D5_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D5_DEFAULT		0x0

#define RAD_PCI_CONTROL_D5_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D5_DEFAULT		0x0

#define RAD_PCI_LOADR_D6_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D6_DEFAULT		0x0

#define RAD_PCI_HIADR_D6_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D6_DEFAULT		0x0

#define RAD_PCI_CONTROL_D6_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D6_DEFAULT		0x0

#define RAD_PCI_LOADR_D7_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D7_DEFAULT		0x0

#define RAD_PCI_HIADR_D7_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D7_DEFAULT		0x0

#define RAD_PCI_CONTROL_D7_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D7_DEFAULT		0x0

#define RAD_PCI_LOADR_D8_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D8_DEFAULT		0x0

#define RAD_PCI_HIADR_D8_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D8_DEFAULT		0x0

#define RAD_PCI_CONTROL_D8_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D8_DEFAULT		0x0

#define RAD_PCI_LOADR_D9_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D9_DEFAULT		0x0

#define RAD_PCI_HIADR_D9_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D9_DEFAULT		0x0

#define RAD_PCI_CONTROL_D9_MASK			0xFFFFFFFF
#define RAD_PCI_CONTROL_D9_DEFAULT		0x0

#define RAD_PCI_LOADR_D10_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D10_DEFAULT		0x0

#define RAD_PCI_HIADR_D10_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D10_DEFAULT		0x0

#define RAD_PCI_CONTROL_D10_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_D10_DEFAULT		0x0

#define RAD_PCI_LOADR_D11_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D11_DEFAULT		0x0

#define RAD_PCI_HIADR_D11_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D11_DEFAULT		0x0

#define RAD_PCI_CONTROL_D11_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_D11_DEFAULT		0x0

#define RAD_PCI_LOADR_D12_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D12_DEFAULT		0x0

#define RAD_PCI_HIADR_D12_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D12_DEFAULT		0x0

#define RAD_PCI_CONTROL_D12_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_D12_DEFAULT		0x0

#define RAD_PCI_LOADR_D13_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D13_DEFAULT		0x0

#define RAD_PCI_HIADR_D13_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D13_DEFAULT		0x0

#define RAD_PCI_CONTROL_D13_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_D13_DEFAULT		0x0

#define RAD_PCI_LOADR_D14_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D14_DEFAULT		0x0

#define RAD_PCI_HIADR_D14_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D14_DEFAULT		0x0

#define RAD_PCI_CONTROL_D14_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_D14_DEFAULT		0x0

#define RAD_PCI_LOADR_D15_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_D15_DEFAULT		0x0

#define RAD_PCI_HIADR_D15_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_D15_DEFAULT		0x0

#define RAD_PCI_CONTROL_D15_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_D15_DEFAULT		0x0

#define RAD_PCI_LOADR_ADAT_RX_MASK		0xFFFFFFFF
#define RAD_PCI_LOADR_ADAT_RX_DEFAULT		0x0

#define RAD_PCI_CONTROL_ADAT_RX_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_ADAT_RX_DEFAULT		0x0

#define RAD_PCI_LOADR_AES_RX_MASK		0xFFFFFFFF
#define RAD_PCI_LOADR_AES_RX_DEFAULT		0x0

#define RAD_PCI_CONTROL_AES_RX_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_AES_RX_DEFAULT		0x0

#define RAD_PCI_LOADR_ATOD_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_ATOD_DEFAULT		0x0

#define RAD_PCI_CONTROL_ATOD_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_ATOD_DEFAULT		0x0

#define RAD_PCI_LOADR_ADATSUB_RX_MASK		0xFFFFFFFF
#define RAD_PCI_LOADR_ADATSUB_RX_DEFAULT		0x0

#define RAD_PCI_CONTROL_ADATSUB_RX_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_ADATSUB_RX_DEFAULT		0x0

#define RAD_PCI_LOADR_AESSUB_RX_MASK		0xFFFFFFFF
#define RAD_PCI_LOADR_AESSUB_RX_DEFAULT		0x0

#define RAD_PCI_CONTROL_AESSUB_RX_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_AESSUB_RX_DEFAULT		0x0

#define RAD_PCI_LOADR_ADAT_TX_MASK		0xFFFFFFFF
#define RAD_PCI_LOADR_ADAT_TX_DEFAULT		0x0

#define RAD_PCI_CONTROL_ADAT_TX_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_ADAT_TX_DEFAULT		0x0

#define RAD_PCI_LOADR_AES_TX_MASK		0xFFFFFFFF
#define RAD_PCI_LOADR_AES_TX_DEFAULT		0x0

#define RAD_PCI_CONTROL_AES_TX_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_AES_TX_DEFAULT		0x0

#define RAD_PCI_LOADR_DTOA_MASK			0xFFFFFFFF
#define RAD_PCI_LOADR_DTOA_DEFAULT		0x0

#define RAD_PCI_CONTROL_DTOA_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_DTOA_DEFAULT		0x0

#define RAD_PCI_LOADR_STATUS_MASK		0xFFFFFFFF
#define RAD_PCI_LOADR_STATUS_DEFAULT		0x0

#define RAD_PCI_CONTROL_STATUS_MASK		0xFFFFFFFF
#define RAD_PCI_CONTROL_STATUS_DEFAULT		0x0

#define RAD_PCI_HIADR_ADAT_RX_MASK		0xFFFFFFFF
#define RAD_PCI_HIADR_ADAT_RX_DEFAULT		0x0

#define RAD_PCI_HIADR_AES_RX_MASK		0xFFFFFFFF
#define RAD_PCI_HIADR_AES_RX_DEFAULT		0x0

#define RAD_PCI_HIADR_ATOD_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_ATOD_DEFAULT		0x0

#define RAD_PCI_HIADR_ADATSUB_RX_MASK		0xFFFFFFFF
#define RAD_PCI_HIADR_ADATSUB_RX_DEFAULT		0x0

#define RAD_PCI_HIADR_AESSUB_RX_MASK		0xFFFFFFFF
#define RAD_PCI_HIADR_AESSUB_RX_DEFAULT		0x0

#define RAD_PCI_HIADR_ADAT_TX_MASK		0xFFFFFFFF
#define RAD_PCI_HIADR_ADAT_TX_DEFAULT		0x0

#define RAD_PCI_HIADR_AES_TX_MASK		0xFFFFFFFF
#define RAD_PCI_HIADR_AES_TX_DEFAULT		0x0

#define RAD_PCI_HIADR_DTOA_MASK			0xFFFFFFFF
#define RAD_PCI_HIADR_DTOA_DEFAULT		0x0

#define RAD_PCI_HIADR_STATUS_MASK		0xFFFFFFFF
#define RAD_PCI_HIADR_STATUS_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U0_0_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U0_0_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U0_1_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U0_1_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U0_2_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U0_2_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U0_3_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U0_3_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U0_4_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U0_4_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U0_5_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U0_5_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U1_0_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U1_0_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U1_1_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U1_1_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U1_2_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U1_2_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U1_3_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U1_3_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U1_4_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U1_4_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U1_5_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U1_5_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U2_0_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U2_0_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U2_1_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U2_1_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U2_2_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U2_2_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U2_3_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U2_3_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U2_4_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U2_4_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U2_5_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U2_5_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U3_0_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U3_0_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U3_1_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U3_1_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U3_2_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U3_2_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U3_3_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U3_3_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U3_4_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U3_4_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXA_U3_5_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXA_U3_5_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_TXB_U0_0_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_TXB_U0_0_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_RXA_U0_0_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_RXA_U0_0_DEFAULT		0x0

#define RAD_ADAT_SUBCODE_RXB_U0_0_MASK		0xFFFFFFFF
#define RAD_ADAT_SUBCODE_RXB_U0_0_DEFAULT		0x0

#define RAD_AES_SUBCODE_TXA_LU0_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_TXA_LU0_DEFAULT		0x0

#define RAD_AES_SUBCODE_TXA_RV2_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_TXA_RV2_DEFAULT		0x0

#define RAD_AES_SUBCODE_TXB_LU0_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_TXB_LU0_DEFAULT		0x0

#define RAD_AES_SUBCODE_TXB_RV2_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_TXB_RV2_DEFAULT		0x0

#define RAD_AES_SUBCODE_RXA_LU0_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_RXA_LU0_DEFAULT		0x0

#define RAD_AES_SUBCODE_RXA_RV2_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_RXA_RV2_DEFAULT		0x0

#define RAD_AES_SUBCODE_RXB_LU0_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_RXB_LU0_DEFAULT		0x0

#define RAD_AES_SUBCODE_RXB_RV2_MASK		0xFFFFFFFF
#define RAD_AES_SUBCODE_RXB_RV2_DEFAULT		0x0

#define RAD_ADAT_TX_DATA_MASK			0xFFFFFFFF
#define RAD_ADAT_TX_DATA_DEFAULT		0x0

#define RAD_AES_TX_DATA_MASK			0xFFFFFFFF
#define RAD_AES_TX_DATA_DEFAULT			0x0

#define RAD_DTOA_DATA_MASK			0xFFFFFFFF
#define RAD_DTOA_DATA_DEFAULT			0x0

#define RAD_ADAT_RX_DATA_MASK			0xFFFFFFFF
#define RAD_ADAT_RX_DATA_DEFAULT		0x0

#define RAD_AES_RX_DATA_MASK			0xFFFFFFFF
#define RAD_AES_RX_DATA_DEFAULT			0x0

#define RAD_ATOD_DATA_MASK			0xFFFFFFFF
#define RAD_ATOD_DATA_DEFAULT			0x0

typedef struct	_rad_Registers {
	char		*name;  	/* name of the register */
	__uint32_t	address;	/* address */
	__uint32_t	mode;	    	/* read / write only or read & write */
	__uint32_t	mask;		/* read write mask*/
	__uint32_t	def;	    	/* default value */
} rad_Registers;

#endif	/* __IDE_rad_H__ */

