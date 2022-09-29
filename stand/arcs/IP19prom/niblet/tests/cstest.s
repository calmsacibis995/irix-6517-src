#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

	.globl ENTRY
	.set noreorder

ENTRY:
	li	r5, 0
	li	r6, 0

	la	r2, csout
	li	RSYS, CSOUT
	syscall

	la	r2, csin
	li	RSYS, CSIN
	syscall

	li	RSYS, REST
	syscall

	bne	r5, r0, test2
	nop

	li	RSYS, NFAIL
	syscall

test2:
	bne	r6, r0, okay
	nop

	li	RSYS, NFAIL
	syscall
okay:
	li	RSYS, VPASS
	syscall
	nop

	.align 12
	# Page 1
	nop
	.align	12
	# Page 2
csout:
	addi	r5, 1
	j	R_VECT
	nop

	.align 12
	nop	# Page 3
	.align 12
	nop	# Page 4
	.align 12
		# Page 5
csin:
	addi	r6, 1
	j	R_VECT
	nop

