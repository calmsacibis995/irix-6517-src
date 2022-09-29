/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Id: setregid.s,v 1.2 1994/09/27 18:59:28 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(setregid)
	RET(setregid)
