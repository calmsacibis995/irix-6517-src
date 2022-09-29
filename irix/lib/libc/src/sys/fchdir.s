/*
 * Copyright 1989 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/fchdir.s,v 1.1 1989/05/04 14:43:11 bowen Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(fchdir)
	RET(fchdir)
