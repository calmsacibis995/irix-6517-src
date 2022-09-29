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

#include <sgidefs.h>

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
#ifdef __STDC__
	#pragma weak pread = _pread
	#pragma weak pread64 = _pread
	#pragma weak _pread64 = _pread
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	#pragma weak __pread32 = ___pread32
#endif
#endif

#include "synonyms.h"
#include "mplib.h"
#include <sys/uio.h>

ssize_t
pread(int fildes, void *buf, size_t nbyte, off64_t offset)
{
	extern int __pread(int, void *, size_t, off64_t);
	MTLIB_BLOCK_CNCL_RET( ssize_t, __pread(fildes, buf, nbyte, offset) );
}
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
ssize_t
__pread32(int fildes, void *buf, size_t nbyte, off_t offset)
{
	extern int __pread(int, void *, size_t, off64_t);
	off64_t off64 = offset;
	MTLIB_BLOCK_CNCL_RET( ssize_t, __pread(fildes, buf, nbyte, off64) );
}
#endif /*_MIPS_SIM == _MIPS_SIM_ABI32  */
#endif /* !_LIBC_ABI && !_LIBC_NOMP */
