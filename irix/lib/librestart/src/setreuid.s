/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Id: setreuid.s,v 1.1 1997/03/12 18:38:31 beck Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(setreuid)
	RET(setreuid)
