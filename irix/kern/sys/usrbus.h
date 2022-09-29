/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * definitions for ioctl to /dev/vme/
 */

#ifndef __USRBUS_H__
#define __USRBUS_H__

#define UBIOC ('u' << 16 | 'b' << 8)

#define UBIOCREGISTERULI	(UBIOC|0)	/* register ULI */

#endif /* __USRBUS_H__ */
