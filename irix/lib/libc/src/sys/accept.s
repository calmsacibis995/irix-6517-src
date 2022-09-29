/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/accept.s,v 1.3 1997/05/12 14:15:01 jph Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(accept)
	RET(accept)
#else
#define SYS__accept SYS_accept
SYSCALL(_accept)
	RET(_accept)
#endif
