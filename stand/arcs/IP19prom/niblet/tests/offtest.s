#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

#define PG_SZ	4096
#define NUM_PAGES 1
#define STRIDE	382	# Must be an even number

	.globl ENTRY
	.set noat
	.set noreorder

.space 0x80

ENTRY:
	li	RSYS, GPID		# Get my PID.
	syscall
	or	r8, RPARM, r0		# Copy PID to r8

	li	r3, 0xa5a5		# pattern to write.
	xor	r3, r8, r3		# XOR it with the PID.

start:
	li      r7, (PG_SZ * NUM_PAGES)	# Size of data area
	la	r6, startmem		# Start of data area
	add	r7, r7, r6		# Compute end of data area
	li	r9, STRIDE		# Stride of _almost_ a cache line
	sub	r7, r7, r9		# Subtract stride from end.
	li	RPARM, 2			# Loop the first time.
	sub	r7, r7, RPARM		# Subtract 2 from end...

wloop1:
	sh	r3, 0(r6)		# Write the halfword to 0(r6)
	add	r6, r9, r6		# Increment r6
	bne	RPARM, r0, wloop1		# Branch based on last test
	sltu	RPARM, r6, r7		# Branch if still in data area

	la	r6, startmem		# load start address
	li	RPARM, 1			# Loop the first time.
rloop1:
	lh	r1, 0(r6)		# read the halfword from 0(r6)
	add	r6, r9, r6		# Increment r6
	andi	r1, 0xffff		# And off top halfword	
	bne	r1, r3, badread		# If the value we put there isn't
					#	there, fail
	nop
	bne	RPARM, r0, rloop1		# Branch based on last test
	sltu	RPARM, r6, r7		# Branch if still in data area

	li	r1, 0xffff
	xor	r3, r3, r1		# Negate r3 (write pattern)
	andi	RPARM, r3, 0xff00		# Get second byte of pattern
	li	r1, 0x5a00		# If it's this, loop
	beq	r1, RPARM, start		# Do loop twice.
	nop

	li	RSYS, NPASS
	syscall

loop:
	j loop
	nop

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

	.data
startmem:
	.space	PG_SZ * NUM_PAGES

