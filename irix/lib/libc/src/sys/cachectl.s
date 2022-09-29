/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/cachectl.s,v 1.1 1988/12/08 18:59:50 eva Exp $ */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(cachectl)
	RET(cachectl)
