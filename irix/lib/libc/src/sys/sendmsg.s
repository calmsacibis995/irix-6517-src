/*
 * Copyright 1985 by Silicon Grapics Incorporated
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/sendmsg.s,v 1.3 1997/05/12 14:15:46 jph Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(sendmsg)
	RET(sendmsg)
#else
#define SYS__sendmsg SYS_sendmsg
SYSCALL(_sendmsg)
	RET(_sendmsg)
#endif
