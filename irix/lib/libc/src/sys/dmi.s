/*
 * Copyright 1994 Silicon Graphics Inc.
 */

#ident	"$Revision: 1.2 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(dmi)
	RET(dmi)
#else
#define SYS__dmi SYS_dmi
SYSCALL(_dmi)
	RET(_dmi)
#endif
