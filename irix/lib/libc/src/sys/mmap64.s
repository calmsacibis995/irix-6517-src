/*
 * Copyright 1994 by Silicon Graphics Inc.
 */

#ident	"$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(mmap64)
	RET(mmap64)
