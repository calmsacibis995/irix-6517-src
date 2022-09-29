/*****************************************************************************
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 *****************************************************************************/

#ifndef __JPEG_DEBUG_H_
#define __JPEG_DEBUG_H_

/**
#define NO_TAG					0x0000
**/

#define FB_01_READY_FOR_UIC_READ		0x0101
#define WRITE_ADV_INTERRUPT_CC_CH		0x1000
#define WRITE_ADV_INTERRUPT_CC_CH__NO_FB_YET	0x1001
#define WRITE_ADV_INTERRUPT_CC_CH__FB_0		0x1002
#define WRITE_ADV_INTERRUPT_UCC_CH		0x1003
#define WRITE_ADV_INTERRUPT_UCC_CH__FB_0	0x1004
#define WRITE_ADV_INTERRUPT_UCC_CH__FB_2	0x1005

/***
#define NO_TAG					0x1006
#define NO_TAG					0x1007
#define NO_TAG					0x1008
**/

#define START_UCC_DMA				0x1ff
#define NEXT_RFB_AVAIL__FB23			0x222
#define JP_SET_FB_FULL__FB23			0x2323
#define START_CC_DMA				0x02ff
#define START_CL560				0x03ff
#define STOP_CL560_A				0x4ff
#define STOP_CL560_B				0x05ff
#define NEXT_WFB_AVAIL__FB_01			0xa000

#define WAKING_UCC_PATH                         0xff1
#define PROCESSED_UIC_INTERRUPT			0xff2
#define WAKING_CC_PATH				0xff3
#define PROCESSED_CC_INTERRUPT			0xff4
#define UIC_DMAChannel_NOT_Ready		0xc1c00
#define CHECK_UIC_DMAChannel_Ready		0xc1c11
#define SRCdmablock_IS_ZERO			0xcc00
#define CHECK_CC_DMAChannel_Ready		0xcc11
#define GOING_TO_SLEEP1				0xc1ccc1
#define GOING_TO_SLEEP2				0xc1ccc2
#define NEXT_WFB_AVAIL__FB_01			0xa000
#define NEXT_Wfb_AVAIL__FB_02			0xa222

/**
#define NO_TAG					0xcccc
#define NO_TAG					0xdddd
#define NO_TAG					0x0ee
#define NO_TAG					0xeee
#define NO_TAG					0x0ff
#define NO_TAG					0xfff
**/

#endif /* __JPEG_DEBUG_H_ */
