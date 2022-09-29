/*
 * Copyright 1998, Silicon Graphics, Inc. 
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
 */
#ifndef _sys_tigon_h
#define _sys_tigon_h

/* $Revision: 1.3 $ */

/*	
	These are the values that need to be defined for SGI.
	Some of these are from various Alteon supplied files
	and are noted as such. The included files are the Alteon
	suppllied files. The names are changed from nic_xxxx to
	tigon_xxx.
*/

#define	PTRS_ARE_64 		1
#define VOLATILE 		volatile
#define U8 			unsigned char
#define U16 			unsigned short
#define U32			__uint32_t
#define U64 			__uint64_t

#define HOSTADDR(x) 		x
#define HOSTADDR_TYPE 		U64
#define HOSTZEROHI(x)

#define	ALT_BIG_ENDIAN		1
#define TG_VENDOR      		0x10a9          /* Silicon Graphics, Inc 		*/
#define TG_DEVICE       	9               /* Tigon1 chip 				*/


/*
	These are from the Alteon file alttypes.h
*/
#define	SEND_RING_PTR		0x3800		/* from file alttypes.h			*/
#define	SEND_RING_ENTRIES	128		/* from file alttypes.h			*/


/*
	These are from the Alteon file altftns.h
*/
#define	ALT_TRACE_SIZE		1024		/* from altftns.h			*/


/*
	These are from the Alteon file altreg.h
*/
#define	WINDOW_LEN		(2*1024)
#define	WINDOW_ALIGNED(addr)	(((int)(addr) & (WINDOW_LEN-1)) == 0)
#define	WINDOW_RESID(addr)	(WINDOW_LEN - ((int)(addr) & (WINDOW_LEN-1)))
#define	WINDOW_OFFSET(addr)	((int)(addr) & (WINDOW_LEN-1))
#define	WINDOW_PAGE(addr)	((int)(addr) & ~(WINDOW_LEN-1))


/*
	This is modified from the Alteon file altdrv.h
	Originally called DO_TG_COMMAND but we enqueue differently
	so all we take is the formatting stuff
*/
#define MAKE_TG_COMMAND(cmd, index, code)	((cmd << TG_COMMAND_CMD_SHIFT)  |	\
                                           	(code << TG_COMMAND_CODE_SHIFT) |	\
                                           	(index))


/*
	Unchanged Alteon files
*/

#include <sys/tigon_rev.h>			/* Alteon name is tg_rev.h		*/

#define	TIGON_REV	TIGON_REV5
#define	HW_NIC		TIGON_REV

#include <sys/tigon_conf.h>			/* Alteon name is nic_conf.h		*/
#include <sys/tigon_api.h>			/* Alteon name is nic_api.h		*/
#include <sys/tigon_link.h>			/* Alteon nam is link.h			*/


/*
	Modified Alteon files
*/
#include <sys/tigon_eeprom.h>			/* Alteon name is eeprom.h (note this
						   file includes media_regions.h which
						   is renamed tigon_media_regions.h	*/

#include <sys/tigon_pub_tg.h>			/* Alteon name is pub_tg.h (note this
						   file includes tg_rev.h which is 
						   renamed to tigon_rev.h)		
						 
						   It also has tg_regs_t added to the 
						   beginning of the shared memory struct */


#endif /* _sys_tigon_h */
