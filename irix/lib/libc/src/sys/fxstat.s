/*
 * Copyright 1986 by Silicon Graphics Inc.
 */

#ident	"$Revision: 1.2 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(fxstat)
	move	v0,zero
	RET(fxstat)
