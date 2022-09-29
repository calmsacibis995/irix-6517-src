/*
 * Copyright 1992, Silicon Graphics Inc.  All rights reserved.
 */

#ident	"$Revision: 1.3 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
        .weakext fstatvfs64, _fstatvfs;
        .weakext _fstatvfs64, _fstatvfs;
#endif

SYSCALL(fstatvfs)
	RET(fstatvfs)
