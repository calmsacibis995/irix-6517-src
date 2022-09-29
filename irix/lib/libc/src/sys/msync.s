/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/msync.s,v 1.2 1997/05/12 14:15:19 jph Exp $ */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(msync)
	RET(msync)
#else
#define SYS__msync SYS_msync
SYSCALL(_msync)
	RET(_msync)
#endif
