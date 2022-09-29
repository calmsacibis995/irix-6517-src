/*
 * Copyright 1988 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/vhangup.s,v 1.1 1988/08/25 17:11:33 paulm Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(vhangup)
	RET(vhangup)
