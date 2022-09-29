#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

	.globl ENTRY
	.globl exception
	.set noreorder
ENTRY:
	li	r6, 0
	la	r3, exception
	li	RPARM, 9		# Steal Breakpoint exception.
	li	RSYS, STEALE
	syscall

	break	0

	li	RPARM, 9	# Breakpoint exception
	li	r3, 0	# Return the vector

	syscall

	li	RSYS, MSG
	li	RPARM, M_EXCCOUNT
	syscall		# Print exception count message

	li	RPARM, 9
	li	RSYS, PRNTHEX
	syscall		# Print 9

	li	RSYS, GECOUNT	# Get exception count
	li	RPARM, 9		# for vector 9
	syscall

	li	RSYS, PRNTHEX
	syscall			# Print exception count

	li	RSYS, MSG
	li	RPARM, M_EXCCOUNT
	syscall		# Print exception count message

	li	RPARM, 2
	li	RSYS, PRNTHEX
	syscall		# Print 2

	li	RSYS, GECOUNT	# Get exception count
	li	RPARM, 9		# for vector 2 (tlbl)
	syscall

	li	RSYS, PRNTHEX
	syscall			# Print exception count

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

	# Putting the exception handler way down here assure that it will
	# not be mapped yet.
exception:
	li	r6, 1
	nop
	j	R_VECT
	nop
