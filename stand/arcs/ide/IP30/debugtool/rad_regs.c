 /* rad_regs.c : 
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

#ident "ide/IP30/rad/rad_regs.c:  $Revision: 1.3 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h" 
#include "d_frus.h"
#include "d_prototypes.h"
#include "rad_defs.h"
#include "rad_regs.h"

/*
 * Forward References: all in d_prototypes.h 
 */
extern const short ArgLength;
extern const short ArgLengthData;

/* rad registers */
rad_Registers	gz_rad_regs_ep[] = {
{"RAD_CFG_ID",			RAD_CFG_ID,			GZ_READ_ONLY,
RAD_CFG_ID_MASK,		RAD_CFG_ID_DEFAULT},

{"RAD_CFG_STATUS",		RAD_CFG_STATUS,			GZ_READ_ONLY,
RAD_CFG_STATUS_MASK,		RAD_CFG_STATUS_DEFAULT},

{"RAD_CFG_REV",			RAD_CFG_REV,			GZ_READ_ONLY,
RAD_CFG_REV_MASK,		RAD_CFG_REV_DEFAULT},

{"RAD_CFG_LATENCY",		RAD_CFG_LATENCY,		GZ_READ_ONLY,
RAD_CFG_LATENCY_MASK,		RAD_CFG_LATENCY_DEFAULT},

{"RAD_CFG_MEMORY_BASE",		RAD_CFG_MEMORY_BASE,		GZ_READ_ONLY,
RAD_CFG_MEMORY_BASE_MASK,	RAD_CFG_MEMORY_BASE_DEFAULT},

{"RAD_PCI_STATUS",		RAD_PCI_STATUS,			GZ_READ_ONLY,
RAD_PCI_STATUS_MASK,		RAD_PCI_STATUS_DEFAULT},

{"RAD_ADAT_RX_MSC_UST",		RAD_ADAT_RX_MSC_UST,		GZ_READ_ONLY,
RAD_ADAT_RX_MSC_UST_MASK,	RAD_ADAT_RX_MSC_UST_DEFAULT},

{"RAD_ADAT_RX_MSC0_SUBMSC",	RAD_ADAT_RX_MSC0_SUBMSC,	GZ_READ_ONLY,
RAD_ADAT_RX_MSC0_SUBMSC_MASK,	RAD_ADAT_RX_MSC0_SUBMSC_DEFAULT},

{"RAD_AES_RX_MSC_UST",		RAD_AES_RX_MSC_UST,		GZ_READ_ONLY,
RAD_AES_RX_MSC_UST_MASK,	RAD_AES_RX_MSC_UST_DEFAULT},

{"RAD_AES_RX_MSC0_SUBMSC",	RAD_AES_RX_MSC0_SUBMSC,		GZ_READ_ONLY,
RAD_AES_RX_MSC0_SUBMSC_MASK,	RAD_AES_RX_MSC0_SUBMSC_DEFAULT},

{"RAD_ATOD_MSC_UST",		RAD_ATOD_MSC_UST,		GZ_READ_ONLY,
RAD_ATOD_MSC_UST_MASK,		RAD_ATOD_MSC_UST_DEFAULT},

{"RAD_ATOD_MSC0_SUBMSC",	RAD_ATOD_MSC0_SUBMSC,		GZ_READ_ONLY,
RAD_ATOD_MSC0_SUBMSC_MASK,	RAD_ATOD_MSC0_SUBMSC_DEFAULT},

{"RAD_ADAT_TX_MSC_UST",		RAD_ADAT_TX_MSC_UST,		GZ_READ_ONLY,
RAD_ADAT_TX_MSC_UST_MASK,	RAD_ADAT_TX_MSC_UST_DEFAULT},

{"RAD_ADAT_TX_MSC0_SUBMSC",	RAD_ADAT_TX_MSC0_SUBMSC,	GZ_READ_ONLY,
RAD_ADAT_TX_MSC0_SUBMSC_MASK,	RAD_ADAT_TX_MSC0_SUBMSC_DEFAULT},

{"RAD_AES_TX_MSC_UST",		RAD_AES_TX_MSC_UST,		GZ_READ_ONLY,
RAD_AES_TX_MSC_UST_MASK,	RAD_AES_TX_MSC_UST_DEFAULT},

{"RAD_AES_TX_MSC0_SUBMSC",	RAD_AES_TX_MSC0_SUBMSC,		GZ_READ_ONLY,
RAD_AES_TX_MSC0_SUBMSC_MASK,	RAD_AES_TX_MSC0_SUBMSC_DEFAULT},

{"RAD_DTOA_MSC_UST",		RAD_DTOA_MSC_UST,		GZ_READ_ONLY,
RAD_DTOA_MSC_UST_MASK,		RAD_DTOA_MSC_UST_DEFAULT},

{"RAD_UST_REGISTER",		RAD_UST_REGISTER,		GZ_READ_ONLY,
RAD_UST_REGISTER_MASK,		RAD_UST_REGISTER_DEFAULT},

{"RAD_GPIO_STATUS",		RAD_GPIO_STATUS,		GZ_READ_ONLY,
RAD_GPIO_STATUS_MASK,		RAD_GPIO_STATUS_DEFAULT},

{"RAD_CHIP_STATUS1",		RAD_CHIP_STATUS1,		GZ_READ_ONLY,
RAD_CHIP_STATUS1_MASK,		RAD_CHIP_STATUS1_DEFAULT},

{"RAD_CHIP_STATUS0",		RAD_CHIP_STATUS0,		GZ_READ_ONLY,
RAD_CHIP_STATUS0_MASK,		RAD_CHIP_STATUS0_DEFAULT},

{"RAD_UST_CLOCK_CONTROL",	RAD_UST_CLOCK_CONTROL,		GZ_READ_ONLY,
RAD_UST_CLOCK_CONTROL_MASK,	RAD_UST_CLOCK_CONTROL_DEFAULT},

{"RAD_ADAT_RX_CONTROL",		RAD_ADAT_RX_CONTROL,		GZ_READ_ONLY,
RAD_ADAT_RX_CONTROL_MASK,	RAD_ADAT_RX_CONTROL_DEFAULT},

{"RAD_AES_RX_CONTROL",		RAD_AES_RX_CONTROL,		GZ_READ_ONLY,
RAD_AES_RX_CONTROL_MASK,	RAD_AES_RX_CONTROL_DEFAULT},

{"RAD_ATOD_CONTROL",		RAD_ATOD_CONTROL,		GZ_READ_ONLY,
RAD_ATOD_CONTROL_MASK,		RAD_ATOD_CONTROL_DEFAULT},

{"RAD_ADAT_TX_CONTROL",		RAD_ADAT_TX_CONTROL,		GZ_READ_ONLY,
RAD_ADAT_TX_CONTROL_MASK,	RAD_ADAT_TX_CONTROL_DEFAULT},

{"RAD_AES_TX_CONTROL",		RAD_AES_TX_CONTROL,		GZ_READ_ONLY,
RAD_AES_TX_CONTROL_MASK,	RAD_AES_TX_CONTROL_DEFAULT},

{"RAD_DTOA_CONTROL",		RAD_DTOA_CONTROL,		GZ_READ_ONLY,
RAD_DTOA_CONTROL_MASK,		RAD_DTOA_CONTROL_DEFAULT},

{"RAD_STATUS_TIMER",		RAD_STATUS_TIMER,		GZ_READ_ONLY,
RAD_STATUS_TIMER_MASK,		RAD_STATUS_TIMER_DEFAULT},

/*read write start*/ 
{"RAD_PCI_LOADR_D0",		RAD_PCI_LOADR_D0,		GZ_READ_WRITE,
RAD_PCI_LOADR_D0_MASK,		RAD_PCI_LOADR_D0_DEFAULT},

{"RAD_PCI_HIADR_D0",		RAD_PCI_HIADR_D0,		GZ_READ_WRITE,
RAD_PCI_HIADR_D0_MASK,		RAD_PCI_HIADR_D0_DEFAULT},

{"RAD_PCI_CONTROL_D0",		RAD_PCI_CONTROL_D0,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D0_MASK,	RAD_PCI_CONTROL_D0_DEFAULT},

{"RAD_PCI_LOADR_D1",		RAD_PCI_LOADR_D1,		GZ_READ_WRITE,
RAD_PCI_LOADR_D1_MASK,		RAD_PCI_LOADR_D1_DEFAULT},

{"RAD_PCI_HIADR_D1",		RAD_PCI_HIADR_D1,		GZ_READ_WRITE,
RAD_PCI_HIADR_D1_MASK,		RAD_PCI_HIADR_D1_DEFAULT},

{"RAD_PCI_CONTROL_D1",		RAD_PCI_CONTROL_D1,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D1_MASK,	RAD_PCI_CONTROL_D1_DEFAULT},

{"RAD_PCI_LOADR_D2",		RAD_PCI_LOADR_D2,		GZ_READ_WRITE,
RAD_PCI_LOADR_D2_MASK,		RAD_PCI_LOADR_D2_DEFAULT},

{"RAD_PCI_HIADR_D2",		RAD_PCI_HIADR_D2,		GZ_READ_WRITE,
RAD_PCI_HIADR_D2_MASK,		RAD_PCI_HIADR_D2_DEFAULT},

{"RAD_PCI_CONTROL_D2",		RAD_PCI_CONTROL_D2,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D2_MASK,	RAD_PCI_CONTROL_D2_DEFAULT},

{"RAD_PCI_LOADR_D3",		RAD_PCI_LOADR_D3,		GZ_READ_WRITE,
RAD_PCI_LOADR_D3_MASK,		RAD_PCI_LOADR_D3_DEFAULT},

{"RAD_PCI_HIADR_D3",		RAD_PCI_HIADR_D3,		GZ_READ_WRITE,
RAD_PCI_HIADR_D3_MASK,		RAD_PCI_HIADR_D3_DEFAULT},

{"RAD_PCI_CONTROL_D3",		RAD_PCI_CONTROL_D3,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D3_MASK,	RAD_PCI_CONTROL_D3_DEFAULT},

{"RAD_PCI_LOADR_D4",		RAD_PCI_LOADR_D4,		GZ_READ_WRITE,
RAD_PCI_LOADR_D4_MASK,		RAD_PCI_LOADR_D4_DEFAULT},

{"RAD_PCI_HIADR_D4",		RAD_PCI_HIADR_D4,		GZ_READ_WRITE,
RAD_PCI_HIADR_D4_MASK,		RAD_PCI_HIADR_D4_DEFAULT},

{"RAD_PCI_CONTROL_D4",		RAD_PCI_CONTROL_D4,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D4_MASK,	RAD_PCI_CONTROL_D4_DEFAULT},

{"RAD_PCI_LOADR_D5",		RAD_PCI_LOADR_D5,		GZ_READ_WRITE,
RAD_PCI_LOADR_D5_MASK,		RAD_PCI_LOADR_D5_DEFAULT},

{"RAD_PCI_HIADR_D5",		RAD_PCI_HIADR_D5,		GZ_READ_WRITE,
RAD_PCI_HIADR_D5_MASK,		RAD_PCI_HIADR_D5_DEFAULT},

{"RAD_PCI_CONTROL_D5",		RAD_PCI_CONTROL_D5,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D5_MASK,	RAD_PCI_CONTROL_D5_DEFAULT},

{"RAD_PCI_LOADR_D6",		RAD_PCI_LOADR_D6,		GZ_READ_WRITE,
RAD_PCI_LOADR_D6_MASK,		RAD_PCI_LOADR_D6_DEFAULT},

{"RAD_PCI_HIADR_D6",		RAD_PCI_HIADR_D6,		GZ_READ_WRITE,
RAD_PCI_HIADR_D6_MASK,		RAD_PCI_HIADR_D6_DEFAULT},

{"RAD_PCI_CONTROL_D6",		RAD_PCI_CONTROL_D6,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D6_MASK,	RAD_PCI_CONTROL_D6_DEFAULT},

{"RAD_PCI_LOADR_D7",		RAD_PCI_LOADR_D7,		GZ_READ_WRITE,
RAD_PCI_LOADR_D7_MASK,		RAD_PCI_LOADR_D7_DEFAULT},

{"RAD_PCI_HIADR_D7",		RAD_PCI_HIADR_D7,		GZ_READ_WRITE,
RAD_PCI_HIADR_D7_MASK,		RAD_PCI_HIADR_D7_DEFAULT},

{"RAD_PCI_CONTROL_D7",		RAD_PCI_CONTROL_D7,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D7_MASK,	RAD_PCI_CONTROL_D7_DEFAULT},

{"RAD_PCI_LOADR_D8",		RAD_PCI_LOADR_D8,		GZ_READ_WRITE,
RAD_PCI_LOADR_D8_MASK,		RAD_PCI_LOADR_D8_DEFAULT},

{"RAD_PCI_HIADR_D8",		RAD_PCI_HIADR_D8,		GZ_READ_WRITE,
RAD_PCI_HIADR_D8_MASK,		RAD_PCI_HIADR_D8_DEFAULT},

{"RAD_PCI_CONTROL_D8",		RAD_PCI_CONTROL_D8,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D8_MASK,	RAD_PCI_CONTROL_D8_DEFAULT},

{"RAD_PCI_LOADR_D9",		RAD_PCI_LOADR_D9,		GZ_READ_WRITE,
RAD_PCI_LOADR_D9_MASK,		RAD_PCI_LOADR_D9_DEFAULT},

{"RAD_PCI_HIADR_D9",		RAD_PCI_HIADR_D9,		GZ_READ_WRITE,
RAD_PCI_HIADR_D9_MASK,		RAD_PCI_HIADR_D9_DEFAULT},

{"RAD_PCI_CONTROL_D9",		RAD_PCI_CONTROL_D9,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D9_MASK,	RAD_PCI_CONTROL_D9_DEFAULT},

{"RAD_PCI_LOADR_D10",		RAD_PCI_LOADR_D10,		GZ_READ_WRITE,
RAD_PCI_LOADR_D10_MASK,		RAD_PCI_LOADR_D10_DEFAULT},

{"RAD_PCI_HIADR_D10",		RAD_PCI_HIADR_D10,		GZ_READ_WRITE,
RAD_PCI_HIADR_D10_MASK,		RAD_PCI_HIADR_D10_DEFAULT},

{"RAD_PCI_CONTROL_D10",		RAD_PCI_CONTROL_D10,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D10_MASK,	RAD_PCI_CONTROL_D10_DEFAULT},

{"RAD_PCI_LOADR_D11",		RAD_PCI_LOADR_D11,		GZ_READ_WRITE,
RAD_PCI_LOADR_D11_MASK,		RAD_PCI_LOADR_D11_DEFAULT},

{"RAD_PCI_HIADR_D11",		RAD_PCI_HIADR_D11,		GZ_READ_WRITE,
RAD_PCI_HIADR_D11_MASK,		RAD_PCI_HIADR_D11_DEFAULT},

{"RAD_PCI_CONTROL_D11",		RAD_PCI_CONTROL_D11,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D11_MASK,	RAD_PCI_CONTROL_D11_DEFAULT},

{"RAD_PCI_LOADR_D12",		RAD_PCI_LOADR_D12,		GZ_READ_WRITE,
RAD_PCI_LOADR_D12_MASK,		RAD_PCI_LOADR_D12_DEFAULT},

{"RAD_PCI_HIADR_D12",		RAD_PCI_HIADR_D12,		GZ_READ_WRITE,
RAD_PCI_HIADR_D12_MASK,		RAD_PCI_HIADR_D12_DEFAULT},

{"RAD_PCI_CONTROL_D12",		RAD_PCI_CONTROL_D12,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D12_MASK,	RAD_PCI_CONTROL_D12_DEFAULT},

{"RAD_PCI_LOADR_D13",		RAD_PCI_LOADR_D13,		GZ_READ_WRITE,
RAD_PCI_LOADR_D13_MASK,		RAD_PCI_LOADR_D13_DEFAULT},

{"RAD_PCI_HIADR_D13",		RAD_PCI_HIADR_D13,		GZ_READ_WRITE,
RAD_PCI_HIADR_D13_MASK,		RAD_PCI_HIADR_D13_DEFAULT},

{"RAD_PCI_CONTROL_D13",		RAD_PCI_CONTROL_D13,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D13_MASK,	RAD_PCI_CONTROL_D13_DEFAULT},

{"RAD_PCI_LOADR_D14",		RAD_PCI_LOADR_D14,		GZ_READ_WRITE,
RAD_PCI_LOADR_D14_MASK,		RAD_PCI_LOADR_D14_DEFAULT},

{"RAD_PCI_HIADR_D14",		RAD_PCI_HIADR_D14,		GZ_READ_WRITE,
RAD_PCI_HIADR_D14_MASK,		RAD_PCI_HIADR_D14_DEFAULT},

{"RAD_PCI_CONTROL_D14",		RAD_PCI_CONTROL_D14,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D14_MASK,	RAD_PCI_CONTROL_D14_DEFAULT},

{"RAD_PCI_LOADR_D15",		RAD_PCI_LOADR_D15,		GZ_READ_WRITE,
RAD_PCI_LOADR_D15_MASK,		RAD_PCI_LOADR_D15_DEFAULT},

{"RAD_PCI_HIADR_D15",		RAD_PCI_HIADR_D15,		GZ_READ_WRITE,
RAD_PCI_HIADR_D15_MASK,		RAD_PCI_HIADR_D15_DEFAULT},

{"RAD_PCI_CONTROL_D15",		RAD_PCI_CONTROL_D15,		GZ_READ_WRITE,
RAD_PCI_CONTROL_D15_MASK,	RAD_PCI_CONTROL_D15_DEFAULT},

{"RAD_PCI_LOADR_ADAT_RX",	RAD_PCI_LOADR_ADAT_RX,		GZ_READ_WRITE,
RAD_PCI_LOADR_ADAT_RX_MASK,	RAD_PCI_LOADR_ADAT_RX_DEFAULT},

{"RAD_PCI_CONTROL_ADAT_RX",	RAD_PCI_CONTROL_ADAT_RX,	GZ_READ_WRITE,
RAD_PCI_CONTROL_ADAT_RX_MASK,	RAD_PCI_CONTROL_ADAT_RX_DEFAULT},

{"RAD_PCI_LOADR_AES_RX",	RAD_PCI_LOADR_AES_RX,		GZ_READ_WRITE,
RAD_PCI_LOADR_AES_RX_MASK,	RAD_PCI_LOADR_AES_RX_DEFAULT},

{"RAD_PCI_CONTROL_AES_RX",	RAD_PCI_CONTROL_AES_RX,		GZ_READ_WRITE,
RAD_PCI_CONTROL_AES_RX_MASK,	RAD_PCI_CONTROL_AES_RX_DEFAULT},

{"RAD_PCI_LOADR_ATOD",		RAD_PCI_LOADR_ATOD,		GZ_READ_WRITE,
RAD_PCI_LOADR_ATOD_MASK,	RAD_PCI_LOADR_ATOD_DEFAULT},

{"RAD_PCI_CONTROL_ATOD",	RAD_PCI_CONTROL_ATOD,		GZ_READ_WRITE,
RAD_PCI_CONTROL_ATOD_MASK,	RAD_PCI_CONTROL_ATOD_DEFAULT},

{"RAD_PCI_LOADR_ADATSUB_RX",	RAD_PCI_LOADR_ADATSUB_RX,	GZ_READ_WRITE,
RAD_PCI_LOADR_ADATSUB_RX_MASK,	RAD_PCI_LOADR_ADATSUB_RX_DEFAULT},

{"RAD_PCI_CONTROL_ADATSUB_RX",		RAD_PCI_CONTROL_ADATSUB_RX,	GZ_READ_WRITE,
RAD_PCI_CONTROL_ADATSUB_RX_MASK,	RAD_PCI_CONTROL_ADATSUB_RX_DEFAULT},

{"RAD_PCI_LOADR_AESSUB_RX",		RAD_PCI_LOADR_AESSUB_RX,	GZ_READ_WRITE,
RAD_PCI_LOADR_AESSUB_RX_MASK,		RAD_PCI_LOADR_AESSUB_RX_DEFAULT},

{"RAD_PCI_CONTROL_AESSUB_RX",		RAD_PCI_CONTROL_AESSUB_RX,	GZ_READ_WRITE,
RAD_PCI_CONTROL_AESSUB_RX_MASK,		RAD_PCI_CONTROL_AESSUB_RX_DEFAULT},

{"RAD_PCI_LOADR_ADAT_TX",		RAD_PCI_LOADR_ADAT_TX,		GZ_READ_WRITE,
RAD_PCI_LOADR_ADAT_TX_MASK,		RAD_PCI_LOADR_ADAT_TX_DEFAULT},

{"RAD_PCI_CONTROL_ADAT_TX",		RAD_PCI_CONTROL_ADAT_TX,	GZ_READ_WRITE,
RAD_PCI_CONTROL_ADAT_TX_MASK,		RAD_PCI_CONTROL_ADAT_TX_DEFAULT},

{"RAD_PCI_LOADR_AES_TX",		RAD_PCI_LOADR_AES_TX,		GZ_READ_WRITE,
RAD_PCI_LOADR_AES_TX_MASK,		RAD_PCI_LOADR_AES_TX_DEFAULT},

{"RAD_PCI_CONTROL_AES_TX",		RAD_PCI_CONTROL_AES_TX,		GZ_READ_WRITE,
RAD_PCI_CONTROL_AES_TX_MASK,		RAD_PCI_CONTROL_AES_TX_DEFAULT},

{"RAD_PCI_LOADR_DTOA",			RAD_PCI_LOADR_DTOA,		GZ_READ_WRITE,
RAD_PCI_LOADR_DTOA_MASK,		RAD_PCI_LOADR_DTOA_DEFAULT},

{"RAD_PCI_CONTROL_DTOA",		RAD_PCI_CONTROL_DTOA,		GZ_READ_WRITE,
RAD_PCI_CONTROL_DTOA_MASK,		RAD_PCI_CONTROL_DTOA_DEFAULT},

{"RAD_PCI_LOADR_STATUS",		RAD_PCI_LOADR_STATUS,		GZ_READ_WRITE,
RAD_PCI_LOADR_STATUS_MASK,		RAD_PCI_LOADR_STATUS_DEFAULT},

{"RAD_PCI_CONTROL_STATUS",		RAD_PCI_CONTROL_STATUS,		GZ_READ_WRITE,
RAD_PCI_CONTROL_STATUS_MASK,		RAD_PCI_CONTROL_STATUS_DEFAULT},

{"RAD_PCI_HIADR_ADAT_RX",		RAD_PCI_HIADR_ADAT_RX,		GZ_READ_WRITE,
RAD_PCI_HIADR_ADAT_RX_MASK,		RAD_PCI_HIADR_ADAT_RX_DEFAULT},

{"RAD_PCI_HIADR_AES_RX",		RAD_PCI_HIADR_AES_RX,		GZ_READ_WRITE,
RAD_PCI_HIADR_AES_RX_MASK,		RAD_PCI_HIADR_AES_RX_DEFAULT},

{"RAD_PCI_HIADR_ATOD",			RAD_PCI_HIADR_ATOD,		GZ_READ_WRITE,
RAD_PCI_HIADR_ATOD_MASK,		RAD_PCI_HIADR_ATOD_DEFAULT},

{"RAD_PCI_HIADR_ADATSUB_RX",		RAD_PCI_HIADR_ADATSUB_RX,	GZ_READ_WRITE,
RAD_PCI_HIADR_ADATSUB_RX_MASK,		RAD_PCI_HIADR_ADATSUB_RX_DEFAULT},

{"RAD_PCI_HIADR_AESSUB_RX",		RAD_PCI_HIADR_AESSUB_RX,	GZ_READ_WRITE,
RAD_PCI_HIADR_AESSUB_RX_MASK,		RAD_PCI_HIADR_AESSUB_RX_DEFAULT},

{"RAD_PCI_HIADR_ADAT_TX",		RAD_PCI_HIADR_ADAT_TX,		GZ_READ_WRITE,
RAD_PCI_HIADR_ADAT_TX_MASK,		RAD_PCI_HIADR_ADAT_TX_DEFAULT},

{"RAD_PCI_HIADR_AES_TX",		RAD_PCI_HIADR_AES_TX,		GZ_READ_WRITE,
RAD_PCI_HIADR_AES_TX_MASK,		RAD_PCI_HIADR_AES_TX_DEFAULT},

{"RAD_PCI_HIADR_DTOA",			RAD_PCI_HIADR_DTOA,		GZ_READ_WRITE,
RAD_PCI_HIADR_DTOA_MASK,		RAD_PCI_HIADR_DTOA_DEFAULT},

{"RAD_PCI_HIADR_STATUS",		RAD_PCI_HIADR_STATUS,		GZ_READ_WRITE,
RAD_PCI_HIADR_STATUS_MASK,		RAD_PCI_HIADR_STATUS_DEFAULT},

{"", -1, -1, -1 }
};



