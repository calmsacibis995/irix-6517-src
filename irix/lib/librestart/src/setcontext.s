#ident "$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(setcontext)
	RET(setcontext)
