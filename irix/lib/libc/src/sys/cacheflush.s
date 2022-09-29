/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/cacheflush.s,v 1.2 1993/01/26 15:15:08 jleong Exp $ */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

/*
 * The MIPS PS ABI defines _flush_cache(char *addr, int nbytes, int cache).
 */
.weakext	_flush_cache, _cacheflush

SYSCALL(cacheflush)
	RET(cacheflush)
