#include <regdef.h>
#include "nsys.h"

	.globl ENTRY
	.set	noreorder
ENTRY:
	li	RSYS, SUSPND	# Suspend Niblet scheduling
	syscall
	li      r6, 400
	li	r5, 1
loop:  
	bne	r6, r0, loop
	sub	r6, r6, r5	# Subtract 1 from r6 (delay slot)
	li	RSYS, RESUME
	syscall
	li	RSYS, NPASS
	syscall
