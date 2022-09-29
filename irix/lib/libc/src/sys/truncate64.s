/*
 * Copyright 1994 by Silicon Graphics Inc.
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/truncate64.s,v 1.1 1994/05/27 23:23:53 ajs Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(truncate64)
	move	v0,zero
	RET(truncate64)
