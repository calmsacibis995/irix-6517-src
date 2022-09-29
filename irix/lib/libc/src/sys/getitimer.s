/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(getitimer)
	RET(getitimer)
