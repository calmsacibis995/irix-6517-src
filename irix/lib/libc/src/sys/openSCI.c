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
	#pragma weak open = _open
	#pragma weak open64 = _open
	#pragma weak _open64 = _open
#endif

#include "synonyms.h"
#include "mplib.h"
#include <sys/types.h>


int
open(const char *path, int oflag, mode_t mode)
{
	extern int __open(const char *, int, mode_t);
	MTLIB_BLOCK_CNCL_RET( int, __open(path, oflag, mode) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
