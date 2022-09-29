/*
 * Copyright 1988 by Silicon Grapics Incorporated
 */

#ident	"$Id: BSDgetpgrp.s,v 1.2 1994/09/27 18:55:40 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(BSDgetpgrp)
	RET(BSDgetpgrp)
