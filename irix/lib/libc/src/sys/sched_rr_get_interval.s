/*
 * Copyright 1996 by Silicon Grapics Incorporated
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(sched_rr_get_interval)
	RET(sched_rr_get_interval)
