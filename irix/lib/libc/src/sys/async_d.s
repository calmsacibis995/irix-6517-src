/*
 * Copyright 1986 by Silicon Graphics Inc.
 */
#ident	"$Revision: 1.3 $"

#include <sys.s>
#include <sys/asm.h>
#include <sys/regdef.h>
#include <sys/syscall.h>

SYSCALL(async_daemon)
	RET(async_daemon)
