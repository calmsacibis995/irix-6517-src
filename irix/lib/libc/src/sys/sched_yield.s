/*
 * Copyright 1996 by Silicon Grapics Incorporated
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(sched_yield)
	RET(sched_yield)
#else
#define SYS__sched_yield SYS_sched_yield
SYSCALL(_sched_yield)
	RET(_sched_yield)
#endif
