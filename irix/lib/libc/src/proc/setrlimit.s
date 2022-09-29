/*
 * Copyright 1989 by Silicon Graphics Incorporated
 */
#ident "$Revision: 1.4 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
#if _MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32
	.weakext setrlimit64, _setrlimit;
	.weakext _setrlimit64, _setrlimit;
#endif
SYSCALL(setrlimit)
	RET(setrlimit)
#else
#define SYS__setrlimit SYS_setrlimit
SYSCALL(_setrlimit)
	RET(_setrlimit)
#endif
