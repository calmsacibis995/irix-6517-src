/*
 * Copyright 1996 by Silicon Grapics Incorporated
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(usync_cntl)
	RET(usync_cntl)
#else
#define SYS__usync_cntl SYS_usync_cntl
SYSCALL(_usync_cntl)
	RET(_usync_cntl)
#endif
