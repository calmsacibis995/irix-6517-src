/*
 * Copyright 1985 by Silicon Graphics Inc.
 */

#ident	"$Id: ftruncate.s,v 1.3 1995/02/18 00:04:05 sfc Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	.weakext ftruncate64, _ftruncate;
	.weakext _ftruncate64, _ftruncate;
#endif

SYSCALL(ftruncate)
	move	v0,zero
	RET(ftruncate)
