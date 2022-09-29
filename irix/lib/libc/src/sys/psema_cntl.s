/*
 * Copyright 1996 by Silicon Graphics Incorporated
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(psema_cntl)
	RET(psema_cntl)
#else
#define SYS__psema_cntl SYS_psema_cntl
SYSCALL(_psema_cntl)
	RET(_psema_cntl)
#endif
