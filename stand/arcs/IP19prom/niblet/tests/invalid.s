#include <regdef.h>
#include "nsys.h"

	.globl ENTRY
	.set noreorder
ENTRY:
	li	RPARM, 400		# Set up a long quantum 
	li	RSYS, PRNTHEX
	syscall
loop:
	li	RSYS, REST	# Wait until rescheduled
	syscall
	li	RSYS, INVALID	# Set up invalidate syscall
	syscall			# Invalidate a TLB entry
	syscall			# Invalidate a TLB entry
	li	RSYS, GNPROC	# Get the number of processes running
	syscall
	li	t0, 1
	bne	t0, RPARM, loop
	li 	RSYS, NPASS
	syscall
	nop
