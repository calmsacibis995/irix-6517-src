/*
 * Copyright 1994 by Silicon Grapics Incorporated
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(nanosleep)
	RET(nanosleep)
#else
#define SYS__nanosleep SYS_nanosleep
SYSCALL(_nanosleep)
	RET(_nanosleep)
#endif