/*
 * Name:	_rad_regs_write.c
 * Description:	Performs Write/Reads on the rad Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_rad_regs_write(rad_Registers *regs_ptr, __uint32_t data)
{					
    msg_printf(DBG, "Write location (32) = 0x%x\n", regs_ptr->address);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    /* Write the test value */
    PIO_REG_WR_32(regs_ptr->address, regs_ptr->mask, data);

    return(0);
}

/*
 * Name:	_rad_regs_read
 * Description:	Performs Reads on the rad Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */

bool_t
_rad_regs_read(rad_Registers *regs_ptr)
{
    radregisters_t	saved_reg_val;
					
    /* Save the current value */
    PIO_REG_RD_32(regs_ptr->address, regs_ptr->mask, saved_reg_val);
    msg_printf(INFO,"Read Addr (32) 0x%x = 0x%x\n", regs_ptr->address, saved_reg_val);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    return(0);
}

/*
 * Name:	rad_regs.c
 * Description:	tests registers in the rad
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_rad_poke -l 1 -d 0x0 #exception
 * ip30_rad_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_rad_poke(__uint32_t argc, char **argv)
{
	rad_Registers	*regs_ptr = gz_rad_regs_ep;
	int data;
   	short bad_arg = 0; 
        int loop;
	short i;
	char *errStr;
	bool_t dataArgDefined = 0;

	loop = 1;
	data = 0xFF;

   	/* get the args */
   	argc--; argv++;

   	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {    
			case 'd':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex data argument required\n");
					msg_printf(INFO, "Usage: ip30_rad_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_rad_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_rad_poke -l<loopcount> -d <data>\n");
						return 1;
					}
				} 
				else {
		    			atob(&argv[0][2], &(data));
	          		}
				 	dataArgDefined = 1;
			break;
			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_rad_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_rad_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_rad_poke -l<loopcount> -d <data>\n");
						return 1;
					}
	          		} else {
		    			atob(&argv[0][2], &(loop));
	         		}
			break;
			default: 
               			bad_arg++; break;
		}
	    argc--; argv++;
	}

	if (!dataArgDefined) {
		msg_printf(INFO,"Arg -d is required\n");
		msg_printf(INFO, "Usage: ip30_rad_poke -l<loopcount> -d <data>\n");
		return 1;
	}
	
	for (i = 1;i<=loop;i++) {

	   /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "Rad Register Write\n");
	   while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _rad_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_rad_regs_ep;
		msg_printf(INFO, "\n");
	}
	   
	return 0;
}

