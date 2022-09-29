/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
#ident "$Revision: 1.5 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#ifndef  _LIBC_ABI
	.weakext BSDfchown, _fchown
#endif /* _LIBC_ABI */
SYSCALL(fchown)
	RET(fchown)
