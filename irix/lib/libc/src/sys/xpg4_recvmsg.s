/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/xpg4_recvmsg.s,v 1.1 1997/02/05 19:51:32 cleo Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

PSEUDO(__xpg4_recvmsg, xpg4_recvmsg)
	RET(__xpg4_recvmsg)
