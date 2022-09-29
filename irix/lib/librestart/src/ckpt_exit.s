/* ------------------------------------------------------------------ */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(exit)
	RET(exit)
