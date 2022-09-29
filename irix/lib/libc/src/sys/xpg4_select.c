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
#include "sys_extern.h"

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

/* Wrapper for the xpg4 flavor of select. */
int
__xpg4_select(
	int nfds,
	fd_set *readfds,
	fd_set *writefds,
	fd_set *exceptfds,
	struct timeval *timeout)
{
	if (nfds < 0 || nfds > FD_SETSIZE) {
		setoserror(EINVAL);
		return -1;
	}

	return (int)syscall(SYS_xpg4_select, nfds, readfds, writefds,
				exceptfds, timeout);
}
