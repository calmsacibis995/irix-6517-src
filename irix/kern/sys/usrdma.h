/**************************************************************************
*                                                                        *
*               Copyright (C) 1993, Silicon Graphics, Inc.               *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/


#ifndef __USRDMA_H__
#define __USRDMA_H__

/* The following are used to create page addresses passed into
 * the mmap routine of usrdma for VME to tell it what function to
 * perform.
 */

#define VME_MAP_REGS	1
#define VME_MAP_BUF	2


/* XXX This needs to be at least NBPP. */
#if _PAGESZ >= 16384
#define VME_REG_SIZE	_PAGESZ
#else
#define VME_REG_SIZE	16384
#endif

#endif /* __USRDMA_H__ */
