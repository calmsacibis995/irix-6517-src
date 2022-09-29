 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.1 $"

#include "synonyms.h"

#include <sys.s>
#include "../sys/sys_extern.h"

#include <sys/types.h>

/* Wrappers for the SVR4 flavor of sigstack. */
int
__xpg4_setregid(gid_t rgid, gid_t egid)
{
	return (int)syscall(SYS_xpg4_setregid, rgid, egid);
}
