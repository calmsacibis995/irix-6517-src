/* hlocktest.s 
 *		Hard lock test
 */

#include "nsys.h"
#include <regdef.h>
	
	.globl ENTRY
	.set noreorder
	.set noat

	/* ********************************************************************
       Summary:
       The basic idea of this test is that each of the two mphlock tasks
       store to and read back from the same one location.  The store/read
       sequence is a critical section and is protected by locks, so a task
       should always get back the data that it stored.  If it doesn't, it fails
       The only difference between this test and mpslock.s is that this test
       uses hardware locks (ll/sc) to do locking, while mpslock uses peterson's
       algorithm. The mpslock test requires that this test run as a
       two task pair, but for this test that is not necessary.  You should be able
       to run exactly one of this test, or multiple tasks of this test.
       mphlock.s and bighlock.s are exactly the same test, except for the number of
       times the store/load combination is executed.  Why do we even maintain
       both mphlock.s and bighlock.s???  They even both have the same share_offset!

            SHMEM = R7 = GET SHMEM ADDRESS
            R9 = PID = MY PID
            LOCKREG = R8 = SHMEM + LOCK
        dotest:	
            COUNTER = Rxx = NUMTIMES

        The testloop loop just stores a value to a location, waits a few
        cycles, then reads it back to make sure it is the same value.  Before
        it does the store it gets the lock and after it does the store, it
        releases the lock, insuring that store/load sequence is protected
        as a critical region.

     -> testloop:
    |       getlock()
    |       R3 = COUNTER * 16 + PID
    |       store R3 into [SHMEM + VALUE]
    |       SCRATCH = WAITCOUNT
    |   spin:
    |       branch back to spin while SCRATCH > 0, decrementing SCRATCH each time
	|
    |       SCR2 = load from [SHMEM + VALUE]
    |       if (SCR2 != R3)
    |           FAIL
    |       releaselock()
	|
    |       if COUNTER > 0
     ---------  go back to testloop

	********************************************************************/
#define SHARE_OFFSET    48      	/* Allows multiple MP tests */

#define LOCK	SHARE_OFFSET + 0	/* 16 byte array of flags */
#define VALUE	SHARE_OFFSET + 4

#define SCR2	r1
#define	LOCKREG	r8
#define PID	r9
#define SCRATCH	r6
#define SHMEM	r7
#define COUNTER r10

#ifdef SHORT
#define NUMTIMES 20
#else
#define NUMTIMES	(2 << 31)
#endif
#define WAITCOUNT	10

	.globl	releaselock
	.globl	getlock
	.globl	toploop

ENTRY:
    li  RSYS, PRNTHEX; li RPARM, 0x1010100b; syscall /* DEBUG: */
	li	RSYS, GSHARED		# Get address of shared memory
	syscall
	or	SHMEM, RPARM, RPARM		# Copy address to SHMEM

	li	RSYS, GPID
	syscall
	or	PID, RPARM, RPARM		# Copy PID

	addi	LOCKREG, SHMEM, LOCK	# add offset to shared mem base

dotest:
	dli	COUNTER, NUMTIMES

	/* While (COUNTER > 0) { */
testloop:
	jal	getlock

	sll	r3, COUNTER, 4		# Shift counter over 4
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

	bgtz	COUNTER, testloop		# Loop if counter > 0
	addi	COUNTER, COUNTER, -1		# Decrement counter

	li	RSYS, NPASS
	syscall

failed:
	li	RSYS, NFAIL
	syscall

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
