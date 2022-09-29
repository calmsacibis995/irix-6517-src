/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak quotactl = _quotactl
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <sys.s>
#include "sys_extern.h"

int
quotactl(int cmd, 
	caddr_t fdev, 
	int uid, 
	caddr_t addr)
{
	return((int)syscall(SYS_quotactl, cmd, fdev, uid, addr));
}
