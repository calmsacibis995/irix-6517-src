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

#ifndef	__SYS_XTALK_HQ4_H__
#define	__SYS_XTALK_HQ4_H__

#ident "sys/xtalk/hq4.h $Revision: 1.3 $"

/*
 * Everything the kernel (outside of gfx) needs
 * to know about the HQ4 chip used as the xtalk
 * interface to the MardiGras graphics.
 */

#ifndef	HQ4_WIDGET_MFGR_NUM
#define HQ4_WIDGET_MFGR_NUM	0x2AA
#endif

#ifndef	HQ4_WIDGET_PART_NUM
#define HQ4_WIDGET_PART_NUM	0xC003
#endif

#ifndef HQ4_WIDGET_NIC
#define HQ4_WIDGET_NIC		0x011098 /* Number In A Can - Offset Addr */
#endif
#endif	/* __SYS_XTALK_HQ4_H__ */
