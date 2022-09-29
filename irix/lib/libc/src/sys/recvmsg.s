/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/recvmsg.s,v 1.3 1997/05/12 14:15:40 jph Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(recvmsg)
	RET(recvmsg)
#else
#define SYS__recvmsg SYS_recvmsg
SYSCALL(_recvmsg)
	RET(_recvmsg)
#endif
