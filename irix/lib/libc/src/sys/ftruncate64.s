/*
 * Copyright 1994 by Silicon Graphics Inc.
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/ftruncate64.s,v 1.1 1994/05/27 23:23:39 ajs Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(ftruncate64)
	move	v0,zero
	RET(ftruncate64)
