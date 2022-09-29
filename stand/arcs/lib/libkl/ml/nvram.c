/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.2 $"

#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <libsc.h>
#include <promgraph.h>
#include <sys/SN/nvram.h>

check_nvram(__psunsigned_t	nvram_base, nic_t	xnic)
{
	/* check for magic number, checksum etc. */
	/* XXX just check for magic number, ignore checksum??? */
#if 0
	if (_cpu_is_nvvalid(nvram_base))
		return 1 ;
	else
#endif
		return 1 ;  /* XXX just to keep things going */
}