/*
 * ip30_rad_peek -l 1
*/

bool_t
ip30_rad_peek(__uint32_t argc, char **argv)
{
	rad_Registers	*regs_ptr = gz_rad_regs_ep;
	short bad_arg = 0;
        int loop;
	short i;
	char *errStr;

	loop = 1;

	/* give register info if it was not given in default checking */
    	msg_printf(INFO, "Rad Register Read\n");

	/* get the args */
   	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_rad_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_rad_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_rad_peek -l<loopcount>\n");
						return 1;
					}
	          		} else {
		    			atob(&argv[0][2], &(loop));
	         		}
			break;
	  		default: 
            			 bad_arg++; break;
	    	}
	    		argc--; argv++;
	}
	
	for (i = 1;i<=loop;i++) {
	   while (regs_ptr->name[0] != NULL) {
		_rad_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_rad_regs_ep;
		msg_printf(INFO, "\n");
	}

	return 0;
}

/*rad registers require direct write to address*/
void rad_ind_peek(int offset, int regAddr32, int loop) {

   rad_Registers *rad_regs_ptr = gz_rad_regs_ep;
   __uint32_t mask;
   short i;
   radregisters_t	readValue;
   bool_t found = 0;

   msg_printf(DBG,"Inside rad_ind_peek\n");

   regAddr32 = offset;
   while (  (!found) && (rad_regs_ptr->address != -1)  ) {
      if (rad_regs_ptr->address == regAddr32) found = 1;
      rad_regs_ptr++;
   }
   if (found) rad_regs_ptr--;
   mask = rad_regs_ptr->mask;
   msg_printf(DBG,"rad_regs_ptr->address = %x;\n", rad_regs_ptr->address);
   msg_printf(DBG,"rad_regs_ptr->mode = %x;\n", rad_regs_ptr->mode);
   msg_printf(DBG,"rad_regs_ptr->mask = %x;\n", rad_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (   (rad_regs_ptr->mode == GZ_READ_ONLY) || (rad_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_RD_32(regAddr32, mask, readValue);
      msg_printf(INFO,"Read Back Value (32) = 0x%x\n", readValue);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}

void rad_ind_poke(int offset, int regAddr32, int loop, __uint32_t data) {

   rad_Registers *rad_regs_ptr = gz_rad_regs_ep;
   __uint32_t mask;
   short i;
   radregisters_t	readValue;
   bool_t found = 0;

  msg_printf(DBG,"Inside rad_ind_poke\n");

   regAddr32 = offset;
   while (  (!found) && (rad_regs_ptr->address != -1)  ) {
      if (rad_regs_ptr->address == regAddr32) found = 1;
      rad_regs_ptr++;
   }
   if (found) rad_regs_ptr--;
   mask = rad_regs_ptr->mask;
   msg_printf(DBG,"rad_regs_ptr->address = %x;\n", rad_regs_ptr->address);
   msg_printf(DBG,"rad_regs_ptr->mode = %x;\n", rad_regs_ptr->mode);
   msg_printf(DBG,"rad_regs_ptr->mask = %x;\n", rad_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (rad_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_WR_32(regAddr32, mask, data);
      msg_printf(INFO,"Register write address = 0x%x\n", regAddr32);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}
