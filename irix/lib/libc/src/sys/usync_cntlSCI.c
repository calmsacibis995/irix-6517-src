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
	#pragma weak usync_cntl = _usync_cntl
#endif

#include "synonyms.h"
#include "mplib.h"


int
_usync_cntl(int op, void *arg)
{
	extern int __usync_cntl(int, void *);
	MTLIB_BLOCK_NOCNCL_RET( int, __usync_cntl(op, arg) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
