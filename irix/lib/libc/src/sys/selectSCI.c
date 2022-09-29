 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

/* Wrapper for blocking system call - see include files for details */

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
#ifdef __STDC__
	#pragma weak select = _select
#endif

#include "synonyms.h"
#include "mplib.h"
#include <unistd.h>


int
select(int nfds, fd_set *readfds, fd_set *writefds,
       fd_set *exceptfds, struct timeval *timeout)
{
	extern int __select(int, fd_set *, fd_set *, fd_set *, struct timeval*);
	MTLIB_BLOCK_CNCL_RET_1( int,
		     __select(nfds, readfds, writefds, exceptfds, timeout) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
