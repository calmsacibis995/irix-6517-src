/*
 * Copyright 1986 by Silicon Graphics Inc.
 */

#ident	"$Revision: 1.3 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(xmknod)
	move	v0,zero
	RET(xmknod)
