/*
 * Copyright 1989 by Silicon Grapics Incorporated
 */
#ident "$Revision: 1.3 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if _MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32
	.weakext getrlimit64, _getrlimit;
	.weakext _getrlimit64, _getrlimit;
#endif

SYSCALL(getrlimit)
	RET(getrlimit)
