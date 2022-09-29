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
#ident "$Revision: 1.4 $"

/* Wrapper for blocking system call - see include files for details */

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
#ifdef __STDC__
	#pragma weak close = _close
#endif

#include "synonyms.h"
#include "mplib.h"

extern struct aioinfo *_aioinfo;
extern void __aio_closechk(int);

int
close(int fd)
{
	extern int __close(int);
	if (_aioinfo) {
	/*
	 * If AIO library is in use, call into it to do KAIO bookkeeping and
	 * (someday) make sure all AIO is done before closing descriptor.
	 * Do KAIO marking before the syscall since close rarely fails and
	 * marking it unknown is probably best if close does fail.
	 */
		__aio_closechk(fd);
	}
	MTLIB_BLOCK_CNCL_RET( int, __close(fd) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
