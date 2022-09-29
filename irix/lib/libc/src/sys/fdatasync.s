/*
 * Copyright 1995 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/fdatasync.s,v 1.1 1995/03/24 21:15:24 jeffreyh Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(fdatasync)
	RET(fdatasync)
