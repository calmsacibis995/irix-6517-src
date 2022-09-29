/*
 * Copyright 1995 by Silicon Graphics Inc.
 */

#ident	"$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(ngetdents64)
	RET(ngetdents64)
