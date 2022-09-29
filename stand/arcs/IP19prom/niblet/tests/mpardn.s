/* mpardn.s
 *		MP ARray test - proceeds DowNward through the address space.
 */

#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

#define PG_SZ   	1024
#define NUM_PAGES 	1

#define BYTE_SIZE	(PG_SZ * NUM_PAGES) 

#define SHARE_OFFSET	1024	/* Lets several tests use shared memory */

#define STRIDE		766	/* Must be an even number > 2 * max_pid */

#define BEGIN_OFFSET	((BYTE_SIZE / STRIDE - 1) * STRIDE + SHARE_OFFSET)

#define ITERATIONS	2

	.globl ENTRY
	.set noat
	.set noreorder
ENTRY:
	li	r31, ITERATIONS - 1	# Set to -1 because of delay slot sub.
	li      RSYS, GPID              # Get my PID.
	syscall
	or      r8, RPARM, r0              # Copy PID to r8

begin_test:
	li      RSYS, GSHARED   # Get the address of the shared address space
	syscall
	li	r3, BEGIN_OFFSET
	add	r6, RPARM, r3	# add offset to start of shared mem.
				# r6 now contains data area start.
	add	r6, r6, r8	# Add PID to start
	add	r6, r6, r8	# Add PID to start
				# Now at begin + 2 * PID.
start:
	li	r7, SHARE_OFFSET 	# Get shared memory offset
	add	r7, r7, RPARM		# Compute end of data area
	li	r9, STRIDE		# Stride of _almost_ a cache line
	add	r7, r7, r9		# Add stride to end.
	li	RPARM, 2			# Loop the first time.
/*	sub	r7, r7, RPARM		# Subtract 2 from end... */

wloop1:
					# Calculate halfword to store:
	or	r3, r6, r6		# Here's the address
	xor	r3, r3, r8		# XOR in the PID.
	xor	r3, r3, r31		# XOR in the iteration number
	sh	r3, 0(r6)		# Write the halfword to 0(r6)
	sub	r6, r6, r9		# Decrement r6
	bne	RPARM, r0, wloop1		# Branch based on last test
	sgtu	RPARM, r6, r7		# Branch if still in data area

	li      RSYS, GSHARED   # Get the address of the shared address space
	syscall
	li	r3, BEGIN_OFFSET
	add	r6, RPARM, r3	# add offset to start of shared mem.
				# r6 now contains data area start.
	add	r6, r6, r8	# Add PID to start
	add	r6, r6, r8	# Add PID to start
				# Now at begin + 2 * PID.

	li	RPARM, 1			# Loop the first time.
rloop1:
	lh	r1, 0(r6)		# read the halfword from 0(r6)
					# Calculate halfword stored:
	or	r3, r6, r6		# Here's the address
	xor	r3, r3, r8		# XOR in the PID.
	xor	r3, r3, r31		# XOR in the iteration number
	andi	r3, 0xffff		# And off top halfword	

	sub	r6, r6, r9		# Decrement r6
	andi	r1, 0xffff		# And off top halfword	
	bne	r1, r3, badread		# If the value we put there isn't
					#	there, fail
	nop
	bne	RPARM, r0, rloop1		# Branch based on last test
	sgtu	RPARM, r6, r7		# Branch if still in data area

	bgtz	r31, begin_test		# Loop?
	addi	r31, -1

	li	RSYS, NPASS
	syscall

badread:
	li	RSYS, MSG
	li	RPARM, M_MEMERR		# Print error message
	syscall

	li	RSYS, PRNTHEX		# Print failure information
	or	RPARM, r6, r0		# Virtual Address
	syscall
        or      RPARM, r6, r0           
        li      RSYS, VTOP              # Get physical address
        syscall
        li      RSYS, PRNTHEX
        syscall                         # Print it.

	li	RSYS, PRNTHEX		# Print failure information
	or	RPARM, r3, r0		# Should have been
	syscall
	li	RSYS, PRNTHEX		# Print failure information
	or	RPARM, r1, r1		# Value actually read
	syscall
	li	RSYS, NFAIL
	syscall

.lcomm  startmem,        PG_SZ * NUM_PAGES


