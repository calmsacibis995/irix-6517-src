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
	#pragma weak msync = _msync
#endif

#include "synonyms.h"
#include "mplib.h"
#include <sys/mman.h>
#include <sys/types.h>


int
msync(void *addr, size_t len, int flags)
{
	extern int __msync(void *, size_t, int);
	MTLIB_BLOCK_CNCL_RET( int, __msync(addr, len, flags) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
