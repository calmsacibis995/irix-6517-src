/*
 * Copyright 1985 by Silicon Graphics Inc.
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/rename.s,v 1.3 1986/12/01 17:02:37 gb Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(rename)
	move	v0,zero
	RET(rename)
