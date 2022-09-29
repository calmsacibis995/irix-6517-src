#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"
#include "../cp0_r4k.h"

	.globl ENTRY
	.set noreorder
ENTRY:
	li	r6, 0
	la	r3, interrupt
	li	RPARM, 1		# Steal SW interrupt RPARM.
	li	RSYS, STEALI
	syscall

	li	RSYS, ORDAIN
	syscall

	li	r6, CAUSE_SW1
	mtc0	r6, C0_CAUSE
	nop

	bne	r6, r0, ok
	nop

	li	RSYS, NFAIL
	syscall

ok:
	li	RSYS, NPASS
	syscall

	.align 12
	nop

	.align 12
	nop

	.align 12
	nop

	.align 12
	nop

	.align 12
interrupt:
	li	r6, 1
	mtc0	r0, C0_CAUSE	# Turn off software interrupt
	nop
	j	R_VECT
	nop
