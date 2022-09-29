/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_XBOW_H__
#define __SYS_SN_XBOW_H__

/*
 * This file contains SN0-specific xbow definitions.  Common definitions
 * shared with SpeedRacer are in sys/xtalk/xbow.h.  Definitions in this
 * file are either node-specific or are dependent of SN0's timing.
 */
#include <sys/xtalk/xbow.h>

/* xbow base address */
#define XBOW_SN0_BASE(pnode) 	NODE_SWIN_BASE(pnode, 0)
 
/* pointer to link registers base, given node and destination widget id */
#define XBOW_LINKREGS_PTR(pnode, dst_wid) (xb_linkregs_t*) \
        XBOW_REG_PTR(XBOW_SN0_BASE(pnode), XB_LINK_REG_BASE(dst_wid))

/* pointer to link arbitration register, given node, dst and src widget id */
#define XBOW_ARBREG_PTR(pnode, dst_wid, src_wid) \
        XBOW_REG_PTR(XBOW_LINKREGS_PTR(pnode, dst_wid), XBOW_ARB_OFF(src_wid))

#define XBOW_LLP_BURST_MASK	0x3ff
#define XBOW_LLP_BURST_SHFT	0
#define XBOW_LLP_NULL_TOUT_MASK	0x3f
#define XBOW_LLP_NULL_TOUT_SHFT	10
#define XBOW_LLP_MAX_RETRY_MASK	0x3ff
#define XBOW_LLP_MAX_RETRY_SHFT 16

/* LLP configuration default control */
#define XBOW_LLP_CFG_NULL_TOUT	0x6
#define XBOW_LLP_CFG_RETRY	0x3ff
#define XBOW_LLP_CFG_MAX_BURST	0x10

#define XBOW_LLP_CFG 		(XBOW_LLP_CFG_MAX_BURST | \
	XBOW_LLP_CFG_RETRY << XBOW_LLP_MAX_RETRY_SHFT | \
        XBOW_LLP_CFG_NULL_TOUT << XBOW_LLP_NULL_TOUT_SHFT)

/* packet time out (in xbow's input buffer), 1.28 us per tick */
#define XBOW_PKT_TIMEOUT_TICKS	16384	/* about 20 ms */

/* 
 * Arbitration reload register, 1.28 us per tick 
 *	granuality = 128 B / (tick * 1.28 us) = 100/tick MB/s
 *	max = (2^5 - 1) * granuality = 3100/tick MB/s
 */
#define XBOW_ARB_RELOAD_TICKS	25	/* granuality: 4 MB/s, max: 124 MB/s */
#define XBOW_MB_TO_GBR(MB_per_s) 	/* convert MB/s to GBR value */ \
	(MB_per_s * XBOW_ARB_RELOAD_TICKS / 100 - 1 & XB_ARB_GBR_MSK)
#define XBOW_GBR_TO_MB(cnt) \
	(((cnt) + 1) * 100 / XBOW_ARB_RELOAD_TICKS)
	
/* LLP widget credit */
#define XBOW_LLP_CREDIT		8	/* XXX change me */

/* 
 * Default arbitration counter values
 */
#define XBOW_ARB_GBR_DEFAULT 	0	/* 1 GBR packet per time interval */
#define XBOW_ARB_RR_DEFAULT	1	/* 2 packets in a RR brust */
#define XBOW_ARB_DEFAULT		/* 4 links combines */ \
	(XBOW_ARB_GBR_DEFAULT * (1 | 1 << 8 | 1 << 16 | 1 << 24) + \
	XBOW_ARB_RR_DEFAULT * (1 << 5 | 1 << 13 | 1 << 21 | 1 << 29))

#endif /* __SYS_SN_XBOW_H__ */
