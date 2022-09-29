/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	__SYS_XTALK_XG_H__
#define	__SYS_XTALK_XG_H__

#ident "sys/xtalk/hq4.h $Revision: 1.1 $"

/*
 * Everything the kernel (outside of gfx) needs
 * to know about the XG chip used as the xtalk
 * interface to the Kona (HILO/HILOLITE) graphics.
 */

#ifndef	XG_WIDGET_MFGR_NUM
#define XG_WIDGET_MFGR_NUM	0x2AA
#endif

#ifndef	XG_WIDGET_PART_NUM
#define XG_WIDGET_PART_NUM	0xC102		/* KONA/xt_regs.h     XG_XT_PART_NUM_VALUE */
#endif

#ifndef	XG_WIDGET_NIC
#define XG_WIDGET_NIC		0x000098        /* Number In A Can - Offset Addr */
#endif

#endif	/* __SYS_XTALK_XG_H__ */
