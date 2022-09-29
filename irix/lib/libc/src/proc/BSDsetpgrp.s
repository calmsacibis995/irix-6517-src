/*
 * Copyright 1988 by Silicon Grapics Incorporated
 */

#ident	"$Id: BSDsetpgrp.s,v 1.2 1994/09/27 18:56:04 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(BSDsetpgrp)
	RET(BSDsetpgrp)
