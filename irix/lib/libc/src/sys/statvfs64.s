/*
 * Copyright 1995, Silicon Graphics Inc.  All rights reserved.
 */

#ident	"$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(statvfs64)
	RET(statvfs64)
