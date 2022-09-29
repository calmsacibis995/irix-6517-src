/*
 * Copyright 1995 by Silicon Graphics Inc.
 */
#ident "$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	.weakext ngetdents64, _ngetdents;
	.weakext _ngetdents64, _ngetdents;
#endif

SYSCALL(ngetdents)
	RET(ngetdents)
