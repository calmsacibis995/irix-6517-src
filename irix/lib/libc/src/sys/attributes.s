/*
 * Copyright 1994 Silicon Graphics Inc.
 */

#ident	"$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(attr_get)
	RET(attr_get)
SYSCALL(attr_getf)
	RET(attr_getf)

SYSCALL(attr_set)
	RET(attr_set)
SYSCALL(attr_setf)
	RET(attr_setf)

SYSCALL(attr_remove)
	RET(attr_remove)
SYSCALL(attr_removef)
	RET(attr_removef)

SYSCALL(attr_list)
	RET(attr_list)
SYSCALL(attr_listf)
	RET(attr_listf)

SYSCALL(attr_multi)
	RET(attr_multi)
SYSCALL(attr_multif)
	RET(attr_multif)
