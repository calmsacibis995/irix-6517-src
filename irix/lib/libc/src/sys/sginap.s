/*  sginap.s 1.1 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
SYSCALL(sginap)
	RET(sginap)
#else
#define SYS__sginap SYS_sginap
SYSCALL(_sginap)
	RET(_sginap)
#endif
