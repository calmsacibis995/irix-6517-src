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

/* Wrapper for modified system call - see include files for details */

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
#ifdef __STDC__
	#pragma weak sigpending = _sigpending
#endif

#include "synonyms.h"
#include "mplib.h"
#include <signal.h>


int
sigpending(sigset_t *maskptr)
{
	extern int __sigpending(sigset_t *);

	MTLIB_RETURN( (MTCTL_SIGPENDING, maskptr) );
	return (__sigpending(maskptr));
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
