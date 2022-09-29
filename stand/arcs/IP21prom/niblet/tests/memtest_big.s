#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

#define PG_SZ		4096
#define NUM_PAGES	512
#ifdef SHORT
#define NUM_PASSES  20
#else
#define NUM_PASSES	2 << 20
#endif
#define STRIDE		382	# Must be an even number
#ifdef SHORT
#define PASSMASK 1
#else
#define PASSMASK	0x1f	# Print every two passes
#endif

	.globl ENTRY
	.set noat
	.set noreorder
	/*
	Margie converted this to 64 bit.  Those are my only changes.

	This test is exactly the same as memtest.s, except that the size
	of the data area that we write to is much larger.  In memtest.s we write
	a half word at a stride of 382 over 20 PAGES of data area.  In bigmem.s
	we write over 512 pages of data area, instead.  See memtest.s
	for a full description of how this test works.
	*/
ENTRY:
    li  RSYS, PRNTHEX; li RPARM, 0x10101004; syscall /* DEBUG: */

/* debug stuff */
	li	RSYS, PRNTHEX		# Print failure information
	li	RPARM, 0x713f0
	syscall
	
	li	RSYS, VTOP		# Get physical address
	syscall
	li	RSYS, PRNTHEX
	syscall				# Print it.
/* debug done */
	
	
	li	RSYS, GPID		# Get my PID.
	syscall
	or	r8, RPARM, r0		# Copy PID to r8


	li	r31, NUM_PASSES
repeat:

	li	r3, 0xa5a5		# pattern to write.
	xor	r3, r8, r3		# XOR it with the PID.

start:
	li      r7, (PG_SZ * NUM_PAGES)	# Size of data area
	dla	r6, startmem		# Start of data area
	dadd	r7, r7, r6		# Compute end of data area
	li	r9, STRIDE		# Stride of _almost_ a cache line
	dsub	r7, r7, r9		# Subtract stride from end.
	li	r10, 2			# Loop the first time.
	dsub	r7, r7, r10		# Subtract 2 from end...

wloop1:
	sh	r3, 0(r6)		# Write the halfword to 0(r6)
	dadd	r6, r9, r6		# Increment r6
	bne	r10, r0, wloop1		# Branch based on last test
	sltu	r10, r6, r7		# Branch if still in data area

	dla	r6, startmem		# load start address
	li	r10, 1			# Loop the first time.
rloop1:
	lh	r1, 0(r6)		# read the halfword from 0(r6)
	dadd	r6, r9, r6		# Increment r6
	andi	r1, 0xffff		# And off top halfword	
	bne	r1, r3, badread		# If the value we put there isn't
					#	there, fail
	nop
	bne	r10, r0, rloop1		# Branch based on last test
	sltu	r10, r6, r7		# Branch if still in data area

	li	r1, 0xffff
	xor	r3, r3, r1		# Negate r3 (write pattern)
	andi	r10, r3, 0xff00		# Get second byte of pattern
	li	r1, 0x5a00		# If it's this, loop
	beq	r1, r10, start		# Do loop twice.
	nop

	li	r1, PASSMASK
	and	r1, r31
	bnez	r1, 1f		# Print pass number?
	nop
	
	move	RPARM, r31
	li	RSYS, PRNTHEX
	syscall

1:
	bne	r31, 0, repeat
	daddi	r31, -1			# (BD)

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
	or	RPARM, r6, r0		# Virtual address
	syscall
	
	or	RPARM, r6, r0
	li	RSYS, VTOP		# Get physical address
	syscall
	li	RSYS, PRNTHEX
	syscall				# Print it.
	
	li	RSYS, PRNTHEX		# Print failure information
	or	RPARM, r3, r0		# Should have been
	syscall
	li	RSYS, PRNTHEX		# Print failure information
	or	RPARM, r1, r1		# Value actually read
	syscall

/* Margie added for debug */
	lh	r1, 0(r6)		# read the halfword from 0(r6)
	andi	r1, 0xffff		# And off top halfword	
	li	RSYS, PRNTHEX		# Print failure information
	or	RPARM, r6, r6		# Value actually read
	syscall
	li	RSYS, PRNTHEX		# Print failure information
	or	RPARM, r1, r1		# Value actually read
	syscall
/* */

	li	RSYS, NFAIL
	syscall

	.lcomm	startmem, PG_SZ * NUM_PAGES

