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
	#pragma weak psema_cntl = _psema_cntl
#endif

#include "synonyms.h"
#include "mplib.h"


int
_psema_cntl(int cmd, void *arg1, void *arg2, void *arg3, void *arg4)
{
	extern int __psema_cntl(int, ...);
	MTLIB_BLOCK_NOCNCL_RET( int, __psema_cntl(cmd, arg1, arg2, arg3, arg4));
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
