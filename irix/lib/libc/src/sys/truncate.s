/*
 * Copyright 1985 by Silicon Graphics Inc.
 */

#ident	"$Id"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	.weakext truncate64, _truncate;
	.weakext _truncate64, _truncate;
#endif

SYSCALL(truncate)
	move	v0,zero
	RET(truncate)
