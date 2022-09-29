/*
 * Copyright 1986 by Silicon Graphics Inc.
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/symlink.s,v 1.2 1986/12/01 17:03:10 gb Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(symlink)
	RET(symlink)
