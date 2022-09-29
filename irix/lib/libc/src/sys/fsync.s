/*
 * Copyright 1989 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/fsync.s,v 1.2 1997/05/12 14:15:12 jph Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(fsync)
	RET(fsync)
#else
#define SYS__fsync SYS_fsync
SYSCALL(_fsync)
	RET(_fsync)
#endif
