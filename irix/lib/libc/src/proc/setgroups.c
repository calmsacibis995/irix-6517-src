 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak setgroups = _setgroups
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <sys/syssgi.h>

/*
 * Setgroups.c: wrapper for the POSIX version of the BSD-style setgroups()
 * system call, added by SGI.  (The BSD version uses integer gids.)
 */
int
setgroups(int gidsetsize, gid_t *grouplist)
{
	return((int)syssgi(SGI_SETGROUPS,gidsetsize,grouplist));
}

