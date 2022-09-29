/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/sys/RCS/fchmod.s,v 1.1 1988/12/08 18:59:04 eva Exp $ */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(fchmod)
	RET(fchmod)
