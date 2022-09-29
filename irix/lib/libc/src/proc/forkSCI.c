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
	#pragma weak fork = _fork
	#pragma weak __vfork = _fork
	/*
	 * BB3.0 wants vfork - but we can give to them ONLY in the link-time
	 * libc - not the runtime since some PD configure programs look at
	 * symbols in libc to determine if vfork (and they want BSD vfork)
	 * is available.
	 * We do offer _vfork. In unistd.h we define vfork to be _vfork in XPG &
	 * ABI mode - this will permit things compiled on SGI to run on other
	 * ABI platforms.
	 */
	#pragma weak _vfork = _fork
#endif

#include "synonyms.h"
#include "mplib.h"
#include <sys/types.h>


pid_t
fork(void)
{
	extern pid_t __fork(void);

	MTLIB_RETURN( (MTCTL_FORK) );
	return (__fork());
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
