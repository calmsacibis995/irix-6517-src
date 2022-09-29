/* hlocktest.s 
 *		Hard lock test
 */

#include "nsys.h"
#include <regdef.h>
	
	.globl ENTRY
	.set noreorder
	.set noat

#define SHARE_OFFSET    48      	/* Allows multiple MP tests */

#define LOCK	SHARE_OFFSET + 0	/* 16 byte array of flags */
#define VALUE	SHARE_OFFSET + 4

#define SCR2	r1
#define	LOCKREG	r8
#define PID	r9
#define SCRATCH	r6
#define SHMEM	r7

#define NUMTIMES	(2 << 31)
#define WAITCOUNT	10

	.globl	releaselock
	.globl	getlock
	.globl	toploop

getlock:
254:	ll      SCR2, 0( LOCKREG )
	nop
	bne     SCR2, r0, 254b
	addi    SCR2, SCR2, 1
	sc      SCR2, 0( LOCKREG )
	nop
	beq     SCR2, r0, 254b
	nop
	j	ra
	nop

releaselock:
	sw      r0, 0( LOCKREG )
	j	ra
	nop

ENTRY:
	li	RSYS, GSHARED		# Get address of shared memory
	syscall
	or	SHMEM, RPARM, RPARM		# Copy address to SHMEM

	li	RSYS, GPID
	syscall
	or	PID, RPARM, RPARM		# Copy PID

	addi	LOCKREG, SHMEM, LOCK	# add offset to shared mem base

dotest:
	li	RPARM, NUMTIMES

	/* While (RPARM > 0) { */
testloop:
	jal	getlock

	sll	r3, RPARM, 4		# Shift counter over 4
	add	r3, r3, PID		# Add in PID

	sw	r3, VALUE(SHMEM)	# Store our value into shared mem.

	li	SCRATCH, WAITCOUNT
spin:
	bgtz	SCRATCH, spin
	addi	SCRATCH, -1

	lw	SCR2, VALUE(SHMEM)
	nop
	bne	SCR2, r3, failed
	nop

	jal	releaselock
	nop

	bgtz	RPARM, testloop		# Loop if counter > 0
	addi	RPARM, RPARM, -1		# Decrement counter

	li	RSYS, NPASS
	syscall

failed:
	li	RSYS, NFAIL
	syscall
