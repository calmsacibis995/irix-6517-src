#include <regdef.h>
#include "nsys.h"
#include "testdefines.h"

	.globl ENTRY
	.set noreorder
ENTRY:
	DEBUG_PRINT_ID(0x10101001)
loop:
	SYS_REST()			# Wait until rescheduled
	SYS_INVALIDATE()	# Invalidate a TLB entry
	SYS_INVALIDATE()	# Invalidate a TLB entry
	SYS_GNPROC()		# Get the number of processes running
	li	t0, 1
	bne	t0, RPARM, loop
	PASS()
