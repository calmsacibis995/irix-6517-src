/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_IP25CONFIG_SETUP_H
#define	_IP25CONFIG_SETUP_H

#include	<sys/R10k.h>
#include		<sys/EVEREST/IP25addrs.h>

#define	IP25C_SCC(_B)			 ((_B)        + \
                                         ((_B) << 8)  + \
                                         ((_B) << 16) + \
                                         ((_B) << 24) + \
                                         ((_B) << 32) + \
                                         ((_B) << 40) + \
                                         ((_B) << 48) + \
                                         ((_B) << 56))

#define	DWORD_SWAP(_D)			((_D) << 32)
#define	DWORD_UNSWAP(_D)		((_D) >> 32)

#endif _IP25CONFIG_SETUP_H
