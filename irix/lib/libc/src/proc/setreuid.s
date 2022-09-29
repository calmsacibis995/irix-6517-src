/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Id: setreuid.s,v 1.2 1994/09/27 18:59:35 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(setreuid)
	RET(setreuid)
