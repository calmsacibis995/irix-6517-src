/*****************************************************************************
 * $Id: mace_regs.h,v 1.1 1996/01/18 17:27:16 montep Exp $
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

/*
 * Moosehead MACE Registers
 *
 */

#ifdef __cplusplus
/*extern "C" {*/
#endif

#ifndef	__MACE_REG_T
#define	__MACE_REG_T
typedef unsigned long long reg_t;
#endif	/* __MACE_REG_T */

#include "mace_regs_audio.h"
#include "mace_regs_crime.h"
#include "mace_regs_ether.h"
#include "mace_regs_iic.h"
#include "mace_regs_isa.h"
#include "mace_regs_keyboard.h"
#include "mace_regs_pci.h"
#include "mace_regs_serial.h"
#include "mace_regs_timers.h"
#include "mace_regs_video.h"

#define	MACE_OFFSET		0x1f000000
#define	MACE_OFFSET_CRIME	0x000000

#define	MACE_OFFSET_PCI_CONTROL	0x080000
#define	MACE_OFFSET_PCI_CONFIG	0x084000

#define	MACE_OFFSET_VIDEO_IN1	0x100000
#define	MACE_OFFSET_VIDEO_IN2	0x180000
#define	MACE_OFFSET_VIDEO_OUT	0x200000

#define	MACE_OFFSET_ETHER	0x280000
#define	MACE_OFFSET_ETHER_FIFO	0x280100

#define	MACE_OFFSET_AUDIO	0x300000
#define	MACE_OFFSET_ISA_DMA	0x310000
#define	MACE_OFFSET_KEYBOARD	0x320000
#define	MACE_OFFSET_IIC		0x330000
#define	MACE_OFFSET_TIMERS	0x340000

#define	MACE_OFFSET_COMPARE1	0x350000
#define	MACE_OFFSET_COMPARE2	0x360000
#define	MACE_OFFSET_COMPARE3	0x370000

#define	MACE_OFFSET_ISA		0x380000
#define	MACE_OFFSET_PAR		0x384000
#define	MACE_OFFSET_SER1	0x388000
#define	MACE_OFFSET_SER2	0x38c000

#define	MACE_OFFSET_PROM	0xC00000

#ifdef __cplusplus
/*}*/
#endif

/* === */
