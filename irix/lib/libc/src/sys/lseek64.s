/*
 * Copyright 1994 by Silicon Graphics Inc.
 */

#ident	"$Revision: 1.2 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(lseek64)
	RET64(lseek64)
