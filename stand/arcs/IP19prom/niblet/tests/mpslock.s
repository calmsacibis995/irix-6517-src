/* slocktest.s 
 *		Soft lock test
 */

#include "nsys.h"
#include <regdef.h>
	
	.globl ENTRY
	.set noreorder

#define SHARE_OFFSET    0       	/* Allows multiple MP tests */

#define FLAG	SHARE_OFFSET + 0
#define LOCK	SHARE_OFFSET + 8
#define TURN	SHARE_OFFSET + 12
#define WHO	SHARE_OFFSET + 16	/* 16 byte array of flags */
#define VALUE	SHARE_OFFSET + 32

#define SCR2	r1
#define CPUNUM	r9
#define SCRATCH	r6
#define SHMEM	r7

#define NUMTIMES	16
#define WAITCOUNT	10

	.globl	releaselock
	.globl	getlock
	.globl	toploop
	.globl	wholoop
	.globl	skipself
	.globl	foundit

	.set	noat

ENTRY:
	# Figure out which two processes we are.
/*
	while (other == 0)
		for (i = 1; i < 16; i++) {
			if (i == PID)
				continue;
			if (who[i] != 0) {
				other = i;
				break;
			}
		}
	
	if (other > PID)
		CPUNUM = 0;
	else
		CPUNUM = 4;
*/

	li	RSYS, GSHARED		# Get address of shared memory
	syscall
	or	SHMEM, RPARM, RPARM		# Copy address to SHMEM

	li	RSYS, GPID
	syscall
	or	r8, RPARM, RPARM		# Copy PID to r8

	li	SCRATCH, 1
	add	SCR2, r8, SHMEM		# shared memory + PID offset
	sb	SCRATCH, WHO(SCR2)	# who[pid] := 1

toploop:
	li	RPARM, 15
wholoop:
	beq	RPARM, r8, skipself	# Don't check our own slot.
	add	SCR2, RPARM, SHMEM		# Add offset to start of shared mem.	
	lb	SCRATCH, WHO(SCR2)	# Get byte flag
	nop
	bne	r0, SCRATCH, foundit	
	nop
skipself:
	addi	RPARM, RPARM, -1		# Decrement r8
	bne	RPARM, 0, wholoop
	nop

	j	toploop
	nop

foundit:
	sgt	SCRATCH, RPARM, r8		# other < PID
	beq	r0, SCRATCH, cpuzero
	nop
	li	CPUNUM, 4		# We're CPU one
	j	dotest
	nop
cpuzero:
	li	CPUNUM, 0		# We're CPU zero
dotest:
	li	RPARM, NUMTIMES

	/* While (RPARM > 0) { */
testloop:
	jal	getlock

	sll	r3, RPARM, 4		# Shift counter over 4
	add	r3, r3, r8		# Add in PID

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

releaselock:
	add	SCRATCH, SHMEM, CPUNUM		# flag[i]
	sw	r0, FLAG(SCRATCH)		# flag[i] := 0
	j	ra
	nop

getlock:
	add	SCRATCH, SHMEM, CPUNUM		# shmem + i * 4
	li	SCR2, 4
	sw	SCR2, FLAG(SCRATCH)		# flag[i] := 4
	sub	SCR2, SCR2, CPUNUM		# SCR2 := j
	add	SCRATCH, SHMEM, SCR2		# shmem + j * 4
	sw	SCR2, TURN(SHMEM)		# turn := j
	nop
254:	lw	SCR2, TURN(SHMEM)		# SCR2 := turn
	nop
	beq	SCR2, CPUNUM, 255f		# if turn <> i, skip
	lw	SCR2, FLAG(SCRATCH)		# SCR2 := flag[j]
	nop
	beq	SCR2, r0, 255f			# if flag[j]<>0, skip
	nop
	j	254b				# end of while
255:	nop					# Acquired lock!
	j	ra
	nop


