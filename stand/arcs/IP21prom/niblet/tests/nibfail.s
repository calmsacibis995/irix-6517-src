#include "regdef.h"
#include "nsys.h"

	.set noreorder

	.globl ENTRY

ENTRY:
	li	RSYS, NFAIL
	syscall

	nop


