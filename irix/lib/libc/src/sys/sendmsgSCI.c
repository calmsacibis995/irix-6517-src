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
	#pragma weak sendmsg = _sendmsg
#endif

#include "synonyms.h"
#include "mplib.h"
#include <sys/socket.h>
#include <sys/socket.h>


int
sendmsg(int s, const struct msghdr *msg, int flags)
{
	extern int __sendmsg(int, const struct msghdr *, int);
	MTLIB_BLOCK_CNCL_RET( int, __sendmsg(s, msg, flags) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
